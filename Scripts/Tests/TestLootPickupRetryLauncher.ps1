#requires -Version 7.0
[CmdletBinding()]
param(
    [string]$Launcher = (Join-Path (Split-Path -Parent $PSScriptRoot) 'VerifyLootPickupRetry.ps1'),
    [Parameter(Mandatory)][string]$EvidenceRoot,
    [string]$RetainedSuiteRoot
)
$ErrorActionPreference = 'Stop'
$Launcher = (Resolve-Path -LiteralPath $Launcher).Path
$EvidenceRoot = [IO.Path]::GetFullPath($EvidenceRoot)
if (Test-Path -LiteralPath $EvidenceRoot) { throw 'Use a fresh test evidence directory.' }
New-Item -ItemType Directory -Path $EvidenceRoot | Out-Null
if (Get-Variable -Name RogueLootLauncherTestState -Scope Global -ErrorAction SilentlyContinue) { throw 'A launcher test is already using the global fake-process state.' }
$global:RogueLootLauncherTestState = [pscustomobject]@{Simulation=$null;FakeProcessCalls=0}
$global:RogueLootLauncherTestState.FakeProcessCalls = 0

# These functions shadow the operating-system commands for the entire child
# script scope. No engine executable, real process inventory, or user save is used.
function Get-CimInstance {
    param([string]$ClassName)
    if ($ClassName -ne 'Win32_Process') { throw 'Unexpected CIM query.' }
    return $global:RogueLootLauncherTestState.Simulation.Processes
}
function Start-Process {
    param([string]$FilePath, [string[]]$ArgumentList, [string]$WorkingDirectory, [string]$WindowStyle, [switch]$PassThru)
    $global:RogueLootLauncherTestState.FakeProcessCalls++
    if ($FilePath -notlike '*UnrealEditor-Cmd.exe') { throw 'Unexpected simulated executable.' }
    $suite = if ($global:RogueLootLauncherTestState.Simulation.FullSuite) { 'Rogue10m.MonsterLoot' } else { 'Rogue10m.MonsterLoot.OverlapRetry' }
    $names = if ($global:RogueLootLauncherTestState.Simulation.FullSuite) {
        @('TableBoundariesAndValidation','SeededRollsAndUniqueInstances','ActualDeathExactlyOnce','PickupFailureRetryAndPersistence','TestAndAuthorityIsolation',
          'OverlapRetry.StationaryCapacityAndTransaction','OverlapRetry.BagExpansion','OverlapRetry.LeaveAndDestroyCleanup')
    } else { @('StationaryCapacityAndTransaction','BagExpansion','LeaveAndDestroyCleanup') }
    $tests = @($names | ForEach-Object { [pscustomobject]@{fullTestPath=($suite+'.'+$_);state='Success';errors=0;warnings=0} })
    if ($global:RogueLootLauncherTestState.Simulation.Mode -eq 'report-warning') { $tests[0].warnings = 1 }
    if ($global:RogueLootLauncherTestState.Simulation.Mode -eq 'report-error') { $tests[0].errors = 1 }
    if ($global:RogueLootLauncherTestState.Simulation.Mode -eq 'missing-test') { $tests = @($tests | Select-Object -Skip 1) }
    if ($global:RogueLootLauncherTestState.Simulation.Mode -eq 'duplicate-test') { $tests[1].fullTestPath = $tests[0].fullTestPath }
    $report = [ordered]@{succeeded=$names.Count;succeededWithWarnings=0;failed=0;notRun=0;inProcess=0;tests=$tests}
    $reportArgument = @($ArgumentList | Where-Object { $_.StartsWith('-ReportExportPath=') })[0]
    $reportRoot = $reportArgument.Substring('-ReportExportPath='.Length).Trim('"')
    New-Item -ItemType Directory -Path $reportRoot -Force | Out-Null
    if ($global:RogueLootLauncherTestState.Simulation.Mode -eq 'retained-suite') {
        $report = Get-Content -LiteralPath (Join-Path $global:RogueLootLauncherTestState.Simulation.RetainedRoot 'report\index.json') -Raw | ConvertFrom-Json
    }
    $report | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath (Join-Path $reportRoot 'index.json') -Encoding utf8
    $logArgument = @($ArgumentList | Where-Object { $_.StartsWith('-abslog=') })[0]
    $logPath = $logArgument.Substring('-abslog='.Length).Trim('"')
    $lines = @('LogEngine: Initializing Engine...')
    if ($global:RogueLootLauncherTestState.Simulation.Mode -eq 'startup-self-test') { $lines = @('LogAutomationTest: Error: Condition failed') + $lines }
    if ($global:RogueLootLauncherTestState.Simulation.Mode -ne 'missing-start') {
        $lines += ('LogAutomationController: Display: Test Started. Name={Synthetic} Path={'+$suite+'.'+$names[0]+'}')
    }
    if ($global:RogueLootLauncherTestState.Simulation.Mode -eq 'engine-error') { $lines += 'LogRogue10m: Error: synthetic inventory failure' }
    if ($global:RogueLootLauncherTestState.Simulation.Mode -eq 'engine-ensure') { $lines += 'LogOutputDevice: Error: Ensure condition failed: Synthetic' }
    if ($global:RogueLootLauncherTestState.Simulation.Mode -eq 'python-error') { $lines += 'Traceback (most recent call last):' }
    if ($global:RogueLootLauncherTestState.Simulation.Mode -ne 'missing-queue') { $lines += ('LogAutomationCommandLine: Display: ...Automation Test Queue Empty '+$names.Count+' tests performed.') }
    if ($global:RogueLootLauncherTestState.Simulation.Mode -eq 'retained-suite') {
        $lines = [IO.File]::ReadAllLines((Join-Path $global:RogueLootLauncherTestState.Simulation.RetainedRoot 'automation.log'))
    }
    $lines | Set-Content -LiteralPath $logPath -Encoding utf8
    $saveRoot = Join-Path $WorkingDirectory 'Saved\SaveGames'
    switch ($global:RogueLootLauncherTestState.Simulation.Mode) {
        'new-owned' { 'qa-only' | Set-Content -LiteralPath (Join-Path $saveRoot 'Rogue10m_Town_QA_LootRetry_CaseA.sav') }
        'new-nested' {
            New-Item -ItemType Directory -Path (Join-Path $saveRoot 'nested') | Out-Null
            'nested' | Set-Content -LiteralPath (Join-Path $saveRoot 'nested\Rogue10m_Town_QA_LootRetry_CaseA.sav')
        }
        'new-nonsave' { 'unrelated' | Set-Content -LiteralPath (Join-Path $saveRoot 'unrelated.tmp') }
        'new-hidden' {
            $hidden = Join-Path $saveRoot 'hidden.tmp'
            [IO.File]::WriteAllText($hidden, 'new hidden file')
            [IO.File]::SetAttributes($hidden, [IO.FileAttributes]::Hidden)
        }
        'mutate-hidden' { [IO.File]::WriteAllText((Join-Path $saveRoot 'HiddenProfile.sav'), 'changed hidden original') }
        'mutate-original' { 'changed' | Set-Content -LiteralPath (Join-Path $saveRoot 'UserProfile.sav') }
        'delete-original' { Remove-Item -LiteralPath (Join-Path $saveRoot 'UserProfile.sav') }
    }
    $process = [pscustomobject]@{ Id=-999; HasExited=$true; ExitCode=0 }
    $process | Add-Member ScriptMethod WaitForExit { param($Milliseconds) return $true }
    $process | Add-Member ScriptMethod Refresh { }
    $process | Add-Member ScriptMethod Kill { throw 'A completed fake process must never be killed.' }
    return $process
}
if ((Get-Command Start-Process).CommandType -ne 'Function' -or (Get-Command Get-CimInstance).CommandType -ne 'Function') { throw 'Required process interception is absent.' }

$cases = @(
    @{name='plan-only';mode='clean';execute=$false;accept=$true;calls=0},
    @{name='plan-full';mode='clean';execute=$false;full=$true;accept=$true;calls=0},
    @{name='clean-retry';mode='clean';accept=$true;calls=1},
    @{name='clean-full';mode='clean';full=$true;accept=$true;calls=1},
    @{name='active-exact';mode='clean';process='exact';accept=$false;calls=0},
    @{name='active-case-slash';mode='clean';process='case-slash';accept=$false;calls=0},
    @{name='active-relative';mode='clean';process='relative';accept=$false;calls=0},
    @{name='active-uninspectable';mode='clean';process='unknown';accept=$false;calls=0},
    @{name='unrelated-editor';mode='clean';process='unrelated';accept=$true;calls=1},
    @{name='existing-slot-case';mode='clean';existing='qa-case';accept=$false;calls=0},
    @{name='fresh-evidence';mode='clean';existing='evidence';accept=$false;calls=0},
    @{name='new-owned-save';mode='new-owned';accept=$true;calls=1},
    @{name='new-nested-save';mode='new-nested';accept=$false;calls=1},
    @{name='new-unrelated-file';mode='new-nonsave';accept=$false;calls=1},
    @{name='new-hidden-file';mode='new-hidden';accept=$false;calls=1},
    @{name='mutated-hidden-original';mode='mutate-hidden';existing='hidden-user';accept=$false;calls=1},
    @{name='mutated-original';mode='mutate-original';existing='user';accept=$false;calls=1},
    @{name='deleted-original';mode='delete-original';existing='user';accept=$false;calls=1},
    @{name='report-warning';mode='report-warning';accept=$false;calls=1},
    @{name='report-error';mode='report-error';accept=$false;calls=1},
    @{name='missing-test';mode='missing-test';accept=$false;calls=1},
    @{name='duplicate-test';mode='duplicate-test';accept=$false;calls=1},
    @{name='engine-error-after-start';mode='engine-error';accept=$false;calls=1},
    @{name='ensure-after-start';mode='engine-ensure';accept=$false;calls=1},
    @{name='python-error-after-start';mode='python-error';accept=$false;calls=1},
    @{name='startup-self-test-recorded';mode='startup-self-test';accept=$true;calls=1},
    @{name='missing-log-start';mode='missing-start';accept=$false;calls=1},
    @{name='missing-queue-marker';mode='missing-queue';accept=$false;calls=1}
)
if ($RetainedSuiteRoot) {
    $RetainedSuiteRoot = (Resolve-Path -LiteralPath $RetainedSuiteRoot).Path
    $cases += @{name='retained-eight-suite-log-replay';mode='retained-suite';full=$true;accept=$true;calls=1}
}
$results = @()
foreach ($case in $cases) {
    $caseRoot = Join-Path $EvidenceRoot $case.name
    New-Item -ItemType Directory -Path (Join-Path $caseRoot 'Content'),(Join-Path $caseRoot 'Saved\SaveGames') -Force | Out-Null
    '{"FileVersion":3}' | Set-Content -LiteralPath (Join-Path $caseRoot 'Rogue10m.uproject') -Encoding ascii
    $project = Join-Path $caseRoot 'Rogue10m.uproject'
    $processes = @()
    if ($case.process) {
        $commandLine = switch ($case.process) {
            'exact' { 'UnrealEditor.exe "'+$project+'"' }
            'case-slash' { 'UnrealEditor.exe "'+$project.ToUpperInvariant().Replace('\','/')+'"' }
            'relative' { 'UnrealEditor.exe Rogue10m.uproject' }
            'unknown' { $null }
            'unrelated' { 'UnrealEditor.exe "C:\Other\Another.uproject"' }
        }
        $processes = @([pscustomobject]@{Name='UnrealEditor.exe';CommandLine=$commandLine})
    }
    if ($case.existing -eq 'qa-case') { 'existing' | Set-Content -LiteralPath (Join-Path $caseRoot 'Saved\SaveGames\ROGUE10M_TOWN_QA_LOOTRETRY_CASEA.SAV') }
    if ($case.existing -eq 'user') { 'original' | Set-Content -LiteralPath (Join-Path $caseRoot 'Saved\SaveGames\UserProfile.sav') }
    if ($case.existing -eq 'hidden-user') {
        $hidden = Join-Path $caseRoot 'Saved\SaveGames\HiddenProfile.sav'
        [IO.File]::WriteAllText($hidden, 'hidden original')
        [IO.File]::SetAttributes($hidden, [IO.FileAttributes]::Hidden)
    }
    if ($case.existing -eq 'evidence') { New-Item -ItemType Directory -Path (Join-Path $caseRoot 'Feature\doc\evidence\monster-loot-pickup-retry-20261110\CaseA') -Force | Out-Null }
    $global:RogueLootLauncherTestState.Simulation = [pscustomobject]@{Mode=$case.mode;FullSuite=[bool]$case.full;Processes=$processes;RetainedRoot=$RetainedSuiteRoot}
    $global:RogueLootLauncherTestState.FakeProcessCalls = 0
    $accepted = $true; $message = $null; $output = @()
    try {
        $output = @(& $Launcher -Label CaseA -ProjectRoot $caseRoot -FullSuite:([bool]$case.full) -Execute:($case.execute -ne $false))
    } catch { $accepted = $false; $message = $_.ToString() }
    $passed = $accepted -eq $case.accept -and $global:RogueLootLauncherTestState.FakeProcessCalls -eq $case.calls
    if ($case.name -like 'plan-*' -and $accepted) {
        $plan = ($output -join "`n") | ConvertFrom-Json
        $expectedCount = if ($case.full) { 8 } else { 3 }
        $passed = $passed -and $plan.executed -eq $false -and $plan.expected.Count -eq $expectedCount -and -not (Test-Path -LiteralPath (Join-Path $caseRoot 'Feature'))
    }
    if ($case.name -eq 'startup-self-test-recorded' -and $accepted) {
        $journal = Get-Content -LiteralPath (Join-Path $caseRoot 'Feature\doc\evidence\monster-loot-pickup-retry-20261110\CaseA\launcher.json') -Raw | ConvertFrom-Json
        $passed = $passed -and $journal.preTestRedLogCount -eq 1 -and $journal.testPhaseRedLogCount -eq 0
    }
    $results += [pscustomobject]@{name=$case.name;passed=$passed;accepted=$accepted;expectedAccepted=$case.accept;fakeProcessCalls=$global:RogueLootLauncherTestState.FakeProcessCalls;expectedCalls=$case.calls;message=$message}
}
$failed = @($results | Where-Object { -not $_.passed })
[pscustomobject]@{checks=$results.Count;passed=$results.Count-$failed.Count;failed=$failed.Count;realEngineProcessesLaunched=0;fixtures='Synthetic reports, fake process methods, and isolated dummy files; no UE gameplay execution';cases=$results} | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $EvidenceRoot 'results.json') -Encoding utf8
$results | Select-Object name,passed,accepted,fakeProcessCalls | Format-Table -AutoSize
Remove-Variable -Name RogueLootLauncherTestState -Scope Global
if ($failed.Count) { throw ('Launcher behavior regressions: '+$failed.Count+' / '+$results.Count+'. Inspect '+(Join-Path $EvidenceRoot 'results.json')) }
Write-Output ('RESULT=LOOT_QA_LAUNCHER_TESTS_PASSED checks='+$results.Count+' realUE=0')
