param([Parameter(Mandatory)][ValidatePattern('^[A-Za-z0-9_]{1,24}$')][string]$Label,[switch]$NullRHI,[switch]$MouseInput,[int]$TimeoutSeconds=210,[switch]$CombatMotion,[switch]$CombatMotionRender,[switch]$DoubleJumpMotion,[switch]$DoubleJumpMotionRender,[switch]$ExpectSecondJumpPose,[switch]$PlayerHitMotion,[switch]$PlayerHitMotionRender,[switch]$ForwardLanding,[switch]$ExpectStockLanding,[ValidateSet('HumanMale','HumanFemale','DwarfMale','DwarfFemale','OrcMale','OrcFemale')][string]$Appearance)
$ErrorActionPreference='Stop'
if($CombatMotion -and (-not $NullRHI -or $MouseInput)){throw 'CombatMotion requires NullRHI without MouseInput.'}
if($CombatMotionRender -and ($NullRHI -or $MouseInput -or $CombatMotion)){throw 'CombatMotionRender is a separate software-rendered run.'}
if($DoubleJumpMotion -and (-not $NullRHI -or $MouseInput -or $CombatMotion -or $CombatMotionRender)){throw 'DoubleJumpMotion requires a separate NullRHI run.'}
if($DoubleJumpMotionRender -and ($NullRHI -or $MouseInput -or $CombatMotion -or $CombatMotionRender -or $DoubleJumpMotion)){throw 'DoubleJumpMotionRender requires a separate software-rendered run.'}
$jumpQA=$DoubleJumpMotion -or $DoubleJumpMotionRender
$softwareCapture=$CombatMotionRender -or $DoubleJumpMotionRender -or $PlayerHitMotionRender -or ($ForwardLanding -and -not $NullRHI)
if($ForwardLanding -and ($jumpQA -or $CombatMotion -or $CombatMotionRender -or $PlayerHitMotion -or $PlayerHitMotionRender -or $MouseInput)){throw 'ForwardLanding requires a separate run.'}
if($ExpectStockLanding -and -not $ForwardLanding){throw 'ExpectStockLanding requires a landing audit.'}
if($Appearance -and -not $ForwardLanding){throw 'Appearance requires a landing audit.'}
if($ExpectSecondJumpPose -and -not $jumpQA){throw 'ExpectSecondJumpPose requires a jump audit run.'}
if($PlayerHitMotionRender -and $NullRHI){throw 'PlayerHitMotionRender requires D3D11.'}
if($PlayerHitMotionRender){$PlayerHitMotion=$true}
if($PlayerHitMotion -and ((-not $NullRHI -and -not $PlayerHitMotionRender) -or $MouseInput -or $CombatMotion -or $CombatMotionRender -or $jumpQA)){throw 'PlayerHitMotion must run independently of the other fixtures.'}
$root=(Resolve-Path -LiteralPath (Split-Path -Parent $PSScriptRoot)).Path
$project=Join-Path $root 'Rogue10m.uproject'
$out=Join-Path $root ('Feature\doc\evidence\third-person-20261110\'+$Label)
if(Test-Path -LiteralPath $out){throw 'Fresh evidence directory required'}
if(@(Get-CimInstance Win32_Process | Where-Object {$_.Name -like 'UnrealEditor*' -and $_.CommandLine -and $_.CommandLine.Contains($project)}).Count){throw 'Shared project UE process already running'}
New-Item -ItemType Directory -Path $out | Out-Null
function Saves {
 $saveRoot=Join-Path $root 'Saved\SaveGames'
 if(Test-Path -LiteralPath $saveRoot -PathType Container){
  Get-ChildItem -LiteralPath $saveRoot -Filter '*.sav' -File | ForEach-Object {[pscustomobject]@{path=$_.FullName;sha256=(Get-FileHash -LiteralPath $_.FullName).Hash}}
 }
}
$before=@(Saves);$before|ConvertTo-Json -Depth 4|Set-Content -LiteralPath (Join-Path $out 'saves-before.json')
$log=Join-Path $out 'engine.log'
$exe='D:\Program Files\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$motionRenderCommands=if($PlayerHitMotionRender){'r.MotionBlurQuality 0,r.ScreenPercentage 50,'}elseif($softwareCapture){'r.MotionBlurQuality 0,'}else{''}
$argsList=@(('"'+$project+'"'),'/Game/Rogue10m/Maps/StartHub','-game','-DisablePython','-unattended','-nosplash','-nosound','-NoLiveCoding','-NoRemoteShaderCompile','-language=en','-locale=en-US','-windowed','-ForceRes','-ResX=1280','-ResY=720',('-abslog="'+$log+'"'),('-RogueProfileSaveSlot=Rogue10m_Town_QA_ThirdPerson_'+$Label),('-RogueProgressionSavePrefix=Rogue10m_Progression_QA_ThirdPerson_'+$Label),('-RogueSkillKeySaveSlot=Rogue10m_Fighter_QA_ThirdPerson_'+$Label),('-RogueThirdPersonQALabel='+$Label),('-ExecCmds="t.MaxFPS 60,'+$motionRenderCommands+'Rogue10m.TestThirdPersonGameplay"'))
if($MouseInput){$argsList+='-RogueThirdPersonMouseQA'}
if($CombatMotion -or $CombatMotionRender){$argsList+=@('-RogueThirdPersonCombatMotionQA','-UseFixedTimeStep','-FPS=60')}
if($CombatMotionRender){$argsList+='-RogueThirdPersonCombatMotionCapture'}
if($jumpQA){$argsList+=@('-RogueThirdPersonDoubleJumpQA','-UseFixedTimeStep','-FPS=60')}
if($DoubleJumpMotionRender){$argsList+='-RogueThirdPersonDoubleJumpCapture'}
if($PlayerHitMotion){$argsList+=@('-RoguePlayerHitMotionQA','-UseFixedTimeStep','-FPS=60')}
if($PlayerHitMotionRender){$argsList+='-RoguePlayerHitMotionCapture'}
if($ForwardLanding){
 $argsList+=@('-RogueThirdPersonForwardLandingQA','-UseFixedTimeStep','-FPS=60')
 if(-not $ExpectStockLanding){$argsList+='-RogueExpectForwardLanding'}
 if($Appearance){$argsList+=('-RogueMotionQAAppearance='+$Appearance)}
}
if($softwareCapture){$argsList+=@('-AllowSoftwareRendering','-graphicsadapter=1')}
if($ExpectSecondJumpPose){$argsList+='-RogueExpectSecondJumpPose'}
if($NullRHI){$argsList+='-nullrhi'}else{$argsList+=@('-d3d11','-RenderOffScreen')}
$p=$null;$failure=$null;$started=$null
try {
 $p=Start-Process -FilePath $exe -ArgumentList $argsList -WindowStyle Hidden -PassThru
 $started=$p.StartTime;Write-Output ('Owned third-person QA PID='+$p.Id+' log='+$log)
 if(-not $p.WaitForExit($TimeoutSeconds*1000)){throw 'Owned third-person UE timeout'}
 $p.Refresh();$result=Get-Content -LiteralPath (Join-Path $root ('DevLog\20261110_ThirdPerson\'+$Label+'\results.txt')) -Raw;Write-Output $result
 if($p.ExitCode -ne 0 -or $result -notmatch '(?m)^failures=0\s*$' -or -not(Select-String -LiteralPath $log -Pattern 'RESULT=THIRD_PERSON_GAMEPLAY_PASSED' -Quiet)){throw ('Third-person result failed; UE exit='+$p.ExitCode)}
 if($MouseInput -and ($result -notmatch '(?m)^mouse_cases=13\s*$' -or $result -notmatch '(?m)^mouse_passed=13\s*$')){throw '13 mouse input cases incomplete'}
 if($CombatMotion){
  $motionCsv=Join-Path $root ('DevLog\20261110_ThirdPerson\'+$Label+'\motion-frames.csv')
  if(-not(Test-Path -LiteralPath $motionCsv)){throw 'Cannot find combat motion frame observations.'}
  foreach($case in @('sword_guard_to_walk','sword_guard_300_long_control','sword_guard_300','unarmed_punch_still','unarmed_punch_to_walk')){if(-not $result.Contains('motion_case='+$case+' ')){throw ('Combat motion case incomplete: '+$case)}}
 }
 if($PlayerHitMotion){
  $hitCsv=Join-Path $root ('DevLog\20261110_ThirdPerson\'+$Label+'\hit-motion-poses.csv')
  $hitCases=if($PlayerHitMotionRender){@('idle_control','front_idle')}
  else{@('idle_control','front_idle','rear_idle','attack_control','front_interrupted_stock','front_interrupted_attack','blocked_front','rapid_front_rear')}
  if($result -notmatch "(?m)^hit_motion_completed=$($hitCases.Count)\s*$" -or -not(Test-Path -LiteralPath $hitCsv)){
   throw 'Missing player hit-motion observations.'
  }
  $poses=Import-Csv -LiteralPath $hitCsv
  foreach($case in $hitCases){
   foreach($stage in @('manny','displayed')){
    $caseFrames=@($poses|Where-Object {$_.case -eq $case -and $_.mesh -eq $stage}|ForEach-Object {[int]$_.frame_from_event}|Sort-Object -Unique)
    if(($caseFrames -join ',') -ne '0,1,2,3,4,5,6'){throw ('Missing first-six player poses for '+$case+' '+$stage)}
   }
  }
 }
 if($jumpQA){
  $jumpFolder=Join-Path $root ('DevLog\20261110_ThirdPerson\'+$Label)
  foreach($file in @('jump-state-frames.csv','jump-pose-frames.csv')){
   if(-not(Test-Path -LiteralPath (Join-Path $jumpFolder $file) -PathType Leaf)){throw ('Missing jump audit evidence '+$file)}
  }
  foreach($case in @('single_space','double_space')){
   if(-not $result.Contains('jump_case='+$case+' ')){throw ('Jump audit case incomplete: '+$case)}
  }
 }
 if($ForwardLanding){
  if($Appearance -and $result -notmatch ('(?m)^appearance='+[regex]::Escape($Appearance)+' body=')){throw 'Landing audit did not enter the expected appearance.'}
  $landingCsv=Join-Path $root ('DevLog\20261110_ThirdPerson\'+$Label+'\forward-landing-frames.csv')
  if(-not(Test-Path -LiteralPath $landingCsv -PathType Leaf)){throw 'Missing forward-landing frame observations.'}
  $cases=if($NullRHI){@('forward_620','forward_610','stationary','sideways')}else{@('forward_620')}
  foreach($case in $cases){if(-not $result.Contains('landing_case='+$case+' ')){throw ('Landing audit case incomplete: '+$case)}}
 }
 if(-not $NullRHI){
  $expectedImages=if($ForwardLanding){@('01_forward_landing_left_support.png','02_forward_landing_jog_handoff.png')}elseif($PlayerHitMotionRender){@('01_idle_control.png','02_front_recoil.png')}elseif($DoubleJumpMotionRender){@('01_single_space_at_matched_frame.png','02_second_space_at_pose_peak.png')}elseif($CombatMotionRender){@('01_sword_guard_before_W.png','02_sword_guard_fading.png')}else{@('01_default_third_person.png','02_wall_camera_contraction.png','03_third_person_movement.png','04_active_third_person_dodge.png','05_wall_dodge_result.png','06_look_up_head.png','07_look_down_head.png')}
  foreach($name in $expectedImages){$file=Join-Path $root ('DevLog\20261110_ThirdPerson\'+$Label+'\'+$name);if(-not(Test-Path -LiteralPath $file) -or (Get-Item -LiteralPath $file).Length -lt 1024){throw ('Missing screenshot '+$name)}}
 }
 $errors=@(Select-String -LiteralPath $log -Pattern '\bError:|Fatal error:|Ensure condition failed')
 if($softwareCapture){
  $selected=Select-String -LiteralPath $log -Pattern 'LogRHI:\s+Name:\s+Microsoft Basic Render Driver' | Select-Object -First 1
  if(-not $selected){throw 'This capture mode must use the software adapter.'}
  $startupProbe=@($errors | Where-Object {$_.LineNumber -lt $selected.LineNumber -and $_.Line -match 'LogD3D11RHI: Error: Result failed with error 887A0004\s*$'})
  if($startupProbe.Count){$startupProbe | ForEach-Object {Write-Output ('Hardware device probe recorded at log line '+$_.LineNumber)}}
  $errors=@($errors | Where-Object {$_.LineNumber -notin $startupProbe.LineNumber})
 }
 if($errors.Count){throw ('Unexpected red output errors='+$errors.Count)}
} catch {$failure=$_} finally {
 if($p -and -not $p.HasExited){$current=Get-Process -Id $p.Id -ErrorAction SilentlyContinue;if($current -and $current.StartTime -eq $started -and $current.Path -eq $exe){Stop-Process -InputObject $current -Force;$null=$p.WaitForExit(10000)}}
 $after=@(Saves);$after|ConvertTo-Json -Depth 4|Set-Content -LiteralPath (Join-Path $out 'saves-after.json');$lookup=@{};foreach($s in $after){$lookup[$s.path]=$s.sha256};$changed=@($before|Where-Object {-not $lookup.ContainsKey($_.path) -or $lookup[$_.path] -ne $_.sha256});$preserved=$changed.Count -eq 0
 [pscustomobject]@{label=$Label;before=$before.Count;after=$after.Count;preserved=$preserved;exit=if($p -and $p.HasExited){$p.ExitCode}else{$null};failure=if($failure){$failure.ToString()}else{$null};first_person_qa=$false;physical_input=$false;full_run=$false;language_override='process only en/en-US'}|ConvertTo-Json|Set-Content -LiteralPath (Join-Path $out 'launcher.json')
}
if(-not $preserved){throw 'Save preservation failure'};if($failure){throw $failure}
if($ForwardLanding){Write-Output ('PASSED third-person running landing phase audit; existing saves preserved='+$before.Count)}
elseif($jumpQA){Write-Output ('PASSED third-person Space input and jump pose audit; existing saves preserved='+$before.Count)}
elseif($PlayerHitMotion){Write-Output ('PASSED player hit-pose observations; existing saves preserved='+$before.Count)}
elseif($CombatMotion -or $CombatMotionRender){Write-Output ('PASSED third-person combat motion observations; existing saves preserved='+$before.Count)}else{
 Write-Output ('PASSED third-person camera/movement/dodge QA; existing saves preserved='+$before.Count+'; review actual PNGs separately')
}
