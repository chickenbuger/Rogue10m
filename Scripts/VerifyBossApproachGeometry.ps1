param(
    [ValidateSet('BossSouthApproach')][string]$Suite = 'BossSouthApproach',
    [ValidateRange(60,600)][int]$TimeoutSeconds = 240,
    [ValidatePattern('^[A-Za-z0-9_]{1,24}$')][string]$RunLabel = 'final',
    [ValidatePattern('^[A-Za-z0-9_-]{1,80}$')][string]$EvidencePacket = 'boss-south-approach-20261101'
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
if(Get-Process -Name 'UnrealEditor','UnrealEditor-Cmd' -ErrorAction SilentlyContinue){throw 'Coordinate the existing UE process before this isolated geometry QA.'}
$evidence = Join-Path $projectRoot ('Feature\doc\evidence\' + $EvidencePacket + '\' + $Suite + '_' + $RunLabel)
if (Test-Path -LiteralPath $evidence) { throw 'Evidence already exists; preserve the previous run or choose a new evidence root before rerunning.' }
New-Item -ItemType Directory -Path $evidence | Out-Null
$expected = @('Transform','InvalidGeometry')
$qaTag = $Suite + '_' + $RunLabel
$qaProfile = 'Rogue10m_Town_QA_' + $qaTag
$qaProgression = 'Rogue10m_Progression_QA_' + $qaTag
$qaKeys = 'Rogue10m_Fighter_QA_' + $qaTag
$saveDirectory = Join-Path $projectRoot 'Saved\SaveGames'
if ($qaProfile.Length -gt 80 -or $qaProgression.Length -gt 80 -or $qaKeys.Length -gt 80) { throw 'QA slot override is too long; refuse production-slot fallback.' }
if ((Test-Path -LiteralPath (Join-Path $saveDirectory ($qaProfile + '.sav'))) -or
    (Test-Path -LiteralPath (Join-Path $saveDirectory ($qaKeys + '.sav'))) -or
    (Get-ChildItem -LiteralPath $saveDirectory -Filter ($qaProgression + '_*') -ErrorAction SilentlyContinue | Select-Object -First 1)) {
    throw 'QA saves already exist. Preserve them and choose a fresh RunLabel.'
}
$arguments = @(
    ('"' + (Join-Path $projectRoot 'Rogue10m.uproject') + '"'), '/Engine/Maps/Entry',
    '-nullrhi','-unattended','-nosplash','-nosound','-NoLiveCoding','-NoRemoteShaderCompile',
    ('-RogueProfileSaveSlot=' + $qaProfile),
    ('-RogueProgressionSavePrefix=' + $qaProgression),
    ('-RogueSkillKeySaveSlot=' + $qaKeys),
    ('-ExecCmds="Automation RunTests Rogue10m.OpenWorld.' + $Suite + '"'),
    '-TestExit="Automation Test Queue Empty"',
    ('-ReportExportPath="' + $evidence + '\report"'), ('-abslog="' + $evidence + '\automation.log"'))
$process = Start-Process -FilePath 'D:\Program Files\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' -ArgumentList $arguments -WindowStyle Hidden -PassThru
Write-Output ('Owned automation PID=' + $process.Id)
try {
    if (-not $process.WaitForExit($TimeoutSeconds * 1000)) { throw 'Automation timed out.' }
    if ($process.ExitCode -ne 0) { throw ('Editor exit=' + $process.ExitCode) }
    $report = Get-Content -LiteralPath (Join-Path $evidence 'report\index.json') -Raw | ConvertFrom-Json
    if ($report.succeeded -ne $expected.Count -or $report.succeededWithWarnings -ne 0 -or $report.failed -ne 0 -or $report.notRun -ne 0 -or $report.inProcess -ne 0 -or @($report.tests).Count -ne $expected.Count) { throw 'Unexpected automation counts.' }
    foreach ($name in $expected) {
        $test = @($report.tests | Where-Object fullTestPath -eq ('Rogue10m.OpenWorld.' + $Suite + '.' + $name))
        if ($test.Count -ne 1 -or $test[0].state -ne 'Success' -or $test[0].errors -ne 0) { throw ('Failed test=' + $name) }
    }
    $report.tests | Select-Object fullTestPath,state,errors | ConvertTo-Json
    Write-Output ('RESULT=' + $Suite + '_PASSED')
} finally {
    if (-not $process.HasExited) { $process.Kill(); $process.WaitForExit(5000) | Out-Null }
}
