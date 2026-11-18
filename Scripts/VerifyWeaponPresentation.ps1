# Numeric/editor automation only. No first-person suites, view toggles or runtime visual approval.
param(
 [Parameter(Mandatory)][Alias('Label')][ValidatePattern('^[A-Za-z0-9_]{1,24}$')][string]$RunLabel,
 [ValidateRange(60,900)][int]$TimeoutSeconds=240
)
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
$root=(Resolve-Path -LiteralPath (Split-Path -Parent $PSScriptRoot)).Path
$project=Join-Path $root 'Rogue10m.uproject'
$out=Join-Path $root ('Feature\doc\evidence\weapon-presentation\'+$RunLabel)
$reportDir=Join-Path $out 'report'
$reportPath=Join-Path $reportDir 'index.json'
$log=Join-Path $out 'automation.log'
# Exact 10 paths from ResumeSword1/weapons-unit/report/index.json, plus StaffAttachment and StaffUprightMath.
# RunTests is a case-insensitive prefix filter: Weapons also selects WeaponSwitch.
$expected=@(
 'Rogue10m.Weapons.SwordPalmGrip',
 'Rogue10m.Weapons.VisualAttachment',
 'Rogue10m.WeaponSwitch.AllMutationPathsSucceedAtIdle',
 'Rogue10m.WeaponSwitch.AttackBoundaryAndLongCooldown',
 'Rogue10m.WeaponSwitch.ConservativeBusyStates',
 'Rogue10m.WeaponSwitch.LocomotionAndNoInvulnerability',
 'Rogue10m.WeaponSwitch.ReentrantCommandsRejected',
 'Rogue10m.WeaponSwitch.RejectBeforeMutation',
 'Rogue10m.WeaponSwitch.SameTypeAndDuplicateInstance',
 'Rogue10m.WeaponSwitch.ValidationFailureIsAtomic',
 'Rogue10m.Weapons.StaffAttachment',
 'Rogue10m.Weapons.StaffUprightMath'
)
if(Test-Path -LiteralPath $out){throw 'Fresh RunLabel required; existing evidence will not be overwritten.'}
# Conservative machine-wide guard, including commandlet Editors, before taking the shared lease.
$editors=@(Get-CimInstance Win32_Process|Where-Object {$_.Name -like 'UnrealEditor*'})
if($editors.Count){throw ('Editor already running; wait for the shared lease. PIDs='+($editors.ProcessId -join ','))}
$exe='D:\Program Files\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
if(-not(Test-Path -LiteralPath $exe)){throw ('Editor executable missing: '+$exe)}
$saveDir=Join-Path $root 'Saved\SaveGames'
function Get-SaveSnapshot {
 if(Test-Path -LiteralPath $saveDir){
  Get-ChildItem -LiteralPath $saveDir -Filter '*.sav' -File|Sort-Object FullName|ForEach-Object {
   [pscustomobject]@{path=$_.FullName;sha256=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash}
  }
 }
}
$town='Rogue10m_Town_QA_WeaponPresentation_'+$RunLabel
$progression='Rogue10m_Progression_QA_WeaponPresentation_'+$RunLabel
$keys='Rogue10m_Fighter_QA_WeaponPresentation_'+$RunLabel
foreach($slot in @($town,$progression,$keys)){if($slot.Length -gt 80){throw 'QA slot too long; refuse production fallback'}}
$before=@(Get-SaveSnapshot)
foreach($save in $before){
 $name=[IO.Path]::GetFileName($save.path)
 if($name -eq ($town+'.sav') -or $name -eq ($keys+'.sav') -or $name -like ($progression+'*.sav')){throw 'Isolated save namespace already exists; use a fresh RunLabel.'}
}
New-Item -ItemType Directory -Path $out|Out-Null
ConvertTo-Json -InputObject $before -Depth 4|Set-Content -LiteralPath (Join-Path $out 'saves-before.json') -Encoding utf8
$arguments=@(
 ('"'+$project+'"'),'/Engine/Maps/Entry','-nullrhi','-unattended','-nosplash','-nosound',
 '-DisablePython','-NoLiveCoding','-NoRemoteShaderCompile','-language=en','-locale=en-US',
 ('-RogueProfileSaveSlot='+$town),('-RogueProgressionSavePrefix='+$progression),('-RogueSkillKeySaveSlot='+$keys),
 '-ExecCmds="Automation RunTests Rogue10m.Weapons"','-TestExit="Automation Test Queue Empty"',
 ('-ReportExportPath="'+$reportDir+'"'),('-abslog="'+$log+'"')
)
$process=$null;$started=$null;$failure=$null;$preserved=$false;$exitCode=$null
$testRows=@();$changed=@();$after=@();$verified=$false;$cleanupFailure=$null
try {
 # Recheck immediately before launch; this wrapper never closes another owner's Editor.
 if(@(Get-CimInstance Win32_Process|Where-Object {$_.Name -like 'UnrealEditor*'}).Count){throw 'Editor appeared before launch; shared lease unavailable.'}
 $process=Start-Process -FilePath $exe -ArgumentList $arguments -WindowStyle Hidden -PassThru
 $started=$process.StartTime
 Write-Output ('Owned weapon presentation automation PID='+$process.Id+' log='+$log)
 if(-not $process.WaitForExit($TimeoutSeconds*1000)){throw 'Owned weapon presentation automation timeout.'}
 $process.Refresh();$exitCode=$process.ExitCode
 if($exitCode -ne 0){throw ('Editor exit='+$exitCode)}
 if(-not(Test-Path -LiteralPath $reportPath)){throw 'Fresh automation report missing.'}
 $report=Get-Content -LiteralPath $reportPath -Raw|ConvertFrom-Json
 $tests=@($report.tests)
 $testRows=@($tests|Select-Object fullTestPath,state,warnings,errors)
 if($report.succeeded -ne 12 -or $report.succeededWithWarnings -ne 0 -or $report.failed -ne 0 -or $report.notRun -ne 0 -or $report.inProcess -ne 0 -or $tests.Count -ne 12){throw 'Expected exactly 12 successes; no warnings, failures, notRun or inProcess.'}
 if(@($tests.fullTestPath|Select-Object -Unique).Count -ne 12){throw 'Duplicate automation paths.'}
 foreach($path in $expected){
  $matches=@($tests|Where-Object {$_.fullTestPath -ceq $path})
  if($matches.Count -ne 1){throw ('Missing or duplicate exact fullTestPath: '+$path)}
  $test=$matches[0]
  if($test.state -cne 'Success' -or $test.warnings -ne 0 -or $test.errors -ne 0){throw ('Test must be exact Success with zero warnings/errors: '+$path)}
  $badEntries=@($test.entries|Where-Object {$_.event.type -in @('Warning','Error')})
  if($badEntries.Count){throw ('Warning/error entries despite counters: '+$path)}
 }
 $red=@(Select-String -LiteralPath $log -Pattern '\bError:|Fatal error:|Ensure condition failed')
 if($red.Count){throw ('Unexpected engine errors='+$red.Count)}
 $verified=$true
}catch{$failure=$_}finally{
 try{
  if($process -and -not $process.HasExited){
   $current=Get-Process -Id $process.Id -ErrorAction SilentlyContinue
   if($current -and $current.StartTime -eq $started -and $current.Path -eq $exe){
    Stop-Process -InputObject $current -Force
    if(-not $process.WaitForExit(10000)){throw 'Owned process did not exit after timeout termination.'}
   }else{throw 'Process identity changed or unavailable; refusing to terminate a non-verified process.'}
  }
  if($process -and $process.HasExited){$process.Refresh();$exitCode=$process.ExitCode}
 }catch{$cleanupFailure=$_.ToString()}
 try{
  $after=@(Get-SaveSnapshot)
  ConvertTo-Json -InputObject $after -Depth 4|Set-Content -LiteralPath (Join-Path $out 'saves-after.json') -Encoding utf8
  $lookup=@{};foreach($save in $after){$lookup[$save.path]=$save.sha256}
  $changed=@($before|Where-Object {-not $lookup.ContainsKey($_.path) -or $lookup[$_.path] -ne $_.sha256})
  $preserved=$changed.Count -eq 0
 }catch{if(-not $failure){$failure=$_};$preserved=$false}
 $summary=[ordered]@{
  result=if($verified -and $preserved -and -not $failure -and -not $cleanupFailure){'PASSED'}else{'FAILED'}
  run_label=$RunLabel;filter='Rogue10m.Weapons';expected_count=$expected.Count;expected_paths=$expected;tests=$testRows
  report=$reportPath;log=$log;exit=$exitCode;existing_save_count=$before.Count;saves_preserved=$preserved;changed_existing_saves=$changed
  failure=if($failure){$failure.ToString()}else{$null};cleanup_failure=$cleanupFailure
  no_first_person=$true;scope='Weapon presentation and WeaponSwitch editor automation only; no first-person suites or view toggles.'
  rendered_visual_approval=$false;physical_input=$false;full_run=$false
  isolated_save_slots=@{profile=$town;progression_prefix=$progression;keys=$keys}
 }
 $summary|ConvertTo-Json -Depth 8|Set-Content -LiteralPath (Join-Path $out 'summary.json') -Encoding utf8
}
if($cleanupFailure){throw $cleanupFailure}
if(-not $preserved){throw 'Existing save hashes changed or preservation could not be verified; inspect summary.json.'}
if($failure){throw $failure}
Write-Output ('RESULT=WEAPON_PRESENTATION_PASSED exact_success='+$expected.Count+' warnings=0 errors=0 saves_preserved='+$before.Count)
Write-Output ('Summary: '+(Join-Path $out 'summary.json'))
