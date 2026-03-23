/* mini-AOSP init — parses init.rc, fork+exec services in order
 *
 * Flow:
 *   main() → setup signals → write PID file
 *          → parse_init_rc()        — read init.rc into g_services[]
 *          → launch_all_services()  — fork+exec each, wait for readiness
 *          → monitor_children()     — waitpid loop until all exit or SIGTERM
 *          → shutdown_services()    — SIGTERM → grace → SIGKILL
 *          → cleanup_pid_files()    — unlink PIDs, free memory
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <errno.h>
#include "../liblog/log.h"
#include "../libcommon/constants.h"
#include "../libcommon/common.h"

#define TAG "init"

/* One entry per service defined in init.rc */
struct service {
    char name[MAX_NAME];          /* e.g. "servicemanager" */
    char *args[MAX_ARGS + 1];     /* execvp argv, NULL-terminated */
    int  n_args;                  /* number of args (excluding NULL) */
    pid_t pid;                    /* child PID after fork, -1 if not running */
    char wait_for[MAX_PATH_LEN];  /* if set, init polls for this file before next launch */
};

static struct service g_services[MAX_SERVICES]; /* all parsed services */
static int g_n_services = 0;                     /* how many we parsed */
static volatile sig_atomic_t g_shutdown = 0;     /* set to 1 by signal handler */

/* ------------------------------------------------------------------ */
/*  Parsing                                                            */
/* ------------------------------------------------------------------ */

/* Parse an indented option line belonging to the previous service.
 * Currently only supports "    wait_for <path>".
 * Steps: 1. skip leading whitespace  2. match "wait_for "  3. store path */
static void parse_option_line(const char *line) {
    const char *p = line;
    while (*p == ' ' || *p == '\t') p++;      /* 1. skip indent */
    if (strncmp(p, "wait_for ", 9) == 0) {    /* 2. match keyword */
        p += 9;
        while (*p == ' ') p++;
        strncpy(g_services[g_n_services - 1].wait_for, p,
                sizeof(g_services[0].wait_for) - 1); /* 3. store */
    }
}

/* Parse "service <name> <cmd> [args...]" into a struct service.
 * Steps: 1. init empty svc  2. skip "service" token  3. read name
 *        4. read remaining tokens as args  5. bump g_n_services */
static void parse_service_line(char *line) {
    if (g_n_services >= MAX_SERVICES) return;

    /* 1. init empty svc */
    struct service *svc = &g_services[g_n_services];
    memset(svc, 0, sizeof(*svc));
    svc->pid = -1;

    char *saveptr = NULL;
    strtok_r(line, " \t", &saveptr);                /* 2. skip "service" keyword */
    char *tok = strtok_r(NULL, " \t", &saveptr);     /* 3. service name */
    if (!tok) return;
    strncpy(svc->name, tok, MAX_NAME - 1);

    /* 4. remaining tokens → args[] for execvp */
    svc->n_args = 0;
    while ((tok = strtok_r(NULL, " \t", &saveptr)) && svc->n_args < MAX_ARGS) {
        svc->args[svc->n_args++] = strdup(tok);
    }
    svc->args[svc->n_args] = NULL; /* execvp requires NULL terminator */

    if (svc->n_args > 0) g_n_services++; /* 5. only count if has a command */
}

/* Read init.rc line by line, filling g_services[].
 * Steps: 1. open file  2. for each line: strip, skip blanks/comments
 *        3. indented lines → parse_option_line (belongs to prev service)
 *        4. "service ..." lines → parse_service_line (new service)
 * Returns number of services parsed. */
static int parse_init_rc(const char *path) {
    FILE *f = fopen(path, "r");                        /* 1. open */
    if (!f) {
        log_error(TAG, "Cannot open %s", path);
        return 0;
    }

    char line[MAX_LINE];
    while (fgets(line, sizeof(line), f)) {             /* 2. read each line */
        size_t len = strlen(line);
        miniaosp_strip_newlines(line, &len);
        if (len == 0 || line[0] == '#') continue;      /* skip blanks/comments */

        if ((line[0] == ' ' || line[0] == '\t') && g_n_services > 0) {
            parse_option_line(line);                    /* 3. indented → option */
        } else if (strncmp(line, "service ", 8) == 0) {
            parse_service_line(line);                   /* 4. new service */
        }
    }

    fclose(f);
    return g_n_services;
}

/* ------------------------------------------------------------------ */
/*  Service lifecycle                                                   */
/* ------------------------------------------------------------------ */

/* Fork a child process and exec a service binary.
 * Steps: 1. fork() — clone this process
 *        2. child: execvp() — replace with service binary (never returns on success)
 *        3. parent: return child PID */
static pid_t launch_service(struct service *svc) {
    pid_t pid = fork();                              /* 1. clone process */
    if (pid < 0) {
        log_error(TAG, "fork() failed for %s: %s",
                  svc->name, strerror(errno));
        return -1;
    }
    if (pid == 0) {
        /* 2. child — replace ourselves with the service binary */
        execvp(svc->args[0], svc->args);
        /* only reached if exec fails */
        fprintf(stderr, "[init] exec failed for %s: %s\n",
                svc->name, strerror(errno));
        _exit(127);
    }
    return pid;                                      /* 3. parent gets child PID */
}

/* Create runtime dir and write our PID so stop.sh can find us.
 * Steps: 1. mkdir /tmp/mini-aosp  2. write PID to init.pid */
static void init_write_pid(void) {
    mkdir(MINIAOSP_RUNTIME_DIR, 0755);
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", getpid());
    miniaosp_write_file(MINIAOSP_INIT_PID, buf);
}

/* Launch every service in g_services[] sequentially.
 * For each service:
 *   1. fork+exec via launch_service()
 *   2. record PID in svc->pid
 *   3. write PID file for stop.sh
 *   4. if wait_for is set: poll for readiness file (e.g. servicemanager.ready)
 *      otherwise: sleep GRACE_PERIOD_US (200ms) before next service */
static void launch_all_services(void) {
    for (int i = 0; i < g_n_services; i++) {
        struct service *svc = &g_services[i];
        pid_t pid = launch_service(svc);             /* 1. fork+exec */
        if (pid <= 0) continue;

        svc->pid = pid;                              /* 2. record PID */
        log_info(TAG, "Starting %s (PID %d)...", svc->name, pid);

        /* 3. write PID file → /tmp/mini-aosp/<name>.pid */
        char pid_path[MAX_PATH_LEN];
        snprintf(pid_path, sizeof(pid_path),
                 MINIAOSP_RUNTIME_DIR "/%s.pid", svc->name);
        char pid_str[32];
        snprintf(pid_str, sizeof(pid_str), "%d", pid);
        miniaosp_write_file(pid_path, pid_str);

        /* 4. wait for readiness or use default grace period */
        if (svc->wait_for[0] != '\0') {
            log_info(TAG, "Waiting for %s ready...", svc->name);
            if (miniaosp_wait_for_file(svc->wait_for, WAIT_FOR_TIMEOUT_MS))
                log_info(TAG, "%s is ready.", svc->name);
            else
                log_warn(TAG, "Timeout waiting for %s", svc->wait_for);
        } else {
            usleep(GRACE_PERIOD_US);
        }
    }
}

/* Poll loop: check for exited children every 100ms.
 * Steps per iteration:
 *   1. waitpid(WNOHANG) — non-blocking check for any exited child
 *   2. if a child exited: log it, mark svc->pid = -1
 *   3. if ALL children exited: return (normal shutdown)
 *   4. if g_shutdown set by signal handler: return (signal shutdown) */
static void monitor_children(void) {
    while (!g_shutdown) {                            /* 4. check signal */
        int status;
        pid_t exited = waitpid(-1, &status, WNOHANG); /* 1. non-blocking check */
        if (exited > 0) {
            for (int i = 0; i < g_n_services; i++) {
                struct service *svc = &g_services[i];
                if (svc->pid != exited) continue;

                if (WIFEXITED(status))
                    log_info(TAG, "%s exited with code %d",
                             svc->name, WEXITSTATUS(status));
                else if (WIFSIGNALED(status))
                    log_info(TAG, "%s killed by signal %d",
                             svc->name, WTERMSIG(status));
                else
                    log_info(TAG, "%s exited (unknown status)", svc->name);
                svc->pid = -1;
                break;
            }

            /* Check if all services exited */
            int any_alive = 0;
            for (int i = 0; i < g_n_services; i++) {
                if (g_services[i].pid > 0) { any_alive = 1; break; }
            }
            if (!any_alive) {
                log_info(TAG, "All services exited. Shutting down.");
                return;
            }
        }
        usleep(POLL_INTERVAL_US * 2); /* 100ms poll */
    }
}

/* Graceful shutdown sequence:
 *   1. send SIGTERM to all living children
 *   2. wait SHUTDOWN_GRACE_US (2s) for them to exit
 *   3. send SIGKILL to any that are still alive */
static void shutdown_services(void) {
    log_info(TAG, "Shutdown signal received. Stopping services...");

    /* 1. SIGTERM — polite request to exit */
    for (int i = 0; i < g_n_services; i++) {
        struct service *svc = &g_services[i];
        if (svc->pid > 0) {
            kill(svc->pid, SIGTERM);
            log_info(TAG, "Sent SIGTERM to %s (PID %d)",
                     svc->name, svc->pid);
        }
    }

    usleep(SHUTDOWN_GRACE_US);                       /* 2. grace period */

    /* 3. SIGKILL — force kill stragglers */
    for (int i = 0; i < g_n_services; i++) {
        if (g_services[i].pid > 0) {
            int status;
            if (waitpid(g_services[i].pid, &status, WNOHANG) == 0)
                kill(g_services[i].pid, SIGKILL);
        }
    }
}

/* Remove PID files and free strdup'd args.
 * Steps: 1. unlink /tmp/mini-aosp/<name>.pid for each service
 *        2. free each strdup'd arg string
 *        3. unlink init.pid */
static void cleanup_pid_files(void) {
    for (int i = 0; i < g_n_services; i++) {
        char pid_path[MAX_PATH_LEN];
        snprintf(pid_path, sizeof(pid_path),
                 MINIAOSP_RUNTIME_DIR "/%s.pid", g_services[i].name);
        unlink(pid_path);

        for (int j = 0; j < g_services[i].n_args; j++)
            free(g_services[i].args[j]);
    }
    unlink(MINIAOSP_INIT_PID);
}

/* ------------------------------------------------------------------ */
/*  main                                                                */
/* ------------------------------------------------------------------ */

/* init entry point.
 * Steps: 1. install signal handlers (SIGTERM/SIGINT → g_shutdown=1)
 *        2. write PID file for stop.sh
 *        3. parse init.rc → fill g_services[]
 *        4. fork+exec each service in order
 *        5. wait for children (blocking loop)
 *        6. on signal: graceful shutdown
 *        7. cleanup PID files + free memory */
int main(int argc, char *argv[]) {
    miniaosp_setup_signals(&g_shutdown);             /* 1. signal handlers */
    init_write_pid();                                /* 2. write PID file */

    const char *rc_path = (argc > 1) ? argv[1] : "system/core/rootdir/init.rc";
    log_info(TAG, "Parsing %s...", rc_path);

    if (parse_init_rc(rc_path) == 0) {               /* 3. parse init.rc */
        log_error(TAG, "No services found in %s", rc_path);
        return 1;
    }

    launch_all_services();                           /* 4. fork+exec all */
    log_info(TAG, "All services started. Waiting...");
    monitor_children();                              /* 5. waitpid loop */

    if (g_shutdown)
        shutdown_services();                         /* 6. SIGTERM → SIGKILL */

    cleanup_pid_files();                             /* 7. cleanup */
    log_info(TAG, "Shutdown complete.");
    return 0;
}
