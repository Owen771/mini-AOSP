# servicemanager 設計文件

---

## 它是什麼

servicemanager 是 Android 的**電話簿** (service registry)。
它是 init 啟動的第一個 daemon，所有其他 process 開機後第一件事就是連它。

在真正 Android 裡，servicemanager 是 **Binder handle 0** — 一個所有 process 都知道的固定地址，不需要查詢就能連到。

---

## 職責

1. **接受 service 註冊** — system_server 啟動後把自己的 service 登記進來
2. **提供 service 查詢** — app 想用某個 service 時，先來這裡問地址
3. **作為 IPC 的起點** — 所有 process 都從連接 servicemanager 開始，再透過它找到其他 service

**重點：servicemanager 只做 discovery，不做 data relay。**

client 拿到 service 的地址後，直接跟 service 通訊，不再經過 servicemanager：

```
HelloApp                    servicemanager              system_server
   │                              │                          │
   │──GET_SERVICE ping──────▶│                          │
   │◀─/tmp/.../ping.sock────│                          │
   │                              │                          │
   │──────────────PING──────────────────────────────▶│  ← 直接連，不經過 SM
   │◀─────────────PONG──────────────────────────────│
```

這跟真正 Android 一樣 — `getSystemService()` 拿到 Binder proxy 後，
後續的 `transact()` 走直接的 Binder IPC，不繞回 servicemanager。

---

## 目前狀態 (Stage 0)

**Source:** `frameworks/native/cmds/servicemanager/main.c`

| 項目 | 狀態 |
|------|------|
| Transport | Unix domain socket（text protocol） |
| Protocol | `ADD_SERVICE <name> <path>\n` / `GET_SERVICE <name>\n` / `LIST_SERVICES\n` |
| Registry 儲存 | `struct service_entry g_services[MAX_SERVICES]`（固定大小 array） |
| Caller identity | 無 — 不知道是誰在註冊/查詢 |
| 型別安全 | 無 — 全是 string，打錯字只能 runtime 發現 |
| Crash recovery | 無 — crash 就永遠死了 |
| Death notification | 無 — service 死了 client 不知道 |

### 內部結構

```c
struct service_entry {
    char name[MAX_NAME];            // e.g. "ping"
    char socket_path[MAX_PATH_LEN]; // e.g. "/tmp/mini-aosp/ping.sock"
};

static struct service_entry g_services[MAX_SERVICES]; // 最多 64 個
static int g_n_services = 0;                           // 目前幾個
```

### 函數地圖

| 函數 | 職責 |
|------|------|
| `create_listening_socket()` | socket() → bind() → listen() 三步 |
| `signal_readiness()` | 寫 .ready 檔案讓 init 繼續 |
| `handle_client()` | 讀 request → tokenize command → dispatch |
| `handle_add_service()` | 存 name + path 到 g_services[] |
| `handle_get_service()` | 查 g_services[] 回傳 path 或 NOT_FOUND |
| `handle_list_services()` | 列出所有已註冊 service 的名稱 |
| `find_service()` | 線性搜尋 g_services[] by name |

---

## Stage 1 之後的變化

Stage 1 主要強化 init（crash-restart + property store），servicemanager 自身 code 不變。

但間接受益：

| 改變 | 影響 |
|------|------|
| init 獲得 crash-restart | servicemanager crash → init 偵測到 → 自動重啟（帶 exponential backoff） |
| init 獲得 property store | servicemanager 啟動後可以 `SETPROP sys.servicemanager.ready 1` |
| init.rc 新增 `restart` 語法 | servicemanager 標記為 `restart always`（persistent daemon） |

Stage 1 完成後的 init.rc：

```
service servicemanager /path/to/servicemanager
    wait_for /tmp/mini-aosp/servicemanager.ready
    restart always       # ← 新增：crash 後自動重啟

service hello_app /path/to/HelloApp.jar
    restart oneshot      # ← 新增：跑完就算了，不重啟
```

---

## Stage 2-3 最終狀態

Stage 2 實作 Binder IPC transport，Stage 3 把 servicemanager 改成 Binder-based。

| 項目 | Stage 0（目前） | Stage 3（最終） |
|------|----------------|----------------|
| Transport | Unix socket + text | Binder transaction（binary） |
| Protocol | `ADD_SERVICE ping /path\n` | Binder `transact(code=ADD, parcel{name, handle})` |
| Serialization | 手動 string parsing | Parcel binary format（自動 codegen） |
| Caller identity | 無 | 每個 transaction 帶 UID/PID |
| Interface 定義 | 無 | `IServiceManager.aidl` → codegen Proxy/Stub |
| Death notification | 無 | `linkToDeath()` — service crash 時通知所有 client |
| Handle | 透過 socket path 找到 | handle 0 — 固定地址，不需要 discovery |
| Client code | 手動 socket connect + write | `ServiceManager.getService("ping")` → 拿到 Binder proxy |

Stage 3 的 IServiceManager AIDL：

```
interface IServiceManager {
    void addService(String name, IBinder service);
    IBinder getService(String name);
    String[] listServices();
}
```

codegen 自動產生 `BpServiceManager`（client proxy）和 `BnServiceManager`（server stub），
所有 process 用同一套 Binder 機制跟 servicemanager 溝通。

---

## 對照真正 AOSP

| | 真正 AOSP | mini-AOSP (Stage 0) | mini-AOSP (Stage 3) |
|---|---|---|---|
| Source | `frameworks/native/cmds/servicemanager/` | 同路徑 | 同路徑 |
| Language | C++ | C | C |
| Transport | Binder (kernel driver `/dev/binder`) | Unix socket | Unix socket Binder (userspace) |
| Handle | context manager (handle 0) | 第一個啟動的 daemon | handle 0 |
| Interface | `IServiceManager.aidl` | 手寫 text protocol | `IServiceManager.aidl` + codegen |
| Security | SELinux policy 控制誰能註冊什麼 | 無 | UID/PID check |

**真正 AOSP source 對照：**
```
frameworks/native/cmds/servicemanager/main.cpp
frameworks/native/cmds/servicemanager/ServiceManager.cpp
frameworks/native/libs/binder/IServiceManager.cpp
out/soong/.intermediates/.../IServiceManager.cpp  ← AIDL codegen 產生
```
