# Pass v2.0 Plus（BCrypt 版）

本版以已驗證的 Pass v2.0 原始碼為基礎，只整合以下四項；顯示版本為
**Pass v2.0 Plus**，CMake 數值版本仍為 2.0.0、Windows 固定版本仍為
2.0.0.0。Windows 字串版本為 2.0 Plus，不是 v2.1。

## 版本線與授權補正

Plus 是原本 Pass 2.0 的 BCrypt 核心強化版，Pass 2.1 起才是 OpenSSL；已公開 v2.2.3 屬於 OpenSSL 系列。本版不取代或覆寫該版。

2026-10-30 僅補正第三方授權與重新封裝，Pass.exe 及既有 11／11 功能驗收保持不變。完整版權及條款見 LICENSES/INDEX.md，公開發行說明見 RELEASE-NOTES.md。安裝包為 NSIS EXE，沒有 MSI。

## ASCII

內建符號由 24 個擴充為 28 個，完整順序如下（最後四個為新增）：

    !@#$%^&*()-_=+[]{};:,.?/~<>|

加上大小寫字母和數字，未排除時共 90 個不同字元。預設排除易混淆字元
`Il1O0o|` 後為 83 個。內建字元不含空白、控制字元、DEL、非 ASCII、
雙引號、單引號、反斜線與反引號，以降低輸入與轉貼混淆。這不表示所有
網站都接受這 28 個符號；請以「排除字元」配合目的網站規則。

為保留 v2.0 既有功能與設定，自訂字元仍允許 ASCII 33–126，包括使用者
明確加入的上述四個標點；空白和控制字元仍禁止。去重、排除優先、類別
最低數量、候選整組拒絕與 BCryptGenRandom 核心不變。新增字元屬於符號
類別，受 minSymbols 限制；自訂重疊不增加其機率。使用既有 8-bit 拒絕
取樣，接受區間大小是字元池大小的整數倍；無 modulo 偏差。

輸出與剪貼簿使用純文字，不將 `<`、`>` 解釋成 HTML；批次僅以換行分隔，
不添加跳脫字元或截斷。系統剪貼簿歷史與其他應用程式可能保留內容。

## 強度評分

按本專案的 analyze() API 整合 Pass-SSL v2.2.2 的鍵盤路徑、重複、序列、
替換詞和本機 zxcvbn-c 詞典上限。詞典在建置時內嵌，無 OpenSSL、網路
查詢或新增外部執行環境。保留其記憶體清理、配置失敗歸零、NUL 與
128 UTF-16 單元界限，以及原有兩種產生規則熵。

公開範例 correct horse battery staple 和大小寫、分隔符、leet 變形不再
得到強／很強。長的無關隨機後綴仍可得到信用，未對含有單一詞的輸入
一律封頂。有限詞典偏向英語；不能認出所有常見、個人或外洩密碼，
也不能證明輸入是隨機產生。分數不是精確熵、破解時間或安全保證。
授權、固定來源版本和整合細節見 third_party/zxcvbn-c/PASS-INTEGRATION.md。

## 外觀

沿用 SSL 的 Fusion 控制項、卡片、分頁、焦點／按下／停用狀態和淺深色
配色，新增「外觀」選單：跟隨 Windows、淺色、深色。保留 PassLocal/Pass
設定名稱、原有雙模式與功能；副標題明示 BCryptGenRandom，沒有 DRBG
選單，也沒有移植 SSL 的語言、剪貼簿倒數或其他額外功能。

## 主視窗擷取排除

建構完成時建立 HWND，在第一次顯示前套用 WDA_EXCLUDEFROMCAPTURE。
WinIdChange 使用 internalWinId() 重新套用；不在銷毀事件強制建立 HWND。
沿用 SSL 已驗證的 SetWindowDisplayAffinity 包裝與不含密碼的 Qt 紀錄。
只有不支援旗標／API 的錯誤才嘗試 WDA_MONITOR；其他錯誤紀錄後繼續執行。
Windows 10 2004 前僅有 WDA_MONITOR 語意；退化時可能顯示空白視窗。

API 只涵蓋特定 Windows 擷取途徑，依賴桌面合成；不保證阻止所有截圖、
錄影、OBS 模式、遠端工具或相機。此次驗證的是主視窗 API 生命週期和
程式化桌面擷取正反對照；未在 Plus 版手動驗證剪取工具／OBS，未驗證
Windows 10 1809 實機。一般 QWidget::grab() 是應用內繪製，可用於 UI
測試，不能拿它判斷桌面擷取排除是否有效。

Microsoft API 說明：
https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setwindowdisplayaffinity

## 建置與交付

build.cmd 使用原有 MSVC x64／Qt 6.12.0／CMake／Ninja，執行完整 CTest。
deploy.ps1 僅取 build/Pass.exe，輸出 dist/portable-v2.0-Plus、
Pass-2.0-Plus-win64-portable.zip 和 installer/Pass-2.0-Plus-win64-setup.exe。
請勿使用舊專案 dist/portable/Pass.exe（v1.01）。

2026-10-16 已同步回 D:/projects/pass（見 SYNC-STATUS.md）。安裝包已建置並核對
抽出的 Pass.exe 與建置、可攜版一致；乾淨 Windows VM 的安裝、解除安裝
與 VC++ 執行階段流程尚未驗證。備份、測試紀錄與 SHA-256 清單另隨交付。
