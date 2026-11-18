#requires -Version 7.2
<#
Owner adapter for internal_smoke.py. Real execution is NOT tested by
the offline packet. Without -Execute this emits only a validated argument plan.
The executing PowerShell process must be the exact process named in the release.
UI7 and visual review remain UNVERIFIED; this artifact is never release eligible.
No process is stopped, no old output is removed, and the lease is retained.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$Seal,
    [Parameter(Mandatory)][ValidatePattern('^[A-Fa-f0-9]{64}$')][string]$SealSha256,
    [Parameter(Mandatory)][string]$Release,
    [Parameter(Mandatory)][ValidatePattern('^[A-Fa-f0-9]{64}$')][string]$ReleaseSha256,
    [Parameter(Mandatory)][ValidateNotNullOrEmpty()][string]$OwnerId,
    [Parameter(Mandatory)][string]$PythonPath,
    [switch]$Execute
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$PSNativeCommandUseErrorActionPreference = $false
$script:ValidationUncertainty = $null
$script:Validator = Join-Path $PSScriptRoot 'internal_smoke.py'
$script:Helper = Join-Path $PSScriptRoot 'strict_primitives.py'
$script:AcceptanceFlags = [ordered]@{UI7='UNVERIFIED';visual_review='UNVERIFIED';release_eligible=$false}
$script:CurrentShell = [Environment]::ProcessPath
$script:Journal = $null
$script:JournalPath = $null
$script:LeaseStream = $null
$script:Phase = 'bootstrap_validation'
$script:BootstrapDiagnostics = $null
$script:EnvironmentChanged = $false
$script:UatAttempted = $false
$script:PriorEnvironment = @{}
$script:EnvironmentNames = @('uebp_LogFolder','uebp_FinalLogFolder','uebp_EngineSavedFolder')

function Get-AbsolutePhysicalPath([string]$Path) {
    if ([string]::IsNullOrWhiteSpace($Path) -or -not [IO.Path]::IsPathFullyQualified($Path)) {
        throw 'An absolute physical path is required.'
    }
    if ($Path.IndexOfAny([char[]]@('"', "`r", "`n", [char]0)) -ge 0) {
        throw 'Quote/control character in path refused.'
    }
    $absolute = [IO.Path]::GetFullPath($Path)
    $itemPath = $absolute
    while ($itemPath) {
        if (Test-Path -LiteralPath $itemPath) {
            $item = Get-Item -LiteralPath $itemPath -Force
            if ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) {
                throw ('Reparse path refused: '+$itemPath)
            }
        }
        $parent = Split-Path -Parent $itemPath
        if ($parent -eq $itemPath) { break }
        $itemPath = $parent
    }
    return $absolute
}

function Test-SamePath([string]$Left,[string]$Right) {
    return [IO.Path]::GetFullPath($Left).TrimEnd('\','/').Equals(
        [IO.Path]::GetFullPath($Right).TrimEnd('\','/'),[StringComparison]::OrdinalIgnoreCase)
}

function Assert-FilePin($Pin) {
    if ($Pin -isnot [Collections.IDictionary] -or -not $Pin.Contains('path') -or -not $Pin.Contains('sha256') -or
        [string]$Pin.sha256 -notmatch '^[A-Fa-f0-9]{64}$') {
        throw 'Invalid file pin.'
    }
    $path = Get-AbsolutePhysicalPath ([string]$Pin.path)
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw ('Pinned file missing: '+$path) }
    $actual = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash
    if (-not $actual.Equals([string]$Pin.sha256,[StringComparison]::OrdinalIgnoreCase)) {
        throw ('Pinned file changed: '+$path)
    }
    return $path
}

function Read-PinnedJson([string]$Path,[string]$Hash) {
    $path = Assert-FilePin @{path=$Path;sha256=$Hash}
    # Hash the same bytes that are parsed; a replacement between two file opens
    # must not turn a checked JSON file into different unchecked instructions.
    $bytes = [IO.File]::ReadAllBytes($path)
    $actual = [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($bytes))
    if (-not $actual.Equals($Hash,[StringComparison]::OrdinalIgnoreCase)) {
        throw ('JSON changed while reading: '+$path)
    }
    $text = [Text.UTF8Encoding]::new($false,$true).GetString($bytes).TrimStart([char]0xfeff)
    return ConvertFrom-Json -InputObject $text -AsHashtable
}

function Assert-PinPresent($Pins,[string]$ExpectedPath) {
    $found = @($Pins | Where-Object { Test-SamePath ([string]$_.path) $ExpectedPath })
    if ($found.Count -ne 1) { throw ('Exactly one tool pin required: '+$ExpectedPath) }
    $null = Assert-FilePin $found[0]
}

function Assert-ToolPins($Pins) {
    $seen = @{}
    foreach ($pin in @($Pins)) {
        $path = Assert-FilePin $pin
        if ($seen.ContainsKey($path)) { throw ('Duplicate tool identity: '+$path) }
        $seen[$path] = $true
    }
    foreach ($required in @($script:CurrentShell,$PythonPath,$script:Validator,$script:Helper,$PSCommandPath)) {
        Assert-PinPresent $Pins $required
    }
}

function Assert-Bootstrap {
    if (-not $IsWindows) { throw 'Windows PowerShell 7 is required.' }
    $script:SealPath = Get-AbsolutePhysicalPath $Seal
    $script:ReleasePath = Get-AbsolutePhysicalPath $Release
    $script:Python = Get-AbsolutePhysicalPath $PythonPath
    $script:CurrentShell = Get-AbsolutePhysicalPath $script:CurrentShell
    $script:Validator = Get-AbsolutePhysicalPath $script:Validator
    $script:SealData = Read-PinnedJson $script:SealPath $SealSha256
    if ($script:SealData.status -cne 'sealed') { throw 'UNSEALED: final internal smoke identities are absent; no process or output will be created.' }
    if ($script:SealData.schema -cne 'R10-internal-smoke-seal-v1') { throw 'An exact internal smoke seal is required; strict packaging seals are refused.' }
    $script:ReleaseData = Read-PinnedJson $script:ReleasePath $ReleaseSha256
    if (-not (Test-SamePath (Assert-FilePin $script:SealData.runtime.python) $script:Python)) {
        throw 'PythonPath differs from the sealed Python executable.'
    }
    if (-not (Test-SamePath (Assert-FilePin $script:SealData.runtime.powershell) $script:CurrentShell)) {
        throw 'The current PowerShell executable differs from the sealed runtime.'
    }
    if (-not (Test-SamePath (Assert-FilePin $script:SealData.runner) $PSCommandPath)) {
        throw 'The invoked adapter differs from seal.runner.'
    }
    foreach ($entry in @(@('validator',$script:Validator),@('helper',$script:Helper))) {
        if (-not (Test-SamePath (Assert-FilePin $script:SealData[$entry[0]]) $entry[1])) {
            throw ('Wrong local bootstrap pin: '+$entry[0])
        }
    }
    # Pin the validator before starting it, rather than trusting a validator to
    # report its own unchanged identity after its code has already executed.
    Assert-ToolPins $script:SealData.tool_pins
}

function Invoke-Validator([ValidateSet('plan','check-immutable','postflight')][string]$Mode,[string]$Evidence = '') {
    # Re-observe all bootstrap pins before every validation subprocess.
    Assert-Bootstrap
    $startInfo = [Diagnostics.ProcessStartInfo]::new()
    $startInfo.FileName = $script:Python
    $startInfo.UseShellExecute = $false
    $startInfo.CreateNoWindow = $true
    $startInfo.RedirectStandardOutput = $true
    $startInfo.RedirectStandardError = $true
    $startInfo.StandardOutputEncoding = [Text.UTF8Encoding]::new($false,$true)
    $startInfo.StandardErrorEncoding = [Text.UTF8Encoding]::new($false,$true)
    $startInfo.WorkingDirectory = $PSScriptRoot
    $arguments = @('-I','-B','-X','utf8',$script:Validator,'--seal',$script:SealPath,'--seal-sha256',$SealSha256,
        '--release',$script:ReleasePath,'--release-sha256',$ReleaseSha256,'--owner-id',$OwnerId)
    if ($Mode -eq 'postflight') { $arguments += @('--postflight',$Evidence) }
    else { $arguments += ('--'+$Mode) }
    foreach ($argument in $arguments) { $startInfo.ArgumentList.Add([string]$argument) }
    $process = [Diagnostics.Process]::new()
    $process.StartInfo = $startInfo
    $started = $false
    $completed = $false
    try {
        if (-not $process.Start()) { throw 'Python validator did not start.' }
        $started = $true
        $stdoutTask = $process.StandardOutput.ReadToEndAsync()
        $stderrTask = $process.StandardError.ReadToEndAsync()
        $process.WaitForExit()
        $completed = $true
        $stdout = $stdoutTask.GetAwaiter().GetResult()
        $stderr = $stderrTask.GetAwaiter().GetResult()
        $code = $process.ExitCode
        $data = $null
        try { $data = ConvertFrom-Json -InputObject $stdout -AsHashtable }
        catch { throw ('Validator output is not one JSON document. stderr='+$stderr+' stdout='+$stdout) }
        if ($code -ne 0 -or -not [string]::IsNullOrWhiteSpace($stderr)) {
            $script:BootstrapDiagnostics = $data
            throw ('Candidate validation blocked; exit='+$code+' stderr='+$stderr)
        }
        return $data
    } finally {
        if ($started -and -not $completed) {
            $script:ValidationUncertainty = [ordered]@{
                pid=$process.Id;executable=$script:Python;mode=$Mode
                status='validator_wait_interrupted_no_signal_sent_outcome_requires_observation'
            }
        }
        # Dispose releases this retained handle; it never terminates a process.
        $process.Dispose()
    }
}

function Assert-SmokeAcceptance($Data) {
    if ($Data.scope -cne 'internal_development_smoke' -or
        $Data.release_eligible -isnot [bool] -or $Data.release_eligible -ne $false) {
        throw 'Internal smoke scope and permanent non-release eligibility are required.'
    }
    $flags = $Data.acceptance_flags
    if ($flags -isnot [Collections.IDictionary] -or $flags.Count -ne 3 -or
        $flags.UI7 -cne 'UNVERIFIED' -or $flags.visual_review -cne 'UNVERIFIED' -or
        $flags.release_eligible -isnot [bool] -or $flags.release_eligible -ne $false) {
        throw 'UI7 and visual review must remain UNVERIFIED with release_eligible=false.'
    }
    if ($Data.not_executed_by_this_validator -isnot [bool] -or
        $Data.not_executed_by_this_validator -ne $true) {
        throw 'The Python validator must never claim to execute UE or packaging.'
    }
}

function Assert-Plan($Plan) {
    if ($Plan.schema -cne 'R10-internal-smoke-plan-v1' -or $Plan.status -cne 'ready_arguments_only') {
        throw 'Validator did not return an internal smoke argument plan.'
    }
    Assert-SmokeAcceptance $Plan
    foreach ($pair in @(@('seal_sha256',$SealSha256),@('release_sha256',$ReleaseSha256))) {
        if (-not ([string]$Plan[$pair[0]]).Equals($pair[1],[StringComparison]::OrdinalIgnoreCase)) {
            throw ('Plan has a different '+$pair[0])
        }
    }
    if ([string]$Plan.plan_sha256 -notmatch '^[A-Fa-f0-9]{64}$') { throw 'Plan digest missing.' }
    if (-not (Test-SamePath (Assert-FilePin $Plan.runtime.python) $script:Python)) {
        throw 'Plan Python runtime differs from the bootstrap pin.'
    }
    if (-not (Test-SamePath (Assert-FilePin $Plan.runtime.powershell) $script:CurrentShell)) {
        throw 'Plan PowerShell runtime differs from the current executable.'
    }
    Assert-ToolPins $Plan.tool_pins
    $root = Get-AbsolutePhysicalPath ([string]$Plan.output_root)
    if (-not (Test-Path -LiteralPath $root -PathType Container)) { throw 'Approved output_root must already exist.' }
    $expected = @('evidence','archive','cook','stage','cook_user','qa_user')
    if ($Plan.outputs -isnot [Collections.IDictionary] -or $Plan.outputs.Count -ne $expected.Count) {
        throw 'Plan must contain exactly six output directories.'
    }
    $seen = @{}
    $projectRoot = Split-Path -Parent (Get-AbsolutePhysicalPath ([string]$Plan.project))
    $engine = Get-AbsolutePhysicalPath ([string]$Plan.engine)
    foreach ($name in $expected) {
        if (-not $Plan.outputs.Contains($name)) { throw ('Plan output missing: '+$name) }
        $path = Get-AbsolutePhysicalPath ([string]$Plan.outputs[$name])
        if (-not (Test-SamePath (Split-Path -Parent $path) $root) -or (Test-SamePath $path $projectRoot) -or
            (Test-SamePath $path $engine)) { throw ('Output is not a dedicated approved sibling: '+$path) }
        if ($seen.ContainsKey($path)) { throw 'Overlapping output identities.' }
        $seen[$path] = $true
    }
    $lease = Get-AbsolutePhysicalPath ([string]$Plan.lease)
    if (-not (Test-SamePath (Split-Path -Parent $lease) $root) -or $seen.ContainsKey($lease)) {
        throw 'Lease must be a separate direct child of approved output_root.'
    }
    $uat = Get-AbsolutePhysicalPath ([string]$Plan.uat)
    if (-not (Test-SamePath $uat (Join-Path $engine 'Engine\Build\BatchFiles\RunUAT.bat'))) {
        throw 'Plan UAT path is not the selected engine RunUAT.bat.'
    }
    Assert-PinPresent $Plan.tool_pins $uat
    foreach ($phaseArgs in @($Plan.sdk_args,$Plan.bcr_args)) {
        if ($phaseArgs -is [string] -or @($phaseArgs).Count -eq 0) { throw 'Structured phase argument array required.' }
        foreach ($argument in $phaseArgs) {
            if ($argument -isnot [string] -or $argument.IndexOfAny([char[]]@("`r","`n",[char]0)) -ge 0) {
                throw 'Invalid phase argument.'
            }
        }
    }
}

function Get-OwnerIdentity {
    $self = [Diagnostics.Process]::GetCurrentProcess()
    try {
        $path = Get-AbsolutePhysicalPath $self.MainModule.FileName
        if (-not (Test-SamePath $path $script:CurrentShell)) { throw 'Invoking PowerShell identity changed.' }
        return [ordered]@{
            pid=[int]$PID;start_utc_ticks=$self.StartTime.ToUniversalTime().Ticks
            executable=$path;sha256=(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash
        }
    } finally { $self.Dispose() }
}

function Assert-OwnerRelease($Identity) {
    $releaseData = $script:ReleaseData
    if ($releaseData.schema -cne 'R10-internal-smoke-release-v1' -or
        $releaseData.status -cne 'released_for_internal_smoke' -or
        $releaseData.owner_id -cne $OwnerId -or
        $releaseData.project -cne $script:SealData.project -or
        -not ([string]$releaseData.seal_sha256).Equals($SealSha256,[StringComparison]::OrdinalIgnoreCase)) {
        throw 'Release schema/status/seal/owner mismatch.'
    }
    $wanted = $releaseData.owner_process
    if ($wanted -isnot [Collections.IDictionary] -or [long]$wanted.pid -ne [long]$Identity.pid -or
        [long]$wanted.start_utc_ticks -ne [long]$Identity.start_utc_ticks -or
        -not (Test-SamePath ([string]$wanted.executable) ([string]$Identity.executable)) -or
        -not ([string]$wanted.sha256).Equals([string]$Identity.sha256,[StringComparison]::OrdinalIgnoreCase)) {
        throw 'Release does not name this exact invoking PowerShell PID/start/executable/hash.'
    }
    $now = [DateTimeOffset]::UtcNow
    $issued = [DateTimeOffset]::Parse([string]$releaseData.released_utc,[Globalization.CultureInfo]::InvariantCulture)
    $expires = [DateTimeOffset]::Parse([string]$releaseData.expires_utc,[Globalization.CultureInfo]::InvariantCulture)
    if ($issued -gt $now -or $expires -le $now -or $expires -le $issued -or
        @($releaseData.scopes).Count -ne 1 -or @($releaseData.scopes)[0] -cne 'Win64 Development internal smoke') {
        throw 'Execution release is expired, premature, or has the wrong scope.'
    }
}

function Write-Journal {
    if (-not $script:JournalPath -or -not $script:Journal) { return }
    $null = Get-AbsolutePhysicalPath $script:JournalPath
    $script:Journal.phase = $script:Phase
    $script:Journal.validation_uncertainty = $script:ValidationUncertainty
    $script:Journal.updated_utc = [DateTime]::UtcNow.ToString('o')
    [IO.File]::WriteAllText($script:JournalPath,($script:Journal | ConvertTo-Json -Depth 15),[Text.UTF8Encoding]::new($false))
}

function Check-Immutable($ReferencePlan,$Identity) {
    $current = Invoke-Validator 'check-immutable'
    Assert-Plan $current
    if (-not ([string]$current.plan_sha256).Equals([string]$ReferencePlan.plan_sha256,[StringComparison]::OrdinalIgnoreCase)) {
        throw 'Candidate argument plan changed between phases.'
    }
    Assert-OwnerRelease $Identity
}

function Set-PhaseEnvironment([string]$PhaseName,[string]$Evidence) {
    if (-not $script:EnvironmentChanged) {
        foreach ($name in $script:EnvironmentNames) {
            $script:PriorEnvironment[$name] = [Environment]::GetEnvironmentVariable($name,'Process')
        }
        $script:EnvironmentChanged = $true
    }
    $logs = Join-Path $Evidence ('uat-'+$PhaseName+'-logs')
    $saved = Join-Path $Evidence ('uat-'+$PhaseName+'-saved')
    $null = Get-AbsolutePhysicalPath $logs
    $null = Get-AbsolutePhysicalPath $saved
    [Environment]::SetEnvironmentVariable('uebp_LogFolder',$logs,'Process')
    [Environment]::SetEnvironmentVariable('uebp_FinalLogFolder',$logs,'Process')
    [Environment]::SetEnvironmentVariable('uebp_EngineSavedFolder',$saved,'Process')
}

try {
    $plan = Invoke-Validator 'plan'
    Assert-Plan $plan
    if (-not $Execute) {
        $plan | ConvertTo-Json -Depth 30
        exit 0
    }
    $script:Phase = 'owner_release_validation'
    $identity = Get-OwnerIdentity
    Assert-OwnerRelease $identity
    # Still no output or lease writes. This second strict plan checks freshness
    # immediately before claiming this reviewed run's new directories.
    $second = Invoke-Validator 'plan'
    Assert-Plan $second
    if (-not ([string]$second.plan_sha256).Equals([string]$plan.plan_sha256,[StringComparison]::OrdinalIgnoreCase)) {
        throw 'Plan changed before lease acquisition.'
    }
    Assert-OwnerRelease $identity
    $script:Phase = 'exclusive_lease'
    $leasePath = Get-AbsolutePhysicalPath ([string]$plan.lease)
    $script:LeaseStream = [IO.FileStream]::new($leasePath,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::Read)
    $leaseData = [ordered]@{
        schema='R10-internal-smoke-lease-v1';run_tag=$plan.run_tag;owner_id=$OwnerId;owner_process=$identity
        scope='internal_development_smoke';acceptance_flags=$script:AcceptanceFlags;release_eligible=$false
        not_executed_by_this_validator=$true
        seal_sha256=$SealSha256.ToUpperInvariant();release_sha256=$ReleaseSha256.ToUpperInvariant()
        plan_sha256=$plan.plan_sha256;created_utc=[DateTime]::UtcNow.ToString('o')
        lease_boundary='This exact internal smoke run only; native UAT/UBT mutexes and external owner coordination remain required.'
        child_process_pids_observed=$false;process_termination_supported=$false;lease_retained=$true
    }
    $leaseBytes = [Text.UTF8Encoding]::new($false).GetBytes(($leaseData | ConvertTo-Json -Depth 10))
    $script:LeaseStream.Write($leaseBytes,0,$leaseBytes.Length)
    $script:LeaseStream.Flush($true)
    $script:Phase = 'fresh_output_directories'
    foreach ($name in @('evidence','archive','cook','stage','cook_user','qa_user')) {
        $path = Get-AbsolutePhysicalPath ([string]$plan.outputs[$name])
        if (Test-Path -LiteralPath $path) { throw ('Output appeared after the fresh plan; preserved: '+$path) }
        New-Item -ItemType Directory -Path $path -ErrorAction Stop | Out-Null
        $null = Get-AbsolutePhysicalPath $path
        if ($name -eq 'evidence') {
            $script:JournalPath = Join-Path $path 'wrapper-journal.json'
            $script:Journal = [ordered]@{
                schema='R10-internal-smoke-wrapper-v1';status='running';phase=$script:Phase
                scope='internal_development_smoke';acceptance_flags=$script:AcceptanceFlags;release_eligible=$false
                not_executed_by_this_validator=$true
                owner_id=$OwnerId;owner_process=$identity;run_tag=$plan.run_tag;project=$plan.project;engine=$plan.engine
                seal_sha256=$SealSha256.ToUpperInvariant();release_sha256=$ReleaseSha256.ToUpperInvariant()
                plan_sha256=$plan.plan_sha256;lease=$leasePath;outputs=$plan.outputs
                sdk_exit=$null;buildcookrun_exit=$null;buildcookrun_started_utc=$null;postflight=$null;failure=$null;validation_uncertainty=$null
                child_process_pids_observed=$false;no_process_termination=$true;outputs_and_lease_retained=$true
                actual_packaging_untested_by_offline_packet=$true;started_utc=[DateTime]::UtcNow.ToString('o')
            }
            Write-Journal
        }
    }
    $evidence = [string]$plan.outputs.evidence
    $script:Phase = 'pre_sdk_immutable_validation'
    Write-Journal
    Check-Immutable $plan $identity
    Set-PhaseEnvironment 'sdk' $evidence
    $script:Phase = 'verify_installed_sdk'
    Write-Journal
    $sdkArguments = [string[]]@($plan.sdk_args)
    $script:UatAttempted = $true
    & ([string]$plan.uat) @sdkArguments 2>&1 | Tee-Object -FilePath (Join-Path $evidence 'sdk-verify.log')
    $script:Journal.sdk_exit = $LASTEXITCODE
    Write-Journal
    if ($script:Journal.sdk_exit -ne 0) { throw ('Verify-only installed SDK failed; exit='+$script:Journal.sdk_exit) }
    $script:Phase = 'pre_build_immutable_validation'
    Write-Journal
    Check-Immutable $plan $identity
    Set-PhaseEnvironment 'bcr' $evidence
    $script:Phase = 'build_cook_stage_archive'
    $script:Journal.buildcookrun_started_utc = [DateTime]::UtcNow.ToString('o')
    Write-Journal
    $buildArguments = [string[]]@($plan.bcr_args)
    & ([string]$plan.uat) @buildArguments 2>&1 | Tee-Object -FilePath (Join-Path $evidence 'buildcookrun.log')
    $script:Journal.buildcookrun_exit = $LASTEXITCODE
    Write-Journal
    if ($script:Journal.buildcookrun_exit -ne 0) { throw ('BuildCookRun failed; exit='+$script:Journal.buildcookrun_exit) }
    $script:Phase = 'postflight_validation'
    Write-Journal
    $postflight = Invoke-Validator 'postflight' $evidence
    if ($postflight.schema -cne 'R10-internal-smoke-result-v1' -or
        $postflight.status -cne 'internal_smoke_artifact_built_UNVERIFIED') {
        throw 'Postflight did not prove an internal smoke artifact; UI7 and visual review remain UNVERIFIED.'
    }
    Assert-SmokeAcceptance $postflight
    $postflightPath = Join-Path $evidence 'postflight.json'
    $null = Get-AbsolutePhysicalPath $postflightPath
    $postflightStream = [IO.FileStream]::new($postflightPath,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::Read)
    try {
        $bytes = [Text.UTF8Encoding]::new($false).GetBytes(($postflight | ConvertTo-Json -Depth 30))
        $postflightStream.Write($bytes,0,$bytes.Length)
        $postflightStream.Flush($true)
    } finally { $postflightStream.Dispose() }
    $script:Journal.postflight = $postflightPath
    $script:Journal.status = 'internal_smoke_artifact_built_UNVERIFIED'
    $script:Phase = 'complete_internal_smoke_artifact_UNVERIFIED'
    Write-Journal
    $postflight | ConvertTo-Json -Depth 30
} catch {
    if ($script:Journal) {
        $script:Journal.status = 'failed'
        $script:Journal.failure = $_.Exception.Message
        try { Write-Journal } catch { Write-Warning ('Could not update owned journal: '+$_.Exception.Message) }
    }
    [ordered]@{
        schema='R10-internal-smoke-result-v1';scope='internal_development_smoke'
        acceptance_flags=$script:AcceptanceFlags;release_eligible=$false;not_executed_by_this_validator=$true
        status=$(if($Execute){'failed'}else{'blocked'});executed=$script:UatAttempted
        phase=$script:Phase;error=$_.Exception.Message;execute_requested=[bool]$Execute
        journal=$script:JournalPath;validation_diagnostics=$script:BootstrapDiagnostics
        validation_uncertainty=$script:ValidationUncertainty
        adapter_signaled_no_process=$true;outputs_and_lease_are_retained=$true
    } | ConvertTo-Json -Depth 20
    exit 1
} finally {
    if ($script:Journal -and $script:Journal.status -eq 'running') {
        $script:Journal.status = 'failed_or_interrupted_outcome_uncertain'
        $script:Journal.failure = 'Adapter execution did not reach a completed phase; observe retained processes/outputs before any retry.'
        try { Write-Journal } catch { Write-Warning ('Could not record interrupted outcome: '+$_.Exception.Message) }
    }
    if ($script:EnvironmentChanged) {
        foreach ($name in $script:EnvironmentNames) {
            [Environment]::SetEnvironmentVariable($name,$script:PriorEnvironment[$name],'Process')
        }
    }
    if ($script:LeaseStream) { $script:LeaseStream.Dispose() }
}
