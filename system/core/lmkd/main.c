/* mini-AOSP lmkd — Low Memory Killer Daemon (stub for Stage 0)
 * Real implementation in Stage 8: monitors /proc/meminfo, kills by oom_adj */
#include <unistd.h>
#include "../liblog/log.h"
#include "../libcommon/common.h"

#define TAG "lmkd"

static volatile sig_atomic_t g_shutdown = 0;

int main(void) {
    miniaosp_setup_signals(&g_shutdown);
    miniaosp_log(TAG, "Started (stub -- no-op in Stage 0)");

    while (!g_shutdown)
        sleep(5);

    miniaosp_log(TAG, "Shutting down.");
    return 0;
}
