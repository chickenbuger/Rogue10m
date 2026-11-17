param(
    [ValidatePattern('^[A-Za-z0-9_]{1,24}$')]
    [string]$Label = 'SelfTest01',
    [string]$OutputRoot = '',
    [string]$HistoricalLogPath = '',
    [string]$HistoricalFailedLogPath = ''
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

# This runner tests readiness helpers and the entry point using isolated text
# fixtures. It never starts Unreal, UBT, Git, or a subprocess. The default
# evidence stays under the project tmp directory.
$projectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$modulePath = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\QA\MonsterAITargetReadiness.psm1'))
if (-not $OutputRoot) { $OutputRoot = Join-Path $projectRoot 'tmp\monster-ai-readiness-tests' }
$output = Join-Path $OutputRoot $Label
if (Test-Path -LiteralPath $output) { throw 'Use a fresh self-test evidence label.' }
New-Item -ItemType Directory -Path $output -Force | Out-Null

$results = [Collections.Generic.List[object]]::new()
$harnessFailure = $null
$moduleHash = $null
$historicalLogHash = $null
$historicalFailedLogHash = $null

function Assert-Equal([object]$Actual, [object]$Expected, [string]$Description) {
    if ($Actual -cne $Expected) {
        throw ('{0}: expected [{1}], received [{2}]' -f $Description, $Expected, $Actual)
    }
}

function Assert-True([bool]$Condition, [string]$Description) {
    if (-not $Condition) { throw $Description }
}

function Write-TextFixture([string]$Root, [string]$RelativePath, [string]$Text) {
    $path = Join-Path $Root $RelativePath
    New-Item -ItemType Directory -Path (Split-Path -Parent $path) -Force | Out-Null
    [IO.File]::WriteAllText($path, $Text)
}

function Invoke-ReadinessCase([string]$Name, [scriptblock]$Body) {
    $caseFailure = $null
    try { & $Body } catch { $caseFailure = $_.Exception.Message }
    $results.Add([ordered]@{
        name = $Name
        passed = ($null -eq $caseFailure)
        failure = $caseFailure
    })
}

function Invoke-SourceCase {
    param(
        [string]$Name,
        [AllowEmptyString()][string]$Controller,
        [AllowEmptyString()][string]$Tests,
        [hashtable]$Expected
    )
    Invoke-ReadinessCase $Name {
        $actual = Get-MonsterAITargetSourceState `
            -CurrentController $Controller -BaselineController $baselineController `
            -PreparedController $preparedController -CurrentTests $Tests -PreparedTests $preparedTests
        Assert-Equal $actual.ControllerState $Expected.Controller 'Controller classification'
        Assert-Equal $actual.TestState $Expected.Tests 'Test classification'
        Assert-Equal $actual.LegacyPatchDisposition $Expected.Disposition 'Legacy patch disposition'
        if ($Expected.ContainsKey('FixPresent')) {
            Assert-Equal $actual.ControllerFixPresent $Expected.FixPresent 'Production fix presence'
        }
    }
}

function Invoke-LogCase {
    param(
        [string]$Name,
        [AllowEmptyString()][string]$Log,
        [bool]$ExpectedLogPassed,
        [bool]$ExpectedPassed,
        [bool]$ExpectedExitVerified = $true,
        [AllowNull()][Nullable[int]]$ExitCode = 0,
        [switch]$OmitExitCode,
        [int]$ExpectedSuccessCount = -1
    )
    Invoke-ReadinessCase $Name {
        if ($OmitExitCode) {
            $actual = Get-MonsterAITargetAutomationResult -LogText $Log
        } else {
            $actual = Get-MonsterAITargetAutomationResult -LogText $Log -ExitCode $ExitCode
        }
        Assert-Equal $actual.LogPassed $ExpectedLogPassed 'Log-only acceptance'
        Assert-Equal $actual.Passed $ExpectedPassed 'Process-backed acceptance'
        Assert-Equal $actual.ProcessExitVerified $ExpectedExitVerified 'Exit verification'
        if ($ExpectedSuccessCount -ge 0) {
            Assert-Equal $actual.SuccessCount $ExpectedSuccessCount 'Successful test count'
        }
        if ($ExpectedPassed) {
            Assert-Equal @($actual.Failures).Count 0 'Successful result has no failures'
        } elseif (-not $ExpectedLogPassed -or $ExpectedExitVerified) {
            Assert-True (@($actual.Failures).Count -gt 0) 'A rejected result must explain its failure.'
        }
    }
}

try {
    if (-not (Test-Path -LiteralPath $modulePath -PathType Leaf)) {
        throw ('Readiness module missing: ' + $modulePath)
    }
    $moduleHash = (Get-FileHash -LiteralPath $modulePath -Algorithm SHA256).Hash
    Import-Module -Name $modulePath -Force -ErrorAction Stop

    $baselineController = @'
bool AMonsterController::ExecuteBehaviorDecision()
{
    ARogue10mCharacter* Target = GetBlackboardTarget();
    if (Target && !IsValidCombatTarget(Target))
    {
        ClearTarget();
        Target = nullptr;
    }
    return true;
}
'@
    $preparedController = @'
bool AMonsterController::ExecuteBehaviorDecision()
{
    ARogue10mCharacter* Target = GetBlackboardTarget();
    if ((Target || MonsterBlackboardComponent->GetValueAsBool(HasTargetKey))
        && !IsValidCombatTarget(Target))
    {
        ClearTarget();
        Target = nullptr;
    }
    return true;
}
'@
    $names = @(
        'Rogue10m.MonsterAI.TargetLifecycle.DestroyedActorClearsState',
        'Rogue10m.MonsterAI.TargetLifecycle.ClearedObjectKeyCancelsWindup',
        'Rogue10m.MonsterAI.TargetLifecycle.LiveTargetRetainsWindup',
        'Rogue10m.MonsterAI.TargetLifecycle.StoppedAIIgnoresStimulus',
        'Rogue10m.MonsterAI.TargetLifecycle.EmptyTargetPreservesPendingPatrol'
    )
    $registrations = @(for ($index = 0; $index -lt $names.Count; $index++) {
        'IMPLEMENT_SIMPLE_AUTOMATION_TEST(FScenario' + $index + ', "' + $names[$index] + '", EAutomationTestFlags::EngineFilter)'
    })
    $preparedTests = "void InitializeFixture() { CreateNativeTarget(); }`n" + ($registrations -join "`n")
    $evolvedTests = "void InitializeFixture() { CreateConcreteTargetAndPlayerState(); }`n" + ($registrations -join "`n")

    Invoke-SourceCase 'source_baseline_and_missing_tests_need_apply' $baselineController '' @{
        Controller = 'baseline'; Tests = 'missing'; Disposition = 'needs_apply'; FixPresent = $false
    }
    Invoke-SourceCase 'source_exact_prepared_pair_already_applied' $preparedController $preparedTests @{
        Controller = 'prepared'; Tests = 'prepared'; Disposition = 'already_applied'; FixPresent = $true
    }
    $preparedCRLF = $preparedController.Replace("`r`n", "`n").Replace("`n", "`r`n")
    $testsCRLF = $preparedTests.Replace("`r`n", "`n").Replace("`n", "`r`n")
    Invoke-SourceCase 'source_line_endings_do_not_imply_divergence' $preparedCRLF $testsCRLF @{
        Controller = 'prepared'; Tests = 'prepared'; Disposition = 'already_applied'; FixPresent = $true
    }
    Invoke-SourceCase 'source_updated_fixture_is_evolved_not_exact_patch' $preparedController $evolvedTests @{
        Controller = 'prepared'; Tests = 'evolved'; Disposition = 'diverged'; FixPresent = $true
    }
    Invoke-SourceCase 'source_fixed_controller_with_unrelated_edit_is_diverged' ($preparedController + "`nvoid NewBehavior() {}") $preparedTests @{
        Controller = 'prepared_guard_with_drift'; Tests = 'prepared'; Disposition = 'diverged'; FixPresent = $true
    }
    Invoke-SourceCase 'source_old_guard_with_unrelated_edit_is_diverged' ($baselineController + "`nvoid NewBehavior() {}") '' @{
        Controller = 'baseline_guard_with_drift'; Tests = 'missing'; Disposition = 'diverged'; FixPresent = $false
    }
    Invoke-SourceCase 'source_missing_controller_requires_manual_review' '' '' @{
        Controller = 'missing'; Tests = 'missing'; Disposition = 'manual_review'; FixPresent = $false
    }
    Invoke-SourceCase 'source_unrecognized_decision_requires_manual_review' 'void Decide() { Patrol(); }' $preparedTests @{
        Controller = 'unknown'; Tests = 'prepared'; Disposition = 'manual_review'; FixPresent = $false
    }
    Invoke-SourceCase 'source_both_guards_require_manual_review' ($baselineController + "`n" + $preparedController) $preparedTests @{
        Controller = 'ambiguous'; Tests = 'prepared'; Disposition = 'manual_review'
    }
    Invoke-SourceCase 'source_fixed_controller_without_tests_is_partial' $preparedController '' @{
        Controller = 'prepared'; Tests = 'missing'; Disposition = 'diverged'; FixPresent = $true
    }
    Invoke-SourceCase 'source_tests_without_controller_fix_are_partial' $baselineController $preparedTests @{
        Controller = 'baseline'; Tests = 'prepared'; Disposition = 'diverged'; FixPresent = $false
    }
    Invoke-SourceCase 'source_missing_controller_with_tests_requires_review' '' $preparedTests @{
        Controller = 'missing'; Tests = 'prepared'; Disposition = 'manual_review'; FixPresent = $false
    }
    $reformattedGuard = $preparedController.Replace('Target || MonsterBlackboardComponent->GetValueAsBool(HasTargetKey)',
        "Target`n            || MonsterBlackboardComponent -> GetValueAsBool ( HasTargetKey )")
    Invoke-SourceCase 'source_reformatted_fixed_guard_still_recognized' $reformattedGuard $preparedTests @{
        Controller = 'prepared_guard_with_drift'; Tests = 'prepared'; Disposition = 'diverged'; FixPresent = $true
    }
    Invoke-SourceCase 'source_reordered_registrations_preserve_test_identity' $preparedController ($registrations[4..0] -join "`n") @{
        Controller = 'prepared'; Tests = 'evolved'; Disposition = 'diverged'; FixPresent = $true
    }
    Invoke-SourceCase 'source_missing_registration_is_unexpected' $preparedController ($registrations[0..3] -join "`n") @{
        Controller = 'prepared'; Tests = 'unexpected_test_set'; Disposition = 'manual_review'; FixPresent = $true
    }
    Invoke-SourceCase 'source_duplicate_registration_is_unexpected' $preparedController ($preparedTests + "`n" + $registrations[0]) @{
        Controller = 'prepared'; Tests = 'unexpected_test_set'; Disposition = 'manual_review'; FixPresent = $true
    }
    Invoke-SourceCase 'source_extra_suite_registration_requires_review' $preparedController ($preparedTests + "`n" +
        'IMPLEMENT_SIMPLE_AUTOMATION_TEST(FExtra, "Rogue10m.MonsterAI.TargetLifecycle.ExtraScenario", EAutomationTestFlags::EngineFilter)') @{
        Controller = 'prepared'; Tests = 'unexpected_test_set'; Disposition = 'manual_review'; FixPresent = $true
    }
    Invoke-SourceCase 'source_renamed_test_does_not_preserve_expected_set' $preparedController ($preparedTests.Replace($names[0], $names[0] + 'Renamed')) @{
        Controller = 'prepared'; Tests = 'unexpected_test_set'; Disposition = 'manual_review'; FixPresent = $true
    }

    $checkerPath = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\CheckMonsterAITargetReadiness.ps1'))
    $fixtureRoot = Join-Path $output 'checker-fixtures'
    $fixtureReferences = Join-Path $fixtureRoot 'references'
    $controllerRelative = 'Source/Rogue10m/Enemy/AI/Rogue10mMonsterAIController.cpp'
    $testsRelative = 'Source/Rogue10m/Tests/Rogue10mMonsterAITargetLifecycleTests.cpp'
    Write-TextFixture $fixtureReferences ('baseline/' + $controllerRelative) $baselineController
    Write-TextFixture $fixtureReferences ('candidate/' + $controllerRelative) $preparedController
    Write-TextFixture $fixtureReferences ('candidate/' + $testsRelative) $preparedTests

    Invoke-ReadinessCase 'checker_baseline_with_missing_tests_reports_needs_apply' {
        $fixtureCurrent = Join-Path $fixtureRoot 'baseline-missing-tests'
        Write-TextFixture $fixtureCurrent $controllerRelative $baselineController
        $fixtureReport = Join-Path $fixtureCurrent 'readiness.json'
        $expectedFailure = $null
        try {
            & $checkerPath -PriorPacketRoot $fixtureReferences -ProjectRoot $fixtureCurrent -OutputPath $fixtureReport | Out-Null
        } catch { $expectedFailure = $_.Exception.Message }
        Assert-True ($null -ne $expectedFailure -and $expectedFailure.StartsWith('Source readiness needs review:')) 'Checker must reject this incomplete source fixture after reporting its state.'
        Assert-True (Test-Path -LiteralPath $fixtureReport -PathType Leaf) 'Checker must persist a JSON report for missing tests.'
        $actual = [IO.File]::ReadAllText($fixtureReport) | ConvertFrom-Json
        Assert-Equal $actual.source.LegacyPatchDisposition 'needs_apply' 'Entry point patch disposition'
        Assert-Equal $actual.source.TestState 'missing' 'Entry point missing-test classification'
        $missingFingerprint = @($actual.fingerprints | Where-Object { $_.path -ceq $testsRelative })
        Assert-Equal $missingFingerprint.Count 1 'Missing tests have one fingerprint record'
        Assert-Equal $missingFingerprint[0].exists $false 'Missing-test fingerprint existence'
        Assert-Equal $missingFingerprint[0].sha256 $null 'Absent tests have no invented hash'
        Assert-True $actual.source_snapshot_stable 'An unchanged absent file must produce a stable snapshot.'
        Assert-True (-not (Test-Path -LiteralPath (Join-Path $fixtureCurrent $testsRelative))) 'Checker must leave the tests absent.'
        Assert-Equal ([IO.File]::ReadAllText((Join-Path $fixtureCurrent $controllerRelative))) $baselineController 'Checker must preserve the fixture controller'
    }
    Invoke-ReadinessCase 'checker_missing_controller_reports_manual_review' {
        $fixtureCurrent = Join-Path $fixtureRoot 'missing-controller-existing-tests'
        Write-TextFixture $fixtureCurrent $testsRelative $preparedTests
        $fixtureReport = Join-Path $fixtureCurrent 'readiness.json'
        $expectedFailure = $null
        try {
            & $checkerPath -PriorPacketRoot $fixtureReferences -ProjectRoot $fixtureCurrent -OutputPath $fixtureReport | Out-Null
        } catch { $expectedFailure = $_.Exception.Message }
        Assert-True ($null -ne $expectedFailure -and $expectedFailure.StartsWith('Source readiness needs review:')) 'Checker must reject the missing controller after reporting its state.'
        Assert-True (Test-Path -LiteralPath $fixtureReport -PathType Leaf) 'Checker must persist a JSON report for a missing controller.'
        $actual = [IO.File]::ReadAllText($fixtureReport) | ConvertFrom-Json
        Assert-Equal $actual.source.LegacyPatchDisposition 'manual_review' 'Entry point patch disposition'
        Assert-Equal $actual.source.ControllerState 'missing' 'Entry point missing-controller classification'
        $missingFingerprint = @($actual.fingerprints | Where-Object { $_.path -ceq $controllerRelative })
        Assert-Equal $missingFingerprint.Count 1 'Missing controller has one fingerprint record'
        Assert-Equal $missingFingerprint[0].exists $false 'Missing-controller fingerprint existence'
        Assert-Equal $missingFingerprint[0].sha256 $null 'Absent controller has no invented hash'
        Assert-True $actual.source_snapshot_stable 'An unchanged absent controller must produce a stable snapshot.'
        Assert-True (-not (Test-Path -LiteralPath (Join-Path $fixtureCurrent $controllerRelative))) 'Checker must leave the controller absent.'
        Assert-Equal ([IO.File]::ReadAllText((Join-Path $fixtureCurrent $testsRelative))) $preparedTests 'Checker must preserve the fixture tests'
    }

    $completionLines = @(foreach ($name in $names) {
        '[2026.11.11-00.00.00:000][  0]LogAutomationController: Display: Test Completed. Result={Success} Name={Scenario} Path={' + $name + '}'
    })
    $queueLine = '[2026.11.11-00.00.01:000][  1]LogAutomationCommandLine: Display: ...Automation Test Queue Empty 5 tests performed.'
    $validLog = (($completionLines + $queueLine) -join "`n")
    Invoke-LogCase 'log_five_successes_queue_and_zero_exit_pass' $validLog $true $true -ExpectedSuccessCount 5
    Invoke-LogCase 'log_completion_order_is_irrelevant' (($completionLines[4..0] + $queueLine) -join "`n") $true $true -ExpectedSuccessCount 5
    Invoke-LogCase 'log_crlf_records_pass' ($validLog.Replace("`n", "`r`n")) $true $true -ExpectedSuccessCount 5
    Invoke-LogCase 'log_complete_without_exit_proof_is_not_process_pass' $validLog $true $false `
        -OmitExitCode -ExpectedExitVerified $false -ExpectedSuccessCount 5
    Invoke-LogCase 'log_explicit_null_exit_is_not_process_pass' $validLog $true $false `
        -ExitCode $null -ExpectedExitVerified $false -ExpectedSuccessCount 5
    Invoke-LogCase 'log_nonzero_exit_rejects_successful_records' $validLog $true $false -ExitCode 7 -ExpectedSuccessCount 5
    Invoke-LogCase 'log_empty_output_rejected_even_with_zero_exit' '' $false $false -ExpectedSuccessCount 0
    Invoke-LogCase 'log_missing_expected_completion_rejected' (($completionLines[0..3] + $queueLine) -join "`n") $false $false
    Invoke-LogCase 'log_duplicate_success_rejected' ($validLog + "`n" + $completionLines[0]) $false $false
    $unexpectedSuccess = 'LogAutomationController: Display: Test Completed. Result={Success} Name={Extra} Path={Rogue10m.MonsterAI.TargetLifecycle.ExtraScenario}'
    Invoke-LogCase 'log_unexpected_suite_success_rejected' ($validLog + "`n" + $unexpectedSuccess) $false $false
    Invoke-LogCase 'log_extra_skipped_suite_test_rejected' ($validLog + "`n" + $unexpectedSuccess.Replace('{Success}', '{Skipped}')) $false $false
    Invoke-LogCase 'log_expected_failed_completion_rejected' ($validLog.Replace($completionLines[0], $completionLines[0].Replace('{Success}', '{Fail}'))) $false $false
    Invoke-LogCase 'log_expected_skipped_completion_rejected' ($validLog.Replace($completionLines[0], $completionLines[0].Replace('{Success}', '{Skipped}'))) $false $false
    Invoke-LogCase 'log_similar_path_cannot_satisfy_expected_identity' ($validLog.Replace($names[0], $names[0] + 'Extra')) $false $false
    Invoke-LogCase 'log_malformed_completion_is_not_success' ($validLog.Replace($completionLines[0], $completionLines[0].TrimEnd('}'))) $false $false
    Invoke-LogCase 'log_global_engine_error_rejects_successful_suite' ($validLog + "`nLogOther: Error: Something failed outside the suite.") $false $false
    Invoke-LogCase 'log_fatal_error_rejects_successful_suite' ($validLog + "`nFatal error: Unexpected shutdown.") $false $false
    Invoke-LogCase 'log_ensure_rejects_successful_suite' ($validLog + "`nLogOutputDevice: Warning: Ensure condition failed: bReady") $false $false
    Invoke-LogCase 'log_assertion_rejects_successful_suite' ($validLog + "`nAssertion failed: bReady") $false $false
    $otherFailure = 'LogAutomationController: Display: Test Completed. Result={Skipped} Name={Other} Path={Rogue10m.OtherSuite.Scenario}'
    Invoke-LogCase 'log_non_success_outside_suite_rejected' ($validLog + "`n" + $otherFailure) $false $false
    Invoke-LogCase 'log_missing_queue_completion_rejected' ($completionLines -join "`n") $false $false
    Invoke-LogCase 'log_wrong_queue_count_rejected' ($validLog.Replace('Empty 5 tests performed.', 'Empty 6 tests performed.')) $false $false
    Invoke-LogCase 'log_duplicate_queue_completion_rejected' ($validLog + "`n" + $queueLine) $false $false
    $started = 'LogAutomationController: Display: Test Started. Name={Scenario} Path={' + $names[0] + '}'
    Invoke-LogCase 'log_started_without_completed_test_rejected' (($started, $queueLine) -join "`n") $false $false
    Invoke-LogCase 'log_empty_queue_is_not_five_test_evidence' ($validLog.Replace('Empty 5 tests performed.', 'Empty 0 tests performed.')) $false $false
    Invoke-LogCase 'log_queue_record_without_display_prefix_accepted' ($validLog.Replace($queueLine,
        'LogAutomationCommandLine: ...Automation Test Queue Empty 5 tests performed.')) $true $true -ExpectedSuccessCount 5
    if ($HistoricalLogPath) {
        if (-not (Test-Path -LiteralPath $HistoricalLogPath -PathType Leaf)) {
            throw ('Requested historical log is missing: ' + $HistoricalLogPath)
        }
        $historicalLogHash = (Get-FileHash -LiteralPath $HistoricalLogPath -Algorithm SHA256).Hash
        $historicalLog = [IO.File]::ReadAllText($HistoricalLogPath).Replace("`r`n", "`n")
        # These cases validate an existing log's format. They deliberately do
        # not supply an exit code or turn historical evidence into a new run.
        Invoke-LogCase 'log_historical_B_lf_format_without_exit_attestation' $historicalLog $true $false `
            -OmitExitCode -ExpectedExitVerified $false -ExpectedSuccessCount 5
        Invoke-LogCase 'log_historical_B_crlf_format_without_exit_attestation' ($historicalLog.Replace("`n", "`r`n")) $true $false `
            -OmitExitCode -ExpectedExitVerified $false -ExpectedSuccessCount 5
    }
    if ($HistoricalFailedLogPath) {
        if (-not (Test-Path -LiteralPath $HistoricalFailedLogPath -PathType Leaf)) {
            throw ('Requested failed historical log is missing: ' + $HistoricalFailedLogPath)
        }
        $historicalFailedLogHash = (Get-FileHash -LiteralPath $HistoricalFailedLogPath -Algorithm SHA256).Hash
        Invoke-LogCase 'log_historical_A_failure_is_not_accepted' ([IO.File]::ReadAllText($HistoricalFailedLogPath)) $false $false `
            -OmitExitCode -ExpectedExitVerified $false
    }
} catch {
    $harnessFailure = $_.Exception.Message
} finally {
    $failed = @($results.ToArray() | Where-Object { -not $_.passed })
    $passed = $null -eq $harnessFailure -and $failed.Count -eq 0 -and $results.Count -ge 20
    [ordered]@{
        status = $(if ($passed) { 'engine_free_tests_passed' } else { 'engine_free_tests_failed' })
        at_utc = [DateTime]::UtcNow.ToString('o')
        total = $results.Count
        passed = $results.Count - $failed.Count
        failed = $failed.Count
        cases = @($results.ToArray())
        harness_failure = $harnessFailure
        module_sha256 = $moduleHash
        historical_log_sha256 = $historicalLogHash
        historical_failed_log_sha256 = $historicalFailedLogHash
        test_script_sha256 = (Get-FileHash -LiteralPath $PSCommandPath -Algorithm SHA256).Hash
        unreal_compile_executed = $false
        unreal_automation_executed = $false
        editor_or_ubt_started = $false
        shared_source_modified = $false
        scope = 'Pure source-state and log-result classification using synthetic fixtures; no build freshness or runtime proof.'
    } | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $output 'verification.json') -Encoding UTF8
}

if (-not $passed) {
    $details = @($failed | ForEach-Object { $_.name + ': ' + $_.failure })
    if ($harnessFailure) { $details += 'Harness: ' + $harnessFailure }
    throw ('ENGINE_FREE_TESTS_FAILED ' + ($details -join ' | '))
}
Write-Output ('ENGINE_FREE_TESTS_PASSED cases=' + $results.Count)
Write-Output ('Evidence: ' + (Join-Path $output 'verification.json'))
