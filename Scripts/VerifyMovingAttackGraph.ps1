param(
    [ValidateSet('apply','audit','stance')][string]$Phase = 'audit',
    [string]$Label = '',
    [int]$TimeoutSeconds = 180
)
$ErrorActionPreference = 'Stop'
if ([string]::IsNullOrEmpty($Label)) { $Label = 'graph-' + $Phase }
if ($Label -notmatch '^[A-Za-z0-9_-]+$') { throw 'Invalid evidence label.' }
$taskRoot = Split-Path -Parent $PSScriptRoot
$taskEvidence = Join-Path $taskRoot 'Feature/doc/evidence/moving-upper-body-attack-20261009'
$taskLog = Join-Path $taskEvidence ($Label + '.log')
if (Test-Path -LiteralPath $taskLog) { throw 'Use a fresh evidence label.' }
$taskScript = Join-Path $PSScriptRoot 'Editor/ConfigureMovingAttackUpperBody.py'
$taskMarker = 'RESULT=MOVING_ATTACK_GRAPH_RELOAD_PASSED'
if ($Phase -eq 'apply') { $taskMarker = 'RESULT=MOVING_ATTACK_GRAPH_APPLY_PASSED' }
if ($Phase -eq 'stance') {
    $taskScript = Join-Path $PSScriptRoot 'Editor/AuditMovingAttackStance.py'
    $taskMarker = 'RESULT=FIGHTER_COMBAT_STANCE_AUDIT_PASSED'
}
$taskArgs = @(('"' + (Join-Path $taskRoot 'Rogue10m.uproject') + '"'),
    '/Game/Rogue10m/Maps/StartHub','-nullrhi','-unattended','-nosplash','-nosound','-NoLiveCoding',
    ('-ExecutePythonScript="' + $taskScript + '"'),('-abslog="' + $taskLog + '"'))
if ($Phase -eq 'apply') { $taskArgs += '-RogueMovingAttackApply' }
$taskProcess = Start-Process -FilePath 'D:/Program Files/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' -ArgumentList $taskArgs -WindowStyle Hidden -PassThru
Write-Output ($Phase + ' pid=' + $taskProcess.Id + ' log=' + $taskLog)
$taskDeadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
while (-not $taskProcess.WaitForExit(1000)) {
    if ([DateTime]::UtcNow -gt $taskDeadline) {
        Stop-Process -Id $taskProcess.Id
        throw 'Graph process timeout: inspect report and log before retrying.'
    }
}
if ($taskProcess.ExitCode -ne 0 -or -not ([IO.File]::ReadAllText($taskLog).Contains($taskMarker))) {
    throw ($Phase + ' failed; exit=' + $taskProcess.ExitCode + ' log=' + $taskLog)
}
Write-Output ($taskMarker + ' exit=' + $taskProcess.ExitCode)
