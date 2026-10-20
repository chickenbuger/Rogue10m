param(
    [ValidateSet('assets','automation','runtime','read')][string]$Phase = 'automation',
    [string]$TestId = 'Hybrid20261020',
    [switch]$InputChecks,
    [int]$TimeoutSeconds = 240
)
$ErrorActionPreference = 'Stop'
if ($TestId -notmatch '^[A-Za-z0-9_]{1,38}$') { throw 'Invalid isolated test ID.' }
function Confirm-HybridAutomationReport {
    param([Parameter(Mandatory=$true)][string]$ReportPath)
    if (-not (Test-Path -LiteralPath $ReportPath -PathType Leaf)) { throw "Missing automation report: $ReportPath" }
    $report = Get-Content -LiteralPath $ReportPath -Raw | ConvertFrom-Json
    foreach ($name in @('succeeded','succeededWithWarnings','failed','notRun','inProcess','tests')) {
        if ($null -eq $report.PSObject.Properties[$name]) { throw "Missing automation report field: $name" }
    }
    if ($report.succeeded -ne 3 -or $report.succeededWithWarnings -ne 0 -or $report.failed -ne 0 `
        -or $report.notRun -ne 0 -or $report.inProcess -ne 0 -or @($report.tests).Count -ne 3) {
        throw 'Hybrid automation aggregate counts must be succeeded=3 and all other outcomes=0.'
    }
    $expected = @(
        'Rogue10m.HybridSkillTree.MasteryAlternativesAndLegacy',
        'Rogue10m.HybridSkillTree.MasteryAndRewards',
        'Rogue10m.HybridSkillTree.SharedLedgerAndValidation'
    )
    foreach ($path in $expected) {
        $match = @($report.tests | Where-Object { $_.fullTestPath -eq $path })
        if ($match.Count -ne 1 -or $match[0].state -ne 'Success' `
            -or $null -eq $match[0].PSObject.Properties['errors'] -or $match[0].errors -ne 0) {
            throw "Hybrid automation test did not complete with Success/errors=0: $path"
        }
    }
}
$projectRoot = Split-Path $PSScriptRoot -Parent
$evidenceName = if ($InputChecks) { 'hybrid-tree-input-polish-20261020' } else { 'hybrid-skill-tree-runtime-20261020' }
$evidence = Join-Path $projectRoot ('Feature\doc\evidence\' + $evidenceName)
New-Item -ItemType Directory -Path $evidence -Force | Out-Null
$log = Join-Path $evidence ($Phase + '-' + $TestId + '.log')
$editor = 'D:\Program Files\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$arguments = @('"' + (Join-Path $projectRoot 'Rogue10m.uproject') + '"',
    '-unattended','-nosplash','-nosound','-NoLiveCoding','-NoRemoteShaderCompile',
    ('-abslog="' + $log + '"'),
    ('-RogueProfileSaveSlot=Rogue10m_Town_QA_' + $TestId),
    ('-RogueProgressionSavePrefix=Rogue10m_Progression_QA_' + $TestId),
    ('-RogueSkillKeySaveSlot=Rogue10m_Fighter_QA_' + $TestId))
if ($InputChecks) { $arguments += '-RogueHybridInputQA' }
if ($Phase -eq 'assets') {
    $arguments += @('-nullrhi','-run=pythonscript',
        ('-script="' + (Join-Path $projectRoot 'Scripts\Editor\CreateHybridSkillTreeAssets.py') + '"'))
    $marker = 'RESULT=HYBRID_TREE_ASSETS_CREATED'
} elseif ($Phase -eq 'automation') {
    $arguments += @('/Engine/Maps/Entry','-nullrhi',
        '-ExecCmds="Automation RunTests Rogue10m.HybridSkillTree"',
        '-TestExit="Automation Test Queue Empty"',
        ('-ReportExportPath="' + (Join-Path $evidence 'automation-report') + '"'))
    $marker = 'Automation Test Queue Empty 3 tests performed.'
} else {
    $arguments += @('/Game/Rogue10m/Maps/StartHub','-game','-windowed','-ForceRes','-ResX=1920','-ResY=1080',
        '-ExecCmds="Rogue10m.TestHybridSkillTree"')
    if ($Phase -eq 'read') {
        $arguments += '-RogueHybridRead'
        $marker = 'RESULT=HYBRID_READ_PASSED'
    } else {
        $marker = 'RESULT=HYBRID_RUNTIME_PASSED'
    }
}
$process = Start-Process -FilePath $editor -ArgumentList $arguments -WindowStyle Hidden -PassThru
Write-Output "$Phase started pid=$($process.Id)"
if (-not $process.WaitForExit($TimeoutSeconds * 1000)) {
    # Only stop the process started by this invocation.
    Stop-Process -Id $process.Id -ErrorAction SilentlyContinue
    throw "$Phase timed out; see $log"
}
$body = Get-Content -LiteralPath $log -Raw
if ($process.ExitCode -ne 0 -or -not $body.Contains($marker)) {
    Get-Content -LiteralPath $log -Tail 75
    throw "$Phase verification failed (exit=$($process.ExitCode)); see $log"
}
if ($InputChecks -and $Phase -in @('runtime','read')) {
    $inputMarker = if ($Phase -eq 'read') { 'RESULT=HYBRID_INPUT_READ_PASSED' } else { 'RESULT=HYBRID_INPUT_RUNTIME_PASSED' }
    if (-not $body.Contains($inputMarker)) { throw "Missing actual input QA marker; see $log" }
}
if ($Phase -eq 'automation') {
    Confirm-HybridAutomationReport -ReportPath (Join-Path $evidence 'automation-report\index.json')
}
Write-Output "$Phase passed: $marker"
