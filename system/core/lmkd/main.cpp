// mini-AOSP lmkd — Low Memory Killer Daemon (stub for Stage 0)
// Real implementation in Stage 3: monitors /proc/meminfo, kills by oom_score_adj
#include <iostream>
#include <unistd.h>
#include <signal.h>
#include "../liblog/log.h"

static const std::string TAG = "lmkd";

volatile sig_atomic_t g_shutdown = 0;
void handle_signal(int) { g_shutdown = 1; }

int main() {
    signal(SIGTERM, handle_signal);
    signal(SIGINT, handle_signal);

    miniaosp::log(TAG, "Started (stub — no-op in Stage 0)");

    while (!g_shutdown) {
        sleep(5);
    }

    miniaosp::log(TAG, "Shutting down.");
    return 0;
}
