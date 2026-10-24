# Nexus

知識圖驅動的 AI 開發環境（C++20 + Qt 6.8.3）。計畫見 `PLAN.md`，規範見 `CLAUDE.md`。

## 建置（Windows）

前置：Visual Studio 2022（含 C++ 工作負載）、Qt 6.8.3 位於 `C:\Qt\6.8.3\msvc2022_64`、git。
全部指令都透過 `scripts/dev.ps1`，它會自動載入 VS x64 環境、設定 `VCPKG_ROOT` / `QT_ROOT`：

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

可用環境變數覆寫：`NEXUS_VS_ROOT`、`VCPKG_ROOT`、`QT_ROOT`。

Qt DLL：建置後由 `windeployqt` 自動複製到執行檔旁（`cmake/NexusQtDeploy.cmake`），
執行檔不需要設定 PATH 即可直接執行。

## 建置（Linux）

```bash
export VCPKG_ROOT=/path/to/vcpkg
export QT_ROOT=/path/to/Qt/6.8.3/gcc_64
cmake --preset debug && cmake --build --preset debug && ctest --preset debug
```
