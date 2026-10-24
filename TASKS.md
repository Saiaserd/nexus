# TASKS.md：任務卡

> 規則見 WORKFLOW.md。PM 負責新增與調整任務；開發者只改自己任務的「狀態」與「備註」。
> 狀態：`待辦` / `進行中` / `待審核` / `已完成` / `阻塞`

---

## 目前 Milestone：M0 專案骨架

M0 全部由 **開發 A** 依序完成。建議順序：T-001 → T-002 → T-004 → T-006 → T-003 → T-005。

開工前必讀：`docs/decisions/0001`（Qt 不走 vcpkg、Windows 工具鏈）、`0002`（`nexus::Expected`）。

---

### T-001 建立 CMake + vcpkg + Qt 6 基本專案
- **負責**：A
- **狀態**：待辦
- **依賴**：無
- **目標**：可在 Windows 與 Linux 建置一個連結 Qt 6 的空執行檔
- **檔案**：
  - `CMakeLists.txt`（根目錄）
  - `vcpkg.json`（相依：sqlite3[fts5]、md4c、tree-sitter、gtest、tl-expected；**不含 Qt**，見 ADR 0001；需設 `builtin-baseline`）
  - `src/app/main.cpp`
  - `src/app/CMakeLists.txt`
  - `.gitignore`
  - `scripts/dev.ps1`（Windows：載入 vcvars64、設定 `VCPKG_ROOT` 與 Qt 路徑後轉呼叫 cmake / ctest，例如 `scripts/dev.ps1 configure debug`）
- **完成標準**：
  - Windows 本機：在乾淨的 PowerShell 中只靠 `scripts/dev.ps1` 即可設定並建置成功
  - Linux：CMake 設定可透過 `CMAKE_PREFIX_PATH` 找到 Qt 6.8.3（本機無法驗證，於 T-005 CI 驗證）
  - 以 `find_package(Qt6 6.8 REQUIRED COMPONENTS Core Widgets)` 連結 Qt
  - 執行檔可啟動並立即結束（尚不需視窗）；Windows 上執行時找得到 Qt DLL（建置後自動部署或設定 PATH，二擇一並在備註說明）
  - 根目錄 README 區塊或 CLAUDE.md 中的建置指令與實際一致
- **備註**：

---

### T-002 CMakePresets 與嚴格編譯警告
- **負責**：A
- **狀態**：待辦
- **依賴**：T-001
- **目標**：建立 `debug`、`release`、`asan` 三個 preset，並統一開啟嚴格警告
- **檔案**：
  - `CMakePresets.json`（含 configure / build / test presets）
  - `cmake/CompilerWarnings.cmake`
  - `cmake/Sanitizers.cmake`
- **完成標準**：
  - GCC/Clang：`-Wall -Wextra -Wpedantic -Werror`
  - MSVC：`/W4 /WX /permissive-`
  - `asan` preset：Linux 開 ASan + UBSan；Windows MSVC 開 `/fsanitize=address`（注意：MSVC ASan 需關閉 `/RTC*` 與增量連結，執行時需找得到 `clang_rt.asan_dynamic-x86_64.dll`）
  - 警告設定只套用在本專案目標，不影響第三方函式庫與 Qt 標頭（Qt 以 SYSTEM include 引入）
  - 三個 preset 在兩平台皆可建置
- **備註**：

---

### T-003 clang-format 與 clang-tidy
- **負責**：A
- **狀態**：待辦
- **依賴**：T-002
- **目標**：統一程式風格並啟用靜態分析
- **檔案**：
  - `.clang-format`（以 LLVM 為基礎，縮排 4 空格，行寬 100）
  - `.clang-tidy`（啟用 bugprone-*、modernize-*、performance-*、cppcoreguidelines-*，排除過嚴且與 Qt 衝突的規則，並在檔案內註解排除理由）
  - CMake 選項 `NEXUS_ENABLE_CLANG_TIDY`（預設 OFF，CI 開啟）
- **完成標準**：
  - 現有程式碼通過 clang-format 檢查
  - 開啟 clang-tidy 建置時零警告
- **備註**：

---

### T-004 接入 GoogleTest
- **負責**：A
- **狀態**：待辦
- **依賴**：T-002
- **目標**：建立測試框架與第一個測試
- **檔案**：
  - `tests/CMakeLists.txt`
  - `tests/core/SmokeTest.cpp`
  - 建立 `src/core/` 函式庫目標（暫時只含一個簡單函式供測試）
  - `src/core/Expected.h`（`nexus::Expected` 別名，見 ADR 0002），並在 SmokeTest 中測一個回傳 `Expected` 的函式
- **完成標準**：
  - `ctest --preset debug` 與 `ctest --preset asan` 皆通過
  - `src/core` 為獨立 library 目標，不連結 Qt Widgets
- **備註**：

---

### T-005 GitHub Actions CI（Windows + Linux）
- **負責**：A
- **狀態**：待辦
- **依賴**：T-003、T-004
- **目標**：每次 push 與 PR 自動建置與測試
- **檔案**：
  - `.github/workflows/ci.yml`
- **完成標準**：
  - 矩陣：`ubuntu-latest`（GCC 與 Clang）、`windows-latest`（MSVC）
  - Qt 6.8.3 以 `jurplel/install-qt-action` 安裝（含快取），其餘相依走 vcpkg（見 ADR 0001）
  - 每個平台跑：debug 建置 + 測試、asan 建置 + 測試
  - Linux 其中一組開啟 clang-tidy 與 clang-format 檢查
  - 啟用 vcpkg binary cache 縮短建置時間
- **備註**：（PM）需要作者先建立 GitHub repo 並設定 remote；在那之前此卡只能寫好 yml、無法驗證綠燈。

---

### T-006 空白主視窗
- **負責**：A
- **狀態**：待辦
- **依賴**：T-001
- **目標**：啟動後顯示 Nexus 主視窗
- **檔案**：
  - `src/ui/MainWindow.h`、`src/ui/MainWindow.cpp`
  - `src/ui/CMakeLists.txt`
  - 修改 `src/app/main.cpp`
- **完成標準**：
  - 視窗標題「Nexus」，預設 1280×800
  - 有空的選單列（檔案、編輯、檢視、說明）與狀態列
  - 關閉視窗正常結束，asan 無錯誤或洩漏回報
  - 只做系統原生外觀的空殼；**不做任何主題或自訂樣式**（UI 設計稿確認前不得加，見 CLAUDE.md「UI」）
- **備註**：

---

## M0 完成檢查（PM 執行）
- [ ] T-001 ~ T-006 全部「已完成」
- [ ] CI 在兩平台皆綠燈
- [ ] PLAN.md 中 M0 項目打勾
- [ ] 拆出 M1 任務卡
- [ ] M1 的 UI 畫面（檔案樹、編輯器）設計稿經作者確認
