/* mini-AOSP servicemanager — Unix socket service registry
 * Protocol: ADD_SERVICE <name> <socket_path>\n → OK\n
 *           GET_SERVICE <name>\n → <socket_path>\n or NOT_FOUND\n
 *           LIST_SERVICES\n → <name1> <name2> ...\n
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

struct service_entry {
    char name[MAX_NAME];
    char socket_path[MAX_PATH_LEN];
};

static struct service_entry g_services[MAX_SERVICES];
static int g_n_services = 0;
static volatile sig_atomic_t g_shutdown = 0;

/* ------------------------------------------------------------------ */
/*  Service registry                                                    */
/* ------------------------------------------------------------------ */

static int find_service(const char *name) {
    for (int i = 0; i < g_n_services; i++) {
        if (strcmp(g_services[i].name, name) == 0) return i;
    }
    return -1;
}

static void handle_add_service(char **saveptr, char *response, size_t rsize) {
    char *name = strtok_r(NULL, " \t", saveptr);
    char *path = strtok_r(NULL, " \t", saveptr);
    if (!name || !path) {
        snprintf(response, rsize,
                 "ERROR: usage: ADD_SERVICE <name> <socket_path>\n");
        return;
    }
    int idx = find_service(name);
    if (idx < 0 && g_n_services < MAX_SERVICES)
        idx = g_n_services++;
    if (idx >= 0) {
        strncpy(g_services[idx].name, name, MAX_NAME - 1);
        strncpy(g_services[idx].socket_path, path, MAX_PATH_LEN - 1);
        miniaosp_log_fmt(TAG, "Registered service: %s -> %s", name, path);
        snprintf(response, rsize, "OK\n");
    }
}

static void handle_get_service(char **saveptr, char *response, size_t rsize) {
    char *name = strtok_r(NULL, " \t", saveptr);
    if (!name) return;
    int idx = find_service(name);
    if (idx >= 0)
        snprintf(response, rsize, "%s\n", g_services[idx].socket_path);
    else
        snprintf(response, rsize, "NOT_FOUND\n");
}

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

static void handle_client(int client_fd) {
    char buf[MAX_LINE];
    ssize_t n = read(client_fd, buf, sizeof(buf) - 1);
    if (n <= 0) return;
    buf[n] = '\0';

    size_t len = (size_t)n;
    miniaosp_strip_newlines(buf, &len);

    char response[MAX_LINE];
    response[0] = '\0';

    char *saveptr = NULL;
    char *command = strtok_r(buf, " \t", &saveptr);
    if (!command) return;

    if (strcmp(command, "ADD_SERVICE") == 0)
        handle_add_service(&saveptr, response, sizeof(response));
    else if (strcmp(command, "GET_SERVICE") == 0)
        handle_get_service(&saveptr, response, sizeof(response));
    else if (strcmp(command, "LIST_SERVICES") == 0)
        handle_list_services(response, sizeof(response));
    else
        snprintf(response, sizeof(response), "ERROR: unknown command\n");

    write(client_fd, response, strlen(response));
}

/* ------------------------------------------------------------------ */
/*  Socket setup                                                        */
/* ------------------------------------------------------------------ */

static int create_listening_socket(const char *path) {
    unlink(path);

    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        miniaosp_log(TAG, "ERROR: socket() failed");
        return -1;
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        miniaosp_log(TAG, "ERROR: bind() failed");
        close(fd);
        return -1;
    }
    if (listen(fd, 10) < 0) {
        miniaosp_log(TAG, "ERROR: listen() failed");
        close(fd);
        return -1;
    }
    return fd;
}

static void signal_readiness(void) {
    char pid_str[32];
    snprintf(pid_str, sizeof(pid_str), "%d", getpid());
    miniaosp_write_file(MINIAOSP_SM_READY, pid_str);
}

/* ------------------------------------------------------------------ */
/*  main                                                                */
/* ------------------------------------------------------------------ */

int main(void) {
    miniaosp_setup_signals(&g_shutdown);
    mkdir(MINIAOSP_RUNTIME_DIR, 0755);

    int server_fd = create_listening_socket(MINIAOSP_SM_SOCKET);
    if (server_fd < 0) return 1;

    miniaosp_log(TAG, "Listening on " MINIAOSP_SM_SOCKET);
    signal_readiness();

    while (!g_shutdown) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(server_fd, &readfds);

        struct timeval tv = { .tv_sec = SELECT_TIMEOUT_SEC, .tv_usec = 0 };
        int ready = select(server_fd + 1, &readfds, NULL, NULL, &tv);
        if (ready < 0) {
            if (errno == EINTR) continue;
            break;
        }
        if (ready == 0) continue;

        int client_fd = accept(server_fd, NULL, NULL);
        if (client_fd < 0) {
            if (errno == EINTR) continue;
            miniaosp_log(TAG, "ERROR: accept() failed");
            continue;
        }
        handle_client(client_fd);
        close(client_fd);
    }

    miniaosp_log(TAG, "Shutting down.");
    close(server_fd);
    unlink(MINIAOSP_SM_SOCKET);
    unlink(MINIAOSP_SM_READY);
    return 0;
}
