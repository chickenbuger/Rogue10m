param(
 [Parameter(Mandatory)][ValidatePattern('^[A-Za-z0-9_]{1,24}$')][string]$Label,
 [ValidateRange(30,600)][int]$TimeoutSeconds=240,
 [switch]$NullRHI
)
$ErrorActionPreference='Stop'
if($NullRHI){throw 'Staff presentation requires eight rendered PNGs; NullRHI is not supported.'}
$root=(Resolve-Path -LiteralPath (Split-Path -Parent $PSScriptRoot)).Path
$project=Join-Path $root 'Rogue10m.uproject'
$out=Join-Path $root ('Feature\doc\evidence\staff-equipment-20261110\'+$Label)
$imagesDir=Join-Path $root ('DevLog\20261110_StaffPresentation\'+$Label)
if((Test-Path -LiteralPath $out)-or(Test-Path -LiteralPath $imagesDir)){throw 'Fresh evidence and image directories required; choose a new label.'}
if(@(Get-CimInstance Win32_Process | Where-Object {$_.Name -like 'UnrealEditor*' -and $_.CommandLine -and $_.CommandLine.Contains($project)}).Count){throw 'Shared project UE process already running; root must serialize the editor/game lease.'}
$token=$Label+'_'+[guid]::NewGuid().ToString('N').Substring(0,12)
$profile='Rogue10m_Town_QA_Staff_'+$token
$progression='Rogue10m_Progression_QA_Staff_'+$token
$keys='Rogue10m_Fighter_QA_Staff_'+$token
$saveDir=Join-Path $root 'Saved\SaveGames'
function Saves {
 if(Test-Path -LiteralPath $saveDir){@(Get-ChildItem -LiteralPath $saveDir -Filter '*.sav' -File -Recurse | ForEach-Object {[pscustomobject]@{path=$_.FullName;sha256=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash}})}
}
$before=@(Saves)
if(@($before|Where-Object {$_.path.Contains($token)}).Count){throw 'QA save namespace unexpectedly exists.'}
# Guard the 17 shared/source animation packages, five owned staff source packages and three candidate packages.
# No full licensed pose exports are copied into this fixture's evidence.
$stage=Get-Content -LiteralPath (Join-Path $root 'Feature\doc\evidence\staff-animation-stage-20261110\StaffResumeC\animation-stage.json') -Raw | ConvertFrom-Json
$staff=Get-Content -LiteralPath (Join-Path $root 'Feature\doc\evidence\staff-equipment-20261110\StaffMeshDerivativeA\manifest.json') -Raw | ConvertFrom-Json
$protected=@($stage.original_files_before_sha256.PSObject.Properties.Name)
$protected+=@($staff.source_before.PSObject.Properties.Name|ForEach-Object {Join-Path (Join-Path $root 'Content\BattleWizardPolyart') $_})
$protected+=@(
 (Join-Path $root 'Content\Weapons\Staff20261110\SM_BattleWizardStaff01.uasset'),
 (Join-Path $root 'Content\Rogue10m\Animation\Combat\Staff20261110\StaffResumeC\A_Staff_StaffStrike_Manny.uasset'),
 (Join-Path $root 'Content\Rogue10m\Animation\Combat\Staff20261110\StaffResumeC\A_Staff_StaffShot_Manny.uasset')
)
$protected=@($protected|Sort-Object -Unique)
function Assets { @($protected|ForEach-Object {[pscustomobject]@{path=$_;sha256=(Get-FileHash -LiteralPath $_ -Algorithm SHA256).Hash}}) }
$assetsBefore=@(Assets)
if($assetsBefore.Count-lt25){throw 'Expected 25 protected original/candidate packages.'}
New-Item -ItemType Directory -Path $out | Out-Null
ConvertTo-Json -InputObject $before -Depth 4 | Set-Content -LiteralPath (Join-Path $out 'saves-before.json')
ConvertTo-Json -InputObject $assetsBefore -Depth 4 | Set-Content -LiteralPath (Join-Path $out 'assets-before.json')
$log=Join-Path $out 'engine.log'
$exe='D:\Program Files\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$argsList=@(('"'+$project+'"'),'/Game/FirstPerson/Lvl_FirstPerson','-game','-DisablePython','-unattended','-nosplash','-nosound','-NoLiveCoding','-NoRemoteShaderCompile','-language=en','-locale=en-US','-windowed','-ForceRes','-ResX=1280','-ResY=720','-d3d11','-RenderOffScreen',('-abslog="'+$log+'"'),('-RogueProfileSaveSlot='+$profile),('-RogueProgressionSavePrefix='+$progression),('-RogueSkillKeySaveSlot='+$keys),('-RogueStaffSaveNamespace='+$token),('-RogueStaffLabel='+$Label),'-ExecCmds="t.MaxFPS 60,Rogue10m.TestStaffPresentation"')
$p=$null;$failure=$null;$started=$null;$preserved=$false;$assetsPreserved=$false
try {
 $p=Start-Process -FilePath $exe -ArgumentList $argsList -WindowStyle Hidden -PassThru
 $started=$p.StartTime
 [pscustomobject]@{pid=$p.Id;start=$started.ToString('o');exe=$exe;project=$project;arguments=$argsList} | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $out 'owned-process.json')
 Write-Output ('Owned Staff presentation QA PID='+$p.Id+' log='+$log)
 if(-not$p.WaitForExit($TimeoutSeconds*1000)){throw 'Owned Staff presentation UE timeout.'}
 $p.Refresh()
 $result=Get-Content -LiteralPath (Join-Path $out 'metrics.txt') -Raw
 Write-Output $result
 if($p.ExitCode-ne0-or-not(Select-String -LiteralPath $log -SimpleMatch 'RESULT=STAFF_PRESENTATION_PASSED' -Quiet)){throw ('Staff presentation result failed; UE exit='+$p.ExitCode)}
 foreach($line in @('failures=0','completed_clips=2','requested_captures=8','completed_captures=8','return_checks=1','combat_request=0','inventory_equip_proof=1','damage_proof=0','frozen_pose=0','visual_approval=0','native_default_integration=1')){
  if($result-notmatch('(?m)^'+[regex]::Escape($line)+'\s*$')){throw ('Required metric missing: '+$line)}
 }
 $images=@(Get-ChildItem -LiteralPath $imagesDir -Filter '*.png' -File)
 if($images.Count-ne8-or@($images|Where-Object Length -lt 1024).Count){throw ('Expected eight non-empty rendered PNGs, got '+$images.Count)}
 $captures=@(Import-Csv -LiteralPath (Join-Path $out 'captures.csv'))
 $requests=@($captures|Where-Object event -eq 'request');$processed=@($captures|Where-Object event -eq 'processed')
 if($requests.Count-ne8-or$processed.Count-ne8-or@($requests.file|Select-Object -Unique).Count-ne8){throw 'Expected eight unique requested/processed capture pairs.'}
 $culture=[Globalization.CultureInfo]::InvariantCulture
 foreach($case in @('StaffStrike','StaffShot')){
  foreach($phase in @('ready','windup','action','recovery')){
   $r=@($requests|Where-Object {$_.case-eq$case-and$_.phase-eq$phase})
   if($r.Count-ne1){throw ('Missing unique phase: '+$case+'/'+$phase)}
   $done=@($processed|Where-Object {$_.file-eq$r[0].file-and$_.case-eq$case-and$_.phase-eq$phase})
   if($done.Count-ne1-or[long]$done[0].bytes-lt1024-or-not(Test-Path -LiteralPath $r[0].file)){throw ('Unverified screenshot: '+$r[0].file)}
   $camera=if($case-eq'StaffStrike'-and$phase-eq'ready'){'default_third_person'}else{'fixed_front_side'}
   if($r[0].camera-ne$camera){throw ('Unexpected capture camera: '+$case+'/'+$phase)}
   if($phase-in@('windup','action')-and($r[0].playing-ne'1'-or[double]::Parse($r[0].montage_s,$culture)-le0)){throw ('Active continuous montage not proven for '+$case+'/'+$phase)}
   if($phase-in@('ready','recovery')-and$r[0].playing-ne'0'){throw ('Expected idle/naturally ended montage for '+$case+'/'+$phase)}
  }
 }
 $frames=@(Import-Csv -LiteralPath (Join-Path $out 'frames.csv'))
 foreach($case in @('StaffStrike','StaffShot')){
  $active=@($frames|Where-Object {$_.case-eq$case-and$_.playing-eq'1'})
  $complete=@(Select-String -LiteralPath $log -Pattern ('STAFF_PRESENTATION COMPLETE case='+$case+' '))
  if($active.Count-lt20-or$complete.Count-ne1){throw ('Continuous playback evidence missing: '+$case)}
  foreach($row in $active){
   $gap=[double]::Parse($row.right_palm_shaft_cm,$culture);$shift=[double]::Parse($row.actor_shift_cm,$culture);$angle=[double]::Parse($row.actor_angle_deg,$culture)
   if([double]::IsNaN($gap)-or[double]::IsInfinity($gap)-or$gap-gt15-or$shift-gt1-or$angle-gt0.1){throw ('Attachment/movement proxy failed for '+$case)}
  }
 }
 $errors=@(Select-String -LiteralPath $log -Pattern '\bError:|Fatal error:|Ensure condition failed')
 if($errors.Count){throw ('Unexpected engine errors='+$errors.Count)}
} catch {$failure=$_} finally {
 if($p-and-not$p.HasExited){
  $current=Get-Process -Id $p.Id -ErrorAction SilentlyContinue
  if($current-and$current.StartTime-eq$started-and$current.Path-eq$exe){Stop-Process -InputObject $current -Force;$null=$p.WaitForExit(10000)}
 }
 $after=@(Saves);ConvertTo-Json -InputObject $after -Depth 4 | Set-Content -LiteralPath (Join-Path $out 'saves-after.json')
 $lookup=@{};foreach($s in $after){$lookup[$s.path]=$s.sha256}
 $changed=@($before|Where-Object {-not$lookup.ContainsKey($_.path)-or$lookup[$_.path]-ne$_.sha256});$preserved=$changed.Count-eq0
 try {
  $assetsAfter=@(Assets);ConvertTo-Json -InputObject $assetsAfter -Depth 4 | Set-Content -LiteralPath (Join-Path $out 'assets-after.json')
  $assetLookup=@{};foreach($a in $assetsAfter){$assetLookup[$a.path]=$a.sha256}
  $assetsPreserved=@($assetsBefore|Where-Object {-not$assetLookup.ContainsKey($_.path)-or$assetLookup[$_.path]-ne$_.sha256}).Count-eq0
 } catch {if(-not$failure){$failure=$_}}
 [pscustomobject]@{
  label=$Label;save_namespace=$token;profile_slot=$profile;progression_prefix=$progression;skill_key_slot=$keys
  original_saves=$before.Count;after_saves=$after.Count;saves_preserved=$preserved;protected_assets=$assetsBefore.Count;assets_preserved=$assetsPreserved
  exit=if($p-and$p.HasExited){$p.ExitCode}else{$null};failure=if($failure){$failure.ToString()}else{$null}
  scope='Actual game Actor, normal appearance/ASC, native Staff defaults without overrides, continuous Common_Unarmed DefaultSlot montages. Synthetic-item real grid inventory transaction, not Combat requests, damage, production-item save restart or physical-input testing.'
  frozen_pose=$false;visual_approval=$false;native_defaults_changed=$false;native_default_integration=$true;asset_saves_requested=$false
  capture_contract='Strict eight async requested/processed PNG pairs; request and completion times retained; first ready default third-person, remaining seven fixture front-side camera.'
  geometry_contract='Finite local-Y model bounds centerline to right palm midpoint <=15 cm while playing; left gap recorded, not gated; coarse proxy, not mesh contact approval.'
  source_pose_exports_copied=$false;public_redistribution_verified=$false
 } | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $out 'launcher.json')
}
if(-not$preserved){throw 'Original save SHA256 preservation failed.'}
if(-not$assetsPreserved){throw 'Protected source/candidate package SHA256 preservation failed.'}
if($failure){throw $failure}
Write-Output ('PASSED Staff continuous presentation numeric/capture checks; eight PNGs still require visual review. Original saves preserved='+$before.Count)
