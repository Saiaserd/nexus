# 0007：tree-sitter 語法套件的取得方式

- 日期：2026-10-25
- 狀態：已採用
- 決定者：PM / CTO

## 背景
M5 要用 tree-sitter 解析專案程式碼，抽出模組、類別、函式，讓它們以節點的形式出現在圖譜中。tree-sitter 本體（0.26.11）已經由 vcpkg 提供，但 vcpkg 的 baseline 只有 `tree-sitter-c` 這一個語法套件，沒有 C++、Python、TypeScript 等。

Nexus 自己就是 C++ 專案，作者會用 Nexus 開發 Nexus，所以 C++ 一定要第一個支援。

## 選項
1. **vcpkg overlay port**（在 repo 的 `vcpkg-overlays/` 下自寫 port）：跟其他相依一樣，釘 tag 與 SHA512、由 vcpkg 建置並快取，CI 和本機都走同一條路。
2. **git submodule / 直接把語法原始碼放進 `third_party/`**：簡單，但要自己寫建置腳本，也不受 vcpkg binary cache 管理。
3. **FetchContent**：每次 configure 都要連網，也沒有雜湊驗證的統一做法。

## 決定
採用選項 1。

- 在 `vcpkg-overlays/ports/tree-sitter-<lang>/` 為每個語法寫 overlay port：`vcpkg.json` 加上 `portfile.cmake`，用 `vcpkg_from_github` 釘住 release tag 與 SHA512，並用一個極簡的 CMakeLists 把 `parser.c`（和 `scanner.c`，如果有的話）編成靜態庫。
- `vcpkg.json` 的 `vcpkg-configuration.overlay-ports` 指向 `vcpkg-overlays/ports`，讓本機、CI、所有 preset 都自動套用。
- 第一批：`tree-sitter-cpp`（並使用 vcpkg 已有的 `tree-sitter-c`）。其他語言等 M5 確定範圍後再加，每加一種語言就新增一個 port。
- 語法的授權（都是 MIT）要記錄在 port 的 `copyright` 中。

## 理由
- 所有第三方原始碼都經過雜湊驗證，也都由 vcpkg 快取，跟 ADR 0003 的原則一致。
- 新增語言的成本固定、可以預期，不會讓建置系統變得零散。
