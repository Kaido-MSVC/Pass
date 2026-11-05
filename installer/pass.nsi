Unicode true
!include "MUI2.nsh"
!include "x64.nsh"
!include "legacy-files.nsh"
; deploy.ps1 passes /DPASS_VERSION from CMakeLists.txt project(VERSION).
!ifndef PASS_VERSION
  !error "Build with makensis /DPASS_VERSION=x.y.z (run deploy.ps1)."
!endif
!define PASS_AUTHOR "Kaido-MSVC"
!define UNINSTALL_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\Pass-SSLLocal"
Name "Pass-SSL 安全密碼產生器 v${PASS_VERSION}"
VIProductVersion "${PASS_VERSION}.0"
VIAddVersionKey "ProductName" "Pass-SSL"
VIAddVersionKey "CompanyName" "${PASS_AUTHOR}"
VIAddVersionKey "ProductVersion" "${PASS_VERSION}"
VIAddVersionKey "FileVersion" "${PASS_VERSION}"
VIAddVersionKey "FileDescription" "Pass-SSL v${PASS_VERSION} Setup"
VIAddVersionKey "LegalCopyright" "Copyright (C) 2026 ${PASS_AUTHOR}"
OutFile "..\dist\installer\Pass-SSL-${PASS_VERSION}-win64-setup.exe"
InstallDir "$PROGRAMFILES64\Pass-SSL"
RequestExecutionLevel admin
!define MUI_ICON "..\resources\app.ico"
!define MUI_UNICON "..\resources\app.ico"
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "..\LICENSE.txt"
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "TradChinese"
Function .onInit
 ${IfNot} ${RunningX64}
  MessageBox MB_ICONSTOP "本程式需要 64 位元 Windows。"
  Abort
 ${EndIf}
 SetRegView 64
FunctionEnd
Section "Pass-SSL（必要）" SEC_MAIN
 SectionIn RO
 SetShellVarContext all
 ; NV-01: never execute code from the existing installation, even in Program Files.
 ; Overwrite from this installer and remove only the reviewed, embedded legacy list.
 ; No directory/registry supplied manifest, recursive deletion or deferred deletion.
 ClearErrors
 Delete "$INSTDIR\Uninstall.exe"
 IfErrors install_failed
 SetOverwrite on
 ClearErrors
 SetOutPath "$INSTDIR"
 File /r "..\dist\portable-v${PASS_VERSION}\*.*"
 IfErrors install_failed
 ClearErrors
 !insertmacro PASS_DELETE_LEGACY_FILES
 IfErrors install_failed
 !insertmacro PASS_REMOVE_LEGACY_DIRS
 ClearErrors
 WriteUninstaller "$INSTDIR\Uninstall.exe"
 IfErrors install_failed
 CreateDirectory "$SMPROGRAMS\Pass-SSL"
 CreateShortcut "$SMPROGRAMS\Pass-SSL\Pass-SSL.lnk" "$INSTDIR\Pass.exe"
 WriteRegStr HKLM "${UNINSTALL_KEY}" "DisplayName" "Pass-SSL 安全密碼產生器"
 WriteRegStr HKLM "${UNINSTALL_KEY}" "DisplayVersion" "${PASS_VERSION}"
 WriteRegStr HKLM "${UNINSTALL_KEY}" "Publisher" "${PASS_AUTHOR}"
 WriteRegStr HKLM "${UNINSTALL_KEY}" "DisplayIcon" "$INSTDIR\Pass.exe"
 WriteRegStr HKLM "${UNINSTALL_KEY}" "UninstallString" '"$INSTDIR\Uninstall.exe"'
 WriteRegDWORD HKLM "${UNINSTALL_KEY}" "NoModify" 1
 WriteRegDWORD HKLM "${UNINSTALL_KEY}" "NoRepair" 1
 Goto install_done
install_failed:
 SetErrorLevel 1
 MessageBox MB_OK|MB_ICONSTOP "無法更新 Pass-SSL 檔案。請關閉 Pass-SSL，確認目錄可寫入後重新執行安裝程式。" /SD IDOK
 Abort
install_done:
SectionEnd
Section "桌面捷徑" SEC_DESKTOP
 SetShellVarContext all
 CreateShortcut "$DESKTOP\Pass-SSL.lnk" "$INSTDIR\Pass.exe"
SectionEnd
Section "Uninstall"
 SetShellVarContext all
 SetRegView 64
 !insertmacro PASS_DELETE_LEGACY_FILES
 !include "uninstall-files.nsh"
 !insertmacro PASS_REMOVE_LEGACY_DIRS
 Delete "$INSTDIR\Uninstall.exe"
 RMDir "$INSTDIR"
 Delete "$SMPROGRAMS\Pass-SSL\Pass-SSL.lnk"
 RMDir "$SMPROGRAMS\Pass-SSL"
 Delete "$DESKTOP\Pass-SSL.lnk"
 DeleteRegKey HKLM "${UNINSTALL_KEY}"
SectionEnd
