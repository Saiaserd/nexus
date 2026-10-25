# TASKS.md：任務卡

> 規則見 WORKFLOW.md。PM 負責新增與調整任務；開發者只改自己任務的「狀態」與「備註」。
> 狀態：`待辦` / `進行中` / `待審核` / `已完成` / `阻塞`

---

## M0 專案骨架（✅ 2026-10-25 完成）

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
- **狀態**：已完成
- **依賴**：T-002
- **目標**：統一程式風格並啟用靜態分析
- **檔案**：
  - `.clang-format`（以 LLVM 為基礎，縮排 4 空格，行寬 100）
  - `.clang-tidy`（啟用 bugprone-*、modernize-*、performance-*、cppcoreguidelines-*，排除過嚴且與 Qt 衝突的規則，並在檔案內註解排除理由）
  - CMake 選項 `NEXUS_ENABLE_CLANG_TIDY`（預設 OFF，CI 開啟）
- **完成標準**：
  - 現有程式碼通過 clang-format 檢查
  - 開啟 clang-tidy 建置時零警告
- **備註**：（A）已完成。
  - 新增：`.clang-format`（LLVM 為底、4 空格、行寬 100，函式大括號換行、其餘附著）、`.clang-tidy`（bugprone/modernize/performance/cppcoreguidelines；每條排除規則都在檔內註解理由；`WarningsAsErrors: '*'`；`HeaderFilterRegex` 只報我們 `src/`、`tests/` 的標頭）、`cmake/ClangTidy.cmake`（選項 `NEXUS_ENABLE_CLANG_TIDY`，預設 OFF；moc 產生碼不被檢查）。另加 `tidy` preset（configure/build/test，Debug + clang-tidy）以及 `dev.ps1` 子命令 `format`、`format-check`（皆用 git ls-files 取得 C++ 檔）。根 CMakeLists include `ClangTidy`；README 補充用法。CMakePresets.json 因用腳本重寫而整檔重排縮排（內容只多 `tidy`）。
  - 驗證：`format-check` 通過（故意放入未格式化檔案時會失敗並回傳非零，已移除）；`./scripts/dev.ps1 all tidy` 零警告、4 個測試通過；tidy 實際檢查 main.cpp、NumberParser.cpp、MainWindow.cpp、SmokeTest.cpp 四個來源。debug/release/asan 重新 `all` 皆 exit 0、4 測試過。
  - **踩到的坑**：VS 的 `Llvm\bin\clang-tidy.exe` 是 32 位元版，分析時直接 access violation 當機；必須用 `Llvm\x64\bin`。`dev.ps1` 預設 `NEXUS_LLVM_BIN` 指到 x64 路徑，CMake 的 find_program 也優先找該目錄。
  - 首次執行 tidy 抓到的兩個真問題已修正：`enum class ParseError` 改為 `: std::uint8_t`（performance-enum-size）；NumberParser.cpp 的指標運算改用 `std::to_address(text.end())`（pro-bounds-pointer-arithmetic）。
  - 只有排除清單內的規則被關閉（trailing-return-type、owning-memory、C 陣列、easily-swappable-parameters、magic-numbers、vararg、macro-usage、non-const-global），未使用任何 NOLINT。
  - clang-format 版本 18.1.8（VS 內建）；CI 的 Linux clang-format 版本須相近，否則格式可能有細微差異（T-005 需注意）。Linux 尚未驗證。

---

### T-004 接入 GoogleTest
- **負責**：A
- **狀態**：已完成
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
- **備註**：（A）已完成。`debug`、`release`、`asan` 的 `./scripts/dev.ps1 all <preset>` 皆 exit 0，4 個測試全過。
  - 新增：`src/core/`（`Expected.h`、`NumberParser.h/.cpp`、`CMakeLists.txt`，靜態庫 `nexus_core`，PUBLIC include 目錄為 `src/`，所以以 `#include "core/Expected.h"` 引用；目前完全不連結 Qt）、`tests/CMakeLists.txt`、`tests/core/SmokeTest.cpp`（測 `parsePositiveInt` 回傳 `nexus::Expected<int, ParseError>` 的成功與三種錯誤）。根 `CMakeLists.txt` 加入 `enable_testing()` 與 `add_subdirectory(src/core|tests)`。
  - 測試執行檔呼叫 `nexus_set_warnings`、`nexus_enable_sanitizers`，以 `gtest_discover_tests` 註冊（置於 sanitizer 步驟之後，確保 ASan DLL 已在 exe 旁）。測試未連結 Qt，故沒有呼叫 `nexus_deploy_qt`；之後若有測試連 Qt 需呼叫。
  - **MSVC ASan LNK2038 真的出現**（gtest.lib 與我們的物件 `annotate_vector` / `annotate_string` 不一致，LNK1319）。選擇方案：在 `cmake/Sanitizers.cmake` 的 MSVC 分支對我們自己的 target 加 `_DISABLE_VECTOR_ANNOTATION` 與 `_DISABLE_STRING_ANNOTATION`。理由：改動小、不用為 ASan 另建一套 vcpkg triplet／相依（建置時間與磁碟加倍、CI 快取也要加倍）。代價：我們程式碼內 `std::vector`／`std::string` 的 container-overflow 偵測失效（堆積／堆疊越界、use-after-free 仍會抓到）。若日後想要完整偵測，可改用 ASan 版 overlay triplet 重建 vcpkg 套件，屆時移除這兩個定義。
  - Linux 尚未驗證，等 T-005 CI（GCC/Clang 下 tl-expected、GTest 的 find_package 與 sanitizer 組合）。

---

### T-005 GitHub Actions CI（Windows + Linux）
- **負責**：C
- **狀態**：已完成
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
- **備註**：（PM）GitHub remote 已設定：https://github.com/Saiaserd/nexus（public），PM 負責 push 與查看結果。
  - （PM，審核 T-003/T-004 時追加）一併處理：
    1. Linux 的 clang-format 版本要和本機 VS 內建的 18.1.8 對齊（例如用 apt 的 `clang-format-18`），否則格式檢查會因版本不同而失敗
    2. `cmake/ClangTidy.cmake` 的 `CMAKE_AUTOGEN_ORIGIN_DEPENDS OFF` 並不會略過 moc 產生碼；改成對 autogen 來源設定 `SKIP_LINTING`（CMake ≥ 3.27），或用其他確實有效的方式
    3. GUI 啟動煙霧測試：`nexus --quit-after-show`（或 `QT_QPA_PLATFORM=offscreen`）放進 ctest，Linux 上搭配 ASan/LSan，讓洩漏偵測能涵蓋主視窗
  - （C）已完成，dev-c 上 CI 全綠（run 40173058620：https://github.com/Saiaserd/nexus/actions/runs/40173058620）。
    - 架構：`format` job（pip 安裝 `clang-format==18.1.8`，與本機同版）＋ `build` 矩陣 linux-gcc／linux-clang／windows-msvc；每組 debug 與 asan 的建置＋測試；linux-clang 另跑 clang-tidy 18（tidy preset，零警告）。Qt 用 `jurplel/install-qt-action@v4`（cache: true），vcpkg 自行 clone 並釘在 builtin-baseline commit，binary cache 為 `files` provider + `actions/cache`（x-gha 已被 GitHub 汰除）。Windows 用 `ilammy/msvc-dev-cmd`，並在其後覆寫 `VCPKG_ROOT`、移除 Git 的 `link.exe`。permissions: contents: read、concurrency 取消舊 run。失敗時 `.github/scripts/run-logged.sh` 輸出 `::error::` annotation 與 step summary。
    - PM 追加三點：(1) clang-format 版本對齊 18.1.8；(2) `cmake/ClangTidy.cmake` 改在 build 根目錄產生極簡 `.clang-tidy`（Checks 實質關閉），使 autogen／moc 產生碼不被 lint（不需 CMake 3.27）；(3) `nexus --quit-after-show` 加進 ctest（`nexus_gui_smoke`，非 Windows 設 `QT_QPA_PLATFORM=offscreen`），Linux ASan job 以 `detect_leaks=1` 涵蓋主視窗，已通過。
    - 發現並修正：新版 MSVC STL 在 ASan 下需 `_DISABLE_OPTIONAL_ANNOTATION`（`cmake/Sanitizers.cmake`），否則 gtest 連結出現 LNK2038。Linux 上 src/ 程式碼無需任何修改。
    - 風險：Node.js 20 action 棄用警告（actions 仍可用）；`ubuntu-latest` 即將遷移到 Ubuntu 26，clang-18／clang-tidy-18 套件屆時可能需調整。

---

### T-006 空白主視窗
- **負責**：A
- **狀態**：已完成
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
- **備註**：（A）已完成。
  - 新增 `src/ui/`（`MainWindow.h/.cpp`、`CMakeLists.txt`，靜態庫 `nexus_ui`，連結 Qt6::Widgets），`main.cpp` 改為 `QApplication` + `MainWindow`，`src/app` 連結 `nexus_ui`；根 CMakeLists 加入 `add_subdirectory(src/ui)`。
  - `MainWindow`：標題「Nexus」、1280×800、選單列「檔案(&F)（內含「結束」，Ctrl+Q）／編輯／檢視／說明」皆為空選單、狀態列。純系統原生外觀，沒有任何樣式表或主題。選單與狀態列皆為視窗的子物件，無裸 new。
  - 驗證：`./scripts/dev.ps1 all asan` exit 0（4 個測試過）。手動執行 asan 版 `nexus.exe`：視窗標題為「Nexus」，送出關閉訊息後 **exit code 0，stderr 為空（無 ASan 報告）**。windeployqt 已部署 Qt6Core/Gui/Widgets 等與平台外掛。
  - 沒有把 GUI 放進 ctest（避免卡在視窗、Linux CI 需 offscreen 平台）；若日後要自動化啟動測試，建議另加 `--quit-after-show` 之類的參數，或以 QT_QPA_PLATFORM=offscreen 並在 windeployqt 加 `--plugindir` 部署 offscreen 外掛。
  - Windows 的 MSVC ASan 沒有 LeakSanitizer，洩漏偵測只能靠 Linux CI（T-005）。Linux 尚未驗證。

---

### T-007 改用獨立 vcpkg，移除自行下載 pkgconf
- **負責**：A
- **狀態**：已完成
- **依賴**：T-002
- **目標**：依 ADR 0003，以釘死版本的獨立 vcpkg 取代 VS 內建 vcpkg
- **檔案**：
  - `scripts/dev.ps1`（clone、checkout、bootstrap vcpkg；刪除 `Ensure-PkgConfig`）
  - `vcpkg.json`（`builtin-baseline` 改成近期穩定 release tag 的 commit，於備註寫明 tag 名稱）
  - `README.md`（更新建置說明）
- **完成標準**：
  - 刪掉 `%LOCALAPPDATA%\nexus` 與 `build/` 後，在乾淨的 PowerShell 執行 `./scripts/dev.ps1 all debug` 一次成功
  - 腳本裡不再有任何沒驗證雜湊的二進位下載
  - `debug`、`release`、`asan` 三個 preset 都建置成功
  - 備註列出升級後各套件的實際版本（尤其 tree-sitter）
- **備註**：（A）已完成。
  - `builtin-baseline` = `9e593bb18ea69cc5095e012465dcd675a822ed0d`，對應 vcpkg release tag **2026.07.29**（注意該 tag 是 annotated tag，`git ls-remote` 的 `refs/tags/2026.07.29` 是 tag 物件、commit 要看 `^{}` 那行）。
  - 升級後實際版本：gtest 1.17.0#3、md4c 0.5.3、sqlite3 3.53.4（core,fts5,json1）、tl-expected 1.3.1、**tree-sitter 0.26.11**。
  - `scripts/dev.ps1`：新增 `Initialize-Vcpkg`（clone 到 `%LOCALAPPDATA%\nexus\vcpkg`、checkout baseline、bootstrap，以 `.nexus-bootstrapped` 戳記判斷 baseline 變更時重 bootstrap）；刪除 `Ensure-PkgConfig`、`PKG_CONFIG`、`VCPKG_KEEP_ENV_VARS`、`VCPKG_DOWNLOADS` 設定。腳本中不再有任何自行下載的二進位，下載皆由 vcpkg 驗證 SHA512。
  - 注意：`vcvars64.bat` 本身會匯出 VCPKG_ROOT 指向 VS 內建 vcpkg，所以腳本在匯入 VS 環境前先記下呼叫者的 `VCPKG_ROOT`；只有它不在 VS 目錄下才當作自備 vcpkg 使用，否則用受管理的 clone。
  - 驗收：刪除 `build/`、舊的 `%LOCALAPPDATA%\vcpkg`（含 pkgconf 快取 `%LOCALAPPDATA%\nexus\tools`）、vcpkg clone 內的 downloads/buildtrees/packages/vcpkg.exe 與 bootstrap 戳記後（clone 本身沿用以省流量，bootstrap 與相依從頭建），`./scripts/dev.ps1 all debug|release|asan` 皆 exit 0。

---

## M0 完成檢查（PM 執行）
- [x] T-001 ~ T-007 全部「已完成」
- [x] CI 在兩平台皆綠燈（run 40174081264）
- [x] PLAN.md 中 M0 項目打勾
- [x] 拆出 M1 任務卡
- [x] M1 的 UI 畫面（檔案樹、編輯器）設計稿經作者確認

---
---

## 下一個 Milestone：M1 Vault 與筆記

> 2026-10-25 PM 決定提早平行開發：**A** 做 T-101、T-102、T-103（src/core），**B** 做 T-104（src/parser），**C** 做 M0 的 T-005（CI）。UI 卡等設計稿確認後再分配。
> 共用檔案 `tests/CMakeLists.txt`、根目錄 `CMakeLists.txt` 只能各自加入自己的那幾行，衝突由 PM 在合併時處理。
> 開工前必讀：ADR 0004（核心層路徑與檔案 I/O）。
> 順序：核心 T-101 → T-102 → T-103（A）；UI T-110 → T-111 → T-112 → T-113 → T-114（D）。設計稿已於 2026-10-25 經作者確認。

### T-101 Vault 開啟與掃描（核心）
- **負責**：A
- **狀態**：已完成
- **依賴**：M0
- **目標**：把一個資料夾開啟為 vault，取得所有筆記與資料夾的結構
- **檔案**：
  - `src/core/FileError.h`（錯誤列舉 + 路徑，ADR 0004）
  - `src/core/Vault.h/.cpp`（`static Expected<Vault, FileError> open(const QString& root)`；`root()`、`notes()`、`folders()`、`absolutePath(rel)`、`contains(rel)`）
  - `src/core/NoteInfo.h`（相對路徑、檔案大小、修改時間）
  - `tests/core/VaultTest.cpp`
- **完成標準**：
  - 遞迴找出所有 `.md`（副檔名不分大小寫）；空資料夾也要列出（UI 要顯示）
  - 略過以 `.` 開頭的資料夾與檔案（`.git`、`.nexus`、`.obsidian`…）；不跟隨指向資料夾的符號連結（避免迴圈）
  - 根目錄不存在、不是資料夾、無法讀取時，回傳對應的錯誤
  - 結果依路徑排序（資料夾在前、同層依名稱的自然排序，例如「筆記2」排在「筆記10」前面），排序規則寫成獨立函式並測試
  - 測試用 `QTemporaryDir` 建立 fixture，包含中文檔名、空格、巢狀資料夾和隱藏資料夾
  - 效能測試：產生 10,000 篇筆記，`Vault::open` 在 release 下 < 1 秒（測試加上 `perf` label，ctest 預設照跑；asan 下只檢查正確性、不檢查時間）
- **備註**：（開發 A）
  - 新增 `FileError.h`（`FileErrorKind` 列舉含 NotFound/NotADirectory/AccessDenied/AlreadyExists/InvalidPath/InvalidName/InvalidEncoding/TrashFailed/IoError…，另有 `NameError`；T-102 共用）、`NoteInfo.h`、`PathOrder.h/.cpp`（`naturalLess`、`pathLess`、`isSafeRelPath`，排序規則獨立並有測試）、`Vault.h/.cpp`。
  - 排序採自己實作的自然排序（數字段比數值、不分大小寫、與語系無關），不用 `QCollator`，避免不同語系結果不同。資料夾排前面：`pathLess` 逐層比較，路徑中途的成分視為資料夾。
  - `Vault` 是純值型別（含 `QSet` 索引供 `contains`）；`absolutePath` 對不安全的相對路徑（`..`、絕對路徑、反斜線、`:`）回傳空字串。子資料夾無法讀取時略過，不讓整個 open 失敗。
  - `.lnk` 在 Windows 不是目錄符號連結，測試的符號連結案例在 Windows 上只驗證不出錯；Linux CI 會建立真正的迴圈連結。無法建立連結時測試自動 skip。
  - 效能測試套件命名為 `VaultPerf`，`tests/CMakeLists.txt` 以兩次 `gtest_discover_tests`（`TEST_FILTER`）讓名稱以 `Perf` 結尾的測試帶 `perf` label；時間斷言只在 Release 編譯（定義 `NEXUS_ENFORCE_PERF`）。Release 計時我沒有量測（debug 下含建檔約 5 秒）。
  - `nexus_core` 連結 `Qt6::Core`；測試執行檔也連結並呼叫 `nexus_deploy_qt`。
  - 測試 `UnreadableRootIsAccessDenied` 在 Windows 上 skip（無 POSIX 權限）。

---

### T-102 筆記檔案操作（核心）
- **負責**：A
- **狀態**：已完成
- **依賴**：T-101
- **目標**：讀、寫、新增、重新命名、刪除筆記與資料夾
- **檔案**：
  - `src/core/NoteDocument.h/.cpp`（`text`（`\n` 換行）、原始換行種類、有沒有 BOM）
  - `src/core/NoteFiles.h/.cpp`（以下函式都以 vault 根目錄 + 相對路徑操作）
    - `read(rel) -> Expected<NoteDocument, FileError>`
    - `write(rel, const NoteDocument&) -> Expected<void, FileError>`（QSaveFile）
    - `createNote(folderRel, baseName = "未命名") -> Expected<QString /*新的 rel*/, FileError>`（名稱重複時自動加「 1」「 2」…）
    - `createFolder(parentRel, name)`
    - `rename(rel, newBaseName) -> Expected<QString, FileError>`（筆記與資料夾都適用；筆記自動保留 `.md`）
    - `moveToTrash(rel)`
    - `validateName(name) -> std::optional<NameError>`（UI 的原地改名也要用這個做即時檢查）
  - `tests/core/NoteFilesTest.cpp`
- **完成標準**：
  - 讀寫都要保留原本的 CRLF/LF 與 BOM（寫回後與原檔逐 byte 相同）
  - 不是合法 UTF-8 的檔案回傳 `InvalidEncoding`，不能默默把內容弄壞
  - 名稱檢查包含：空字串、`\ / : * ? " < > |`、控制字元、結尾是空白或 `.`、Windows 保留名（CON、PRN、AUX、NUL、COM1–9、LPT1–9，不分大小寫，含副檔名的情況）、長度上限
  - 名稱衝突判斷在 Windows 與 macOS 不分大小寫，Linux 分大小寫；只改大小寫（`a.md` → `A.md`）要能成功
  - 每個錯誤情境都有測試
- **備註**：（開發 A）
  - 新增 NoteDocument.h/.cpp（text 一律 \n；LineEnding Lf/Crlf；hasBom；decode/encode 獨立成純函式，encode(decode(x)) 對單一換行種類的檔案逐 byte 相同）、NoteFiles.h/.cpp、tests/core/NoteFilesTest.cpp、tests/core/TestSupport.h（測試共用 fixture 輔助）。
  - NoteFiles 設計成「持有 vault 根目錄的類別」而非自由函式：NoteFiles files(root); files.read(rel)…。validateName 是獨立的自由函式（UI 即時檢查用，無副作用）。所有方法同步、需由 worker 執行緒呼叫，不可在 UI 執行緒用。
  - 介面變更（給 T-103）：NoteFiles::setWriteListener(std::function<void(const NoteInfo&)>)，write 成功後以新的 size 與 mtime 回呼；VaultWatcher 用它登記預期的變動。失敗的 write 不會回呼。
  - 混合換行（同檔既有 LF 又有 CRLF）：以多數種類為準，寫回時會統一成該種類，因此混合檔無法逐 byte 還原；單孤立 CR 保留原樣。這是刻意的取捨，若 PM 要求完整還原需要改成逐行記錄換行。
  - 名稱規則：長度上限 250 個 UTF-8 bytes（預留 .md）；檢查順序為 空 → 過長 → 控制字元 → 禁用字元 → 結尾空白或點 → 保留名（取第一個 . 之前的主檔名並略過結尾空白，所以 CON.txt、nul.tar.gz 都擋）。不擋開頭是 . 的名稱（規格沒列），但這種名稱建立後會被 Vault 掃描視為隱藏而看不到，UI 層可考慮自行提示，請 PM 決定是否要加進 validateName。
  - rename：筆記保留原副檔名（連 .MD 大小寫都保留）；資料夾不加副檔名。名稱衝突由檔案系統判斷（Windows/macOS 不分大小寫，Linux 分），只改大小寫時先改成暫時名稱 <name>.nexus-rename-N 再改成目標，失敗會盡力還原。rename 成同名是 no-op 成功。
  - createNote 用 QIODevice::NewOnly 建立，名稱重複時加「 1」「 2」…；模組內寫死預設名稱「未命名」是規格要求，屬資料而非錯誤文案。
  - moveToTrash 用 QFile::moveToTrash，同時適用資料夾；失敗回 TrashFailed，絕不改為永久刪除。測試在沒有資源回收筒的環境會 skip。
  - 本機 debug 與 asan 皆 48 個測試通過（1 skip：Windows 上的 AccessDenied 測試；資源回收筒測試有實際執行）；tidy 零警告。

---

### T-104 Markdown 行內標記解析（給語法高亮用）
- **負責**：B
- **狀態**：已完成
- **依賴**：M0
- **目標**：逐行把 Markdown 切成帶種類的區段，給 UI 的 `QSyntaxHighlighter` 上色（對照設計稿中編輯器的上色規則）
- **檔案**：
  - `src/parser/CMakeLists.txt`（新的 `nexus_parser` 函式庫，只連 QtCore）
  - `src/parser/MarkdownLexer.h/.cpp`：`LineResult lexLine(QStringView line, LexState prev)`，回傳 `std::vector<Span{int start; int length; SpanKind kind;}>` 和下一行的 `LexState`
  - `tests/parser/MarkdownLexerTest.cpp`
- **SpanKind**：`HeadingMarker`、`HeadingText`（含層級 1–6）、`ListMarker`（`-`、`*`、`+`、`1.`、任務 `- [ ]`/`- [x]`）、`QuoteMarker`、`StrongMarker`/`StrongText`、`EmphasisMarker`/`EmphasisText`、`InlineCode`、`FenceDelimiter`（含語言標籤）、`CodeBlockText`、`WikiLinkBracket`、`WikiLinkTarget`（`[[目標]]`、`[[目標|別名]]`、`[[目標#標題]]`）、`Tag`、`LinkText`/`LinkUrl`（`[文字](網址)`）、`HorizontalRule`
- **完成標準**：
  - 以 ``` 或 ~~~ 開頭的程式碼區塊跨行狀態正確（結束符號要同字元、長度不短於開頭）；區塊內不解析任何行內標記
  - 行內程式碼內不解析連結、標籤、粗體
  - 標籤規則：`#` 後接非空白；前一個字元不能是英數字（`a#b` 不算）；純數字不算（`#123`）；`#` 後面是空白則是標題或一般文字；中文標籤（`#規格`）要支援
  - 位置以 UTF-16 code unit 計算；測試要涵蓋中文和 emoji（代理對）
  - 至少 40 個測試案例；另用 1 萬行的混合文字做效能測試，release 下 < 50ms
  - 這不是完整的 CommonMark 解析器（M2 的連結索引用 md4c）；只求高亮正確，邊界情況照 Obsidian 的直覺行為
- **備註**：
  - Span 多了 `int level`（標題 1–6，其餘為 0）。額外新增的 SpanKind：`WikiLinkAlias`（`[[目標|別名]]` 的別名；`|` 算 `WikiLinkBracket`）、`LinkMarker`（`[`、`](`、`)`）。`WikiLinkTarget` 含 `#標題`（`[[筆記#段落]]` → `筆記#段落`）。
  - 輸出依 start 排序，巢狀時外層在前（例如標題內的 Tag、粗體內的連結），UI 依序套用格式即可讓內層覆蓋外層。span 長度必為正。
  - `LexState{inFence, fenceChar, fenceLength}`：`toInt()` 預設狀態為 0，圍欄為 `1 | tilde<<1 | 長度<<2`；`fromInt` 對 -1（QSyntaxHighlighter 的未設定值）與 0 都回傳預設。
  - 判斷：標籤前一字元不可為英數字、`#`、`/`、`&`、`_`（避免網址片段）；emoji 不擋標籤；標籤字元為字母/數字/`_`/`-`/`/`；`***x***` 同時產生 Strong 與 Emphasis；`_` 不處理單字內（`a_b_c`）；縮排 4 格以上不當標題、圍欄。程式碼區塊在引用（`>`）內不辨識。
  - 實作時 `emit` 是 Qt 巨集，內部函式改名 `addSpan`。
  - 測試 91 個（`nexus_parser_tests`）。效能：1 萬行 release 約 2 ms、debug 約 16 ms；僅在 `NDEBUG` 時檢查 < 50ms。
  - 對抗性輸入修正（PM 退回）：所有「往後找閉合」的搜尋（行內程式碼、強調、`[[ ]]`、`[文字](網址)` 的文字與網址）都有兩層上限，常數在 `MarkdownLexer.cpp`：`kMaxSearchDistance = 4096`（單次搜尋最多往後看 4096 個 UTF-16 單位，超過視為未閉合）、`kSearchBudgetPerUnit = 8` 與 `kSearchBudgetBase = 4096`（一行所有搜尋合計最多掃 8×行長+4096，用完後其餘搜尋直接當純文字）。單次上限不足以擋住「每個開頭各掃 4096」的情況，所以另加整行預算，兩者合起來保證 O(n)。正常行不會碰到上限（原 91 個測試不變，另加一個 3 萬字元正常長行仍可高亮到結尾的測試）。
  - 對抗性效能測試（`MarkdownLexerAdversarial`，單行 100,000 個 UTF-16 字元，release 下每種 < 20ms；debug/asan 只檢查 span 不變式）。release 實測毫秒：`*a` 1、`**a` 1、`_a ` 6、`*a ` 5、`[[` 4、`[` 4、`[a` 3、`](` 0、`[a](` 4、`[[a|` 3、`` ` `` 0、`` ` ``加空白 0、``` ``a` ``` 0、`#a ` 0、星號牆接文字 0、大量開頭接長星號串 5、標題前綴 5、清單前綴 4、隨機混合（seed 1/2/3）2/3/2。最壞約 6ms。總測試數 97。

---

### T-103 Vault 檔案監看（核心）
- **負責**：A
- **狀態**：已完成
- **依賴**：T-102
- **目標**：外部新增、修改、刪除、改名筆記時發出 signal；Nexus 自己存檔造成的變動不算
- **檔案**：
  - `src/core/VaultWatcher.h/.cpp`（`QObject`；以 `QFileSystemWatcher` 監看所有資料夾，加上 debounce；signal：`notesAdded(QStringList)`、`notesRemoved(QStringList)`、`notesModified(QStringList)`、`foldersChanged()`）
  - `NoteFiles` 寫檔後通知 watcher 忽略這次變動（例如登記「預期的 mtime 與大小」或內容雜湊）。若要改 NoteFiles 的介面，在備註說明
  - `tests/core/VaultWatcherTest.cpp`（`QSignalSpy`；測試執行檔需要 `QCoreApplication`）
- **完成標準**：
  - debounce 預設 150ms，同一批變動合成一次 signal
  - 外部新增、修改、刪除筆記，以及在外部新增資料夾後再於裡面新增筆記，都能正確偵測
  - Nexus 自己 `write` 的檔案不會觸發 `notesModified`
  - 改名在 signal 層呈現為「一個 removed 加一個 added」即可（UI 自行配對）
  - 測試不能不穩定（不靠固定 sleep；用 `QSignalSpy::wait` 並設上限）。連續跑 20 次都要通過（`ctest --repeat until-fail:20 -R VaultWatcher`）
  - 監看本身不能卡住 UI 執行緒（重新掃描放到 `QtConcurrent`）
- **備註**：（開發 A）
  - 新增 VaultWatcher.h/.cpp、tests/core/VaultWatcherTest.cpp、tests/core/TestMain.cpp；修改 nexus_core 連結 Qt6::Concurrent、根目錄 CMakeLists 的 find_package 加入 Concurrent 與 Test。
  - 做法：QFileSystemWatcher 監看根目錄、所有資料夾與所有筆記檔，事件只負責啟動 debounce（預設 150ms，從第一個事件起算固定視窗，不會被連續事件無限延後）；視窗結束後在 QtConcurrent 工作執行緒重新掃描（Vault::open），回到主執行緒與上一份快照比對，產生 notesAdded/Removed/Modified/foldersChanged。因此行為與平台事件細節無關，新資料夾與其內筆記一併建立也能偵測（登記監看的空窗期由「登記完成後再驗證掃描一次」補上）。掃描中再有事件會排隊重掃一次。
  - 登記數千個監看路徑本身是 I/O：改成每批 256 個、在事件迴圈輪次之間分批登記，避免建構時卡住 UI。
  - 忽略自己的寫入：NoteFiles::setWriteListener(VaultWatcher::writeListener())，write 成功後登記新的 size+mtime；掃描結果與登記值相同就不發 notesModified。登記後若又被外部改成別的內容，仍會回報。已知小空窗：寫入 commit 到登記之間若剛好有一次掃描完成，會多報一次 notesModified（UI 收到時比對內容即可，無害）。這是 NoteFiles 唯一的介面新增（T-102 已加，非破壞性）。
  - 額外公開介面：rescanNow()（立即重掃，略過 debounce）、signal rescanned()（每次掃描結束都發，供測試與 UI 判斷穩定）、setDebounceMs()/debounceMs()、expectWrite()（可從任何執行緒呼叫，內部用 QMutex）。根目錄消失或不可讀時保留舊快照、不發任何 signal（只發 rescanned）。
  - 測試：自訂 gtest main（tests/core/TestMain.cpp）建立 QCoreApplication，nexus_core_tests 不再連結 GTest::gtest_main；選擇自訂 main 而非 GTest environment，因為 QCoreApplication 必須在 main 的堆疊上建立並先於任何測試。QSignalSpy 搭配上限 10 秒的 wait，沒有固定 sleep；「不應發生」的斷言一律用 rescanNow()+等待 rescanned 取代等待逾時。
  - 穩定性：debug 與 asan 的 build 目錄各跑 ctest --repeat until-fail:20 -R VaultWatcher（13 個測試各重複 20 次）全數通過。dev.ps1 吃不下 --repeat 參數（會被 PowerShell 當成自己的參數），所以直接呼叫 VS 內建的 ctest.exe；PM 若要納入腳本需改 dev.ps1，我沒動（開發 C 的範圍）。
  - clang-tidy：Q_OBJECT 的 moc 碼沒有造成問題，不需要 SKIP_LINTING；唯一的坑是 #include <QtConcurrent> 總標頭會引入 qplugin.h，clang 在 MSVC 模式下解析失敗（offsetof 常數運算式），改成 <QtConcurrent/QtConcurrentRun> 即可，以後請避免在被 tidy 的檔案引入 Qt 的總標頭。
  - Release 也跑過：61 個測試通過，VaultPerf 在 Release 啟用時間斷言（Vault::open 10k < 1 秒）通過。
  - 限制：重新命名在 signal 層是一個 removed 加一個 added（規格如此）；監看路徑數受作業系統上限限制（Linux inotify max_user_watches），登記失敗的路徑會被略過，但資料夾事件加上手動 rescanNow() 仍可補救。Linux 上尚未實測，等 CI（T-005）。

---

### UI 共通說明（T-110～T-114）
- **負責**：開發 D（UI 專責，worktree `nexus-dev-d`）
- 設計稿（作者已確認，2026-10-25）：https://claude.ai/artifact/TvyTEzBXdyibRPPXrfGSiH
  - `Main.dc.html`：主畫面，底部的 `renderVals()` 有完整的淺色／深色色票
  - `Welcome.dc.html`：歡迎畫面；`States.dc.html`：檔案操作狀態
  - 讀取方式：PM 已把四張設計稿的 HTML 存進 repo 的 `docs/design/m1/`，請直接讀這些檔案
- 技術基礎見 ADR 0006（Fusion + QSS、色票集中、系統原生標題列、系統字型、自動儲存）
- 測試：新增 `nexus_ui_tests`，用 `QT_QPA_PLATFORM=offscreen` 跑 Widgets 測試；邏輯（model、格式化、狀態機）一定要有單元測試，純視覺的部分則用截圖人工比對
- 每張卡完成時，要在 PR 摘要附上**淺色與深色的實際截圖**（存到 `docs/screenshots/T-xxx-*.png`），PM 會拿來和設計稿逐一比對

### T-110 主題系統
- **負責**：D
- **狀態**：待辦
- **依賴**：M0
- **檔案**：`src/ui/Theme.h/.cpp`（`Palette` 結構：bg、chrome、panel、surface、border、borderSoft、text、muted、accent、accentSoft、accentText、marker、gutter、gutterActive、lineActive、link、tag、tagBg、codeBg、kw、ok、danger、warnBg、warnText…）、`src/ui/ThemeManager.h/.cpp`（淺色／深色／跟隨系統、QSettings、`setColorScheme`、發出 `themeChanged` signal）、`src/ui/style/app.qss.in` 或用程式產生 QSS、`src/ui/resources.qrc`（JetBrains Mono、SVG 圖示）、修改 `MainWindow`（加入「檢視 › 主題」選單）、`tests/ui/ThemeTest.cpp`
- **完成標準**：
  - 兩套色票與設計稿完全相同（測試逐一比對數值）
  - 自動檢查對比：text、muted 對 bg、panel、surface 的對比度都要 ≥ 4.5:1，兩套主題都要測
  - 切換主題時立即生效、不必重啟；原生標題列跟著變深色或淺色（Windows 11 手動驗證並附截圖）
  - 跟隨系統時，系統切換深淺色，Nexus 跟著切換
  - 主視窗的選單列、狀態列套用色票
- **備註**：

### T-111 主視窗框架、歡迎畫面、開啟 vault
- **負責**：D
- **狀態**：待辦
- **依賴**：T-110、T-101
- **檔案**：`src/ui/MainWindow`（左邊檔案樹、右邊工作區的 QSplitter，工作區用 QStackedWidget 切換歡迎畫面和編輯區）、`src/ui/WelcomePage.h/.cpp`、`src/ui/RecentVaults.h/.cpp`（QSettings，最多 10 筆）、`src/ui/VaultSession.h/.cpp`（持有目前開啟的 Vault，用 QtConcurrent 開啟）、`tests/ui/RecentVaultsTest.cpp`
- **完成標準**：
  - 歡迎畫面照 `Welcome.dc.html`：開啟資料夾（Ctrl+O）、建立新 vault、最近開啟清單（找不到的資料夾要標紅並提供「從清單移除」；檢查是否存在要在背景執行，避免網路磁碟卡住）、清除清單
  - 建立新 vault：選一個空資料夾，在裡面建立一篇「歡迎.md」說明筆記，然後開啟
  - 啟動時自動開啟上次的 vault；不存在就顯示歡迎畫面
  - 開啟 vault 期間顯示載入狀態，UI 不能卡住
  - 選單：
    - 檔案：開啟 vault…、建立新 vault…、最近開啟 ›、新增筆記（Ctrl+N）、儲存（Ctrl+S）、關閉分頁（Ctrl+W）、關閉 vault、結束
    - 檢視：主題 ›、顯示檔案樹（Ctrl+B）、顯示行號
    - 說明：關於 Nexus
  - 視窗標題：「筆記名 — vault 名 — Nexus」；記住視窗位置與大小
- **備註**：

### T-112 檔案樹
- **負責**：D
- **狀態**：待辦
- **依賴**：T-111、T-102
- **檔案**：`src/ui/VaultTreeModel.h/.cpp`（QAbstractItemModel）、`src/ui/FileTreePanel.h/.cpp`（標頭列：vault 名稱，加上新增筆記、新增資料夾、全部收合三個按鈕）、`src/ui/RenameDelegate`、`src/ui/DeleteConfirmDialog`、`tests/ui/VaultTreeModelTest.cpp`
- **完成標準**：
  - 照 `Main.dc.html` 左欄與 `States.dc.html` 狀態 1～3
  - 不顯示 `.md`；資料夾在前；用 core 的自然排序
  - 選取時的底色與文字色、資料夾展開與收合箭頭、未儲存的圓點，都照設計稿
  - 右鍵選單：在新分頁開啟、在同一資料夾新增筆記、重新命名（F2）、複製相對路徑、在檔案總管中顯示（Windows 用 `explorer /select,`；Linux 開啟所在資料夾）、刪除（Delete）
  - F2 原地改名：用 `NoteFiles::validateName` 即時檢查，錯誤提示顯示在輸入框下方（設計稿狀態 2）；Enter 確認、Esc 取消
  - 刪除確認對話框照設計稿狀態 3，有「不要再詢問」選項，記在 QSettings；實際刪除是移到資源回收筒
  - 新增筆記後，自動進入改名狀態並開啟該筆記
  - model 有完整測試，包含增刪改後的結構與排序；用 `QAbstractItemModelTester` 驗證
- **備註**：

### T-113 編輯器、分頁、狀態列
- **負責**：D
- **狀態**：待辦
- **依賴**：T-110、T-102、T-104
- **檔案**：`src/ui/NoteEditor.h/.cpp`（QPlainTextEdit，含行號區、目前行底色、內容最大寬度 820px 置中）、`src/ui/MarkdownHighlighter.h/.cpp`（QSyntaxHighlighter，用 `lexLine`，block state 存 `LexState::toInt()`）、`src/ui/EditorTabs`（分頁，未儲存時把關閉鈕換成圓點）、`src/ui/BreadcrumbBar`、`src/ui/StatusBarWidgets`、`src/ui/NoteSession`（每個分頁的文件狀態與自動儲存）、`tests/ui/MarkdownHighlighterTest.cpp`、`tests/ui/WordCountTest.cpp`
- **完成標準**：
  - 上色規則照 `Main.dc.html`：
    - 標題 H1 24px、H2 19px、H3 17px 加粗，`#` 用 marker 色
    - `[[ ]]` 用 marker 色、目標文字用 link 色加底線
    - 標籤用 tag 色加 tagBg 底色（QSyntaxHighlighter 做不出圓角膠囊，用底色即可）
    - 行內程式碼和程式碼區塊用 JetBrains Mono 加 codeBg；清單符號用 accent 加粗；引用用斜體 muted
  - 內文 15px、行高 28px；行號用等寬字型、gutter 色，目前行號用 gutterActive 色
  - 自動儲存（ADR 0006）：停止輸入 1 秒後、切換或關閉分頁、失去焦點、關閉程式時；Ctrl+S 立即存；讀寫都在 QtConcurrent
  - 存檔保留原本的換行和 BOM（用 T-102 的 NoteDocument）
  - 狀態列：監看中、N 篇筆記、已儲存／儲存中／未儲存、第 X 行，第 Y 欄、字數（中日韓字元每字算 1，英數單字算 1）、UTF-8 · LF/CRLF
  - 麵包屑：vault 名 › 資料夾 › 檔名.md
  - 效能：開啟 1 萬行的筆記 < 300ms；在 1 萬行筆記中打字，人工量測不能有明顯延遲（附量測方法和數字）
  - 用 highlighter 測試確認各種 span 套到正確的格式
- **備註**：

### T-114 外部修改處理
- **負責**：D
- **狀態**：待辦
- **依賴**：T-113、T-103
- **檔案**：`src/ui/ExternalChangeBar`、修改 `NoteSession` 和 `MainWindow`（接上 VaultWatcher）、`tests/ui/NoteSessionTest.cpp`
- **完成標準**（照 `States.dc.html` 狀態 4）：
  - 沒有未儲存變更：自動重新載入，保留游標位置（盡量保留在同一行），狀態列短暫提示
  - 有未儲存變更：編輯器上方出現衝突列，有三個選項：比較差異（M1 先用簡單的左右對照對話框即可）、載入外部版本、保留我的版本（立刻覆寫）
  - 檔案在外部被刪除：分頁標示刪除線與「已從磁碟刪除」，Ctrl+S 可以存回去
  - 外部新增或刪除檔案時，檔案樹即時更新
  - NoteSession 的狀態轉換要有單元測試
- **備註**：

---
---

## 提前進行：M2 核心層（不含 UI）

> 2026-10-25 PM 決定：M1 的 UI 在等設計稿確認，先做 M2 不需要 UI 的核心層，避免開發停下來（ADR 0005）。
> 負責：**開發 B**。順序：T-201 → T-202。

### T-201 筆記內容擷取：連結、標籤、標題、frontmatter
- **負責**：B
- **狀態**：待辦
- **依賴**：T-104
- **目標**：把一篇筆記的完整文字轉成索引需要的結構化資料
- **檔案**：
  - `src/parser/NoteParser.h/.cpp`：`ParsedNote parseNote(QStringView text)`
  - `ParsedNote` 包含：
    - `title`：第一個 H1；沒有的話留空，由呼叫端用檔名
    - `headings`：`{level, text, line}`
    - `wikiLinks`：`{target, heading, alias, line, column}`
    - `mdLinks`：`[文字](相對路徑.md)`，只收 vault 內的相對路徑，網址不收
    - `tags`：去重、保留原本大小寫，另提供小寫正規化的版本，並區分來源是內文還是 frontmatter
    - `aliases`：來自 frontmatter
    - `plainText`：給 FTS 用，去掉 Markdown 標記符號但保留文字
  - `tests/parser/NoteParserTest.cpp`
- **完成標準**：
  - 連結和標籤一律透過 `MarkdownLexer` 取得（ADR 0005），程式碼區塊與行內程式碼內的不算
  - frontmatter：只在檔案第一行是 `---` 時才算，到下一個 `---` 或 `...` 結束；支援 `tags: [a, b]`、`tags:` 加上 `- a` 清單、`tags: a`，`aliases` 同樣三種寫法；frontmatter 內容不出現在 headings、links 與 plainText
  - 巢狀標籤 `#專案/nexus` 視為一個標籤，另提供展開成父標籤 `專案` 的函式（給之後的標籤樹用）
  - line 和 column 從 0 開始；column 以 UTF-16 計算
  - 效能：10,000 篇、每篇約 2KB 的筆記，release 下全部解析完 < 1 秒（單執行緒）
  - 至少 30 個測試
- **備註**：

### T-202 SQLite 索引與全文搜尋（storage）
- **負責**：B
- **狀態**：待辦
- **依賴**：T-201
- **目標**：把 `ParsedNote` 存進 SQLite，提供反向連結、標籤查詢與 FTS5 全文搜尋
- **檔案**：
  - `src/storage/CMakeLists.txt`：新的 `nexus_storage` 函式庫，連結 vcpkg 的 `unofficial::sqlite3::sqlite3`（或該套件實際的 target）與 `Qt6::Core`
  - `src/storage/SqliteHandle.h/.cpp`：RAII 包裝 `sqlite3*` 與 `sqlite3_stmt*`，錯誤回傳 `Expected`
  - `src/storage/IndexDb.h/.cpp`：
    - `open(path)`：自動建立或升級 schema，用 `PRAGMA user_version` 記版本
    - `upsertNote(relPath, mtime, size, const ParsedNote&)`
    - `removeNote(relPath)`、`renameNote(from, to)`
    - `backlinks(relPath)`：回傳 `{sourcePath, line, column, context}`，context 是那一行的文字
    - `outgoingLinks(relPath)`
    - `notesWithTag(tag)`：可選擇是否包含子標籤
    - `allTags()`：附各標籤的篇數
    - `search(query, limit)`：FTS5，回傳 `{path, title, snippet}`
    - `transaction()`：RAII，批次寫入用
  - 連結目標解析（`[[x]]` 對應到哪一篇）：照 Obsidian 的規則。先找檔名（不含 `.md`、不分大小寫）完全相同的；有多篇同名時優先選路徑最短的；也可以寫相對路徑 `[[資料夾/x]]`；還可以比對 aliases。找不到的連結也要存（之後做「尚未建立的筆記」）。目標筆記新增或改名時，相關連結要重新解析
  - `tests/storage/IndexDbTest.cpp`（用 `QTemporaryDir`）
- **完成標準**：
  - 資料庫在 `<vault>/.nexus/index.sqlite`（ADR 0005），開 WAL；`open` 遇到損毀的檔案時回傳錯誤，讓呼叫端決定是否刪掉重建
  - schema 建表 SQL 集中在一個地方，有版本號；升級路徑要有測試，即使目前只有 v1
  - FTS5 搜尋支援中文：用 `trigram` tokenizer（需要確認 vcpkg 的 sqlite3 版本支援），或另一個能處理中文的方案；在備註說明選擇與限制（例如 trigram 至少要 3 個字元）
  - 效能（release）：10,000 篇筆記批次 upsert < 2 秒；`search` < 50ms；`backlinks` < 5ms
  - SQL 一律使用參數綁定，不可以字串拼接（防 injection）
  - 根目錄 `CMakeLists.txt` 加入 `add_subdirectory(src/storage)`；`tests/CMakeLists.txt` 新增 `nexus_storage_tests`（只新增，不重排）
- **備註**：

---
---

## 提前進行：M3／M4 核心層與小修

> 2026-10-25 PM：UI 只有一位開發者（D），是目前的瓶頸。核心層開發者繼續把後面 Milestone 中不需要 UI 的核心往前做。

### T-105 小修：隱藏檔名
- **負責**：A
- **狀態**：待審核
- **依賴**：T-102
- **目標**：處理 T-102 審核時留下的事項
- **檔案**：`src/core/NoteFiles.cpp`、`src/core/FileError.h`（若需要新的 NameError）、`tests/core/NoteFilesTest.cpp`
- **完成標準**：
  - `validateName` 拒絕以 `.` 開頭的名稱（新增 `NameError::Hidden` 或沿用合適的列舉），並附測試（ADR 0004 補充）
- **備註**：新增 `NameError::Hidden`；`validateName` 在尾端空白／點檢查之後判斷，所以 `.` 與 `..` 仍回報 `TrailingSpaceOrDot`（既有測試不變），`.hidden`、`.nexus` 回報 `Hidden`。測試：`ValidateNameTest.RejectsHiddenNames`、`NameValidationOnOperationsTest.RejectsHiddenNamesOnCreateAndRename`（create／rename 都拒絕；放在獨立 suite，因為 `NoteFilesTest` 是 TEST_F fixture，不能混用 TEST）。debug／release／asan／tidy 全部 156 測試通過，format-check 通過。

### T-301 關係圖力導向佈局（核心演算法）
- **負責**：A
- **狀態**：待審核
- **依賴**：M0
- **目標**：M3 圖譜視圖使用的佈局引擎，不依賴 Qt Widgets，可單元測試
- **檔案**：`src/core/graph/ForceLayout.h/.cpp`、`src/core/graph/QuadTree.h/.cpp`（Barnes-Hut）、`tests/core/ForceLayoutTest.cpp`
- **設計**：
  - 輸入：節點數 N、邊清單（索引對）；可以增量加減節點與邊，已存在的節點位置要保留
  - 力：Barnes-Hut 斥力（θ 可調，預設 0.9）、邊的彈簧引力（理想長度可調）、往中心的弱引力（避免孤立節點飛走）、速度衰減
  - 收斂：類似 d3-force 的 alpha 冷卻（alpha、alphaDecay、alphaMin）；`step()` 推進一格，`isSettled()` 判斷是否穩定；`reheat()` 用在拖曳或結構改變時
  - 固定節點：`pin(i, x, y)` 與 `unpin(i)`，給拖曳用
  - 決定性：同一個 seed 的結果完全相同（初始位置用固定 seed 的螺旋或亂數）
- **完成標準**：
  - 正確性：沒有 NaN 或 Inf；兩個節點重疊時不會爆炸；相連節點的平均距離明顯小於隨機節點對；固定節點的位置不動
  - 效能（release）：1,000 個節點、3,000 條邊時單步 < 4ms（PLAN「1000 節點 60 FPS」，留時間給繪圖）；10,000 個節點、30,000 條邊時單步 < 60ms。時間斷言只在 `NEXUS_ENFORCE_PERF` 下檢查
  - 至少 15 個測試
- **備註**：
  - 檔案：`src/core/graph/{Vec2.h, QuadTree.h/.cpp, ForceLayout.h/.cpp}`（命名空間 `nexus::core`，只用標準函式庫）、`tests/core/ForceLayoutTest.cpp`（20 個測試，含 5 個 QuadTree、2 個 Perf）。
  - 介面：`ForceLayout(seed)` 或 `ForceLayout(N, edges, seed)`；`addNode()`／`addNode(Vec2)`／`removeNode(i)`／`addEdge`／`removeEdge`（任何結構變動都會 `reheat()`，既有節點位置保留）；`step()`、`isSettled()`、`reheat(alpha=0.5)`（只會升高 alpha）、`pin/unpin/isPinned`、`positions()`（`std::span<const Vec2>`，結構變動後失效）、`setParams()`。
  - `removeNode(i)` 採「保序」：較大的索引全部減 1，相連的邊一併移除（UI 需同步自己的索引表）。邊為無向，自環與重複邊被忽略（回傳 false）。
  - 演算法照 d3-force：斥力為 1/d 核（`repulsion` 30 × alpha），Barnes-Hut θ 預設 0.9（θ=0 為精確 O(n²)，測試用來對照）；彈簧 `linkDistance` 30、強度預設 1/min(度數)、依度數分配偏移；中心引力 `centerStrength` 0.05；`velocityDecay` 0.6；`alphaMin` 0.001、`alphaDecay` 0.0228（約 300 步）、`alphaTarget` 0（拖曳時可設 0.3 讓它不 settle）。
  - 重疊節點：QuadTree 對座標完全相同的點用索引雜湊出確定的方向推開；彈簧長度為 0 時用 mt19937 的微小抖動。Cell 含查詢點時一律往下展開，避免把自己算進質心。
  - 決定性：初始位置為黃金角螺旋（起始角由 seed 決定），亂數只用原始 `std::mt19937` 輸出（不用 `<random>` 的 distribution），所以同 seed 同平台結果相同。
  - 效能（release，30～60 步平均，含最初最擁擠的步驟）：1,000 節點／3,000 邊 0.54 ms（上限 4）；10,000 節點／30,000 邊 8.4 ms（上限 60）。
  - 沒有做：節點質量／權重（目前每個點質量 1）、碰撞力、多執行緒；新加入的節點不會自動放在鄰居附近（UI 可用 `addNode(Vec2)` 指定）。
  - debug／release／asan／tidy 全部 181 個測試通過，format-check 通過。

### T-106 dev.ps1 參數轉發
- **負責**：C
- **狀態**：待辦
- **依賴**：T-005
- **目標**：`./scripts/dev.ps1 test debug --repeat until-fail:20 -R VaultWatcher` 能把後面的參數完整轉給 ctest（目前 PowerShell 會把 `--repeat` 吃掉）
- **檔案**：`scripts/dev.ps1`、`README.md`
- **完成標準**：`test`、`build`、`configure` 後面的任意參數（含 `--` 開頭的）都能原樣轉給底層工具；README 附上範例
- **備註**：

### T-401 Claude Code CLI 橋接（M4 核心）
- **負責**：C
- **狀態**：待辦
- **依賴**：M0
- **目標**：用 `QProcess` 呼叫 Claude Code CLI 執行任務，把串流輸出解析成結構化事件（PLAN M4）
- **檔案**：`src/ai/CMakeLists.txt`（新的 `nexus_ai`，只連 QtCore）、`src/ai/AgentRun.h/.cpp`（QObject）、`src/ai/StreamJsonParser.h/.cpp`（純函式，逐行解析）、`src/ai/AgentEvent.h`、`tests/ai/StreamJsonParserTest.cpp`、`tests/ai/AgentRunTest.cpp`、`tests/ai/fake_claude/`（測試用的假 CLI 執行檔，C++ 寫，依參數輸出預錄的 stream-json，並可模擬錯誤、卡住、非 0 結束）
- **設計**：
  - 啟動：`<claude 路徑> -p --output-format stream-json --verbose [其他設定的參數]`，工作目錄為專案程式碼資料夾，prompt 從 stdin 寫入後關閉 stdin。CLI 路徑與額外參數由呼叫端設定（之後由設定頁提供），預設是 PATH 上的 `claude`
  - 事件：`started`、`assistantText(text)`、`toolUse(name, inputSummary)`、`toolResult(isError, summary)`、`finished(AgentResult{success, durationMs, costUsd?, sessionId?, finalText})`、`failed(AgentError{kind, message})`；AgentError 的 kind 包含：找不到執行檔、啟動失敗、非 0 結束、輸出無法解析、逾時、使用者取消
  - 取消：`cancel()` 先 terminate，3 秒後還沒結束就 kill；可設定整體逾時
  - 串流：逐行解析 stdout（要處理一行被切成好幾塊送進來的情況），stderr 另外收集，失敗時附在錯誤裡
  - 解析器對未知的事件類型與欄位要寬容（略過但記錄），不可以因 CLI 升版而崩潰
  - 不做：組裝 context（之後的 T-402 會依賴 T-201 的 NoteParser）、UI 面板
- **完成標準**：
  - 用假 CLI 測試完整流程：正常完成、工具呼叫、CLI 回報錯誤、非 0 結束、逾時、取消、輸出被切塊、巨量輸出（10MB）不卡住
  - 解析器至少 20 個測試，含畸形 JSON
  - 根目錄 `CMakeLists.txt` 加入 `add_subdirectory(src/ai)`；`tests/CMakeLists.txt` 新增 `nexus_ai_tests` 與假 CLI 目標（只新增，不重排）
  - 備註記錄所參考的 stream-json 格式（事件種類與欄位），以及驗證的方式（若本機有安裝 claude，可以實際跑一次簡單的 prompt 對照；沒有的話就照官方文件）
- **備註**：
