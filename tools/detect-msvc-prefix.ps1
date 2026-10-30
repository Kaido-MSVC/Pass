param([string]$BuildDirectory = 'build')
$ErrorActionPreference = 'Stop'
# Ninja compares raw /showIncludes bytes. Keep MSVC and this probe on UTF-8,
# and detect the installed compiler language instead of hard-coding English.
[Console]::OutputEncoding = New-Object System.Text.UTF8Encoding($false)
$OutputEncoding = [Console]::OutputEncoding
$taskProbeDirectory = Join-Path $BuildDirectory 'msvc-prefix-probe'
New-Item -ItemType Directory -Path $taskProbeDirectory -Force | Out-Null
$taskProbeDirectory = (Resolve-Path -LiteralPath $taskProbeDirectory).Path
$taskHeader = Join-Path $taskProbeDirectory 'pass-prefix-probe.h'
$taskSource = Join-Path $taskProbeDirectory 'pass-prefix-probe.cpp'
$taskObject = Join-Path $taskProbeDirectory 'pass-prefix-probe.obj'
[IO.File]::WriteAllText($taskHeader, "#pragma once`n", (New-Object Text.UTF8Encoding($false)))
[IO.File]::WriteAllText($taskSource, '#include "' + $taskHeader.Replace('\','/') + '"' + "`n", (New-Object Text.UTF8Encoding($false)))
$taskCompilerOutput = @(& cl.exe /nologo /c /showIncludes "/Fo$taskObject" $taskSource 2>&1)
if ($LASTEXITCODE -ne 0) { throw "MSVC prefix probe failed: $taskCompilerOutput" }
$taskLine = $taskCompilerOutput | ForEach-Object { $_.ToString().Replace('/','\') } | Where-Object { $_.Contains($taskHeader) } | Select-Object -First 1
if (!$taskLine) { throw ('MSVC prefix probe did not report its header: ' + ($taskCompilerOutput -join "`n")) }
$taskPrefix = $taskLine.Substring(0,$taskLine.IndexOf($taskHeader))
$taskEscaped = $taskPrefix.Replace('\','\\').Replace('"','\"')
$taskDefinition = 'set(CMAKE_CL_SHOWINCLUDES_PREFIX "' + $taskEscaped + '")' + "`n"
[IO.File]::WriteAllText((Join-Path $BuildDirectory 'msvc-prefix.cmake'),$taskDefinition,(New-Object Text.UTF8Encoding($false)))
Write-Output "Detected MSVC include prefix: $taskPrefix"
