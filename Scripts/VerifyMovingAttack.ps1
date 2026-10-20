param(
    [ValidateSet('runtime','render')][string]$Phase = 'runtime',
    [string]$Label = '',
    [switch]$NeutralAim,
    [int]$TimeoutSeconds = 300,
    [string]$EngineRoot = 'D:\Program Files\UE_5.8',
    [string]$ProjectRoot = 'D:\Project\Rogue10m'
)
$ErrorActionPreference = 'Stop'
if ([string]::IsNullOrEmpty($Label)) { $Label = 'moving-attack-20261009-' + $Phase }
if ($Label -notmatch '^[A-Za-z0-9_-]+$') { throw 'Invalid evidence label.' }
if ($TimeoutSeconds -lt 30 -or $TimeoutSeconds -gt 600) { throw 'Timeout must be 30..600 seconds.' }
$evidence = Join-Path $ProjectRoot 'Feature\doc\evidence\moving-upper-body-attack-20261009'
$log = Join-Path $evidence ($Label + '.log')
$metrics = Join-Path $ProjectRoot ('tmp\moving-attack\' + $Label + '\metrics.txt')
if ((Test-Path -LiteralPath $log) -or (Test-Path -LiteralPath $metrics)) { throw 'Existing evidence: choose a fresh label.' }
New-Item -ItemType Directory -Path $evidence -Force | Out-Null
$arguments = @(('"' + (Join-Path $ProjectRoot 'Rogue10m.uproject') + '"'),
    '/Game/FirstPerson/Lvl_FirstPerson','-game','-unattended','-nosplash','-nosound',
    '-NoLiveCoding','-NoRemoteShaderCompile','-windowed','-ForceRes','-ResX=1280','-ResY=720',
    '-RogueSkillKeySaveSlot=Rogue10m_Fighter_QA_20261009_MovingAttack',
    '-RogueProgressionSavePrefix=Rogue10m_Progression_QA_ViewAimMoving',
    '-ExecCmds="Rogue10m.TestMovingAttack"',('-RogueMovingAttackLabel=' + $Label),('-abslog="' + $log + '"'))
if ($NeutralAim) { $arguments += '-RogueMovingAttackNeutralAim' }
if ($Phase -eq 'render') {
    $arguments += @('-RogueMovingAttackCapture','-ini:Engine:[/Script/Engine.RendererSettings]:r.RayTracing=0,r.PathTracing=0')
} else { $arguments += '-nullrhi' }
$editor = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$process = Start-Process -FilePath $editor -ArgumentList $arguments -WindowStyle Hidden -PassThru
Write-Output ($Phase + ' pid=' + $process.Id + ' log=' + $log)
$deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
while (-not $process.WaitForExit(1000)) {
    if ([DateTime]::UtcNow -gt $deadline) { Stop-Process -Id $process.Id; throw ($Phase + ' timed out; log=' + $log) }
}
$expected = 'RESULT=MOVING_ATTACK_PASSED'
if ($process.ExitCode -ne 0 -or -not (Test-Path -LiteralPath $log) -or
    -not ([IO.File]::ReadAllText($log).Contains($expected)) -or -not (Test-Path -LiteralPath $metrics)) {
    throw ($Phase + ' failed; exit=' + $process.ExitCode + ' log=' + $log)
}
Write-Output ($expected + ' exit=' + $process.ExitCode + ' metrics=' + $metrics)
