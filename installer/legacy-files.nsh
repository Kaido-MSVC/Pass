; Reviewed files shipped by Pass-SSL v2.2.2 and removed in v2.2.3.
; v2.2.3-v2.2.5 have the same payload paths as v2.2.6.
; Keep this list in source control. Add future retired payload paths here.
; Never populate it from the installation directory or remove unknown files.
!macro PASS_DELETE_LEGACY_FILES
 Delete "$INSTDIR\D3Dcompiler_47.dll"
 Delete "$INSTDIR\opengl32sw.dll"
 Delete "$INSTDIR\Qt6Network.dll"
 Delete "$INSTDIR\generic\qtuiotouchplugin.dll"
 Delete "$INSTDIR\imageformats\qgif.dll"
 Delete "$INSTDIR\imageformats\qicns.dll"
 Delete "$INSTDIR\imageformats\qjpeg.dll"
 Delete "$INSTDIR\imageformats\qtga.dll"
 Delete "$INSTDIR\imageformats\qtiff.dll"
 Delete "$INSTDIR\imageformats\qwbmp.dll"
 Delete "$INSTDIR\imageformats\qwebp.dll"
 Delete "$INSTDIR\LICENSES\Copyright.txt"
 Delete "$INSTDIR\networkinformation\qnetworklistmanager.dll"
 Delete "$INSTDIR\tls\qcertonlybackend.dll"
 Delete "$INSTDIR\tls\qschannelbackend.dll"
!macroend
!macro PASS_REMOVE_LEGACY_DIRS
 RMDir "$INSTDIR\generic"
 RMDir "$INSTDIR\networkinformation"
 RMDir "$INSTDIR\tls"
!macroend
