#include "log.h"
#include <stdio.h>
#include <string.h>

void miniaosp_log(const char *tag, const char *message) {
    /* Pad tag to 16 chars for aligned output */
    char padded[17];
    memset(padded, ' ', 16);
    padded[16] = '\0';
    size_t len = strlen(tag);
    if (len > 16) len = 16;
    memcpy(padded, tag, len);
    printf("[%s] %s\n", padded, message);
    fflush(stdout);
}
