#requires -Version 7.5
<# Pure PowerShell parser/owner-window counterexamples. No wrapper body, UAT or UE. #>
[CmdletBinding()]
param([Parameter(Mandatory)][string]$EvidenceRoot)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$stage = [IO.Path]::GetFullPath($PSScriptRoot)
if (-not [IO.Path]::IsPathFullyQualified($EvidenceRoot) -or $EvidenceRoot.Split([char[]]@('\','/')) -contains '..') {
    throw 'Canonical absolute evidence path required'
}
$out = [IO.Path]::GetFullPath($EvidenceRoot)
$allowed = Join-Path $stage 'evidence'
if (-not $out.StartsWith($allowed+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase) -or (Test-Path -LiteralPath $out)) {
    throw 'Fresh child of staging/evidence required'
}
$ancestor = Split-Path -Parent $out
while ($ancestor) {
    if ((Test-Path -LiteralPath $ancestor) -and ((Get-Item -LiteralPath $ancestor -Force).Attributes -band [IO.FileAttributes]::ReparsePoint)) {
        throw 'Reparse evidence ancestor refused'
    }
    $next = Split-Path -Parent $ancestor
    if ($next -eq $ancestor) { break }
    $ancestor = $next
}
New-Item -ItemType Directory -Path $out -ErrorAction Stop | Out-Null
$wrapper = Join-Path $stage 'Invoke-InternalSmoke.ps1'
$wrapperBytes = [IO.File]::ReadAllBytes($wrapper)
$wrapperSha = [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($wrapperBytes))
if ($wrapperSha -cne '32A95E82FB7C68485FD90086D0F185E583BAC86A9B28287B2E69F1367C4CFBD9') { throw 'Frozen UTC wrapper changed' }
$text = [Text.UTF8Encoding]::new($false,$true).GetString($wrapperBytes)
$tokens=$null; $parseErrors=$null
$ast = [Management.Automation.Language.Parser]::ParseInput($text,[ref]$tokens,[ref]$parseErrors)
if (@($parseErrors).Count) { throw 'Wrapper parse error' }
$functionNames=@('Get-AbsolutePhysicalPath','Test-SamePath','Assert-FilePin','Read-PinnedJson','Assert-OwnerRelease')
foreach ($name in $functionNames) {
    $found=@($ast.FindAll({param($node) $node -is [Management.Automation.Language.FunctionDefinitionAst]},$true) | Where-Object Name -CEQ $name)
    if ($found.Count -ne 1) { throw ('One exact function required: '+$name) }
    . ([scriptblock]::Create($found[0].Extent.Text))
}
$checks=[Collections.Generic.List[string]]::new()
$rejected=[ordered]@{}
function Check([bool]$Condition,[string]$Name) {
    if (-not $Condition) { throw ('FAILED '+$Name) }
    $checks.Add($Name)
}
function Write-JsonNew([string]$Path,$Value) {
    $bytes=[Text.UTF8Encoding]::new($false).GetBytes(($Value|ConvertTo-Json -Depth 20))
    $stream=[IO.FileStream]::new($Path,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::Read)
    try { $stream.Write($bytes,0,$bytes.Length) } finally { $stream.Dispose() }
}
function Copy-Release($Value) { return ($Value|ConvertTo-Json -Depth 20|ConvertFrom-Json -AsHashtable -DateKind String) }
$process=[Diagnostics.Process]::GetCurrentProcess()
try {
    $identity=[ordered]@{pid=[int]$PID;start_utc_ticks=$process.StartTime.ToUniversalTime().Ticks;executable=$process.MainModule.FileName;sha256=(Get-FileHash -LiteralPath $process.MainModule.FileName -Algorithm SHA256).Hash}
} finally { $process.Dispose() }
$OwnerId='SYNTHETIC-UTC-TEST-ONLY'
$SealSha256='A'*64
$script:SealData=@{project=(Join-Path $out 'SYNTHETIC-Rogue10m.uproject')}
$now=[DateTime]::UtcNow
$release=[ordered]@{
    schema='R10-internal-smoke-release-v1';status='released_for_internal_smoke'
    owner_id=$OwnerId;project=$script:SealData.project;seal_sha256=$SealSha256
    owner_process=$identity;scopes=@('Win64 Development internal smoke')
    released_utc=$now.AddMinutes(-10).ToString('o',[Globalization.CultureInfo]::InvariantCulture)
    expires_utc=$now.AddMinutes(10).ToString('o',[Globalization.CultureInfo]::InvariantCulture)
    synthetic_only=$true;note='Synthetic pure-function fixture, never an actual smoke release'
}
$releasePath=Join-Path $out 'SYNTHETIC-release.json'
Write-JsonNew $releasePath $release
$releaseHash=(Get-FileHash -LiteralPath $releasePath -Algorithm SHA256).Hash
$parsed=Read-PinnedJson $releasePath $releaseHash
Check ($parsed.released_utc -is [string] -and $parsed.expires_utc -is [string]) 'actual_JSON_UTC_fields_remain_strings'
Check ($parsed.released_utc -ceq $release.released_utc -and $parsed.expires_utc -ceq $release.expires_utc) 'actual_UTC_Z_and_fractional_bytes_preserved'
$parsedIssued=[DateTimeOffset]::Parse([string]$parsed.released_utc,[Globalization.CultureInfo]::InvariantCulture)
Check ($parsedIssued.Offset -eq [TimeSpan]::Zero) 'actual_UTC_offset_zero'
Check ($parsedIssued.UtcDateTime.Ticks -eq $now.AddMinutes(-10).Ticks) 'actual_UTC_ticks_unchanged'
$script:ReleaseData=$parsed
Assert-OwnerRelease $identity
Check $true 'active_window_and_exact_actual_process_identity_accepted'

$fixed=[ordered]@{released_utc='2026-11-13T13:55:30.1234567Z';expires_utc='2026-11-13T17:55:30.7654321+00:00'}
$fixedPath=Join-Path $out 'SYNTHETIC-fixed-fractional-UTC.json'
Write-JsonNew $fixedPath $fixed
$fixedHash=(Get-FileHash -LiteralPath $fixedPath -Algorithm SHA256).Hash
$beforeCulture=[Threading.Thread]::CurrentThread.CurrentCulture
$beforeUiCulture=[Threading.Thread]::CurrentThread.CurrentUICulture
try {
    foreach ($culture in @('en-US','ko-KR','de-DE')) {
        [Threading.Thread]::CurrentThread.CurrentCulture=[Globalization.CultureInfo]::GetCultureInfo($culture)
        [Threading.Thread]::CurrentThread.CurrentUICulture=[Globalization.CultureInfo]::GetCultureInfo($culture)
        $value=Read-PinnedJson $fixedPath $fixedHash
        Check ($value.released_utc -is [string] -and $value.expires_utc -is [string]) ($culture+'_JSON_types_are_strings')
        Check ($value.released_utc -ceq $fixed.released_utc -and $value.expires_utc -ceq $fixed.expires_utc) ($culture+'_fractional_Z_and_explicit_zero_offset_preserved')
        $time=[DateTimeOffset]::Parse([string]$value.released_utc,[Globalization.CultureInfo]::InvariantCulture)
        Check ($time.Offset -eq [TimeSpan]::Zero -and $time.UtcDateTime.Ticks -eq [DateTimeOffset]::Parse($fixed.released_utc,[Globalization.CultureInfo]::InvariantCulture).UtcDateTime.Ticks) ($culture+'_parsed_ticks_and_zero_offset_preserved')
        $script:ReleaseData=Read-PinnedJson $releasePath $releaseHash
        Assert-OwnerRelease $identity
        Check $true ($culture+'_actual_active_release_accepted')
    }
} finally {
    [Threading.Thread]::CurrentThread.CurrentCulture=$beforeCulture
    [Threading.Thread]::CurrentThread.CurrentUICulture=$beforeUiCulture
}

function Expect-Rejected([string]$Name,[scriptblock]$Mutate) {
    $value=Copy-Release $release
    & $Mutate $value
    $script:ReleaseData=$value
    $blocked=$false
    try { Assert-OwnerRelease $identity } catch { $blocked=$true; $rejected[$Name]=$_.Exception.Message }
    Check $blocked $Name
}
Expect-Rejected 'expired_window_still_rejected' {param($v) $v.released_utc=$now.AddMinutes(-20).ToString('o');$v.expires_utc=$now.AddMinutes(-10).ToString('o')}
Expect-Rejected 'future_issue_still_rejected' {param($v) $v.released_utc=$now.AddMinutes(5).ToString('o');$v.expires_utc=$now.AddMinutes(10).ToString('o')}
Expect-Rejected 'reversed_window_still_rejected' {param($v) $v.released_utc=$now.AddMinutes(-1).ToString('o');$v.expires_utc=$now.AddMinutes(-2).ToString('o')}
Expect-Rejected 'wrong_scope_still_rejected' {param($v) $v.scopes=@('release')}
Expect-Rejected 'extra_scope_still_rejected' {param($v) $v.scopes=@('Win64 Development internal smoke','release')}
Expect-Rejected 'wrong_PID_still_rejected' {param($v) $v.owner_process.pid=[int]$identity.pid+100000}
Expect-Rejected 'wrong_start_ticks_still_rejected' {param($v) $v.owner_process.start_utc_ticks=[long]$identity.start_utc_ticks+1}
Expect-Rejected 'wrong_executable_still_rejected' {param($v) $v.owner_process.executable=Join-Path $out 'SYNTHETIC-other.exe'}
Expect-Rejected 'wrong_executable_SHA_still_rejected' {param($v) $v.owner_process.sha256='B'*64}
Expect-Rejected 'wrong_owner_still_rejected' {param($v) $v.owner_id='SYNTHETIC-other'}
Expect-Rejected 'wrong_seal_still_rejected' {param($v) $v.seal_sha256='B'*64}
Expect-Rejected 'wrong_project_still_rejected' {param($v) $v.project=Join-Path $out 'SYNTHETIC-other.uproject'}
Expect-Rejected 'wrong_schema_still_rejected' {param($v) $v.schema='wrong'}
Expect-Rejected 'unreleased_status_still_rejected' {param($v) $v.status='UNSEALED'}

$legacy=ConvertFrom-Json -InputObject ([IO.File]::ReadAllText($releasePath)) -AsHashtable
$legacyString=[string]$legacy.released_utc
$legacyParsed=[DateTimeOffset]::Parse($legacyString,[Globalization.CultureInfo]::InvariantCulture)
$legacyRejected=$false
$script:ReleaseData=$legacy
try { Assert-OwnerRelease $identity } catch { $legacyRejected=$true }
Check ($legacy.released_utc -is [DateTime]) 'legacy_JSON_automatic_DateTime_conversion_reproduced'
Check ($legacyString -cne $release.released_utc) 'legacy_string_conversion_loses_exact_UTC_representation'
if ([TimeZoneInfo]::Local.GetUtcOffset($now) -ne [TimeSpan]::Zero) {
    Check $legacyRejected 'legacy_active_window_false_rejection_reproduced_in_nonzero_local_timezone'
}
$report=[ordered]@{
    schema='R10-culture-utc-d-pure-powershell-validation-v1';passed=$true;checks=$checks.Count
    wrapper_sha256=$wrapperSha;wrapper_body_executed=$false;extracted_functions=$functionNames
    powershell=@{path=[Environment]::ProcessPath;version=$PSVersionTable.PSVersion.ToString();sha256=(Get-FileHash -LiteralPath ([Environment]::ProcessPath) -Algorithm SHA256).Hash}
    local_timezone=[TimeZoneInfo]::Local.Id;local_offset=[TimeZoneInfo]::Local.GetUtcOffset($now).ToString()
    legacy=@{input=$release.released_utc;converted_type=$legacy.released_utc.GetType().FullName;string=$legacyString;parsed_offset=$legacyParsed.Offset.ToString();guard_rejected=$legacyRejected}
    positive=@{released_utc=$release.released_utc;expires_utc=$release.expires_utc;offset=$parsedIssued.Offset.ToString();guard_accepted=$true}
    rejected=$rejected;check_names=@($checks);synthetic_only=$true
    ue_ubt_uat_cook_game_mcp_gui_calls=0;native_validator_executed=$false;actual_smoke_executed=$false
    acceptance_flags=@{UI7='UNVERIFIED';visual_review='UNVERIFIED';release_eligible=$false}
}
Write-JsonNew (Join-Path $out 'validation.json') $report
$report|ConvertTo-Json -Depth 12
