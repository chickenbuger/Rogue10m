param(
    [ValidateSet('runtime','render','negative')][string]$Phase='runtime',
    [string]$Label='',
    [switch]$Movie,
    [int]$TimeoutSeconds=300,
    [string]$EngineRoot='D:\Program Files\UE_5.8',
    [string]$ProjectRoot='D:\Project\Rogue10m'
)
$ErrorActionPreference='Stop'
if([string]::IsNullOrEmpty($Label)){$Label='mouse_head_20261013_'+$Phase}
if($Label -notmatch '^[A-Za-z0-9_]{1,32}$'){throw 'Evidence label must contain 1..32 ASCII letters, digits or underscores. Hyphens are not allowed in QA save tokens.'}
if($TimeoutSeconds -lt 30 -or $TimeoutSeconds -gt 600){throw 'Timeout must be 30..600 seconds.'}
if($Movie -and $Phase -ne 'render'){throw 'Movie requires the render phase.'}
$skillSlot='Rogue10m_Fighter_QA_StableHead_'+$Label
$progressPrefix='Rogue10m_Progression_QA_StableHead_'+$Label
if($skillSlot.Length -gt 100 -or $progressPrefix.Length -gt 80){throw 'QA save token exceeds the production whitelist length.'}
$evidence=Join-Path $ProjectRoot 'Feature\doc\evidence\mouse-head-stable-locomotion-20261013'
$log=Join-Path $evidence ($Label+'.log')
$metrics=Join-Path $ProjectRoot ('tmp\mouse-head-stable\'+$Label+'\metrics.txt')
if((Test-Path -LiteralPath $log) -or (Test-Path -LiteralPath $metrics)){throw 'Existing evidence: choose a fresh label.'}
New-Item -ItemType Directory -Path $evidence -Force | Out-Null
# Capture the full non-QA save inventory before engine startup. Both persistence
# systems receive strict isolated tokens; additions catch unexpected fallback slots.
$saveRoot=Join-Path $ProjectRoot 'Saved\SaveGames'
function Get-ProductionSaveInventory {
    $inventory=@{}
    if(Test-Path -LiteralPath $saveRoot){Get-ChildItem -LiteralPath $saveRoot -File -Filter '*.sav' | Where-Object {$_.Name -notmatch '_QA_'} | ForEach-Object {$inventory[$_.FullName]=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash}}
    return $inventory
}
function Assert-ProductionSaveInventoryUnchanged {
    $after=Get-ProductionSaveInventory
    $saveChanged=@()
    foreach($entry in $before.GetEnumerator()){if(-not $after.ContainsKey($entry.Key) -or $after[$entry.Key] -ne $entry.Value){$saveChanged+=$entry.Key}}
    foreach($path in $after.Keys){if(-not $before.ContainsKey($path)){$saveChanged+=$path}}
    if($saveChanged.Count){throw ('Production save inventory changed unexpectedly: '+(($saveChanged | Sort-Object -Unique) -join ', '))}
}
$before=Get-ProductionSaveInventory
$execCommands=if($Phase -eq 'render'){'r.MotionBlurQuality 0,Rogue10m.TestMouseHeadStableMotion'}else{'Rogue10m.TestMouseHeadStableMotion'}
$arguments=@(('"'+(Join-Path $ProjectRoot 'Rogue10m.uproject')+'"'),'/Game/FirstPerson/Lvl_FirstPerson','-game','-unattended','-nosplash','-nosound',
    '-NoLiveCoding','-NoRemoteShaderCompile','-windowed','-ForceRes','-ResX=1280','-ResY=720',
    ('-RogueSkillKeySaveSlot='+$skillSlot),('-RogueProgressionSavePrefix='+$progressPrefix),
    ('-ExecCmds="'+$execCommands+'"'),('-RogueMouseHeadLabel='+$Label),('-abslog="'+$log+'"'))
if($Phase -eq 'render'){$arguments+=@('-RogueMouseHeadCapture','-ini:Engine:[/Script/Engine.RendererSettings]:r.RayTracing=0,r.PathTracing=0');if($Movie){$arguments+='-RogueMouseHeadMovie'}}else{$arguments+='-nullrhi'}
if($Phase -eq 'negative'){$arguments+='-RogueMouseHeadDisabledControl'}
$editor=Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$process=$null
try {
    $process=Start-Process -FilePath $editor -ArgumentList $arguments -WindowStyle Hidden -PassThru
    Write-Output ($Phase+' pid='+$process.Id+' log='+$log)
    $deadline=[DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    while(-not $process.WaitForExit(1000)){if([DateTime]::UtcNow -gt $deadline){throw ($Phase+' timed out; log='+$log)}}
    $expected=if($Phase -eq 'negative'){'RESULT=MOUSE_HEAD_STABLE_FAILED'}else{'RESULT=MOUSE_HEAD_STABLE_PASSED'}
    $validExit=if($Phase -eq 'negative'){$process.ExitCode -in @(0,1)}else{$process.ExitCode -eq 0}
    if(-not $validExit -or -not (Test-Path -LiteralPath $log) -or -not ([IO.File]::ReadAllText($log).Contains($expected)) -or -not (Test-Path -LiteralPath $metrics))
    {throw ($Phase+' failed; exit='+$process.ExitCode+' log='+$log)}
    if($Phase -eq 'negative'){$observed=[IO.File]::ReadAllText($log);if(-not $observed.Contains('source_head_exceeds_mouse_residual_quaternion_cap')){throw 'Disabled-head negative control failed for an unrelated reason.'}}
} finally {
    if($null -ne $process -and -not $process.HasExited){Stop-Process -Id $process.Id; $process.WaitForExit(5000) | Out-Null}
    Assert-ProductionSaveInventoryUnchanged
}
Write-Output ($expected+' exit='+$process.ExitCode+' metrics='+$metrics+' production_save_inventory_unchanged='+$before.Count)
