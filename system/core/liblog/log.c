#include "log.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ANSI color codes — each tag gets a fixed color */
static const char *tag_color(const char *tag) {
    if (strcmp(tag, "init") == 0)             return "\033[36m"; /* cyan */
    if (strcmp(tag, "servicemanager") == 0)   return "\033[33m"; /* yellow */
    if (strcmp(tag, "system_server") == 0)    return "\033[35m"; /* magenta */
    if (strcmp(tag, "HelloApp") == 0)         return "\033[32m"; /* green */
    if (strcmp(tag, "lmkd") == 0)             return "\033[34m"; /* blue */
    return "\033[37m";                                           /* white */
}

static const char *level_prefix(enum log_level level) {
    switch (level) {
    case LOG_LEVEL_WARN:  return "W";
    case LOG_LEVEL_ERROR: return "E";
    default:              return "I";
    }
}

/* Internal: format and write a log line.
 * log_error → stderr, others → stdout. */
static void log_write(enum log_level level, const char *tag,
                       const char *fmt, va_list args) {
    char message[1024];
    vsnprintf(message, sizeof(message), fmt, args);

    /* Pad tag to 16 chars for aligned output */
    char padded[17];
    memset(padded, ' ', 16);
    padded[16] = '\0';
    size_t len = strlen(tag);
    if (len > 16) len = 16;
    memcpy(padded, tag, len);

    FILE *out = (level == LOG_LEVEL_ERROR) ? stderr : stdout;
    const char *pfx = level_prefix(level);

    const char *no_color = getenv("MINIAOSP_NO_COLOR");
    if (no_color && no_color[0] == '1')
        fprintf(out, "%s/[%s] %s\n", pfx, padded, message);
    else
        fprintf(out, "%s/%s[%s]\033[0m %s\n", pfx, tag_color(tag), padded, message);
    fflush(out);
}

void log_info(const char *tag, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    log_write(LOG_LEVEL_INFO, tag, fmt, args);
    va_end(args);
}

void log_warn(const char *tag, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    log_write(LOG_LEVEL_WARN, tag, fmt, args);
    va_end(args);
}

void log_error(const char *tag, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    log_write(LOG_LEVEL_ERROR, tag, fmt, args);
    va_end(args);
}
