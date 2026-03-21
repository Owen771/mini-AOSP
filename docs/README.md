# mini-AOSP Documentation

---

## Getting Started

| Doc | 內容 |
|-----|------|
| [00-setup.md](./00-setup.md) | 環境安裝、build、deploy |

## Phase 1（按閱讀順序）

| # | Doc | 內容 |
|---|-----|------|
| 01 | [why-each-layer.md](./phase-1/01-why-each-layer.md) | 為什麼需要每個元件？從 app 需求一路追到 kernel |
| 02 | [learning-guide.md](./phase-1/02-learning-guide.md) | Phase 1 完整學習路線（Stage 0-8） |
| 03 | [system-walkthrough.md](./phase-1/03-system-walkthrough.md) | 從 start.sh 到 PONG 的每一步，對應到每一行 code |
| 05 | [aosp-cross-check.md](./phase-1/05-aosp-cross-check.md) | 每個 Stage 與真正 AOSP source 的交叉驗證 |

## Components（元件設計文件）

| # | Doc | 內容 |
|---|-----|------|
| 04 | [servicemanager.md](./components/04-servicemanager.md) | servicemanager 設計：現狀、Stage 1 後、Stage 3 最終狀態 |

## Reference

| Doc | 內容 |
|-----|------|
| [conventions.md](./reference/06-conventions.md) | C 命名慣例、變數前綴、函數命名、常數命名 |

## Stages（實作計畫）

| Stage | 主題 | Doc |
|-------|------|-----|
| 0 | 基礎設施：Looper + Parcel | [stage-0.md](./stages/stage-0.md) |
| 1 | init 強化：crash-restart + property store | [stage-1.md](./stages/stage-1.md) |
| 2 | Binder IPC transport | [stage-2.md](./stages/stage-2.md) |
| 3 | servicemanager 改用 Binder | [stage-3.md](./stages/stage-3.md) |
| 4 | PMS + Intent | [stage-4.md](./stages/stage-4.md) |
| 5 | Zygote | [stage-5.md](./stages/stage-5.md) |
| 6 | AMS + Activity lifecycle | [stage-6.md](./stages/stage-6.md) |
| 7 | 多 app 支援 | [stage-7.md](./stages/stage-7.md) |
| 8 | lmkd + BroadcastReceiver | [stage-8.md](./stages/stage-8.md) |
| — | Phase 1 總結 + Phase 2 展望 | [summary.md](./stages/summary.md) |

## 閱讀建議

**第一次讀：** 00-setup → 01-why-each-layer → 03-system-walkthrough
（先裝好環境，理解動機，再看 code 怎麼跑）

**開始寫 code 前：** 02-learning-guide → 對應的 stage-N.md
（看路線圖，再看該 stage 的詳細步驟）

**寫 code 時查閱：** 06-conventions → 03-system-walkthrough → 04-servicemanager
（命名規則、執行流程、元件設計）

**Review 時：** 05-aosp-cross-check
（確認設計跟真正 AOSP 一致）
