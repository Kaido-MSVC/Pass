param([switch]$SkipInstaller)
$ErrorActionPreference = 'Stop'
Set-Location -LiteralPath $PSScriptRoot
if (!(Test-Path -LiteralPath 'build\Pass.exe')) { throw 'Run build.cmd first.' }

# Version comes only from CMakeLists.txt project(VERSION).
$projectLine = Select-String -LiteralPath 'CMakeLists.txt' -Pattern '^\s*project\(PassSSL VERSION (\d+\.\d+\.\d+)' | Select-Object -First 1
if (!$projectLine) { throw 'Could not read project version from CMakeLists.txt.' }
$version = $projectLine.Matches[0].Groups[1].Value

$cache = Get-Content -LiteralPath 'build\CMakeCache.txt'
$rootLine = $cache | Where-Object { $_ -match '^OPENSSL_ROOT_DIR:PATH=' }
if (!$rootLine) { throw 'OpenSSL root missing from configured build.' }
$opensslRoot = $rootLine.Substring($rootLine.IndexOf('=') + 1)
$crypto = Join-Path $opensslRoot 'libcrypto-4-x64.dll'
if ((Get-FileHash $crypto).Hash -ne (Get-FileHash 'build\libcrypto-4-x64.dll').Hash) { throw 'Build/runtime OpenSSL mismatch. Reconfigure first.' }

# Locate the newest installed VC++ x64 CRT redistributable instead of a hard-coded build number.
# The app is built with MSVC 14.44; any newer 14.x CRT is backward compatible.
$vsRoot = 'C:\Program Files\Microsoft Visual Studio\18\Community'
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (Test-Path -LiteralPath $vswhere) {
    $found = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if ($LASTEXITCODE -eq 0 -and $found) { $vsRoot = @($found)[0] }
}
$redistBase = Join-Path $vsRoot 'VC\Redist\MSVC'
$crtDir = Get-ChildItem -LiteralPath $redistBase -Directory -ErrorAction Stop |
    Where-Object { $_.Name -match '^\d+\.\d+\.\d+$' -and [version]$_.Name -ge [version]'14.44.0' } |
    Sort-Object { [version]$_.Name } -Descending |
    ForEach-Object { Get-ChildItem -LiteralPath (Join-Path $_.FullName 'x64') -Directory -Filter 'Microsoft.VC*.CRT' -ErrorAction SilentlyContinue } |
    Select-Object -First 1
if (!$crtDir) { throw "No VC++ x64 CRT 14.44+ found under $redistBase." }
Write-Output "Using VC++ runtime: $($crtDir.FullName)"

$portable = Join-Path $PSScriptRoot "dist\portable-v$version"
# Recreate only the exact project-owned deployment directory, never the source tree.
$expected = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot "dist\portable-v$version"))
if ([IO.Path]::GetFullPath($portable) -ne $expected -or $expected -notlike "$PSScriptRoot\dist\*") { throw 'Unsafe deployment path.' }
$running = Get-Process -Name Pass -ErrorAction SilentlyContinue | Where-Object { $_.Path -and $_.Path.StartsWith($expected, [StringComparison]::OrdinalIgnoreCase) }
if ($running) { throw "Pass-SSL is still running from $expected (PID $($running.Id -join ', ')). Close it and run deploy.ps1 again." }
if (Test-Path -LiteralPath $portable) { Remove-Item -LiteralPath $portable -Recurse -Force }
New-Item -ItemType Directory -Path $portable -Force | Out-Null
Copy-Item -LiteralPath 'build\Pass.exe',$crypto -Destination $portable
$env:PATH = 'D:\qt\6.12.0\msvc2022_64\bin;' + $env:PATH
# Qt Widgets with the raster engine needs no software OpenGL, D3D compiler, TLS,
# network-information or touch plugins; keep only image formats the app uses (ico, svg).
& 'D:\qt\6.12.0\msvc2022_64\bin\windeployqt.exe' --release --no-translations --no-compiler-runtime `
    --no-opengl-sw --no-system-d3d-compiler `
    --skip-plugin-types tls,networkinformation,generic `
    --exclude-plugins qgif,qicns,qjpeg,qpdf,qtga,qtiff,qwbmp,qwebp `
    --dir $portable 'build\Pass.exe'
if ($LASTEXITCODE -ne 0) { throw 'windeployqt failed.' }
Copy-Item -Path (Join-Path $crtDir.FullName '*.dll') -Destination $portable
Copy-Item -LiteralPath 'README.md','THIRD_PARTY_NOTICES.md','LICENSE.txt' -Destination $portable
Copy-Item -LiteralPath 'LICENSES' -Destination $portable -Recurse -Force
Copy-Item -LiteralPath (Join-Path $opensslRoot 'license.txt') -Destination "$portable\LICENSES\OpenSSL-Apache-2.0.txt"
# default provider is compiled into libcrypto; no external provider/config is required.
@('[Paths]','Plugins=.') | Set-Content -LiteralPath "$portable\qt.conf" -Encoding ascii

# Fail before packaging if the trimmed deployment cannot start and generate.
$smoke = Start-Process -FilePath (Join-Path $portable 'Pass.exe') -ArgumentList '--smoke-test' -PassThru -Wait
if ($smoke.ExitCode -ne 0) { throw "Portable smoke test failed (exit $($smoke.ExitCode))." }

if (!$SkipInstaller) {
    New-Item -ItemType Directory -Path 'dist\installer' -Force | Out-Null
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
    & 'C:\Program Files (x86)\NSIS\makensis.exe' /INPUTCHARSET UTF8 /V2 "/DPASS_VERSION=$version" installer\pass.nsi
    if ($LASTEXITCODE -ne 0) { throw 'NSIS failed.' }
}
Compress-Archive -Path "$portable\*" -DestinationPath "dist\Pass-SSL-$version-win64-portable.zip" -Force

# Refresh this version's entries in dist\SHA256SUMS.txt; entries of other versions are kept.
$sumsPath = Join-Path $PSScriptRoot 'dist\SHA256SUMS.txt'
$artifacts = @("Pass-SSL-$version-win64-portable.zip", "portable-v$version/Pass.exe")
if (!$SkipInstaller) { $artifacts += "installer/Pass-SSL-$version-win64-setup.exe" }
$kept = @()
if (Test-Path -LiteralPath $sumsPath) {
    $kept = @([IO.File]::ReadAllLines($sumsPath) | Where-Object { $_ -and ($_ -split '\s+', 2)[1] -notin $artifacts })
}
$fresh = foreach ($name in $artifacts) {
    $file = Join-Path $PSScriptRoot ('dist\' + $name.Replace('/', '\'))
    (Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash + '  ' + $name
}
[IO.File]::WriteAllLines($sumsPath, [string[]]($kept + $fresh), (New-Object Text.UTF8Encoding($false)))
$fresh | ForEach-Object { Write-Output "SHA256 $_" }
Write-Output "Pass-SSL $version deployment is ready in dist."
