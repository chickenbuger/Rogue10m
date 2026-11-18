#requires -Version 7.5
[CmdletBinding()]
param([switch]$Execute)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$PSNativeCommandUseErrorActionPreference = $false
$npcRoot = 'D:\Project\Rogue10m'
$npcEvidence = Join-Path $npcRoot 'Feature\doc\evidence\npc-intro-compact20261113B'
$npcArea = 'C:\Users\PC\AppData\Local\Temp\R10Native51IntroSmoke20261113C'
$npcPacket = Join-Path $npcRoot 'Feature\implementation\native51-internal-smoke-cook-fix-f'
$npcCheckpoint = Get-Content -LiteralPath (Join-Path $npcEvidence 'internal-smoke-C-content-checkpoint.json') -Raw | ConvertFrom-Json -AsHashtable -DateKind String
if ($npcCheckpoint.status -cne 'ACTUAL_CONTENT_INPUTS_SEALED_INTERNAL_DEVELOPMENT_NOT_EXECUTED') { throw 'Exact content checkpoint required.' }
$npcSeal = $npcCheckpoint.seal
if ((Get-FileHash -LiteralPath $npcSeal.path -Algorithm SHA256).Hash -cne $npcSeal.sha256) { throw 'Seal changed.' }
$npcData = Get-Content -LiteralPath $npcSeal.path -Raw | ConvertFrom-Json -AsHashtable -DateKind String
if ($npcData.output_root -cne ($npcArea+'\run01') -or $npcData.run_tag -cne 'Native51IntroC01') { throw 'Unexpected output identity.' }
$npcOthers = @(Get-CimInstance Win32_Process | Where-Object {
    $_.Name -match '^(UnrealEditor.*|UnrealBuildTool|AutomationTool|Rogue10m.*)\.exe$' -or
    ($_.Name -eq 'dotnet.exe' -and $_.CommandLine -match 'UnrealBuildTool|AutomationTool')
})
if ($npcOthers.Count -ne 0) { throw 'Another engine/build/game is running; preserve it and stop.' }

$npcSelf = [Diagnostics.Process]::GetCurrentProcess()
$npcIdentity = [ordered]@{
    pid=[int]$PID
    start_utc_ticks=[long]$npcSelf.StartTime.ToUniversalTime().Ticks
    executable=[Environment]::ProcessPath
    sha256=(Get-FileHash -LiteralPath ([Environment]::ProcessPath) -Algorithm SHA256).Hash
}
if ($npcIdentity.executable -cne $npcData.runtime.powershell.path -or $npcIdentity.sha256 -cne $npcData.runtime.powershell.sha256) { throw 'Exact PS7 owner identity required.' }
$npcRelease = [ordered]@{
    schema='R10-native51-smoke-release-v1';status='released_for_native51_smoke'
    seal_sha256=$npcSeal.sha256;project=$npcData.project;owner_id='RootNpcIntroC'
    owner_process=$npcIdentity;run_tag=$npcData.run_tag;active_destination=$npcData.active_destination
    scopes=@('Win64 Development Native51 internal smoke')
    project_build_outputs_authorized=$true;gui_forbidden=$true;release_eligible=$false
    released_utc=[DateTime]::UtcNow.ToString('o');expires_utc=[DateTime]::UtcNow.AddHours(4).ToString('o')
    note='Actual integration owner session for the user-requested internal Windows build only. This is not root-startup-approval-A.json, metadata/clone QA approval, a test result or release certification.'
}
$npcReleasePath = Join-Path $npcData.output_root 'native-owner-release.json'
$npcBytes = [Text.UTF8Encoding]::new($false).GetBytes(($npcRelease | ConvertTo-Json -Depth 15))
$npcStream = [IO.FileStream]::new($npcReleasePath,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::Read)
try { $npcStream.Write($npcBytes,0,$npcBytes.Length); $npcStream.Flush($true) } finally { $npcStream.Dispose() }
$npcReleaseSha = (Get-FileHash -LiteralPath $npcReleasePath -Algorithm SHA256).Hash
$npcStarted = [DateTime]::UtcNow.ToString('o')
$npcOwnerLog = Join-Path $npcEvidence 'internal-windows-C-owner.log'
if (Test-Path -LiteralPath $npcOwnerLog) { throw 'Owner log already exists; preserve prior run.' }
$npcWrapper = Join-Path $npcPacket 'Invoke-InternalSmoke.ps1'
if ($Execute) {
    & $npcWrapper -Seal $npcSeal.path -SealSha256 $npcSeal.sha256 -Release $npcReleasePath -ReleaseSha256 $npcReleaseSha -OwnerId 'RootNpcIntroC' -PythonPath $npcData.runtime.python.path -Execute *> $npcOwnerLog
} else {
    & $npcWrapper -Seal $npcSeal.path -SealSha256 $npcSeal.sha256 -Release $npcReleasePath -ReleaseSha256 $npcReleaseSha -OwnerId 'RootNpcIntroC' -PythonPath $npcData.runtime.python.path *> $npcOwnerLog
}
$npcExit = $LASTEXITCODE
$npcResult = [ordered]@{
    status='OWNER_WRAPPER_RETURNED_OBSERVE_LOG_AND_POSTFLIGHT';execute_requested=[bool]$Execute
    started_utc=$npcStarted;ended_utc=[DateTime]::UtcNow.ToString('o');exit_code=$npcExit
    owner_process=$npcIdentity;seal=$npcSeal;release=@{path=$npcReleasePath;sha256=$npcReleaseSha}
    owner_log=$npcOwnerLog;output_root=$npcData.output_root;release_eligible=$false
    denied_startup_or_metadata_clone_QA_executed=$false
}
$npcResultPath = Join-Path $npcEvidence 'internal-windows-C-owner-result.json'
if (Test-Path -LiteralPath $npcResultPath) { throw 'Owner result already exists.' }
[IO.File]::WriteAllText($npcResultPath,($npcResult | ConvertTo-Json -Depth 15),[Text.UTF8Encoding]::new($false))
$npcResult | ConvertTo-Json -Depth 15
exit $npcExit
