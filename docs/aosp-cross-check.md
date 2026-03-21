# mini-AOSP vs 真正 AOSP 對照檢查

> 逐 Stage 比對我們的設計跟真正 AOSP 的實作，
> 找出需要修正的差異、可以接受的簡化、以及必須遵守的結構約束。

---

## 發現的問題（需要修正）

### 🔴 Issue 1：Zygote 用 fork+exec 而非純 fork

| | 真正 AOSP | 我們的設計 |
|---|---|---|
| **fork 行為** | `fork()` — child 繼承整個 ART VM state（copy-on-write） | `fork()` + `exec("java -jar")` — 全新 JVM |
| **記憶體共享** | 7,000 個 preloaded class 共享，省 ~30MB/app | 無共享，每個 JVM 獨立載入 |
| **啟動速度** | fork ~10ms | exec java ~1-2s |

**影響：** 這是跟真正 AOSP 最大的結構性差異。

**修正方案（Stage 5 學習指南需要更新）：**
- Phase 1 先用 `fork+exec`（因為我們用 OpenJDK 不是 ART，無法嵌入 VM）
- 在文件中明確說明這個差異和原因
- Phase 2 改用 embedded JVM（`JNI_CreateJavaVM`）可以做到真正的 fork-without-exec

### 🔴 Issue 2：lmkd 應該是獨立 daemon，不在 system_server 裡

| | 真正 AOSP | Stage 8 設計 |
|---|---|---|
| **位置** | 獨立 C daemon，init 啟動 | 設計文件暗示在 system_server 裡 |
| **語言** | C | 設計文件沒明確指定 |
| **溝通** | 跟 AMS 透過 socket 互動 | 直接呼叫 AMS 方法 |

**影響：** 違反 AOSP 的 process boundary。lmkd 必須在 system_server 外面，
因為如果它在裡面，system_server OOM 時 lmkd 也跟著死。

**修正方案：**
- lmkd 保持在 `system/core/lmkd/main.cpp`（已有 stub）
- init.rc 加入 `service lmkd`
- lmkd 跟 AMS 之間用 socket 通訊（不是 Binder——避免依賴 Binder 的 process 被 kill）
- AMS 定期把 oom_adj 資訊推給 lmkd

### 🟡 Issue 3：init 應該等 servicemanager.ready 而非固定 sleep

| | 真正 AOSP | 我們的設計 |
|---|---|---|
| **等待機制** | init 監聽 `property:servicemanager.ready=true` 才啟動 system_server | `usleep(500000)` 固定等 500ms |

**修正方案（Stage 1）：**
- servicemanager 啟動成功後 set property `servicemanager.ready=true`
- init 的 property trigger 機制偵測到後才啟動下一個 service
- 或者更簡單：servicemanager 啟動成功後在 socket 上寫 "READY\n"，init 讀到才繼續

### 🟡 Issue 4：Lifecycle transition 應用 Transaction 模式

| | 真正 AOSP | 我們的設計 |
|---|---|---|
| **呼叫方式** | `ClientTransaction` + `LaunchActivityItem` + `TransactionExecutor` | AMS 直接呼叫 `scheduleLaunchActivity()` |

**影響：** 功能上沒差，但真正 AOSP 用 Transaction 物件是為了能 batch 多個 lifecycle 變化。

**決定：** 保持我們的直接呼叫方式。Transaction pattern 是 optimization，不是 architecture。
在學習指南裡提一下就好。

---

## 逐 Stage 對照

### Stage 0：Build + Looper + Parcel

| 元件 | 真正 AOSP | mini-AOSP | 差異可接受？ |
|------|----------|-----------|------------|
| **C++ Looper** | `system/core/libutils/Looper.cpp` — epoll + timerfd + eventfd | epoll + 手動 timeout 計算 | ✅ 核心機制相同 |
| **Kotlin Looper** | `frameworks/base/core/java/android/os/Looper.java` — native epoll via JNI `nativePollOnce` | 純 Kotlin `Object.wait()/notify()` | ✅ 行為相同，底層不同 |
| **Handler** | `frameworks/base/core/java/android/os/Handler.java` | `frameworks/base/core/kotlin/os/Handler.kt` | ✅ API 一致 |
| **MessageQueue** | `frameworks/base/core/java/android/os/MessageQueue.java` — linked list by `when` | Priority queue by `when` | ✅ 功能等價 |
| **Parcel** | `frameworks/native/libs/binder/Parcel.cpp` — 4-byte aligned, UTF-16 strings | 4-byte aligned, UTF-8 strings | ✅ UTF-8 更簡單，wire format 不同但概念相同 |

**AOSP 參考檔案：**
```
system/core/libutils/Looper.cpp              → pollOnce(), addFd(), sendMessageDelayed()
frameworks/base/core/java/android/os/Looper.java
frameworks/base/core/java/android/os/Handler.java
frameworks/base/core/java/android/os/MessageQueue.java → next(), enqueueMessage()
frameworks/native/libs/binder/Parcel.cpp      → writeInt32(), writeString16(), flatten()
```

---

### Stage 1：init 強化

| 元件 | 真正 AOSP | mini-AOSP | 差異可接受？ |
|------|----------|-----------|------------|
| **init 入口** | `system/core/init/main.cpp` → 分流到 FirstStageMain / SecondStageMain | 單一 main() | ✅ 兩階段 init 是 bootloader 相關，我們不需要 |
| **rc 解析** | `Parser` + `ServiceParser` + `ActionParser` — token state machine | 簡單 line-by-line | ✅ 語法子集足夠教學 |
| **Service struct** | `Service` class — ~40 個 flags + options | 簡化版 ~5 個 fields | ✅ 核心 flag 有就好 |
| **Crash-restart** | `Service::Reap()` — crash count + `SVC_CRITICAL` flag + restart_period | exponential backoff | ✅ 概念相同 |
| **Property system** | 共享記憶體 `__system_property_area_init()` + `property_service` socket | Unix socket | ✅ 功能相同，性能不同 |
| **Trigger system** | `on property:X=Y`, `on boot` 等 action triggers | 無（Stage 1 不實作） | 🟡 之後可加 |

**真正 AOSP 的 Service::Reap() 流程（我們的 crash-restart 應該 follow）：**

1. `KillProcessGroup()` — kill 整個 process group
2. 清理 socket resources
3. 計算 crash count（time window 內）
4. 如果是 `SVC_CRITICAL` 且 crash > 4 次 → reboot（我們不需要）
5. 如果是 `SVC_ONESHOT` → 設 `SVC_DISABLED`，不重啟
6. 否則 → 設 `SVC_RESTARTING`，等 restart_period 後重啟

**我們應該實作的：** 步驟 3-6。步驟 1 (process group kill) 可以簡化成 `kill(pid)` 。

**AOSP 參考檔案：**
```
system/core/init/service.cpp         → Service::Reap(), Service::Start(), Service::Restart()
system/core/init/service.h           → Service class, flags 定義
system/core/init/property_service.cpp → PropertyInit(), StartPropertyService()
system/core/init/README.md           → 完整文件
```

---

### Stage 2：Binder IPC

| 元件 | 真正 AOSP | mini-AOSP | 差異可接受？ |
|------|----------|-----------|------------|
| **Transport** | Kernel driver `/dev/binder` + `ioctl(BINDER_WRITE_READ)` | Unix domain socket `read()/write()` | ✅ AOSP 自己也有 RPC Binder 用 socket |
| **IPCThreadState** | `frameworks/native/libs/binder/IPCThreadState.cpp` — per-thread, mIn/mOut Parcels | 同名 class，同結構 | ✅ |
| **Transaction 格式** | `binder_transaction_data` — handle, code, flags, data_size, data_ptr | 自定義 wire format — handle, code, flags, data_len, data | ✅ 欄位對應 |
| **Caller identity** | Kernel stamp UID/PID 到 transaction | `SO_PEERCRED` | ✅ 都是不可偽造的 |
| **Thread pool** | `ProcessState::startThreadPool()` + kernel dispatch | 自己管理的 thread pool | ✅ |
| **linkToDeath** | Kernel 的 `BR_DEAD_BINDER` 通知 | Socket EOF 偵測 | ✅ 功能等價 |
| **Proxy/Stub 命名** | `BpXxx`（Binder Proxy） / `BnXxx`（Binder Native） | `XxxProxy` / `XxxStub` | 🟡 改用 AOSP 命名更好 |
| **AIDL** | `system/tools/aidl/` — C++ compiler, yacc grammar | Python script, regex | ✅ 教學夠用 |

**重要發現：AOSP 的 RPC Binder**

Android 已經開發了 [RPC Binder](https://2net.co.uk/slides/aosp-aaos-meetup/2023-july-dbrazdil-rpc-binder.pdf)——
用 POSIX socket（`AF_INET`, `AF_UNIX`, `AF_VSOCK`）做 Binder transport。
**我們的 Unix socket 方案跟這個方向完全一致。**

**建議修正：Proxy/Stub 命名**

把 `ICalculatorProxy` → `BpCalculator`，`ICalculatorStub` → `BnCalculator`。
這樣讀 AOSP source code 時能直接對應。在學習指南裡解釋 Bp = Binder Proxy, Bn = Binder Native。

**AOSP 參考檔案：**
```
frameworks/native/libs/binder/IPCThreadState.cpp → transact(), talkWithDriver(), executeCommand()
frameworks/native/libs/binder/BpBinder.cpp       → transact(), linkToDeath(), sendObituary()
frameworks/native/libs/binder/Binder.cpp         → BBinder::transact(), onTransact()
frameworks/native/libs/binder/ProcessState.cpp   → startThreadPool(), spawnPooledThread()
frameworks/native/libs/binder/Parcel.cpp         → writeTransactionData()
system/tools/aidl/                               → AIDL compiler source
```

---

### Stage 3：servicemanager

| 元件 | 真正 AOSP | mini-AOSP | 差異可接受？ |
|------|----------|-----------|------------|
| **成為 context manager** | `ProcessState::becomeContextManager()` → `ioctl()` 到 kernel | 綁定 well-known socket path | ✅ |
| **Handle 0** | Kernel 保留 handle 0 給 context manager | 自己的 handle map 裡保留 0 | ✅ |
| **Permission check** | `Access.cpp` — SELinux `selinux_check_access()` | 無 permission check | ✅ Phase 1 不需要 |
| **ready 通知** | Set property `servicemanager.ready=true` | 🔴 目前沒有 | 🔴 需要加 |
| **Lazy service** | 未找到時透過 property 觸發啟動 | 無 | ✅ 可跳過 |
| **VINTF manifest** | `isDeclared()` 查 VINTF | 無 | ✅ 無 HAL |

**真正 AOSP servicemanager 的 main() 流程：**
```cpp
1. ProcessState::self() — 開啟 /dev/binder
2. ps->setThreadPoolMaxThreadCount(0) — 單 thread
3. ServiceManager sm = new ServiceManager()
4. ps->becomeContextManager() — 成為 handle 0
5. Looper::prepare() — 建立 event loop
6. looper->addFd(binder_fd) — 監聽 binder fd
7. SetProperty("servicemanager.ready", "true") — 通知 init
8. while(true) looper->pollAll(-1) — event loop
```

**我們需要 follow 的結構：**
1. 建立 socket ✅
2. 綁定為 well-known address（handle 0）✅
3. Set property 或發信號通知 init 🔴 需要加
4. Enter event loop ✅

**AOSP 參考檔案：**
```
frameworks/native/cmds/servicemanager/main.cpp            → main()
frameworks/native/cmds/servicemanager/ServiceManager.cpp  → addService(), getService()
frameworks/native/cmds/servicemanager/Access.cpp          → canAdd(), canFind(), canList()
```

---

### Stage 4：system_server + Managers

| 元件 | 真正 AOSP | mini-AOSP | 差異可接受？ |
|------|----------|-----------|------------|
| **三階段 boot** | `startBootstrapServices()` → `startCoreServices()` → `startOtherServices()` | 同 | ✅ |
| **AMS** | `ActivityManagerService.java` — 23,000 行 | Stub → 逐步完善 | ✅ |
| **PMS** | `PackageManagerService.java` — 12,000 行 | Stub → 逐步完善 | ✅ |
| **Service 數量** | 100+ services | 3 (AMS, PMS, PropertyManager) | ✅ 教學足夠 |
| **Watchdog** | `Watchdog.java` — 監控 system_server hang | 無 | ✅ 可跳過 |

**真正 AOSP SystemServer.java 的 startBootstrapServices()：**
```java
// 順序非常重要：
1. Installer — 用於安裝 app 的 native service
2. DeviceIdentifiersPolicyService
3. UriGrantsManagerService
4. ActivityManagerService.Lifecycle.startService() ← AMS 最先
5. PowerManagerService ← 很多 service 依賴它
6. RecoverySystemService
7. LightsService
8. DisplayManagerService
9. PackageManagerService ← PMS 在 AMS 之後
10. UserManagerService
11. OverlayManagerService
12. SensorPrivacyService
```

**重點：AMS 必須在 PMS 之前啟動。** 我們的 `startBootstrapServices()` 也要保持這個順序。

**AOSP 參考檔案：**
```
frameworks/base/services/java/com/android/server/SystemServer.java
  → main(), run(), startBootstrapServices()
frameworks/base/services/core/java/com/android/server/am/ActivityManagerService.java
frameworks/base/services/core/java/com/android/server/pm/PackageManagerService.java
```

---

### Stage 5：Zygote

| 元件 | 真正 AOSP | mini-AOSP | 差異可接受？ |
|------|----------|-----------|------------|
| **入口** | `app_main.cpp` → `AndroidRuntime::start()` → `ZygoteInit.main()` | `app_process/main.cpp`（C++） | ✅ 我們用 C++ |
| **Preload** | `ZygoteInit.preload()` — 7,000 class + resources + .so | 幾乎空（無 ART） | ✅ Phase 1 限制 |
| **Fork** | `Zygote.forkAndSpecialize()` — 純 fork，child 繼承 VM | `fork()` + `exec("java -jar")` | 🔴 結構性差異，需文件說明 |
| **system_server** | `forkSystemServer()` — hardcoded 在 ZygoteInit 裡 | 同，第一個 fork | ✅ |
| **Socket** | `/dev/socket/zygote`（init 建立） | `/tmp/mini-aosp/zygote.sock` | ✅ |
| **USAP pool** | Pre-fork unspecialized processes | 無 | ✅ 優化項，不影響架構 |
| **Specialize** | `setuid` + `setgid` + `setgroups` + `seccomp` + SELinux | `setuid` + `setgid` | ✅ 安全加固非核心 |

**真正 Zygote 的 fork+specialize 流程（ZygoteInit.java）：**
```java
1. ZygoteInit.main(args)
2.   preload()                    // 載入共用資源
3.   ZygoteServer server = new ZygoteServer()
4.   forkSystemServer(...)        // fork 第一個 child = system_server
5.   server.runSelectLoop()       // 等待 fork 請求

// forkSystemServer 內部：
Zygote.forkSystemServer(uid=1000, gid=1000, gids, flags, rlimits, ...)
  → native nativeForkSystemServer()
    → fork()
    → child: specialize (setuid, setgid, mount namespace, seccomp, selinux)
    → child: return to Java → handleSystemServerProcess()
      → 關閉 zygote socket（child 不需要）
      → RuntimeInit.applicationInit() → SystemServer.main()
```

**我們的 Stage 5 應該 follow 的結構：**
```
1. app_process main()
2.   preload()              ← 目前幾乎空的
3.   forkSystemServer()     ← fork + exec java -jar system_server.jar
4.   runSelectLoop()        ← accept fork 請求
     - 收到 FORK 請求
     - fork()
     - child: exec java -jar app.jar（我們用 exec，真 AOSP 不用）
     - parent: 回報 PID
```

**AOSP 參考檔案：**
```
frameworks/base/cmds/app_process/app_main.cpp
  → main(), 判斷是 zygote 還是 application

frameworks/base/core/java/com/android/internal/os/ZygoteInit.java
  → main(), preload(), forkSystemServer()

frameworks/base/core/java/com/android/internal/os/Zygote.java
  → forkAndSpecialize(), nativeForkAndSpecialize()

frameworks/base/core/jni/com_android_internal_os_Zygote.cpp
  → ForkCommon(), SpecializeCommon()
```

---

### Stage 6：App Process + Lifecycle

| 元件 | 真正 AOSP | mini-AOSP | 差異可接受？ |
|------|----------|-----------|------------|
| **App 入口** | `ActivityThread.main()` | 同名 `ActivityThread.main()` | ✅ |
| **Looper setup** | `Looper.prepareMainLooper()` | 同 | ✅ |
| **AMS attach** | `AMS.attachApplication(appThread)` — 傳 IApplicationThread binder | 同概念，傳 PID | ✅ |
| **Launch Activity** | `TransactionExecutor` + `LaunchActivityItem` | 直接 `scheduleLaunchActivity()` | 🟡 功能等價，pattern 不同 |
| **7 callbacks** | 全部有，用 `mCalled` flag 檢查 `super` | 全部有，不檢查 super | ✅ |
| **Save/Restore** | `onSaveInstanceState` → AMS 保管 Bundle | 同 | ✅ |
| **Service lifecycle** | Started + Bound + Hybrid | 同 | ✅ |
| **Restart policy** | `START_STICKY` / `START_NOT_STICKY` / `START_REDELIVER_INTENT` | 同 | ✅ |

**真正 AOSP ActivityThread.main() 流程：**
```java
public static void main(String[] args) {
    Looper.prepareMainLooper();
    ActivityThread thread = new ActivityThread();
    thread.attach(false, startSeq);  // false = not system
    Looper.loop();  // 永遠不回傳
}

void attach(boolean system, long startSeq) {
    IActivityManager mgr = ActivityManager.getService();
    mgr.attachApplication(mAppThread, startSeq);
    // mAppThread 是 ApplicationThread（IApplicationThread 的實作）
    // AMS 透過這個 binder 回呼 app 的 lifecycle
}
```

**重點：`mAppThread` 是一個 Binder 物件**，AMS 持有它的 handle 來推送 lifecycle 事件。
我們也應該這樣做——app attach 時把自己的 binder 傳給 AMS。

**AOSP 參考檔案：**
```
frameworks/base/core/java/android/app/ActivityThread.java
  → main(), handleLaunchActivity(), performLaunchActivity()

frameworks/base/core/java/android/app/Activity.java
  → performCreate(), performStart(), performResume()

frameworks/base/core/java/android/app/servertransaction/TransactionExecutor.java
  → execute(), executeCallbacks(), executeLifecycleState()

frameworks/base/core/java/android/app/servertransaction/LaunchActivityItem.java
```

---

### Stage 7：PMS + Intent System

| 元件 | 真正 AOSP | mini-AOSP | 差異可接受？ |
|------|----------|-----------|------------|
| **Manifest 格式** | Binary XML in APK | JSON | ✅ |
| **掃描路徑** | `/data/app/`, `/system/app/`, `/system/priv-app/` | `packages/apps/` | ✅ |
| **UID 分配** | `Settings.java` — 持久化到 `/data/system/packages.xml` | In-memory map | 🟡 重啟後 UID 會變，但 Phase 1 可接受 |
| **FIRST_APPLICATION_UID** | 10000 | 10000 | ✅ |
| **Intent resolve** | Action + Category + MIME type + URI | Action only | ✅ 簡化 |
| **Permission check** | `checkPermission()` at Binder call time | 暫無 | 🟡 Stage 8 可加 |

**真正 AOSP PMS 的 intent resolution 流程：**
```java
// PackageManagerService.java
resolveIntent(Intent intent, String resolvedType, int flags, int userId)
  → queryIntentActivities(intent, resolvedType, flags, userId)
    → 遍歷所有 Activity 的 IntentFilter
    → IntentFilter.match(action, type, scheme, data, categories)
    → 按 priority 排序
    → 回傳 ResolveInfo list
```

**我們的簡化版只 match action string**——足夠展示 intent resolution 的概念。

**AOSP 參考檔案：**
```
frameworks/base/services/core/java/com/android/server/pm/PackageManagerService.java
  → resolveIntentInternal(), queryIntentActivitiesInternal()

frameworks/base/core/java/android/content/IntentFilter.java
  → match(), matchAction(), matchData(), matchCategories()

frameworks/base/services/core/java/com/android/server/pm/Settings.java
  → mPackages, newUserIdLPw()
```

---

### Stage 8：AMS + lmkd + BroadcastReceiver

| 元件 | 真正 AOSP | mini-AOSP | 差異可接受？ |
|------|----------|-----------|------------|
| **OomAdjuster** | `OomAdjuster.java` — 4,000+ 行 | 簡化版 ~100 行 | ✅ 核心邏輯相同 |
| **Process priority** | `ProcessList.java` — FOREGROUND(0) → CACHED(999) | 5 級 + inheritance | ✅ |
| **lmkd 位置** | 獨立 C daemon `system/core/lmkd/` | 🔴 需確保是獨立 process | 🔴 見 Issue 2 |
| **lmkd 偵測** | PSI (`/proc/pressure/memory`) + vmpressure | `/proc/meminfo` polling | ✅ |
| **BroadcastQueue** | `BroadcastQueue.java` — ordered + parallel queues | 簡化版 unordered | ✅ |
| **Broadcast timeout** | `BROADCAST_FG_TIMEOUT=10s`, `BROADCAST_BG_TIMEOUT=60s` | 10s 統一 | ✅ |
| **Force-stop** | `forceStopPackageLocked()` — kill + cleanup + clear data option | kill + cleanup | ✅ |

**真正 lmkd 的架構：**
```
init
 ├─ servicemanager
 ├─ lmkd              ← 獨立 daemon (C)
 │   ├─ 監聽 /proc/pressure/memory (PSI)
 │   ├─ 收到 AMS 推送的 oom_adj 更新
 │   └─ 壓力事件 → kill by oom_adj
 ├─ zygote
 └─ system_server
     └─ AMS
         ├─ 計算 oom_adj
         ├─ 推送 oom_adj 到 lmkd（透過 lmkd socket）
         └─ 收到 lmkd 的 kill 通知 → cleanup ProcessRecord
```

**我們必須保持 lmkd 為獨立 process 的原因：**
1. 如果 system_server 記憶體爆了，lmkd 還能 kill 其他 process
2. lmkd 需要比 system_server 更高的 oom_adj priority（不被 kill）
3. 跟真正 AOSP 架構一致

**AOSP 參考檔案：**
```
system/core/lmkd/lmkd.cpp               → main loop, mp_event_common()
frameworks/base/services/core/java/com/android/server/am/OomAdjuster.java
  → updateOomAdjLSP(), computeOomAdjLSP()
frameworks/base/services/core/java/com/android/server/am/ProcessList.java
  → FOREGROUND_APP_ADJ, VISIBLE_APP_ADJ, ...
frameworks/base/services/core/java/com/android/server/am/BroadcastQueue.java
  → processNextBroadcast(), scheduleBroadcastsLocked()
```

---

## 修正行動

### 必須修正（影響架構正確性）

| Issue | 修正 | 影響的 Stage |
|-------|------|-------------|
| lmkd 必須是獨立 daemon | 保持 `system/core/lmkd/main.cpp`，init.rc 加 `service lmkd` | Stage 8 |
| servicemanager.ready 通知 | SM 啟動後通知 init（property 或 socket signal） | Stage 1, 3 |

### 建議改善（更貼近 AOSP）

| 改善 | 詳細 | 影響的 Stage |
|------|------|-------------|
| Proxy/Stub 命名 | `XxxProxy` → `BpXxx`, `XxxStub` → `BnXxx` | Stage 2 |
| app attach 傳 binder | App attach AMS 時傳自己的 binder handle（不只是 PID） | Stage 6 |
| Boot 順序：AMS before PMS | `startBootstrapServices()` 裡 AMS 先啟動 | Stage 4 |

### 已知差異（Phase 1 可接受，Phase 2 處理）

| 差異 | 原因 | Phase 2 方案 |
|------|------|-------------|
| Zygote fork+exec 而非純 fork | OpenJDK 無法嵌入式 fork | 用 JNI_CreateJavaVM 嵌入 |
| Parcel 用 UTF-8 而非 UTF-16 | 更簡單 | 改 UTF-16 |
| 無 SELinux permission check | 不影響核心架構 | 加簡化版 MAC |
| PMS UID 不持久化 | In-memory map | 寫到 file |

---

## 參考資料

- [AOSP Binder IPC](https://source.android.com/docs/core/architecture/hidl/binder-ipc)
- [AOSP Binder Overview](https://source.android.com/docs/core/architecture/ipc/binder-overview)
- [Binder Deep Dive Part 1](https://medium.com/@ganeshdagadi3/android-deep-dive-binder-ipc-from-the-application-to-the-kernel-and-back-part-1-1ef620f71818)
- [Binder Deep Dive Part 2](https://medium.com/@ganeshdagadi3/android-deep-dive-binder-ipc-from-the-application-to-the-kernel-and-back-part-2-c1b2d9c70532)
- [AOSP Zygote Docs](https://source.android.com/docs/core/runtime/zygote)
- [Activity Lifecycle Blueprint (Android 15)](https://8ksec.io/a-blueprint-of-android-activity-lifecycle/)
- [Activity Lifecycle Official](https://developer.android.com/guide/components/activities/activity-lifecycle)
- [RPC Binder (POSIX socket transport)](https://2net.co.uk/slides/aosp-aaos-meetup/2023-july-dbrazdil-rpc-binder.pdf)
- [Binder Tracing — Wire Format](https://foundryzero.co.uk/2022/08/30/binder-tracing-part-1.html)
