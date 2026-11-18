param(
 [Parameter(Mandatory)][ValidatePattern('^[A-Za-z0-9_]{1,24}$')][string]$Label,
 [ValidateRange(60,900)][int]$TimeoutSeconds=240,
 [double]$HiltMinZ=-26,[double]$HiltMaxZ=14,
 [ValidatePattern('^[A-Za-z0-9_]+$')][string]$PalmR='middle_01_r',
 [ValidatePattern('^[A-Za-z0-9_]+$')][string]$PalmL='middle_01_l'
)
$ErrorActionPreference='Stop'
if([double]::IsNaN($HiltMinZ)-or[double]::IsInfinity($HiltMinZ)-or[double]::IsNaN($HiltMaxZ)-or[double]::IsInfinity($HiltMaxZ)-or$HiltMinZ-ge$HiltMaxZ){throw 'Finite ascending hilt local Z endpoints required'}
$root=(Resolve-Path -LiteralPath (Split-Path -Parent $PSScriptRoot)).Path
$project=Join-Path $root 'Rogue10m.uproject'
$out=Join-Path $root ('Feature\doc\evidence\sword-knuckle-20261110\locomotion\'+$Label)
$images=Join-Path $root ('DevLog\20261110_SwordKnuckle\Locomotion_'+$Label)
if((Test-Path -LiteralPath $out)-or(Test-Path -LiteralPath $images)){throw 'Fresh label and evidence directories required'}
if(@(Get-CimInstance Win32_Process|Where-Object {$_.Name-like'UnrealEditor*'-and$_.CommandLine-and$_.CommandLine.Contains($project)}).Count){throw 'Shared project UE process already running'}
$saveDir=Join-Path $root 'Saved\SaveGames'
function Saves {if(Test-Path -LiteralPath $saveDir){@(Get-ChildItem -LiteralPath $saveDir -Filter '*.sav' -File|ForEach-Object{[pscustomobject]@{path=$_.FullName;sha256=(Get-FileHash -LiteralPath $_.FullName).Hash}})}}
$before=@(Saves)
$town='Rogue10m_Town_QA_SwordMove_'+$Label;$prefix='Rogue10m_Progression_QA_SwordMove_'+$Label;$keys='Rogue10m_Fighter_QA_SwordMove_'+$Label
if(@($before|Where-Object {$n=[IO.Path]::GetFileName($_.path);$n-eq($town+'.sav')-or$n-eq($keys+'.sav')-or$n-like($prefix+'*.sav')}).Count){throw 'Fresh isolated save namespace required'}
New-Item -ItemType Directory -Path $out|Out-Null
$before|ConvertTo-Json -Depth 4|Set-Content -LiteralPath (Join-Path $out 'saves-before.json')
$exe='D:\Program Files\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe';$log=Join-Path $out 'engine.log'
$ci=[Globalization.CultureInfo]::InvariantCulture
$arguments=@(('"'+$project+'"'),'/Game/FirstPerson/Lvl_FirstPerson','-game','-DisablePython','-unattended','-nosplash','-nosound','-NoLiveCoding','-NoRemoteShaderCompile','-language=en','-locale=en-US','-windowed','-ForceRes','-ResX=1280','-ResY=720','-d3d11','-RenderOffScreen',('-abslog="'+$log+'"'),('-RogueProfileSaveSlot='+$town),('-RogueProgressionSavePrefix='+$prefix),('-RogueSkillKeySaveSlot='+$keys),('-RogueSwordLocomotionLabel='+$Label),('-RogueSwordHiltMinZ='+$HiltMinZ.ToString($ci)),('-RogueSwordHiltMaxZ='+$HiltMaxZ.ToString($ci)),('-RogueSwordPalmR='+$PalmR),('-RogueSwordPalmL='+$PalmL),'-ExecCmds="t.MaxFPS 60,Rogue10m.TestGreatSwordLocomotion"')
$p=$null;$started=$null;$failure=$null;$preserved=$false
try{
 $p=Start-Process -FilePath $exe -ArgumentList $arguments -WindowStyle Hidden -PassThru;$started=$p.StartTime
 Write-Output ('Owned greatsword locomotion QA PID='+$p.Id+' log='+$log)
 if(-not$p.WaitForExit($TimeoutSeconds*1000)){throw 'Owned locomotion QA timeout'};$p.Refresh()
 $metrics=Get-Content -LiteralPath (Join-Path $out 'metrics.txt') -Raw;Write-Output $metrics
 if($p.ExitCode-ne0-or$metrics-notmatch'(?m)^failures=0\s*$'-or$metrics-notmatch'(?m)^completed_cases=20\s*$'-or-not(Select-String -LiteralPath $log -Pattern 'RESULT=GREATSWORD_LOCOMOTION_PASSED' -Quiet)){throw ('Locomotion result failed; exit='+$p.ExitCode)}
 $cases=@(Select-String -LiteralPath $log -Pattern 'SWORDMOVE CASE case=');if($cases.Count-ne20){throw '20 complete measured cases required'}
 $samples=@(Import-Csv -LiteralPath (Join-Path $out 'samples.csv'));if($samples.Count-ne27-or@($samples.animation|Select-Object -Unique).Count-ne17){throw '27 sample coordinates and 17 unique task derivatives required'}
 $pngs=@(Get-ChildItem -LiteralPath $images -Filter '*.png' -File);if($pngs.Count-ne20-or@($pngs|Where-Object Length -lt 1024).Count){throw '20 non-empty rendered PNGs required'}
 $events=@(Import-Csv -LiteralPath (Join-Path $out 'captures.csv'));$requests=@($events|Where-Object event -eq 'request');$processed=@($events|Where-Object event -eq 'processed')
 if($requests.Count-ne20-or$processed.Count-ne20-or@($requests.file|Select-Object -Unique).Count-ne20-or@($requests.case|Select-Object -Unique).Count-ne20){throw '20 unique case capture requests and processing records required'}
 foreach($r in $requests){$done=@($processed|Where-Object {$_.file-eq$r.file-and$_.case-eq$r.case});if($done.Count-ne1-or[long]$done[0].bytes-lt1024-or-not(Test-Path -LiteralPath $r.file)){throw ('Unverified screenshot '+$r.file)}}
 $red=@(Select-String -LiteralPath $log -Pattern '\bError:|Fatal error:|Ensure condition failed');if($red.Count){throw ('Unexpected red output errors='+$red.Count)}
}catch{$failure=$_}finally{
 if($p-and-not$p.HasExited){$current=Get-Process -Id $p.Id -ErrorAction SilentlyContinue;if($current-and$current.StartTime-eq$started-and$current.Path-eq$exe){Stop-Process -InputObject $current -Force;$null=$p.WaitForExit(10000)}}
 $after=@(Saves);$after|ConvertTo-Json -Depth 4|Set-Content -LiteralPath (Join-Path $out 'saves-after.json');$lookup=@{};foreach($s in $after){$lookup[$s.path]=$s.sha256};$changed=@($before|Where-Object {-not$lookup.ContainsKey($_.path)-or$lookup[$_.path]-ne$_.sha256});$preserved=$changed.Count-eq0
 [pscustomobject]@{label=$Label;preserved=$preserved;existing_saves=$before.Count;exit=if($p-and$p.HasExited){$p.ExitCode}else{$null};failure=if($failure){$failure.ToString()}else{$null};physical_input=$false;full_run=$false;first_person=$false;movement='DoMove analog probes 300/600cm_s; real CharacterMovement, no velocity injection';coverage='idle, walk8, run8, Knuckle forward walk, Unarmed forward walk, sword idle return; 27 coordinates/17 unique derivatives';hilt_local_z=@($HiltMinZ,$HiltMaxZ);palms=@($PalmR,$PalmL);grip_contract='midpoint(hand,middle base), shaft distance and local Z recorded only';visual_review='required separately; numeric/render capture success is not visual approval'}|ConvertTo-Json -Depth 4|Set-Content -LiteralPath (Join-Path $out 'launcher.json')
}
if(-not$preserved){throw 'Existing save preservation failed'};if($failure){throw $failure}
Write-Output ('PASSED locomotion numeric/capture QA; review all 20 actual PNGs separately: '+$images)
