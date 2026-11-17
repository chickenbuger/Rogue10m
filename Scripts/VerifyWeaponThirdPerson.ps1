param([Parameter(Mandatory)][ValidatePattern('^[A-Za-z0-9_]{1,24}$')][string]$Label,[switch]$NullRHI,[int]$TimeoutSeconds=240,[switch]$RequireGreatSwordAuthored)
$ErrorActionPreference='Stop'
if($NullRHI){throw 'This fixture requires rendered screenshots. NullRHI cannot certify weapon visual QA.'}
$root=(Resolve-Path -LiteralPath (Split-Path -Parent $PSScriptRoot)).Path
$project=Join-Path $root 'Rogue10m.uproject'
$out=Join-Path $root ('Feature\doc\evidence\sword-knuckle-20261110\'+$Label)
if(Test-Path -LiteralPath $out){throw 'Fresh evidence directory required'}
if(@(Get-CimInstance Win32_Process | Where-Object {$_.Name -like 'UnrealEditor*' -and $_.CommandLine -and $_.CommandLine.Contains($project)}).Count){throw 'Shared project UE process already running'}
New-Item -ItemType Directory -Path $out|Out-Null
function Saves{@(Get-ChildItem -LiteralPath (Join-Path $root 'Saved\SaveGames') -Filter '*.sav' -File|ForEach-Object{[pscustomobject]@{path=$_.FullName;sha256=(Get-FileHash -LiteralPath $_.FullName).Hash}})}
$before=@(Saves);$before|ConvertTo-Json -Depth 4|Set-Content -LiteralPath (Join-Path $out 'saves-before.json')
$log=Join-Path $out 'engine.log';$exe='D:\Program Files\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$argsList=@(('"'+$project+'"'),'/Game/FirstPerson/Lvl_FirstPerson','-game','-DisablePython','-unattended','-nosplash','-nosound','-NoLiveCoding','-NoRemoteShaderCompile','-language=en','-locale=en-US','-windowed','-ForceRes','-ResX=1280','-ResY=720',('-abslog="'+$log+'"'),('-RogueProfileSaveSlot=Rogue10m_Town_QA_Weapons_'+$Label),('-RogueProgressionSavePrefix=Rogue10m_Progression_QA_Weapons_'+$Label),('-RogueSkillKeySaveSlot=Rogue10m_Fighter_QA_Weapons_'+$Label),('-RogueWeaponLabel='+$Label),'-ExecCmds="t.MaxFPS 60,Rogue10m.TestWeaponThirdPerson"')
if($RequireGreatSwordAuthored){$argsList+='-RogueRequireAuthoredSword'}
if($NullRHI){$argsList+='-nullrhi'}else{$argsList+=@('-d3d11','-RenderOffScreen')}
$p=$null;$failure=$null;$started=$null
try{
 $p=Start-Process -FilePath $exe -ArgumentList $argsList -WindowStyle Hidden -PassThru;$started=$p.StartTime;Write-Output ('Owned weapon-third-person QA PID='+$p.Id+' log='+$log)
 if(-not $p.WaitForExit($TimeoutSeconds*1000)){throw 'Owned weapon third-person UE timeout'};$p.Refresh()
 $result=Get-Content -LiteralPath (Join-Path $root ('Feature\doc\evidence\sword-knuckle-20261110\'+$Label+'\metrics.txt')) -Raw;Write-Output $result
 if($p.ExitCode -ne 0 -or $result -notmatch '(?m)^failures=0\s*$' -or -not(Select-String -LiteralPath $log -Pattern 'RESULT=WEAPON_THIRDPERSON_PASSED' -Quiet)){throw ('Weapon third-person result failed; UE exit='+$p.ExitCode)}
 if(-not $NullRHI){$images=@(Get-ChildItem -LiteralPath (Join-Path $root ('DevLog\20261110_SwordKnuckle\'+$Label)) -Filter '*.png' -File);if($images.Count-ne34 -or @($images|Where-Object{$_.Length-lt1024}).Count){throw ('Expected 34 rendered captures, got '+$images.Count)}}
 $routes=@(Select-String -LiteralPath $log -Pattern 'WEAPON3P ROUTE skill=');$accepts=@(Select-String -LiteralPath $log -Pattern 'WEAPON3P ACCEPT skill=');$completes=@(Select-String -LiteralPath $log -Pattern 'WEAPON3P COMPLETE skill=')
 if($routes.Count-ne10 -or $accepts.Count-ne10 -or $completes.Count-ne10 -or $result -notmatch '(?m)^completed_skills=10\s*$'){throw 'All 10 exact skills require route, acceptance and completed-motion evidence'}
 if(-not(Select-String -LiteralPath $log -Pattern 'WEAPON3P FIXTURE progression=validated_M13_450XP_each' -Quiet)){throw 'Validated catalog fixture evidence missing'}
 $captures=@(Import-Csv -LiteralPath (Join-Path $out 'captures.csv'))
 $requests=@($captures|Where-Object event -eq 'request');$processed=@($captures|Where-Object event -eq 'processed')
 if($requests.Count-ne34 -or $processed.Count-ne34 -or @($requests.file|Select-Object -Unique).Count-ne34){throw 'Expected 34 unique requested and processed captures'}
 foreach($r in $requests){$done=@($processed|Where-Object {$_.file-eq$r.file -and $_.skill-eq$r.skill -and $_.phase-eq$r.phase});if($done.Count-ne1 -or [long]$done[0].bytes-lt1024 -or -not(Test-Path -LiteralPath $r.file)){throw ('Unverified capture: '+$r.file)}}
 if($RequireGreatSwordAuthored){
  foreach($requiredLine in @('require_authored_sword=1','authored_sword_validated=5','sword_first_damage_checks=5','sword_first_damage_surface_gap_limit_cm=10')){if($result -notmatch ('(?m)^'+[regex]::Escape($requiredLine)+'\s*$')){throw ('Strict authored-sword evidence missing: '+$requiredLine)}}
  $firstDamage=@(Select-String -LiteralPath $log -Pattern 'WEAPON3P FIRST_DAMAGE skill=DA_Attack_GreatSword_.* require_authored_sword=1 sword_gate=1 ')
  if($firstDamage.Count-ne5){throw 'Five strict authored-sword first-damage records required'}
  $frames=@(Import-Csv -LiteralPath (Join-Path $out 'frames.csv'));$culture=[Globalization.CultureInfo]::InvariantCulture
  foreach($variant in @('Primary01','Primary02','Primary03','Special','Charged')){
   $skill='DA_Attack_GreatSword_'+$variant
   $hit=@($frames|Where-Object {$_.skill-eq$skill -and [double]::Parse($_.damage,$culture)-gt0.01}|Select-Object -First 1)
   if($hit.Count-ne1){throw ('Missing first-damage CSV row: '+$skill)}
   $gap=[double]::Parse($hit[0].sword_capsule_gap_cm,$culture);$sourceTime=[double]::Parse($hit[0].montage_source_s,$culture)
   if([double]::IsNaN($gap)-or[double]::IsInfinity($gap)-or$gap-gt10-or[double]::IsNaN($sourceTime)-or[double]::IsInfinity($sourceTime)-or$sourceTime-lt0-or$hit[0].montage_source_path-eq'None'){throw ('First HP-drop capsule/timing proxy failed: '+$skill)}
   $sameFrame=@($firstDamage|Where-Object {$_.Line -match ('skill='+[regex]::Escape($skill)+' frame='+[regex]::Escape($hit[0].frame)+' ')})
   if($sameFrame.Count-ne1){throw ('First-damage log/CSV frame mismatch: '+$skill)}
  }
 }
 $errors=@(Select-String -LiteralPath $log -Pattern '\bError:|Fatal error:|Ensure condition failed');if($errors.Count){throw ('Unexpected red output errors='+$errors.Count)}
}catch{$failure=$_}finally{
 if($p-and-not$p.HasExited){$current=Get-Process -Id $p.Id -ErrorAction SilentlyContinue;if($current-and$current.StartTime-eq$started-and$current.Path-eq$exe){Stop-Process -InputObject $current -Force;$null=$p.WaitForExit(10000)}}
 $after=@(Saves);$after|ConvertTo-Json -Depth 4|Set-Content -LiteralPath (Join-Path $out 'saves-after.json');$lookup=@{};foreach($s in $after){$lookup[$s.path]=$s.sha256};$changed=@($before|Where-Object{-not$lookup.ContainsKey($_.path)-or$lookup[$_.path]-ne$_.sha256});$preserved=$changed.Count-eq0
 [pscustomobject]@{label=$Label;before=$before.Count;after=$after.Count;preserved=$preserved;exit=if($p-and$p.HasExited){$p.ExitCode}else{$null};failure=if($failure){$failure.ToString()}else{$null};physical_input=$false;full_run=$false;balance='prototype assumptions';require_greatsword_authored=[bool]$RequireGreatSwordAuthored;sword_surface_gap_limit_cm=10;damage_contract='opt-in: five GreatSword20261110/AM_* and positive delays; first HP-drop world-post-actor-tick blade segment vs actual oriented scaled capsule surface gap <=10cm; abstract capsule intersection/timing proxy only, not skeletal-mesh or physical collision approval; Knuckle not gated';observation_order='source montage/sequence time, raw geometry and HP read in same callback; source/retarget bone age and damage timer order may differ; screenshot deferred';repeat_cases='each case waits for real cooldown and montage/action completion; combo-only cases execute and verify actual Primary precursors';progression_fixture='validated isolated M13 mastery, automatic catalog grants, no preview or slot-policy override';ready_capture='combo case ready is before precursor chain, not exact follow-up ready pose';language_override='process only en/en-US'}|ConvertTo-Json|Set-Content -LiteralPath (Join-Path $out 'launcher.json')
}
if(-not$preserved){throw 'Save preservation failure'};if($failure){throw $failure}
Write-Output ('PASSED weapon third-person numeric/render capture QA; existing saves preserved='+$before.Count+'; inspect actual PNGs separately')