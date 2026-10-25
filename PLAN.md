# 專案計畫書：知識圖驅動的 AI 開發環境

> 暫定名稱：**Nexus**（可自行更換）
> 平台：Windows、Linux
> 語言：C++20
> 開發方式：人負責架構與規格，AI（Claude）負責撰寫程式碼

---

## 1. 願景

現在的 IDE（如 VS Code）仍以「人寫程式碼」為中心設計。但在 AI 時代，開發者真正需要管理的是：

- **意圖**：要做什麼、為什麼做
- **結構**：模組、架構、決策之間的關係
- **脈絡**：哪些知識該交給 AI，才能讓它正確地改程式

Nexus 的核心想法：**以 Obsidian 式的連結知識庫為主體，程式碼是 AI 根據知識產出的結果。**

使用者在 Nexus 中撰寫需求、規格、架構決策，並用雙向連結串起來；接著選取相關筆記，交由 AI agent 去實作或修改專案程式碼。

---

## 2. 目標使用者

第一階段：**用 AI 寫程式的個人開發者**（包含作者本人）。

作者本人即為第一位使用者：Nexus 本身就用「寫規格 → 交給 AI 實作」的方式開發。

---

## 3. 技術選型

| 項目 | 選擇 | 理由 |
|---|---|---|
| 語言 | C++20（錯誤處理用 `tl::expected`，ADR 0002） | 極致效能、完整掌控 |
| 建置 | CMake（3.24+）+ Ninja | 跨平台標準 |
| 套件管理 | vcpkg（manifest 模式）；Qt 例外，走官方安裝 / install-qt-action（ADR 0001） | Windows / Linux 皆可用 |
| UI | Qt 6.8.3（Widgets 為主） | 最成熟的 C++ 跨平台 UI，內建文字編輯、畫布、網路、行程管理 |
| Markdown 解析 | md4c | 輕量、快速、C 函式庫 |
| 語法解析 | tree-sitter | 程式碼高亮與結構分析 |
| 資料庫 / 索引 | SQLite + FTS5 | 儲存連結、標籤、全文搜尋 |
| 檔案監看 | Qt `QFileSystemWatcher`（必要時換 efsw） | vault 檔案變動即時同步 |
| AI 整合 | `QProcess` 呼叫 Claude Code CLI；之後可加 HTTP 直連模型 API | 初期不自建 agent |
| 測試 | GoogleTest（或 Catch2） | 單元測試 |
| 靜態分析 | clang-tidy、clang-format | 統一風格、提早抓錯 |
| 授權 | Qt 以 LGPL 動態連結 | 可商用 |

---

## 4. 系統架構

```
┌──────────────────────────────────────────────┐
│                  UI 層（Qt）                   │
│  筆記編輯器 │ 圖譜/畫布視圖 │ 檔案樹 │ AI 面板  │
└───────────────┬──────────────────────────────┘
                │ signals / slots
┌───────────────▼──────────────────────────────┐
│                  核心層（純 C++，不依賴 UI）     │
│  Vault 管理 │ Markdown 解析 │ 連結圖 │ 搜尋      │
│  Context 組裝器（把筆記整理成給 AI 的脈絡）       │
└───────────────┬──────────────────────────────┘
                │
┌───────────────▼──────────────────────────────┐
│                  整合層                        │
│  SQLite 索引 │ 檔案監看 │ AI Agent 橋接（CLI）   │
└──────────────────────────────────────────────┘
```

**重要原則**：核心層不得依賴 Qt Widgets，只能使用標準函式庫與必要的 QtCore，以便測試與未來替換 UI。

---

## 5. 目錄結構

```
nexus/
├── CMakeLists.txt
├── CMakePresets.json        # debug / release / asan 預設
├── vcpkg.json
├── .clang-format
├── .clang-tidy
├── CLAUDE.md                # 給 AI 的開發規範（見第 8 節）
├── PLAN.md                  # 本文件
├── docs/
│   └── decisions/           # 架構決策紀錄（ADR）
├── src/
│   ├── app/                 # main、應用程式啟動
│   ├── core/                # vault、筆記模型、連結圖、搜尋
│   ├── parser/              # md4c / tree-sitter 包裝
│   ├── storage/             # SQLite 索引
│   ├── ai/                  # Context 組裝器、Agent 橋接
│   └── ui/                  # 所有 Qt Widgets
├── tests/
│   ├── core/
│   ├── parser/
│   └── storage/
└── .github/workflows/       # CI：Windows + Linux
```

---

## 6. 開發階段（Milestones）

### M0：專案骨架（✅ 2026-10-25 完成）
- [x] CMake + vcpkg + Qt 6 可在 Windows 與 Linux 編譯
- [x] CMakePresets：`debug`、`release`、`asan`
- [x] 嚴格警告：GCC/Clang `-Wall -Wextra -Wpedantic -Werror`；MSVC `/W4 /WX`
- [x] clang-format、clang-tidy 設定
- [x] GoogleTest 接好，至少一個測試通過
- [x] GitHub Actions 在兩個平台跑建置 + 測試
- [x] 空白主視窗可啟動

**完成標準**：兩平台 CI 皆綠燈。

### M1：Vault 與筆記
- [ ] 開啟資料夾作為 vault，讀取所有 `.md` 檔
- [ ] 檔案樹顯示
- [ ] 基本 Markdown 編輯器（純文字 + 語法高亮）
- [ ] 儲存、新增、重新命名、刪除筆記
- [ ] 檔案監看：外部修改即時反映

**完成標準**：可以把 Nexus 當成最陽春的 Markdown 編輯器使用。

### M2：連結與索引
- [ ] 解析 `[[雙向連結]]` 與 `#標籤`
- [ ] SQLite 儲存筆記、連結、標籤
- [ ] 反向連結（backlinks）面板
- [ ] FTS5 全文搜尋
- [ ] 重新命名筆記時自動更新所有連結

**完成標準**：1 萬篇筆記的 vault，開啟索引 < 2 秒，搜尋 < 50ms。

### M3：圖譜 / 畫布視圖
- [ ] 以 `QGraphicsView` 繪製筆記關係圖
- [ ] 力導向佈局（force-directed layout）
- [ ] 縮放、拖曳、點擊開啟筆記
- [ ] 篩選（依標籤、依深度）

**完成標準**：1000 個節點仍維持 60 FPS。

### M4：AI 整合（核心差異化）
- [ ] 設定「專案程式碼資料夾」與 vault 的對應
- [ ] 在圖譜或列表中多選筆記
- [ ] Context 組裝器：把選取筆記（含連結的一層鄰居，可設定深度）整理成結構化提示
- [ ] 透過 `QProcess` 呼叫 Claude Code CLI，傳入提示與專案路徑
- [ ] AI 面板即時顯示 agent 輸出（串流）
- [ ] 執行完成後顯示變更的檔案清單

**完成標準**：能用「選三張規格筆記 → 按一個按鈕」讓 AI 實作出一個功能。

### M5：程式碼 ↔ 知識雙向連結
- [ ] tree-sitter 解析專案程式碼，抽出模組 / 類別 / 函式
- [ ] 程式碼符號以節點形式出現在圖譜中
- [ ] 筆記可連結到程式碼符號（例如 `[[code:src/core/Vault.cpp#Vault::open]]`）
- [ ] AI 修改程式碼後，自動提示哪些筆記可能需要更新

**完成標準**：從任一規格筆記能看到實作它的程式碼，反之亦然。

### 之後再考慮
- 內建程式碼編輯器（初期使用者繼續用 VS Code 看程式碼即可）
- 外掛系統
- 多 agent / 多模型支援
- 同步與協作

---

## 7. 效能目標

| 指標 | 目標 |
|---|---|
| 冷啟動 | < 500 ms |
| 開啟 1 萬篇筆記的 vault | < 2 s |
| 全文搜尋 | < 50 ms |
| 輸入延遲 | < 16 ms |
| 圖譜 1000 節點 | 60 FPS |
| 閒置記憶體 | < 150 MB |

規則：任何可能耗時的操作（索引、解析、AI 呼叫）一律不得在 UI 執行緒執行。

---

## 8. 給 AI 的開發規範（複製到 `CLAUDE.md`）

> 已建立 `CLAUDE.md`，之後以該檔為準（已依 ADR 0001、0002 修訂）。下方為初版內容，僅供參考。

```markdown
# CLAUDE.md

## 專案
Nexus：知識圖驅動的 AI 開發環境。C++20 + Qt 6，目標平台 Windows 與 Linux。
完整計畫見 PLAN.md，依 Milestone 順序開發，一次只做一個 Milestone。

## 建置與測試
- 設定：cmake --preset debug
- 建置：cmake --build --preset debug
- 測試：ctest --preset debug
- 記憶體檢查：cmake --preset asan && cmake --build --preset asan && ctest --preset asan
- 每次修改後都必須建置並跑測試，全部通過才算完成。

## 程式碼規範
- C++20。不使用例外以外的錯誤處理捷徑；可失敗的函式回傳 std::expected 或明確的結果型別。
- 禁止裸 new / delete。一律使用 std::unique_ptr、std::shared_ptr 或 Qt 父子物件所有權。
- 遵循 RAII，資源（檔案、資料庫連線、行程）由物件生命週期管理。
- src/core、src/parser、src/storage 不得 include 任何 Qt Widgets 標頭。
- UI 執行緒不得執行 I/O、解析或 AI 呼叫；使用 QThreadPool / QtConcurrent，並以 signal 回傳結果。
- 所有程式碼必須通過 clang-format 與 clang-tidy，零警告。
- 新功能必須附單元測試；修 bug 先寫重現該 bug 的測試。

## 命名
- 類別：PascalCase（VaultManager）
- 函式與變數：camelCase（openVault）
- 成員變數：m_ 前綴（m_notes）
- 檔案：與類別同名（VaultManager.h / VaultManager.cpp）

## 工作方式
- 動手前先說明計畫：會新增或修改哪些檔案。
- 遇到架構層級的決定，先停下來詢問，不要自行決定。
- 做出的重要決定記錄到 docs/decisions/，格式：背景、選項、決定、理由。
- 若 sanitizer 回報錯誤，優先修正，不得忽略或抑制。
```

---

## 9. 除錯與品質策略

C++ 最大的風險是記憶體錯誤與未定義行為，且不易從症狀推回原因。對策是讓錯誤**自動、立即、清楚地**出現：

1. **AddressSanitizer + UndefinedBehaviorSanitizer**：`asan` preset 每次開發都跑（Linux 用 GCC/Clang；Windows 用 MSVC 的 `/fsanitize=address`）。
2. **嚴格警告 + `-Werror`**：警告即錯誤。
3. **clang-tidy**：啟用 `bugprone-*`、`modernize-*`、`performance-*`、`cppcoreguidelines-*`（依實際情況排除過嚴項目）。
4. **測試先行**：核心層邏輯全部可單元測試。
5. **CI 雙平台**：每次 push 在 Windows 與 Linux 建置並測試。

作者角色：撰寫規格、審核 AI 的計畫、閱讀測試與 sanitizer 報告，而非逐行審程式碼。

---

## 10. 第一步

在 VS Code 中對 Claude 說：

> 請閱讀 PLAN.md，先建立 CLAUDE.md（內容照第 8 節），然後開始 M0。動手前先列出你要建立的檔案。
