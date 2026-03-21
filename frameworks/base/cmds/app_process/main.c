/* mini-AOSP app_process — Zygote replacement (stub for Stage 0)
 * Real implementation in Stage 5: preload classes, fork app processes */
#include "../../../../system/core/liblog/log.h"
#include "../../../../system/core/libcommon/common.h"

#define TAG "app_process"

int main(void) {
    miniaosp_log(TAG, "Started (stub -- no-op in Stage 0)");
    miniaosp_log(TAG, "In real AOSP, this is Zygote: preloads classes, forks apps.");
    return 0;
}
