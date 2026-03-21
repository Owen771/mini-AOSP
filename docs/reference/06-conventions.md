# mini-AOSP C Conventions

本專案的 C code 命名與風格慣例。

---

## 變數命名前綴

| 前綴 | 意思 | 例子 |
|------|------|------|
| `g_` | **global** — 檔案層級全域變數（跨函數共享狀態） | `g_shutdown`, `g_services`, `g_n_services` |
| `n_` | **number of** — 計數器 | `g_n_services`, `svc->n_args` |
| `_fd` | **file descriptor** — kernel 給的 handle | `server_fd`, `client_fd` |
| `_path` | 檔案路徑字串 | `pid_path`, `rc_path` |
| `_str` | 要寫入/傳送的字串 | `pid_str` |
| `_src` | source 檔案路徑（Makefile） | `INIT_SRC`, `LOG_SRC`, `COMMON_SRC` |

## 常見縮寫

| 縮寫 | 全名 | 出現位置 |
|------|------|----------|
| `svc` | service | `struct service *svc` |
| `sm` | servicemanager | `MINIAOSP_SM_SOCKET`, `SM_SRC` |
| `buf` | buffer | 暫存讀寫資料的 char array |
| `msg` | message | log 訊息字串（已移除，改用 `miniaosp_log_fmt`） |
| `tok` | token | `strtok_r` 的回傳值 |
| `rc` | run commands | `init.rc`（設定檔） |

## `static` + `g_` 的組合

```c
static struct service g_services[MAX_SERVICES];
static int g_n_services = 0;
static volatile sig_atomic_t g_shutdown = 0;
```

- `static` = 只在這個 `.c` 檔案內可見（不會 leak 到其他 .c）
- `g_` = 表明這是 global，不是某個函數的 local 變數
- 多個函數會讀寫它們，所以需要 file-scope

## `volatile sig_atomic_t`

```c
static volatile sig_atomic_t g_shutdown = 0;
```

- `volatile` — 告訴 compiler：這個變數可能被 signal handler 隨時改掉，不要 cache 或 optimize 掉讀取
- `sig_atomic_t` — 保證讀寫是 atomic 的整數型別，在 signal handler 裡安全使用
- 組合起來：signal handler 裡 `*flag = 1`，main loop 裡 `while (!g_shutdown)` 能正確看到變化

## 函數命名

| 模式 | 意思 | 例子 |
|------|------|------|
| `miniaosp_*` | 共用 library 函數（跨檔案使用） | `miniaosp_setup_signals()`, `miniaosp_log_fmt()` |
| `動詞_名詞()` | 做一件事 | `launch_service()`, `monitor_children()`, `shutdown_services()` |
| `handle_*` | 處理特定 command/event | `handle_add_service()`, `handle_client()` |
| `parse_*` | 解析輸入 | `parse_init_rc()`, `parse_service_line()` |
| `create_*` | 建立資源 | `create_listening_socket()` |

## 常數命名（constants.h）

| 模式 | 意思 | 例子 |
|------|------|------|
| `MINIAOSP_*` | 路徑常數 | `MINIAOSP_RUNTIME_DIR`, `MINIAOSP_SM_SOCKET` |
| `MAX_*` | 上限值 | `MAX_SERVICES`, `MAX_NAME`, `MAX_LINE` |
| `*_US` | 微秒 (microseconds) | `POLL_INTERVAL_US`, `GRACE_PERIOD_US` |
| `*_MS` | 毫秒 (milliseconds) | `WAIT_FOR_TIMEOUT_MS` |
| `*_SEC` | 秒 | `SELECT_TIMEOUT_SEC` |

## 檔案組織

```
system/core/libcommon/     共用 code（所有 binary 都 link）
  constants.h              magic numbers
  common.h + common.c      utility functions

system/core/liblog/        logging（所有 binary 都 link）
  log.h + log.c

system/core/init/          init daemon
  main.c                   一個檔案，函數依邏輯分區（parsing → lifecycle → main）

frameworks/.../servicemanager/
  main.c                   一個檔案，函數依邏輯分區（registry → dispatch → socket → main）
```

每個 `main.c` 內部用註解分區：
```c
/* ------------------------------------------------------------------ */
/*  Section name                                                       */
/* ------------------------------------------------------------------ */
```
