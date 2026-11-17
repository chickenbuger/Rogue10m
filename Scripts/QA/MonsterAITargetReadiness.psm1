Set-StrictMode -Version Latest

$script:Suite = 'Rogue10m.MonsterAI.TargetLifecycle.'
$script:ExpectedTests = @(
    'DestroyedActorClearsState', 'ClearedObjectKeyCancelsWindup', 'LiveTargetRetainsWindup',
    'StoppedAIIgnoresStimulus', 'EmptyTargetPreservesPendingPatrol'
) | ForEach-Object { $script:Suite + $_ }

function ConvertTo-CanonicalText([AllowEmptyString()][string]$Text) {
    return $Text.Replace("`r`n", "`n").Trim()
}

function Get-MonsterAITargetSourceState {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)][AllowEmptyString()][string]$CurrentController,
        [Parameter(Mandatory)][AllowEmptyString()][string]$BaselineController,
        [Parameter(Mandatory)][AllowEmptyString()][string]$PreparedController,
        [Parameter(Mandatory)][AllowEmptyString()][string]$CurrentTests,
        [Parameter(Mandatory)][AllowEmptyString()][string]$PreparedTests
    )
    $current = ConvertTo-CanonicalText $CurrentController
    $baseline = ConvertTo-CanonicalText $BaselineController
    $prepared = ConvertTo-CanonicalText $PreparedController
    $compact = [regex]::Replace($current, '\s+', '')
    $oldGuard = 'if(Target&&!IsValidCombatTarget(Target))'
    $newGuard = 'if((Target||MonsterBlackboardComponent->GetValueAsBool(HasTargetKey))&&!IsValidCombatTarget(Target))'
    $hasOldGuard = $compact.Contains($oldGuard)
    $hasNewGuard = $compact.Contains($newGuard)
    $controllerState = if (-not $current) { 'missing' }
        elseif ($hasOldGuard -and $hasNewGuard) { 'ambiguous' }
        elseif ($current -ceq $baseline) { 'baseline' }
        elseif ($current -ceq $prepared) { 'prepared' }
        elseif ($hasNewGuard) { 'prepared_guard_with_drift' }
        elseif ($hasOldGuard) { 'baseline_guard_with_drift' }
        else { 'unknown' }

    $tests = ConvertTo-CanonicalText $CurrentTests
    $testNames = @([regex]::Matches($tests, '"(Rogue10m\.MonsterAI\.TargetLifecycle\.[A-Za-z0-9_]+)"') |
        ForEach-Object { $_.Groups[1].Value })
    $duplicates = @($testNames | Group-Object | Where-Object Count -gt 1 | ForEach-Object Name)
    $missing = @($script:ExpectedTests | Where-Object { $_ -cnotin $testNames })
    $unexpected = @($testNames | Where-Object { $_ -cnotin $script:ExpectedTests })
    $testState = if (-not $tests) { 'missing' }
        elseif ($testNames.Count -ne 5 -or $duplicates.Count -or $missing.Count -or $unexpected.Count) { 'unexpected_test_set' }
        elseif ($tests -ceq (ConvertTo-CanonicalText $PreparedTests)) { 'prepared' }
        else { 'evolved' }

    $disposition = if ($controllerState -eq 'baseline' -and $testState -eq 'missing') { 'needs_apply' }
        elseif ($controllerState -eq 'prepared' -and $testState -eq 'prepared') { 'already_applied' }
        elseif ($controllerState -in @('missing', 'unknown', 'ambiguous') -or $testState -eq 'unexpected_test_set') { 'manual_review' }
        else { 'diverged' }
    [pscustomobject]@{
        ControllerState = $controllerState
        ControllerFixPresent = $hasNewGuard -and -not $hasOldGuard
        TestState = $testState
        LegacyPatchDisposition = $disposition
        ExpectedTests = @($script:ExpectedTests)
        RegisteredTests = $testNames
        MissingTests = $missing
        UnexpectedTests = $unexpected
        DuplicateTests = $duplicates
        Scope = 'text comparison only; diverged does not authorize replacing current files'
    }
}

function Get-MonsterAITargetAutomationResult {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)][AllowEmptyString()][string]$LogText,
        [Nullable[int]]$ExitCode = $null
    )
    $failures = [Collections.Generic.List[string]]::new()
    $records = @([regex]::Matches($LogText,
        '(?m)^[^\r\n]*Test Completed\.\s+Result=\{(?<result>[^}\r\n]+)\}[^\r\n]*Path=\{(?<path>[^}\r\n]+)\}[^\r\n]*\r?$') |
        ForEach-Object { [pscustomobject]@{ Path = $_.Groups['path'].Value; Result = $_.Groups['result'].Value } })
    $suiteRecords = @($records | Where-Object { $_.Path.StartsWith($script:Suite, [StringComparison]::Ordinal) })
    foreach ($name in $script:ExpectedTests) {
        $matching = @($suiteRecords | Where-Object { $_.Path -ceq $name })
        if ($matching.Count -ne 1) { $failures.Add('expected_exactly_one_completion: ' + $name) }
        elseif ($matching[0].Result -cne 'Success') { $failures.Add('non_success_result: ' + $name + '=' + $matching[0].Result) }
    }
    foreach ($record in $suiteRecords) {
        if ($record.Path -cnotin $script:ExpectedTests) { $failures.Add('unexpected_suite_test: ' + $record.Path) }
    }
    if ($suiteRecords.Count -ne $script:ExpectedTests.Count) { $failures.Add('suite_completion_count_mismatch') }
    if (@($records | Where-Object { $_.Result -cne 'Success' }).Count) { $failures.Add('non_success_completion_in_log') }
    if ($LogText -match '(?i)\bError:|Fatal error:|Ensure condition failed|Assertion failed') { $failures.Add('engine_error_in_log') }
    $queueRecords = @([regex]::Matches($LogText,
        '(?m)^[^\r\n]*LogAutomationCommandLine:[^\r\n]*Automation Test Queue Empty (?<count>\d+) tests performed\.\s*$'))
    if ($queueRecords.Count -ne 1) { $failures.Add('expected_exactly_one_queue_completion') }
    elseif ([int]$queueRecords[0].Groups['count'].Value -ne $script:ExpectedTests.Count) { $failures.Add('queue_test_count_mismatch') }

    $logPassed = $failures.Count -eq 0
    $exitVerified = $null -ne $ExitCode
    if ($exitVerified -and $ExitCode -ne 0) { $failures.Add('nonzero_process_exit: ' + $ExitCode) }
    [pscustomobject]@{
        LogPassed = $logPassed
        Passed = $logPassed -and $exitVerified -and $ExitCode -eq 0
        SuccessCount = @($suiteRecords | Where-Object Result -CEQ 'Success').Count
        Failures = @($failures.ToArray())
        ProcessExitVerified = $exitVerified
        ExitCode = $ExitCode
        QueueCompletionCount = $queueRecords.Count
        Records = $suiteRecords
        Scope = 'evidence parsing only; does not execute Unreal or attest current build/source freshness'
    }
}

Export-ModuleMember -Function Get-MonsterAITargetSourceState, Get-MonsterAITargetAutomationResult
