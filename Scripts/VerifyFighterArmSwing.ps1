param(
    [ValidateSet('baseline','apply','reload','combat','stone','render')][string]$Phase = 'reload',
    [string]$Label = '',
    [switch]$FirstPersonOnly,
    [int]$TimeoutSeconds = 240,
    [string]$EngineRoot = 'D:\Program Files\UE_5.8',
    [string]$ProjectRoot = 'D:\Project\Rogue10m'
)
$ErrorActionPreference = 'Stop'
if ([string]::IsNullOrEmpty($Label)) { $Label = 'swing-20261005-' + $Phase }
if ($Label -notmatch '^[A-Za-z0-9_-]+$') { throw 'Invalid evidence label.' }
if ($TimeoutSeconds -lt 30 -or $TimeoutSeconds -gt 600) { throw 'Timeout must be 30..600 seconds.' }
$evidence = Join-Path $ProjectRoot 'Feature\doc\evidence\fighter-arm-swing-20261005'
$log = Join-Path $evidence ($Label + '.log')
if (Test-Path -LiteralPath $log) { throw 'Existing evidence: choose a new label.' }
New-Item -ItemType Directory -Path $evidence -Force | Out-Null
$arguments = @(('"' + (Join-Path $ProjectRoot 'Rogue10m.uproject') + '"'),
    '-unattended','-nosplash','-nosound','-NoLiveCoding','-NoRemoteShaderCompile',('-abslog="' + $log + '"'))
if ($Phase -in @('baseline','apply','reload')) {
    $arguments += @('-nullrhi', ('-ExecutePythonScript="' + (Join-Path $ProjectRoot 'Scripts\Editor\EnhanceFighterArmSwing.py') + '"'))
    $expected = 'RESULT=FIGHTER_ARM_SWING_BASELINE_CAPTURED'
    if ($Phase -eq 'apply') { $arguments += '-RogueFistSwingApply'; $expected = 'RESULT=FIGHTER_ARM_SWING_APPLY_PASSED' }
    if ($Phase -eq 'reload') { $arguments += '-RogueFistSwingAudit'; $expected = 'RESULT=FIGHTER_ARM_SWING_RELOAD_PASSED' }
} else {
    $arguments += @('/Game/FirstPerson/Lvl_FirstPerson','-game','-windowed','-ForceRes','-ResX=1280','-ResY=720',
        '-RogueSkillKeySaveSlot=Rogue10m_Fighter_QA_20261005_Swing')
    if ($Phase -eq 'stone') {
        $arguments += @('-nullrhi','-ExecCmds="Rogue10m.TestStoneFistAnimations"',('-RogueStoneFistLabel=' + $Label))
        $expected = 'RESULT=STONE_FIST_ANIMATIONS_PASSED'
    } else {
        $arguments += @('-ExecCmds="Rogue10m.TestFistCombat"','-RogueFistDamageTarget',('-RogueFistLabel=' + $Label))
        $expected = 'RESULT=FIST_COMBAT_PASSED'
        if ($Phase -eq 'render') {
            $arguments += @('-RogueFistFirstPersonOnly','-RogueFistCaptureAll',
                '-ini:Engine:[/Script/Engine.RendererSettings]:r.RayTracing=0,r.PathTracing=0')
        } else {
            $arguments += '-nullrhi'
            if ($FirstPersonOnly) { $arguments += '-RogueFistFirstPersonOnly' }
        }
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
