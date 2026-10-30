# Pass v2.0 Plus（BCrypt 版）

作者：Kaido-MSVC　·　Copyright (c) 2026 Kaido-MSVC　·　MIT License

## 核心演進與版本關係

**Pass 2.0 Plus 是原本非 OpenSSL 核心的強化版；Pass 2.1 起才切換至 OpenSSL。**

| 版本線 | 核心 | 原始碼／相依與授權 |
|---|---|---|
| Pass 2.0 Plus | Windows BCryptGenRandom | `plus` 分支；Pass MIT、Qt 6.12.0 LGPLv3 動態連結、zxcvbn-c／Dropbox MIT、EFF CC BY 4.0；Mesa／LLVM／Microsoft 元件見授權索引 |
| Pass 2.1 起（含已公開 v2.2.3） | OpenSSL | `main` 的 Pass-SSL 系列；v2.2.3 使用 OpenSSL 4.0.3（Apache-2.0），其他相依依該版文件 |

Plus 的評分與外觀部分整合自 Pass-SSL v2.2.2，亂數核心仍為 BCrypt，沒有 OpenSSL 相依或 DRBG 選單，也未包含 v2.2.3 的全部後續修正。兩條版本線獨立保存，不能視為可互換的原地更新。

[Plus 正式下載](https://github.com/Kaido-MSVC/Pass/releases/tag/v2.0-plus) ·
[既有 Pass-SSL v2.2.3](https://github.com/Kaido-MSVC/Pass/releases/tag/v2.2.3) ·
[完整第三方授權索引](LICENSES/INDEX.md)

## 授權

本程式以 MIT License 釋出，全文見 `LICENSE.txt`。使用的第三方元件各自適用其授權，完整條文在 `LICENSES/`，詳細說明見 `THIRD_PARTY_NOTICES.md`：

| 元件 | 用途 | 授權 |
|---|---|---|
| Qt 6.12.0 | 介面（動態連結，DLL 未修改、可替換） | LGPLv3 |
| Mesa llvmpipe 11.2.2／LLVM 3.6 系列 | 軟體 OpenGL 備援（opengl32sw.dll） | MIT、Boost 1.0；LLVM UIUC/NCSA，見授權全文 |
| Microsoft D3DCompiler_47.dll | Qt 圖形輔助元件 | Microsoft Windows SDK 再發行條款 |
| zxcvbn-c | 密碼強度估算與字典 | MIT（字典源自 Dropbox zxcvbn，MIT） |
| EFF Long Wordlist | 隨機單詞密碼詞表 | CC BY 4.0 |
| Microsoft VC++ Redistributable | 執行階段（安裝版以官方安裝程式安裝） | Microsoft 授權條款 |

程式內「說明 › 關於 Pass」顯示作者與元件授權摘要，「說明 › 關於 Qt」顯示 Qt 的授權聲明。安裝程式會先顯示 `LICENSE.txt`。

顯示版本 Pass v2.0 Plus；Windows 字串版本 2.0 Plus；CMake 數值版本 2.0.0，Windows 固定版本 2.0.0.0。

本次四項修改、ASCII 完整集合與已知限制詳見 [PLUS.md](PLUS.md)。以下保留原有 v2.0 功能說明與歷史驗證紀錄。

## v2.0 雙模式

- Qt Widgets 雙分頁：原有隨機字元密碼＋EFF Long Wordlist 隨機單詞密碼。
- 單詞 4–12（預設 6）、連字號／底線／空白（預設連字號）、批次 1–100。
- 7,776 個官方原詞固定內嵌，保留順序、允許重複，執行時完全離線。
- 原始詞表 SHA-256：addd35536511597a02fa0a9ff1e5284677b8883b83e986e43f15a3db996b903e。
- 詞表載入時驗證雜湊、筆數、唯一單詞、骰碼格式與完整順序；失敗只停用單詞模式。
- 字元使用原有 8-bit 拒絕取樣；單詞使用 16-bit，接受 0–62207，拒絕 62208–65535；每次最多 1,024 次。
- 兩模式共用 BCrypt、安全結果儲存、整批 10 秒期限、單一未完成工作限制、取消與清理。
- 切換分頁非阻塞取消、清除結果與仍相同的剪貼簿、恢復遮罩，拒收延遲結果。
- 單詞產生規則熵為詞數 × log2(7776)，六詞約 77.5 bits；固定分隔符不增加熵。
- 最長 12 詞為 119 字元，100 筆批次上限 11,999 字元；完整複製，不截斷或為迎合網站而篩選短詞。
- 保留 PassLocal/Pass 與 v1.01 字元設定鍵，新增 passphrase/*。只保存規則，啟動預設字元頁與遮罩。
- 無密碼歷史、無明文持久化、無正式版測試亂數切換功能。

## 保留的 v1.01 維護修正

- 產生工作最多等待 10 秒；取消、清除結果及關閉視窗不再等候背景 BCrypt 呼叫返回。取消或逾時後不交付結果；同一程序只允許一個未完成工作，避免累積卡住的執行緒。
- BCrypt 仍使用 Windows 系統安全亂數，不加入 OpenSSL，也不使用不安全備援或強制終止執行緒。同步核心 generate() 本身仍需等 BCrypt 返回；若作業系統呼叫永久卡住，請關閉並重新啟動程式。
- 成功結果與批次結果改為獨占、不可複製且清除時安全覆寫的儲存；移除 QVariant 明文及 QFuture 結果副本。取消、失敗、替換、清除及結束會清理受控結果。結果欄關閉復原紀錄；「清除結果」亦清除仍屬於本次複製的剪貼簿。
- Qt 控制項、手動輸入、暫存 QString 的隱含副本、系統剪貼簿與歷史、暫存器、頁面檔及程序傾印無法保證全面抹除；卡在 BCrypt 內的候選及 RNG 緩衝區須待呼叫返回才由原有 RAII 清零。此版本降低殘留，並非完整記憶體抹除保證。
- build.cmd 現在包含原有核心/UI 測試、36 項安全測試，以及直接阻塞 BCrypt、取消、逾時、關窗、延遲結果、部分批次清除與儲存清零測試。

Qt 6 Widgets / C++17 / Windows BCryptGenRandom，繁體中文離線桌面程式。

## 執行

可攜版：解壓 dist/Pass-2.0-Plus-win64-portable.zip 後執行 Pass.exe。
目標電腦須有 Microsoft Visual C++ x64 執行階段。
安裝版：dist/installer/Pass-2.0-Plus-win64-setup.exe，會安裝官方執行階段、捷徑及解除安裝程式。
支援 Windows 10 1809 以上及 Windows 11 x64。

## 功能

- 密碼長度 4–128，批次 1–100 筆，預設 20 字元。
- 大小寫、數字、符號各類最低數量 0–8；自訂字元加入字元池，也可關閉各類只用自訂字元。
- 排除易混淆字元或指定字元；自訂輸入限定可見 ASCII，不含空白。
- 安全亂數、無偏差字元取樣及整組拒絕取樣；有效候選比例低於 0.1% 時提示調整規則。
- 顯示受限制的有效組合熵，手動密碼的離線啟發式強度檢查及規則符合度。
- 密碼預設隱藏，可複製全部；預設 30 秒後僅清除仍相同的剪貼簿內容。
- 僅保存規則於目前使用者的 PassLocal/Pass 設定，不將密碼寫入檔案或紀錄。

手動強度評分不是外洩密碼檢查，百分比是介面分數，不是安全機率。
剪貼簿歷史或其他應用程式可能保留複製內容；本程式只清除目前剪貼簿。
密碼在程式記憶體中存在；Qt 字串不提供所有副本皆可安全抹除的保證。

## 建置與測試

執行 build.cmd；已固定本機 MSVC 14.44 x64、SDK 10.0.26100.0、Qt 6.12.0、CMake 3.30.5 及 Ninja。
測試涵蓋產生規則、不合法規則、自訂字元、各類最低數量、有效組合熵、弱密碼辨識及取消。
介面測試另驗證批次產生、預設遮罩、顯示密碼、複製、定時清除、保留其他程式的剪貼簿內容、清除結果與無效規則禁止產生。
工具路徑不同時修改 build.cmd 和 deploy.ps1；不要混用 MinGW Qt。
執行 powershell -ExecutionPolicy Bypass -File deploy.ps1 建立可攜 ZIP 與 NSIS 安裝包。
主程式圖示由 resources/app.ico 嵌入視窗、執行檔、安裝程式及捷徑。

## 驗證範圍

本機編譯、核心功能測試、可攜資料夾啟動及介面版面檢查。
2026-10-10 驗證：當時兩組自動測試皆通過；移除 Qt PATH 與外掛環境路徑後，可攜版啟動成功。
安裝包已編譯；尚未在未安裝 Qt 的乾淨 Windows 虛擬機完成安裝／解除安裝驗證。

## v2.0 建置與驗收

build.cmd 依原有 MSVC x64 / Qt 6.12.0 / CMake / Ninja 工具鏈建置 Release。
建置時以 UTF-8 探測本機 MSVC 的 /showIncludes 前綴，避免中文訊息編碼造成 Ninja 漏追蹤原始碼／標頭相依性。
測試為七組：字元規則、共用 UI、安全抽樣、秘密生命週期、單詞規則、單詞清理、詞表缺失。
保留原有 36 項安全案例，另窮舉 65,536 個 16-bit 輸入；加入取消、逾時、關窗、分頁切換、
跨模式忙碌限制、延遲結果、最長批次、設定相容及詞表完整性檢查。
測試亂數替身僅在測試目標中編譯，Pass.exe 連結正式 pass_core。

Windows 10 1809 / Windows 11 x64 為延續的支援目標；實際宿主驗收與尚未完成的乾淨系統
安裝／解除安裝、Windows 10 1809 實機驗證，見隨附 v2.0 交付報告。

## 2026-10-30 Plus 公開發行紀錄

本次只補正 Mesa llvmpipe／LLVM 與再發行材料、核心版本說明並重新封裝，沒有修改應用程式、測試或 RNG 邏輯，也沒有重新編譯 Pass.exe。沿用 2026-10-30 補授權後已通過的 11／11 組功能測試；重新核對 ZIP／NSIS 負載與完整授權，最終 SHA-256 見 Release 的 SHA256SUMS.txt。

安裝包是 **NSIS EXE，不是 MSI**。Portable 需已安裝 VC++ x64 執行階段；NSIS 內含未修改的官方 VC++ Redistributable 安裝器。執行檔與安裝器未簽章；乾淨 VM 安裝／解除安裝、Windows 10 1809 實機及 Plus 的剪取工具／OBS 手動驗收尚未完成。完整限制見 PLUS.md 與 RELEASE-NOTES.md。
