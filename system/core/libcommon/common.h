#ifndef MINIAOSP_COMMON_H
#define MINIAOSP_COMMON_H

#include <signal.h>

/* Install SIGTERM+SIGINT handlers that set *flag to 1 */
void miniaosp_setup_signals(volatile sig_atomic_t *flag);

/* Write a string to a file (PID files, ready files, etc.) */
void miniaosp_write_file(const char *path, const char *content);

/* Return 1 if path exists, 0 otherwise */
int miniaosp_file_exists(const char *path);

/* Poll until path exists or timeout_ms elapses. Returns 1 if found. */
int miniaosp_wait_for_file(const char *path, int timeout_ms);

/* Strip trailing \n and \r in-place */
void miniaosp_strip_newlines(char *buf, size_t *len);

#endif /* MINIAOSP_COMMON_H */