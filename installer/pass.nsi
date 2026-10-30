Unicode true
!include "MUI2.nsh"
!include "x64.nsh"
Name "Pass 安全密碼產生器 v2.0 Plus"
VIProductVersion "2.0.0.0"
VIAddVersionKey "ProductName" "Pass v2.0 Plus"
VIAddVersionKey "ProductVersion" "2.0 Plus"
VIAddVersionKey "FileVersion" "2.0 Plus"
VIAddVersionKey "FileDescription" "Pass v2.0 Plus Setup"
VIAddVersionKey "CompanyName" "Kaido-MSVC"
VIAddVersionKey "LegalCopyright" "Copyright (c) 2026 Kaido-MSVC"
OutFile "..\dist\installer\Pass-2.0-Plus-win64-setup.exe"
InstallDir "$PROGRAMFILES64\Pass"
RequestExecutionLevel admin
!define MUI_ICON "..\resources\app.ico"
!define MUI_UNICON "..\resources\app.ico"
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "..\LICENSE.txt"
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
Section "Pass" SEC_MAIN
 SetShellVarContext all
 InitPluginsDir
 SetOutPath "$PLUGINSDIR"
 File "..\dist\redist\vc_redist.x64.exe"
 ExecWait '"$PLUGINSDIR\vc_redist.x64.exe" /install /quiet /norestart' $0
 ${If} $0 == 3010
  SetRebootFlag true
 ${ElseIf} $0 != 0
 ${AndIf} $0 != 1638
  MessageBox MB_ICONSTOP "VC++ 執行階段安裝失敗（代碼 $0）。"
  Abort
 ${EndIf}
 SetOutPath "$INSTDIR"
 File /r "..\dist\portable-v2.0-Plus\*.*"
 WriteUninstaller "$INSTDIR\Uninstall.exe"
 CreateDirectory "$SMPROGRAMS\Pass"
 CreateShortcut "$SMPROGRAMS\Pass\Pass.lnk" "$INSTDIR\Pass.exe"
 CreateShortcut "$DESKTOP\Pass.lnk" "$INSTDIR\Pass.exe"
 WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\PassLocal" "DisplayName" "Pass v2.0 Plus 安全密碼產生器"
 WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\PassLocal" "DisplayVersion" "2.0 Plus"
 WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\PassLocal" "Publisher" "Kaido-MSVC"
 WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\PassLocal" "DisplayIcon" "$INSTDIR\Pass.exe"
 WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\PassLocal" "UninstallString" '"$INSTDIR\Uninstall.exe"'
 WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\PassLocal" "NoModify" 1
 WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\PassLocal" "NoRepair" 1
SectionEnd
Section "Uninstall"
 SetShellVarContext all
 SetRegView 64
 !include "uninstall-files.nsh"
 Delete "$INSTDIR\Uninstall.exe"
 RMDir "$INSTDIR"
 Delete "$SMPROGRAMS\Pass\Pass.lnk"
 RMDir "$SMPROGRAMS\Pass"
 Delete "$DESKTOP\Pass.lnk"
 DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\PassLocal"
SectionEnd
