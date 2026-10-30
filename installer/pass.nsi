Unicode true
!include "MUI2.nsh"
!include "x64.nsh"
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
 ; Upgrade: let the previous version's uninstaller remove its own file list first,
 ; so files dropped in this version do not linger. _?= keeps it running in place.
 IfFileExists "$INSTDIR\Uninstall.exe" 0 +2
  ExecWait '"$INSTDIR\Uninstall.exe" /S _?=$INSTDIR'
 ; The VC++ runtime DLLs ship app-local in the package; no system-wide redist is installed.
 SetOutPath "$INSTDIR"
 File /r "..\dist\portable-v${PASS_VERSION}\*.*"
 WriteUninstaller "$INSTDIR\Uninstall.exe"
 CreateDirectory "$SMPROGRAMS\Pass-SSL"
 CreateShortcut "$SMPROGRAMS\Pass-SSL\Pass-SSL.lnk" "$INSTDIR\Pass.exe"
 WriteRegStr HKLM "${UNINSTALL_KEY}" "DisplayName" "Pass-SSL 安全密碼產生器"
 WriteRegStr HKLM "${UNINSTALL_KEY}" "DisplayVersion" "${PASS_VERSION}"
 WriteRegStr HKLM "${UNINSTALL_KEY}" "Publisher" "${PASS_AUTHOR}"
 WriteRegStr HKLM "${UNINSTALL_KEY}" "DisplayIcon" "$INSTDIR\Pass.exe"
 WriteRegStr HKLM "${UNINSTALL_KEY}" "UninstallString" '"$INSTDIR\Uninstall.exe"'
 WriteRegDWORD HKLM "${UNINSTALL_KEY}" "NoModify" 1
 WriteRegDWORD HKLM "${UNINSTALL_KEY}" "NoRepair" 1
SectionEnd
Section "桌面捷徑" SEC_DESKTOP
 SetShellVarContext all
 CreateShortcut "$DESKTOP\Pass-SSL.lnk" "$INSTDIR\Pass.exe"
SectionEnd
Section "Uninstall"
 SetShellVarContext all
 SetRegView 64
 !include "uninstall-files.nsh"
 Delete "$INSTDIR\Uninstall.exe"
 RMDir "$INSTDIR"
 Delete "$SMPROGRAMS\Pass-SSL\Pass-SSL.lnk"
 RMDir "$SMPROGRAMS\Pass-SSL"
 Delete "$DESKTOP\Pass-SSL.lnk"
 DeleteRegKey HKLM "${UNINSTALL_KEY}"
SectionEnd
