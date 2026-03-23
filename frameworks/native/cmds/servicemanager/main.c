/* mini-AOSP servicemanager — Unix socket service registry
 *
 * Protocol (text over Unix domain socket):
 *   ADD_SERVICE <name> <socket_path>\n → OK\n
 *   GET_SERVICE <name>\n               → <socket_path>\n or NOT_FOUND\n
 *   LIST_SERVICES\n                    → <name1> <name2> ...\n
 *
 * Flow:
 *   main() → setup signals → mkdir runtime dir
 *          → create_listening_socket()  — socket/bind/listen on .sock
 *          → signal_readiness()         — write .ready file so init continues
 *          → select() accept loop      — dispatch to handle_client()
 *          → on shutdown: close socket, unlink files
 *
 * See docs/components/04-servicemanager.md for full design doc.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <errno.h>
#include "../../../../system/core/liblog/log.h"
#include "../../../../system/core/libcommon/constants.h"
#include "../../../../system/core/libcommon/common.h"

#define TAG "servicemanager"

/* One entry in the service registry (name → socket path) */
struct service_entry {
    char name[MAX_NAME];            /* e.g. "ping" */
    char socket_path[MAX_PATH_LEN]; /* e.g. "/tmp/mini-aosp/ping.sock" */
};

static struct service_entry g_services[MAX_SERVICES]; /* the registry */
static int g_n_services = 0;                           /* entries used */
static volatile sig_atomic_t g_shutdown = 0;           /* signal flag */

/* ------------------------------------------------------------------ */
/*  Service registry                                                    */
/* ------------------------------------------------------------------ */

/* Linear scan for a service by name. Returns index or -1. */
static int find_service(const char *name) {
    for (int i = 0; i < g_n_services; i++) {
        if (strcmp(g_services[i].name, name) == 0) return i;
    }
    return -1;
}

/* Handle "ADD_SERVICE <name> <socket_path>".
 * Steps: 1. parse name and path tokens
 *        2. find existing entry or allocate new slot
 *        3. store name + path, respond OK */
static void handle_add_service(char **saveptr, char *response, size_t rsize) {
    char *name = strtok_r(NULL, " \t", saveptr);     /* 1. parse name */
    char *path = strtok_r(NULL, " \t", saveptr);     /*    parse path */
    if (!name || !path) {
        snprintf(response, rsize,
                 "ERROR: usage: ADD_SERVICE <name> <socket_path>\n");
        return;
    }
    int idx = find_service(name);                    /* 2. existing? */
    if (idx < 0 && g_n_services < MAX_SERVICES)
        idx = g_n_services++;                        /*    or new slot */
    if (idx >= 0) {
        strncpy(g_services[idx].name, name, MAX_NAME - 1);       /* 3. store */
        strncpy(g_services[idx].socket_path, path, MAX_PATH_LEN - 1);
        log_info(TAG, "Registered service: %s -> %s", name, path);
        snprintf(response, rsize, "OK\n");
    }
}

/* Handle "GET_SERVICE <name>" — look up and return socket path, or NOT_FOUND. */
static void handle_get_service(char **saveptr, char *response, size_t rsize) {
    char *name = strtok_r(NULL, " \t", saveptr);
    if (!name) return;
    int idx = find_service(name);
    if (idx >= 0)
        snprintf(response, rsize, "%s\n", g_services[idx].socket_path);
    else
        snprintf(response, rsize, "NOT_FOUND\n");
}

/* Handle "LIST_SERVICES" — return space-separated names of all registered services. */
static void handle_list_services(char *response, size_t rsize) {
    if (g_n_services == 0) {
        snprintf(response, rsize, "(none)\n");
        return;
    }
    size_t off = 0;
    for (int i = 0; i < g_n_services; i++)
        off += snprintf(response + off, rsize - off, "%s ", g_services[i].name);
    response[off++] = '\n';
    response[off] = '\0';
}

/* ------------------------------------------------------------------ */
/*  Client dispatch                                                     */
/* ------------------------------------------------------------------ */

/* Read one request from a client, dispatch to the right handler, write response.
 * Steps: 1. read request bytes  2. strip newlines
 *        3. tokenize first word as command  4. dispatch  5. write response */
static void handle_client(int client_fd) {
    char buf[MAX_LINE];
    ssize_t n = read(client_fd, buf, sizeof(buf) - 1); /* 1. read */
    if (n <= 0) return;
    buf[n] = '\0';

    size_t len = (size_t)n;
    miniaosp_strip_newlines(buf, &len);                /* 2. strip \n\r */

    char response[MAX_LINE];
    response[0] = '\0';

    char *saveptr = NULL;
    char *command = strtok_r(buf, " \t", &saveptr);    /* 3. first token = command */
    if (!command) return;

    /* 4. dispatch to handler */
    if (strcmp(command, "ADD_SERVICE") == 0)
        handle_add_service(&saveptr, response, sizeof(response));
    else if (strcmp(command, "GET_SERVICE") == 0)
        handle_get_service(&saveptr, response, sizeof(response));
    else if (strcmp(command, "LIST_SERVICES") == 0)
        handle_list_services(response, sizeof(response));
    else
        snprintf(response, sizeof(response), "ERROR: unknown command\n");

    write(client_fd, response, strlen(response));      /* 5. send response */
}

/* ------------------------------------------------------------------ */
/*  Socket setup                                                        */
/* ------------------------------------------------------------------ */

/* Create a Unix domain socket ready to accept connections.
 * Steps: 1. remove stale socket file  2. socket()  3. bind() to path  4. listen()
 * Returns fd on success, -1 on failure. */
static int create_listening_socket(const char *path) {
    unlink(path);                                    /* 1. remove stale */

    int fd = socket(AF_UNIX, SOCK_STREAM, 0);        /* 2. create socket */
    if (fd < 0) {
        log_error(TAG, "socket() failed");
        return -1;
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) { /* 3. bind */
        log_error(TAG, "bind() failed");
        close(fd);
        return -1;
    }
    if (listen(fd, 10) < 0) {                        /* 4. listen */
        log_error(TAG, "listen() failed");
        close(fd);
        return -1;
    }
    return fd;
}

/* Write .ready file so init's wait_for_file() poll detects we're up. */
static void signal_readiness(void) {
    char pid_str[32];
    snprintf(pid_str, sizeof(pid_str), "%d", getpid());
    miniaosp_write_file(MINIAOSP_SM_READY, pid_str);
}

/* ------------------------------------------------------------------ */
/*  main                                                                */
/* ------------------------------------------------------------------ */

/* servicemanager entry point.
 * Steps: 1. install signal handlers
 *        2. create listening Unix socket
 *        3. write .ready file (init is polling for this)
 *        4. select() loop: accept clients, handle one request each
 *        5. on shutdown: close socket, remove .sock + .ready files */
int main(void) {
    miniaosp_setup_signals(&g_shutdown);             /* 1. signal handlers */
    mkdir(MINIAOSP_RUNTIME_DIR, 0755);

    int server_fd = create_listening_socket(MINIAOSP_SM_SOCKET); /* 2. socket */
    if (server_fd < 0) return 1;

    log_info(TAG, "Listening on " MINIAOSP_SM_SOCKET);
    signal_readiness();                              /* 3. write .ready */

    /* 4. accept loop — select() with timeout so we can check g_shutdown */
    while (!g_shutdown) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(server_fd, &readfds);

        struct timeval tv = { .tv_sec = SELECT_TIMEOUT_SEC, .tv_usec = 0 };
        int ready = select(server_fd + 1, &readfds, NULL, NULL, &tv);
        if (ready < 0) {
            if (errno == EINTR) continue;  /* interrupted by signal, recheck */
            break;
        }
        if (ready == 0) continue;          /* timeout, recheck g_shutdown */

        int client_fd = accept(server_fd, NULL, NULL);
        if (client_fd < 0) {
            if (errno == EINTR) continue;
            log_error(TAG, "accept() failed");
            continue;
        }
        handle_client(client_fd);          /* read request, dispatch, respond */
        close(client_fd);
    }

    /* 5. cleanup */
    log_info(TAG, "Shutting down.");
    close(server_fd);
    unlink(MINIAOSP_SM_SOCKET);
    unlink(MINIAOSP_SM_READY);
    return 0;
}
