param(
 [Parameter(Mandatory)][ValidatePattern('^[A-Za-z][A-Za-z0-9_]{0,31}$')][string]$Label,
 [Parameter(Mandatory)][string]$AppliedReport,
 [Parameter(Mandatory)][ValidatePattern('^[a-fA-F0-9]{64}$')][string]$AppliedReportSha256,
 [Parameter(Mandatory)][string]$ColdReadbackReport,
 [Parameter(Mandatory)][ValidatePattern('^[a-fA-F0-9]{64}$')][string]$ColdReadbackReportSha256,
 [Parameter(Mandatory)][switch]$CoordinatorLeaseGranted,
 [switch]$ReadabilityCandidate,
 [ValidateRange(30,1800)][int]$TimeoutSeconds=300)
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
if(-not $CoordinatorLeaseGranted){throw 'Coordinator exclusive Editor/build lease is required'}
$root=(Resolve-Path -LiteralPath (Split-Path -Parent $PSScriptRoot)).Path
$project=Join-Path $root 'Rogue10m.uproject'
if(-not(Test-Path -LiteralPath $project)){throw 'Integrate into project Scripts before running'}
# This mutex serializes THIS launcher. Coordinator lease and process guards cover other tools.
$mutex=[Threading.Mutex]::new($false,'Local\Rogue10mStaffVfx20261111')
$owned=$false;$p=$null;$started=$null;$failure=$null;$preserved=$false;$protected=$false;$out=$null
function Saves {
 $dir=Join-Path $root 'Saved\SaveGames'
 if(Test-Path -LiteralPath $dir){@(Get-ChildItem -LiteralPath $dir -File -Recurse | ForEach-Object {[pscustomobject]@{path=$_.FullName;sha256=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash}})}
}
function AssertIdle {
 $busy=@(Get-CimInstance Win32_Process | Where-Object {($_.Name -like 'UnrealEditor*') -or (($_.Name -match 'UnrealBuildTool|MSBuild|dotnet|Build') -and $_.CommandLine -and $_.CommandLine.Contains($root))})
 if($busy.Count){throw ('Editor/build process must be fully stopped before cold runtime launch: '+($busy.ProcessId -join ','))}
}
function VerifyHashes($hashes) {
 foreach($property in $hashes.PSObject.Properties){
  $path=[IO.Path]::GetFullPath((Join-Path $root $property.Name))
  if(-not $path.StartsWith($root+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){throw 'Manifest escapes project'}
  if(-not(Test-Path -LiteralPath $path)-or(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -ne $property.Value){throw ('Pinned protected file changed: '+$property.Name)}
 }
}
function Get-StaffVfxCaptureNumber($Sample,[string]$Field) {
 $property=$Sample.PSObject.Properties[$Field]
 if($null-eq$property-or$null-eq$property.Value-or$property.Value-is[bool]){throw ('Missing or invalid capture number: '+$Field)}
 $raw=[Convert]::ToString($property.Value,[Globalization.CultureInfo]::InvariantCulture)
 $number=0.
 if([string]::IsNullOrWhiteSpace($raw)-or-not[double]::TryParse($raw,[Globalization.NumberStyles]::Float,[Globalization.CultureInfo]::InvariantCulture,[ref]$number)-or[double]::IsNaN($number)-or[double]::IsInfinity($number)){throw ('Finite capture number required: '+$Field)}
 return $number
}
function Assert-StaffVfxCaptureWindow($Request,$Processed) {
 if($null-eq$Request-or$null-eq$Processed){throw 'Request and processed capture rows required'}
 if([string]::IsNullOrWhiteSpace([string]$Request.phase)-or$Processed.phase-ne$Request.phase){throw 'Request and processed phase metadata must match'}
 if($Request.phase-match'^(cast|impact)_age_(010|018)$'){
  $minAge=if($Matches[2]-eq'010'){.10}else{.18};$maxAge=if($Matches[2]-eq'010'){.17}else{.25}
  $requestAge=Get-StaffVfxCaptureNumber $Request 'actual_sim_age_s';$processedAge=Get-StaffVfxCaptureNumber $Processed 'actual_sim_age_s'
  foreach($sample in @($Request,$Processed)){
   if($sample.age_source-ne'FNiagaraSystemInstanceController::GetAge'-or[string]::IsNullOrWhiteSpace([string]$sample.cue_component)-or$sample.cue_component-ne$Request.cue_component){throw 'Request and processed live cue metadata must match'}
   if((Get-StaffVfxCaptureNumber $sample 'cue_min_age_s')-ne$minAge-or(Get-StaffVfxCaptureNumber $sample 'cue_max_age_s')-ne$maxAge){throw 'Request and processed cue windows must match required phase'}
  }
  if($requestAge-lt$minAge-or$requestAge-gt$maxAge-or$processedAge-lt$minAge-or$processedAge-gt$maxAge-or$processedAge-lt$requestAge){throw 'Request and processed cue ages must remain ordered inside required phase window'}
 } elseif($Request.phase-match'^motion_(030|060|100)$'){
  $minimum=[int]$Matches[1]/100.
  $requestElapsed=Get-StaffVfxCaptureNumber $Request 'accepted_elapsed_s';$processedElapsed=Get-StaffVfxCaptureNumber $Processed 'accepted_elapsed_s'
  if($requestElapsed-lt$minimum-or$requestElapsed-gt($minimum+.10)-or$processedElapsed-lt$minimum-or$processedElapsed-gt($minimum+.10)-or$processedElapsed-lt$requestElapsed){throw 'Request and processed motion times must remain ordered inside required elapsed-time window'}
 }
}
function VerifyReadabilityArtifacts {
 # Standalone Python only: reuse the detached full-property/fingerprint contracts.
 # Importing these modules does not dispatch MCP, launch Unreal or write assets.
 $python='D:\Program Files\UE_5.8\Engine\Binaries\ThirdParty\Python3\Win64\python.exe'
 if(-not(Test-Path -LiteralPath $python)){throw 'Bundled Python required for exact readability artifact audit'}
 $audit=@'
import hashlib, json, re, sys
from pathlib import Path
root=Path(sys.argv[1]).resolve()
sys.path[:0]=[str(root/'Scripts'),str(root/'Scripts/Editor')]
import StaffVfxReadability20261111 as contract
import BindMagicSkillVfxMCP20261111 as native
import StageStaffVfxReadability20261111 as recipe
def require(ok,reason):
    if not ok: raise RuntimeError(reason)
def read_pinned(path,sha):
    data=Path(path).read_bytes()
    require(hashlib.sha256(data).hexdigest()==sha.lower(),'Pinned candidate report SHA mismatch')
    return json.loads(data)
a=read_pinned(sys.argv[2],sys.argv[3]);v=read_pinned(sys.argv[4],sys.argv[5])
require(native.ROOT.resolve()==recipe.ROOT.resolve()==root,'Readability audit imports belong to wrong checkout')
require(a['phase']=='activate' and a['status']=='four_property_trial_activated_gameplay_visual_review_pending','Successful activation report required')
require(v['phase']=='verify' and v['status']=='four_property_trial_readback_verified_gameplay_qa_pending','Pinned candidate readback report required')
plan=a['plan'];contract.validate_plan(plan)
require(v['plan']==plan and v['activation_report_sha256']==sys.argv[3].lower(),'Readback activation lineage mismatch')
require(v['inspect_snapshot_sha256']==a['inspect_snapshot_sha256'] and re.fullmatch('[a-f0-9]{64}',a['stage_report_sha256']),'Inspect/stage lineage mismatch')
for report in (a,v):
    require(report['nonvisual_fields_preserved'] is True and report['protected_files_preserved'] is True,'Protected property/file audit required')
    require(report['saved_clean_readback_verified'] is True,'Clean saved readback required')
    require(report['runtime_gameplay_validated'] is False and report['visual_quality_approved'] is False,'Readback must not claim unperformed gameplay/visual approval')
restart=v['cold_editor_restart']
require(v['cold_reload_validated'] is True and restart['old_shutdown_proven'] is True,'Proven editor cold reload required for candidate runtime')
require(restart['old_pid']==a['listener_identity']['pid'] and restart['new_pid']==v['listener_identity']['pid'] and restart['old_pid']!=restart['new_pid'],'Cold reload PID lineage mismatch')
require(a['identity']==v['identity'] and Path(a['identity']['local_project_root']).resolve()==root,'Pinned project identity changed')
baseline=a['live']['properties'];current=v['live']['properties'];pilot=plan['source_asset']
require(contract.validate_runtime_profile(current[pilot],True)=='readability_candidate','Candidate runtime profile required')
base_plan=native.build_binding_plan(root)
expected={row['asset'] for row in base_plan['targets']}|{base_plan['profile_asset'],base_plan['character_asset']}
require(set(baseline)==set(current)==expected,'Exact seven-asset full-property scope required')
audit=contract.audit_transition(baseline[pilot],a['source_properties_after'])
require(a['property_audit']==audit,'Activation full-property audit differs')
require(current[pilot]==a['source_properties_after'],'Readback pilot properties differ from activated snapshot')
require(a['properties_after']==current==v['properties_after'],'Complete activated/readback property scope differs')
require(v['property_audit']==contract.audit_transition(baseline[pilot],current[pilot]),'Readback full-property audit differs')
for asset in expected-{pilot}:
    require(current[asset]==baseline[asset],'Protected full-property snapshot drift: '+asset)
require(a['live']['classes']==v['live']['classes'] and a['live']['project_skills_sha256']==v['live']['project_skills_sha256'],'Class or project skills changed')
pilot_key=native.package_file(pilot).relative_to(root).as_posix()
native.validate_final_fingerprints(a['fingerprints_before'],a['fingerprints_after'],pilot_key,a['saved_asset_sha256'])
hashes=recipe.readability_fingerprints(plan)
require(hashes==a['fingerprints_after']==v['fingerprints_before']==v['fingerprints_after'],'Exact current protected fingerprint scope/hash mismatch')
require(native.file_sha(native.package_file(pilot))==a['saved_asset_sha256'],'Activated pilot file hash changed')
require(a['staged_asset_sha256']==v['staged_asset_sha256']==native.file_sha(native.package_file(plan['staged_asset'])),'Staged candidate file hash changed')
print('READABILITY_RUNTIME_ARTIFACT_AUDIT=PASS')
'@
 & $python -I -c $audit $root $AppliedReport $AppliedReportSha256 $ColdReadbackReport $ColdReadbackReportSha256
 if($LASTEXITCODE-ne0){throw 'Readability full-field/lineage/fingerprint audit failed'}
}
function VerifyBaselinePlacementArtifacts {
 $python='D:\Program Files\UE_5.8\Engine\Binaries\ThirdParty\Python3\Win64\python.exe'
 if(-not(Test-Path -LiteralPath $python)){throw 'Bundled Python required for baseline placement audit'}
 $baselineAudit=@'
import hashlib,json,sys
from pathlib import Path
root=Path(sys.argv[1]).resolve()
sys.path.insert(0,str(root/'Scripts'))
import StaffVfxReadability20261111 as contract
def pinned(path,sha):
    data=Path(path).read_bytes()
    if hashlib.sha256(data).hexdigest()!=sha.lower():raise RuntimeError('Pinned baseline report changed')
    return json.loads(data)
applied=pinned(sys.argv[2],sys.argv[3]);readback=pinned(sys.argv[4],sys.argv[5])
contract.audit_baseline_capture_reports(applied,readback)
print('BASELINE_ZERO_PLACEMENT_AUDIT=PASS')
'@
 & $python -I -c $baselineAudit $root $AppliedReport $AppliedReportSha256 $ColdReadbackReport $ColdReadbackReportSha256
 if($LASTEXITCODE-ne0){throw 'Baseline zero-placement full-property audit failed'}
}
try {
 $owned=$mutex.WaitOne(0);if(-not$owned){throw 'Another Staff VFX launcher owns mutex'}
 AssertIdle
 foreach($pair in @(@($AppliedReport,$AppliedReportSha256),@($ColdReadbackReport,$ColdReadbackReportSha256))){if((Get-FileHash -LiteralPath $pair[0] -Algorithm SHA256).Hash -ne $pair[1]){throw 'Pinned report SHA mismatch'}}
 $applied=Get-Content -LiteralPath $AppliedReport -Raw|ConvertFrom-Json
 $cold=Get-Content -LiteralPath $ColdReadbackReport -Raw|ConvertFrom-Json
 if($ReadabilityCandidate){VerifyReadabilityArtifacts}else{
  if($applied.phase -ne 'apply'-or$applied.data_asset_connected -ne $true-or$applied.protected_files_preserved -ne $true){throw 'Successful pinned applied report required'}
  if($cold.phase -ne 'verify'-or$cold.status -ne 'pilot_binding_readback_verified_gameplay_qa_not_performed'-or$cold.applied_report_sha256 -ne $AppliedReportSha256.ToLowerInvariant()){throw 'Pinned post-restart MCP verify report required'}
  VerifyBaselinePlacementArtifacts
 }
 VerifyHashes $applied.fingerprints_after;VerifyHashes $cold.fingerprints_after
 $out=Join-Path $root ('Feature\doc\evidence\staff-vfx-runtime-20261111\'+$Label)
 $images=Join-Path $root ('Feature\doc\evidence\staff-vfx-runtime-20261111\images\'+$Label)
 if((Test-Path -LiteralPath $out)-or(Test-Path -LiteralPath $images)){throw 'Fresh evidence label required'}
 $token=[Guid]::NewGuid().ToString('N')
 $before=@(Saves)
 if(@($before|Where-Object {$_.path.Contains($token)}).Count){throw 'Save namespace already exists'}
 New-Item -ItemType Directory -Path $out|Out-Null
 $before|ConvertTo-Json -Depth 4|Set-Content -LiteralPath (Join-Path $out 'saves-before.json')
 $log=Join-Path $out 'engine.log';$exe='D:\Program Files\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
 $arguments=@(('"'+$project+'"'),'/Game/FirstPerson/Lvl_FirstPerson','-game','-DisablePython','-unattended','-nosplash','-nosound','-NoLiveCoding','-NoRemoteShaderCompile','-language=en','-locale=en-US','-windowed','-ForceRes','-ResX=1280','-ResY=720','-d3d11','-RenderOffScreen',('-abslog="'+$log+'"'),('-RogueProfileSaveSlot=Rogue10m_Town_QA_StaffVfx_'+$token),('-RogueProgressionSavePrefix=Rogue10m_Progression_QA_StaffVfx_'+$token),('-RogueSkillKeySaveSlot=Rogue10m_Fighter_QA_StaffVfx_'+$token),('-RogueStaffVfxNamespace='+$token),('-RogueStaffVfxLabel='+$Label),'-ExecCmds="t.MaxFPS 60,Rogue10m.TestStaffVfx"')
 if($ReadabilityCandidate){$arguments+='-RogueStaffVfxReadabilityCandidate'}
 AssertIdle
 $p=Start-Process -FilePath $exe -ArgumentList $arguments -WindowStyle Hidden -PassThru;$started=$p.StartTime
 if(-not$p.WaitForExit($TimeoutSeconds*1000)){throw 'Owned Staff VFX runtime timeout'};$p.Refresh()
 $metrics=Get-Content -LiteralPath (Join-Path $out 'metrics.txt') -Raw
 if($p.ExitCode -ne 0-or$metrics -notmatch '(?m)^failures=0\s*$'-or$metrics -notmatch '(?m)^completed_cases=3\s*$'-or -not(Select-String -LiteralPath $log -Pattern 'RESULT=STAFF_VFX_PASSED' -Quiet)){throw 'Runtime gates failed'}
 $captures=@(Import-Csv -LiteralPath (Join-Path $out 'captures.csv'));$requests=@($captures|Where-Object event -eq 'request');$processed=@($captures|Where-Object event -eq 'processed')
 if($requests.Count-ne31-or$processed.Count-ne31-or@($requests.file|Select-Object -Unique).Count-ne31){throw 'Thirty-one unique requested/processed captures required'}
 # Retain every original early/recovery frame and require later samples on the
 # SAME gameplay camera. Later cue age comes from the live Niagara controller.
 $commonPhases=@('ready','acceptance_cue','cast_age_010','cast_age_018','motion_030','motion_060','motion_100','motion_recovery','recovery')
 for($caseIndex=0;$caseIndex-lt3;++$caseIndex){
  $wanted=@($commonPhases);if($caseIndex-eq0){$wanted+=@('after_first_damage','impact_residual','impact_age_010','impact_age_018')}
  $caseRequests=@($requests|Where-Object {[int]$_.case_index-eq$caseIndex})
  if($caseRequests.Count-ne$wanted.Count){throw 'Unexpected phase count for case'}
  foreach($phase in $wanted){if(@($caseRequests|Where-Object phase -eq $phase).Count-ne1){throw ('Missing or duplicate required phase: '+$phase)}}
 }
 $vfx=@(Import-Csv -LiteralPath (Join-Path $out 'vfx-components.csv'))
 Add-Type -AssemblyName System.Drawing
 foreach($r in $requests){
  $done=@($processed|Where-Object {$_.file-eq$r.file-and$_.phase-eq$r.phase})
  if($done.Count-ne1-or[long]$done[0].bytes-lt1024-or(Get-Item -LiteralPath $r.file).Length-ne[long]$done[0].bytes-or$done[0].case_index-ne$r.case_index){throw 'Capture bytes or case identity do not match'}
  if($r.camera_view-ne'default_third_person'-or$done[0].camera_view-ne$r.camera_view){throw 'Default third-person camera provenance required'}
  foreach($sample in @($r,$done[0])){
   foreach($field in @('camera_x','camera_y','camera_z','camera_pitch','camera_yaw','camera_roll')){
    $value=[double]$sample.$field;if([double]::IsNaN($value)-or[double]::IsInfinity($value)){throw 'Finite camera transform required'}
   }
  }
  Assert-StaffVfxCaptureWindow $r $done[0]
  if($r.phase-match'^(cast|impact)_age_(010|018)$'){
   $age=Get-StaffVfxCaptureNumber $r 'actual_sim_age_s'
   $evidence=@($vfx|Where-Object {$_.event-eq$r.phase-and$_.fixture_frame-eq$r.fixture_frame-and$_.component_path-eq$r.cue_component-and$_.eligible-eq'1'})
   if($evidence.Count-ne1-or[double]::IsNaN([double]$evidence[0].actual_sim_age_s)-or[double]::IsInfinity([double]$evidence[0].actual_sim_age_s)-or[math]::Abs([double]$evidence[0].actual_sim_age_s-$age)-gt.000002){throw 'Capture must match same-frame live Niagara telemetry'}
  }
  if($r.phase-eq'motion_recovery'){
   foreach($sample in @($r,$done[0])){
    $position=[double]$sample.montage_s;$duration=[double]$sample.montage_duration_s;$normalized=[double]$sample.montage_normalized_pos
    foreach($value in @($position,$duration,$normalized)){if([double]::IsNaN($value)-or[double]::IsInfinity($value)){throw 'Finite recovery montage position required'}}
    if($sample.montage_playing-ne'1'-or$duration-le0-or$position-le0-or$normalized-lt.90-or$normalized-gt1.-or[math]::Abs($position/$duration-$normalized)-gt.000002){throw 'Recovery capture must remain inside active montage final tenth'}
   }
  }
  $image=[Drawing.Image]::FromFile($r.file)
  try{if($image.Width-ne1280-or$image.Height-ne720-or$image.RawFormat.Guid-ne[Drawing.Imaging.ImageFormat]::Png.Guid){throw 'Expected real 1280x720 PNG'}}finally{$image.Dispose()}
 }
 if(@(Get-ChildItem -LiteralPath $images -Filter '*.png' -File).Count-ne31){throw 'Unexpected PNG count'}
 if(@(Select-String -LiteralPath $log -Pattern '\bError:|Fatal error:|Ensure condition failed').Count){throw 'Engine errors present'}
} catch {$failure=$_} finally {
 if($p-and-not$p.HasExited){$current=Get-Process -Id $p.Id -ErrorAction SilentlyContinue;if($current-and$current.StartTime-eq$started-and$current.Path-eq$exe){Stop-Process -InputObject $current -Force;$null=$p.WaitForExit(10000)}}
 if($out-and(Test-Path -LiteralPath $out)){
  $after=@(Saves);$after|ConvertTo-Json -Depth 4|Set-Content -LiteralPath (Join-Path $out 'saves-after.json')
  $lookup=@{};foreach($s in $after){$lookup[$s.path]=$s.sha256}
  $preserved=@($before|Where-Object{-not$lookup.ContainsKey($_.path)-or$lookup[$_.path]-ne$_.sha256}).Count-eq0
  try{VerifyHashes $applied.fingerprints_after;VerifyHashes $cold.fingerprints_after;if($ReadabilityCandidate){VerifyReadabilityArtifacts}else{VerifyBaselinePlacementArtifacts};$protected=$true}catch{if(-not$failure){$failure=$_}}
  [pscustomobject]@{label=$Label;namespace=$token;readability_candidate=[bool]$ReadabilityCandidate;original_saves_preserved=$preserved;protected_files_preserved=$protected;applied_report=$AppliedReport;applied_sha256=$AppliedReportSha256;cold_readback_report=$ColdReadbackReport;cold_readback_sha256=$ColdReadbackReportSha256;cold_editor_restart_proven_by_this_launcher=$false;runtime_new_process=$true;physical_input=$false;visual_approval=$false;failure=if($failure){$failure.ToString()}else{$null}}|ConvertTo-Json|Set-Content -LiteralPath (Join-Path $out 'launcher.json')
 }
 if($owned){$mutex.ReleaseMutex()};$mutex.Dispose()
}
if($failure){throw $failure};if(-not$preserved-or-not$protected){throw 'Save/protected file preservation failed'}
Write-Output ('PASSED Staff VFX numeric/render capture gates; inspect PNGs separately: '+$out)
