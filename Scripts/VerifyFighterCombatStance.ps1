param(
    [ValidateSet('discover','prepare','audit','runtime','render','combat','stone')][string]$Phase = 'audit',
    [string]$Label = '',
    [int]$TimeoutSeconds = 300,
    [string]$EngineRoot = 'D:\Program Files\UE_5.8',
    [string]$ProjectRoot = 'D:\Project\Rogue10m'
)
$ErrorActionPreference = 'Stop'
if ([string]::IsNullOrEmpty($Label)) { $Label = 'stance-20261005-' + $Phase }
if ($Label -notmatch '^[A-Za-z0-9_-]+$') { throw 'Invalid evidence label.' }
if ($TimeoutSeconds -lt 30 -or $TimeoutSeconds -gt 600) { throw 'Timeout must be 30..600 seconds.' }
$evidence = Join-Path $ProjectRoot 'Feature\doc\evidence\fighter-combat-stance-20261005'
$log = Join-Path $evidence ($Label + '.log')
if (Test-Path -LiteralPath $log) { throw 'Existing evidence: choose a new label.' }
New-Item -ItemType Directory -Path $evidence -Force | Out-Null
$arguments = @(('"' + (Join-Path $ProjectRoot 'Rogue10m.uproject') + '"'),
    '-unattended','-nosplash','-nosound','-NoLiveCoding','-NoRemoteShaderCompile',('-abslog="' + $log + '"'))
if ($Phase -in @('discover','prepare','audit')) {
    $arguments += @('-nullrhi', ('-ExecutePythonScript="' + (Join-Path $ProjectRoot 'Scripts\Editor\PrepareFighterCombatStance.py') + '"'))
    $expected = 'RESULT=FIGHTER_COMBAT_STANCE_AUDIT_PASSED'
    if ($Phase -eq 'discover') { $arguments += '-RogueFighterStanceDiscover'; $expected = 'RESULT=FIGHTER_COMBAT_STANCE_DISCOVERY_PASSED' }
    if ($Phase -eq 'prepare') { $arguments += '-RogueFighterStancePrepare'; $expected = 'RESULT=FIGHTER_COMBAT_STANCE_PREPARE_PASSED' }
} else {
    $arguments += @('/Game/FirstPerson/Lvl_FirstPerson','-game','-windowed','-ForceRes','-ResX=1280','-ResY=720',
        '-RogueSkillKeySaveSlot=Rogue10m_Fighter_QA_20261005_Stance',
        '-RogueProgressionSavePrefix=Rogue10m_Progression_QA_FighterStance')
    if ($Phase -eq 'combat') {
        $arguments += @('-nullrhi','-ExecCmds="Rogue10m.TestFistCombat"','-RogueFistDamageTarget',('-RogueFistLabel=' + $Label))
        $expected = 'RESULT=FIST_COMBAT_PASSED'
    } elseif ($Phase -eq 'stone') {
        $arguments += @('-nullrhi','-ExecCmds="Rogue10m.TestStoneFistAnimations"',('-RogueStoneFistLabel=' + $Label))
        $expected = 'RESULT=STONE_FIST_ANIMATIONS_PASSED'
    } else {
        $arguments += @('-ExecCmds="Rogue10m.TestFighterCombatStance"',('-RogueFighterStanceLabel=' + $Label))
        $expected = 'RESULT=FIGHTER_COMBAT_STANCE_RUNTIME_PASSED'
        if ($Phase -eq 'render') {
            $arguments += @('-RogueFighterStanceCaptureAll',
                '-ini:Engine:[/Script/Engine.RendererSettings]:r.RayTracing=0,r.PathTracing=0')
        } else { $arguments += '-nullrhi' }
    }
}
$editor = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$process = Start-Process -FilePath $editor -ArgumentList $arguments -WindowStyle Hidden -PassThru
Write-Output ($Phase + ' pid=' + $process.Id + ' log=' + $log)
$deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
while (-not $process.WaitForExit(1000)) {
    if ([DateTime]::UtcNow -gt $deadline) { Stop-Process -Id $process.Id; throw ($Phase + ' timed out; log=' + $log) }
}
if ($process.ExitCode -ne 0 -or -not (Test-Path -LiteralPath $log) -or
    -not ([IO.File]::ReadAllText($log).Contains($expected))) { throw ($Phase + ' failed; exit=' + $process.ExitCode + ' log=' + $log) }
Write-Output ($expected + ' exit=' + $process.ExitCode)
