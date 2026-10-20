param(
    [string]$Label = '',
    [ValidateRange(30, 900)][int]$TimeoutSeconds = 180,
    [string]$EngineRoot = 'D:\Program Files\UE_5.8',
    [string]$ProjectRoot = (Split-Path -Parent $PSScriptRoot)
)
$ErrorActionPreference = 'Stop'
if ([string]::IsNullOrWhiteSpace($Label)) { $Label = 'trait16_' + [DateTime]::UtcNow.ToString('yyyyMMdd_HHmmss') }
if ($Label -notmatch '^[A-Za-z0-9_]{1,50}$') { throw 'Label must contain 1..50 letters, digits, or underscores.' }
$project = Join-Path $ProjectRoot 'Rogue10m.uproject'
$editor = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$profileSlot = 'Rogue10m_Town_QA_' + $Label
$savePrefix = 'Rogue10m_Progression_QA_' + $Label
$evidence = Join-Path $ProjectRoot ('Feature\doc\evidence\trait16-runtime-20261011\' + $profileSlot)
if ((Test-Path -LiteralPath $evidence) -or
    (Test-Path -LiteralPath (Join-Path $ProjectRoot ('Saved\SaveGames\' + $profileSlot + '.sav')))) {
    throw 'This QA label already exists. Choose a fresh label; prior evidence and saves are preserved.'
}
if (-not (Test-Path -LiteralPath $editor) -or -not (Test-Path -LiteralPath $project)) { throw 'Editor or project is missing.' }
New-Item -ItemType Directory -Path $evidence -Force | Out-Null
foreach ($phase in @('Write', 'Read')) {
    $log = Join-Path $evidence ($phase.ToLowerInvariant() + '.log')
    $arguments = @(
        ('"' + $project + '"'), '/Game/Rogue10m/Maps/StartHub', '-game', '-nullrhi',
        '-unattended', '-nosplash', '-nosound', '-NoLiveCoding', '-NoRemoteShaderCompile',
        ('-RogueProfileSaveSlot=' + $profileSlot), ('-RogueProgressionSavePrefix=' + $savePrefix),
        ('-RogueSkillKeySaveSlot=Rogue10m_Fighter_QA_Trait_' + $Label),
        ('-ExecCmds="Rogue10m.TestTraitPersistence' + $phase + '"'), ('-abslog="' + $log + '"')
    )
    $process = $null
    try {
        $process = Start-Process -FilePath $editor -ArgumentList $arguments -WindowStyle Hidden -PassThru
        Write-Output ($phase + ' pid=' + $process.Id + ' log=' + $log)
        $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
        while (-not $process.WaitForExit(1000)) {
            if ([DateTime]::UtcNow -gt $deadline) { throw ($phase + ' timed out; log=' + $log) }
        }
        $result = Join-Path $evidence ($phase.ToLowerInvariant() + '-results.txt')
        $expected = 'RESULT=TRAIT_PERSISTENCE_PASSED mode=' + $phase.ToLowerInvariant()
        if ($process.ExitCode -ne 0 -or -not (Test-Path -LiteralPath $log) -or
            -not ([IO.File]::ReadAllText($log).Contains($expected)) -or -not (Test-Path -LiteralPath $result)) {
            throw ($phase + ' failed; exit=' + $process.ExitCode + ' log=' + $log)
        }
        Write-Output ($expected + ' exit=' + $process.ExitCode + ' results=' + $result)
    } finally {
        if ($null -ne $process -and -not $process.HasExited) {
            Stop-Process -Id $process.Id
            $process.WaitForExit(5000) | Out-Null
        }
    }
}
Write-Output ('Two-process trait migration, profile isolation and rejected-save preservation verified. Evidence=' + $evidence)
