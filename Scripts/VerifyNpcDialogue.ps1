param(
 [Parameter(Mandatory=$true)][ValidatePattern('^[A-Za-z0-9_]{1,24}$')][string]$Label,
 [switch]$Offscreen,[switch]$VisibleWindow,[switch]$Manual,
 [switch]$Motion,[switch]$NullRHI,[switch]$AllowSoftwareRendering,
 [ValidateSet('D3D11','D3D12')][string]$Renderer='D3D11',
 [ValidateRange(60,1000)][int]$TimeoutSeconds=240,
 [string]$ProjectRoot=(Split-Path -Parent $PSScriptRoot)
)
$ErrorActionPreference='Stop'
function Saves {
 @(Get-ChildItem -LiteralPath (Join-Path $ProjectRoot 'Saved\SaveGames') -Filter '*.sav' -Recurse -File -ErrorAction SilentlyContinue | Sort-Object FullName | ForEach-Object {
  [pscustomobject]@{Path=$_.FullName;SHA256=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash}
 })
}
if($Manual -and (-not $VisibleWindow -or $Offscreen)){throw 'Manual mode requires VisibleWindow without Offscreen.'}
if(($Motion -and $Manual) -or ($NullRHI -and (-not $Motion -or $Offscreen -or $VisibleWindow))){
 throw 'Motion and Manual are separate fixtures. NullRHI is available for Motion without window options.'
}
if($AllowSoftwareRendering -and ($NullRHI -or $Renderer -ne 'D3D11')){
 throw 'Software rendering requires D3D11.'
}
$ProjectRoot=(Resolve-Path -LiteralPath $ProjectRoot).Path
$town='Rogue10m_Town_QA_NpcDialog_'+$Label;$prefix='Rogue10m_Progression_QA_NpcDialog_'+$Label;$keys='Rogue10m_Fighter_QA_NpcDialog_'+$Label
$output=Join-Path $ProjectRoot ('DevLog\20261110_NpcDialogue\'+$Label)
$evidence=Join-Path $ProjectRoot 'Feature\doc\evidence\npc-dialogue-20261110';$log=Join-Path $evidence ($Label+'.log')
$beforeCsv=Join-Path $evidence ($Label+'.saves-before.csv');$afterCsv=Join-Path $evidence ($Label+'.saves-after.csv')
$before=@(Saves)
if((Test-Path -LiteralPath $output) -or (Test-Path -LiteralPath $log) -or (Test-Path -LiteralPath $beforeCsv) -or (Test-Path -LiteralPath $afterCsv) -or @($before|Where-Object {([IO.Path]::GetFileName($_.Path) -in @(($town+'.sav'),($keys+'.sav'))) -or ([IO.Path]::GetFileName($_.Path) -like ($prefix+'_*.sav'))}).Count){throw 'Fresh QA label required; preserve all previous evidence and saves.'}
New-Item -ItemType Directory -Force -Path $evidence | Out-Null;$before|Export-Csv -LiteralPath $beforeCsv -NoTypeInformation -Encoding utf8
$exe='D:\Program Files\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$execCmds=if($AllowSoftwareRendering){'t.MaxFPS 60,r.MotionBlurQuality 0,r.ScreenPercentage 50,Rogue10m.TestNpcDialogue'}else{'t.MaxFPS 60,Rogue10m.TestNpcDialogue'}
$args=@(('"'+(Join-Path $ProjectRoot 'Rogue10m.uproject')+'"'),'/Game/Rogue10m/Maps/StartHub','-game','-unattended','-nosplash','-nosound','-DisablePython','-language=en','-locale=en-US','-NoLiveCoding','-NoRemoteShaderCompile',('-RogueProfileSaveSlot='+$town),('-RogueProgressionSavePrefix='+$prefix),('-RogueSkillKeySaveSlot='+$keys),('-RogueNpcDialogueLabel='+$Label),('-ExecCmds="'+$execCmds+'"'),('-abslog="'+$log+'"'),'-windowed','-ForceRes','-ResX=1280','-ResY=720')
if($Offscreen){$args+='-RenderOffScreen'};if($Manual){$args+='-RogueNpcDialogueManual'}
if($Motion){$args+=@('-RogueNpcDialogueMotion','-UseFixedTimeStep','-FPS=60')}
if($NullRHI){$args+='-nullrhi'}else{$args+=('-'+$Renderer.ToLowerInvariant())}
if($AllowSoftwareRendering){$args+='-AllowSoftwareRendering'}
$process=$null;$failure=$null;$preservation=$null
try{
 $window=if($VisibleWindow){'Normal'}else{'Hidden'};$process=Start-Process -FilePath $exe -ArgumentList $args -WindowStyle $window -PassThru
 Write-Output ('Owned NPC dialogue QA PID='+$process.Id+' log='+$log+' output='+$output)
 $timer=[Diagnostics.Stopwatch]::StartNew();while(-not $process.WaitForExit(1000)){if($timer.Elapsed.TotalSeconds -gt $TimeoutSeconds){throw 'Owned NPC dialogue QA timed out.'}}
 $process.Refresh();$result=Join-Path $output 'results.txt'
 if(-not(Test-Path -LiteralPath $result)){throw ('No results produced; exit='+$process.ExitCode+' log='+$log)}
 $r=[IO.File]::ReadAllText($result);Write-Output $r;$engine=[IO.File]::ReadAllText($log)
 $expectedResult=if($Manual){'RESULT=NPC_DIALOGUE_MANUAL_SETUP_COMPLETE'}else{'RESULT=NPC_DIALOGUE_PASSED'}
 $engineErrors=@(Select-String -LiteralPath $log -Pattern '\bError:|Fatal error:|Ensure condition failed')
 if($AllowSoftwareRendering -and $engine -match 'LogRHI:\s+Name: Microsoft Basic Render Driver'){
  $adapterFailure=@($engineErrors|Where-Object {$_.Line -match '\[\s*0\]LogD3D11RHI: Error: Result failed with error 887A0004\s*$'})
  if($adapterFailure.Count -eq 1){
   Write-Warning ('Hardware D3D11 probe failed before Microsoft Basic Render Driver took over. Preserved log line '+$adapterFailure[0].LineNumber+': '+$adapterFailure[0].Line)
   $engineErrors=@($engineErrors|Where-Object {$_.LineNumber -ne $adapterFailure[0].LineNumber})
  }
 }
 if($engineErrors.Count){throw 'Unexpected engine Error/ensure/fatal; inspect full log.'}
 if($process.ExitCode -ne 0 -or -not $engine.Contains($expectedResult) -or $r -notmatch '(?m)^failures=0$'){throw ('Dialogue QA failed; exit='+$process.ExitCode+' log='+$log)}
 if($Motion){
  if($r -notmatch '(?m)^validation_scope=real_Arin_native_dialogue_pose_transitions$' -or
   -not (Test-Path -LiteralPath (Join-Path $output 'motion-poses.csv'))){throw 'The Arin motion fixture did not write its measured poses.'}
  foreach($phase in @('idle_to_talk','talk_to_idle','talk_to_idle_before_reopen','reopen_after_three_poses')){
   if(-not $r.Contains("MOTION $phase ")){throw ('Missing motion phase '+$phase)}
  }
 }
 if(-not $Manual -and -not $NullRHI){
  $screenshots=if($Motion){@('01_arin_close_source.png','02_arin_close_blend.png','03_arin_motion_reopened.png')}else{@('01_proximity_prompt.png','02_selectable_dialogue.png','03_service_result.png','04_trait_branch.png','05_closed_restored.png','06_death_boundary.png')}
  foreach($name in $screenshots){if(-not(Test-Path -LiteralPath (Join-Path $output $name)) -or (Get-Item -LiteralPath (Join-Path $output $name)).Length -lt 1024){throw ('Missing rendered screenshot '+$name)}}
 }
 if(-not $NullRHI -and $engine -notmatch ('(?m)Log'+$Renderer+'RHI:')){throw 'Requested RHI was not observed.'}
}catch{$failure=$_}finally{
 if($null -ne $process -and -not $process.HasExited){$process.Kill();$process.WaitForExit(5000)|Out-Null}
 try{
  $after=@(Saves);$after|Export-Csv -LiteralPath $afterCsv -NoTypeInformation -Encoding utf8;$lookup=@{};foreach($s in $after){$lookup[$s.Path]=$s.SHA256}
  $changed=@($before|Where-Object {-not $lookup.ContainsKey($_.Path)-or $lookup[$_.Path]-ne $_.SHA256});if($changed.Count){throw ('Preexisting save hashes changed count='+$changed.Count)}
  $old=@{};foreach($s in $before){$old[$s.Path]=$true};$unexpected=@($after|Where-Object {-not $old.ContainsKey($_.Path)-and [IO.Path]::GetFileName($_.Path)-notin @(($town+'.sav'),($keys+'.sav'))-and [IO.Path]::GetFileName($_.Path)-notlike ($prefix+'_*.sav')});if($unexpected.Count){throw ('Unexpected save paths count='+$unexpected.Count)}
  Write-Output ('Preexisting save SHA256 preserved='+$before.Count+' new_slots=exact_NpcDialog_namespace')
 }catch{$preservation=$_}
}
if($preservation){throw $preservation};if($failure){throw $failure};Write-Output ($expectedResult+' output='+$output)
