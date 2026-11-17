#requires -Version 7.0
[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'

$taskRepository = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$taskSource = Join-Path $PSScriptRoot 'VillageQuestCombatCreditModelTests.cpp'
$taskHeader = Join-Path $taskRepository 'Source/Rogue10m/World/Rogue10mVillageQuestCombatCreditModel.h'
$taskVsWhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
if (-not (Test-Path -LiteralPath $taskVsWhere -PathType Leaf)) { throw 'Installed MSVC discovery tool vswhere.exe is unavailable.' }
$taskInstallation = & $taskVsWhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if ($LASTEXITCODE -ne 0 -or @($taskInstallation).Count -ne 1 -or -not $taskInstallation) { throw 'An existing x64 MSVC toolchain is required; this runner installs nothing.' }
$taskVcVars = Join-Path $taskInstallation 'VC/Auxiliary/Build/vcvars64.bat'
if (-not (Test-Path -LiteralPath $taskVcVars -PathType Leaf)) { throw 'Existing vcvars64.bat is unavailable.' }

# GetTempPath/LocalApplicationData may be a per-command disposable sandbox.
# The user-profile Temp directory retains the standalone evidence after exit.
$taskTempBoundary = [IO.Path]::TrimEndingDirectorySeparator([IO.Path]::GetFullPath((Join-Path ([Environment]::GetFolderPath([Environment+SpecialFolder]::UserProfile)) 'AppData/Local/Temp')))
if (-not (Test-Path -LiteralPath $taskTempBoundary -PathType Container)) { throw 'Persistent user-profile Temp directory is unavailable.' }
$taskOutput = [IO.Path]::GetFullPath((Join-Path $taskTempBoundary ('RogueVillageCombatCredit_' + [DateTime]::UtcNow.ToString('yyyyMMddTHHmmss') + '_' + [guid]::NewGuid().ToString('N'))))
if ([IO.Path]::GetDirectoryName($taskOutput) -cne $taskTempBoundary -or [IO.Path]::GetFileName($taskOutput) -cnotmatch '^RogueVillageCombatCredit_[0-9]{8}T[0-9]{6}_[a-f0-9]{32}$') {
    throw 'Standalone output escaped its exact temporary namespace.'
}
if (Test-Path -LiteralPath $taskOutput) { throw 'A new standalone evidence directory is required.' }
foreach ($taskPath in @($taskSource, $taskHeader, $taskVcVars, $taskOutput)) {
    if ($taskPath -match '[%"&|<>^\r\n]') { throw 'A toolchain/test path contains unsupported command-interpreter characters.' }
}
[IO.Directory]::CreateDirectory($taskOutput) | Out-Null
Write-Output ('EVIDENCE=' + $taskOutput)
$taskExecutable = Join-Path $taskOutput 'VillageQuestCombatCreditModelTests.exe'
$taskObject = Join-Path $taskOutput 'VillageQuestCombatCreditModelTests.obj'
$taskBatchPath = Join-Path $taskOutput 'run-native-test.cmd'
$taskEnvironmentLog = Join-Path $taskOutput 'environment.log'
$taskCompileLog = Join-Path $taskOutput 'compile.log'
$taskRunLog = Join-Path $taskOutput 'run.log'
$taskCompileExitPath = Join-Path $taskOutput 'compile.exit'
$taskRunExitPath = Join-Path $taskOutput 'run.exit'
$taskBatch = @"
@echo off
setlocal DisableDelayedExpansion
chcp 65001 >nul
set "VSLANG=1033"
call "$taskVcVars" > "$taskEnvironmentLog" 2>&1
if errorlevel 1 exit /b 99
setlocal DisableDelayedExpansion
cl.exe /nologo /std:c++17 /EHsc /W4 /WX /permissive- /utf-8 "$taskSource" /Fe:"$taskExecutable" /Fo:"$taskObject" > "$taskCompileLog" 2>&1
set "COMBAT_CREDIT_COMPILE_EXIT=%ERRORLEVEL%"
> "$taskCompileExitPath" echo %COMBAT_CREDIT_COMPILE_EXIT%
if not "%COMBAT_CREDIT_COMPILE_EXIT%"=="0" exit /b %COMBAT_CREDIT_COMPILE_EXIT%
"$taskExecutable" > "$taskRunLog" 2>&1
set "COMBAT_CREDIT_RUN_EXIT=%ERRORLEVEL%"
> "$taskRunExitPath" echo %COMBAT_CREDIT_RUN_EXIT%
exit /b %COMBAT_CREDIT_RUN_EXIT%
"@
[IO.File]::WriteAllText($taskBatchPath, $taskBatch.Replace("`r`n", "`n").Replace("`n", "`r`n"), [Text.UTF8Encoding]::new($false))

# vcvars and compiler environment stay inside this owned command process.
$taskStart = [Diagnostics.ProcessStartInfo]::new()
$taskStart.FileName = $env:ComSpec
$taskStart.Arguments = '/d /v:off /s /c ""' + $taskBatchPath + '""'
$taskStart.WorkingDirectory = $taskOutput
$taskStart.UseShellExecute = $false
$taskStart.CreateNoWindow = $true
$taskStart.RedirectStandardOutput = $true
$taskStart.RedirectStandardError = $true
$taskProcess = [Diagnostics.Process]::new()
$taskProcess.StartInfo = $taskStart
if (-not $taskProcess.Start()) { throw ('Standalone compiler process did not start. Evidence preserved: ' + $taskOutput) }
$taskStdout = $taskProcess.StandardOutput.ReadToEndAsync()
$taskStderr = $taskProcess.StandardError.ReadToEndAsync()
$taskProcess.WaitForExit()
$taskNativeExit = $taskProcess.ExitCode
[IO.File]::WriteAllText((Join-Path $taskOutput 'runner.stdout.log'), $taskStdout.GetAwaiter().GetResult())
[IO.File]::WriteAllText((Join-Path $taskOutput 'runner.stderr.log'), $taskStderr.GetAwaiter().GetResult())
$taskProcess.Dispose()

$taskCompileExit = if (Test-Path -LiteralPath $taskCompileExitPath) { [int]([IO.File]::ReadAllText($taskCompileExitPath).Trim()) } else { $null }
$taskRunExit = if (Test-Path -LiteralPath $taskRunExitPath) { [int]([IO.File]::ReadAllText($taskRunExitPath).Trim()) } else { $null }
$taskRunText = if (Test-Path -LiteralPath $taskRunLog) { [IO.File]::ReadAllText($taskRunLog) } else { '' }
$taskMatches = [regex]::Matches($taskRunText, '(?m)^RESULT=VILLAGE_COMBAT_CREDIT_MODEL_(PASSED|FAILED) checks=([1-9][0-9]*) failures=([0-9]+) scenarios=9 mixed_traces=7776\r?$')
$taskPassed = $taskNativeExit -eq 0 -and $taskCompileExit -eq 0 -and $taskRunExit -eq 0 -and $taskMatches.Count -eq 1 -and $taskMatches[0].Groups[1].Value -ceq 'PASSED' -and $taskMatches[0].Groups[3].Value -ceq '0' -and $taskRunText -notmatch '(?m)^FAIL '
$taskReport = [ordered]@{
    status = $(if ($taskPassed) { 'passed' } else { 'failed' })
    checks = $(if ($taskMatches.Count -eq 1) { [int]$taskMatches[0].Groups[2].Value } else { $null })
    failures = $(if ($taskMatches.Count -eq 1) { [int]$taskMatches[0].Groups[3].Value } else { $null })
    scenarios = 9
    mixed_traces = 7776
    compiler_exit = $taskCompileExit
    test_exit = $taskRunExit
    native_exit = $taskNativeExit
    source_sha256 = (Get-FileHash -LiteralPath $taskSource -Algorithm SHA256).Hash
    header_sha256 = (Get-FileHash -LiteralPath $taskHeader -Algorithm SHA256).Hash
    toolchain = $taskInstallation
    command_file = $taskBatchPath
    compile_log = $taskCompileLog
    run_log = $taskRunLog
    executable = $taskExecutable
    unreal_executed = $false
    failure_artifacts_preserved = $true
}
$taskJson = $taskReport | ConvertTo-Json -Depth 4
[IO.File]::WriteAllText((Join-Path $taskOutput 'result.json'), $taskJson + "`r`n", [Text.UTF8Encoding]::new($false))
Write-Output $taskJson
if (-not $taskPassed) { throw ('Standalone combat-credit test failed. Inspect preserved evidence: ' + $taskOutput) }
