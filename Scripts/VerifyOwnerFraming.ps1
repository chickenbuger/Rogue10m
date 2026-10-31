param(
 [Parameter(Mandatory=$true)][ValidatePattern('^[A-Za-z0-9_]{1,24}$')][string]$Label,
 [ValidateSet('Baseline','Candidate')][string]$Mode='Baseline',
 [switch]$Rendered,[switch]$Offscreen,
 [ValidateRange(60,360)][int]$TimeoutSeconds=240,
 [string]$ProjectRoot=(Split-Path -Parent $PSScriptRoot)
)
$ErrorActionPreference='Stop'
function Read-BomText([string]$Path){$r=[IO.StreamReader]::new($Path,[Text.Encoding]::UTF8,$true);try{return $r.ReadToEnd()}finally{$r.Dispose()}}
function Save-Inventory([string]$Dir){return @(Get-ChildItem -LiteralPath $Dir -Filter '*.sav' -File -Recurse | Sort-Object FullName | ForEach-Object {[pscustomobject]@{Path=$_.FullName;SHA256=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash}})}
function Number([string]$Text){return [double]::Parse($Text,[Globalization.CultureInfo]::InvariantCulture)}
if($Offscreen -and -not $Rendered){throw 'Offscreen requires Rendered.'}
$ProjectRoot=(Resolve-Path -LiteralPath $ProjectRoot).Path
$town='Rogue10m_Town_QA_OwnerFraming_'+$Label
$prefix='Rogue10m_Progression_QA_OwnerFraming_'+$Label
$keys='Rogue10m_Fighter_QA_OwnerFraming_'+$Label
$saves=Join-Path $ProjectRoot 'Saved\SaveGames'
$base=Join-Path $ProjectRoot 'Feature\doc\evidence\owner-framing-20261031'
$output=Join-Path $base $Label
$images=Join-Path $ProjectRoot ('DevLog\20261031_OwnerFraming_'+$Label)
$log=Join-Path $base ($Label+'.log')
$beforeFile=Join-Path $base ($Label+'.saves-before.csv')
$afterFile=Join-Path $base ($Label+'.saves-after.csv')
if((Test-Path -LiteralPath $output) -or (Test-Path -LiteralPath $images) -or (Test-Path -LiteralPath $log) -or
 (Test-Path -LiteralPath $beforeFile) -or (Test-Path -LiteralPath $afterFile) -or
 (Test-Path -LiteralPath (Join-Path $saves ($town+'.sav'))) -or (Test-Path -LiteralPath (Join-Path $saves ($keys+'.sav'))) -or
 (Get-ChildItem -LiteralPath $saves -File -Filter ($prefix+'_*') -ErrorAction SilentlyContinue|Select-Object -First 1)){throw 'Preserve previous saves/evidence; choose a fresh OwnerFraming label.'}
$exe='D:\Program Files\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
if($Rendered){$exe='D:\Program Files\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'}
$project=Join-Path $ProjectRoot 'Rogue10m.uproject'
if(-not(Test-Path -LiteralPath $exe) -or -not(Test-Path -LiteralPath $project)){throw 'Expected UE5.8 executable and project are required.'}
$before=@(Save-Inventory $saves);New-Item -ItemType Directory -Force -Path $base|Out-Null
$before|Export-Csv -LiteralPath $beforeFile -NoTypeInformation -Encoding utf8
$arguments=@(('"'+$project+'"'),'/Game/Rogue10m/Maps/StartHub','-game','-unattended','-nosplash','-nosound','-NoLiveCoding','-NoRemoteShaderCompile',
 ('-RogueProfileSaveSlot='+$town),('-RogueProgressionSavePrefix='+$prefix),('-RogueSkillKeySaveSlot='+$keys),
 ('-RogueOwnerFramingLabel='+$Label),('-RogueOwnerFramingMode='+$Mode),'-ExecCmds="t.MaxFPS 60,Rogue10m.TestOwnerFraming"',('-abslog="'+$log+'"'))
if($Rendered){$arguments+=@('-d3d11','-windowed','-ForceRes','-ResX=1280','-ResY=720')}else{$arguments+='-nullrhi'}
if($Offscreen){$arguments+='-RenderOffScreen'}
$process=$null;$failure=$null;$preservationFailure=$null
try{
 $process=Start-Process -FilePath $exe -ArgumentList $arguments -WindowStyle Hidden -PassThru
 Write-Output ('Owned OwnerFraming QA PID='+$process.Id+' mode='+$Mode+' rendered='+$Rendered+' log='+$log)
 if(-not $process.WaitForExit($TimeoutSeconds*1000)){throw 'OwnerFraming QA timed out.'}
 $process.Refresh();$result=Join-Path $output 'results.txt';$vertices=Join-Path $output 'vertices.csv';$geometry=Join-Path $output 'geometry.csv';$captures=Join-Path $output 'captures.csv'
 foreach($p in @($result,$vertices,$geometry,$captures)){if(-not(Test-Path -LiteralPath $p)){throw ('Incomplete QA; exit='+$process.ExitCode+' missing='+$p+' log='+$log)}}
 $text=Read-BomText $result;Write-Output $text
 if($process.ExitCode -ne 0 -or $text -notmatch '(?m)^failures=0\s*$' -or $text -notmatch ('(?m)^mode='+$Mode+'\s*$') -or
  -not((Read-BomText $log).Contains('RESULT=OWNER_FRAMING_PASSED'))){throw ('OwnerFraming diagnostic failed; exit='+$process.ExitCode+' log='+$log)}
 $geo=@((Read-BomText $geometry)|ConvertFrom-Csv)
 if($geo.Count -ne 15){throw 'All fifteen actual geometry captures are required.'}
 foreach($row in $geo){if((Number $row.vertices) -lt 1 -or (Number $row.triangles) -lt 1 -or (Number $row.near_cm) -le 0){throw 'Invalid actual geometry counts/near plane.'};foreach($field in @('min_depth','max_depth','hand_l_depth','hand_r_depth')){$v=Number $row.$field;if([double]::IsNaN($v) -or [double]::IsInfinity($v)){throw ('Non-finite actual projection '+$field)}}}
 if(@((Read-BomText $vertices)|ConvertFrom-Csv).Count -lt 1500){throw 'Actual skinned-vertex evidence is missing.'}
 if($Rendered){
  if($text -notmatch '(?m)^requested_captures=15\s*$' -or $text -notmatch '(?m)^completed_captures=15\s*$'){throw 'All fifteen actual viewport captures are required.'}
  $shots=@((Read-BomText $captures)|ConvertFrom-Csv);$taken=@($shots|Where-Object event -eq 'captured')
  if($taken.Count -ne 15){throw 'Actual capture metadata missing.'}
  for($scene=0;$scene -lt 5;$scene++){for($modeIndex=0;$modeIndex -lt 3;$modeIndex++){
   $row=@($taken|Where-Object {$_.scene -eq [string]$scene -and $_.visibility_mode -eq [string]$modeIndex})
   if($row.Count -ne 1 -or $row[0].body_owner_hidden -ne '1' -or $row[0].proxy_view_hidden -ne $(if($modeIndex -eq 1){'1'}else{'0'})){throw ('Diagnostic visibility metadata mismatch scene='+$scene+' mode='+$modeIndex)}
   if($modeIndex -eq 2 -and $row[0].body_cast_shadow -ne '0'){throw 'World body shadow was not disabled in shadow isolation mode.'}
   if($Mode -eq 'Candidate' -and $row[0].framing_active -ne '1'){throw 'Candidate capture used a leader fallback instead of active presentation framing.'}
   if($scene -eq 3 -and $row[0].dodging -ne '1'){throw 'Active E dodge photo was not captured during an actual active dodge.'}
   $p=Join-Path $images $row[0].name;if(-not(Test-Path -LiteralPath $p) -or (Get-Item -LiteralPath $p).Length -lt 1024){throw ('Missing viewport PNG '+$p)}
   $bytes=[IO.File]::ReadAllBytes($p);if([BitConverter]::ToString($bytes[0..7]) -ne '89-50-4E-47-0D-0A-1A-0A'){throw ('Invalid PNG '+$p)}
  }}
 }
 Write-Output 'Numeric diagnostic completion is not visual acceptance. Real PNG review must separate mesh pixels from world shadows; poses/timestamps are recorded without animation freeze. No physical input/full-run claim.'
}catch{$failure=$_}finally{
 if($null -ne $process -and -not $process.HasExited){$process.Kill();$process.WaitForExit(5000)|Out-Null}
 try{
  $after=@(Save-Inventory $saves);$after|Export-Csv -LiteralPath $afterFile -NoTypeInformation -Encoding utf8
  $lookup=@{};foreach($item in $after){$lookup[$item.Path]=$item.SHA256}
  $changed=@($before|Where-Object {-not $lookup.ContainsKey($_.Path) -or $lookup[$_.Path] -ne $_.SHA256});if($changed.Count){throw ('Preexisting save preservation failed: '+$changed.Count)}
  $old=@{};foreach($item in $before){$old[$item.Path]=$true}
  $unexpected=@($after|Where-Object {-not $old.ContainsKey($_.Path) -and [IO.Path]::GetFileName($_.Path) -ne ($town+'.sav') -and [IO.Path]::GetFileName($_.Path) -ne ($keys+'.sav') -and [IO.Path]::GetFileName($_.Path) -notlike ($prefix+'_*.sav')})
  if($unexpected.Count){throw ('Unexpected save creation outside exact OwnerFraming namespace: '+$unexpected.Count)}
  Write-Output ('Preexisting saves preserved='+$before.Count+'; exact QA slots retained; before/after SHA256 checked.')
 }catch{$preservationFailure=$_}
}
if($null -ne $preservationFailure){throw $preservationFailure};if($null -ne $failure){throw $failure}
Write-Output ('RESULT=OWNER_FRAMING_PASSED mode='+$Mode+' rendered='+$Rendered+' output='+$output+' images='+$images)
