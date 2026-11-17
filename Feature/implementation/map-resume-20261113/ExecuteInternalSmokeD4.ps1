#requires -Version 7.5
# Owner invocation only; sealed production wrapper and its checks remain unchanged.
# No GUI/input or game execution. Prior attempts and all existing outputs are retained.
$ErrorActionPreference='Stop'
$PSNativeCommandUseErrorActionPreference=$false
$area='C:\Users\PC\AppData\Local\Temp\R10InternalSmoke20261113D'
$packet='D:\Project\Rogue10m\Feature\implementation\map-resume-20261113\internal-smoke-culture-utc-d'
$python='D:\Program Files\UE_5.8\Engine\Binaries\ThirdParty\Python3\Win64\python.exe'
function Write-FreshJson([string]$Path,$Value){
    $bytes=[Text.UTF8Encoding]::new($false).GetBytes(($Value | ConvertTo-Json -Depth 35))
    $stream=[IO.File]::Open($Path,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::Read)
    try{$stream.Write($bytes,0,$bytes.Length);$stream.Flush($true)}finally{$stream.Dispose()}
}
$active=@(Get-CimInstance Win32_Process | Where-Object {
    $_.Name -match '^(UnrealEditor.*|UnrealBuildTool.*|UnrealPak.*|Rogue10m.*|AutomationTool.*)\.exe$' -or
    ($_.Name -eq 'dotnet.exe' -and $_.CommandLine -match 'UnrealBuildTool|AutomationTool')
})
if($active.Count){throw 'Another UE/build/game process is active; do not start'}
$preparedPath=Join-Path $area 'smoke-d4.sealed.prepared.json'
$prepared=Get-Content -LiteralPath $preparedPath -Raw | ConvertFrom-Json -AsHashtable -DateKind String
$sealPath=[string]$prepared.seal.path
$sealSHA=(Get-FileHash -LiteralPath $sealPath).Hash
if($sealSHA -cne $prepared.seal.sha256){throw 'Prepared seal changed'}
$seal=Get-Content -LiteralPath $sealPath -Raw | ConvertFrom-Json -AsHashtable -DateKind String
if($seal.run_tag -cne 'SmokeD20261113D' -or $seal.output_root -cne (Join-Path $area 'run04')){throw 'Wrong new run04 seal'}
if((Get-FileHash -LiteralPath (Join-Path $packet 'Invoke-InternalSmoke.ps1')).Hash -cne '32A95E82FB7C68485FD90086D0F185E583BAC86A9B28287B2E69F1367C4CFBD9'){throw 'Frozen wrapper changed'}
$own=[Diagnostics.Process]::GetCurrentProcess()
$identity=@{pid=$PID;start_utc_ticks=$own.StartTime.ToUniversalTime().Ticks;executable=$own.MainModule.FileName;sha256=(Get-FileHash -LiteralPath $own.MainModule.FileName).Hash}
$own.Dispose()
if($identity.sha256 -cne '2A6769A3330DC1084D0154BFDE7D164AB750CA7F700F313F9D417C1BE78B2233'){throw 'Wrong host PowerShell runtime'}
$release=@{
 schema='R10-internal-smoke-release-v1';status='released_for_internal_smoke';seal_sha256=$sealSHA
 project=$seal.project;owner_id='MapOwnerD';owner_process=$identity;run_tag=$seal.run_tag;active_destination=$seal.active_destination
 scopes=@('Win64 Development internal smoke');project_build_outputs_authorized=$true;gui_forbidden=$true;release_eligible=$false
 released_utc=[datetime]::UtcNow.ToString('o');expires_utc=[datetime]::UtcNow.AddHours(4).ToString('o')
 note='Explicit owner live session. Fresh guard-fixed Editor/native43D, frozen capture03, separate run04. Internal Development only; no GUI/game launch/release promotion. Old failures and provisional run03 retained.'
}
$releasePath=Join-Path $area 'owner-release-execute04.json'
Write-FreshJson $releasePath $release
$releaseSHA=(Get-FileHash -LiteralPath $releasePath).Hash
Write-FreshJson (Join-Path $area 'execution-request04.json') @{
 started_utc=[datetime]::UtcNow.ToString('o');owner_process=$identity;seal_sha256=$sealSHA;release_sha256=$releaseSHA
 invocation_script=@{path=$PSCommandPath;sha256=(Get-FileHash -LiteralPath $PSCommandPath).Hash}
 scope='Separate Win64 Development internal artifact; UI/visual UNVERIFIED/releasefalse';gui_operations=0
}
& (Join-Path $packet 'Invoke-InternalSmoke.ps1') -Seal $sealPath -SealSha256 $sealSHA -Release $releasePath -ReleaseSha256 $releaseSHA -OwnerId 'MapOwnerD' -PythonPath $python -Execute *> (Join-Path $area 'wrapper-console04.log')
if($LASTEXITCODE -ne 0){throw ('Sealed wrapper failed; exit='+$LASTEXITCODE)}
$journal=Get-Content -LiteralPath (Join-Path $seal.outputs.evidence 'wrapper-journal.json') -Raw | ConvertFrom-Json -AsHashtable -DateKind String
if($journal.status -cne 'internal_smoke_artifact_built_UNVERIFIED' -or $journal.buildcookrun_exit -ne 0){throw 'Wrapper journal did not complete successfully'}
$postPath=Join-Path $seal.outputs.evidence 'postflight.json'
if(-not (Test-Path -LiteralPath $postPath -PathType Leaf)){throw 'No successful postflight; preserve outputs and inspect failure before retry'}
$post=Get-Content -LiteralPath $postPath -Raw | ConvertFrom-Json -AsHashtable -DateKind String
if($post.status -cne 'internal_smoke_artifact_built_UNVERIFIED' -or $post.release_eligible -ne $false){throw 'Wrong internal artifact receipt'}
$exe=[string]$post.next_iostore.exe
$pin=@($seal.tool_pins | Where-Object {$_.path -ceq $exe})
if($pin.Count -ne 1 -or (Get-FileHash -LiteralPath $exe).Hash -cne $pin[0].sha256){throw 'Official UnrealPak pin differs'}
foreach($container in $post.iostore_containers){if((Get-FileHash -LiteralPath $container.path).Hash -cne $container.sha256){throw 'Container changed before listing'}}
$info=[Diagnostics.ProcessStartInfo]::new()
$info.FileName=$exe;$info.WorkingDirectory=$seal.root;$info.UseShellExecute=$false;$info.CreateNoWindow=$true
$info.RedirectStandardOutput=$true;$info.RedirectStandardError=$true
foreach($arg in @($post.next_iostore.args)){$info.ArgumentList.Add([string]$arg)}
$proc=[Diagnostics.Process]::new();$proc.StartInfo=$info
$started=[datetime]::UtcNow.ToString('o')
if(-not $proc.Start()){throw 'UnrealPak did not start'}
$listingPid=$proc.Id;$startTicks=$proc.StartTime.ToUniversalTime().Ticks
$stdout=$proc.StandardOutput.ReadToEndAsync();$stderr=$proc.StandardError.ReadToEndAsync()
$proc.WaitForExit();$code=$proc.ExitCode
[IO.File]::WriteAllText((Join-Path $seal.outputs.evidence 'iostore-list.stdout.log'),$stdout.GetAwaiter().GetResult(),[Text.UTF8Encoding]::new($false))
[IO.File]::WriteAllText((Join-Path $seal.outputs.evidence 'iostore-list.stderr.log'),$stderr.GetAwaiter().GetResult(),[Text.UTF8Encoding]::new($false))
$proc.Dispose()
$csv=Join-Path $seal.outputs.evidence 'iostore.csv'
Write-FreshJson (Join-Path $seal.outputs.evidence 'iostore-listing-process.json') @{
 schema='R10-owned-official-iostore-listing-v1';exe=$exe;exe_sha256=$pin[0].sha256;arguments=@($post.next_iostore.args)
 pid=$listingPid;start_utc_ticks=$startTicks;started_utc=$started;ended_utc=[datetime]::UtcNow.ToString('o');exit_code=$code
 owner_process=$identity;no_gui=$true;no_game_execution=$true;csv=$csv
}
if($code -ne 0){throw ('Official IoStore listing failed; exit='+$code)}
& $python -I -B -X utf8 (Join-Path $packet 'internal_smoke.py') --seal $sealPath --seal-sha256 $sealSHA --release $releasePath --release-sha256 $releaseSHA --owner-id 'MapOwnerD' --verify-iostore $csv *> (Join-Path $seal.outputs.evidence 'iostore-verification.json')
if($LASTEXITCODE -ne 0){throw 'IoStore identity validation failed; retain evidence'}
Write-FreshJson (Join-Path $area 'execution-completed04.json') @{
 status='internal_artifact_and_iostore_identity_UNVERIFIED';release_eligible=$false;UI7='UNVERIFIED';visual_review='UNVERIFIED'
 game_execution=0;gui_operations=0;postflight=$postPath;iostore_csv=$csv;seal_sha256=$sealSHA
 ended_utc=[datetime]::UtcNow.ToString('o');owner_process=$identity
}
Get-Content -LiteralPath (Join-Path $area 'execution-completed04.json') -Raw
