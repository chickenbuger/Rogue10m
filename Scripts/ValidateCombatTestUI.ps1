param(
    [string]$Label = '',
    [ValidateRange(640,3840)][int]$Width = 1280,
    [ValidateRange(480,2160)][int]$Height = 720,
    [ValidateRange(30,600)][int]$TimeoutSeconds = 240,
    [string]$ProjectRoot = (Split-Path -Parent $PSScriptRoot)
)
$ErrorActionPreference = 'Stop'
if ([string]::IsNullOrWhiteSpace($Label)) { $Label = [DateTime]::UtcNow.ToString('yyyyMMdd_HHmmss') }
if ($Label -notmatch '^[A-Za-z0-9_]{1,50}$') { throw 'QA label must be 1..50 letters, digits or underscores.' }
$combatSlot = 'Rogue10m_Town_QA_CombatUI_' + $Label
$combatEvidence = Join-Path $ProjectRoot ('Feature\doc\evidence\monster-test-ui-20261011\' + $combatSlot)
if ((Test-Path -LiteralPath $combatEvidence) -or (Test-Path -LiteralPath (Join-Path $ProjectRoot ('Saved\SaveGames\' + $combatSlot + '.sav')))) { throw 'QA label already exists; choose a fresh label.' }
New-Item -ItemType Directory -Path $combatEvidence -Force | Out-Null
$combatLog = Join-Path $combatEvidence 'runtime.log'
$combatArgs = @(
    ('"' + (Join-Path $ProjectRoot 'Rogue10m.uproject') + '"'), '/Game/Rogue10m/Maps/OpenRunMap',
    '-game', '-RenderOffscreen', '-windowed', '-ForceRes', ('-ResX='+$Width), ('-ResY='+$Height),
    '-unattended', '-nosound', '-nosplash', '-NoLiveCoding', '-NoRemoteShaderCompile',
    ('-RogueProfileSaveSlot='+$combatSlot), ('-RogueProgressionSavePrefix=Rogue10m_Progression_QA_CombatUI_'+$Label),
    ('-RogueSkillKeySaveSlot=Rogue10m_Fighter_QA_CombatUI_'+$Label),
    '-ExecCmds="Rogue10m.TestCombatUI"', ('-abslog="'+$combatLog+'"')
)
$combatProcess = $null
try {
    $combatProcess = Start-Process -FilePath 'D:\Program Files\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe' -ArgumentList $combatArgs -WindowStyle Hidden -PassThru
    Write-Output ('Combat UI QA pid='+$combatProcess.Id+' log='+$combatLog)
    $combatDeadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    while (-not $combatProcess.WaitForExit(1000)) {
        if ([DateTime]::UtcNow -gt $combatDeadline) { throw ('Combat UI QA timeout: '+$combatLog) }
    }
    if ($combatProcess.ExitCode -ne 0 -or -not ([IO.File]::ReadAllText($combatLog).Contains('RESULT=COMBAT_UI_PASSED'))) { throw ('Combat UI QA failed: '+$combatLog) }
    Write-Output ('RESULT=COMBAT_UI_PASSED evidence='+$combatEvidence)
} finally {
    if ($null -ne $combatProcess -and -not $combatProcess.HasExited) { Stop-Process -Id $combatProcess.Id; $combatProcess.WaitForExit(5000) | Out-Null }
}
