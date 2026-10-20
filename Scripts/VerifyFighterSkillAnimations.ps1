param(
    [string]$EngineRoot = 'D:\Program Files\UE_5.8',
    [string]$ProjectRoot = 'D:\Project\Rogue10m',
    [ValidateSet('full','combat','stone')][string]$Phase = 'full',
    [string]$TestSlot = 'Rogue10m_Fighter_QA_20261002_Animations',
    [string]$Label = 'fighter-animations-20261002',
    [switch]$NullRHI
)
$ErrorActionPreference = 'Stop'
if ($TestSlot -notmatch '^Rogue10m_Fighter_QA_[A-Za-z0-9_]+$') { throw 'Isolated QA save slot required.' }
if ($Label -notmatch '^[A-Za-z0-9_-]+$') { throw 'Invalid QA label.' }
$evidence = Join-Path $ProjectRoot 'Feature\doc\evidence\fighter-animation-binding-20261002'
$validation = Get-Content -LiteralPath (Join-Path $evidence 'binding-validation.json') -Raw | ConvertFrom-Json
if ($validation.result -ne 'PASS') { throw 'Run the Editor binding audit first.' }
$tests = @{
    full = @('Rogue10m.TestFighterSkillTree','RESULT=FIGHTER_SKILL_TREE_PASSED mode=full')
    combat = @('Rogue10m.TestFistCombat','RESULT=FIST_COMBAT_PASSED')
    stone = @('Rogue10m.TestStoneFistAnimations','RESULT=STONE_FIST_ANIMATIONS_PASSED')
}
$log = Join-Path $evidence ($Label + '-' + $Phase + '.log')
if (Test-Path -LiteralPath $log) { throw 'Choose a new label; existing evidence is preserved.' }
$arguments = @('"' + (Join-Path $ProjectRoot 'Rogue10m.uproject') + '"','/Game/FirstPerson/Lvl_FirstPerson',
    '-game','-windowed','-ForceRes','-ResX=1920','-ResY=1080','-unattended','-nosplash','-nosound','-NoLiveCoding','-NoRemoteShaderCompile',
    ('-ExecCmds="' + $tests[$Phase][0] + '"'),('-abslog="' + $log + '"'),('-RogueSkillKeySaveSlot=' + $TestSlot))
if ($Phase -eq 'stone') { $arguments += ('-RogueStoneFistLabel=' + $Label) }
if ($NullRHI) { $arguments += '-nullrhi' }
if ($Phase -eq 'combat') {
    $arguments += @('-RogueFistDamageTarget','-RogueFistFirstPersonOnly',('-RogueFistLabel=' + $Label))
    if (-not $NullRHI) { $arguments += '-RogueFistCaptureKeyframes' }
}
# The existing full test writes a fixed 20260930 assertion path. Preserve its bytes,
# collect this run's assertions under 20261002, then restore the original in finally.
$oldAssertions = Join-Path $ProjectRoot 'Feature\doc\evidence\fighter-skill-tree-20260930\runtime-assertions.txt'
$priorBytes = $null
if ($Phase -eq 'full' -and (Test-Path -LiteralPath $oldAssertions)) { $priorBytes = [IO.File]::ReadAllBytes($oldAssertions) }
$process = $null
try {
    $editor = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor.exe'
    $process = Start-Process -FilePath $editor -ArgumentList $arguments -WindowStyle Hidden -PassThru
    Write-Output ($Phase + ' pid=' + $process.Id)
    $deadline = [DateTime]::UtcNow.AddMinutes(5)
    while (-not $process.WaitForExit(1000)) {
        if ([DateTime]::UtcNow -gt $deadline) { Stop-Process -Id $process.Id; throw ('QA timeout: ' + $Phase) }
    }
    if ($Phase -eq 'full' -and (Test-Path -LiteralPath $oldAssertions)) {
        Copy-Item -LiteralPath $oldAssertions -Destination (Join-Path $evidence ($Label+'-runtime-assertions.txt'))
    }
    $body = Get-Content -LiteralPath $log -Raw
    if ($process.ExitCode -ne 0 -or -not $body.Contains($tests[$Phase][1])) { throw ('QA failed: ' + $log) }
    Write-Output $tests[$Phase][1]
} finally {
    if ($priorBytes -ne $null) { [IO.File]::WriteAllBytes($oldAssertions,$priorBytes) }
}
