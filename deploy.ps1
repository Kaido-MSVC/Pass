$ErrorActionPreference = 'Stop'
Set-Location -LiteralPath $PSScriptRoot
if (!(Test-Path -LiteralPath 'build\Pass.exe')) { throw 'Run build.cmd first.' }
$portable = Join-Path $PSScriptRoot 'dist\portable-v2.0-Plus'
# Start from an empty folder so files removed from the project (or from windeployqt output) don't linger in the ZIP/installer.
if (Test-Path -LiteralPath $portable) { Remove-Item -LiteralPath $portable -Recurse -Force }
New-Item -ItemType Directory -Path $portable -Force | Out-Null
Copy-Item -LiteralPath 'build\Pass.exe' -Destination $portable
$env:PATH = 'D:\qt\6.12.0\msvc2022_64\bin;' + $env:PATH
& 'D:\qt\6.12.0\msvc2022_64\bin\windeployqt.exe' --release --no-translations --no-compiler-runtime --exclude-plugins qpdf --dir $portable 'build\Pass.exe'
if ($LASTEXITCODE -ne 0) { throw 'windeployqt failed.' }
Copy-Item -LiteralPath 'LICENSE.txt','README.md','PLUS.md','THIRD_PARTY_NOTICES.md','RELEASE-NOTES.md' -Destination $portable
Copy-Item -LiteralPath 'LICENSES' -Destination $portable -Recurse -Force
New-Item -ItemType Directory -Path 'dist\installer','dist\redist' -Force | Out-Null
Copy-Item -LiteralPath 'C:\Program Files\Microsoft Visual Studio\18\Community\VC\Redist\MSVC\14.51.36231\vc_redist.x64.exe' -Destination 'dist\redist\vc_redist.x64.exe'
$uninstall = @()
foreach ($file in Get-ChildItem -LiteralPath $portable -File -Recurse) {
    $relative = $file.FullName.Substring($portable.Length + 1)
    $uninstall += 'Delete "$INSTDIR\' + $relative + '"'
}
foreach ($dir in Get-ChildItem -LiteralPath $portable -Directory -Recurse | Sort-Object { $_.FullName.Length } -Descending) {
    $relative = $dir.FullName.Substring($portable.Length + 1)
    $uninstall += 'RMDir "$INSTDIR\' + $relative + '"'
}
$uninstall | Set-Content -LiteralPath 'installer\uninstall-files.nsh' -Encoding utf8
& 'C:\Program Files (x86)\NSIS\makensis.exe' /INPUTCHARSET UTF8 /V2 installer\pass.nsi
if ($LASTEXITCODE -ne 0) { throw 'NSIS failed.' }
Compress-Archive -Path "$portable\*" -DestinationPath 'dist\Pass-2.0-Plus-win64-portable.zip' -Force
Write-Output 'Portable ZIP and installer are ready in dist.'
