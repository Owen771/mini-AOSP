#ifndef MINIAOSP_LOG_H
#define MINIAOSP_LOG_H

enum log_level { LOG_LEVEL_INFO, LOG_LEVEL_WARN, LOG_LEVEL_ERROR };

/* Level-aware tagged logging: I/[tag] message, W/[tag] ..., E/[tag] ...
 * log_error writes to stderr; log_info and log_warn write to stdout. */
void log_info(const char *tag, const char *fmt, ...)
    __attribute__((format(printf, 2, 3)));
void log_warn(const char *tag, const char *fmt, ...)
    __attribute__((format(printf, 2, 3)));
void log_error(const char *tag, const char *fmt, ...)
    __attribute__((format(printf, 2, 3)));

#endif /* MINIAOSP_LOG_H */
