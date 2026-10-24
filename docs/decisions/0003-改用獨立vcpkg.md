# 0003：改用獨立安裝、釘死版本的 vcpkg

- 日期：2026-10-25
- 狀態：已採用（取代 ADR 0001 中「使用 VS 內建 vcpkg」一項）
- 決定者：PM / CTO

## 背景
T-001 實作時發現，VS 內建 vcpkg（2024-09-30）的腳本會去 MSYS2 下載 pkgconf 2.2.0，但這個檔案已經從鏡像站移除（404），導致 gtest 建置失敗。開發 A 的暫時解法是在 `scripts/dev.ps1` 自行從 repo.msys2.org 下載 pkgconf，有兩個問題：

- 下載的執行檔沒有驗證雜湊；指定的檔名下載失敗時，還會從網頁抓「最新版」，有供應鏈風險。
- 這個 baseline 的 tree-sitter 是 0.20.6，太舊，跟近期的語法解析器 ABI 不相容（M5 會用到）。

## 選項
1. 繼續用 VS 內建 vcpkg 加上自行下載 pkgconf：維持現狀，但風險會一直留著，VS 更新時也可能壞掉。
2. **獨立 clone 的 vcpkg，釘在一個較新的 release tag**：vcpkg 自己下載工具時會驗證雜湊；本機和 CI 用同一個 commit。
3. 改用 Conan：要整個換掉套件管理，代價太大。

## 決定
採用選項 2。

- `scripts/dev.ps1` 在 `%LOCALAPPDATA%\nexus\vcpkg` clone microsoft/vcpkg，checkout 到 `vcpkg.json` 的 `builtin-baseline` 那個 commit，第一次使用時執行 bootstrap。之後若 baseline 改了，腳本會自動 fetch 並 checkout。`VCPKG_ROOT` 仍可用環境變數覆寫。
- `builtin-baseline` 改成一個近期的穩定 release tag 所對應的 commit。
- 刪掉 `dev.ps1` 中自行下載 pkgconf 的程式碼。
- CI（T-005）用同一個 baseline commit（例如 `lukka/run-vcpkg` 的 `vcpkgGitCommitId`）。

## 理由
- 排除沒有驗證的下載，本機與 CI 的相依版本一致。
- 升級 baseline 後 tree-sitter 等套件變新，避免 M5 才遇到不相容。
