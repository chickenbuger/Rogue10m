param(
    [string]$EngineRoot = 'D:\Program Files\UE_5.8',
    [string]$ProjectRoot = 'D:\Project\Rogue10m',
    [ValidateSet('full','persistence','weapons','combat')][string]$Phase = 'full',
    [string]$TestSlot = 'Rogue10m_Fighter_QA_20260930',
    [string]$Label = 'fighter'
)
$ErrorActionPreference = 'Stop'
if ($TestSlot -notmatch '^Rogue10m_Fighter_QA_[A-Za-z0-9_]+$' -or $TestSlot.Length -gt 100) { throw 'Use an isolated fighter QA save slot.' }
if ($Label -notmatch '^[A-Za-z0-9_-]+$') { throw 'Invalid verification label.' }
$tests = @{
    full = @('Rogue10m.TestFighterSkillTree', 'RESULT=FIGHTER_SKILL_TREE_PASSED mode=full')
    persistence = @('Rogue10m.TestFighterSkillKeyPersistence', 'RESULT=FIGHTER_SKILL_TREE_PASSED mode=persistence')
    weapons = @('Rogue10m.TestWeaponSkillTreeSwitching', 'RESULT=WEAPON_SKILL_TREE_PASSED')
    combat = @('Rogue10m.TestFistCombat', 'RESULT=FIST_COMBAT_PASSED')
}
$evidence = Join-Path $ProjectRoot 'Feature\doc\evidence\fighter-skill-tree-20260930'
New-Item -ItemType Directory -Path $evidence -Force | Out-Null
$log = Join-Path $evidence ($Label + '-' + $Phase + '.log')
$arguments = @('"' + (Join-Path $ProjectRoot 'Rogue10m.uproject') + '"', '/Game/FirstPerson/Lvl_FirstPerson',
    '-game', '-windowed', '-ForceRes', '-ResX=1920', '-ResY=1080', '-unattended', '-nosplash', '-nosound', '-NoLiveCoding', '-NoRemoteShaderCompile',
    ('-ExecCmds="' + $tests[$Phase][0] + '"'), ('-abslog="' + $log + '"'), ('-RogueSkillKeySaveSlot=' + $TestSlot))
if ($Phase -eq 'combat') { $arguments += @('-RogueFistDamageTarget','-RogueFistFirstPersonOnly',('-RogueFistLabel=' + $Label + '-regression')) }
$editor = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor.exe'
$process = Start-Process -FilePath $editor -ArgumentList $arguments -WindowStyle Hidden -PassThru
Write-Output ($Phase + ' pid=' + $process.Id)
if (-not $process.WaitForExit(300000)) { Stop-Process -Id $process.Id; throw ('Verification timeout: ' + $Phase) }
$body = Get-Content -LiteralPath $log -Raw
if ($process.ExitCode -ne 0 -or -not $body.Contains($tests[$Phase][1])) { throw ('Verification failed: ' + $log) }
Write-Output $tests[$Phase][1]
