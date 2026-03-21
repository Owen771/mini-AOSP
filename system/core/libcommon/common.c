#include "common.h"
#include "constants.h"
#include "../liblog/log.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

/* --- Signal handling ---------------------------------------------------- */

static volatile sig_atomic_t *g_flag_ptr;
static void signal_handler(int sig) { (void)sig; *g_flag_ptr = 1; }

void miniaosp_setup_signals(volatile sig_atomic_t *flag) {
    g_flag_ptr = flag;
    signal(SIGTERM, signal_handler);
    signal(SIGINT,  signal_handler);
}

/* --- File utilities ----------------------------------------------------- */

void miniaosp_write_file(const char *path, const char *content) {
    FILE *f = fopen(path, "w");
    if (f) { fputs(content, f); fclose(f); }
}

int miniaosp_file_exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0;
}

int miniaosp_wait_for_file(const char *path, int timeout_ms) {
    int elapsed = 0;
    while (elapsed < timeout_ms) {
        if (miniaosp_file_exists(path)) return 1;
        usleep(POLL_INTERVAL_US);
        elapsed += POLL_INTERVAL_US / 1000;
    }
    return 0;
}

/* --- String utilities --------------------------------------------------- */

void miniaosp_strip_newlines(char *buf, size_t *len) {
    while (*len > 0 && (buf[*len - 1] == '\n' || buf[*len - 1] == '\r'))
        buf[--(*len)] = '\0';
}

/* --- Logging helper ----------------------------------------------------- */

void miniaosp_log_fmt(const char *tag, const char *fmt, ...) {
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    miniaosp_log(tag, buf);
}