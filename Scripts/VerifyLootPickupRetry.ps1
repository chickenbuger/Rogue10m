#requires -Version 7.0
[CmdletBinding()]
param(
    [Parameter(Mandatory)][ValidatePattern('^[A-Za-z0-9_]{1,20}$')][string]$Label,
    [string]$ProjectRoot = (Split-Path -Parent $PSScriptRoot),
    [ValidateRange(60,600)][int]$TimeoutSeconds = 240,
    [switch]$FullSuite,
    [switch]$Execute
)
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path -LiteralPath $ProjectRoot).Path
$project = (Resolve-Path -LiteralPath (Join-Path $root 'Rogue10m.uproject')).Path
$editor = 'D:\Program Files\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$out = Join-Path $root ('Feature\doc\evidence\monster-loot-pickup-retry-20261110\' + $Label)
$tag = 'LootRetry_' + $Label
$town = 'Rogue10m_Town_QA_' + $tag
$progression = 'Rogue10m_Progression_QA_' + $tag
$keys = 'Rogue10m_Fighter_QA_' + $tag
$suite = 'Rogue10m.MonsterLoot.OverlapRetry'
$expected = @('StationaryCapacityAndTransaction','BagExpansion','LeaveAndDestroyCleanup')
if ($FullSuite) {
    $suite = 'Rogue10m.MonsterLoot'
    $expected = @(
        'TableBoundariesAndValidation','SeededRollsAndUniqueInstances','ActualDeathExactlyOnce',
        'PickupFailureRetryAndPersistence','TestAndAuthorityIsolation',
        'OverlapRetry.StationaryCapacityAndTransaction','OverlapRetry.BagExpansion','OverlapRetry.LeaveAndDestroyCleanup'
    )
}
$arguments = @(
    ('"' + $project + '"'), '/Engine/Maps/Entry', '-nullrhi', '-unattended', '-nosplash', '-nosound',
    '-NoLiveCoding', '-NoRemoteShaderCompile',
    ('-RogueProfileSaveSlot=' + $town), ('-RogueProgressionSavePrefix=' + $progression),
    ('-RogueSkillKeySaveSlot=' + $keys), ('-ExecCmds="Automation RunTests ' + $suite + '"'),
    '-TestExit="Automation Test Queue Empty"', ('-ReportExportPath="' + $out + '\report"'),
    ('-abslog="' + $out + '\automation.log"')
)
if (-not $Execute) {
    [pscustomobject]@{ executed=$false; editor=$editor; arguments=$arguments; expected=$expected; note='Plan only. Coordinate the shared build/editor, apply the patch and build first; then use -Execute.' } | ConvertTo-Json -Depth 5
    return
}
if (-not (Test-Path -LiteralPath (Join-Path $root 'Content') -PathType Container)) { throw 'A complete independent project checkout is required.' }
if (Test-Path -LiteralPath $out) { throw 'Preserve existing evidence and choose a fresh Label.' }
function Assert-NoProjectEditor {
    $running = @(Get-CimInstance Win32_Process | Where-Object {
        $_.Name -like 'UnrealEditor*' -and (-not $_.CommandLine -or
            $_.CommandLine.Replace('/','\').IndexOf($project,[StringComparison]::OrdinalIgnoreCase) -ge 0 -or
            $_.CommandLine.IndexOf([IO.Path]::GetFileName($project),[StringComparison]::OrdinalIgnoreCase) -ge 0)
    })
    if ($running.Count) { throw 'Project UE process is running, or its command line cannot be inspected; coordinate before QA.' }
}
Assert-NoProjectEditor
$saveRoot = Join-Path $root 'Saved\SaveGames'
function Get-SaveHashes {
    if (Test-Path -LiteralPath $saveRoot) {
        Get-ChildItem -LiteralPath $saveRoot -Recurse -File -Force | ForEach-Object {
            [pscustomobject]@{ path=$_.FullName; sha256=(Get-FileHash -LiteralPath $_.FullName).Hash }
        }
    }
}
function Test-OwnedSave([string]$Path) {
    $name = [IO.Path]::GetFileName($Path)
    return [IO.Path]::GetDirectoryName($Path) -ieq $saveRoot -and
        ($name -ieq ($town + '.sav') -or $name -ieq ($keys + '.sav') -or
        ($name.StartsWith($progression + '_', [StringComparison]::OrdinalIgnoreCase) -and $name.EndsWith('.sav', [StringComparison]::OrdinalIgnoreCase)))
}
$before = @(Get-SaveHashes)
if (@($before | Where-Object { Test-OwnedSave $_.path }).Count) { throw 'QA namespace already contains saves; choose a fresh Label.' }
New-Item -ItemType Directory -Path $out | Out-Null
ConvertTo-Json -InputObject $before -Depth 4 | Set-Content -LiteralPath (Join-Path $out 'saves-before.json') -Encoding utf8
$process = $null; $failure = $null; $exitCode = $null
$preTestRedLogCount = 0; $testPhaseRedLogCount = 0
try {
    Assert-NoProjectEditor
    $process = Start-Process -FilePath $editor -ArgumentList $arguments -WorkingDirectory $root -WindowStyle Hidden -PassThru
    Write-Output ('Owned loot-retry QA PID=' + $process.Id)
    $watch = [Diagnostics.Stopwatch]::StartNew()
    while (-not $process.WaitForExit(1000)) {
        if ($watch.Elapsed.TotalSeconds -ge $TimeoutSeconds) { throw 'Owned loot-retry QA process timed out.' }
    }
    $process.Refresh(); $exitCode = $process.ExitCode
    if ($exitCode -ne 0) { throw ('Editor exit=' + $exitCode) }
    $report = Get-Content -LiteralPath (Join-Path $out 'report\index.json') -Raw | ConvertFrom-Json
    if ($report.succeeded -ne $expected.Count -or $report.succeededWithWarnings -ne 0 -or $report.failed -ne 0 -or $report.notRun -ne 0 -or $report.inProcess -ne 0 -or @($report.tests).Count -ne $expected.Count) { throw 'Automation report is failed, incomplete, or contains unexpected tests/warnings.' }
    foreach ($name in $expected) {
        $test = @($report.tests | Where-Object fullTestPath -eq ($suite + '.' + $name))
        if ($test.Count -ne 1 -or $test[0].state -ne 'Success' -or $test[0].errors -ne 0 -or $test[0].warnings -ne 0) { throw ('Failed or missing test=' + $name) }
    }
    $lines = [IO.File]::ReadAllLines((Join-Path $out 'automation.log'))
    $startPattern = 'LogAutomationController:\s*(?:Display:\s*)?Test Started\..*Path=\{' + [regex]::Escape($suite) + '(?:\.|\})'
    $firstTest = -1
    for ($index = 0; $index -lt $lines.Count; $index++) {
        if ($lines[$index] -match $startPattern) { $firstTest = $index; break }
    }
    if ($firstTest -lt 0) { throw 'Automation log does not prove that the requested loot suite started.' }
    $redPattern = '(?i)\bLog[^\s:]*:\s*(?:Error|Fatal)\s*:|\bFatal error\b|\b(?:Handled\s+)?ensure\s+(?:condition|failed|failure)|\bTraceback \(most recent call last\)|\bAttributeError:'
    $preTestErrors = @(); $testErrors = @()
    for ($index = 0; $index -lt $lines.Count; $index++) {
        if ($lines[$index] -match $redPattern) {
            if ($index -lt $firstTest) { $preTestErrors += $lines[$index] }
            else { $testErrors += $lines[$index] }
        }
    }
    $preTestRedLogCount = $preTestErrors.Count; $testPhaseRedLogCount = $testErrors.Count
    # UE startup self-tests can print intentional errors before this suite starts.
    # Preserve those separately; never claim the complete process log is error-free.
    [pscustomobject]@{preTest=$preTestErrors;testPhaseAndShutdown=$testErrors} | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $out 'red-log.json') -Encoding utf8
    if ($testPhaseRedLogCount) { throw 'Error/Fatal/ensure/Python output occurred during loot tests or shutdown.' }
    $queuePattern = 'LogAutomationCommandLine:.*Automation Test Queue Empty\s+' + $expected.Count + '\s+tests performed\.'
    if (-not @($lines | Select-String -Pattern $queuePattern).Count) { throw 'Automation log is missing the exact completed test-queue marker.' }
} catch { $failure = $_ } finally {
    if ($process -and -not $process.HasExited) {
        $process.Kill($true)
        if (-not $process.WaitForExit(10000)) { throw 'Owned QA process remains active; save hashes cannot be certified.' }
    }
    $after = @(Get-SaveHashes)
    ConvertTo-Json -InputObject $after -Depth 4 | Set-Content -LiteralPath (Join-Path $out 'saves-after.json') -Encoding utf8
    $lookup = @{}; foreach ($save in $after) { $lookup[$save.path] = $save.sha256 }
    $original = @{}; foreach ($save in $before) { $original[$save.path] = $true }
    $changed = @($before | Where-Object { -not $lookup.ContainsKey($_.path) -or $lookup[$_.path] -ne $_.sha256 })
    $unexpected = @($after | Where-Object { -not $original.ContainsKey($_.path) -and -not (Test-OwnedSave $_.path) })
    [pscustomobject]@{ executed=$true; label=$Label; originalChanged=$changed.Count; unexpectedNew=$unexpected.Count; exitCode=$exitCode; preTestRedLogCount=$preTestRedLogCount; testPhaseRedLogCount=$testPhaseRedLogCount; failure=if ($failure) { $failure.ToString() } else { $null }; physicalInput=$false; fullRun=$false } | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $out 'launcher.json') -Encoding utf8
}
if ($changed.Count -or $unexpected.Count) { throw 'Save preservation or namespace check failed; evidence was retained.' }
if ($failure) { throw $failure }
if ($FullSuite) { Write-Output ('RESULT=MONSTER_LOOT_AUTOMATION_PASSED tests=8 evidence=' + $out) }
else { Write-Output ('RESULT=LOOT_PICKUP_RETRY_PASSED tests=3 evidence=' + $out) }
