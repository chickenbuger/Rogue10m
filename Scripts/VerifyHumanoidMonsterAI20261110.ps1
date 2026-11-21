param(
 [Parameter(Mandatory)][ValidatePattern('^[A-Za-z0-9_]{1,24}$')][string]$Label,
 [int]$TimeoutSeconds=240,
 [switch]$ThornWalk,
 [switch]$SporelingWalk,
 [switch]$SporelingSavedProfile,
 [ValidateSet('VerdantSlime','CrimsonSlime','IronShell')][string]$OrdinaryWalk,
 [switch]$OrdinarySavedProfile,
 [switch]$MonsterDeathMotion,
 [switch]$MonsterDeathRender,
 [switch]$MonsterLivingMotion,
 [switch]$MonsterLivingRender
)
$ErrorActionPreference='Stop';$root=(Resolve-Path -LiteralPath (Split-Path -Parent $PSScriptRoot)).Path;$project=Join-Path $root 'Rogue10m.uproject'
if (($ThornWalk -and $SporelingWalk) -or ($SporelingSavedProfile -and -not $SporelingWalk)) {
 throw 'Select one walk fixture. SporelingSavedProfile requires SporelingWalk.'
}
if(($OrdinaryWalk -and ($ThornWalk -or $SporelingWalk)) -or ($OrdinarySavedProfile -and -not $OrdinaryWalk)){
 throw 'Select one gait fixture and specify an ordinary species for a saved-profile check.'
}
if($MonsterDeathRender){$MonsterDeathMotion=$true}
if($MonsterDeathMotion -and ($OrdinaryWalk -or $ThornWalk -or $SporelingWalk)){throw 'MonsterDeathMotion requires a separate run.'}
if($MonsterLivingRender){$MonsterLivingMotion=$true}
if($MonsterLivingMotion -and ($MonsterDeathMotion -or $OrdinaryWalk -or $ThornWalk -or $SporelingWalk)){
 throw 'MonsterLivingMotion requires a separate run.'
}
if(@(Get-CimInstance Win32_Process|Where-Object {$_.Name -like 'UnrealEditor*' -and $_.CommandLine -and $_.CommandLine.Contains($project)}).Count){throw 'Shared project UE process running'}
$out=Join-Path $root ('Feature\doc\evidence\humanoid-boss-npc-20261110\'+$Label);if(Test-Path -LiteralPath $out){throw 'Fresh output required'};New-Item -ItemType Directory -Path $out|Out-Null
function Saves {
 $directory=Join-Path $root 'Saved\SaveGames'
 if(Test-Path -LiteralPath $directory -PathType Container){
  Get-ChildItem -LiteralPath $directory -File -Filter '*.sav'|ForEach-Object {[pscustomobject]@{path=$_.FullName;sha256=(Get-FileHash -LiteralPath $_.FullName).Hash}}
 }
}
$before=@(Saves);$before|ConvertTo-Json -Depth 3|Set-Content -LiteralPath (Join-Path $out 'saves-before.json');$log=Join-Path $out 'engine.log';$slot='Rogue10m_Town_QA_MonsterFX_'+$Label
$exe='D:\Program Files\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$map=if($ThornWalk -or $SporelingWalk -or $OrdinaryWalk -or $MonsterDeathMotion -or $MonsterLivingMotion){'/Game/Rogue10m/Maps/StartHub'}else{'/Game/Rogue10m/Maps/OpenRunMap'}
$gameCommands=if($MonsterLivingRender){'t.MaxFPS 60,r.MotionBlurQuality 0,Rogue10m.TestMonsterChaseEffects'}else{'t.MaxFPS 60,Rogue10m.TestMonsterChaseEffects'}
$a=@(('"'+$project+'"'),$map,'-game','-DisablePython','-language=en','-locale=en-US','-windowed','-ForceRes','-ResX=1280','-ResY=720','-unattended','-nosplash','-nosound','-NoLiveCoding','-NoRemoteShaderCompile',('-RogueProfileSaveSlot='+$slot),('-RogueProgressionSavePrefix=Rogue10m_Progression_QA_MonsterFX_'+$Label),('-RogueSkillKeySaveSlot=Rogue10m_Fighter_QA_MonsterFX_'+$Label),('-ExecCmds="'+$gameCommands+'"'),('-abslog="'+$log+'"'))
if ($MonsterDeathRender -or $MonsterLivingRender) { $a+=@('-RenderOffScreen','-d3d11','-UseFixedTimeStep','-FPS=60','-AllowSoftwareRendering','-graphicsadapter=1') }
elseif ($ThornWalk -or $SporelingWalk -or $OrdinaryWalk -or $MonsterDeathMotion -or $MonsterLivingMotion) { $a+=@('-NullRHI','-UseFixedTimeStep','-FPS=60') }
else { $a+=@('-RenderOffScreen','-d3d11') }
if ($ThornWalk) { $a+='-RogueThornWardenWalkQA' }
if ($SporelingWalk) { $a+='-RogueSporelingWalkQA' }
if ($SporelingSavedProfile) { $a+='-RogueSporelingSavedProfile' }
if ($OrdinaryWalk) { $a+='-RogueOrdinaryGaitQA='+$OrdinaryWalk }
if ($OrdinarySavedProfile) { $a+='-RogueOrdinaryGaitSaved' }
if ($MonsterDeathMotion) { $a+='-RogueMonsterDeathMotionQA' }
if ($MonsterDeathRender) { $a+='-RogueMonsterDeathRender' }
if ($MonsterLivingMotion) { $a+='-RogueMonsterLivingTransitionsQA' }
if ($MonsterLivingRender) { $a+='-RogueMonsterLivingRender' }
$p=$null;$failure=$null;$started=$null
try {
 $p=Start-Process -FilePath $exe -ArgumentList $a -WindowStyle Hidden -PassThru;$started=$p.StartTime;Write-Output ('Owned monsterAI PID='+$p.Id)
 if(-not $p.WaitForExit($TimeoutSeconds*1000)){throw 'Owned AI timeout'}
 $p.Refresh()
 if($p.ExitCode -ne 0 -or -not(Select-String -LiteralPath $log -Pattern 'RESULT=MONSTER_FX_PASSED' -Quiet)){throw 'AI result failed or incomplete'}
 $errors=@(Select-String -LiteralPath $log -Pattern '\bError:|Fatal error:|Ensure condition failed')
 if($MonsterDeathRender -or $MonsterLivingRender){
  $adapter=Select-String -LiteralPath $log -Pattern 'LogRHI:\s+Name:\s+Microsoft Basic Render Driver' | Select-Object -First 1
  if(-not $adapter){throw 'Monster transition captures require the software adapter.'}
  $probes=@($errors|Where-Object {$_.LineNumber -lt $adapter.LineNumber -and $_.Line -match 'LogD3D11RHI: Error: Result failed with error 887A0004\s*$'})
  foreach($probe in $probes){Write-Output ('Hardware adapter probe recorded at line '+$probe.LineNumber)}
  $errors=@($errors|Where-Object {$_.LineNumber -notin $probes.LineNumber})
 }
 if($errors.Count){throw 'Unexpected red output'}
} catch {$failure=$_} finally {if($p -and -not $p.HasExited){$now=Get-Process -Id $p.Id -ErrorAction SilentlyContinue;if($now -and $now.StartTime -eq $started -and $now.Path -eq $exe){Stop-Process -InputObject $now -Force;$null=$p.WaitForExit(10000)}};$after=@(Saves);$after|ConvertTo-Json -Depth 3|Set-Content -LiteralPath (Join-Path $out 'saves-after.json');$lookup=@{};foreach($s in $after){$lookup[$s.path]=$s.sha256};$changed=@($before|Where-Object {-not $lookup.ContainsKey($_.path) -or $lookup[$_.path] -ne $_.sha256});$preserved=$changed.Count -eq 0;[pscustomobject]@{label=$Label;preserved=$preserved;before=$before.Count;after=$after.Count;exit=if($p -and $p.HasExited){$p.ExitCode}else{$null};failure=if($failure){$failure.ToString()}else{$null};input='programmatic existing MonsterChaseEffects fixture';map_saved=$false}|ConvertTo-Json|Set-Content -LiteralPath (Join-Path $out 'launcher.json')}
if(-not $preserved){throw 'Save preservation failure'}
if($failure){throw $failure}
if($ThornWalk){Write-Output 'PASSED ThornWarden blended locomotion and legacy attack recovery'}
elseif($SporelingWalk){Write-Output 'PASSED Sporeling paired locomotion and basic attack recovery'}
elseif($OrdinaryWalk){Write-Output ('PASSED paired gait, attack and hit recovery observations for '+$OrdinaryWalk)}
elseif($MonsterDeathRender){Write-Output 'PASSED matched ThornWarden walking death WARP captures'}
elseif($MonsterDeathMotion){Write-Output 'PASSED six-species idle, walk and attack lethal-pose observations'}
elseif($MonsterLivingRender){Write-Output 'PASSED ThornWarden living attack and hit WARP captures'}
elseif($MonsterLivingMotion){Write-Output 'PASSED six-species idle and walking attack and hit motion observations'}
else{Write-Output 'PASSED natural sight/chase/effects regression'}
