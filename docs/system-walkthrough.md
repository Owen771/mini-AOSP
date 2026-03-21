# mini-AOSP 系統流程完整解析

這份文件把 `./scripts/start.sh` 從執行到結束的每一步都拆開來講，
對應到 source code 的每一行，同時對照真正 Android 是怎麼做的。

---

## 總覽：誰啟動了誰

```
你打指令
  │
  ▼
start.sh  (bash script)
  │  產生 init.rc，然後 exec init
  ▼
init  (C++ binary, PID 1 的角色)
  │  讀 init.rc
  │  fork() + exec() 三個 child process
  │
  ├──▶ servicemanager  (C++ binary, 長駐 daemon)
  │      建立 Unix socket，等別人來註冊/查詢 service
  │
  ├──▶ system_server  (Kotlin JVM process, 長駐 daemon)
  │      向 servicemanager 註冊 PingService
  │      建立自己的 Unix socket，等 PING 請求
  │
  └──▶ hello_app  (Kotlin JVM process, 跑完就退出)
         向 servicemanager 查詢 PingService 的位置
         連到 PingService，送 PING，收 PONG
         印出結果，退出
```

---

## 第一段：`start.sh` → 啟動 init

**Source:** `scripts/start.sh`

```bash
# 第 29-30 行：用 sed 把 init.rc 裡的 ${MINI_AOSP_ROOT} 替換成實際路徑
sed "s|\${MINI_AOSP_ROOT}|$ROOT_DIR|g" \
    "$ROOT_DIR/system/core/rootdir/init.rc" > "$GENERATED_RC"
```

原始 `init.rc` 長這樣：
```
service servicemanager ${MINI_AOSP_ROOT}/out/bin/servicemanager
service system_server java -jar ${MINI_AOSP_ROOT}/out/jar/system_server.jar
service hello_app java -jar ${MINI_AOSP_ROOT}/out/jar/HelloApp.jar
```

替換後變成（例如）：
```
service servicemanager /tmp/mini-AOSP/out/bin/servicemanager
service system_server java -jar /tmp/mini-AOSP/out/jar/system_server.jar
service hello_app java -jar /tmp/mini-AOSP/out/jar/HelloApp.jar
```

```bash
# 第 38 行：exec 取代當前 shell process，變成 init
exec "$ROOT_DIR/out/bin/init" "$GENERATED_RC"
```

`exec` 的意思是：不是「啟動一個新 process 來跑 init」，而是「把自己變成 init」。
所以 `start.sh` 的 bash process 直接變成 init process，PID 不變。

> **對照真正 Android：** Linux kernel 開機後，直接執行 `/init` 作為 PID 1。
> 我們用 shell script 模擬這個過程。

**你看到的 log：**
```
=== mini-AOSP Starting ===
Root: /tmp/mini-AOSP
Runtime: /tmp/mini-aosp
```

---

## 第二段：init 解析 init.rc

**Source:** `system/core/init/main.cpp` 第 84-111 行

```cpp
// 第 88-89 行：建立 /tmp/mini-aosp/ 資料夾
std::filesystem::create_directories(RUNTIME_DIR);

// 第 92-94 行：把自己的 PID 寫到檔案，方便 stop.sh 找到
std::ofstream pf(PID_FILE);
pf << getpid();
```

```cpp
// 第 105-106 行：解析 init.rc
miniaosp::log(TAG, "Parsing " + rc_path + "...");
auto services = parse_init_rc(rc_path);
```

`parse_init_rc()`（第 28-56 行）逐行讀檔案：
- 跳過空行和 `#` 開頭的註解
- 遇到 `service` 開頭的行，拆出 name 和 command
- 回傳一個 `Service` 結構的 vector：`[{servicemanager, [...]}, {system_server, [...]}, {hello_app, [...]}]`

> **對照真正 Android：** 真正的 init 也是解析 `/system/etc/init/` 下的 `.rc` 檔。
> 格式更複雜（有 `on boot`、`on property` 等 trigger），但核心概念一樣。

**你看到的 log：**
```
[init            ] Parsing /tmp/mini-aosp/init.rc...
```

---

## 第三段：init 用 fork+exec 啟動 servicemanager

**Source:** `system/core/init/main.cpp` 第 113-129 行

```cpp
for (auto& svc : services) {
    pid_t pid = launch_service(svc);  // fork + exec
    // ...
    usleep(500000); // 等 500ms 讓 service 初始化
}
```

`launch_service()`（第 60-78 行）是整個系統最關鍵的函數：

```cpp
pid_t pid = fork();    // ← kernel syscall：複製當前 process
if (pid == 0) {
    // 這裡是 child process
    execvp(argv[0], argv.data());  // ← kernel syscall：把自己替換成新程式
}
return pid;  // parent 拿到 child 的 PID
```

### fork() 做了什麼

`fork()` 是 Linux kernel 的 system call。它把當前 process **完整複製**一份：
- 複製後有兩個幾乎一模一樣的 process 在跑
- 差別只有 `fork()` 的回傳值：parent 拿到 child 的 PID，child 拿到 0

### exec() 做了什麼

`execvp()` 也是 kernel syscall。它把當前 process 的程式碼**整個替換**掉：
- 讀取 `out/bin/servicemanager` 這個 binary
- 載入它的 code、data、stack
- 從它的 `main()` 開始執行
- 原本 fork 出來的 init 副本就不存在了，變成了 servicemanager

所以 fork+exec 的組合效果是：**從 init 生出一個新 process，跑完全不同的程式。**

> **對照真正 Android：** 完全一樣。Android 的 init 用同樣的 fork+exec 啟動所有 native daemon。

**你看到的 log：**
```
[init            ] Starting servicemanager (PID 6531)...
```

---

## 第四段：servicemanager 建立 socket 開始監聽

**Source:** `frameworks/native/cmds/servicemanager/main.cpp` 第 77-110 行

servicemanager 被 exec 起來後，做三件事：

```cpp
// 1. 建立 Unix domain socket
int server_fd = socket(AF_UNIX, SOCK_STREAM, 0);  // ← kernel syscall

// 2. 綁定到檔案路徑
struct sockaddr_un addr;
addr.sun_family = AF_UNIX;
strncpy(addr.sun_path, "/tmp/mini-aosp/servicemanager.sock", ...);
bind(server_fd, &addr, sizeof(addr));  // ← kernel syscall

// 3. 開始監聽
listen(server_fd, 10);  // ← kernel syscall
```

### 什麼是 Unix domain socket

Unix domain socket 是 Linux kernel 提供的**同一台機器上 process 之間通訊 (IPC)** 的機制。
它跟網路 socket (TCP/UDP) 的 API 一模一樣（socket, bind, listen, accept, read, write），
但不走網路——直接在 kernel memory 裡傳資料，速度極快。

它綁定的不是 IP:port，而是**檔案路徑**（如 `/tmp/mini-aosp/servicemanager.sock`）。
任何 process 都能用 `connect()` 連到這個路徑來通訊。

### servicemanager 的 protocol

進入 accept loop 後（第 112-138 行），每收到一個連線就處理一個指令：

| 指令 | 回覆 | 用途 |
|------|------|------|
| `ADD_SERVICE ping /tmp/mini-aosp/ping.sock` | `OK` | 註冊 service |
| `GET_SERVICE ping` | `/tmp/mini-aosp/ping.sock` | 查詢 service 位置 |
| `LIST_SERVICES` | `ping ` | 列出所有已註冊 service |

內部就是一個 `unordered_map<string, string>` — service name 對應 socket path。

> **對照真正 Android：** 真正的 servicemanager 用 binder（kernel driver）而非 Unix socket。
> 但概念完全相同：一個集中的 service registry，讓所有 process 能互相找到。

**你看到的 log：**
```
[servicemanager  ] Listening on /tmp/mini-aosp/servicemanager.sock
```

---

## 第五段：init 啟動 system_server

跟第三段一模一樣的 fork+exec，但這次 exec 的是：
```
java -jar out/jar/system_server.jar
```

`java` 啟動 JVM，載入 JAR 裡的 `services.SystemServer.main()`。

**你看到的 log：**
```
[init            ] Starting system_server (PID 6532)...
```

---

## 第六段：system_server 註冊 PingService

**Source:** `frameworks/base/services/core/kotlin/SystemServer.kt` 第 28-41 行

### 6a. 連線到 servicemanager

```kotlin
val smAddr = UnixDomainSocketAddress.of("/tmp/mini-aosp/servicemanager.sock")
val channel = SocketChannel.open(StandardProtocolFamily.UNIX)
channel.connect(smAddr)  // ← 底層也是 kernel syscall: connect()
```

Java 16+ 支援 Unix domain socket，所以 Kotlin 可以直接用。
這裡的 `connect()` 最終也是呼叫 Linux kernel 的 `connect()` syscall。

### 6b. 送 ADD_SERVICE 指令

```kotlin
val request = "ADD_SERVICE ping /tmp/mini-aosp/ping.sock\n"
channel.write(ByteBuffer.wrap(request.toByteArray()))  // ← kernel syscall: write()

// 讀回覆
channel.read(buf)  // ← kernel syscall: read()
// response == "OK"
```

此時 servicemanager 那邊的 `handle_client()` 收到這個指令，
把 `{"ping" → "/tmp/mini-aosp/ping.sock"}` 存進 map，回 `OK`。

### 6c. 建立 PingService socket

```kotlin
val server = ServerSocketChannel.open(StandardProtocolFamily.UNIX)
server.bind(UnixDomainSocketAddress.of("/tmp/mini-aosp/ping.sock"))
```

然後進入 accept loop 等人來 PING。

> **對照真正 Android：** system_server 啟動時會註冊上百個 service
> （ActivityManagerService、PackageManagerService、WindowManagerService…）。
> 我們只註冊一個 PingService 作為 demo。

**你看到的 log：**
```
[system_server   ] Connecting to servicemanager...
[servicemanager  ] Registered service: ping → /tmp/mini-aosp/ping.sock
[system_server   ] Registered service: ping → /tmp/mini-aosp/ping.sock
[system_server   ] PingService listening...
```

注意同一件事出現了兩次——一次是 servicemanager 印的（收到註冊），一次是 system_server 印的（確認成功）。
它們是**兩個不同的 process**，各自印自己的 log。

---

## 第七段：init 啟動 HelloApp

又是 fork+exec，跑 `java -jar out/jar/HelloApp.jar`。

**你看到的 log：**
```
[init            ] Starting hello_app (PID 6551)...
```

---

## 第八段：HelloApp 完成 PING/PONG 旅程

**Source:** `packages/apps/HelloApp/HelloApp.kt` 第 18-42 行

### 8a. 向 servicemanager 查詢 PingService

```kotlin
// 連到 servicemanager
val channel = SocketChannel.open(StandardProtocolFamily.UNIX)
channel.connect(UnixDomainSocketAddress.of("/tmp/mini-aosp/servicemanager.sock"))

// 問：ping service 在哪裡？
channel.write("GET_SERVICE ping\n")

// 收到回覆：/tmp/mini-aosp/ping.sock
val response = channel.read(buf)  // → "/tmp/mini-aosp/ping.sock"
```

### 8b. 連到 PingService，送 PING

```kotlin
val startTime = System.nanoTime()

// 連到 system_server 的 PingService socket
val channel = SocketChannel.open(StandardProtocolFamily.UNIX)
channel.connect(UnixDomainSocketAddress.of("/tmp/mini-aosp/ping.sock"))

// 送 PING
channel.write("PING\n")

// 收 PONG
val response = channel.read(buf)  // → "PONG uid=6532 pid=6532 time=..."

val elapsed = (System.nanoTime() - startTime) / 1_000_000.0  // 算 round-trip 時間
```

### 8c. 印出成功訊息，退出

```kotlin
Log.i(TAG, "Received: PONG — round-trip 10.3ms")
Log.i(TAG, "✓ Full stack verified: App → SystemServer → ServiceManager → init → kernel")
// main() 結束，JVM 退出，process 結束
```

> **對照真正 Android：** 真正的 app 用 `getSystemService()` 取得 service 的 binder proxy，
> 然後直接呼叫方法。我們用 text protocol 模擬了同樣的「查詢 → 連線 → 通訊」流程。

**你看到的 log：**
```
[HelloApp        ] Connecting to servicemanager...
[HelloApp        ] Resolved service: ping → /tmp/mini-aosp/ping.sock
[HelloApp        ] Sending PING...
[system_server   ] PingService: PING from uid=6532 pid=6532
[HelloApp        ] Received: PONG — round-trip 10.3ms
[HelloApp        ] ✓ Full stack verified: App → SystemServer → ServiceManager → init → kernel
```

---

## 第九段：init 等待 + 卡住

**Source:** `system/core/init/main.cpp` 第 133-163 行

```cpp
while (!g_shutdown) {
    pid_t exited = waitpid(-1, &status, WNOHANG);  // ← kernel syscall：檢查有沒有 child 退出
    if (exited > 0) {
        // 找到退出的 child，印 log
        // 檢查是不是所有 child 都退出了
    }
    usleep(100000); // 100ms 後再檢查
}
```

`waitpid(-1, ..., WNOHANG)` 是 kernel syscall：
- `-1` = 監控所有 child process
- `WNOHANG` = 不等待，有退出就回報，沒有就立刻返回

HelloApp 退出後，init 偵測到：
```
[init            ] hello_app exited with code 0
```

但 servicemanager 和 system_server 還活著，所以 init 繼續 loop。
**這就是你看到「卡住」的原因——init 在等剩下的 daemon。**

這是正確的行為。就像你的手機開著的時候，init 也永遠在跑。

### 怎麼結束

按 `Ctrl+C` 送 `SIGINT`，或從另一個 terminal 執行 `./scripts/stop.sh`。

init 收到 signal 後（第 81-82 行）：
```cpp
volatile sig_atomic_t g_shutdown = 0;
void handle_signal(int) { g_shutdown = 1; }
```

`g_shutdown` 變成 1，while loop 結束，進入 shutdown 流程（第 166-185 行）：
1. 對每個還活著的 child 送 `SIGTERM`
2. 等 2 秒讓它們 graceful shutdown
3. 還沒死的送 `SIGKILL` 強制終止
4. 清理 PID 檔案

---

## 完整 IPC 路徑圖

```
                        kernel syscalls 用到的
                        ════════════════════

HelloApp ──connect()──▶ servicemanager.sock  ──read()/write()──  servicemanager
         ◀─response────                                          (查 map，回覆路徑)

HelloApp ──connect()──▶ ping.sock  ──read()/write()──  system_server
         ◀─"PONG"─────                                 (收 PING，回 PONG)

init 用 fork()+exec() 啟動以上所有 process
init 用 waitpid() 監控它們的生死
init 用 kill()+signal() 控制 shutdown
```

每一條線都是**真正的 kernel syscall**——不是模擬，不是 mock。
這就是為什麼這個 prototype 有意義：它證明了完整的 Android boot → IPC → app 流程。

---

## Kernel Syscall 清單

這個 Stage 0 prototype 使用了以下 Linux kernel system calls：

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
| `signal()` | init, servicemanager | 註冊 signal handler |
| `getpid()` | init | 取得自己的 PID |
| `unlink()` | servicemanager | 刪除 socket 檔案 |
