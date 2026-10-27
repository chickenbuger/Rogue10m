param(
    [ValidateSet('AttackResources','HybridSkillTree','ProgressionRequirements')][string]$Suite = 'AttackResources',
    [ValidateRange(60,600)][int]$TimeoutSeconds = 240,
    [ValidatePattern('^[A-Za-z0-9_]{1,24}$')][string]$RunLabel = 'final',
    [ValidatePattern('^[A-Za-z0-9_-]{1,80}$')][string]$EvidencePacket = 'atomic-attack-costs-20261026'
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$evidence = Join-Path $projectRoot ('Feature\doc\evidence\' + $EvidencePacket + '\' + $Suite + '_' + $RunLabel)
if (Test-Path -LiteralPath $evidence) { throw 'Evidence already exists; preserve the previous run or choose a new evidence root before rerunning.' }
New-Item -ItemType Directory -Path $evidence | Out-Null
$expected = if ($Suite -eq 'AttackResources') {
    @('AggregationAndAtomicFailure','InvalidDataAndDiscount','OptionalManaAndExecutionSnapshot','ReentrantAcceptanceRejected')
} elseif ($Suite -eq 'ProgressionRequirements') { @('TruthTable','InvalidDataAndEmptyDefaults','LegacyAlternativesAndGroups','LiveAcquisitionAndAttack') }
else { @('MasteryAlternativesAndLegacy','MasteryAndRewards','SharedLedgerAndValidation') }
$arguments = @(
    ('"' + (Join-Path $projectRoot 'Rogue10m.uproject') + '"'), '/Engine/Maps/Entry',
    '-nullrhi','-unattended','-nosplash','-nosound','-NoLiveCoding','-NoRemoteShaderCompile',
    '-RogueProfileSaveSlot=Rogue10m_Town_QA_R11_20261026A',
    '-RogueProgressionSavePrefix=Rogue10m_Progression_QA_R11_20261026A',
    '-RogueSkillKeySaveSlot=Rogue10m_Fighter_QA_R11_20261026A',
    ('-ExecCmds="Automation RunTests Rogue10m.' + $Suite + '"'),
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
        $test = @($report.tests | Where-Object fullTestPath -eq ('Rogue10m.' + $Suite + '.' + $name))
        if ($test.Count -ne 1 -or $test[0].state -ne 'Success' -or $test[0].errors -ne 0) { throw ('Failed test=' + $name) }
    }
    $report.tests | Select-Object fullTestPath,state,errors | ConvertTo-Json
    Write-Output ('RESULT=' + $Suite + '_PASSED')
} finally {
    if (-not $process.HasExited) { Stop-Process -Id $process.Id; $process.WaitForExit(5000) | Out-Null }
}
