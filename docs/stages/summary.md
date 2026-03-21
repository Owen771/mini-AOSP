## Phase 1 完成後你學到了什麼

### Kernel 層
- `fork()`, `exec()`, `waitpid()`, `kill()` — process 生命週期
- `socket()`, `bind()`, `connect()`, `read()`, `write()` — IPC
- `epoll` — 高效能 event loop
- `SO_PEERCRED` — unforgeable caller identity
- cgroups — resource limits

### Native 層
- **init** — PID 1 的責任：啟動、監控、重啟 service
- **Binder** — handle-based IPC, Parcel 序列化, Proxy/Stub, thread pool, death notification
- **servicemanager** — service discovery (handle 0)
- **Zygote** — fork + specialize, copy-on-write 記憶體共享

### Framework 層
- **AMS** — Activity/Service lifecycle state machine, process priority, OomAdjuster, force-stop
- **PMS** — manifest 掃描, UID 分配, intent resolution
- **Intent** — explicit vs implicit, action-based routing
- **BroadcastReceiver** — pub/sub event system
- **lmkd** — memory pressure → kill by priority → save/restore state

### App 層
- **Activity** 7 個 lifecycle callbacks + save/restore
- **Service** 三種模式 (started/bound/hybrid) + restart policy
- **Process priority** 5 級 + priority inheritance via bindings

### Design Patterns
- Proxy/Stub (RPC)
- Observer (linkToDeath, BroadcastReceiver)
- Service Locator (servicemanager)
- Code Generation (AIDL)
- Event Loop (Looper/Handler)
- Copy-on-Write (Zygote fork)

---

## 接下來：Phase 2

Phase 1 完成後，你已經有一個完整運作的迷你 Android。Phase 2 加入：

- **HAL** — 模擬硬體（Camera, Audio, Sensors）
- **ART** — 自己的 bytecode VM
- **ContentProvider** — 跨 app 資料共享
- **Custom kernel modifications** — 在 Linux kernel 加入 Binder driver
- **Other form factors** — tablet, TV, auto

詳見 `phase-2-advanced.md`。

---

## 學習資源總表

### 必讀

| 資源 | 對應 Stage | 重點 |
|------|-----------|------|
| **AOSP Architecture Overview** — [source.android.com](https://source.android.com/docs/core/architecture) | 全部 | 系統層次圖 |
| **AOSP `init/README.md`** — [cs.android.com](https://cs.android.com/android/platform/superproject/+/main:system/core/init/README.md) | S0-S1 | init.rc 語法、service 行為 |
| **AOSP Binder docs** — [source.android.com/docs/core/architecture/hidl/binder-ipc](https://source.android.com/docs/core/architecture/hidl/binder-ipc) | S2-S3 | Binder 架構 |
| **`man epoll(7)`** | S0 | Looper 的底層 |
| **`man unix(7)`** | S0-S2 | Unix domain socket |
| **`man fork(2)`, `exec(3)`, `waitpid(2)`** | S1, S5 | Process 管理 |

### 推薦閱讀

| 資源 | 對應 Stage | 重點 |
|------|-----------|------|
| **Android Internals: A Confectioner's Cookbook** (Jonathan Levin) | 全部 | 最深入的非官方 internals 參考 |
| **xv6 教學 OS** — [pdos.csail.mit.edu/6.828/xv6](https://pdos.csail.mit.edu/6.828/2024/xv6.html) | S0-S1 | fork/exec/scheduler 的最小實作 |
| **Beej's Guide to Unix IPC** | S0-S2 | Unix IPC 機制全覽 |

### AOSP Source 導航

隨時可以去 [cs.android.com](https://cs.android.com) 搜尋真正的 AOSP source code。
每個 Step 的 "🆚 真正 AOSP 對照" 都有具體檔案路徑。

**最有教學價值的 AOSP 檔案（建議通讀）：**

```
system/core/init/README.md ← init 的完整文件
system/core/init/service.cpp ← service 生命週期
system/core/libutils/Looper.cpp ← event loop
frameworks/native/libs/binder/Parcel.cpp ← 序列化
frameworks/native/cmds/servicemanager/ServiceManager.cpp ← service registry
frameworks/base/core/java/android/os/Looper.java ← Java Looper
frameworks/base/core/java/android/app/ActivityThread.java ← app 主入口（巨大但值得瀏覽）
frameworks/base/services/core/java/.../ActivityManagerService.java ← AMS 核心
```
