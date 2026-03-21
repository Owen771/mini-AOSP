#ifndef MINIAOSP_CONSTANTS_H
#define MINIAOSP_CONSTANTS_H

/* Runtime paths */
#define MINIAOSP_RUNTIME_DIR  "/tmp/mini-aosp"
#define MINIAOSP_SM_SOCKET    MINIAOSP_RUNTIME_DIR "/servicemanager.sock"
#define MINIAOSP_SM_READY     MINIAOSP_RUNTIME_DIR "/servicemanager.ready"
#define MINIAOSP_INIT_PID     MINIAOSP_RUNTIME_DIR "/init.pid"

/* Buffer sizes */
#define MAX_NAME       128
#define MAX_PATH_LEN   512
#define MAX_LINE       4096
#define MAX_ARGS       16
#define MAX_SERVICES   64

/* Timeouts and intervals (milliseconds unless noted) */
#define POLL_INTERVAL_US     50000   /* 50ms  — file-poll sleep */
#define GRACE_PERIOD_US     200000   /* 200ms — between service launches */
#define SHUTDOWN_GRACE_US  2000000   /* 2s    — SIGTERM→SIGKILL window */
#define WAIT_FOR_TIMEOUT_MS  10000   /* 10s   — wait_for dependency */
#define SELECT_TIMEOUT_SEC       1   /* 1s    — servicemanager select() */

#endif /* MINIAOSP_CONSTANTS_H */