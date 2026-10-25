# CLAUDE.md

## 專案
Nexus：知識圖驅動的 AI 開發環境。C++20 + Qt 6，目標平台 Windows 與 Linux。
完整計畫見 PLAN.md，依 Milestone 順序開發，一次只做一個 Milestone。
協作流程見 WORKFLOW.md，任務見 TASKS.md，架構決策見 docs/decisions/。

## 建置與測試
- 設定：cmake --preset debug
- 建置：cmake --build --preset debug
- 測試：ctest --preset debug
- 記憶體檢查：cmake --preset asan && cmake --build --preset asan && ctest --preset asan
- 每次修改後都必須建置並跑測試，全部通過才算完成。

### Windows 本機環境（見 ADR 0001、0003）
- 一律透過 `scripts/dev.ps1`，不要手動設定 PATH：
  - `./scripts/dev.ps1 all <preset>`（configure + build + test），preset：`debug`、`release`、`asan`、`tidy`
  - `./scripts/dev.ps1 format` / `format-check`
- 腳本會載入 VS 2022 的 vcvars64，並使用 `%LOCALAPPDATA%
exuscpkg`（獨立 clone，釘在 `vcpkg.json` 的 `builtin-baseline`）。
  注意：vcvars64 會把 `VCPKG_ROOT` 指向 VS 內建的 vcpkg，腳本已處理，不要用 VS 內建的那個。
- Qt 6.8.3：`C:\Qt.8.3\msvc2022_64`（`QT_ROOT` 可覆寫）。
- clang-tidy / clang-format 要用 `VC\Tools\Llvmdin` 底下的 64 位元版；32 位元版的 clang-tidy 會當機。
- MSVC ASan 已關閉 STL 容器標註（`_DISABLE_VECTOR_ANNOTATION` / `_DISABLE_STRING_ANNOTATION`），因為 vcpkg 的套件不是用 ASan 建置的。洩漏偵測只在 Linux CI 上有。

## 程式碼規範
- C++20。可失敗的函式回傳 `nexus::Expected<T, E>`（`tl::expected` 的別名，見 ADR 0002）或明確的結果型別；不以例外做一般錯誤流程。
- 禁止裸 new / delete。一律使用 std::unique_ptr、std::shared_ptr 或 Qt 父子物件所有權。
- 遵循 RAII，資源（檔案、資料庫連線、行程）由物件生命週期管理。
- src/core、src/parser、src/storage 不得 include 任何 Qt Widgets 標頭（只允許標準函式庫與必要的 QtCore）。
- UI 執行緒不得執行 I/O、解析或 AI 呼叫；使用 QThreadPool / QtConcurrent，並以 signal 回傳結果。
- 所有程式碼必須通過 clang-format 與 clang-tidy，零警告。
- 新功能必須附單元測試；修 bug 先寫重現該 bug 的測試。

## 命名
- 類別：PascalCase（VaultManager）
- 函式與變數：camelCase（openVault）
- 成員變數：m_ 前綴（m_notes）
- 檔案：與類別同名（VaultManager.h / VaultManager.cpp）
- 命名空間：`nexus`（子模組可用 `nexus::core`、`nexus::storage` 等）

## UI
- 任何新畫面（M1 起的檔案樹、編輯器、圖譜、AI 面板…）必須先有作者確認過的設計稿，才能實作。沒有設計稿的 UI 任務不得開工。
- 主題：淺色・森林綠（預設）與深色・森林綠，於設定切換。

## 工作方式
- 動手前先說明計畫：會新增或修改哪些檔案。
- 遇到架構層級的決定，先停下來詢問，不要自行決定。
- 做出的重要決定記錄到 docs/decisions/，格式：背景、選項、決定、理由。
- 若 sanitizer 回報錯誤，優先修正，不得忽略或抑制。
- 所有對作者的回覆與文件一律使用繁體中文；程式碼識別字與註解可用英文。
