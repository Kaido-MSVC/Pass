param([string]$TestDirectory = '')
$ErrorActionPreference = 'Stop'
$project = Split-Path -Parent $PSScriptRoot
if (!$TestDirectory) { $TestDirectory = Join-Path $project ('build\installer-regression-' + [guid]::NewGuid().ToString('N')) }
if (Test-Path -LiteralPath $TestDirectory) { throw 'Use a new test directory.' }
New-Item -ItemType Directory -Path $TestDirectory | Out-Null
$TestDirectory = (Resolve-Path -LiteralPath $TestDirectory).Path
$version = [regex]::Match([IO.File]::ReadAllText("$project\CMakeLists.txt"), 'project\(PassSSL VERSION (\d+\.\d+\.\d+)').Groups[1].Value
$payload = "$project\dist\portable-v$version"
$nsis = 'C:\Program Files (x86)\NSIS\makensis.exe'
$key = 'Software\PassSSL-Installer-Test-' + [guid]::NewGuid().ToString('N')
$reg = 'HKCU:\' + $key
$results = [Collections.Generic.List[object]]::new()
function Check($name, $condition) {
    if (!$condition) { throw "FAIL: $name" }
    $results.Add([pscustomobject]@{name=$name; passed=$true})
    Write-Output "PASS: $name"
}
function CompileFixture([string]$source, [string]$name, [string]$fixtureVersion) {
    # Only authority and side-effect destinations change. The production sections,
    # file operations, error handling and /D parsing are compiled without changes.
    $source = $source.Replace('RequestExecutionLevel admin', 'RequestExecutionLevel user')
    $source = $source.Replace('SetShellVarContext all', 'SetShellVarContext current')
    $source = $source.Replace('WriteRegStr HKLM', 'WriteRegStr HKCU').Replace('WriteRegDWORD HKLM', 'WriteRegDWORD HKCU').Replace('DeleteRegKey HKLM', 'DeleteRegKey HKCU')
    $source = $source.Replace('Software\Microsoft\Windows\CurrentVersion\Uninstall\Pass-SSLLocal', $key)
    $source = $source.Replace('$SMPROGRAMS', "$TestDirectory\shortcuts").Replace('$DESKTOP', "$TestDirectory\desktop")
    $source = $source.Replace('$PROGRAMFILES64\Pass-SSL', "$TestDirectory\default\Pass-SSL")
    $source = [regex]::Replace($source, '(?m)^OutFile .+$', 'OutFile "' + $TestDirectory + '\' + $name + '.exe"')
    $source = $source.Replace('..\dist\portable-v${PASS_VERSION}', "$project\dist\portable-v" + '${PASS_VERSION}')
    $source = $source.Replace('..\resources\app.ico', "$project\resources\app.ico").Replace('..\LICENSE.txt', "$project\LICENSE.txt")
    [IO.File]::WriteAllText("$TestDirectory\$name.nsi", $source, [Text.UTF8Encoding]::new($false))
    & $nsis /INPUTCHARSET UTF8 /V2 "/DPASS_VERSION=$fixtureVersion" "$TestDirectory\$name.nsi" *> "$TestDirectory\$name-build.log"
    if ($LASTEXITCODE -ne 0) { throw "Fixture compilation failed: $name" }
    return "$TestDirectory\$name.exe"
}
function Install($exe, $target) {
    $arguments = '/S'
    if ($target) { $arguments += ' /D=' + $target }
    return (Start-Process -FilePath $exe -ArgumentList $arguments -WindowStyle Hidden -PassThru -Wait).ExitCode
}
function CheckPayload($target) {
    $files = @(Get-ChildItem -LiteralPath $payload -Recurse -File)
    foreach ($file in $files) {
        $destination = Join-Path $target $file.FullName.Substring($payload.Length + 1)
        if (!(Test-Path -LiteralPath $destination) -or (Get-FileHash -LiteralPath $destination).Hash -ne (Get-FileHash -LiteralPath $file.FullName).Hash) { throw "Payload mismatch: $destination" }
    }
    Check "All $($files.Count) installed payload hashes match" $true
    Check 'Version registry updated' ((Get-ItemProperty -LiteralPath $reg).DisplayVersion -eq $version)
    Check 'Quoted uninstall command uses selected path' ((Get-ItemProperty -LiteralPath $reg).UninstallString -eq ('"' + $target + '\Uninstall.exe"'))
}
function Uninstall($target) {
    # Normal NSIS uninstall uses a temporary copy. Wait for its observable result.
    $process = Start-Process -FilePath "$target\Uninstall.exe" -ArgumentList '/S' -WindowStyle Hidden -PassThru -Wait
    if ($process.ExitCode -ne 0) { throw 'Uninstaller launcher failed.' }
    $deadline = [DateTime]::UtcNow.AddSeconds(20)
    while ((Test-Path -LiteralPath $reg) -and [DateTime]::UtcNow -lt $deadline) { Start-Sleep -Milliseconds 100 }
    Check 'Uninstall removes test registration' (!(Test-Path -LiteralPath $reg))
    Check 'Uninstall removes owned executable' (!(Test-Path -LiteralPath "$target\Pass.exe"))
    Check 'Uninstall removes itself' (!(Test-Path -LiteralPath "$target\Uninstall.exe"))
    Check 'Uninstall removes shortcuts' (!(Test-Path -LiteralPath "$TestDirectory\shortcuts\Pass-SSL\Pass-SSL.lnk") -and !(Test-Path -LiteralPath "$TestDirectory\desktop\Pass-SSL.lnk"))
}
try {
    Copy-Item -LiteralPath "$project\installer\uninstall-files.nsh", "$project\installer\legacy-files.nsh" -Destination $TestDirectory
    New-Item -ItemType Directory -Path "$TestDirectory\desktop" | Out-Null
    $source = [IO.File]::ReadAllText("$project\installer\pass.nsi")
    Check 'Production requests administrator' ($source.Contains('RequestExecutionLevel admin'))
    Check 'Production never launches existing code' ($source -notmatch '(?im)^\s*(Exec|ExecWait|ExecShell|ExecShellWait|nsExec::|System::Call)\b')
    Check 'No recursive or reboot-deferred deletion' ($source -notmatch '(?im)^\s*(RMDir\s+/r|Delete\s+/REBOOTOK)\b')
    $new = CompileFixture $source 'new-setup' $version
    $oldSource = (& git -C $project show aeb4b00:installer/pass.nsi) -join "`r`n"
    if ($LASTEXITCODE -ne 0) { throw 'Cannot read v2.2.5 installer source.' }
    $old = CompileFixture $oldSource 'old-setup' '2.2.5'
    # Default-path fresh install and normal uninstall.
    $default = "$TestDirectory\default\Pass-SSL"
    Check 'Fresh default install exits zero' ((Install $new '') -eq 0)
    CheckPayload $default
    Uninstall $default
    Check 'Empty default install directory removed' (!(Test-Path -LiteralPath $default))
    # A genuine old install, /D with spaces and non-ASCII, and a benign replacement.
    $custom = "$TestDirectory\custom path $([char]0x81ea)$([char]0x8a02)\Pass-SSL"
    Check 'v2.2.5 custom install exits zero' ((Install $old $custom) -eq 0)
    Check 'v2.2.5 installed' ((Get-ItemProperty -LiteralPath $reg).DisplayVersion -eq '2.2.5')
    Check 'Normal v2.2.5 to current upgrade exits zero' ((Install $new $custom) -eq 0)
    CheckPayload $custom
    Uninstall $custom
    Check 'v2.2.5 reinstall for replacement test exits zero' ((Install $old $custom) -eq 0)
    $marker = "$TestDirectory\old-uninstaller-was-executed.txt"
    $sentinel = @'
Unicode true
RequestExecutionLevel user
SilentInstall silent
OutFile "SENTINEL_EXE"
Section
 FileOpen $0 "MARKER_PATH" w
 FileWrite $0 "benign sentinel executed"
 FileClose $0
SectionEnd
'@
    $sentinel = $sentinel.Replace('SENTINEL_EXE', "$TestDirectory\sentinel.exe").Replace('MARKER_PATH', $marker)
    [IO.File]::WriteAllText("$TestDirectory\sentinel.nsi", $sentinel)
    & $nsis /INPUTCHARSET UTF8 /V2 "$TestDirectory\sentinel.nsi" *> "$TestDirectory\sentinel-build.log"
    if ($LASTEXITCODE -ne 0) { throw 'Sentinel compilation failed.' }
    # Positive control: prove that execution of the harmless replacement is observable.
    Check 'Sentinel positive control exits zero' ((Install "$TestDirectory\sentinel.exe" '') -eq 0)
    Check 'Sentinel positive control produces marker' (Test-Path -LiteralPath $marker)
    Remove-Item -LiteralPath $marker
    Copy-Item -LiteralPath "$TestDirectory\sentinel.exe" -Destination "$custom\Uninstall.exe" -Force
    $unknown = "$custom\my-notes.txt"
    [IO.File]::WriteAllText($unknown, 'public test data to preserve')
    $legacy = [regex]::Matches([IO.File]::ReadAllText("$project\installer\legacy-files.nsh"), 'Delete "\$INSTDIR\\([^"\r\n]+)"') | ForEach-Object {$_.Groups[1].Value}
    foreach ($relative in $legacy) {
        $file = Join-Path $custom $relative
        New-Item -ItemType Directory -Path (Split-Path -Parent $file) -Force | Out-Null
        [IO.File]::WriteAllText($file, 'public obsolete-file fixture')
    }
    [IO.File]::WriteAllText("$custom\tls\keep.txt", 'public extra file')
    # Common error: a locked old uninstaller must fail before touching the payload.
    $lock = [IO.File]::Open("$custom\Uninstall.exe", 'Open', 'Read', 'None')
    try { Check 'Locked old uninstaller fails safely' ((Install $new $custom) -ne 0) } finally { $lock.Dispose() }
    Check 'Failure preserves old version registration' ((Get-ItemProperty -LiteralPath $reg).DisplayVersion -eq '2.2.5')
    Check 'Failure never executes old uninstaller' (!(Test-Path -LiteralPath $marker))
    Check 'Custom upgrade exits zero' ((Install $new $custom) -eq 0)
    CheckPayload $custom
    Check 'Replacement uninstaller was never executed' (!(Test-Path -LiteralPath $marker))
    Check 'Every known obsolete file removed' (@($legacy | Where-Object {Test-Path -LiteralPath (Join-Path $custom $_)}).Count -eq 0)
    Check 'Unknown root and plugin files preserved' ((Test-Path -LiteralPath $unknown) -and (Test-Path -LiteralPath "$custom\tls\keep.txt"))
    Check 'Installed app smoke succeeds' ((Start-Process -FilePath "$custom\Pass.exe" -ArgumentList '--smoke-test' -WindowStyle Hidden -PassThru -Wait).ExitCode -eq 0)
    Uninstall $custom
    Check 'Unknown files survive uninstall' (([IO.File]::ReadAllText($unknown) -eq 'public test data to preserve') -and (Test-Path -LiteralPath "$custom\tls\keep.txt"))
    Check 'No stale payload DLLs after uninstall' (@(Get-ChildItem -LiteralPath $custom -Recurse -Filter '*.dll').Count -eq 0)
    $results | ConvertTo-Json | Set-Content -LiteralPath "$TestDirectory\results.json" -Encoding UTF8
    Write-Output "Installer regression passed: $($results.Count) checks. Evidence: $TestDirectory"
} finally {
    # Never touch the real HKLM registration, shared shortcuts or installed product.
    if (Test-Path -LiteralPath $reg) { Remove-Item -LiteralPath $reg -Recurse }
}
