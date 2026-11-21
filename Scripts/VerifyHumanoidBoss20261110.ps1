param([Parameter(Mandatory)][ValidatePattern('^[A-Za-z0-9_]{1,24}$')][string]$Label,[switch]$NullRHI,[switch]$WalkContact,[int]$TimeoutSeconds=300)
$ErrorActionPreference='Stop'
$root=(Resolve-Path -LiteralPath (Split-Path -Parent $PSScriptRoot)).Path
$project=Join-Path $root 'Rogue10m.uproject'
$out=Join-Path $root ('Feature\doc\evidence\humanoid-boss-npc-20261110\'+$Label)
if(Test-Path -LiteralPath $out){throw 'Fresh evidence directory required'}
if(@(Get-CimInstance Win32_Process | Where-Object {$_.Name -like 'UnrealEditor*' -and $_.CommandLine -and $_.CommandLine.Contains($project)}).Count){throw 'Shared project UE process already running'}
New-Item -ItemType Directory -Path $out | Out-Null
function Saves {
 $directory=Join-Path $root 'Saved\SaveGames'
 if(Test-Path -LiteralPath $directory -PathType Container){
  Get-ChildItem -LiteralPath $directory -Filter '*.sav' -File | ForEach-Object {[pscustomobject]@{path=$_.FullName;sha256=(Get-FileHash -LiteralPath $_.FullName).Hash}}
 }
}
$before=@(Saves); $before|ConvertTo-Json -Depth 4|Set-Content -LiteralPath (Join-Path $out 'saves-before.json')
$binarySha=(Get-FileHash -LiteralPath (Join-Path $root 'Binaries\Win64\UnrealEditor-Rogue10m.dll')).Hash
$log=Join-Path $out 'engine.log'
$exe='D:\Program Files\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$argsList=@(('"'+$project+'"'),'/Game/Rogue10m/Maps/StartHub','-game','-DisablePython','-unattended','-nosplash','-nosound','-NoLiveCoding','-NoRemoteShaderCompile','-language=en','-locale=en-US','-RogueBossThirdPersonQA','-RenderOffScreen','-windowed','-ForceRes','-ResX=1280','-ResY=720',('-abslog="'+$log+'"'),('-RogueProfileSaveSlot=Rogue10m_Town_QA_BossReadability_'+$Label),('-RogueProgressionSavePrefix=Rogue10m_Progression_QA_BossReadability_'+$Label),('-RogueSkillKeySaveSlot=Rogue10m_Fighter_QA_BossReadability_'+$Label),('-RogueBossReadabilityLabel='+$Label),'-ExecCmds="t.MaxFPS 60,Rogue10m.TestBossReadability"')
if($NullRHI){$argsList+='-nullrhi'} else {$argsList+='-d3d11'}
if($WalkContact){$argsList+=@('-RogueBossWalkContactQA','-UseFixedTimeStep','-FPS=60')}
$p=$null;$failure=$null
try {
 $p=Start-Process -FilePath $exe -ArgumentList $argsList -WindowStyle Hidden -PassThru
 $started=$p.StartTime; Write-Output ('Owned boss QA PID='+$p.Id+' log='+$log)
 $timer=[Diagnostics.Stopwatch]::StartNew()
 while(-not $p.WaitForExit(1000)){if($timer.Elapsed.TotalSeconds -gt $TimeoutSeconds){throw 'Owned UE timeout'}}
 $p.Refresh();if($p.ExitCode -ne 0){throw ('UE exit='+$p.ExitCode)}
 $result=Get-Content -LiteralPath (Join-Path $root ('Feature\doc\evidence\boss-readability-20261103\'+$Label+'\results.txt')) -Raw
 $expected = if($WalkContact){'(?m)^PASS paced_walk_reduces_measured_ground_contact_drift\s*$'}else{'(?m)^cases_completed=18\s*$'}
 if($result -notmatch '(?m)^failures=0\s*$' -or $result -notmatch $expected -or $result -notmatch '(?m)^third_person_gameplay_QA=1\s*$' -or -not(Select-String -LiteralPath $log -Pattern 'RESULT=BOSS_READABILITY_PASSED' -Quiet)){throw 'Boss result failed or incomplete'}
 $errors=@(Select-String -LiteralPath $log -Pattern '\bError:|Fatal error:|Ensure condition failed')
 if($errors.Count){throw ('Unexpected red output errors='+$errors.Count)}
} catch {$failure=$_} finally {
 if($p -and -not $p.HasExited){$current=Get-Process -Id $p.Id -ErrorAction SilentlyContinue;if($current -and $current.StartTime -eq $started -and $current.Path -eq $exe){Stop-Process -InputObject $current -Force;$null=$p.WaitForExit(10000)}}
 $after=@(Saves);$after|ConvertTo-Json -Depth 4|Set-Content -LiteralPath (Join-Path $out 'saves-after.json')
 $lookup=@{};foreach($s in $after){$lookup[$s.path]=$s.sha256};$changed=@($before|Where-Object {-not $lookup.ContainsKey($_.path) -or $lookup[$_.path] -ne $_.sha256});$preserved=$changed.Count -eq 0
 [pscustomobject]@{label=$Label;before=$before.Count;after=$after.Count;preserved=$preserved;exit=if($p -and $p.HasExited){$p.ExitCode}else{$null};failure=if($failure){$failure.ToString()}else{$null};third_person=$true;first_person_qa=$false;binary_sha256=$binarySha;language_override='process only en/en-US'}|ConvertTo-Json|Set-Content -LiteralPath (Join-Path $out 'launcher.json')
}
if(-not $preserved){throw 'Save preservation failure'};if($failure){throw $failure}
$suite = if($WalkContact){'fixed and paced boss ground contact'}else{'third-person boss 18 cases'}
Write-Output ('PASSED '+$suite+'; existing saves preserved='+$before.Count)
