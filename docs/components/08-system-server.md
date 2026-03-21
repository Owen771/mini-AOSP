# system_server 設計文件

---

## 它是什麼

system_server 是 Android 的**大腦**。
它是一個長駐 JVM process，裡面跑著幾乎所有 framework service：
AMS、PMS、WindowManagerService、PackageManagerService……

在真正 Android 裡，system_server 由 Zygote fork 出來，
啟動時註冊上百個 service 到 servicemanager。

我們的 mini-AOSP system_server 是簡化版：一個 Kotlin JVM process，
只註冊一個 PingService 做 demo。

---

## 職責

1. **向 servicemanager 註冊 service** — 啟動後連到 servicemanager，告訴它「我提供 ping service，地址是 ping.sock」
2. **提供 service 實作** — 建立 Unix socket，accept client 連線，處理 PING → 回 PONG
3. **（未來）託管所有 framework service** — AMS、PMS、WMS 都跑在 system_server 裡

**system_server 是 servicemanager 的最大客戶。**
真正 Android 開機時，system_server 一口氣註冊 100+ service。

---

## 目前狀態 (Stage 0)

**Source:** `frameworks/base/services/core/kotlin/SystemServer.kt`

| 項目 | 狀態 |
|------|------|
| Language | Kotlin（跑在 JVM 上） |
| 啟動方式 | init fork+exec `java -jar system_server.jar` |
| 註冊的 service | 只有 PingService |
| Transport | Unix domain socket（text protocol） |
| Service discovery | 手動 connect servicemanager.sock + send "ADD_SERVICE" |

### 啟動流程

```
main()
  │
  ├─ registerService("ping", "/tmp/mini-aosp/ping.sock")
  │    ├─ connect to servicemanager.sock
  │    ├─ send "ADD_SERVICE ping /tmp/mini-aosp/ping.sock\n"
  │    └─ read response "OK\n"
  │
  └─ startPingService()
       ├─ bind to /tmp/mini-aosp/ping.sock
       └─ accept loop:
            ├─ read "PING\n"
            ├─ log "PingService: PING from uid=... pid=..."
            └─ write "PONG uid=... pid=... time=...\n"
```

### 函數地圖

| 函數 | 職責 |
|------|------|
| `main()` | entry point — register then listen |
| `registerService()` | 連 servicemanager → ADD_SERVICE → check OK |
| `startPingService()` | bind ping.sock → accept loop → PING/PONG |

### 與其他元件的關係

```
init
  │ fork+exec
  ▼
system_server ──ADD_SERVICE──▶ servicemanager
  │                                   ▲
  │ bind ping.sock                    │
  ▼                                   │
HelloApp ◀──GET_SERVICE ping──────────┘
  │
  │ connect ping.sock
  ▼
system_server (PingService)
  │
  └─ PONG
```

---

## Stage 2 之後的變化

Stage 2（Binder IPC）改變 system_server 與 servicemanager 的溝通方式。

| 項目 | Stage 0（目前） | Stage 2-3（之後） |
|------|----------------|-------------------|
| 註冊方式 | text: `"ADD_SERVICE ping /path\n"` | Binder: `ServiceManager.addService("ping", binder)` |
| Service 暴露 | 自己開 Unix socket | 透過 Binder stub 暴露（`BnPingService`） |
| Client 呼叫 | 手動 connect + write | `proxy.ping()` — Binder proxy 自動處理 |
| Serialization | text string | Parcel（binary） |

---

## Stage 6 之後的變化

Stage 6（AMS）是 system_server 的大升級 — 從「只有 PingService」變成「管理所有 app lifecycle」。

| 項目 | Stage 0 | Stage 6（之後） |
|------|---------|----------------|
| Services | PingService only | + ActivityManagerService (AMS) |
| App 管理 | 無 | 追蹤每個 app 的 lifecycle state |
| Lifecycle callbacks | 無 | `onCreate`、`onPause`、`onResume`、`onDestroy` |
| OOM priority | 無 | 計算 oom_adj score，告訴 lmkd 誰可以 kill |

### 最終狀態（Phase 1 完成）

system_server 內的 service 清單：

| Service | Stage | 職責 |
|---------|-------|------|
| PingService | 0 | Demo IPC |
| ActivityManagerService (AMS) | 6 | App lifecycle + oom_adj |
| PackageManagerService (PMS) | 4 | App 資訊 + intent 解析 |
| WindowManagerService (WMS) | — | 不在 Phase 1 scope |

---

## 對照真正 AOSP

| | 真正 AOSP | mini-AOSP (Stage 0) | mini-AOSP (Stage 6) |
|---|---|---|---|
| Source | `frameworks/base/services/` | 同路徑 | 同路徑 |
| Language | Java/Kotlin | Kotlin | Kotlin |
| 啟動方式 | Zygote fork（共享 framework class） | init fork+exec `java -jar` | Zygote fork（Stage 5） |
| Service 數量 | 100+ | 1 (PingService) | 3 (Ping + AMS + PMS) |
| 註冊方式 | `ServiceManager.addService()` via Binder | Unix socket text protocol | Binder |
| Boot time | ~10s（載入所有 service） | <1s | ~2s |

**真正 AOSP source 對照：**
```
frameworks/base/services/java/com/android/server/SystemServer.java  ← entry point
  → startBootstrapServices()    ← AMS, PMS, 必須最先啟動
  → startCoreServices()         ← BatteryService, UsageStatsService
  → startOtherServices()        ← WMS, InputManagerService, ...
frameworks/base/services/core/java/com/android/server/am/
  ActivityManagerService.java   ← AMS 實作
frameworks/base/services/core/java/com/android/server/pm/
  PackageManagerService.java    ← PMS 實作
```

真正 AOSP 的 system_server 啟動分三個階段（bootstrap → core → other），
確保有依賴關係的 service 按正確順序初始化。
我們目前只有一個 service，不需要這個分階段機制。
