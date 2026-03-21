# init 設計文件

---

## 它是什麼

init 是 **PID 1** — Linux kernel 開機後執行的第一個 userspace process。
它是所有其他 process 的祖先，負責把整個 Android 系統拉起來。

在真正 Android 裡，init 讀取 `/system/etc/init/*.rc` 檔案，
按照 trigger（`on boot`、`on property:xxx=yyy`）的順序啟動所有 daemon。

我們的 mini-AOSP init 是簡化版：讀一個 `init.rc`，按順序 fork+exec 每個 service。

---

## 職責

1. **解析 init.rc** — 讀取 service 定義（name、command、wait_for dependency）
2. **啟動所有 service** — 依序 fork+exec，寫 PID file，等待 readiness
3. **監控 children** — waitpid loop 偵測 child 退出/crash
4. **Graceful shutdown** — 收到 SIGTERM → 通知所有 child → 等待 → 強殺

init 是唯一知道所有 service 的 process。其他 process 只知道自己和 servicemanager。

---

## 目前狀態 (Stage 0)

**Source:** `system/core/init/main.c`

| 項目 | 狀態 |
|------|------|
| Config 格式 | 簡易 `service <name> <cmd>` + `wait_for <path>` |
| 啟動順序 | 嚴格按 init.rc 出現順序 |
| Readiness check | 輪詢檔案存在（`wait_for`）或固定等 200ms |
| Crash handling | 偵測到但不做任何事（log then ignore） |
| Property store | 無 |
| Restart | 無 — crash 就永遠死了 |

### 內部結構

```c
struct service {
    char name[MAX_NAME];          // "servicemanager", "system_server", "hello_app"
    char *args[MAX_ARGS + 1];     // execvp argv, NULL-terminated
    int  n_args;                  // arg count
    pid_t pid;                    // child PID, -1 if not running
    char wait_for[MAX_PATH_LEN];  // readiness file path, empty = use grace period
};

static struct service g_services[MAX_SERVICES]; // all parsed services
static int g_n_services = 0;
static volatile sig_atomic_t g_shutdown = 0;    // set by signal handler
```

### 函數地圖

| 函數 | 職責 |
|------|------|
| `parse_init_rc()` | 讀 init.rc，逐行分派給下面兩個 parser |
| `parse_service_line()` | 拆 `service <name> <cmd> [args]` 存入 g_services[] |
| `parse_option_line()` | 處理縮排行 `wait_for <path>` |
| `init_write_pid()` | mkdir runtime dir + 寫 init.pid |
| `launch_service()` | fork() → child: execvp() / parent: return PID |
| `launch_all_services()` | for loop：launch → write PID file → wait_for or grace |
| `monitor_children()` | waitpid(WNOHANG) poll loop，偵測 child 退出 |
| `shutdown_services()` | SIGTERM → 2s grace → SIGKILL |
| `cleanup_pid_files()` | unlink PID files + free strdup'd args |

### 執行流程

```
main()
  │
  ├─ miniaosp_setup_signals()     ← SIGTERM/SIGINT → g_shutdown=1
  ├─ init_write_pid()             ← mkdir + write /tmp/mini-aosp/init.pid
  ├─ parse_init_rc()              ← fill g_services[0..2]
  │
  ├─ launch_all_services()        ← for each service:
  │    ├─ launch_service()        ←   fork+exec
  │    ├─ write PID file          ←   /tmp/mini-aosp/<name>.pid
  │    └─ wait_for or grace       ←   poll .ready file or sleep 200ms
  │
  ├─ monitor_children()           ← blocking loop until all exit or signal
  │
  ├─ shutdown_services()          ← SIGTERM → grace → SIGKILL
  └─ cleanup_pid_files()          ← unlink + free
```

---

## Stage 1 之後的變化

Stage 1 是 init 的大升級。

| 功能 | Stage 0（目前） | Stage 1（之後） |
|------|----------------|----------------|
| Crash handling | 偵測 + log，不重啟 | 自動重啟 + exponential backoff（1s→2s→4s→...60s） |
| Service 類型 | 全部一樣 | `restart always`（persistent）vs `restart oneshot` |
| Property store | 無 | system-wide key-value store（`GETPROP`/`SETPROP`） |
| `ro.*` properties | 無 | 唯讀 property，boot 後不可改 |
| Boot completion | 無 | `sys.boot_completed=1` when all services up |

Stage 1 的 init.rc：

```
service servicemanager /path/to/servicemanager
    wait_for /tmp/mini-aosp/servicemanager.ready
    restart always

service hello_app /path/to/HelloApp.jar
    restart oneshot
```

Stage 1 的 struct service 新增：

```c
int restart_count;        // crash 次數
int oneshot;              // 1 = 不重啟
time_t last_crash_time;   // 上次 crash 時間（算 backoff 用）
```

---

## 最終狀態（Phase 1 完成）

| 項目 | Stage 0 | 最終 |
|------|---------|------|
| Config | `service` + `wait_for` | + `restart`、`on property:` trigger |
| Restart | 無 | backoff restart（1s→60s cap） |
| Property | 無 | Unix socket property store（`GETPROP`/`SETPROP`/`LISTPROP`） |
| Startup order | 固定順序 | 可依 property trigger 動態決定 |

---

## 對照真正 AOSP

| | 真正 AOSP | mini-AOSP (Stage 0) | mini-AOSP (Stage 1) |
|---|---|---|---|
| Source | `system/core/init/` | 同路徑 | 同路徑 |
| Language | C++ | C | C |
| Config | Action/Service/Import 語法 | `service` + `wait_for` | + `restart` |
| Restart | `Service::Reap()` + `restart_period_` | 無 | backoff restart |
| Property | shared memory (`/dev/__properties__`) | 無 | Unix socket |
| Trigger | `on boot`、`on property:` | 固定順序 | 固定順序（Stage 1 不加 trigger） |
| Security | SELinux context per service | 無 | 無 |

**真正 AOSP source 對照：**
```
system/core/init/main.cpp                     ← entry point
system/core/init/init.cpp                     ← SecondStageMain()
system/core/init/service.cpp                  ← Service::Start(), Reap(), Restart()
system/core/init/service_parser.cpp           ← parse init.rc
system/core/init/property_service.cpp         ← property store
system/core/init/README.md                    ← 行為文件
```
