param(
 [Parameter(Mandatory=$true)][ValidatePattern('^[A-Za-z0-9_]{1,24}$')][string]$Label,
 [ValidateSet('Original','Weighted')][string]$Profile='Weighted',
 [ValidateRange(0,48)][float]$EyeForwardCm=0,
 [switch]$Rendered,[switch]$Offscreen,[switch]$VisibleWindow,
 [ValidateRange(60,260)][int]$TimeoutSeconds=250,
 [string]$ProjectRoot=(Split-Path -Parent $PSScriptRoot)
)
$ErrorActionPreference='Stop'
function Read-BomText([string]$Path) {$reader=[IO.StreamReader]::new($Path,[Text.Encoding]::UTF8,$true);try{return $reader.ReadToEnd()}finally{$reader.Dispose()}}
function Save-Inventory([string]$Directory) {return @(Get-ChildItem -LiteralPath $Directory -Filter '*.sav' -File -Recurse | Sort-Object FullName | ForEach-Object {[pscustomobject]@{Path=$_.FullName;SHA256=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash}})}
if($Offscreen -and -not $Rendered){throw 'Offscreen requires Rendered.'}
if($VisibleWindow -and (-not $Rendered -or $Offscreen)){throw 'VisibleWindow requires native Rendered mode.'}
$ProjectRoot=(Resolve-Path -LiteralPath $ProjectRoot).Path
$town='Rogue10m_Town_QA_LookDash_'+$Label
$prefix='Rogue10m_Progression_QA_LookDash_'+$Label
$keys='Rogue10m_Fighter_QA_LookDash_'+$Label
$saves=Join-Path $ProjectRoot 'Saved\SaveGames'
$base=Join-Path $ProjectRoot 'Feature\doc\evidence\look-dash-20261029'
$output=Join-Path $base $Label
$images=Join-Path $ProjectRoot ('DevLog\20261029_LookDash_'+$Label)
$log=Join-Path $base ($Label+'.log')
$beforeFile=Join-Path $base ($Label+'.saves-before.csv')
$afterFile=Join-Path $base ($Label+'.saves-after.csv')
if((Test-Path -LiteralPath $output) -or (Test-Path -LiteralPath $images) -or (Test-Path -LiteralPath $log) -or
 (Test-Path -LiteralPath $beforeFile) -or (Test-Path -LiteralPath $afterFile) -or
 (Test-Path -LiteralPath (Join-Path $saves ($town+'.sav'))) -or (Test-Path -LiteralPath (Join-Path $saves ($keys+'.sav'))) -or
 (Get-ChildItem -LiteralPath $saves -File -Filter ($prefix+'_*') -ErrorAction SilentlyContinue | Select-Object -First 1)){throw 'Preserve existing QA evidence/saves; use a fresh LookDash label.'}
$exe='D:\Program Files\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
if($Rendered){$exe='D:\Program Files\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'}
$project=Join-Path $ProjectRoot 'Rogue10m.uproject'
if(-not(Test-Path -LiteralPath $exe) -or -not(Test-Path -LiteralPath $project)){throw 'Expected project and UE5.8 executable are required.'}
$before=@(Save-Inventory $saves)
New-Item -ItemType Directory -Force -Path $base | Out-Null
$before | Export-Csv -LiteralPath $beforeFile -NoTypeInformation -Encoding utf8
$arguments=@(('"'+$project+'"'),'/Game/Rogue10m/Maps/StartHub','-game','-unattended','-nosplash','-nosound','-NoLiveCoding','-NoRemoteShaderCompile',
 ('-RogueProfileSaveSlot='+$town),('-RogueProgressionSavePrefix='+$prefix),('-RogueSkillKeySaveSlot='+$keys),
 ('-RogueLookDashLabel='+$Label),('-RogueLookDashProfile='+$Profile),'-ExecCmds="t.MaxFPS 60,Rogue10m.TestLookDash"',('-abslog="'+$log+'"'))
if($EyeForwardCm -gt 0){$arguments+=('-RogueLookDashEyeForward='+$EyeForwardCm.ToString([Globalization.CultureInfo]::InvariantCulture))}
if($Rendered){$arguments+=@('-d3d11','-windowed','-ForceRes','-ResX=1280','-ResY=720')}else{$arguments+='-nullrhi'}
if($Offscreen){$arguments+='-RenderOffScreen'}
$window=if($VisibleWindow){'Normal'}else{'Hidden'}
$process=$null;$failure=$null;$preservationFailure=$null
try {
 $process=Start-Process -FilePath $exe -ArgumentList $arguments -WindowStyle $window -PassThru
 Write-Output ('Owned LookDash QA PID='+$process.Id+' profile='+$Profile+' rendered='+$Rendered+' offscreen='+$Offscreen+' log='+$log)
 if(-not $process.WaitForExit($TimeoutSeconds*1000)){throw 'LookDash QA timed out.'}
 $process.Refresh()
 $result=Join-Path $output 'results.txt';$frames=Join-Path $output 'frames.csv';$captures=Join-Path $output 'captures.csv'
 if(-not(Test-Path -LiteralPath $result) -or -not(Test-Path -LiteralPath $frames) -or -not(Test-Path -LiteralPath $captures)){throw ('QA incomplete; exit='+$process.ExitCode+' log='+$log)}
 $text=Read-BomText $result;Write-Output $text
 $renderedValue=if($Rendered){1}else{0};$offValue=if($Offscreen){1}else{0}
 if($process.ExitCode -ne 0 -or $text -notmatch '(?m)^failures=0\s*$' -or $text -notmatch ('(?m)^profile='+$Profile+'\s*$') -or
  $text -notmatch ('(?m)^rendered='+$renderedValue+'\s*$') -or $text -notmatch ('(?m)^offscreen='+$offValue+'\s*$') -or
  $text -notmatch '(?m)^measured_flushes=0\s*$' -or $text -notmatch '(?m)^measured_viewport_events=0\s*$' -or
  -not((Read-BomText $log).Contains('RESULT=LOOK_DASH_PASSED'))){throw ('LookDash QA failed; exit='+$process.ExitCode+' log='+$log)}
 $rows=@((Read-BomText $frames)|ConvertFrom-Csv)
 if($rows.Count -lt 1 -or @($rows|Where-Object stage -eq 'downlook_sprint').Count -lt 1 -or @($rows|Where-Object dodging -eq '1').Count -lt 1){throw 'Actual movement frame observations are missing.'}
 if($Rendered) {
  if($text -notmatch '(?m)^requested_captures=8\s*$' -or $text -notmatch '(?m)^completed_captures=8\s*$'){throw 'Capture counts are incomplete.'}
  $names=@('01_neutral.png','02_look_down60.png','03_look_down80.png','04_look_down85.png','05_downlook_sprint.png','06_level_E_dodge.png','07_downlook_E_dodge.png','08_downlook_wall_contact.png')
  foreach($name in $names){$p=Join-Path $images $name;if(-not(Test-Path -LiteralPath $p) -or (Get-Item -LiteralPath $p).Length -lt 1024){throw ('Missing actual viewport PNG '+$name)};$bytes=[IO.File]::ReadAllBytes($p);if([BitConverter]::ToString($bytes[0..7]) -ne '89-50-4E-47-0D-0A-1A-0A'){throw ('Invalid PNG '+$name)}}
  $shotRows=@((Read-BomText $captures)|ConvertFrom-Csv)
  if(@($shotRows|Where-Object event -eq 'captured').Count -ne 8){throw 'Capture phase metadata missing.'}
  foreach($name in @('06_level_E_dodge.png','07_downlook_E_dodge.png')){if(@($shotRows|Where-Object {$_.name -eq $name -and $_.event -eq 'captured' -and $_.dodging -eq '1'}).Count -ne 1){throw ('Dodge phase image not captured during actual active dodge: '+$name)}}
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
  if($unexpected.Count){throw ('Unexpected save creation outside exact LookDash namespace: '+$unexpected.Count)}
  Write-Output ('Original saves preserved='+$before.Count+'; before/after SHA256 verified; QA slots retained.')
 }catch{$preservationFailure=$_}
}
if($null -ne $preservationFailure){throw $preservationFailure}
if($null -ne $failure){throw $failure}
Write-Output ('RESULT=LOOK_DASH_PASSED profile='+$Profile+' rendered='+$Rendered+' evidence='+$output+' images='+$images)
