// mini-AOSP app_process — Zygote replacement (stub for Stage 0)
// Real implementation in Stage 3: preload classes, fork app processes
#include <iostream>
#include "../../../../system/core/liblog/log.h"

static const std::string TAG = "app_process";

int main() {
    miniaosp::log(TAG, "Started (stub — no-op in Stage 0)");
    miniaosp::log(TAG, "In real AOSP, this is Zygote: preloads classes, forks apps.");
    return 0;
}
