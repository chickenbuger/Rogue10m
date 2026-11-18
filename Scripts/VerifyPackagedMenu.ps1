# Requires PowerShell 7 on Windows. Tests the cold menu only; inspect the PNGs separately.
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$ArchiveDir,
    [Parameter(Mandatory)][string]$OutputDir,
    [ValidateRange(100, 900)][int]$WallTimeoutSeconds = 180,
    [switch]$UseBootstrap
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

function Get-SaveSnapshot([string]$Root) {
    if (-not [IO.Directory]::Exists($Root)) { return }
    Get-ChildItem -LiteralPath $Root -Recurse -File -Filter '*.sav' | Sort-Object FullName | ForEach-Object {
        [pscustomobject]@{ path = $_.FullName; sha256 = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash; bytes = $_.Length }
    }
}
function Assert-ChildPath([string]$Root, [string]$Child) {
    $prefix = $Root.TrimEnd([IO.Path]::DirectorySeparatorChar) + [IO.Path]::DirectorySeparatorChar
    if (-not [IO.Path]::GetFullPath($Child).StartsWith($prefix, [StringComparison]::OrdinalIgnoreCase)) {
        throw 'Runtime output escaped the fresh output directory'
    }
}
function Quote-UEValue([string]$Value) {
    if ($Value.IndexOfAny([char[]]@('"', "`r", "`n", [char]0)) -ge 0) { throw 'Unsupported quote or control character in launch value' }
    return '"' + $Value.Replace('\', '/') + '"'
}

if (-not $IsWindows) { throw 'This Win64 package verification requires Windows and PowerShell 7' }
$archive = (Resolve-Path -LiteralPath $ArchiveDir).ProviderPath
if (-not [IO.Directory]::Exists($archive)) { throw 'ArchiveDir must be a directory' }
$out = [IO.Path]::GetFullPath($OutputDir)
if (Test-Path -LiteralPath $out) { throw 'OutputDir already exists; preserve prior evidence and choose a fresh directory' }
if ($out.IndexOfAny([char[]]@('"', "`r", "`n", [char]0)) -ge 0) { throw 'Unsupported output path character' }
$executables = @(Get-ChildItem -LiteralPath $archive -Recurse -File -Filter 'Rogue10m.exe' |
    Where-Object { $_.FullName -match '[\\/]Rogue10m[\\/]Binaries[\\/]Win64[\\/]Rogue10m\.exe$' })
if ($executables.Count -ne 1) { throw 'Require exactly one archived inner Rogue10m/Binaries/Win64/Rogue10m.exe' }
$runtimeExe = $executables[0].FullName
$exe = $runtimeExe
if ($UseBootstrap) {
    $platformRoot = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $runtimeExe)))
    $exe = Join-Path $platformRoot 'Rogue10m.exe'
    if (-not [IO.File]::Exists($exe)) { throw 'Root packaged bootstrap executable missing' }
}
if (@(Get-CimInstance Win32_Process | Where-Object { $_.Name -eq 'Rogue10m.exe' }).Count) { throw 'Require exclusive packaged runtime ownership' }
$user = Join-Path $out 'runtime-user'
$log = Join-Path $out 'packaged.log'
$recordPath = Join-Path $out 'smoke.json'
$preservationPath = Join-Path $out 'save-preservation.json'
foreach ($child in @($user, $log, $recordPath, $preservationPath)) { Assert-ChildPath $out $child }
[void][IO.Directory]::CreateDirectory($out)
if (@(Get-ChildItem -LiteralPath $out -Force).Count) { throw 'Fresh OutputDir is not empty' }
[void][IO.Directory]::CreateDirectory($user)
$realSaveRoot = Join-Path ([Environment]::GetFolderPath('LocalApplicationData')) 'Rogue10m\Saved\SaveGames'
$record = [ordered]@{
    phase = 'starting'; scope = $(if ($UseBootstrap) { 'packaged_bootstrap_cold_menu_only' } else { 'packaged_cold_menu_only' }); visualReview = 'pending';
    useBootstrap = [bool]$UseBootstrap; runtimeExe = $runtimeExe; runtimeExeSHA256 = (Get-FileHash -LiteralPath $runtimeExe -Algorithm SHA256).Hash;
    exe = $exe; exeSHA256 = (Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash;
    outputDir = $out; userDir = $user; realSaveRoot = $realSaveRoot;
    startedUtc = [DateTime]::UtcNow.ToString('o'); pid = $null; exitCode = $null;
    timedOut = $false; userDirProbeVerified = $false; screenshots = @(); failures = @()
}
function Write-Journal { $record | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $recordPath -Encoding utf8NoBOM }
$p = $null
$before = @()
$snapshotReady = $false
$failures = [Collections.Generic.List[string]]::new()
Write-Journal
try {
    $before = @(Get-SaveSnapshot $realSaveRoot)
    ConvertTo-Json -InputObject $before -Depth 4 | Set-Content -LiteralPath (Join-Path $out 'real-save-baseline.json') -Encoding utf8NoBOM
    $snapshotReady = $true
    # Relative filenames avoid nested quoting inside the comma-delimited CSV frame commands.
    # UE resolves these below the isolated UserDir's Saved/Screenshots/<platform> directory.
    $commands = 'REDIRECTTOFILE PackageUserDirProbe.txt GetIni Engine:/Script/EngineSettings.GameMapsSettings GameDefaultMap,t.MaxFPS 60,csvprofile start'
    $frames = '120:shot showui filename=menu120.png -nosuffix,240:shot showui filename=menu240.png -nosuffix,360:quit'
    $arguments = @('-RenderOffscreen', '-d3d12', '-windowed', '-ResX=1280', '-ResY=720', '-ForceRes', '-NoSplash', '-Unattended', '-NoSound', '-seconds=90',
        ('-UserDir=' + (Quote-UEValue $user)), ('-abslog=' + (Quote-UEValue $log)),
        ('-ExecCmds=' + (Quote-UEValue $commands)), ('-csvExecCmds=' + (Quote-UEValue $frames)))
    $record.arguments = $arguments
    $p = Start-Process -FilePath $exe -ArgumentList $arguments -WorkingDirectory (Split-Path -Parent $exe) -WindowStyle Hidden -PassThru
    if ($UseBootstrap) {
        $bootstrap = $p
        $null = $bootstrap.Handle
        $record.bootstrapPid = $bootstrap.Id
        $record.bootstrapStartedUtc = $bootstrap.StartTime.ToUniversalTime().ToString('o')
        $deadline = [DateTime]::UtcNow.AddSeconds(30)
        $child = $null
        do {
            $matches = @(Get-CimInstance Win32_Process | Where-Object { $_.ExecutablePath -eq $runtimeExe -and $_.ParentProcessId -eq $bootstrap.Id -and $_.CommandLine.Contains($user.Replace('\', '/')) })
            if ($matches.Count -gt 1) { throw 'Ambiguous packaged bootstrap child' }
            if ($matches.Count -eq 1) { $child = $matches[0]; break }
            Start-Sleep -Milliseconds 200
        } while ([DateTime]::UtcNow -lt $deadline)
        if (-not $child) { throw 'Bootstrap did not launch its exact inner game with isolated UserDir' }
        $p = [Diagnostics.Process]::GetProcessById([int]$child.ProcessId)
        $null = $p.Handle
        $record.bootstrapChildParentPid = $child.ParentProcessId
        $record.bootstrapChildCommandLine = $child.CommandLine
    }
    $record.pid = $p.Id
    $record.processStartedUtc = $p.StartTime.ToUniversalTime().ToString('o')
    $record.phase = 'running'
    Write-Journal
    Write-Output ('Owned packaged menu PID=' + $p.Id + ' UserDir=' + $user)
    $timer = [Diagnostics.Stopwatch]::StartNew()
    while (-not $p.WaitForExit(1000)) {
        if ($timer.Elapsed.TotalSeconds -gt $WallTimeoutSeconds) { $record.timedOut = $true; throw 'Owned packaged menu wall timeout' }
    }
    $p.Refresh()
    $record.exitCode = $p.ExitCode
    $record.wallSeconds = $timer.Elapsed.TotalSeconds
    if ($UseBootstrap) {
        if (-not $bootstrap.WaitForExit(10000)) { throw 'Bootstrap did not exit after owned game ended' }
        $record.bootstrapExitCode = $bootstrap.ExitCode
        if ($bootstrap.ExitCode -ne 0) { throw 'Bootstrap returned nonzero exit' }
    }
    if ($p.ExitCode -ne 0) { throw ('Packaged Game exit=' + $p.ExitCode) }
    $saved = Join-Path $user 'Saved'
    $probe = Join-Path $saved 'PackageUserDirProbe.txt'
    Assert-ChildPath $out $probe
    if (-not [IO.File]::Exists($probe)) { throw 'Actual engine Saved routing probe missing' }
    if (-not [IO.File]::ReadAllText($probe).Contains('/Game/Rogue10m/Maps/StartHub')) { throw 'Saved routing probe did not report StartHub' }
    $text = [IO.File]::ReadAllText($log)
    if ($text -notmatch 'LogLoad:\s+LoadMap:\s+/Game/Rogue10m/Maps/StartHub(?:[?.\s]|$)') { throw 'Actual StartHub map load missing' }
    if ($text -notmatch "LogLoad:\s+Game class is 'Rogue10mMenuGameMode'") { throw 'Front-end game mode missing on initial StartHub load' }
    if ($text -match 'Fatal error:|Assertion failed:|LowLevelFatalError|Failed to open descriptor') { throw 'Fatal packaged startup error' }
    $startupFailures = @($text -split "`r?`n" | Where-Object { $_ -match 'Log[A-Za-z0-9_]+:\s+Error:|LoadPackage: SkipPackage:|Widget is unavailable' })
    if ($startupFailures.Count) { throw ('Packaged runtime asset/UI failure: ' + ($startupFailures -join "`n")) }
    $record.userDirProbeVerified = $true
    Add-Type -AssemblyName System.Drawing
    $pngRecords = @()
    foreach ($name in @('menu120.png', 'menu240.png')) {
        $matches = @(Get-ChildItem -LiteralPath (Join-Path $saved 'Screenshots') -Recurse -File -Filter $name)
        if ($matches.Count -ne 1) { throw ('Require exactly one actual delayed SHOWUI screenshot: ' + $name) }
        $png = $matches[0].FullName
        Assert-ChildPath $out $png
        $image = [Drawing.Image]::FromFile($png)
        try {
            if ($image.Width -ne 1280 -or $image.Height -ne 720) { throw 'Screenshot dimension mismatch' }
            $pngRecords += [pscustomobject]@{ path = $png; width = $image.Width; height = $image.Height; sha256 = (Get-FileHash -LiteralPath $png -Algorithm SHA256).Hash }
        } finally { $image.Dispose() }
    }
    $record.screenshots = $pngRecords
    $record.createdIsolatedSaves = @(Get-ChildItem -LiteralPath $user -Recurse -File -Filter '*.sav' | Select-Object -ExpandProperty FullName)
    $record.phase = 'runtime_checks_complete_preservation_pending'
} catch {
    $failures.Add($_.ToString())
} finally {
    try {
        if ($p -and -not $p.HasExited) {
            $p.Kill($true)
            if (-not $p.WaitForExit(10000)) { throw 'Owned packaged process did not exit after timeout cleanup' }
        }
    } catch { $failures.Add('Process cleanup: ' + $_.ToString()) }
    try {
        if (-not $snapshotReady) { throw 'Original default save snapshot was not completed; preservation cannot be established' }
        $after = @(Get-SaveSnapshot $realSaveRoot)
        $beforeMap = @{}
        $afterMap = @{}
        foreach ($save in $before) { $beforeMap[$save.path] = $save }
        foreach ($save in $after) { $afterMap[$save.path] = $save }
        $missing = @($before | Where-Object { -not $afterMap.ContainsKey($_.path) } | Select-Object -ExpandProperty path)
        $added = @($after | Where-Object { -not $beforeMap.ContainsKey($_.path) } | Select-Object -ExpandProperty path)
        $changed = @($before | Where-Object { $afterMap.ContainsKey($_.path) -and $_.sha256 -ne $afterMap[$_.path].sha256 } | Select-Object -ExpandProperty path)
        $preservation = [ordered]@{ root = $realSaveRoot; beforeCount = $before.Count; afterCount = $after.Count; missing = $missing; added = $added; changed = $changed; after = $after }
        $preservation | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $preservationPath -Encoding utf8NoBOM
        if ($missing.Count -or $added.Count -or $changed.Count) { throw 'Default packaged save path set or original save content changed' }
        $record.defaultSavePreservationVerified = $true
    } catch { $failures.Add('Save preservation: ' + $_.ToString()) }
    $record.failures = @($failures.ToArray())
    $record.endedUtc = [DateTime]::UtcNow.ToString('o')
    if ($failures.Count) { $record.phase = 'failed' }
    else { $record.phase = 'packaged_cold_menu_passed_visual_review_pending' }
    Write-Journal
}
if ($failures.Count) { throw ($failures -join "`n") }
Write-Output ('Packaged cold menu data checks passed; actual PNG visual review pending. ' + $out)
