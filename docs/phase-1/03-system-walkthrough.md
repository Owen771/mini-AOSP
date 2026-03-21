# mini-AOSP 系統流程完整解析

這份文件把 `./scripts/start.sh` 從執行到結束的每一步都拆開來講，
對應到 source code 的每一行，同時對照真正 Android 是怎麼做的。

Code 已從 C++ 重構為 C，共用常數在 `system/core/libcommon/constants.h`，
共用工具函數在 `system/core/libcommon/common.h` + `common.c`。

---

## 關鍵資料結構與變數

在看流程之前，先認識 code 裡的核心結構和命名慣例。

### C 命名慣例

| 前綴 | 意思 | 例子 |
|------|------|------|
| `g_` | **global** — 檔案層級的全域變數 | `g_shutdown`, `g_services`, `g_n_services` |
| `svc` | **service** 的縮寫 | `struct service *svc` |
| `n_` | **number of** — 計數器 | `g_n_services`（目前有幾個 service） |
| `_fd` | **file descriptor** — kernel 給的 handle | `server_fd`, `client_fd` |
| `_path` | 檔案路徑字串 | `pid_path`, `rc_path` |

`g_` 前綴是 C 慣例，表示這個變數是 global scope（不是 local）。
在多個函數之間共享狀態時使用。`static` 加上 `g_` 表示「這個 global 只在這個 .c 檔案內可見」。

### `struct service`（init/main.c）

init 把每個 service 的資訊存在這個 struct 裡：

```c
struct service {
    char name[MAX_NAME];          // "servicemanager", "system_server", "hello_app"
    char *args[MAX_ARGS + 1];     // execvp 的參數 array，NULL-terminated
                                  // 例如 ["/path/to/servicemanager", NULL]
                                  // 或 ["java", "-jar", "/path/to/system_server.jar", NULL]
    int  n_args;                  // args 裡有幾個元素（不含 NULL）
    pid_t pid;                    // fork 後 child 的 PID，未啟動時 = -1
    char wait_for[MAX_PATH_LEN];  // 選填：init 要等這個檔案出現才啟動下一個 service
                                  // 例如 "/tmp/mini-aosp/servicemanager.ready"
                                  // 空字串 = 不等，用 200ms grace period
};
```

全域 array 和計數器：
```c
static struct service g_services[MAX_SERVICES];  // 最多 64 個 service 的 array
static int g_n_services = 0;                      // 目前 parse 到幾個 service
static volatile sig_atomic_t g_shutdown = 0;      // 收到 SIGTERM/SIGINT 時設為 1
```

- `volatile` — 告訴 compiler「這個變數可能在 signal handler 裡被改，不要 optimize 掉讀取」
- `sig_atomic_t` — 保證讀寫是 atomic 的型別，在 signal handler 裡安全使用

### `struct service_entry`（servicemanager/main.c）

servicemanager 的 service registry：

```c
struct service_entry {
    char name[MAX_NAME];            // service 名稱，例如 "ping"
    char socket_path[MAX_PATH_LEN]; // service 的 Unix socket 路徑
                                    // 例如 "/tmp/mini-aosp/ping.sock"
};
```

### 常數一覽（constants.h）

```c
#define MINIAOSP_RUNTIME_DIR  "/tmp/mini-aosp"           // 所有 runtime 檔案放這裡
#define MINIAOSP_SM_SOCKET    "/tmp/mini-aosp/servicemanager.sock"
#define MINIAOSP_SM_READY     "/tmp/mini-aosp/servicemanager.ready"
#define MINIAOSP_INIT_PID     "/tmp/mini-aosp/init.pid"

#define POLL_INTERVAL_US     50000   // 50ms  — wait_for_file poll 間隔
#define GRACE_PERIOD_US     200000   // 200ms — 沒有 wait_for 時的預設等待
#define SHUTDOWN_GRACE_US  2000000   // 2s    — SIGTERM 到 SIGKILL 的寬限期
#define WAIT_FOR_TIMEOUT_MS  10000   // 10s   — wait_for 最長等多久
```

---

## 總覽：誰啟動了誰

```
你打指令
  │
  ▼
start.sh  (bash script)
  │  產生 init.rc，然後 exec init
  ▼
init  (C binary, PID 1 的角色)
  │  讀 init.rc → g_services[] 填入 3 個 service
  │  launch_all_services() 依序 fork()+exec()
  │
  ├──▶ servicemanager  (C binary, 長駐 daemon)
  │      create_listening_socket() → 建立 Unix socket
  │      signal_readiness() → 寫 .ready 檔案
  │      select() accept loop 等別人來註冊/查詢 service
  │
  ├──▶ system_server  (Kotlin JVM process, 長駐 daemon)
  │      連 servicemanager → ADD_SERVICE ping
  │      建立 ping.sock → accept loop 等 PING 請求
  │
  └──▶ hello_app  (Kotlin JVM process, 跑完就退出)
         連 servicemanager → GET_SERVICE ping → 拿到 ping.sock
         連 ping.sock → 送 PING → 收 PONG
         印出結果，退出
```

---

## 第一段：`start.sh` → 啟動 init

**Source:** `scripts/start.sh`

```bash
# 用 sed 把 init.rc 裡的 ${MINI_AOSP_ROOT} 替換成實際路徑
sed "s|\${MINI_AOSP_ROOT}|$ROOT_DIR|g" \
    "$ROOT_DIR/system/core/rootdir/init.rc" > "$GENERATED_RC"
```

原始 `init.rc`（`system/core/rootdir/init.rc`）：
```
service servicemanager ${MINI_AOSP_ROOT}/out/bin/servicemanager
    wait_for /tmp/mini-aosp/servicemanager.ready

service system_server java -jar ${MINI_AOSP_ROOT}/out/jar/system_server.jar
service hello_app java -jar ${MINI_AOSP_ROOT}/out/jar/HelloApp.jar
```

```bash
# exec 取代當前 shell process，變成 init
exec "$ROOT_DIR/out/bin/init" "$GENERATED_RC"
```

`exec` 的意思是：不是「啟動一個新 process 來跑 init」，而是「把自己變成 init」。
所以 `start.sh` 的 bash process 直接變成 init process，PID 不變。

> **對照真正 Android：** Linux kernel 開機後，直接執行 `/init` 作為 PID 1。
> 我們用 shell script 模擬這個過程。

---

## 第二段：init 解析 init.rc

**Source:** `system/core/init/main.c`

```c
// main()
miniaosp_setup_signals(&g_shutdown);  // SIGTERM+SIGINT → g_shutdown=1
init_write_pid();                      // mkdir + 寫 /tmp/mini-aosp/init.pid

const char *rc_path = (argc > 1) ? argv[1] : "system/core/rootdir/init.rc";
miniaosp_log_fmt(TAG, "Parsing %s...", rc_path);

if (parse_init_rc(rc_path) == 0) { ... return 1; }
```

`init_write_pid()` 做兩件事：

```c
static void init_write_pid(void) {
    mkdir(MINIAOSP_RUNTIME_DIR, 0755);         // 建立 /tmp/mini-aosp/
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", getpid()); // 例如 "40094"
    miniaosp_write_file(MINIAOSP_INIT_PID, buf); // 寫到 /tmp/mini-aosp/init.pid
}
```

`parse_init_rc()` 逐行讀檔案，用兩個 helper：
- `parse_service_line()` — 拆 `"service <name> <cmd> [args...]"`，用 `strtok_r` tokenize 成 `struct service`
- `parse_option_line()` — 處理縮排行如 `"    wait_for /tmp/mini-aosp/servicemanager.ready"`

結果存在 `g_services[]`，`g_n_services = 3`。

> **對照真正 Android：** 真正的 init 也是解析 `/system/etc/init/` 下的 `.rc` 檔。
> 格式更複雜（有 `on boot`、`on property` 等 trigger），但核心概念一樣。

**你看到的 log：**
```
[init            ] Parsing /tmp/mini-aosp/init.rc...
```

---

## 第三段：init 用 fork+exec 啟動 servicemanager（深入解析）

**Source:** `system/core/init/main.c` — `launch_all_services()` + `launch_service()`

```c
// launch_all_services() 走 for loop，i=0 是 servicemanager
for (int i = 0; i < g_n_services; i++) {
    struct service *svc = &g_services[i];
    pid_t pid = launch_service(svc);  // fork + exec
    ...
}
```

### 3a. fork() — 複製 process

```c
// launch_service()
pid_t pid = fork();
```

`fork()` 把目前的 process **複製一份**。呼叫一次，回傳兩次：
- parent (init) 拿到 child 的 PID（例如 40094）
- child 拿到 0

```
            fork()
              │
     ┌────────┴────────┐
     │                 │
  parent              child
  pid = 40094         pid = 0
  (init 繼續跑)       (也是 init 的 copy)
```

此時 child 是 init 的**完整 clone** — 一樣的 code、一樣的 memory。
但我們不要第二個 init，我們要 servicemanager。所以：

### 3b. execvp() — 替換 process 內容

```c
if (pid == 0) {
    // child process — 用 servicemanager 的 binary 取代自己
    execvp(svc->args[0], svc->args);
    // ^^^ 如果成功，這行之後的 code 永遠不會跑到
    //     因為整個 process 已經變成 servicemanager 了

    // 只有 exec 失敗才會到這裡
    fprintf(stderr, "[init] exec failed for %s: %s\n",
            svc->name, strerror(errno));
    _exit(127);
}
```

**`execvp` 名字拆解：**
- `exec` = **replace** 目前 process 的 code + data + stack，換成另一個 binary
- `v` = 參數用 **vector**（`char *args[]` array），不是一個一個傳
- `p` = 在 `$PATH` 裡找 binary（不過我們給的是絕對路徑所以沒差）

```
execvp 之前:              execvp 之後:
┌─────────────────┐      ┌─────────────────┐
│ init 的 code     │      │ servicemanager   │
│ init 的 data     │  →   │ 的 code + data   │
│ init 的 stack    │      │ 全新的 stack      │
│ PID = 40094     │      │ PID = 40094      │  ← PID 不變！
└─────────────────┘      └─────────────────┘
```

關鍵：PID 不變，file descriptor 不變，parent-child 關係不變。只是裡面跑的程式換了。

**為什麼不直接 "spawn" servicemanager？** 因為 Unix 沒有 "spawn" 的概念。
建立新 process 永遠是兩步：fork（複製）+ exec（替換）。
這也是為什麼 Zygote 可以 fork-without-exec — 省掉 exec 那步，直接用 fork 出來的 JVM copy。

### 3c. 回到 parent — 寫 PID file

```c
// launch_all_services() 繼續
pid_t pid = launch_service(svc);   // 拿到 40094
if (pid <= 0) continue;

svc->pid = pid;                     // 記住：servicemanager 的 PID 是 40094
miniaosp_log_fmt(TAG, "Starting %s (PID %d)...", svc->name, pid);

// 寫 PID file — 讓外部 script（stop.sh）知道要 kill 誰
char pid_path[MAX_PATH_LEN];
snprintf(pid_path, sizeof(pid_path),
         MINIAOSP_RUNTIME_DIR "/%s.pid", svc->name);
// → "/tmp/mini-aosp/servicemanager.pid"

char pid_str[32];
snprintf(pid_str, sizeof(pid_str), "%d", pid);
// → "40094"

miniaosp_write_file(pid_path, pid_str);
```

### 3d. wait_for — 等 child 準備好

```c
if (svc->wait_for[0] != '\0') {
    // wait_for 欄位有值 → 這個 service 有 readiness dependency
    // servicemanager 的 wait_for = "/tmp/mini-aosp/servicemanager.ready"
    miniaosp_log_fmt(TAG, "Waiting for %s ready...", svc->name);

    if (miniaosp_wait_for_file(svc->wait_for, WAIT_FOR_TIMEOUT_MS))
        miniaosp_log_fmt(TAG, "%s is ready.", svc->name);
    else
        miniaosp_log_fmt(TAG, "WARNING: timeout waiting for %s", svc->wait_for);
} else {
    usleep(GRACE_PERIOD_US);  // 200ms 預設等待
}
```

**為什麼需要等？** 因為 **fork+exec 回傳不代表 service 準備好了**。
servicemanager 要先 `socket()` → `bind()` → `listen()` 才能接受連線。
如果 init 馬上 launch system_server，system_server 試著連 `servicemanager.sock` 會失敗
（socket 還不存在）。

`miniaosp_wait_for_file()` 就是個 poll loop（`system/core/libcommon/common.c`）：

```c
int miniaosp_wait_for_file(const char *path, int timeout_ms) {
    int elapsed = 0;
    while (elapsed < timeout_ms) {          // 最多等 10 秒 (WAIT_FOR_TIMEOUT_MS)
        if (miniaosp_file_exists(path))     // stat() 檢查檔案存不存在
            return 1;                        // 找到了！
        usleep(POLL_INTERVAL_US);           // 睡 50ms
        elapsed += POLL_INTERVAL_US / 1000; // elapsed += 50
    }
    return 0;  // timeout
}
```

同一時間，servicemanager child process 跑完 `bind()` + `listen()` 後：

```c
// servicemanager/main.c
static void signal_readiness(void) {
    char pid_str[32];
    snprintf(pid_str, sizeof(pid_str), "%d", getpid());
    miniaosp_write_file(MINIAOSP_SM_READY, pid_str);
    // → /tmp/mini-aosp/servicemanager.ready 出現了
}
```

init 的 poll loop 下一輪 `stat()` 發現 `.ready` 存在 → return 1 → init 繼續 for loop。

### 3e. 時間線

```
init (parent)                    servicemanager (child)
─────────────                    ──────────────────────
fork() ──────────────────────▶  execvp() → 變成 servicemanager
寫 servicemanager.pid            socket()
wait_for_file() polling...       bind()
  stat() → 不存在                listen()
  sleep 50ms                     signal_readiness()
  stat() → 不存在                  → 寫 .ready 檔案
  sleep 50ms                     進入 select() accept loop...
  stat() → 存在！ ◀───────────
log "servicemanager is ready"
繼續 launch system_server...
```

> **對照真正 Android：** 完全一樣。Android 的 init 用同樣的 fork+exec 啟動所有 native daemon。
> 真正 Android 用 property system（`setprop init.svc.servicemanager running`）來通知 readiness，
> 我們用檔案存在性來近似。

**你看到的 log：**
```
[init            ] Starting servicemanager (PID 40094)...
[init            ] Waiting for servicemanager ready...
[servicemanager  ] Listening on /tmp/mini-aosp/servicemanager.sock
[init            ] servicemanager is ready.
```

---

## 第四段：servicemanager 建立 socket 開始監聽

**Source:** `frameworks/native/cmds/servicemanager/main.c`

servicemanager 被 exec 起來後，main() 做這些事：

```c
miniaosp_setup_signals(&g_shutdown);
mkdir(MINIAOSP_RUNTIME_DIR, 0755);

int server_fd = create_listening_socket(MINIAOSP_SM_SOCKET);
```

`create_listening_socket()` 封裝了三個 syscall：

```c
static int create_listening_socket(const char *path) {
    unlink(path);  // 清掉舊的 socket 檔案

    int fd = socket(AF_UNIX, SOCK_STREAM, 0);  // kernel syscall: 建立 socket

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);

    bind(fd, (struct sockaddr *)&addr, sizeof(addr));  // kernel syscall: 綁定到檔案路徑
    listen(fd, 10);                                     // kernel syscall: 開始監聽
    return fd;
}
```

### 什麼是 Unix domain socket

Unix domain socket 是 Linux kernel 提供的**同一台機器上 process 之間通訊 (IPC)** 的機制。
它跟網路 socket (TCP/UDP) 的 API 一模一樣（socket, bind, listen, accept, read, write），
但不走網路——直接在 kernel memory 裡傳資料，速度極快。

它綁定的不是 IP:port，而是**檔案路徑**（如 `/tmp/mini-aosp/servicemanager.sock`）。
任何 process 都能用 `connect()` 連到這個路徑來通訊。

### servicemanager 的 protocol

client 指令由 `handle_client()` dispatch 到三個 handler：

| 指令 | Handler | 回覆 | 用途 |
|------|---------|------|------|
| `ADD_SERVICE ping /tmp/.../ping.sock` | `handle_add_service()` | `OK` | 註冊 service |
| `GET_SERVICE ping` | `handle_get_service()` | `/tmp/.../ping.sock` | 查詢 service 位置 |
| `LIST_SERVICES` | `handle_list_services()` | `ping ` | 列出所有已註冊 service |

內部是 `struct service_entry g_services[MAX_SERVICES]` array。

> **對照真正 Android：** 真正的 servicemanager 用 binder（kernel driver）而非 Unix socket。
> 但概念完全相同：一個集中的 service registry，讓所有 process 能互相找到。

---

## 第五段：init 啟動 system_server

跟第三段一模一樣的 fork+exec，但這次 `svc->args` 是：
```
["java", "-jar", "/path/to/system_server.jar", NULL]
```

`java` 啟動 JVM，載入 JAR 裡的 `services.SystemServer.main()`。

init.rc 裡 system_server 沒有 `wait_for`，所以 init 只 `usleep(GRACE_PERIOD_US)`（200ms）就繼續。

---

## 第六段：system_server 註冊 PingService

**Source:** `frameworks/base/services/core/kotlin/SystemServer.kt`

### 6a. 連線到 servicemanager + 送 ADD_SERVICE

```kotlin
val smAddr = UnixDomainSocketAddress.of("/tmp/mini-aosp/servicemanager.sock")
val channel = SocketChannel.open(StandardProtocolFamily.UNIX)
channel.connect(smAddr)  // 底層是 kernel syscall: connect()

val request = "ADD_SERVICE ping /tmp/mini-aosp/ping.sock\n"
channel.write(ByteBuffer.wrap(request.toByteArray()))

channel.read(buf)  // response == "OK"
```

此時 servicemanager 那邊的 `handle_add_service()` 收到這個指令，
把 `{name:"ping", socket_path:"/tmp/mini-aosp/ping.sock"}` 存進 array，回 `OK`。

### 6b. 建立 PingService socket

```kotlin
val server = ServerSocketChannel.open(StandardProtocolFamily.UNIX)
server.bind(UnixDomainSocketAddress.of("/tmp/mini-aosp/ping.sock"))
// 進入 accept loop 等人來 PING
```

> **對照真正 Android：** system_server 啟動時會註冊上百個 service
> （ActivityManagerService、PackageManagerService、WindowManagerService…）。
> 我們只註冊一個 PingService 作為 demo。

**你看到的 log：**
```
[system_server   ] Connecting to servicemanager...
[servicemanager  ] Registered service: ping -> /tmp/mini-aosp/ping.sock
[system_server   ] Registered service: ping → /tmp/mini-aosp/ping.sock
[system_server   ] PingService listening...
```

注意同一件事出現了兩次——一次是 servicemanager 印的，一次是 system_server 印的。
它們是**兩個不同的 process**，各自印自己的 log。

---

## 第七段：init 啟動 HelloApp → PING/PONG

**Source:** `packages/apps/HelloApp/HelloApp.kt`

### 7a. 向 servicemanager 查詢 PingService

```kotlin
channel.connect(UnixDomainSocketAddress.of("/tmp/mini-aosp/servicemanager.sock"))
channel.write("GET_SERVICE ping\n")
val response = channel.read(buf)  // → "/tmp/mini-aosp/ping.sock"
```

### 7b. 連到 PingService，送 PING

```kotlin
val startTime = System.nanoTime()
channel.connect(UnixDomainSocketAddress.of("/tmp/mini-aosp/ping.sock"))
channel.write("PING\n")
val response = channel.read(buf)  // → "PONG uid=40111 pid=40111 time=..."
val elapsed = (System.nanoTime() - startTime) / 1_000_000.0
```

### 7c. 印出成功訊息，退出

```kotlin
Log.i(TAG, "Received: PONG — round-trip 53.6ms")
Log.i(TAG, "✓ Full stack verified: App → SystemServer → ServiceManager → init → kernel")
// main() 結束，JVM 退出
```

**重點：servicemanager 只是電話簿，不 relay 資料。**

```
HelloApp                    servicemanager              system_server
   │                              │                          │
   │──GET_SERVICE ping──────▶│                          │
   │◀─/tmp/.../ping.sock────│                          │
   │                              │                          │
   │──────────────PING──────────────────────────────▶│
   │◀─────────────PONG──────────────────────────────│
```

servicemanager 告訴 HelloApp「ping 在哪個 socket」。
之後的 PING/PONG 是 HelloApp **直接連到** system_server 的 `ping.sock`，不經過 servicemanager。
真正的 Android Binder 也一樣 — servicemanager 只做 service discovery，不做 data relay。

---

## 第八段：init 監控 children + shutdown

**Source:** `system/core/init/main.c` — `monitor_children()` + `shutdown_services()`

```c
// main() 繼續
launch_all_services();
miniaosp_log(TAG, "All services started. Waiting...");
monitor_children();  // ← 阻塞在這裡
```

`monitor_children()` 用 `waitpid()` poll loop：

```c
static void monitor_children(void) {
    while (!g_shutdown) {
        int status;
        pid_t exited = waitpid(-1, &status, WNOHANG);
        // -1 = 監控所有 child process
        // WNOHANG = 不等待，有退出就回報，沒有就立刻返回

        if (exited > 0) {
            // 找到退出的 child，更新 svc->pid = -1
            if (WIFEXITED(status))      // 正常退出
                miniaosp_log_fmt(TAG, "%s exited with code %d", ...);
            else if (WIFSIGNALED(status))  // 被 signal 殺掉
                miniaosp_log_fmt(TAG, "%s killed by signal %d", ...);

            // 檢查是不是所有 child 都退出了
            int any_alive = 0;
            for (...) if (g_services[i].pid > 0) { any_alive = 1; break; }
            if (!any_alive) return;
        }
        usleep(POLL_INTERVAL_US * 2);  // 100ms 後再檢查
    }
}
```

HelloApp 退出 → init log "hello_app exited with code 0"。
servicemanager 和 system_server 還活著 → init 繼續 loop。
**這就是「卡住」的原因——init 在等剩下的 daemon。這是正確行為。**

### 怎麼結束

按 `Ctrl+C` 或 `./scripts/stop.sh` → init 收到 SIGTERM/SIGINT：

```c
// common.c 裡的 signal handler
static void signal_handler(int sig) { (void)sig; *g_flag_ptr = 1; }
// → g_shutdown = 1 → monitor_children() 跳出 → 回到 main()
```

`shutdown_services()` 做三件事：
1. 對每個還活著的 child 送 `SIGTERM`（禮貌請求退出）
2. `usleep(SHUTDOWN_GRACE_US)` — 等 2 秒
3. 對還沒死的送 `SIGKILL`（強制終止，無法攔截）

最後 `cleanup_pid_files()` 清理所有 `.pid` 檔案 + `free()` strdup'd args。

---

## 完整 IPC 路徑圖

```
HelloApp ──connect()──▶ servicemanager.sock  ──read()/write()──  servicemanager
         ◀─response────                                          (查 array，回覆路徑)

HelloApp ──connect()──▶ ping.sock  ──read()/write()──  system_server
         ◀─"PONG"─────                                 (收 PING，回 PONG)

init 用 fork()+exec() 啟動以上所有 process
init 用 waitpid() 監控它們的生死
init 用 kill()+signal() 控制 shutdown
```

每一條線都是**真正的 kernel syscall**——不是模擬，不是 mock。

---

## Q&A：常見問題

### Q: Bp / Bn 是什麼？

**Bp** = Binder Proxy (client 側), **Bn** = Binder Native (server 側)。

- **BpMusicPlayer** — 在呼叫方 process 裡。看起來像本地物件，但內部把 method call 序列化成 Parcel，透過 Binder IPC 送出去。是個「假的」MusicPlayer。
- **BnMusicPlayer** — 在實作方 process 裡。從 Parcel 反序列化參數，呼叫真正的 `play()` 實作。

```
App A (client)                      App B (server)
┌─────────────────────┐            ┌──────────────────────┐
│ BpMusicPlayer       │            │ BnMusicPlayer        │
│  .play("song.mp3")  │            │  收到 Parcel         │
│   → writeInt(1)     │──Binder──▶│   → readInt() = 1    │
│   → writeString()   │   IPC     │   → readString()     │
│   → transact()      │           │   → 呼叫真正的 play()│
└─────────────────────┘            └──────────────────────┘
```

為什麼叫 "Native"？server 端持有真正的 Binder node（原生物件），client 端只有 proxy（代理引用）。

### Q: 為什麼 fork+exec 是兩步，不能直接 spawn？

Unix 設計哲學：fork 和 exec 是**正交的**操作。

- `fork()` 只負責複製 process（包括所有 file descriptor、signal handler、environment）
- `exec()` 只負責替換程式碼

分開的好處：在 fork 之後、exec 之前，child 可以做任何準備工作
（redirect stdout、close fd、改 env），然後再 exec。

這也是 Zygote 的關鍵：它 fork **不 exec**，直接用 fork 出來的 JVM copy，
省掉重新載入 framework class 的時間。

### Q: servicemanager 只是電話簿，不 relay 資料？

對。流程是：
1. system_server 告訴 servicemanager：「我叫 ping，地址是 ping.sock」
2. HelloApp 問 servicemanager：「ping 在哪？」→ 拿到 `ping.sock`
3. HelloApp **直接連** `ping.sock` 跟 system_server 對話

第 3 步不經過 servicemanager。真正 Android 也一樣。

### Q: g_ 前綴是什麼意思？

`g_` = **global**。C 慣例，表示變數是 file-scope global（不是函數內的 local）。
加上 `static` 表示「只在這個 .c 檔案內可見」。例如：

- `g_shutdown` — 全域 flag，signal handler 設為 1，main loop 檢查它
- `g_services[]` — 全域 service array，多個函數共用
- `g_n_services` — array 裡目前有幾個元素

---

## Kernel Syscall 清單

| Syscall | 誰用的 | 做什麼 |
|---------|--------|--------|
| `fork()` | init | 複製 process，產生 child |
| `execvp()` | init | 把 child 替換成新程式 |
| `waitpid()` | init | 監控 child 的生死 |
| `kill()` | init, stop.sh | 送 signal 給 process |
| `socket()` | servicemanager, system_server | 建立 Unix domain socket |
| `bind()` | servicemanager, system_server | 綁定 socket 到檔案路徑 |
| `listen()` | servicemanager, system_server | 開始監聽連線 |
| `accept()` | servicemanager, system_server | 接受新連線 |
| `connect()` | system_server, HelloApp | 連到 server socket |
| `read()` | 全部 | 從 socket 讀資料 |
| `write()` | 全部 | 往 socket 寫資料 |
| `select()` | servicemanager | 等待 socket 可讀（帶 timeout） |
| `signal()` | init, servicemanager, lmkd | 註冊 signal handler |
| `getpid()` | init, servicemanager | 取得自己的 PID |
| `mkdir()` | init, servicemanager | 建立目錄 |
| `stat()` | init | 檢查檔案是否存在（wait_for） |
| `unlink()` | init, servicemanager | 刪除 PID/socket 檔案 |
| `usleep()` | init | 定時休眠（poll interval、grace period） |

---

## Source 檔案索引

| 檔案 | 用途 |
|------|------|
| `system/core/libcommon/constants.h` | 所有共用常數（路徑、buffer 大小、timeout） |
| `system/core/libcommon/common.h` + `common.c` | 共用工具：signal setup、file ops、log helper |
| `system/core/liblog/log.h` + `log.c` | Tagged logging `[tag] message` |
| `system/core/init/main.c` | init — parse init.rc、fork+exec、monitor、shutdown |
| `frameworks/native/cmds/servicemanager/main.c` | servicemanager — socket registry |
| `system/core/lmkd/main.c` | lmkd stub（Stage 0 no-op） |
| `frameworks/base/cmds/app_process/main.c` | app_process/Zygote stub（Stage 0 no-op） |
| `frameworks/base/services/core/kotlin/SystemServer.kt` | system_server — registers PingService |
| `packages/apps/HelloApp/HelloApp.kt` | HelloApp — PING/PONG demo |
| `system/core/rootdir/init.rc` | Service definitions (template) |
| `build/Makefile` | Build system |
| `scripts/start.sh` | Generate init.rc + exec init |
| `scripts/stop.sh` | Graceful shutdown via SIGTERM |
