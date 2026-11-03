param(
 [Parameter(Mandatory)][ValidateSet('Automation','Boss')][string]$Mode,
 [Parameter(Mandatory)][ValidatePattern('^[A-Za-z0-9_]{1,24}$')][string]$Label,
 [string]$ProjectRoot=(Split-Path -Parent $PSScriptRoot),
 [string]$EngineRoot='D:\Program Files\UE_5.8',
 [int]$TimeoutSeconds=300
)
$ErrorActionPreference='Stop'
$projectRoot=(Resolve-Path -LiteralPath $ProjectRoot).Path
$project=Join-Path $projectRoot 'Rogue10m.uproject'
$out=Join-Path $projectRoot ('Feature\doc\evidence\combat-readability-20261103\launchers\'+$Label)
if(Test-Path -LiteralPath $out){throw 'Fresh launcher output required'}
if(@(Get-CimInstance Win32_Process | Where-Object {$_.Name -like 'UnrealEditor*' -and $_.CommandLine -and $_.CommandLine.Contains($project)}).Count){throw 'Project UE process already running'}
New-Item -ItemType Directory -Path $out | Out-Null
function Saves {
 @(Get-ChildItem -LiteralPath (Join-Path $projectRoot 'Saved\SaveGames') -Filter '*.sav' -Recurse -File | ForEach-Object {
 [pscustomobject]@{path=$_.FullName;sha256=(Get-FileHash -LiteralPath $_.FullName).Hash}
 })
}
$before=@(Saves);$before | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $out 'saves-before.json')
$log=Join-Path $out 'engine.log'
$exe=Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$argsList=@(('"'+$project+'"'),'-unattended','-nosplash','-nosound','-NoLiveCoding','-NoRemoteShaderCompile',('-abslog="'+$log+'"'))
if($Mode -eq 'Automation') {
 $filter='Rogue10m.AttackResources+Rogue10m.BasicRunEvidence+Rogue10m.HybridSkillTree+Rogue10m.MonsterAttackGeometry+Rogue10m.MonsterLoot+Rogue10m.OpenWorld+Rogue10m.ProgressionRequirements+Rogue10m.RunCurrencyBaseline+Rogue10m.WeaponCooldown+Rogue10m.WeaponSwitch'
 $argsList+=@('-nullrhi',('-ExecCmds="Automation RunTests '+$filter+'"'),'-TestExit="Automation Test Queue Empty"',('-ReportExportPath="'+(Join-Path $out 'automation')+'"'),
 ('-RogueProfileSaveSlot=Rogue10m_Town_QA_'+$Label),('-RogueProgressionSavePrefix=Rogue10m_Progression_QA_'+$Label),('-RogueSkillKeySaveSlot=Rogue10m_Fighter_QA_'+$Label))
} else {
 $exe=Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor.exe'
 $argsList=@(('"'+$project+'"'),'/Game/Rogue10m/Maps/StartHub')+$argsList[1..($argsList.Count-1)]
 $argsList+=@('-game','-d3d11','-windowed','-ForceRes','-ResX=1280','-ResY=720',
 ('-RogueProfileSaveSlot=Rogue10m_Town_QA_BossReadability_'+$Label),
 ('-RogueProgressionSavePrefix=Rogue10m_Progression_QA_BossReadability_'+$Label),
 ('-RogueSkillKeySaveSlot=Rogue10m_Fighter_QA_BossReadability_'+$Label),
 ('-RogueBossReadabilityLabel='+$Label),'-ExecCmds="t.MaxFPS 60,Rogue10m.TestBossReadability"')
}
$p=$null;$failure=$null;$preserved=$false
try {
 $p=Start-Process -FilePath $exe -ArgumentList $argsList -WindowStyle Hidden -PassThru
 $started=$p.StartTime
 Write-Output ('Owned '+$Mode+' PID='+$p.Id+' log='+$log)
 $timer=[Diagnostics.Stopwatch]::StartNew()
 while(-not $p.WaitForExit(1000)){if($timer.Elapsed.TotalSeconds -gt $TimeoutSeconds){throw 'Owned UE timeout'}}
 $p.Refresh();if($p.ExitCode -ne 0){throw ('UE exit='+$p.ExitCode)}
 if($Mode -eq 'Automation') {
  $report=Get-Content -LiteralPath (Join-Path $out 'automation\index.json') -Raw | ConvertFrom-Json
  $allTests=@($report.tests);$badTests=@($allTests | Where-Object {$_.state -ne 'Success' -or $_.errors -ne 0})
  if($report.failed -ne 0 -or $report.notRun -ne 0 -or $report.inProcess -ne 0 -or $allTests.Count -ne 40 -or $badTests.Count){throw ('Automation report failed/incomplete: '+$report.failed+' failures, '+$allTests.Count+' test rows')}
 }
 if($Mode -eq 'Boss') {
  $result=Get-Content -LiteralPath (Join-Path $projectRoot ('Feature\doc\evidence\boss-readability-20261103\'+$Label+'\results.txt')) -Raw
  if($result -notmatch '(?m)^failures=0\s*$' -or $result -notmatch '(?m)^cases_completed=18\s*$' -or -not(Select-String -LiteralPath $log -Pattern 'RESULT=BOSS_READABILITY_PASSED' -Quiet)){throw 'Boss result failed/incomplete; engine exit alone is insufficient'}
 }
} catch {$failure=$_} finally {
 if($p -and -not $p.HasExited){
  $current=Get-Process -Id $p.Id -ErrorAction SilentlyContinue
  if($current -and $current.StartTime -eq $started -and $current.Path -eq $exe){Stop-Process -InputObject $current -Force;$null=$p.WaitForExit(10000)}
 }
 $after=@(Saves);$after | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $out 'saves-after.json')
 $lookup=@{};foreach($s in $after){$lookup[$s.path]=$s.sha256}
 $changed=@($before | Where-Object {-not $lookup.ContainsKey($_.path) -or $lookup[$_.path] -ne $_.sha256})
 $preserved=$changed.Count -eq 0
 [pscustomobject]@{mode=$Mode;label=$Label;before=$before.Count;after=$after.Count;preserved=$preserved;exit=if($p.HasExited){$p.ExitCode}else{$null};failure=if($failure){$failure.ToString()}else{$null}} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $out 'launcher.json')
}
if(-not $preserved){throw 'Save preservation failure'}
if($failure){throw $failure}
Write-Output ('Completed '+$Mode+'; saves preserved '+$before.Count+' -> '+$after.Count)
