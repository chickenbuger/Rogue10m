#requires -Version 7.0
[CmdletBinding()]
param(
    [Parameter(Mandatory)][ValidatePattern('^[A-Za-z0-9_]{1,20}$')][string]$Label,
    [ValidateSet('Direct','Revisit','Both')][string]$Scenario = 'Both',
    [switch]$Execute,
    [string]$EditorHandoff = '',
    [switch]$NullRHI,
    [string]$EditorExe = 'D:\Program Files\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe',
    [ValidateRange(90,600)][int]$DirectTimeoutSeconds = 210,
    [ValidateRange(300,1200)][int]$RevisitTimeoutSeconds = 600
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'FiveVillageTravelContract.ps1')
$projectRoot = (Resolve-Path -LiteralPath (Split-Path -Parent $PSScriptRoot)).Path
$projectFile = Join-Path $projectRoot 'Rogue10m.uproject'
$saveRoot = Join-Path $projectRoot 'Saved\SaveGames'
$stagedFixture = Join-Path $projectRoot 'Feature\implementation\five-village-travel-20261111\Rogue10mFiveVillageTravelRuntimeTest.cpp'
$installedFixture = Join-Path $projectRoot 'Source\Rogue10m\Tests\Rogue10mFiveVillageTravelRuntimeTest.cpp'
$moduleDll = Join-Path $projectRoot 'Binaries\Win64\UnrealEditor-Rogue10m.dll'
$scenarios = if ($Scenario -ieq 'Both') { @('Direct','Revisit') } elseif ($Scenario -ieq 'Direct') { @('Direct') } else { @('Revisit') }
$contracts = @($scenarios | ForEach-Object { Get-FiveVillageTravelContract -ProjectRoot $projectRoot -Label $Label -Scenario $_ })

function Get-CaseArguments($Contract, [string]$Log) {
    $arguments = @(
        ('"' + $projectFile + '"'), '/Game/Rogue10m/Maps/StartHub', '-game', '-unattended',
        '-DisablePython', '-NoSplash', '-NoSound', '-NoLiveCoding', '-NoRemoteShaderCompile',
        '-culture=en', '-language=en', '-locale=en-US', '-windowed', '-ForceRes', '-ResX=1280', '-ResY=720',
        ('-RogueFiveVillageTravelLabel=' + $Contract.Label),
        ('-RogueFiveVillageTravelScenario=' + $Contract.Scenario),
        ('-RogueProfileSaveSlot=' + $Contract.ProfileSlot),
        ('-RogueProgressionSavePrefix=' + $Contract.ProgressionPrefix),
        ('-RogueSkillKeySaveSlot=' + $Contract.SkillSlot),
        '-ExecCmds="t.MaxFPS 60,Rogue10m.TestFiveVillageTravel"', ('-abslog="' + $Log + '"')
    )
    if ($NullRHI) { $arguments += '-nullrhi' } else { $arguments += @('-d3d11','-RenderOffScreen') }
    return $arguments
}

$plan = [ordered]@{
    status = 'plan_only_not_executed'; label = $Label; scenario = $Scenario
    project = $projectFile; editor = $EditorExe
    fixture_staged_path = $stagedFixture; fixture_installed_path = $installedFixture
    cases = @($contracts | ForEach-Object {
        $log = Join-Path $_.CaseDirectory 'engine.log'
        [ordered]@{ scenario = $_.Scenario; evidence = $_.CaseDirectory; runtime = $_.RuntimeDirectory
            profileSlot = $_.ProfileSlot; progressionPrefix = $_.ProgressionPrefix; skillSlot = $_.SkillSlot
            timeoutSeconds = if ($_.Scenario -eq 'Direct') { $DirectTimeoutSeconds } else { $RevisitTimeoutSeconds }
            arguments = @(Get-CaseArguments $_ $log) }
    })
    outcome = 'Abandoned'; physical_input = $false; victory_certified = $false
    execution_target = 'DevelopmentEditor -game only; not a packaged game EXE'
    shared_editor_build_owner_required = $true
}
if (-not $Execute) {
    $plan | ConvertTo-Json -Depth 10
    return
}
if ([string]::IsNullOrWhiteSpace($EditorHandoff)) {
    throw 'Execute requires the actual shared editor/build ownership handoff; the default command only prints a plan.'
}
if (-not $IsWindows) { throw 'The real UE launcher requires Windows.' }
$editorFile = (Resolve-Path -LiteralPath $EditorExe).Path
if ([IO.Path]::GetFileName($editorFile) -notin @('UnrealEditor.exe','UnrealEditor-Cmd.exe')) {
    throw 'This fixture requires a DevelopmentEditor executable, not a packaged game EXE. Shipping save overrides are not supported.'
}
if (-not (Test-Path -LiteralPath $projectFile)) { throw 'Project file is missing.' }
if (-not (Test-Path -LiteralPath $stagedFixture) -or -not (Test-Path -LiteralPath $installedFixture)) {
    throw 'The fixture must first be reviewed and installed into Source after its source freeze is released. This launcher never installs or builds it.'
}
$fixtureHash = (Get-FileHash -LiteralPath $stagedFixture -Algorithm SHA256).Hash
if ($fixtureHash -ne (Get-FileHash -LiteralPath $installedFixture -Algorithm SHA256).Hash) {
    throw 'Installed fixture differs from the reviewed isolated fixture; inspect it before running.'
}
if (-not (Test-Path -LiteralPath $moduleDll)) { throw 'A compiled project DLL is required.' }
$sourceFiles = @(Get-ChildItem -LiteralPath (Join-Path $projectRoot 'Source') -Recurse -File |
    Where-Object { $_.Extension -in @('.cpp','.h','.cs') })
$newestSource = $sourceFiles | Sort-Object LastWriteTimeUtc -Descending | Select-Object -First 1
if (-not $newestSource -or (Get-Item -LiteralPath $moduleDll).LastWriteTimeUtc -lt $newestSource.LastWriteTimeUtc) {
    throw 'Project DLL predates Source changes. Ask the shared build owner to build/restart before this run.'
}
function Assert-NoSharedUE {
    $running = @(Get-Process -Name UnrealEditor,UnrealEditor-Cmd,UnrealBuildTool,dotnet -ErrorAction SilentlyContinue)
    if ($running.Count) { throw 'UE/UBT or a potentially related dotnet process is active. Wait for the shared owner; no process was stopped.' }
}
function Save-Journal {
    $journal | ConvertTo-Json -Depth 14 | Set-Content -LiteralPath $journalFile -Encoding utf8NoBOM
}
function Stop-OnlyOwnedProcess {
    if (-not $process -or $process.HasExited) { return }
    $current = Get-Process -Id $process.Id -ErrorAction SilentlyContinue
    if (-not $current -or $current.StartTime -ne $ownedStart -or $current.Path -ine $editorFile) {
        throw 'Owned process identity cannot be established; no unverified process will be terminated.'
    }
    $process.Kill($true)
    if (-not $process.WaitForExit(10000)) { throw 'Owned UE did not exit after bounded cleanup.' }
}
Assert-NoSharedUE
$labelRoot = Split-Path -Parent $contracts[0].CaseDirectory
if (Test-Path -LiteralPath $labelRoot) { throw 'A fresh nonexistent Label evidence directory is required; previous evidence is never overwritten.' }
$before = @(Get-FiveVillageSaveSnapshot -SaveRoot $saveRoot)
if (@($before | Where-Object { Test-FiveVillageOwnedSave -Path $_.path -SaveRoot $saveRoot -Contracts $contracts }).Count) {
    throw 'Existing QA saves match this Label/Scenario. They will not be overwritten or removed.'
}
[IO.Directory]::CreateDirectory($labelRoot) | Out-Null
ConvertTo-Json -InputObject $before -Depth 5 | Set-Content -LiteralPath (Join-Path $labelRoot 'saves-before.json') -Encoding utf8NoBOM
$journalFile = Join-Path $labelRoot 'launcher.json'
$journal = [ordered]@{
    schema_version = 1; status = 'running'; label = $Label; requested_scenario = $Scenario
    editor_handoff = $EditorHandoff; project = $projectFile; editor = $editorFile
    started_utc = [DateTime]::UtcNow.ToString('o'); fixture_sha256 = $fixtureHash
    module_sha256 = (Get-FileHash -LiteralPath $moduleDll -Algorithm SHA256).Hash
    newest_source_path = $newestSource.FullName; cases = @(); preservation = $null
    physical_input = $false; full_victory_run = $false; outcome = 'Abandoned'
}
Save-Journal
$process = $null; $ownedStart = $null; $failure = $null; $cleanupFailure = $null
try {
    foreach ($contract in $contracts) {
        Assert-NoSharedUE
        if (Test-Path -LiteralPath $contract.CaseDirectory) { throw 'Case evidence exists unexpectedly.' }
        [IO.Directory]::CreateDirectory($contract.CaseDirectory) | Out-Null
        $log = Join-Path $contract.CaseDirectory 'engine.log'
        $arguments = @(Get-CaseArguments $contract $log)
        $record = [ordered]@{ scenario = $contract.Scenario; started_utc = [DateTime]::UtcNow.ToString('o')
            arguments = $arguments; pid = $null; exit_code = $null; timed_out = $false
            result_validation = $null; red_log_count = $null; passed = $false }
        $journal.cases += @($record)
        Save-Journal
        $process = Start-Process -FilePath $editorFile -ArgumentList $arguments -WorkingDirectory $projectRoot -WindowStyle Hidden -PassThru
        $record.pid = $process.Id
        Save-Journal
        $ownedStart = $process.StartTime
        Save-Journal
        Write-Output ('Owned five-village ' + $contract.Scenario + ' QA PID=' + $process.Id + ' log=' + $log)
        $timeout = if ($contract.Scenario -eq 'Direct') { $DirectTimeoutSeconds } else { $RevisitTimeoutSeconds }
        $clock = [Diagnostics.Stopwatch]::StartNew()
        while (-not $process.WaitForExit(1000)) {
            if ($clock.Elapsed.TotalSeconds -ge $timeout) {
                $record.timed_out = $true
                Stop-OnlyOwnedProcess
                break
            }
        }
        $process.Refresh(); $record.exit_code = $process.ExitCode
        $record.ended_utc = [DateTime]::UtcNow.ToString('o')
        $text = if (Test-Path -LiteralPath $log) { [IO.File]::ReadAllText($log) } else { '' }
        $red = @($text -split '\r?\n' | Where-Object {
            $_ -match '(?i)\bLog[^\s:]*:\s*(?:Error|Fatal)\s*:|\bFatal error\b|\b(?:Handled\s+)?ensure\s+(?:condition|failed|failure)|\bTraceback \(most recent call last\)|\bAttributeError:'
        })
        $record.red_log_count = $red.Count
        ConvertTo-Json -InputObject $red -Depth 4 | Set-Content -LiteralPath (Join-Path $contract.CaseDirectory 'red-log.json') -Encoding utf8NoBOM
        $resultPath = Join-Path $contract.RuntimeDirectory 'results.json'
        $validation = Test-FiveVillageTravelResult -Path $resultPath -Label $Label -Scenario $contract.Scenario -ExpectedRendered (-not [bool]$NullRHI)
        $record.result_validation = $validation
        $events = Join-Path $contract.RuntimeDirectory 'events.csv'
        $eventsValid = (Test-Path -LiteralPath $events) -and @((Get-Content -LiteralPath $events -ErrorAction Stop)).Count -gt 1
        $marker = 'RESULT=FIVE_VILLAGE_TRAVEL_PASSED label=' + $Label + ' scenario=' + $contract.Scenario
        $record.passed = ($record.exit_code -eq 0 -and -not $record.timed_out -and $red.Count -eq 0 `
            -and $validation.passed -and $eventsValid -and $text.Contains($marker))
        Save-Journal
        if (-not $record.passed) { throw ('Actual travel ' + $contract.Scenario + ' failed or is unproven; inspect retained case evidence.') }
    }
} catch { $failure = $_ } finally {
    try { Stop-OnlyOwnedProcess } catch { $cleanupFailure = $_ }
    if (-not $cleanupFailure) {
        try {
            $after = @(Get-FiveVillageSaveSnapshot -SaveRoot $saveRoot)
            ConvertTo-Json -InputObject $after -Depth 5 | Set-Content -LiteralPath (Join-Path $labelRoot 'saves-after.json') -Encoding utf8NoBOM
            $journal.preservation = Compare-FiveVillageSaveSnapshots -Before $before -After $after -SaveRoot $saveRoot -Contracts $contracts
            if (-not $journal.preservation.preserved -and -not $failure) { $failure = 'Original save or unexpected new save check failed.' }
        } catch { if (-not $failure) { $failure = $_ }; $journal.preservation = @{ preserved = $false; snapshot_error = $_.ToString() } }
    } else { $journal.preservation = @{ preserved = $null; reason = 'Owned UE cleanup uncertain; no post-run snapshot certified.' } }
    $journal.status = if ($cleanupFailure) { 'cleanup_uncertain' } elseif ($failure) { 'failed' } else { 'passed' }
    $journal.failure = if ($failure) { $failure.ToString() } else { $null }
    $journal.cleanup_failure = if ($cleanupFailure) { $cleanupFailure.ToString() } else { $null }
    $journal.ended_utc = [DateTime]::UtcNow.ToString('o')
    Save-Journal
}
if ($cleanupFailure) { throw $cleanupFailure }
if ($failure) { throw $failure }
Write-Output ('PASSED actual five-village travel/Abandon settlement/Hub return. Physical input, boss victory and 15-minute content are not certified. Evidence=' + $labelRoot)
