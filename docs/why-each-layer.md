# 為什麼需要這些東西？— Top-Down 動機鏈

> 從一個 app 開發者的需求出發，一路追問「為什麼」，
> 直到碰到 Linux kernel。每一層的存在都由上一層的需求驅動。
>
> **沒有一個元件是任意的。每一個都是因為上面那層少了它就活不下去。**

---

## 起點：App A 想呼叫 App B 的方法

```kotlin
// 我是 App A 的開發者。我想控制 MusicApp 的播放。
val player = getService("music_player") as IMusicPlayer
player.play("song.mp3")
```

看起來很簡單。但底下需要解決一連串問題。

---

## 第一個問題：App A 和 App B 在不同的 process 裡

```
App A 的記憶體空間          App B 的記憶體空間
┌──────────────────┐       ┌──────────────────┐
│ player.play()    │──✗──▶│ MusicPlayer 實體  │
│                  │ 看不到 │                  │
└──────────────────┘       └──────────────────┘
```

Kotlin object 存在 App A 的 heap 裡。App B 的 MusicPlayer 在完全不同的 memory space。
你不能直接呼叫——就像你不能直接開車到另一個城市的房間裡。

### 為什麼要不同 process？

**安全。** Android 的核心安全模型：每個 app 一個 Linux UID，一個 process。
App A 不能讀 App B 的記憶體、不能存取 App B 的檔案。
如果所有 app 跑在同一個 process，任何 app 都能讀你的密碼。

**穩定。** App B crash 不會拖垮 App A。

→ **所以我們需要跨 process 通訊的機制。**

---

## 需要 #1：IPC — 跨 Process 通訊

兩個 process 要通訊，必須透過 **kernel**——因為只有 kernel 能同時碰到兩邊的記憶體。

Linux 提供的 IPC 機制：pipe、shared memory、message queue、**socket**。

Android 選 socket-based 方案（Binder 底層也是 kernel 幫忙搬資料），因為：
- 可以附帶 **caller identity**（UID/PID）——知道是誰在呼叫
- 支援 **雙向通訊**（request + reply）
- 可以 **多對多**（多個 client 連到一個 server）

→ **這就是為什麼需要 Binder / Unix domain socket。**

```
我想呼叫 App B 的方法
  → 它們在不同 process（安全 + 穩定）
    → 需要 kernel 幫忙傳資料
      → 需要 IPC 機制
        → Binder（或我們用的 Unix socket）
```

---

## 需要 #2：Parcel — 序列化

問題：你要傳 `play("song.mp3")` 給 App B。
但 socket 只能傳 **bytes**，不能傳 Kotlin object。

你需要把「函數名稱 + 參數」變成 bytes（序列化），送過去，
對方再從 bytes 還原成「函數名稱 + 參數」（反序列化）。

```
App A:  play("song.mp3")
        ↓ 序列化
        [method_code=1][string_len=9]["song.mp3"]  ← bytes
        ↓ socket write
        ─── kernel ───
        ↓ socket read
App B:  [method_code=1][string_len=9]["song.mp3"]
        ↓ 反序列化
        play("song.mp3")
```

→ **這就是為什麼需要 Parcel。**

為什麼不用 JSON？太慢。Binder call 每秒可能發生數千次（畫面每幀都要跨 process）。
Parcel 是 binary format，直接 memcpy，零解析開銷。

---

## 需要 #3：Proxy/Stub — 讓開發者不用管序列化

問題：如果每次跨 process 呼叫都要手寫 Parcel 序列化，開發者會瘋掉：

```kotlin
// 沒有 Proxy 的世界——每次呼叫都要手動序列化
val parcel = Parcel()
parcel.writeInt(1)  // method code
parcel.writeString("song.mp3")
val reply = Parcel()
binder.transact(handle, parcel, reply)
val success = reply.readBoolean()
```

我們想要的是：

```kotlin
// 有 Proxy 的世界——看起來像本地呼叫
player.play("song.mp3")
```

Proxy（client 側）自動把參數寫進 Parcel + 送出。
Stub（server 側）自動從 Parcel 讀出參數 + 呼叫真正的實作。

AOSP 命名：**Bp** (Binder Proxy) / **Bn** (Binder Native)。

→ **這就是為什麼需要 Proxy/Stub pattern (Bp/Bn)。**

---

## 需要 #4：AIDL — 自動產生 Proxy/Stub

問題：每個 service 都要手寫一對 Bp/Bn，重複又容易出錯。

解法：定義一個 interface 檔案，讓工具自動產生。

```
// IMusicPlayer.aidl
interface IMusicPlayer {
    boolean play(String path);
    void pause();
    int getPosition();
}
```

→ codegen 自動產生 `BpMusicPlayer.kt`（Proxy）和 `BnMusicPlayer.kt`（Stub）

→ **這就是為什麼需要 AIDL codegen。**

```
我想呼叫 App B
  → 需要 IPC（Binder）
    → 需要序列化（Parcel）
      → 想自動化序列化（Proxy/Stub）
        → 想自動產生 Proxy/Stub（AIDL）
```

---

## 需要 #5：servicemanager — 怎麼找到 App B？

問題：App A 要呼叫 MusicPlayer service，但它在另一個 process 裡。
App A 怎麼知道要連到哪個 socket？

```kotlin
// App A 只知道 service 的名字
val player = ???.getService("music_player")
// 它不知道（也不該知道）MusicPlayer 的 socket path
```

解法：一個 **中央註冊中心**。所有 service 啟動時來註冊名字，client 來查詢。

```
MusicApp 啟動:  servicemanager.addService("music_player", myBinder)
App A 查詢:     servicemanager.getService("music_player") → binder handle
App A 呼叫:     handle.play("song.mp3")
```

servicemanager 就是 Android 的 **電話簿**——你知道名字，它告訴你怎麼連。

→ **這就是為什麼需要 servicemanager。**

---

## 需要 #6：Looper/Handler — main thread 不能 block

問題：App B 收到 Binder call 時，是在 **binder thread** 上。
但 Android 的 lifecycle callback（`onCreate`, `onPause`...）必須在 **main thread** 上跑。

如果 binder thread 直接呼叫 `activity.onPause()`，會有 race condition
（兩個 thread 同時改 Activity state）。

解法：binder thread 收到消息後，用 **Handler** 把工作 **post** 到 main thread 的 **MessageQueue**。
Main thread 的 **Looper** 不斷從 queue 取消息執行。

```
Binder thread:  收到 "pause this activity"
                → handler.post { activity.onPause() }
                → 放進 MessageQueue

Main thread:    Looper.loop()
                → 從 MessageQueue 取出
                → 在 main thread 上執行 activity.onPause()
```

→ **這就是為什麼需要 Looper + Handler + MessageQueue。**

---

## 需要 #7：AMS — 誰來管 lifecycle？

問題：5 個 app 同時在跑。用戶切到 App C。
誰來告訴 App A「你要 onPause 了」？誰來告訴 App C「你要 onResume 了」？

不能讓 app 自己管——它們可以撒謊（「我永遠是 foreground！別 kill 我！」）。

解法：一個**中央管理者** ActivityManagerService (AMS)：
- 追蹤每個 app 的 lifecycle 狀態
- 在正確的時機推送 callback（透過 app 給的 binder）
- 決定 process 優先順序（oom_adj）
- 可以 force-stop 任何 app

→ **這就是為什麼需要 ActivityManagerService。**

---

## 需要 #8：PMS — 怎麼知道有哪些 app？怎麼解析 intent？

問題 1：AMS 要啟動 NotesApp，但它怎麼知道 NotesApp 存在？裝在哪裡？UID 是多少？

問題 2：App A 送一個 implicit intent `ACTION_SEND`，系統怎麼知道誰能處理？

```kotlin
// App A 不指定目標，只說「我要分享文字」
startActivity(Intent(action = "ACTION_SEND", extras = { text: "hello" }))
// 系統要找出誰註冊了 ACTION_SEND → ReceiverApp
```

解法：PackageManagerService (PMS)：
- 開機時掃描所有 app manifest
- 記住每個 app 的 package name、UID、components、intent-filters
- 提供 `resolveIntent()` API 給 AMS 用

→ **這就是為什麼需要 PackageManagerService。**

---

## 需要 #9：Zygote — 啟動 app 太慢了

問題：每次啟動一個 app 都要 `java -jar app.jar`——JVM 啟動要 1-2 秒。
用戶點一個 app icon，2 秒才有反應，不可接受。

解法：**預先啟動一個 JVM**（Zygote），載入共用的 framework class。
需要新 app 時，`fork()` 一份——copy-on-write，幾乎瞬間。

```
Zygote（已載入 framework）
  ├─ fork() → App A（共享 framework，只佔差異的記憶體）
  ├─ fork() → App B
  └─ fork() → App C
```

5 個 app 共享一份 framework code → 省 ~150MB RAM。
（Phase 1 我們用 fork+exec 近似，Phase 2 改嵌入式 JVM 做真正的 fork-without-exec）

→ **這就是為什麼需要 Zygote。**

---

## 需要 #10：lmkd — 記憶體不夠了

問題：手機只有 4GB RAM。OS 自己用 2GB，剩 2GB 給 app。
用戶開了 10 個 app，記憶體滿了。怎麼辦？

不能讓 app 自己決定——沒有 app 會自願退出。

解法：lmkd（Low Memory Killer Daemon）：
- 監控系統記憶體壓力
- 按 AMS 算出的 oom_adj 優先順序 kill app
- 最低優先的（CACHED，最久沒用的）先死
- 死之前 AMS 已經 `onSaveInstanceState` 保存了 state
- 用戶切回時，Zygote 重新 fork，`onCreate(savedState)` 恢復

**這就是 Android 能在 4GB 上跑 20 個 app 的祕密。**

→ **這就是為什麼需要 lmkd。**

---

## 需要 #11：BroadcastReceiver — 系統事件通知

問題：系統記憶體不足了，想告訴所有 app「請釋放 cache」。
開機完成了，想告訴所有 app「可以做初始化了」。

不能一個一個通知——系統不知道誰想聽什麼事件。

解法：pub/sub 模式。App 在 manifest 裡宣告「我要聽 LOW_MEMORY」。
系統 broadcast 時，AMS 查 PMS 的 intent-filter index，通知所有符合的 receiver。

→ **這就是為什麼需要 BroadcastReceiver。**

---

## 需要 #12：init — 誰來啟動這一切？

問題：以上所有元件——servicemanager、zygote、system_server、lmkd——
都是獨立 process。誰來啟動它們？按什麼順序？如果它們 crash 了怎麼辦？

解法：init（PID 1）：
- Kernel 開機後執行的第一個 process
- 讀 init.rc，按順序啟動所有 daemon
- 監控 child process，crash 了自動重啟
- 提供 property system（system-wide key-value config）

→ **這就是為什麼需要 init。**

---

## 完整動機鏈

```
App A 想呼叫 App B 的方法
│
├─ 它們在不同 process → 需要 IPC
│   └─ IPC 需要傳 bytes → 需要 Parcel（序列化）
│       └─ 序列化太煩 → 需要 Proxy/Stub（Bp/Bn）
│           └─ Proxy/Stub 太重複 → 需要 AIDL（codegen）
│
├─ 怎麼找到 App B → 需要 servicemanager（service discovery）
│
├─ Callback 要在 main thread → 需要 Looper/Handler
│
├─ 誰管 lifecycle → 需要 AMS
│   └─ AMS 要知道有哪些 app → 需要 PMS
│       └─ PMS 解析 implicit intent → 需要 intent-filter
│
├─ 啟動 app 太慢 → 需要 Zygote（fork）
│
├─ 記憶體不夠 → 需要 lmkd（kill by priority）
│   └─ Kill 前要存 state → 需要 onSaveInstanceState
│   └─ 優先順序怎麼算 → 需要 OomAdjuster
│
├─ 系統事件通知 → 需要 BroadcastReceiver
│
└─ 以上全部要有人啟動 → 需要 init（PID 1）
```

---

## 對應到我們的 Stage

| 動機 | 元件 | Stage | 語言 |
|------|------|-------|------|
| 跨 process 通訊 | Binder transport | 2 | C (native) |
| 序列化 | Parcel | 0 | C + Kotlin |
| 自動化序列化 | Bp/Bn (Proxy/Stub) | 2 | Kotlin (generated) |
| 自動產生 Bp/Bn | AIDL codegen | 2 | Python (tool) |
| Service discovery | servicemanager | 3 | C (native) |
| Main thread dispatch | Looper/Handler/MessageQueue | 0 | C + Kotlin |
| Lifecycle 管理 | AMS | 6, 8 | Kotlin (framework) |
| App 資訊 + intent | PMS | 4, 7 | Kotlin (framework) |
| 快速啟動 app | Zygote | 5 | C (native) |
| 記憶體管理 | lmkd | 8 | C (native daemon) |
| 系統事件 | BroadcastReceiver | 8 | Kotlin (framework) |
| 啟動一切 | init | 0, 1 | C (native, PID 1) |

**語言分界（DEC-013）：**
- **C：** init、servicemanager、zygote、lmkd、Binder library、Looper (native) — 直接碰 kernel syscall 的都是 C
- **Kotlin：** system_server、AMS、PMS、所有 framework service、所有 app — 業務邏輯在 Kotlin
