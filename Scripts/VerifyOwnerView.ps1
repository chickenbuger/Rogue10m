param(
 [Parameter(Mandatory=$true)][ValidatePattern('^[A-Za-z0-9_]{1,24}$')][string]$Label,
 [ValidateSet('Original','Weighted')][string]$Profile='Weighted',
 [switch]$Rendered,[switch]$Offscreen,[switch]$VisibleWindow,
 [ValidateRange(60,360)][int]$TimeoutSeconds=300,
 [string]$ProjectRoot=(Split-Path -Parent $PSScriptRoot)
)
$ErrorActionPreference='Stop'
function Read-BomText([string]$Path) {$reader=[IO.StreamReader]::new($Path,[Text.Encoding]::UTF8,$true);try{return $reader.ReadToEnd()}finally{$reader.Dispose()}}
function Save-Inventory([string]$Directory) {return @(Get-ChildItem -LiteralPath $Directory -Filter '*.sav' -File -Recurse | Sort-Object FullName | ForEach-Object {[pscustomobject]@{Path=$_.FullName;SHA256=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash}})}
if($Offscreen -and -not $Rendered){throw 'Offscreen requires Rendered.'}
if($VisibleWindow -and (-not $Rendered -or $Offscreen)){throw 'VisibleWindow requires native Rendered mode.'}
$ProjectRoot=(Resolve-Path -LiteralPath $ProjectRoot).Path
$town='Rogue10m_Town_QA_OwnerView_'+$Label
$prefix='Rogue10m_Progression_QA_OwnerView_'+$Label
$keys='Rogue10m_Fighter_QA_OwnerView_'+$Label
$saves=Join-Path $ProjectRoot 'Saved\SaveGames'
$base=Join-Path $ProjectRoot 'Feature\doc\evidence\owner-view-20261029'
$output=Join-Path $base $Label
$images=Join-Path $ProjectRoot ('DevLog\20261029_OwnerView_'+$Label)
$log=Join-Path $base ($Label+'.log')
$beforeFile=Join-Path $base ($Label+'.saves-before.csv')
$afterFile=Join-Path $base ($Label+'.saves-after.csv')
if((Test-Path -LiteralPath $output) -or (Test-Path -LiteralPath $images) -or (Test-Path -LiteralPath $log) -or
 (Test-Path -LiteralPath $beforeFile) -or (Test-Path -LiteralPath $afterFile) -or
 (Test-Path -LiteralPath (Join-Path $saves ($town+'.sav'))) -or (Test-Path -LiteralPath (Join-Path $saves ($keys+'.sav'))) -or
 (Get-ChildItem -LiteralPath $saves -File -Filter ($prefix+'_*') -ErrorAction SilentlyContinue | Select-Object -First 1)){throw 'Preserve existing QA evidence/saves; use a fresh OwnerView label.'}
$exe='D:\Program Files\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
if($Rendered){$exe='D:\Program Files\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'}
$project=Join-Path $ProjectRoot 'Rogue10m.uproject'
if(-not(Test-Path -LiteralPath $exe) -or -not(Test-Path -LiteralPath $project)){throw 'Expected project and UE5.8 executable are required.'}
$before=@(Save-Inventory $saves)
New-Item -ItemType Directory -Force -Path $base | Out-Null
$before | Export-Csv -LiteralPath $beforeFile -NoTypeInformation -Encoding utf8
$arguments=@(('"'+$project+'"'),'/Game/Rogue10m/Maps/StartHub','-game','-unattended','-nosplash','-nosound','-NoLiveCoding','-NoRemoteShaderCompile',
 ('-RogueProfileSaveSlot='+$town),('-RogueProgressionSavePrefix='+$prefix),('-RogueSkillKeySaveSlot='+$keys),
 ('-RogueOwnerViewLabel='+$Label),('-RogueOwnerViewProfile='+$Profile),'-ExecCmds="t.MaxFPS 60,Rogue10m.TestOwnerView"',('-abslog="'+$log+'"'))
if($Rendered){$arguments+=@('-d3d11','-windowed','-ForceRes','-ResX=1280','-ResY=720')}else{$arguments+='-nullrhi'}
if($Offscreen){$arguments+='-RenderOffScreen'}
$window=if($VisibleWindow){'Normal'}else{'Hidden'}
$process=$null;$failure=$null;$preservationFailure=$null
try {
 $process=Start-Process -FilePath $exe -ArgumentList $arguments -WindowStyle $window -PassThru
 Write-Output ('Owned OwnerView QA PID='+$process.Id+' profile='+$Profile+' rendered='+$Rendered+' offscreen='+$Offscreen+' log='+$log)
 if(-not $process.WaitForExit($TimeoutSeconds*1000)){throw 'OwnerView QA timed out.'}
 $process.Refresh()
 $result=Join-Path $output 'results.txt';$frames=Join-Path $output 'frames.csv';$captures=Join-Path $output 'captures.csv';$attackFrames=Join-Path $output 'attackframes.csv';$animationFrames=Join-Path $output 'animationframes.csv'
 if(-not(Test-Path -LiteralPath $result) -or -not(Test-Path -LiteralPath $frames) -or -not(Test-Path -LiteralPath $captures) -or -not(Test-Path -LiteralPath $attackFrames) -or -not(Test-Path -LiteralPath $animationFrames)){throw ('QA incomplete; exit='+$process.ExitCode+' log='+$log)}
 $text=Read-BomText $result;Write-Output $text
 $renderedValue=if($Rendered){1}else{0};$offValue=if($Offscreen){1}else{0}
 if($process.ExitCode -ne 0 -or $text -notmatch '(?m)^failures=0\s*$' -or $text -notmatch ('(?m)^profile='+$Profile+'\s*$') -or
  $text -notmatch ('(?m)^rendered='+$renderedValue+'\s*$') -or $text -notmatch ('(?m)^offscreen='+$offValue+'\s*$') -or
  $text -notmatch '(?m)^measured_flushes=0\s*$' -or $text -notmatch '(?m)^measured_viewport_events=0\s*$' -or $text -notmatch '(?m)^owner_view_toggle_invariant_pairs=3\s*$' -or
  -not((Read-BomText $log).Contains('RESULT=OWNER_VIEW_PASSED'))){throw ('OwnerView QA failed; exit='+$process.ExitCode+' log='+$log)}
 $rows=@((Read-BomText $frames)|ConvertFrom-Csv)
 if($rows.Count -lt 1 -or @($rows|Where-Object stage -eq 'downlook_sprint').Count -lt 1 -or @($rows|Where-Object dodging -eq '1').Count -lt 1){throw 'Actual movement frame observations are missing.'}
 $attackRows=@((Read-BomText $attackFrames)|ConvertFrom-Csv)
 foreach($stage in @('attack_0','attack_1','attack_2')){if(@($attackRows|Where-Object {$_.stage -eq $stage -and [double]::Parse($_.montage_weight,[Globalization.CultureInfo]::InvariantCulture) -gt 0}).Count -lt 2){throw ('Actual montage frame observations missing: '+$stage)}}
 $animationRows=@((Read-BomText $animationFrames)|ConvertFrom-Csv)
 foreach($stage in @('attack_0_ready_before_input','attack_0','attack_0_recovered','attack_1_ready_before_input','attack_1','attack_1_recovered')){if(@($animationRows|Where-Object stage -eq $stage).Count -lt 1){throw ('Actual animation trajectory stage missing: '+$stage)}}
 foreach($row in $animationRows){foreach($field in @('hand_l_world_x','hand_l_world_y','hand_l_world_z','hand_r_world_x','hand_r_world_y','hand_r_world_z','hand_l_view_forward','hand_l_view_right','hand_l_view_up','hand_r_view_forward','hand_r_view_right','hand_r_view_up')){$v=[double]::Parse($row.$field,[Globalization.CultureInfo]::InvariantCulture);if([double]::IsNaN($v) -or [double]::IsInfinity($v)){throw ('Non-finite actual hand trajectory: '+$field)}}}
 if($Rendered) {
  if($text -notmatch '(?m)^requested_captures=20\s*$' -or $text -notmatch '(?m)^completed_captures=20\s*$'){throw 'Capture counts are incomplete.'}
  $names=@('01_neutral.png','02_look_down60.png','03_look_down80.png','04_look_down85.png','05_downlook_sprint.png','06_level_E_dodge.png','07_downlook_E_dodge.png','08_downlook_wall_contact.png','09_level_actual_LMB.png','10_downlook_actual_LMB.png','11_deep85_actual_LMB_no_target.png','12_fullbody_inspection.png','13_owned_greatsword.png','14_owner_view_return.png','15_level_ready_before_LMB.png','16_level_first_damage_observed.png','17_level_action_recovered.png','18_downlook_ready_before_LMB.png','19_downlook_first_damage_observed.png','20_downlook_action_recovered.png')
  foreach($name in $names){$p=Join-Path $images $name;if(-not(Test-Path -LiteralPath $p) -or (Get-Item -LiteralPath $p).Length -lt 1024){throw ('Missing actual viewport PNG '+$name)};$bytes=[IO.File]::ReadAllBytes($p);if([BitConverter]::ToString($bytes[0..7]) -ne '89-50-4E-47-0D-0A-1A-0A'){throw ('Invalid PNG '+$name)}}
  $shotRows=@((Read-BomText $captures)|ConvertFrom-Csv)
  if(@($shotRows|Where-Object event -eq 'captured').Count -ne 20){throw 'Capture phase metadata missing.'}
  foreach($name in @('06_level_E_dodge.png','07_downlook_E_dodge.png')){if(@($shotRows|Where-Object {$_.name -eq $name -and $_.event -eq 'captured' -and $_.dodging -eq '1'}).Count -ne 1){throw ('Dodge phase image not captured during actual active dodge: '+$name)}}
  foreach($name in @('15_level_ready_before_LMB.png','18_downlook_ready_before_LMB.png')){if(@($shotRows|Where-Object {$_.name -eq $name -and $_.event -eq 'captured' -and $_.attack_busy -eq '0' -and [double]::Parse($_.target_health,[Globalization.CultureInfo]::InvariantCulture) -eq 10000}).Count -ne 1){throw ('Ready phase not captured before actual input: '+$name)}}
  foreach($name in @('16_level_first_damage_observed.png','19_downlook_first_damage_observed.png')){if(@($shotRows|Where-Object {$_.name -eq $name -and $_.event -eq 'captured' -and [double]::Parse($_.target_health,[Globalization.CultureInfo]::InvariantCulture) -lt 9999.99}).Count -ne 1){throw ('Contact evidence lacks actual damaged target: '+$name)}}
  foreach($name in @('17_level_action_recovered.png','20_downlook_action_recovered.png')){if(@($shotRows|Where-Object {$_.name -eq $name -and $_.event -eq 'captured' -and $_.attack_busy -eq '0' -and $_.combo_active -eq '0' -and [double]::Parse($_.attack_cooldown,[Globalization.CultureInfo]::InvariantCulture) -le .001}).Count -ne 1){throw ('Recovery evidence lacks actual action/cooldown/combo recovery: '+$name)}}
  foreach($name in @('09_level_actual_LMB.png','10_downlook_actual_LMB.png','11_deep85_actual_LMB_no_target.png')){if(@($shotRows|Where-Object {$_.name -eq $name -and $_.event -eq 'captured' -and $_.attack_busy -eq '1' -and $_.montage_active -eq '1' -and $_.owner_proxy_active -eq '1'}).Count -ne 1){throw ('Attack phase image not captured during actual action/montage/owner-proxy: '+$name)}}
 }
 Write-Output 'Programmatic real input only. PNG visual review is required for body/hand near-plane clipping; finite transforms/depth diagnostics do not certify clipping-free motion.'
}catch{$failure=$_}finally{
 if($null -ne $process -and -not $process.HasExited){$process.Kill();$process.WaitForExit(5000)|Out-Null}
 try{
  $after=@(Save-Inventory $saves);$after|Export-Csv -LiteralPath $afterFile -NoTypeInformation -Encoding utf8
  $lookup=@{};foreach($item in $after){$lookup[$item.Path]=$item.SHA256}
  $changed=@($before|Where-Object {-not $lookup.ContainsKey($_.Path) -or $lookup[$_.Path] -ne $_.SHA256});if($changed.Count){throw ('Original save preservation failed: '+$changed.Count)}
  $old=@{};foreach($item in $before){$old[$item.Path]=$true}
  $unexpected=@($after|Where-Object {-not $old.ContainsKey($_.Path) -and [IO.Path]::GetFileName($_.Path) -ne ($town+'.sav') -and [IO.Path]::GetFileName($_.Path) -ne ($keys+'.sav') -and [IO.Path]::GetFileName($_.Path) -notlike ($prefix+'_*.sav')})
  if($unexpected.Count){throw ('Unexpected save creation outside exact OwnerView namespace: '+$unexpected.Count)}
  Write-Output ('Original saves preserved='+$before.Count+'; before/after SHA256 verified; QA slots retained.')
 }catch{$preservationFailure=$_}
}
if($null -ne $preservationFailure){throw $preservationFailure}
if($null -ne $failure){throw $failure}
Write-Output ('RESULT=OWNER_VIEW_PASSED profile='+$Profile+' rendered='+$Rendered+' evidence='+$output+' images='+$images)
