# Nexus

知識圖驅動的 AI 開發環境（C++20 + Qt 6.8.3）。計畫見 `PLAN.md`，規範見 `CLAUDE.md`。

## 建置（Windows）

前置：Visual Studio 2022（含 C++ 工作負載）、Qt 6.8.3 位於 `C:\Qt\6.8.3\msvc2022_64`、git。
全部指令都透過 `scripts/dev.ps1`，它會自動載入 VS x64 環境、設定 `QT_ROOT`，並準備釘死版本的 vcpkg（見 ADR 0003）：
首次執行會把 vcpkg clone 到 `%LOCALAPPDATA%/nexus/vcpkg`、checkout 到 `vcpkg.json` 的 `builtin-baseline` 並 bootstrap；baseline 變更時自動更新。

```powershell
./scripts/dev.ps1 configure debug   # 首次會用 vcpkg 建置相依套件，需數分鐘
./scripts/dev.ps1 build debug
./scripts/dev.ps1 test debug
./scripts/dev.ps1 all debug         # configure + build + test
./scripts/dev.ps1 clean debug       # 刪除 build/debug
./scripts/dev.ps1 shell             # 顯示解析到的工具路徑
```

若 PowerShell 擋下腳本（執行原則），改用：
`powershell -ExecutionPolicy Bypass -File scripts/dev.ps1 build debug`

可用環境變數覆寫：`NEXUS_VS_ROOT`、`VCPKG_ROOT`（自備 vcpkg，腳本不管理；注意 VS 開發者環境本身設的 VCPKG_ROOT 會被忽略）、`QT_ROOT`。

Qt DLL：建置後由 `windeployqt` 自動複製到執行檔旁（`cmake/NexusQtDeploy.cmake`），
執行檔不需要設定 PATH 即可直接執行。

## 建置（Linux）

```bash
export VCPKG_ROOT=/path/to/vcpkg   # checkout 到 vcpkg.json 的 builtin-baseline
export QT_ROOT=/path/to/Qt/6.8.3/gcc_64
cmake --preset debug && cmake --build --preset debug && ctest --preset debug
```

## Presets 與品質檢查

- `debug`、`release`、`asan` 三個 preset（`CMakePresets.json`）。
- 警告：MSVC `/W4 /WX /permissive-`；GCC/Clang `-Wall -Wextra -Wpedantic -Werror`，只套用在本專案 target（`cmake/CompilerWarnings.cmake`）。
- `asan`：Linux 為 ASan + UBSan；Windows 為 MSVC `/fsanitize=address`（已關閉 `/RTC*` 與增量連結，
  執行階段 DLL `clang_rt.asan_*.dll` 會在建置後複製到執行檔旁，見 `cmake/Sanitizers.cmake`）。
  `./scripts/dev.ps1 all asan`
