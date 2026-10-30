# Pass-SSL 安全密碼產生器 v2.2.3

作者：Kaido-MSVC　·　Copyright (c) 2026 Kaido-MSVC　·　MIT License

## 授權

Pass-SSL 本身以 **MIT License** 開源，全文見 `LICENSE.txt`。MIT 只涵蓋本專案撰寫的程式碼與資源；使用的第三方元件各自適用其授權，完整條文在 `LICENSES/`，詳細說明見 `THIRD_PARTY_NOTICES.md`：

| 元件 | 用途 | 授權 |
|---|---|---|
| Qt 6.12.0 | 介面（動態連結，DLL 未修改、可替換） | LGPLv3 |
| OpenSSL 4.0.3 | 安全亂數（libcrypto） | Apache License 2.0 |
| zxcvbn-c | 密碼強度估算與字典 | MIT（字典源自 Dropbox zxcvbn，MIT） |
| EFF Long Wordlist | 隨機單詞密碼詞表 | CC BY 4.0 |
| Microsoft VC++ Runtime | 執行階段 DLL | Microsoft 再發行條款 |

程式內「說明 › 關於 Pass-SSL」顯示作者與元件授權摘要，「說明 › 關於 Qt」顯示 Qt 的授權聲明。安裝程式第一頁顯示 `LICENSE.txt`。

## v2.2.3 改善

- 複製密碼時同時標記 `ExcludeClipboardContentFromMonitorProcessing`、`CanIncludeInClipboardHistory=0`、`CanUploadToCloudClipboard=0`，要求 Windows 不把密碼存進剪貼簿歷程（Win+V），也不同步到其他裝置。仍無法約束其他程式自行記錄的剪貼簿內容。
- 視窗關閉後會取消背景產生工作，並最多等待 2 秒讓它結束，再釋放靜態資源與 OpenSSL，避免結束時競爭。
- 背景輪詢只在產生中或舊工作尚未釋放時執行，閒置時不再每 25 ms 喚醒。
- 修正英文／日文介面視窗標題仍顯示中文（翻譯表停在 v2.2.0 字串）。
- 版本號只在 `CMakeLists.txt` 的 `project(VERSION)` 設定一次，由 CMake 產生 `pass_version.h` 供程式、版本資源、測試使用；`deploy.ps1`、`run-validation.ps1` 與 NSIS 從同一處讀取。
- `deploy.ps1` 自動尋找已安裝的最新 VC++ x64 CRT（14.44 以上），不再寫死 Redist 版本資料夾；部署後先對可攜版執行 `--smoke-test`，失敗即停止。
- 可攜版移除純 Widgets 程式用不到的軟體 OpenGL、D3D 編譯器、TLS、網路資訊、觸控與多數影像格式外掛（保留 ico、svg）。
- 安裝程式：顯示授權頁、桌面捷徑改為可選、升級前先執行舊版解除安裝程式以移除舊檔；不再另外執行 VC++ redist（執行階段 DLL 已隨程式附帶）。

## v2.2.2 主視窗擷取排除

主視窗預設以 Windows `SetWindowDisplayAffinity(hwnd, WDA_EXCLUDEFROMCAPTURE)` 排除支援此機制的截圖／錄影擷取。Qt HWND 在建構完成前建立，顯示前即套用；`WinIdChange` 後對新的 HWND 重新套用，銷毀時不重建 HWND。不新增選項或執行期依賴。

Windows 10 2004（build 19041）起支援排除；較舊系統依 Microsoft 文件只具有 `WDA_MONITOR` 空白視窗語義。旗標不支援時另嘗試 `WDA_MONITOR`，其他失敗維持程式可用。結果與 Win32 錯誤碼寫入 Qt 診斷日誌，不記錄密碼；沒有新增檔案日誌。

只處理整個主視窗，獨立選單、對話框不在此範圍。依賴 Windows DWM 與擷取方式；不是 DRM，無法保證防止所有截圖、錄影、遠端擷取或外部拍照。程式內部 QWidget 繪製／grab 不受此系統機制保證。

以 Pass BCrypt 2.0 為基礎的獨立 OpenSSL 版。專案位於 `D:\projects\pass-SSL`，原專案 `D:\projects\pass` 維持獨立。應用程式名稱、設定與安裝目錄均為 Pass-SSL，兩版可並存。

## 亂數演算法

頂部「演算法」選單可選：

| 選項 | 固定參數 | Provider |
|---|---|---|
| CTR-DRBG（首次預設） | AES-256-CTR、啟用 derivation function | default |
| HASH-DRBG | SHA-256 | default |
| HMAC-DRBG | SHA-256 | default |
| HMAC-DRBG | SHA-512 | default |

四個選項皆以 OpenSSL 4.0.3 EVP_RAND API 建立，要求 256-bit DRBG security strength；這不代表輸出密碼必有 256-bit 熵。每個同步產生器持有獨立 SEED-SRC 與所選 DRBG，不共用亂數狀態；每執行緒共用私有 OSSL_LIB_CTX 以快取 provider 演算法。SEED-SRC 取得作業系統熵；不自行用時間或固定值播種。內建 default provider 提供三種 DRBG；不載入外部 openssl.cnf、legacy 或 FIPS provider，亦不宣稱 FIPS 驗證。

切換會取消舊工作、清除舊結果與仍屬於本次複製的剪貼簿。下一次工作擷取新的選擇；已卡住的底層呼叫返回前不允許另開工作。選擇立即存至目前使用者的 `PassLocal/Pass-SSL`、`random/algorithm`，重新啟動恢復。未知設定會明確阻擋產生並要求重新選擇。

初始化、provider、熵來源或 EVP_RAND_generate 失敗立即中止。只有回傳 1 才算成功；錯誤顯示演算法、失敗階段及可取得的 OpenSSL 原因。失敗後同一產生器不可再使用，不改用另一演算法或 BCrypt。

## 保留功能與安全語義

- 字元密碼 4–128 字元，大小寫／數字／符號最低數量、自訂與排除字元、批次 1–100。
- EFF Long Wordlist 7,776 詞內嵌，單詞 4–12（預設 6），允許重複，三種分隔符。來源雜湊 `addd35536511597a02fa0a9ff1e5284677b8883b83e986e43f15a3db996b903e`。
- 8-bit 字元拒絕取樣與整組候選拒絕取樣；16-bit 單詞取樣接受 0–62207、拒絕 62208–65535。保留 1,024 次單次取樣上限及 100,000 次字元候選上限。
- 整批 10 秒期限、合作式取消、單一未完成工作限制；取消或逾時後拒收延遲結果，不強制終止執行緒。
- RNG 失敗立即清零緩衝區；RAII 清除緩衝區、部分候選及批次結果，釋放 DRBG 前 uninstantiate。測試使用 EVP_RAND_verify_zeroization 驗證 DRBG 清零。
- 預設遮罩、複製後 30 秒倒數；使用 Windows 原生剪貼簿鎖定、複製序號與內容核對，避免清除其他應用後續複製內容（包括相同文字）。不保存密碼歷史。

取消無法中斷正在執行的 OpenSSL／OS 呼叫，該呼叫返回後才完成其資源清理。Qt 控制項、隱含共享副本、系統剪貼簿歷史、暫存器、頁面檔及傾印不保證全面抹除。統計測試是回歸檢查，不能證明密碼學安全。

## 建置與測試

執行 `build.cmd`。需要已安裝的 Qt 6.12.0 MSVC x64、MSVC 14.44、Windows SDK 10.0.26100.0、CMake、Ninja。C++17、Release、/MD。

CMake 明確使用 `D:/Program Files/OpenSSL-Win64`，精確要求 4.0.3 Crypto；匯入庫鎖定 `lib/VC/x64/MD/libcrypto.lib`，用 dumpbin 驗證 x64 與 libcrypto-4-x64.dll 匯入、Release 動態 CRT 相依性，避免 static、MT 或 Debug 庫。組態後將同來源 DLL 複製到 build。更換 OpenSSL 根目錄必須重新組態。

CTest 包含原有規則、GUI、拒絕取樣、失敗、取消、逾時、清零、詞表與生命週期測試，另有四演算法獨立狀態、失敗不備援、provider/SEED-SRC/DRBG 初始化錯誤、重填錯誤及零化測試。GUI 測試使用暫存設定；畫面檔存至 build。

## 部署

執行 `powershell -NoProfile -ExecutionPolicy Bypass -File deploy.ps1`，使用既有 windeployqt、NSIS 與 Visual Studio 附帶的 VC++ 執行階段 DLL；不安裝任何開發工具。`-SkipInstaller` 可只製作 ZIP。

- `dist/portable-v<版本>/Pass.exe`
- `dist/Pass-SSL-<版本>-win64-portable.zip`
- `dist/installer/Pass-SSL-<版本>-win64-setup.exe`

可攜版附 Qt DLL/plugins、`libcrypto-4-x64.dll`、VC++ Release CRT DLL 與 LICENSES。default provider 內建於 libcrypto，不需外部 provider DLL、OpenSSL 安裝或 D 槽路徑。保留 DLL 與 plugins 的相對位置。Windows 10 1809+／Windows 11 x64；仍依賴 Windows 系統元件與 Universal CRT。

安裝版使用獨立 Pass-SSL 目錄、捷徑與解除安裝鍵；VC++ 執行階段 DLL 與程式一起安裝，不另外執行系統層級 redist。建置測試不執行安裝器。

`Pass.exe --smoke-test` 會實際使用四種演算法分別產生字元及單詞密碼、檢查規則，再開啟 GUI 並退出；失敗回傳非零。可攜驗證應搬移目錄、移除開發 PATH、設定無效 OPENSSL_CONF/OPENSSL_MODULES，確認仍可完成，並檢查載入 DLL 實際位於部署目錄。

API 參考：https://docs.openssl.org/4.0/man7/EVP_RAND/ 、https://docs.openssl.org/4.0/man7/EVP_RAND-SEED-SRC/

## v2.1.1 新增

- 繁體中文、日本語、English 可即時切換。介面、規則驗證、強度說明、狀態和錯誤訊息同步更新；演算法名稱、官方英文詞表和產生結果不翻譯。偏好 `ui/language` 立即同步保存；未知語言使用繁體中文。
- 設定採白名單，只保存語言、演算法、數量、長度、類型、最低數量、模糊字元排除和剪貼簿清除開關。自訂／排除字元只留在當次視窗；舊的這兩個欄位及未知鍵會刪除。不保存密碼、檢查欄輸入、亂數種子、DRBG 狀態或歷史。
- 每次重填檢查 DRBG READY 狀態；提供 `Random::reseed()`，清除預取緩衝後以 OS 熵重新播種，並在長存活 Random 每 4,096 次重填（約 1 MiB）明確呼叫。OpenSSL 自身的自動重新播種仍啟用。一般單筆產生器生命週期很短，不會達到此額外門檻。
- 初始化、重新播種、產生或狀態檢查失敗均清除緩衝、釋放 context，永久停用該 Random。沒有其他演算法或 BCrypt 回退。
- 演算法說明不宣稱安全性排名。DRBG 強度與密碼組合熵是不同指標。

## 自動驗證報告

先執行 `build.cmd`，再執行：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/run-validation.ps1
```

報告預設存於獨立的 `validation/yyyyMMdd-HHmmss`。`-ReportDirectory` 可指定位置，`-SkipStress` 僅重跑快速回歸。報告保存每項測試的名稱與通過狀態、原有 87 項回歸對照、CTest 時間、每演算法壓力完成數、停止原因與資源統計。原始比較診斷只留在 build；不複製到報告，因失敗比較可能含測試產生的密碼。

壓力測試每演算法最多 1,000,000 筆或 180 秒，以先達到者停止。字元與單詞模式交替，包含長度128／12詞邊界，逐筆以獨立規則檢查並清除自有文字緩衝。不把實際輸出寫入報告。期限無法中斷尚未返回的底層 OpenSSL／OS 呼叫，報告一律記錄實際耗時與完成數。

`tools/baseline-87.json` 保留 v2.0 的87項語義測試清單（不含 Qt 初始化／結束項）；v2.1 已增加的31項和 v2.1.1 新增項另計。原有設定測試調整為 v2.2.0 的非敏感白名單契約。

`Pass.exe --smoke-test` 與 `--screenshot` 使用暫存 INI 偏好，不操作正式設定。`--smoke-hold-ms 3000` 可讓四演算法實測後的 GUI 短暫存活，供唯讀模組來源檢查（250–10000毫秒）。

### 驗證限制

同機移除開發環境 PATH、隔離 OpenSSL 設定與搬移目錄，只能證明部署對本機開發路徑的獨立性，不能取代全新 Windows VM。測試通過、統計檢查與清零觀察都不能證明密碼學安全；不宣稱 FIPS 驗證。Qt 隱含共享副本、控制項內部、OS 剪貼簿歷史、頁面檔、傾印與暫存器的全面清零不在保證範圍。

API 依據：[OpenSSL EVP_RAND](https://docs.openssl.org/4.0/man3/EVP_RAND/)、[Windows OpenClipboard](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-openclipboard)、[Clipboard sequence](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getclipboardsequencenumber)。

## v2.2.0 介面與演算法

- 頂部「演算法／語言／外觀／說明」選單。主畫面顯示目前演算法，關於視窗顯示程式、實際載入的 OpenSSL 與 Qt 版本。
- 新增 HMAC-DRBG SHA-512，仍要求 256-bit DRBG 強度。SHA-512 不代表密碼有 512-bit 熵，也不宣稱演算法安全性排名。
- 外觀可跟隨 Windows、淺色或深色；系統外觀變更時自動更新。選擇立即保存於 `ui/appearance`，無效值回到跟隨 Windows。
- 保留原本 CTR-DRBG、HASH-DRBG、HMAC-DRBG 設定識別值；新選項使用 `HMAC-DRBG-SHA512`。新版設定檔交給舊版時，新演算法會依既有規則停用產生，須明確選回舊演算法。
- 新增 SHA-512 實際摘要查詢、失敗停止／清理／重播種、字元和單詞規則測試，以及三語系 × 三外觀 × 兩模式畫面與偏好重啟測試。

## v2.2.2 安全修正

僅修正密碼強度誤評與剪貼簿到期清除失敗；四種 DRBG、產生／規則驗證、UI 配置、語言及外觀不變。

強度檢查保留原有模式辨識，加入內嵌 MIT 授權 zxcvbn-c 詞庫評估，並辨識有限的公開教學片語及簡單變形。
`correct horse battery staple` 不再因長度而取得 100 分。分數仍為保守離線提示，不能辨認所有公開、外洩、個人或非英文片語，也不能證明亂數來源。
詳見 `third_party/zxcvbn-c/PASS-INTEGRATION.md`。NUL 或超過 128 個 UTF-16 單位的輸入不評估。

30 秒到期清除遇到鎖定時，每 100ms 非阻塞重試，最多 50 次且最多 5 秒；每次在鎖內重新核對序號、擁有者和內容。
若使用者後來複製任何其他內容（即使文字相同），取消本次清除。停用自動清除會取消排程；關閉視窗僅作一次即時嘗試，不阻塞退出。
持續鎖定超過重試上限時停止重試並清除應用內追蹤副本，不宣稱系統剪貼簿已清除。系統歷史、雲端剪貼簿和其他程序保存的副本不在抹除保證範圍。

完整回歸包含原有 8 組、上游估算器語料、配置失敗清理測試，以及 v2.2.2 的擷取排除測試，共 11 組 CTest。
`tools/build-asan.cmd` 另外以 MSVC 14.51 執行估算器記憶體安全檢查；產品仍使用原有 MSVC 14.44 x64 Release /MD。
