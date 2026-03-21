/* mini-AOSP init — parses init.rc, fork+exec services in order */
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

struct service {
    char name[MAX_NAME];
    char *args[MAX_ARGS + 1]; /* NULL-terminated for execvp */
    int  n_args;
    pid_t pid;
    char wait_for[MAX_PATH_LEN]; /* optional: wait for this file before starting next */
};

static struct service g_services[MAX_SERVICES];
static int g_n_services = 0;
static volatile sig_atomic_t g_shutdown = 0;

/* ------------------------------------------------------------------ */
/*  Parsing                                                            */
/* ------------------------------------------------------------------ */

/* Parse a single "    wait_for <path>" option line */
static void parse_option_line(const char *line) {
    const char *p = line;
    while (*p == ' ' || *p == '\t') p++;
    if (strncmp(p, "wait_for ", 9) == 0) {
        p += 9;
        while (*p == ' ') p++;
        strncpy(g_services[g_n_services - 1].wait_for, p,
                sizeof(g_services[0].wait_for) - 1);
    }
}

/* Parse a "service <name> <cmd> [args...]" line */
static void parse_service_line(char *line) {
    if (g_n_services >= MAX_SERVICES) return;

    struct service *svc = &g_services[g_n_services];
    memset(svc, 0, sizeof(*svc));
    svc->pid = -1;

    char *saveptr = NULL;
    strtok_r(line, " \t", &saveptr);                /* skip "service" */
    char *tok = strtok_r(NULL, " \t", &saveptr);     /* name */
    if (!tok) return;
    strncpy(svc->name, tok, MAX_NAME - 1);

    svc->n_args = 0;
    while ((tok = strtok_r(NULL, " \t", &saveptr)) && svc->n_args < MAX_ARGS) {
        svc->args[svc->n_args++] = strdup(tok);
    }
    svc->args[svc->n_args] = NULL;

    if (svc->n_args > 0) g_n_services++;
}

static int parse_init_rc(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) {
        miniaosp_log_fmt(TAG, "ERROR: Cannot open %s", path);
        return 0;
    }

    char line[MAX_LINE];
    while (fgets(line, sizeof(line), f)) {
        size_t len = strlen(line);
        miniaosp_strip_newlines(line, &len);
        if (len == 0 || line[0] == '#') continue;

        if ((line[0] == ' ' || line[0] == '\t') && g_n_services > 0) {
            parse_option_line(line);
        } else if (strncmp(line, "service ", 8) == 0) {
            parse_service_line(line);
        }
    }

    fclose(f);
    return g_n_services;
}

/* ------------------------------------------------------------------ */
/*  Service lifecycle                                                   */
/* ------------------------------------------------------------------ */

static pid_t launch_service(struct service *svc) {
    pid_t pid = fork();
    if (pid < 0) {
        miniaosp_log_fmt(TAG, "ERROR: fork() failed for %s: %s",
                         svc->name, strerror(errno));
        return -1;
    }
    if (pid == 0) {
        execvp(svc->args[0], svc->args);
        fprintf(stderr, "[init] exec failed for %s: %s\n",
                svc->name, strerror(errno));
        _exit(127);
    }
    return pid;
}

static void init_write_pid(void) {
    mkdir(MINIAOSP_RUNTIME_DIR, 0755);
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", getpid());
    miniaosp_write_file(MINIAOSP_INIT_PID, buf);
}

static void launch_all_services(void) {
    for (int i = 0; i < g_n_services; i++) {
        struct service *svc = &g_services[i];
        pid_t pid = launch_service(svc);
        if (pid <= 0) continue;

        svc->pid = pid;
        miniaosp_log_fmt(TAG, "Starting %s (PID %d)...", svc->name, pid);

        /* Write child PID file */
        char pid_path[MAX_PATH_LEN];
        snprintf(pid_path, sizeof(pid_path),
                 MINIAOSP_RUNTIME_DIR "/%s.pid", svc->name);
        char pid_str[32];
        snprintf(pid_str, sizeof(pid_str), "%d", pid);
        miniaosp_write_file(pid_path, pid_str);

        /* Wait for readiness or use default grace period */
        if (svc->wait_for[0] != '\0') {
            miniaosp_log_fmt(TAG, "Waiting for %s ready...", svc->name);
            if (miniaosp_wait_for_file(svc->wait_for, WAIT_FOR_TIMEOUT_MS))
                miniaosp_log_fmt(TAG, "%s is ready.", svc->name);
            else
                miniaosp_log_fmt(TAG, "WARNING: timeout waiting for %s",
                                 svc->wait_for);
        } else {
            usleep(GRACE_PERIOD_US);
        }
    }
}

static void monitor_children(void) {
    while (!g_shutdown) {
        int status;
        pid_t exited = waitpid(-1, &status, WNOHANG);
        if (exited > 0) {
            for (int i = 0; i < g_n_services; i++) {
                struct service *svc = &g_services[i];
                if (svc->pid != exited) continue;

                if (WIFEXITED(status))
                    miniaosp_log_fmt(TAG, "%s exited with code %d",
                                     svc->name, WEXITSTATUS(status));
                else if (WIFSIGNALED(status))
                    miniaosp_log_fmt(TAG, "%s killed by signal %d",
                                     svc->name, WTERMSIG(status));
                else
                    miniaosp_log_fmt(TAG, "%s exited (unknown status)", svc->name);
                svc->pid = -1;
                break;
            }

            /* Check if all services exited */
            int any_alive = 0;
            for (int i = 0; i < g_n_services; i++) {
                if (g_services[i].pid > 0) { any_alive = 1; break; }
            }
            if (!any_alive) {
                miniaosp_log(TAG, "All services exited. Shutting down.");
                return;
            }
        }
        usleep(POLL_INTERVAL_US * 2); /* 100ms poll */
    }
}

static void shutdown_services(void) {
    miniaosp_log(TAG, "Shutdown signal received. Stopping services...");
    for (int i = 0; i < g_n_services; i++) {
        struct service *svc = &g_services[i];
        if (svc->pid > 0) {
            kill(svc->pid, SIGTERM);
            miniaosp_log_fmt(TAG, "Sent SIGTERM to %s (PID %d)",
                             svc->name, svc->pid);
        }
    }
    usleep(SHUTDOWN_GRACE_US);
    for (int i = 0; i < g_n_services; i++) {
        if (g_services[i].pid > 0) {
            int status;
            if (waitpid(g_services[i].pid, &status, WNOHANG) == 0)
                kill(g_services[i].pid, SIGKILL);
        }
    }
}

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

int main(int argc, char *argv[]) {
    miniaosp_setup_signals(&g_shutdown);
    init_write_pid();

    const char *rc_path = (argc > 1) ? argv[1] : "system/core/rootdir/init.rc";
    miniaosp_log_fmt(TAG, "Parsing %s...", rc_path);

    if (parse_init_rc(rc_path) == 0) {
        miniaosp_log_fmt(TAG, "ERROR: No services found in %s", rc_path);
        return 1;
    }

    launch_all_services();
    miniaosp_log(TAG, "All services started. Waiting...");
    monitor_children();

    if (g_shutdown)
        shutdown_services();

    cleanup_pid_files();
    miniaosp_log(TAG, "Shutdown complete.");
    return 0;
}
