param([Parameter(Mandatory)][ValidatePattern('^[A-Za-z0-9_]{1,24}$')][string]$Label,[switch]$NullRHI,[int]$TimeoutSeconds=240)
$ErrorActionPreference='Stop'
$root=(Resolve-Path -LiteralPath (Split-Path -Parent $PSScriptRoot)).Path
$project=Join-Path $root 'Rogue10m.uproject'
$out=Join-Path $root ('Feature\doc\evidence\weapon-identity-20261110\'+$Label)
if(Test-Path -LiteralPath $out){throw 'Fresh evidence directory required'}
if(@(Get-CimInstance Win32_Process | Where-Object {$_.Name -like 'UnrealEditor*' -and $_.CommandLine -and $_.CommandLine.Contains($project)}).Count){throw 'Shared project UE process already running'}
New-Item -ItemType Directory -Path $out|Out-Null
function Saves{@(Get-ChildItem -LiteralPath (Join-Path $root 'Saved\SaveGames') -Filter '*.sav' -File|ForEach-Object{[pscustomobject]@{path=$_.FullName;sha256=(Get-FileHash -LiteralPath $_.FullName).Hash}})}
$before=@(Saves);$before|ConvertTo-Json -Depth 4|Set-Content -LiteralPath (Join-Path $out 'saves-before.json')
$log=Join-Path $out 'engine.log';$exe='D:\Program Files\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$argsList=@(('"'+$project+'"'),'/Game/Rogue10m/Maps/StartHub','-game','-DisablePython','-unattended','-nosplash','-nosound','-NoLiveCoding','-NoRemoteShaderCompile','-language=en','-locale=en-US','-windowed','-ForceRes','-ResX=1280','-ResY=720',('-abslog="'+$log+'"'),('-RogueProfileSaveSlot=Rogue10m_Town_QA_WeaponIdentity_'+$Label),('-RogueProgressionSavePrefix=Rogue10m_Progression_QA_WeaponIdentity_'+$Label),('-RogueSkillKeySaveSlot=Rogue10m_Fighter_QA_WeaponIdentity_'+$Label),('-RogueWeaponIdentityQALabel='+$Label),'-ExecCmds="t.MaxFPS 60,Rogue10m.TestWeaponIdentity"')
if($NullRHI){$argsList+='-nullrhi'}else{$argsList+=@('-d3d11','-RenderOffScreen')}
$p=$null;$failure=$null;$started=$null
try{
 $p=Start-Process -FilePath $exe -ArgumentList $argsList -WindowStyle Hidden -PassThru;$started=$p.StartTime;Write-Output ('Owned weapon-identity QA PID='+$p.Id+' log='+$log)
 if(-not $p.WaitForExit($TimeoutSeconds*1000)){throw 'Owned weapon identity UE timeout'};$p.Refresh()
 $result=Get-Content -LiteralPath (Join-Path $root ('DevLog\20261110_WeaponIdentity\'+$Label+'\results.txt')) -Raw;Write-Output $result
 if($p.ExitCode -ne 0 -or $result -notmatch '(?m)^failures=0\s*$' -or -not(Select-String -LiteralPath $log -Pattern 'RESULT=WEAPON_IDENTITY_PASSED' -Quiet)){throw ('Weapon identity result failed; UE exit='+$p.ExitCode)}
 if(-not $NullRHI){foreach($name in @('01_baseline.png','02_active.png','03_late_active.png','04_expired.png','05_before_weapon_swap.png','06_weapon_swap_cleared.png','07_before_death.png','08_death_cleared.png')){$file=Join-Path $root ('DevLog\20261110_WeaponIdentity\'+$Label+'\'+$name);if(-not(Test-Path -LiteralPath $file) -or (Get-Item -LiteralPath $file).Length-lt1024){throw ('Missing screenshot '+$name)}}}
 $errors=@(Select-String -LiteralPath $log -Pattern '\bError:|Fatal error:|Ensure condition failed');if($errors.Count){throw ('Unexpected red output errors='+$errors.Count)}
}catch{$failure=$_}finally{
 if($p-and-not$p.HasExited){$current=Get-Process -Id $p.Id -ErrorAction SilentlyContinue;if($current-and$current.StartTime-eq$started-and$current.Path-eq$exe){Stop-Process -InputObject $current -Force;$null=$p.WaitForExit(10000)}}
 $after=@(Saves);$after|ConvertTo-Json -Depth 4|Set-Content -LiteralPath (Join-Path $out 'saves-after.json');$lookup=@{};foreach($s in $after){$lookup[$s.path]=$s.sha256};$changed=@($before|Where-Object{-not$lookup.ContainsKey($_.path)-or$lookup[$_.path]-ne$_.sha256});$preserved=$changed.Count-eq0
 [pscustomobject]@{label=$Label;before=$before.Count;after=$after.Count;preserved=$preserved;exit=if($p-and$p.HasExited){$p.ExitCode}else{$null};failure=if($failure){$failure.ToString()}else{$null};physical_input=$false;full_run=$false;balance='prototype assumptions';run_end='direct production handler only';repeat_cases='QA cooldown reset between independent lifecycle cases';language_override='process only en/en-US'}|ConvertTo-Json|Set-Content -LiteralPath (Join-Path $out 'launcher.json')
}
if(-not$preserved){throw 'Save preservation failure'};if($failure){throw $failure}
Write-Output ('PASSED weapon identity numeric/render capture QA; existing saves preserved='+$before.Count+'; inspect actual PNGs separately')