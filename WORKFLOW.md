# WORKFLOW.md：多聊天室開發流程

> 本文件說明 Nexus 如何用多個 Claude 聊天室協作開發。
> 所有聊天室開始工作前都必須先讀：`CLAUDE.md` → `WORKFLOW.md` → `TASKS.md`。

---

## 1. 核心原則

1. **聊天室之間不共享記憶**。所有共識、決定、進度一律寫進 repo 的檔案，不能只存在對話裡。
2. **單一事實來源**：
   - `PLAN.md`：整體方向與 Milestone（只有 PM 可修改）
   - `CLAUDE.md`：程式碼規範（只有 PM 可修改）
   - `TASKS.md`：任務卡與進度（PM 新增任務；開發者只能改自己任務的狀態與備註）
   - `docs/decisions/`：架構決策紀錄（ADR）
3. **一個開發聊天室 = 一個 git 分支 = 一個 worktree 資料夾**，互不干擾。
4. **只有 PM 能合併分支**：作者已於 2026-10-25 授權 PM（Opus）在審核通過後直接合併進 `main`。開發者一律不得自行合併。作者保留隨時收回授權或否決合併的權利。

---

## 2. 聊天室角色

| 聊天室 | 模型 | 負責範圍 | 啟用時機 |
|---|---|---|---|
| PM | Opus 5.5 | 規劃、拆任務、審核計畫與 diff、更新文件 | 一開始 |
| 開發 A | Sonnet 5.5 | `src/core/`、`src/parser/`、`src/storage/` | 一開始（M0、M1 由 A 獨自完成） |
| 開發 B | Sonnet 5.5 | `src/ui/` | M2 開始 |
| 開發 C | Sonnet 5.5 | `src/ai/`、CMake、CI、建置腳本 | M2 開始 |
| 開發 D | Sonnet 5.5 | `src/ui/`（2026-10-25 起 UI 專責；B 暫時支援 parser／storage） | M1 UI 開始 |

**M0、M1 只開 PM + 開發 A。**（2026-10-25 PM 修訂：M0 骨架完成、核心介面穩定後，提早開 B、C 平行處理 M1 的獨立模組與 CI，見 TASKS.md。） 骨架與基礎層前後依賴重，平行開發只會衝突。M1 完成、核心介面穩定後，才開 B 和 C。

開發聊天室遇到困難的記憶體錯誤、多執行緒問題，可暫時切換為 Opus 處理，修完再切回 Sonnet。

---

## 3. 各角色規則

### PM（Opus）
- **不寫產品程式碼**，只寫文件與任務卡。
- 依 `PLAN.md` 的 Milestone 把工作拆成任務卡寫進 `TASKS.md`，每張卡必須有：負責人、目標、會動到的檔案、完成標準、依賴。
- 一張任務卡的大小以「一個開發聊天室一次對話能完成」為準。
- 分配任務時避免兩位開發者同時修改同一個檔案；共用檔案（如根目錄 `CMakeLists.txt`）只交給一個人。
- 審核開發者提交的計畫與 diff，回覆「可合併」或列出需修改處。
- 重要決定寫成 ADR 放進 `docs/decisions/`，檔名 `NNNN-標題.md`，內容：背景、選項、決定、理由。

### 開發者（Sonnet）
- 只做 `TASKS.md` 中指派給自己的任務，只修改自己負責範圍的檔案。
- 需要修改範圍外的檔案時，停下來在任務備註寫明原因，請作者轉給 PM 決定。
- 動手前先列出計畫（新增/修改哪些檔案、做法），等作者確認後再寫。
- 完成標準：建置成功、所有測試通過、asan preset 無錯誤、clang-tidy 零警告。
- 完成後：
  1. 在 `TASKS.md` 把任務狀態改成 `待審核`，備註寫上做了什麼、注意事項
  2. commit 到自己的分支，commit 訊息格式：`[T-編號] 簡述`
  3. 輸出一段給 PM 審核用的摘要（改了哪些檔案、為什麼、有什麼風險）

---

## 4. Git 分支與 Worktree

主資料夾 `nexus/` 固定在 `main` 分支，給 PM 使用。

每個開發者一個 worktree（在主資料夾執行一次即可）：

```bash
git worktree add ../nexus-dev-a -b dev-a
git worktree add ../nexus-dev-b -b dev-b   # M2 開始
git worktree add ../nexus-dev-c -b dev-c   # M2 開始
```

在 VS Code 中分別開啟這些資料夾，各自開一個 Claude 聊天室。

**開始新任務前**，開發者先同步 main：

```bash
git fetch && git rebase main
```

**合併流程**（PM 執行）：開發者完成 → PM 審核通過 → PM 在 main 執行 `git merge --no-ff dev-x` → 建置與測試仍通過才算合併完成 → 通知其他開發者 rebase。

---

## 5. 任務狀態

`待辦` → `進行中` → `待審核` → `已完成`
被退回時從 `待審核` 改回 `進行中`，並在備註寫退回原因。
卡住時標 `阻塞`，並寫明卡在哪。

---

## 6. 一次完整循環

1. **PM**：拆好任務卡寫進 `TASKS.md`
2. **作者**：對開發聊天室說「做 T-XXX」
3. **開發者**：讀文件 → 提出計畫 → 作者確認 → 實作 → 測試 → 更新 TASKS.md → commit → 輸出審核摘要
4. **作者**：把審核摘要貼給 PM
5. **PM**：審核 → 合併 / 退回
6. **PM**：合併後更新進度並拆下一批任務；向作者回報結果

---

## 7. 開場指令（直接複製貼上）

**PM 聊天室：**
> 你是 Nexus 的 PM。請依序閱讀 CLAUDE.md、PLAN.md、WORKFLOW.md、TASKS.md，確認你理解自己的角色與規則，然後檢查 TASKS.md 目前的任務卡是否需要調整。

**開發 A 聊天室：**
> 你是 Nexus 的開發 A。請依序閱讀 CLAUDE.md、PLAN.md、WORKFLOW.md、TASKS.md，然後開始 TASKS.md 中指派給 A 的第一個「待辦」任務。動手前先列出計畫。

**開發 B / C 聊天室（M2 開始）：**
> 你是 Nexus 的開發 B（或 C）。請依序閱讀 CLAUDE.md、PLAN.md、WORKFLOW.md、TASKS.md，然後開始指派給你的第一個「待辦」任務。動手前先列出計畫。

**之後每次新任務：**
> 讀 TASKS.md，做 T-XXX。

**交給 PM 審核：**
> 審核以下 T-XXX 的提交：（貼上開發者的摘要）
