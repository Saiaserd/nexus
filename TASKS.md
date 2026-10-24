# TASKS.md：任務卡

> 規則見 WORKFLOW.md。PM 負責新增與調整任務；開發者只改自己任務的「狀態」與「備註」。
> 狀態：`待辦` / `進行中` / `待審核` / `已完成` / `阻塞`

---

## 目前 Milestone：M0 專案骨架

M0 全部由 **開發 A** 依序完成。建議順序：T-001 → T-002 → **T-007** → T-004 → T-006 → T-003 → T-005。

開工前必讀：`docs/decisions/0001`（Qt 不走 vcpkg、Windows 工具鏈）、`0002`（`nexus::Expected`）、`0003`（獨立 vcpkg）。

---

### T-001 建立 CMake + vcpkg + Qt 6 基本專案
- **負責**：A
- **狀態**：已完成
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
- **備註**：（A）已完成，Windows 本機驗證通過（乾淨 PowerShell 只用 `scripts/dev.ps1` 完成 configure + build + test；`nexus.exe` 在不含 Qt 的 PATH 下可執行，exit 0）。
  - **Qt DLL 方案**：選 `windeployqt`（build 後自動部署，`cmake/NexusQtDeploy.cmake` 的 `nexus_deploy_qt(target)`），未用 PATH。T-004 的測試執行檔也請呼叫此函式。dev.ps1 另外會把 Qt bin 加進該 process 的 PATH，僅作保險。
  - **超出卡片清單的檔案**：`.gitattributes`（PM 指示）、`README.md`（建置指令，取代改 CLAUDE.md）、`cmake/NexusQtDeploy.cmake`、`CMakePresets.json`（最小版，只含 `debug`；因 `dev.ps1 configure debug` 需要 preset，T-002 會擴充 release / asan 並補 test preset 細節）。
  - **vcpkg**：`builtin-baseline` = `c82f74667287d3dc386bce81e44964370c91a289`（tag 2024.09.30，與 VS 內建 vcpkg 2024-09-30 對應）。解析版本：gtest 1.15.2、md4c 0.5.2、sqlite3 3.46.1（含 fts5）、tl-expected 1.1.0、tree-sitter 0.20.6。
  - **Triplet**：Windows 預設 `x64-windows-static-md`（第三方靜態、CRT 動態 /MD，與 Qt 一致且利於 MSVC ASan），在根 CMakeLists 的 `project()` 前設定，可用 `-DVCPKG_TARGET_TRIPLET` 覆寫。此為 vcpkg 的 community triplet。Linux 用 vcpkg 預設 triplet。
  - **踩到的坑（已處理）**：VS 內建 vcpkg 的腳本會去 MSYS2 下載已被移除（404）的 pkgconf 2.2.0，導致 gtest 建置失敗。`dev.ps1` 會自動（一次性）從 repo.msys2.org 下載 pkgconf 到 `%LOCALAPPDATA%\nexus\tools\pkgconf`，並以 `PKG_CONFIG` + `VCPKG_KEEP_ENV_VARS` 交給 vcpkg。若 PM 想改用獨立 clone 的新版 vcpkg（可免此 workaround），需另立 ADR（ADR 0001 已預留）。CI 的 windows-latest 內建 vcpkg 較新，預期不受影響。
  - **其他 dev.ps1 設定**：`VCPKG_DOWNLOADS` 設在 `%LOCALAPPDATA%\vcpkg\downloads`（VS 內建 vcpkg 位於 Program Files，無法寫入）；關閉 vcpkg metrics。
  - **Linux 尚未驗證，等 T-005 CI**：CMake 邏輯為跨平台寫法（`WIN32` 才套用 triplet / windeployqt；Qt 由 `QT_ROOT` → `CMAKE_PREFIX_PATH`）。
  - 目前 `main.cpp` 只用 QtCore（`QCoreApplication`），所以 windeployqt 只部署 Qt6Core；Widgets 雖已 link 但尚未被引用，T-006 之後會自動部署。

---

### T-002 CMakePresets 與嚴格編譯警告
- **負責**：A
- **狀態**：已完成
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
- **備註**：（A）已完成，Windows 本機驗證通過；`debug`、`release`、`asan` 三個 preset 皆 configure + build + test 成功（目前尚無測試，ctest 回報 "No tests were found"，T-004 才會有）。
  - **/WX 驗證**：故意加入 `int unusedVar = 42;`，debug 建置出現 `C4189` + `C2220`（警告視為錯誤）並失敗；已還原、未 commit。
  - **ASan 驗證**：故意在 main 寫 `new int[4]` 越界，asan 版執行得到 `AddressSanitizer: heap-buffer-overflow` 並以 exit 1 結束；已還原、未 commit。確認 asan 編譯旗標含 `/fsanitize=address` 且不含 `/RTC1`，連結旗標含 `/INCREMENTAL:NO`。
  - **警告範圍**：`nexus_set_warnings(target)` 以 PRIVATE 套用，不影響 vcpkg 套件；Qt 標頭為 IMPORTED target 的 SYSTEM include（實測編譯列為 `-external:I...`），MSVC 額外加 `/external:W0`。`nexus_enable_sanitizers(target)` 同樣需對每個 target 呼叫（T-004 的 core library、測試執行檔也要呼叫這兩個函式，以及測試執行檔要呼叫 `nexus_deploy_qt`）。
  - **MSVC ASan 執行階段**：Debug（/MDd）實際匯入的是 `clang_rt.asan_dynamic-x86_64.dll`（非 `_dbg_`）；Sanitizers.cmake 會把兩個 DLL（`asan_dynamic`、`asan_dbg_dynamic`）從 cl.exe 所在目錄複製到可執行檔旁，因此不依賴 PATH。
  - **Preset 變更**：`VCPKG_INSTALLED_DIR` 設為 `build/vcpkg_installed`，三個 preset 共用同一份 vcpkg 安裝，避免重複建置相依；binaryDir 為 `build/<preset>`。asan 的 test preset 設 `UBSAN_OPTIONS`（僅 Linux 有效；Windows 無 UBSan）。
  - **風險／未驗證**：(1) **Linux 尚未驗證、等 T-005 CI**：GCC/Clang 旗標、`-fsanitize=address,undefined`、`-fno-sanitize-recover=all`、Linux 下 Qt 的 SYSTEM include 皆未實測。(2) MSVC 混用「有 ASan」與「無 ASan」的靜態函式庫（vcpkg 的 gtest 等）可能在 T-004 遇到 `annotate_vector` 的 LNK2038 不一致或 ODR 類問題，屆時視情況處理。(3) Windows 上未啟用 LeakSanitizer（MSVC 不支援）。

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

### T-007 改用獨立 vcpkg，移除自行下載 pkgconf
- **負責**：A
- **狀態**：待辦
- **依賴**：T-002
- **目標**：依 ADR 0003，以釘死版本的獨立 vcpkg 取代 VS 內建 vcpkg
- **檔案**：
  - `scripts/dev.ps1`（clone、checkout、bootstrap vcpkg；刪除 `Ensure-PkgConfig`）
  - `vcpkg.json`（`builtin-baseline` 改成近期穩定 release tag 的 commit，於備註寫明 tag 名稱）
  - `README.md`（更新建置說明）
- **完成標準**：
  - 刪掉 `%LOCALAPPDATA%
exus` 與 `build/` 後，在乾淨的 PowerShell 執行 `./scripts/dev.ps1 all debug` 一次成功
  - 腳本裡不再有任何沒驗證雜湊的二進位下載
  - `debug`、`release`、`asan` 三個 preset 都建置成功
  - 備註列出升級後各套件的實際版本（尤其 tree-sitter）
- **備註**：（PM）審核 T-001 時提出，見 ADR 0003。

---

## M0 完成檢查（PM 執行）
- [ ] T-001 ~ T-006 全部「已完成」
- [ ] CI 在兩平台皆綠燈
- [ ] PLAN.md 中 M0 項目打勾
- [ ] 拆出 M1 任務卡
- [ ] M1 的 UI 畫面（檔案樹、編輯器）設計稿經作者確認
