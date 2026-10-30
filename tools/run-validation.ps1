param([string]$ReportDirectory = '', [switch]$SkipStress, [string]$ReuseStressReport = '', [switch]$CollectOnly)
$ErrorActionPreference = 'Stop'
$project = Split-Path -Parent $PSScriptRoot
$versionLine = Select-String -LiteralPath (Join-Path $project 'CMakeLists.txt') -Pattern '^\s*project\(PassSSL VERSION (\d+\.\d+\.\d+)' | Select-Object -First 1
if (!$versionLine) { throw 'Could not read project version from CMakeLists.txt.' }
$version = $versionLine.Matches[0].Groups[1].Value
if (!$ReportDirectory) { $ReportDirectory = Join-Path $project ('validation\' + (Get-Date -Format 'yyyyMMdd-HHmmss')) }
New-Item -ItemType Directory -Path $ReportDirectory -Force | Out-Null
$build = Join-Path $project 'build'
$ctest = 'D:\qt\Tools\CMake_64\bin\ctest.exe'
$env:PATH = 'D:\qt\6.12.0\msvc2022_64\bin;' + $env:PATH
# Raw test diagnostics can contain failed comparisons: retain only in build.
if($CollectOnly){
    $last=Get-Item -LiteralPath (Join-Path $build 'Testing\Temporary\LastTest.log')
    $newest=Get-ChildItem -LiteralPath $build -Filter '*.exe' | Sort-Object LastWriteTime -Descending | Select-Object -First 1
    if($last.LastWriteTime -lt $newest.LastWriteTime){throw 'Test results predate current binaries; run validation first.'}
    $lastText=Get-Content -LiteralPath $last.FullName -Raw
    $groups=@([regex]::Matches($lastText,'(?s)\d+/\d+ Testing: (\w+).*?Test time =\s*([\d.]+) sec.*?Test (Passed|Failed)\.') | ForEach-Object {
        [pscustomobject]@{name=$_.Groups[1].Value;seconds=$_.Groups[2].Value;passed=($_.Groups[3].Value -eq 'Passed')}
    })
    $testExit=0;if($groups.Count -ne 11 -or @($groups | Where-Object {!$_.passed}).Count){$testExit=1}
    $securityOutput=$lastText
}else{
    $ErrorActionPreference='Continue' # Native stderr must not bypass failure-report generation on Windows PowerShell.
    & $ctest --test-dir $build --test-output-size-passed 1048576 --test-output-size-failed 1048576 --output-junit (Join-Path $build 'validation-raw.xml') *> (Join-Path $build 'validation-raw.log')
    $testExit = $LASTEXITCODE
    $ErrorActionPreference='Stop'
    [xml]$xml = Get-Content -LiteralPath (Join-Path $build 'validation-raw.xml') -Raw
    $groups = @($xml.testsuite.testcase | ForEach-Object {
        [pscustomobject]@{name=$_.name; seconds=$_.time; passed=($_.status -eq 'run' -and !$_.failure)}
    })
    $security=$xml.testsuite.testcase | Where-Object {$_.name -eq 'security_audit'}
    $securityOutput=$security.'system-out'
}
$cases = @()
foreach($log in Get-ChildItem -LiteralPath $build -Filter '*-results.txt') {
    foreach($line in Get-Content -LiteralPath $log.FullName) {
        if($line -match '^(PASS|FAIL!|SKIP)\s*:\s*([A-Za-z0-9_]+)::([A-Za-z0-9_]+)(\([^\)\r\n]*\))') {
            if($Matches[3] -notin @('initTestCase','cleanupTestCase')) {
                $cases += [pscustomobject]@{suite=$Matches[2];name=($Matches[3]+$Matches[4]);status=$Matches[1]}
            }
        }
    }
}
$baseline = Get-Content -LiteralPath (Join-Path $PSScriptRoot 'baseline-87.json') -Raw | ConvertFrom-Json
if($baseline.Count -ne 87){throw 'Invalid baseline inventory.'}
foreach($line in ($securityOutput -split "`n")){
    if($line -match '^(PASS|FAIL)\s+(.+)$'){
        $status=$Matches[1];$diagnostic=$Matches[2].Trim()
        foreach($known in ($baseline | Where-Object {$_.suite -eq 'SecurityAudit'})){
            if($diagnostic -eq $known.name -or $diagnostic.StartsWith($known.name+':')){
                $cases += [pscustomobject]@{suite='SecurityAudit';name=$known.name;status=$status};break
            }
        }
    }
}
$stressExit = $null
if($cases.Count -eq 0){throw 'No case results found.'}
$regression = @($baseline | ForEach-Object {
    $expectedCase=$_
    $actual=@($cases | Where-Object { $_.suite -eq $expectedCase.suite -and $_.name -eq $expectedCase.name })
    [pscustomobject]@{suite=$expectedCase.suite;name=$expectedCase.name;passed=($actual.Count -eq 1 -and $actual[0].status -eq 'PASS')}
})
if(@($regression | Where-Object {!$_.passed}).Count -gt 0){$testExit=1}
$inputs=[ordered]@{}
foreach($file in @('src/password.cpp','src/password.h','src/passphrase.cpp','src/passphrase.h','src/secure_random.cpp','src/secure_random.h','src/secret_text.h','src/random_algorithm.h','src/wordlist.cpp','src/wordlist.h','resources/wordlists.qrc','resources/wordlists/eff_large_wordlist.txt','tests/stress_test.cpp')){
    $inputs[$file]=(Get-FileHash -LiteralPath (Join-Path $project $file) -Algorithm SHA256).Hash
}
if($ReuseStressReport){
    $prior=Get-Content -LiteralPath $ReuseStressReport -Raw | ConvertFrom-Json
    if(!$prior.input_sha256 -or !$prior.passed){throw 'Reusable stress report lacks passing source provenance.'}
    foreach($file in $inputs.Keys){if($prior.input_sha256.$file -ne $inputs[$file]){throw ('Stress source differs: '+$file)}}
    $destination=Join-Path $ReportDirectory 'stress.json'
    if([IO.Path]::GetFullPath($destination) -ne [IO.Path]::GetFullPath($ReuseStressReport)){Copy-Item -LiteralPath $ReuseStressReport -Destination $destination -Force}
    $stressExit=0
}elseif(!$SkipStress){
    $stressFile=Join-Path $ReportDirectory 'stress.json'
    & (Join-Path $build 'pass_stress_tests.exe') $stressFile;$stressExit=$LASTEXITCODE
    $stress=Get-Content -LiteralPath $stressFile -Raw | ConvertFrom-Json
    $stress | Add-Member -NotePropertyName input_sha256 -NotePropertyValue $inputs -Force
    $stress | Add-Member -NotePropertyName executable_sha256 -NotePropertyValue (Get-FileHash -LiteralPath (Join-Path $build 'pass_stress_tests.exe')).Hash -Force
    $stress | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $stressFile -Encoding utf8
}
$report = [ordered]@{schema_version=1;version=$version;generated_at=(Get-Date -Format o);project=$project;collected_existing_tests=[bool]$CollectOnly;ctest_exit=$testExit;groups=$groups;cases=$cases;baseline_87=$regression;stress_skipped=[bool]($SkipStress -and !$ReuseStressReport);stress_reused=[bool]$ReuseStressReport;stress_exit=$stressExit;contains_passwords=$false}
$report | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $ReportDirectory 'tests.json') -Encoding utf8
$summary=@("# Pass-SSL v$version automated validation", '', ('CTest exit: '+$testExit), ('Test cases: '+$cases.Count), ('Stress skipped: '+[bool]$SkipStress), '', 'Only test names, status, duration and aggregate counts are included. No generated passwords or raw failure comparisons are copied.', '', '| Group | Passed | Seconds |','|---|---|---|')
foreach($group in $groups){$summary += "| $($group.name) | $($group.passed) | $($group.seconds) |"}
$summary | Set-Content -LiteralPath (Join-Path $ReportDirectory 'summary.md') -Encoding utf8
if($testExit -ne 0 -or (!$SkipStress -and $stressExit -ne 0)){throw 'Validation failed; see sanitized report and local build diagnostics.'}
Write-Output "Validation report: $ReportDirectory"
