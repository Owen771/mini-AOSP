# mini-AOSP 設置指南

## 需求

| 工具 | 用途 | 最低版本 |
|------|------|----------|
| g++ 或 clang++ | 編譯 C++ (init, servicemanager) | C++17 支援 |
| Java (JDK) | 執行 Kotlin JAR (system_server, HelloApp) | 16+ (需要 Unix domain socket API) |
| kotlinc | 編譯 Kotlin source → JAR | 任意近期版本 |
| make | Build system | 任意 |

## 本機開發 (macOS)

```bash
# 一次性安裝
./scripts/setup-env.sh

# 編譯
make -C build

# 啟動（前景執行，Ctrl+C 停止）
./scripts/start.sh

# 或從另一個 terminal 停止
./scripts/stop.sh
```

## 部署到 K8s Pod (Debian/Linux)

### 第一步：從本機推送到 pod

```bash
./scripts/deploy.sh
```

預設 pod 和 namespace 寫在 script 裡。要覆蓋的話：

```bash
./scripts/deploy.sh <pod-name> <namespace>
```

`deploy.sh` 做了三件事：
1. `tar czf` 打包（排除 `out/` 和 `.git/`）
2. `kubectl cp` 複製到 pod 的 `/tmp/mini-aosp.tar.gz`
3. `kubectl exec` 在 pod 上解壓到 `/tmp/mini-AOSP/`

### 第二步：在 pod 上安裝 + 編譯

```bash
kubectl -n <namespace> exec -it <pod> -- bash
cd /tmp/mini-AOSP
./scripts/bootstrap.sh
```

`bootstrap.sh` 做了四件事：
1. `apt-get install` g++, make, openjdk-17（已裝就跳過）
2. 下載 Kotlin compiler 到 `/opt/kotlin/`（已裝就跳過）
3. 驗證所有工具版本
4. `make -C build all` 編譯

整個過程是 idempotent 的——重跑只會重新編譯，不會重裝。

### 第三步：執行

```bash
./scripts/start.sh
```

### 其他指令

| 指令 | 用途 |
|------|------|
| `./scripts/start.sh` | 啟動整個系統（前景執行） |
| `./scripts/stop.sh` | 停止所有 process |
| `./scripts/status.sh` | 查看執行中的 process、socket、記憶體用量 |
| `./scripts/clean.sh` | 刪除 `out/` build artifacts |
| `make -C build` | 編譯全部 |
| `make -C build cpp` | 只編譯 C++ |
| `make -C build kotlin` | 只編譯 Kotlin |

## Build 產出

```
out/
├── bin/
│   ├── init              ← C++ binary, 類似 Android 的 PID 1
│   └── servicemanager    ← C++ binary, service 註冊中心
└── jar/
    ├── system_server.jar ← Kotlin, framework 層的核心 service
    └── HelloApp.jar      ← Kotlin, demo app
```

## 故障排除

**`kotlinc: command not found`**
→ `bootstrap.sh` 會自動安裝。如果手動跑，確認 PATH 有 `/opt/kotlin/kotlinc/bin`

**`java: command not found` 或版本太低**
→ 需要 JDK 16+，Debian 11 上 `apt install openjdk-17-jdk`

**`Address already in use` (socket)**
→ 上次沒有乾淨關閉。執行 `./scripts/stop.sh` 或 `rm /tmp/mini-aosp/*.sock`

**`start.sh` 說 "already running"**
→ `./scripts/stop.sh` 先停掉，或 `rm /tmp/mini-aosp/init.pid`
