param(
    [string]$Label = '',
    [ValidateRange(30, 900)][int]$TimeoutSeconds = 180,
    [string]$EngineRoot = 'D:\Program Files\UE_5.8',
    [string]$ProjectRoot = (Split-Path -Parent $PSScriptRoot)
)
$ErrorActionPreference = 'Stop'
if ([string]::IsNullOrWhiteSpace($Label)) { $Label = 'failure_' + [DateTime]::UtcNow.ToString('yyyyMMdd_HHmmss') }
if ($Label -notmatch '^[A-Za-z0-9_]{1,50}$') { throw 'Label must contain 1..50 letters, digits, or underscores.' }
$project = Join-Path $ProjectRoot 'Rogue10m.uproject'
$editor = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$profileSlot = 'Rogue10m_Town_QA_' + $Label
$evidenceRoot = Join-Path $ProjectRoot ('Feature\doc\evidence\town-services-persistence-20261011\' + $profileSlot)
if ((Test-Path -LiteralPath $evidenceRoot) -or
    (Test-Path -LiteralPath (Join-Path $ProjectRoot ('Saved\SaveGames\' + $profileSlot + '.sav')))) {
    throw 'This QA label already exists. Choose a fresh label; prior evidence and saves are preserved.'
}
if (-not (Test-Path -LiteralPath $editor) -or -not (Test-Path -LiteralPath $project)) { throw 'Editor or project is missing.' }
$evidence = Join-Path $evidenceRoot 'failures'
New-Item -ItemType Directory -Path $evidence -Force | Out-Null
$log = Join-Path $evidence 'runtime.log'
$arguments = @(
    ('"' + $project + '"'), '/Game/Rogue10m/Maps/StartHub', '-game', '-nullrhi',
    '-unattended', '-nosplash', '-nosound', '-NoLiveCoding', '-NoRemoteShaderCompile',
    ('-RogueProfileSaveSlot=' + $profileSlot), ('-RogueProgressionSavePrefix=Rogue10m_Progression_QA_' + $Label),
    ('-RogueSkillKeySaveSlot=Rogue10m_Fighter_QA_Failure_' + $Label),
    '-ExecCmds="Rogue10m.TestProgressionFailures"', ('-abslog="' + $log + '"')
)
$process = $null
try {
    $process = Start-Process -FilePath $editor -ArgumentList $arguments -WindowStyle Hidden -PassThru
    Write-Output ('ProgressionFailures pid=' + $process.Id + ' log=' + $log)
    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    while (-not $process.WaitForExit(1000)) {
        if ([DateTime]::UtcNow -gt $deadline) { throw ('ProgressionFailures timed out; log=' + $log) }
    }
    $result = Join-Path $evidence 'results.txt'
    if ($process.ExitCode -ne 0 -or -not (Test-Path -LiteralPath $log) -or
        -not ([IO.File]::ReadAllText($log).Contains('RESULT=PROGRESSION_FAILURES_PASSED')) -or
        -not (Test-Path -LiteralPath $result) -or -not (Test-Path -LiteralPath (Join-Path $evidence 'version.sav')) -or
        -not (Test-Path -LiteralPath (Join-Path $evidence 'missing_asset_after_growth.sav'))) {
        throw ('ProgressionFailures failed; exit=' + $process.ExitCode + ' log=' + $log)
    }
    Write-Output ('RESULT=PROGRESSION_FAILURES_PASSED exit=' + $process.ExitCode + ' results=' + $result)
} finally {
    if ($null -ne $process -and -not $process.HasExited) {
        Stop-Process -Id $process.Id
        $process.WaitForExit(5000) | Out-Null
    }
}
