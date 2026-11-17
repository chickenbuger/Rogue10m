param([string]$Launcher=(Join-Path (Split-Path -Parent $PSScriptRoot) 'VerifyStaffVfxRuntime.ps1'))
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest

# Parse and load only these pure functions. Never dot-source the launcher body:
# it owns engine/process/filesystem actions that are outside this offline test.
$tokens=$null;$parseErrors=$null
$ast=[Management.Automation.Language.Parser]::ParseFile($Launcher,[ref]$tokens,[ref]$parseErrors)
if($parseErrors.Count){throw ('Launcher parse failed: '+($parseErrors.Message-join'; '))}
foreach($name in @('Get-StaffVfxCaptureNumber','Assert-StaffVfxCaptureWindow')){
 $definitions=@($ast.FindAll({param($node) $node-is[Management.Automation.Language.FunctionDefinitionAst]-and$node.Name-eq$name},$true))
 if($definitions.Count-ne1){throw ('Expected exactly one pure function: '+$name)}
 . ([scriptblock]::Create($definitions[0].Extent.Text))
}
$calls=@($ast.FindAll({param($node) $node-is[Management.Automation.Language.CommandAst]-and$node.GetCommandName()-eq'Assert-StaffVfxCaptureWindow'},$true))
if($calls.Count-ne1){throw 'Launcher must invoke the tested capture-window validator exactly once'}

$script:passed=0
function New-Cue([string]$Phase,$Age) {
 $early=$Phase.EndsWith('_010')
 [pscustomobject]@{phase=$Phase;actual_sim_age_s=$Age;cue_min_age_s=$(if($early){'0.10'}else{'0.18'});cue_max_age_s=$(if($early){'0.17'}else{'0.25'});age_source='FNiagaraSystemInstanceController::GetAge';cue_component='/Transient/StaffCue'}
}
function New-Motion([string]$Phase,$Elapsed) {
 [pscustomobject]@{phase=$Phase;accepted_elapsed_s=$Elapsed}
}
function Assert-Allowed([string]$Name,$Request,$Processed) {
 Assert-StaffVfxCaptureWindow $Request $Processed
 ++$script:passed
 Write-Output ('PASS '+$Name)
}
function Assert-Rejected([string]$Name,$Request,$Processed) {
 $rejected=$false
 try{Assert-StaffVfxCaptureWindow $Request $Processed}catch{$rejected=$true}
 if(-not$rejected){throw ('Expected rejection: '+$Name)}
 ++$script:passed
 Write-Output ('PASS '+$Name)
}

foreach($kind in @('cast','impact')){
 Assert-Allowed ($kind+'_010_inclusive_window') (New-Cue ($kind+'_age_010') '0.10') (New-Cue ($kind+'_age_010') '0.17')
 Assert-Allowed ($kind+'_018_inclusive_window') (New-Cue ($kind+'_age_018') '0.18') (New-Cue ($kind+'_age_018') '0.25')
}
Assert-Allowed 'cue_equal_request_processed_age' (New-Cue 'cast_age_010' .16) (New-Cue 'cast_age_010' .16)
Assert-Rejected 'deferred_010_callback_016_to_0183333' (New-Cue 'cast_age_010' .16) (New-Cue 'cast_age_010' .183333)
Assert-Rejected 'late_018_callback_025_to_026' (New-Cue 'impact_age_018' .25) (New-Cue 'impact_age_018' .26)
Assert-Rejected 'cue_backwards' (New-Cue 'cast_age_010' .16) (New-Cue 'cast_age_010' .15)
Assert-Rejected 'cue_request_below_window' (New-Cue 'cast_age_010' .09) (New-Cue 'cast_age_010' .10)
Assert-Rejected 'cue_phase_mismatch' (New-Cue 'cast_age_010' .16) (New-Cue 'impact_age_010' .16)
foreach($field in @('cue_component','age_source','cue_min_age_s','cue_max_age_s')){
 $request=New-Cue 'cast_age_010' .16;$processed=New-Cue 'cast_age_010' .16
 $processed.$field=if($field-eq'cue_min_age_s'){.11}elseif($field-eq'cue_max_age_s'){.18}else{'mismatch'}
 Assert-Rejected ('processed_metadata_mismatch_'+$field) $request $processed
}
foreach($side in @('request','processed')){
 foreach($invalid in @([double]::NaN,[double]::PositiveInfinity,[double]::NegativeInfinity,'', ' ', 'not-a-number', $null, $true)){
  $request=New-Cue 'cast_age_010' .16;$processed=New-Cue 'cast_age_010' .16
  if($side-eq'request'){$request.actual_sim_age_s=$invalid}else{$processed.actual_sim_age_s=$invalid}
  Assert-Rejected ('invalid_cue_age_'+$side+'_'+$script:passed) $request $processed
 }
 $request=New-Cue 'cast_age_010' .16;$processed=New-Cue 'cast_age_010' .16
 if($side-eq'request'){$request.PSObject.Properties.Remove('actual_sim_age_s')}else{$processed.PSObject.Properties.Remove('actual_sim_age_s')}
 Assert-Rejected ('missing_cue_age_'+$side) $request $processed
}

foreach($entry in @(@('motion_030',.30,.40),@('motion_060',.60,.70),@('motion_100',1.,1.10))){
 Assert-Allowed ($entry[0]+'_inclusive_window') (New-Motion $entry[0] $entry[1]) (New-Motion $entry[0] $entry[2])
}
Assert-Rejected 'motion_deferred_030_to_0401' (New-Motion 'motion_030' .30) (New-Motion 'motion_030' .401)
Assert-Rejected 'motion_backwards' (New-Motion 'motion_060' .65) (New-Motion 'motion_060' .64)
Assert-Rejected 'motion_request_below_window' (New-Motion 'motion_100' .99) (New-Motion 'motion_100' 1.)
foreach($side in @('request','processed')){
 foreach($invalid in @([double]::NaN,[double]::PositiveInfinity,[double]::NegativeInfinity,'', ' ', 'not-a-number', $null, $false)){
  $request=New-Motion 'motion_030' .30;$processed=New-Motion 'motion_030' .30
  if($side-eq'request'){$request.accepted_elapsed_s=$invalid}else{$processed.accepted_elapsed_s=$invalid}
  Assert-Rejected ('invalid_motion_elapsed_'+$side+'_'+$script:passed) $request $processed
 }
 $request=New-Motion 'motion_030' .30;$processed=New-Motion 'motion_030' .30
 if($side-eq'request'){$request.PSObject.Properties.Remove('accepted_elapsed_s')}else{$processed.PSObject.Properties.Remove('accepted_elapsed_s')}
 Assert-Rejected ('missing_motion_elapsed_'+$side) $request $processed
}
Assert-Allowed 'recovery_keeps_existing_separate_validator' ([pscustomobject]@{phase='motion_recovery'}) ([pscustomobject]@{phase='motion_recovery'})
Write-Output ('PASSED Staff capture-window checks='+$script:passed+'; pure AST functions only; no launcher body, engine, CIM or rendering executed')
