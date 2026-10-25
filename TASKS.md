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
- **狀態**：待審核
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
- **狀態**：待審核
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
- **狀態**：待審核
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
- **備註**：（A）已完成。
  - `builtin-baseline` = `9e593bb18ea69cc5095e012465dcd675a822ed0d`，對應 vcpkg release tag **2026.07.29**（注意該 tag 是 annotated tag，`git ls-remote` 的 `refs/tags/2026.07.29` 是 tag 物件、commit 要看 `^{}` 那行）。
  - 升級後實際版本：gtest 1.17.0#3、md4c 0.5.3、sqlite3 3.53.4（core,fts5,json1）、tl-expected 1.3.1、**tree-sitter 0.26.11**。
  - `scripts/dev.ps1`：新增 `Initialize-Vcpkg`（clone 到 `%LOCALAPPDATA%\nexus\vcpkg`、checkout baseline、bootstrap，以 `.nexus-bootstrapped` 戳記判斷 baseline 變更時重 bootstrap）；刪除 `Ensure-PkgConfig`、`PKG_CONFIG`、`VCPKG_KEEP_ENV_VARS`、`VCPKG_DOWNLOADS` 設定。腳本中不再有任何自行下載的二進位，下載皆由 vcpkg 驗證 SHA512。
  - 注意：`vcvars64.bat` 本身會匯出 VCPKG_ROOT 指向 VS 內建 vcpkg，所以腳本在匯入 VS 環境前先記下呼叫者的 `VCPKG_ROOT`；只有它不在 VS 目錄下才當作自備 vcpkg 使用，否則用受管理的 clone。
  - 驗收：刪除 `build/`、舊的 `%LOCALAPPDATA%\vcpkg`（含 pkgconf 快取 `%LOCALAPPDATA%\nexus\tools`）、vcpkg clone 內的 downloads/buildtrees/packages/vcpkg.exe 與 bootstrap 戳記後（clone 本身沿用以省流量，bootstrap 與相依從頭建），`./scripts/dev.ps1 all debug|release|asan` 皆 exit 0。

---

## M0 完成檢查（PM 執行）
- [ ] T-001 ~ T-006 全部「已完成」
- [ ] CI 在兩平台皆綠燈
- [ ] PLAN.md 中 M0 項目打勾
- [ ] 拆出 M1 任務卡
- [ ] M1 的 UI 畫面（檔案樹、編輯器）設計稿經作者確認

---
---

## 下一個 Milestone：M1 Vault 與筆記

> M0 合併後開工。M1 仍由 **開發 A** 負責（WORKFLOW：M0、M1 只開 A）。
> 開工前必讀：ADR 0004（核心層路徑與檔案 I/O）。
> 順序：T-101 → T-102 → T-104 → T-103 →（設計稿確認後）T-110 → T-111 → T-112 → T-113 → T-114。
> UI 卡依 CLAUDE.md 規定，要等作者確認設計稿（待決事項 D-01）才能開工。設計稿：https://claude.ai/artifact/TvyTEzBXdyibRPPXrfGSiH

### T-101 Vault 開啟與掃描（核心）
- **負責**：A
- **狀態**：待辦
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
- **備註**：

---

### T-102 筆記檔案操作（核心）
- **負責**：A
- **狀態**：待辦
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
- **備註**：

---

### T-104 Markdown 行內標記解析（給語法高亮用）
- **負責**：A
- **狀態**：待辦
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

---

### T-103 Vault 檔案監看（核心）
- **負責**：A
- **狀態**：待辦
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
- **備註**：

---

### T-110～T-114 UI（阻塞：等設計稿確認，見待決事項 D-01）
- **T-110 主題系統**：依設計稿兩套色票，建立 `src/ui/Theme`（QPalette + QSS）；選單「檢視 › 主題」可切換淺色／深色／跟隨系統，用 `QSettings` 記住
- **T-111 歡迎畫面與開啟 vault**：歡迎畫面、最近開啟清單（找不到的資料夾要標示）、開啟／建立 vault；`Vault::open` 用 `QtConcurrent` 在背景執行
- **T-112 檔案樹**：`QAbstractItemModel` 包裝 Vault、不顯示 `.md`、右鍵選單、F2 原地改名（用 `validateName` 即時提示）、刪除確認對話框
- **T-113 編輯器與分頁**：`QPlainTextEdit` + 行號 + 目前行底色 + 用 T-104 上色；分頁（未儲存圓點）、Ctrl+S、麵包屑、狀態列（行／欄、字數、換行種類）
- **T-114 外部修改處理**：接上 VaultWatcher；沒有未儲存變更就自動重新載入並在狀態列提示；有未儲存變更就顯示衝突列；檔案被刪除時，分頁顯示刪除線
- 詳細檔案清單與完成標準：設計稿確認後再補
