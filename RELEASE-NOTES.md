# Pass 2.0 Plus（Windows BCrypt 核心）

Pass 2.0 Plus 是原本 Pass 2.0 非 OpenSSL 核心的強化版。**Pass 2.1 起才切換 OpenSSL**；已公開的 [Pass-SSL v2.2.3](https://github.com/Kaido-MSVC/Pass/releases/tag/v2.2.3) 屬於 OpenSSL 系列，保留原 main、Tag、Release 與附件。Plus 的原始碼在 `plus` 分支，正式 Tag 為 `v2.0-plus`，不是 v2.2.3 的原地升級。

## 下載與使用

- `Pass-2.0-Plus-win64-portable.zip`：完整解壓後執行 Pass.exe，目標機須有 Microsoft Visual C++ x64 執行階段。
- `Pass-2.0-Plus-win64-setup.exe`：NSIS 安裝包，內含官方 VC++ Redistributable；**不是 MSI**。
- `SHA256SUMS.txt`：上述二進位附件的最終 SHA-256。
- 原始碼：本 Release 的 Source code，或 clone 後切換 `v2.0-plus`。

顯示／字串版本為 2.0 Plus；CMake 數值版 2.0.0，Windows 固定版本 2.0.0.0。請同時核對 Plus 檔名與 SHA-256。Windows 10 1809+／Windows 11 x64 為支援目標。

## 功能與變更

- 原有隨機字元密碼及 EFF 單詞密碼雙模式，支援批次產生、遮罩與剪貼簿到期清除。
- 符號池由 24 增至 28，新增 `~ < > |`。
- 整合離線 zxcvbn-c 詞典、鍵盤／重複／序列等強度提示。
- 跟隨 Windows、淺色、深色外觀；主視窗使用 Windows 擷取排除 API。
- 說明選單提供作者與授權摘要；本次補齊 Mesa／LLVM 完整授權與來源紀錄，更新兩個封裝。

亂數仍來自 Windows BCryptGenRandom，失敗即停止。沒有 OpenSSL 相依、DRBG 選單或弱亂數備援。部分評分與外觀整合自 Pass-SSL v2.2.2，沒有移入其 OpenSSL 核心。Plus 未包含 v2.2.3 的全部後續功能與修正。

## 相依與授權

Pass 自有程式碼 MIT；Qt 6.12.0 動態連結 LGPLv3；zxcvbn-c／Dropbox 字典 MIT；EFF 詞表 CC BY 4.0；Mesa llvmpipe 11.2.2 的 Brian Paul、Khronos MIT 與 C11 threads Boost 1.0 全文，以及 legacy LLVM 3.6 UIUC/NCSA 材料均隨附。Microsoft VC++ Redistributable／D3DCompiler 適用 Microsoft 條款。見 `LICENSE.txt`、`THIRD_PARTY_NOTICES.md`、`LICENSES/INDEX.md`。

對照：v2.2.3 使用 OpenSSL 4.0.3（Apache-2.0），以該版自己的授權材料為準，不能套用 Plus 的無 OpenSSL 相依說明。

## 驗證與已知限制

沿用 2026-10-30 補授權後已通過的 11／11 組功能測試。此次僅改文件與封裝，未修改或重新編譯 Pass.exe，未重跑完整功能／邊界／壓力測試；重新核對兩包負載逐檔雜湊、完整版權聲明及最终 SHA-256。公開驗證摘要見 `docs/PUBLICATION-VALIDATION.md`。

執行檔與安裝器均未數位簽章。尚未在乾淨 Windows VM 完成安裝／解除安裝及 VC++ redist 流程，也未驗證 Windows 10 1809 實機。擷取排除依賴 DWM／擷取途徑，不能保證防止所有截圖、錄影或外部拍照；Plus 的剪取工具／OBS 未做手動驗收。強度分數不是安全機率、精確熵或外洩查詢；Qt 字串、剪貼簿歷程、其他程式及系統副本無法保證全面清除。
