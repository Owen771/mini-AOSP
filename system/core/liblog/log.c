#include "log.h"
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

void miniaosp_log(const char *tag, const char *message) {
    /* Pad tag to 16 chars for aligned output */
    char padded[17];
    memset(padded, ' ', 16);
    padded[16] = '\0';
    size_t len = strlen(tag);
    if (len > 16) len = 16;
    memcpy(padded, tag, len);

    /* Always emit color — output is meant for terminals even when
     * captured to a temp file (e.g. run-test.sh → cat back to tty).
     * Set MINIAOSP_NO_COLOR=1 to disable. */
    const char *no_color = getenv("MINIAOSP_NO_COLOR");
    if (no_color && no_color[0] == '1')
        printf("[%s] %s\n", padded, message);
    else
        printf("%s[%s]\033[0m %s\n", tag_color(tag), padded, message);
    fflush(stdout);
}
