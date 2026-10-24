# 0001：Qt 取得方式與 Windows 工具鏈

- 日期：2026-10-25
- 狀態：已採用
- 決定者：PM / CTO

## 背景
PLAN.md 與 T-001 原本規劃由 vcpkg manifest 安裝 `qtbase`。實測開發機狀況：

- 已安裝 Qt 6.8.3（`C:\Qt\6.8.3\msvc2022_64`，含 Core / Gui / Widgets / Sql / Test / Concurrent 的 debug 與 release DLL）。
- VS 2022 Community 17.12.3：MSVC 14.42，並內建 CMake、Ninja、vcpkg（2024-09-30）、clang-format、clang-tidy，但都不在 PATH。
- 沒有 WSL，Linux 只能在 CI 驗證。

vcpkg 在 Windows 從原始碼建置 `qtbase` 需 1～2 小時、數 GB 空間，CI 冷快取時也一樣慢。

## 選項
1. **Qt 走 vcpkg**：所有相依一致由 vcpkg 管理；代價是首次建置極慢，CI 快取失效時 Windows job 可能超時。
2. **Qt 走官方安裝（本機）＋ `install-qt-action`（CI），其他小型相依走 vcpkg**：建置快；Qt 版本需在兩處（本機安裝、CI 設定）手動對齊。
3. 全部不用 vcpkg，改 FetchContent：違反 PLAN 的套件管理選型，且 tree-sitter / md4c 需自行維護建置腳本。

## 決定
採用選項 2。

- Qt：固定 **6.8.3**。本機用 `C:\Qt\6.8.3\msvc2022_64`；Linux / CI 用 `jurplel/install-qt-action` 安裝同版本。CMake 以 `CMAKE_PREFIX_PATH`（或 preset 中的 `QT_ROOT` 類變數）尋找。
- vcpkg manifest 只放：`sqlite3`（需 fts5 feature）、`md4c`、`tree-sitter`、`gtest`、`tl-expected`。
- Windows 本機一律在 VS 開發者環境（vcvars64）中跑 CMake + Ninja；由 `scripts/` 下的包裝腳本處理，作者不需手動設定 PATH。
- `VCPKG_ROOT` 使用 VS 內建的 vcpkg；若日後遇到內建版本過舊，再改為獨立 clone 的 vcpkg（另立 ADR）。

## 理由
- Qt 是最大且最慢的相依，已存在於開發機，重編沒有收益。
- 其餘相依都很小，vcpkg 仍能提供一致的跨平台管理與 binary cache。
- Qt 以 LGPL 動態連結的授權策略不受影響。
