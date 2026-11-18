param(
    [string]$HostDirectory = 'cold-host',
    [string]$RunDirectory = 'cold-verified'
)
$ErrorActionPreference = 'Stop'
$TaskRoot = 'D:\Project\Rogue10m'
$TaskExe = 'D:\Program Files\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$TaskBase = Join-Path $TaskRoot 'Feature\doc\evidence\cave-organic20261118'
$TaskExpectedConfigSha = 'd6eed04d51fe439b46c6092d06696634ecfa1ee8e330697347a044a186690d4f'
$TaskMap = '/Game/Rogue10m/Maps/Production/WorldLayoutCompactCrop20261114A'
$TaskOwn = '/Game/Rogue10m/World/Production/WorldLayoutCompactCrop20261114A/NaturalBoundary20261117/Caves20261118/Organic20261118/'

if ($HostDirectory -cnotmatch '^cold-host(?:-[a-z0-9_-]{1,48})?$') { throw 'Bounded fresh cold-host directory required' }
if ($RunDirectory -cnotmatch '^cold-[a-z0-9_-]{1,64}$') { throw 'Bounded fresh cold-* semantic run directory required' }
$TaskOut = Join-Path $TaskBase $HostDirectory
$TaskRunOut = Join-Path $TaskBase $RunDirectory
if ((Test-Path -LiteralPath $TaskOut) -or (Test-Path -LiteralPath $TaskRunOut)) {
    throw 'Preserve existing organic-cave cold evidence; use an explicitly selected fresh run'
}
$TaskConfigFile = Join-Path $TaskBase 'validation-config.json'
$TaskConfigSha = (Get-FileHash -LiteralPath $TaskConfigFile -Algorithm SHA256).Hash.ToLowerInvariant()
if ($TaskConfigSha -cne $TaskExpectedConfigSha) { throw 'The explicitly reviewed organic-cave configuration changed' }
$TaskConfig = Get-Content -LiteralPath $TaskConfigFile -Raw | ConvertFrom-Json
if ($TaskConfig.expected_material_path -cne ($TaskOwn + 'MI_OrganicRock.MI_OrganicRock')) {
    throw 'Exact reviewed organic material required'
}

$TaskActorPackages = @(
    '/Game/__ExternalActors__/Rogue10m/Maps/Production/WorldLayoutCompactCrop20261114A/9/EE/NR3SMNORZ57AQ1FV26VAFO',
    '/Game/__ExternalActors__/Rogue10m/Maps/Production/WorldLayoutCompactCrop20261114A/3/BY/2CW0JPTDIEMKKD1T5LATX6',
    '/Game/__ExternalActors__/Rogue10m/Maps/Production/WorldLayoutCompactCrop20261114A/7/0E/NL2M3149WONPRWLKFKVP6Y',
    '/Game/__ExternalActors__/Rogue10m/Maps/Production/WorldLayoutCompactCrop20261114A/2/VQ/PYZIJL655JALHJM6PS58A8'
)
$TaskAllowedPackages = $TaskActorPackages + @(
    ($TaskOwn + 'M_OrganicRock'), ($TaskOwn + 'MI_OrganicRock'),
    ($TaskConfig.expected_mesh_paths.Q.Split('.')[0]), ($TaskConfig.expected_mesh_paths.Z.Split('.')[0])
)
if ($TaskAllowedPackages.Count -ne 8 -or @($TaskAllowedPackages | Sort-Object -Unique).Count -ne 8) {
    throw 'Exactly four existing cave actors and four new owned assets required'
}
foreach ($TaskNewMeshPackage in $TaskAllowedPackages[6..7]) {
    if (-not $TaskNewMeshPackage.StartsWith($TaskOwn + 'SM_', [StringComparison]::Ordinal)) {
        throw 'New mesh package escaped the explicitly reviewed owned namespace'
    }
}
$TaskSaveFile = Join-Path $TaskBase 'author-live\final-save-result.json'
$TaskSaved = Get-Content -LiteralPath $TaskSaveFile -Raw | ConvertFrom-Json
if ($TaskSaved.success -ne $true -or $TaskSaved.native_save_returned -ne $true -or
    $TaskSaved.save_called -ne $true -or $TaskSaved.save_all_dirty_called -ne $false -or
    $TaskSaved.map -cne $TaskMap -or @($TaskSaved.dirty_packages).Count -ne 0) {
    throw 'Successful saved-clean bounded organic-cave final save required'
}
foreach ($TaskSaveField in @('requested_packages', 'dirty_before')) {
    $TaskObservedPackages = @($TaskSaved.$TaskSaveField)
    if ($TaskObservedPackages.Count -ne 8 -or @($TaskObservedPackages | Sort-Object -Unique).Count -ne 8 -or
        @(Compare-Object -ReferenceObject $TaskAllowedPackages -DifferenceObject $TaskObservedPackages -CaseSensitive).Count -ne 0) {
        throw ('Final save must contain exactly the eight approved packages: ' + $TaskSaveField)
    }
}
$TaskSourceHashes = [ordered]@{}
foreach ($TaskRelative in @('Scripts/Editor/OrganicCaves20261118QA.py', 'Scripts/Editor/OrganicCaves20261118Cold.py')) {
    $TaskSourceHashes[$TaskRelative] = (Get-FileHash -LiteralPath (Join-Path $TaskRoot $TaskRelative) -Algorithm SHA256).Hash.ToLowerInvariant()
}
$TaskDllSha = (Get-FileHash -LiteralPath (Join-Path $TaskRoot 'Binaries\Win64\UnrealEditor-Rogue10m.dll') -Algorithm SHA256).Hash.ToLowerInvariant()
New-Item -ItemType Directory -Path $TaskOut | Out-Null
$TaskArgs = @(
    ('"' + $TaskRoot + '\Rogue10m.uproject"'), $TaskMap,
    '-NoLiveCoding', '-NoSplash', '-NoSound', '-unattended', '-nullrhi', '-language=en', '-locale=en',
    '-RogueOrganicCaveStage=cold', ('-RogueOrganicCaveRun=' + $RunDirectory),
    ('-RogueOrganicCaveConfigSHA256=' + $TaskConfigSha),
    '-ini:EditorPerProjectUserSettings:[/Script/UnrealEd.EditorLoadingSavingSettings]:bAutoSaveEnable=False',
    '-ini:EditorPerProjectUserSettings:[/Script/ModelContextProtocolEngine.ModelContextProtocolSettings]:bAutoStartServer=False',
    ('"-AbsLog=' + (Join-Path $TaskOut 'engine.log') + '"'),
    '"-ExecCmds=py D:/Project/Rogue10m/Scripts/Editor/OrganicCaves20261118Cold.py"'
)
$TaskWatch = [Diagnostics.Stopwatch]::StartNew()
$TaskProcess = Start-Process -FilePath $TaskExe -ArgumentList $TaskArgs -WorkingDirectory $TaskRoot -WindowStyle Hidden -PassThru
[ordered]@{
    pid = $TaskProcess.Id; exe = $TaskExe; arguments = $TaskArgs; startedUtc = [DateTime]::UtcNow.ToString('o')
    dllSha256 = $TaskDllSha; script_hashes = $TaskSourceHashes; validation_config_file_sha256 = $TaskConfigSha
    bounded_save_file_sha256 = (Get-FileHash -LiteralPath $TaskSaveFile -Algorithm SHA256).Hash.ToLowerInvariant()
    host_directory = $HostDirectory; semantic_run_directory = $RunDirectory; assets_saved_by_launcher = $false
} | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $TaskOut 'launch.json') -Encoding utf8
Write-Output ('Independent organic-cave cold PID ' + $TaskProcess.Id)
$TaskTimedOut = $false
while (-not $TaskProcess.HasExited) {
    if ($TaskWatch.Elapsed.TotalSeconds -gt 600) {
        $TaskTimedOut = $true
        Stop-Process -InputObject $TaskProcess -Force
        break
    }
    Start-Sleep -Seconds 2
    $TaskProcess.Refresh()
}
$TaskProcess.WaitForExit()
[ordered]@{
    pid = $TaskProcess.Id; exit_code = $TaskProcess.ExitCode; exitCode = $TaskProcess.ExitCode
    elapsed_seconds = $TaskWatch.Elapsed.TotalSeconds; observedUtc = [DateTime]::UtcNow.ToString('o')
    process_handle_retained = $true; assets_saved_by_launcher = $false; timed_out = $TaskTimedOut
} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $TaskOut 'process-exit.json') -Encoding utf8
Write-Output ('Organic-cave cold process exit ' + $TaskProcess.ExitCode)
if ($TaskTimedOut) { throw 'Owned organic-cave cold process exceeded the600-second watchdog; reports retained' }
if (-not (Test-Path -LiteralPath (Join-Path $TaskOut 'engine.log'))) { throw 'No Unreal log was created; cold validation did not run' }
if ($TaskProcess.ExitCode -ne 0) { exit $TaskProcess.ExitCode }
$TaskResultFile = Join-Path $TaskRunOut 'result.json'
if (-not (Test-Path -LiteralPath $TaskResultFile)) { throw 'Normal engine exit without a fresh semantic result is not acceptance' }
$TaskResult = Get-Content -LiteralPath $TaskResultFile -Raw | ConvertFrom-Json
$TaskSemanticPassed = $TaskResult.passed -eq $true -and $TaskResult.pid -eq $TaskProcess.Id -and $TaskResult.assets_saved -eq $false
[ordered]@{
    passed = $TaskSemanticPassed; pid = $TaskProcess.Id; process_exit_code = $TaskProcess.ExitCode
    semantic_result = ('Feature/doc/evidence/cave-organic20261118/' + $RunDirectory + '/result.json')
    semantic_result_sha256 = (Get-FileHash -LiteralPath $TaskResultFile -Algorithm SHA256).Hash.ToLowerInvariant()
    process_handle_retained = $true; assets_saved_by_launcher = $false
} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $TaskOut 'acceptance.json') -Encoding utf8
if (-not $TaskSemanticPassed) { throw 'Owned engine exit0 did not accompany same-PID semantic validation success' }
exit 0
