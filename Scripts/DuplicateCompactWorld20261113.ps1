#requires -Version 7.0
[CmdletBinding()]
param(
    [switch]$Execute,
    [string]$EditorHandoff = '',
    [ValidateRange(30,900)][int]$TimeoutSeconds = 600
)
$ErrorActionPreference = 'Stop'
$projectRoot = [IO.Path]::GetFullPath((Split-Path -Parent $PSScriptRoot))
if ($projectRoot -ine 'D:\Project\Rogue10m') { throw 'Wrong physical project root.' }
$project = Join-Path $projectRoot 'Rogue10m.uproject'
$sourcePackage = '/Game/Rogue10m/Maps/Production/WorldLayout20261112'
$targetPackage = '/Game/Rogue10m/Maps/Production/WorldLayoutCompact20261113A'
$targetRelative = @(
    'Content/Rogue10m/Maps/Production/WorldLayoutCompact20261113A.umap',
    'Content/Rogue10m/Maps/Production/WorldLayoutCompact20261113A_BuiltData.uasset',
    'Content/__ExternalActors__/Rogue10m/Maps/Production/WorldLayoutCompact20261113A',
    'Content/__ExternalObjects__/Rogue10m/Maps/Production/WorldLayoutCompact20261113A'
)
$evidence = Join-Path $projectRoot 'Feature/doc/evidence/map-resume-20261113/DuplicateA'
$commandlet = 'D:\Program Files\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$arguments = @($project,$sourcePackage,'-run=WorldPartitionBuilderCommandlet',
    '-Builder=WorldPartitionRenameDuplicateBuilder',"-NewPackage=$targetPackage",'-SCCProvider=None',
    '-unattended','-NullRHI','-NoSound','-NoSplash','-NoLiveCoding','-NoSourceControl',
    '-ini:EditorPerProjectUserSettings:[/Script/ModelContextProtocolEngine.ModelContextProtocolSettings]:bAutoStartServer=False',
    ('-abslog=' + (Join-Path $evidence 'engine.log')))

function Assert-TargetAbsent {
    foreach ($relative in $targetRelative) {
        $absolute = [IO.Path]::GetFullPath((Join-Path $projectRoot $relative))
        if (-not $absolute.StartsWith((Join-Path $projectRoot 'Content') + [IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)) { throw 'Target escaped Content.' }
        if (Test-Path -LiteralPath $absolute) { throw "Target already exists; builder may delete it, so no run is allowed: $relative" }
        $ancestor = [IO.Path]::GetDirectoryName($absolute)
        while ($ancestor.Length -gt $projectRoot.Length) {
            if (Test-Path -LiteralPath $ancestor) {
                if ((Get-Item -LiteralPath $ancestor -Force).Attributes -band [IO.FileAttributes]::ReparsePoint) { throw 'Target ancestor is a reparse point.' }
            }
            $ancestor = [IO.Path]::GetDirectoryName($ancestor)
        }
    }
}

function Assert-SharedProcessesIdle {
    $busy = @(Get-CimInstance Win32_Process | Where-Object {
        $_.Name -match '^(UnrealEditor(-Cmd)?|UnrealBuildTool|dotnet|MSBuild|AutomationTool|Rogue10m(-Win64-(Development|Shipping|Test))?)\.exe$'
    })
    if ($busy.Count -gt 0) { throw 'A shared editor/build/UAT/game process already exists.' }
}

Assert-TargetAbsent
if (-not $Execute) {
    [pscustomobject]@{mode='plan_only';source=$sourcePackage;target=$targetPackage;arguments=$arguments;
        source_modified=$false;target_must_be_absent=$true;rename_switch=$false;editor_started=$false} | ConvertTo-Json -Depth 5
    return
}
if ([string]::IsNullOrWhiteSpace($EditorHandoff)) { throw 'Explicit shared editor/build handoff is required.' }
if (-not (Test-Path -LiteralPath $commandlet -PathType Leaf)) { throw 'Verified UE5.8 commandlet executable missing.' }
if (Test-Path -LiteralPath $evidence) { throw 'Existing duplicate evidence is preserved; inspect it instead of replaying.' }
Assert-SharedProcessesIdle
$baseline = Get-Content -LiteralPath (Join-Path $projectRoot 'Feature/doc/evidence/map-resume-20261113/BaselineA/checkpoint.json') -Raw -Encoding UTF8 | ConvertFrom-Json
$build = Get-Content -LiteralPath (Join-Path $projectRoot 'Feature/doc/evidence/map-resume-20261113/BaselineA/baseline-build.json') -Raw -Encoding UTF8 | ConvertFrom-Json
if ($baseline.status -ne 'snapshot_and_backups_verified' -or $build.status -ne 'baseline_editor_build_passed') { throw 'Verified backup and baseline build are required.' }
if ((Get-FileHash -LiteralPath (Join-Path $projectRoot 'Binaries/Win64/UnrealEditor-Rogue10m.dll') -Algorithm SHA256).Hash -ne $build.dll_sha256) { throw 'The baseline DLL changed; inspect before duplicating.' }
foreach ($row in $baseline.files) {
    if ((Get-FileHash -LiteralPath (Join-Path $projectRoot $row.path) -Algorithm SHA256).Hash -ne $row.sha256) { throw "Protected input changed: $($row.path)" }
}
Assert-TargetAbsent
[void][IO.Directory]::CreateDirectory($evidence)
$report = [ordered]@{status='starting';source=$sourcePackage;target=$targetPackage;handoff=$EditorHandoff;
    started_utc=[DateTime]::UtcNow.ToString('o');arguments=$arguments;rename_switch=$false;
    scope='independent saved-source duplicate only; no crop or gameplay certification';
    autosave_recovery='preserved separately; not restored or silently merged';process_id=$null;process_start_utc=$null;
    exit_code=$null;timed_out=$false;process_cleanup='not_needed';source_hash_changes=@();error=$null}
$process = $null
function Write-Journal {
    [IO.File]::WriteAllText((Join-Path $evidence 'result.json'),($report | ConvertTo-Json -Depth 7)+[Environment]::NewLine,[Text.UTF8Encoding]::new($false))
}
try {
    Assert-SharedProcessesIdle
    Assert-TargetAbsent
    $process = Start-Process -FilePath $commandlet -ArgumentList $arguments -WorkingDirectory $projectRoot -WindowStyle Hidden -PassThru `
        -RedirectStandardOutput (Join-Path $evidence 'stdout.log') -RedirectStandardError (Join-Path $evidence 'stderr.log')
    $report.process_id = $process.Id
    Write-Journal
    $report.process_start_utc = $process.StartTime.ToUniversalTime().ToString('o')
    Write-Journal
    $watch = [Diagnostics.Stopwatch]::StartNew()
    while (-not $process.WaitForExit(1000)) {
        if ($watch.Elapsed.TotalSeconds -gt $TimeoutSeconds) { $report.timed_out=$true; throw 'Owned duplicate commandlet timed out.' }
    }
    $process.WaitForExit()
    $report.exit_code = $process.ExitCode
    if ($process.ExitCode -ne 0) { throw "Duplicate commandlet exit $($process.ExitCode); retain partial target for inspection." }
    if (-not (Test-Path -LiteralPath (Join-Path $projectRoot $targetRelative[0]) -PathType Leaf)) { throw 'Commandlet exited without the target map.' }
    $report.target_actor_packages = @(Get-ChildItem -LiteralPath (Join-Path $projectRoot $targetRelative[2]) -Recurse -File).Count
    $report.target_object_packages = @(Get-ChildItem -LiteralPath (Join-Path $projectRoot $targetRelative[3]) -Recurse -File).Count
    if ($report.target_actor_packages -lt 1) { throw 'Target external actor packages are missing.' }
    $report.status = 'duplicate_saved_reload_and_crop_pending'
} catch {
    $report.status = 'failed_or_partial_preserved'
    $report.error = $_.Exception.Message
} finally {
    if ($process -and -not $process.HasExited) {
        $owned = $false
        try {
            $live = Get-Process -Id $process.Id -ErrorAction Stop
            $owned = $report.process_start_utc -and $live.Path -ieq $commandlet -and $live.StartTime.ToUniversalTime().ToString('o') -eq $report.process_start_utc
            if ($owned) {
                $live.Kill()
                if ($live.WaitForExit(10000) -and $live.HasExited) { $report.process_cleanup='owned_root_terminated_children_not_certified' }
                else { $report.process_cleanup='owned_root_exit_unconfirmed'; $report.status='failed_cleanup_uncertain' }
            }
            else { $report.process_cleanup='uncertain_not_terminated'; $report.status='failed_cleanup_uncertain' }
        } catch { $report.process_cleanup='uncertain_not_terminated'; $report.status='failed_cleanup_uncertain' }
    }
    foreach ($row in $baseline.files) {
        $path = Join-Path $projectRoot $row.path
        if (-not (Test-Path -LiteralPath $path) -or (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -ne $row.sha256) { $report.source_hash_changes += $row.path }
    }
    if ($report.source_hash_changes.Count -gt 0) { $report.status='protected_source_changed_inspect_without_reverting' }
    $report.finished_utc = [DateTime]::UtcNow.ToString('o')
    Write-Journal
}
[pscustomobject]$report | ConvertTo-Json -Depth 7
if ($report.status -ne 'duplicate_saved_reload_and_crop_pending') { throw 'Duplicate checkpoint did not pass. Do not replay or delete the partial target.' }
