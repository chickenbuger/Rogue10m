#requires -Version 7.5
[CmdletBinding()]
param([switch]$Execute)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$PSNativeCommandUseErrorActionPreference = $false
$hubRoot = 'D:\Project\Rogue10m'
$hubEvidence = Join-Path $hubRoot 'Feature\doc\evidence\hub-entry-windows20261114A'
$hubArea = 'C:\Users\PC\AppData\Local\Temp\R10HubEntryWindows20261114A'
$hubPacket = Join-Path $hubRoot 'Feature\implementation\native51-internal-smoke-cook-fix-f'
$hubCheckpoint = Get-Content -LiteralPath (Join-Path $hubEvidence 'content-checkpoint.json') -Raw | ConvertFrom-Json -AsHashtable -DateKind String
if ($hubCheckpoint.status -cne 'ACTUAL_HUB_INPUTS_SEALED_INTERNAL_DEVELOPMENT_NOT_EXECUTED') { throw 'Exact new content checkpoint required.' }
$hubSeal = $hubCheckpoint.seal
if ((Get-FileHash -LiteralPath $hubSeal.path -Algorithm SHA256).Hash -cne $hubSeal.sha256) { throw 'Seal changed.' }
$hubData = Get-Content -LiteralPath $hubSeal.path -Raw | ConvertFrom-Json -AsHashtable -DateKind String
if ($hubData.output_root -cne ($hubArea+'\run01') -or $hubData.run_tag -cne 'HubEntry20261114A01') { throw 'Unexpected output identity.' }
$hubOthers = @(Get-CimInstance Win32_Process | Where-Object {
    $_.Name -match '^(UnrealEditor.*|UnrealBuildTool|AutomationTool|Rogue10m.*)\.exe$' -or
    ($_.Name -eq 'dotnet.exe' -and $_.CommandLine -match 'UnrealBuildTool|AutomationTool')
})
if ($hubOthers.Count -ne 0) { throw 'Another engine/build/game is running; preserve it and stop.' }
foreach ($hubPrior in @($hubCheckpoint.prior_records)) {
    if ((Get-FileHash -LiteralPath $hubPrior.path -Algorithm SHA256).Hash -cne $hubPrior.sha256) { throw ('Prior record changed: '+$hubPrior.path) }
}
$hubSelf = [Diagnostics.Process]::GetCurrentProcess()
$hubIdentity = [ordered]@{
    pid=[int]$PID;start_utc_ticks=[long]$hubSelf.StartTime.ToUniversalTime().Ticks
    executable=[Environment]::ProcessPath
    sha256=(Get-FileHash -LiteralPath ([Environment]::ProcessPath) -Algorithm SHA256).Hash
}
if ($hubIdentity.executable -cne $hubData.runtime.powershell.path -or $hubIdentity.sha256 -cne $hubData.runtime.powershell.sha256) { throw 'Exact PS7 owner identity required.' }
$hubRelease = [ordered]@{
    schema='R10-native51-smoke-release-v1';status='released_for_native51_smoke'
    seal_sha256=$hubSeal.sha256;project=$hubData.project;owner_id='RootHubEntryA'
    owner_process=$hubIdentity;run_tag=$hubData.run_tag;active_destination=$hubData.active_destination
    scopes=@('Win64 Development Native51 internal smoke')
    project_build_outputs_authorized=$true;gui_forbidden=$true;release_eligible=$false
    released_utc=[DateTime]::UtcNow.ToString('o');expires_utc=[DateTime]::UtcNow.AddHours(4).ToString('o')
    note='Actual current integration owner session for user-requested packaging only. No Game execution or GUI control; not root-startup-approval-A.json, metadata/clone QA permission, a test result or release certification.'
}
$hubReleasePath = Join-Path $hubData.output_root 'native-owner-release.json'
$hubBytes = [Text.UTF8Encoding]::new($false).GetBytes(($hubRelease | ConvertTo-Json -Depth 15))
$hubStream = [IO.FileStream]::new($hubReleasePath,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::Read)
try { $hubStream.Write($hubBytes,0,$hubBytes.Length); $hubStream.Flush($true) } finally { $hubStream.Dispose() }
$hubReleaseSha = (Get-FileHash -LiteralPath $hubReleasePath -Algorithm SHA256).Hash
$hubStarted = [DateTime]::UtcNow.ToString('o')
$hubOwnerLog = Join-Path $hubEvidence 'owner.log'
if (Test-Path -LiteralPath $hubOwnerLog) { throw 'Owner log already exists; preserve prior run.' }
$hubWrapper = Join-Path $hubPacket 'Invoke-InternalSmoke.ps1'
if ($Execute) {
    & $hubWrapper -Seal $hubSeal.path -SealSha256 $hubSeal.sha256 -Release $hubReleasePath -ReleaseSha256 $hubReleaseSha -OwnerId 'RootHubEntryA' -PythonPath $hubData.runtime.python.path -Execute *> $hubOwnerLog
} else {
    & $hubWrapper -Seal $hubSeal.path -SealSha256 $hubSeal.sha256 -Release $hubReleasePath -ReleaseSha256 $hubReleaseSha -OwnerId 'RootHubEntryA' -PythonPath $hubData.runtime.python.path *> $hubOwnerLog
}
$hubExit = $LASTEXITCODE
$hubResult = [ordered]@{
    status='OWNER_WRAPPER_RETURNED_OBSERVE_LOG_AND_POSTFLIGHT';execute_requested=[bool]$Execute
    started_utc=$hubStarted;ended_utc=[DateTime]::UtcNow.ToString('o');exit_code=$hubExit
    owner_process=$hubIdentity;seal=$hubSeal;release=@{path=$hubReleasePath;sha256=$hubReleaseSha}
    owner_log=$hubOwnerLog;output_root=$hubData.output_root;release_eligible=$false
    new_game_execution_or_input=0;denied_startup_or_metadata_clone_QA_executed=$false
}
$hubResultPath = Join-Path $hubEvidence 'owner-result.json'
if (Test-Path -LiteralPath $hubResultPath) { throw 'Owner result already exists.' }
[IO.File]::WriteAllText($hubResultPath,($hubResult | ConvertTo-Json -Depth 15),[Text.UTF8Encoding]::new($false))
$hubResult | ConvertTo-Json -Depth 15
exit $hubExit
