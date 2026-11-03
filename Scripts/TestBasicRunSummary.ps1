param(
    [string]$LauncherPath = (Join-Path $PSScriptRoot 'VerifyBasicRunPlay.ps1')
)
$ErrorActionPreference = 'Stop'
$utf8 = [Text.UTF8Encoding]::new($false)
$source = [IO.Path]::GetFullPath($LauncherPath)
$sourceHash = (Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash
$tokens = $null; $parseErrors = $null
$ast = [Management.Automation.Language.Parser]::ParseFile($source, [ref]$tokens, [ref]$parseErrors)
if ($parseErrors.Count) { throw 'Launcher AST has parse errors; no fixture code executed.' }
$functions = @($ast.FindAll({
    param($node)
    $node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -ceq 'Read-DriverSummary'
}, $true))
if ($functions.Count -ne 1) { throw 'Exactly one Read-DriverSummary definition is required.' }
# Execute only this definition. Never dot-source or invoke the launcher/top-level AST.
$definition = $functions[0].Extent.Text
. ([scriptblock]::Create($definition))

$runName = 'synthetic-' + [DateTime]::UtcNow.ToString('yyyyMMddTHHmmssfff') + '-' + [guid]::NewGuid().ToString('N').Substring(0, 8)
$output = Join-Path (Join-Path (Split-Path -Parent $PSScriptRoot) 'Feature/doc/evidence/next-run-validation-20261103/parser') $runName
[IO.Directory]::CreateDirectory($output) | Out-Null
$results = [Collections.Generic.List[object]]::new()
$allFlags = @(
    'first_run_full_gate_frozen','next_run_hub_verified','next_run_field_verified','next_run_timer_verified',
    'next_run_cancel_focus_observed','next_run_cancel_action_verified','next_run_abandon_ui_submitted',
    'next_run_saved_receipt_observed','next_run_returned_hub','next_run_same_profile_verified',
    'next_run_connection_complete','next_run_returned_slot_reset_verified',
    'next_run_slot_initial_baseline_recorded','next_run_slot_reset_verified'
)
function New-CompleteFixture {
    $row = [ordered]@{
        outcome='returned_victory';full_boss_completion='1';physical_input='not_performed'
        prepared_boss_strategy='0';orbit_boss_strategy='0';gear_ui_requested='0';great_sword_growth_requested='0'
        recording_required='0';recording_ready_observed='0'
        next_run_connection_requested='1';first_run_frozen_outcome='returned_victory';first_run_frozen_terminal='Victory'
        next_run_status='complete';next_run_slot_comparison_scope='four_effective_stable_asset_or_null_and_IsAttackSkillUnlocked'
        next_run_first_id='11111111111111111111111111111111'
        next_run_id='22222222222222222222222222222222'
        next_run_after_return_id='33333333333333333333333333333333'
    }
    foreach ($key in $allFlags) { $row[$key] = '1' }
    return $row
}
function Test-Fixture {
    param(
        [string]$Name,
        [scriptblock]$Change = {},
        [bool]$ExpectValid = $false,
        [string]$ExpectedIssue = '',
        [bool]$ExpectNextRun = $true,
        [string]$ExtraLine = '',
        [bool]$MissingFile = $false
    )
    $row = New-CompleteFixture
    & $Change $row
    $path = Join-Path $output ($Name + '.txt')
    if (-not $MissingFile) {
        $lines = @($row.GetEnumerator() | ForEach-Object { $_.Key + '=' + [string]$_.Value })
        if ($ExtraLine) { $lines += $ExtraLine }
        [IO.File]::WriteAllLines($path, $lines, $utf8)
    }
    $parsed = Read-DriverSummary -Path $path -ExpectNextRun $ExpectNextRun
    $hasIssue = [string]::IsNullOrEmpty($ExpectedIssue) -or @($parsed.Issues | Where-Object {
        ([string]$_).StartsWith($ExpectedIssue, [StringComparison]::Ordinal)
    }).Count -gt 0
    $passed = ([bool]$parsed.Valid -eq $ExpectValid) -and $hasIssue
    $results.Add([pscustomobject][ordered]@{
        Name=$Name;Passed=$passed;ExpectedValid=$ExpectValid;ActualValid=[bool]$parsed.Valid
        ExpectedIssue=$ExpectedIssue;ActualIssues=@($parsed.Issues)
        NextRunComplete=$parsed.Values['next_run_connection_complete']
    })
    Write-Output ((@('FAIL','PASS')[[int]$passed]) + ' ' + $Name)
}

Test-Fixture 'complete_all_observed_boundaries' -ExpectValid $true
Test-Fixture 'missing_cancel_action' { param($r) $r.Remove('next_run_cancel_action_verified') } -ExpectedIssue 'invalid_or_missing_next_run_cancel_action_verified'
Test-Fixture 'focus_only_is_not_cancel_action' { param($r) $r['next_run_cancel_action_verified']='0' } -ExpectedIssue 'next_run_dependency_missing:next_run_abandon_ui_submitted:next_run_cancel_action_verified'
Test-Fixture 'cancel_without_focus' { param($r) $r['next_run_cancel_focus_observed']='0' } -ExpectedIssue 'next_run_dependency_missing:next_run_cancel_action_verified:next_run_cancel_focus_observed'
Test-Fixture 'cancel_without_running_timer' { param($r) $r['next_run_timer_verified']='0' } -ExpectedIssue 'next_run_dependency_missing:next_run_cancel_action_verified:next_run_timer_verified'
Test-Fixture 'missing_final_slot_comparison' { param($r) $r.Remove('next_run_returned_slot_reset_verified') } -ExpectedIssue 'invalid_or_missing_next_run_returned_slot_reset_verified'
Test-Fixture 'field_slots_alone_do_not_prove_return_slots' { param($r) $r['next_run_returned_slot_reset_verified']='0' } -ExpectedIssue 'next_run_dependency_missing:next_run_returned_hub:next_run_returned_slot_reset_verified'
Test-Fixture 'return_slot_without_field_slot_baseline' { param($r) $r['next_run_slot_reset_verified']='0' } -ExpectedIssue 'next_run_dependency_missing:next_run_returned_slot_reset_verified:next_run_slot_reset_verified'
Test-Fixture 'return_slot_without_saved_receipt' { param($r) $r['next_run_saved_receipt_observed']='0' } -ExpectedIssue 'next_run_dependency_missing:next_run_returned_slot_reset_verified:next_run_saved_receipt_observed'
Test-Fixture 'slot_comparison_without_initial_baseline' { param($r) $r['next_run_slot_initial_baseline_recorded']='0' } -ExpectedIssue 'next_run_slot_verification_without_initial_baseline_or_first_victory'
Test-Fixture 'completion_without_same_profile' { param($r) $r['next_run_same_profile_verified']='0' } -ExpectedIssue 'next_run_dependency_missing:next_run_connection_complete:next_run_same_profile_verified'
Test-Fixture 'completion_without_first_victory_freeze' { param($r) $r['first_run_full_gate_frozen']='0' } -ExpectedIssue 'next_run_milestone_without_first_victory:'
Test-Fixture 'missing_first_victory_freeze' { param($r) $r.Remove('first_run_full_gate_frozen') } -ExpectedIssue 'invalid_or_missing_first_run_full_gate_frozen'
Test-Fixture 'frozen_victory_with_incomplete_followup' { param($r) $r['next_run_connection_complete']='0';$r['next_run_status']='blocked' } -ExpectedIssue 'next_run_connection_incomplete_after_first_victory'
Test-Fixture 'frozen_wrong_outcome' { param($r) $r['first_run_frozen_outcome']='returned_death' } -ExpectedIssue 'next_run_without_preserved_first_full_victory'
Test-Fixture 'frozen_wrong_terminal' { param($r) $r['first_run_frozen_terminal']='Defeat' } -ExpectedIssue 'next_run_without_preserved_first_full_victory'
Test-Fixture 'duplicate_first_and_second_run_ids' { param($r) $r['next_run_id']=$r['next_run_first_id'] } -ExpectedIssue 'next_run_RunIds_not_distinct_at_Hub_boundaries'
Test-Fixture 'duplicate_first_and_final_run_ids' { param($r) $r['next_run_after_return_id']=$r['next_run_first_id'] } -ExpectedIssue 'next_run_RunIds_not_distinct_at_Hub_boundaries'
Test-Fixture 'empty_final_run_id' { param($r) $r['next_run_after_return_id']='00000000000000000000000000000000' } -ExpectedIssue 'next_run_invalid_RunId:next_run_after_return_id'
Test-Fixture 'noncanonical_run_id' { param($r) $r['next_run_id']='22222222-2222-2222-2222-222222222222' } -ExpectedIssue 'next_run_invalid_RunId:next_run_id'
Test-Fixture 'complete_flag_with_blocked_status' { param($r) $r['next_run_status']='blocked' } -ExpectedIssue 'next_run_completed_with_wrong_status'
Test-Fixture 'duplicate_summary_key' -ExtraLine 'next_run_cancel_action_verified=1' -ExpectedIssue 'duplicate_summary_key:next_run_cancel_action_verified'
Test-Fixture 'malformed_cancel_boolean' { param($r) $r['next_run_cancel_action_verified']='true' } -ExpectedIssue 'invalid_or_missing_next_run_cancel_action_verified'
Test-Fixture 'unexpected_next_run_mode' -ExpectNextRun $false -ExpectedIssue 'unexpected_next_run_connection_in_baseline_run'
Test-Fixture 'requested_next_run_not_echoed' { param($r) $r['next_run_connection_requested']='0' } -ExpectedIssue 'requested_next_run_connection_not_confirmed'
Test-Fixture 'missing_summary_file' -MissingFile $true -ExpectedIssue 'driver_summary_missing'

# Natural first-run death/timeout is a valid recorded run, but explicitly not a next-run success.
foreach ($ending in @('death','timeout')) {
    $row = New-CompleteFixture
    $row['outcome']='returned_'+$ending;$row['full_boss_completion']='0'
    foreach ($key in $allFlags) { $row[$key]='0' }
    $row['next_run_slot_initial_baseline_recorded']='1'
    $row['first_run_frozen_outcome']='';$row['first_run_frozen_terminal']=''
    $row['next_run_status']='not_attempted_first_no_victory'
    $path=Join-Path $output ('recorded_first_'+$ending+'_not_next_run_success.txt')
    [IO.File]::WriteAllLines($path,@($row.GetEnumerator()|ForEach-Object{$_.Key+'='+[string]$_.Value}),$utf8)
    $parsed=Read-DriverSummary -Path $path -ExpectNextRun $true
    $passed=[bool]$parsed.Valid -and $parsed.Values['next_run_connection_complete'] -ceq '0'
    $results.Add([pscustomobject][ordered]@{Name=('recorded_first_'+$ending+'_not_next_run_success');Passed=$passed;ExpectedValid=$true;ActualValid=[bool]$parsed.Valid;ExpectedIssue='';ActualIssues=@($parsed.Issues);NextRunComplete=$parsed.Values['next_run_connection_complete']})
    Write-Output ((@('FAIL','PASS')[[int]$passed])+' recorded_first_'+$ending+'_not_next_run_success')
}

if ((Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash -cne $sourceHash) { throw 'Launcher changed during parser fixtures; results require a fresh run.' }
$failed=@($results|Where-Object{-not $_.Passed}).Count
$report=[ordered]@{
    Scope='synthetic_summary_parser_only_not_UE_or_runtime_verification'
    LauncherPath=$source;LauncherSHA256=$sourceHash
    Extraction='one_Read-DriverSummary_FunctionDefinitionAst_only; launcher_never_dot_sourced_or_launched'
    LauncherASTErrors=$parseErrors.Count;Tests=$results.Count;Passed=($results.Count-$failed);Failed=$failed;Results=@($results.ToArray())
}
[IO.File]::WriteAllText((Join-Path $output 'report.json'),(ConvertTo-Json -InputObject $report -Depth 12),$utf8)
Write-Output ('SUMMARY_PARSER_FIXTURES tests='+$results.Count+' failed='+$failed+' report='+(Join-Path $output 'report.json'))
if ($failed) { throw 'Synthetic parser fixtures failed. Inspect retained report; do not count these as runtime tests.' }
