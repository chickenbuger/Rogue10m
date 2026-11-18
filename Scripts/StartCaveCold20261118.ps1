param([string]$HostDirectory='cold-host')
$ErrorActionPreference='Stop'
$TaskRoot='D:\Project\Rogue10m'
$TaskExe='D:\Program Files\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
if($HostDirectory -notmatch '^cold-host(?:-[a-z0-9]+)?$'){throw 'Bounded cave host directory name required'}
$TaskOut=Join-Path $TaskRoot ('Feature\doc\evidence\caves20261118\'+$HostDirectory)
if(Test-Path -LiteralPath $TaskOut){throw 'Preserve existing cold evidence; no automatic retry'}
$TaskSaved=Get-Content -LiteralPath (Join-Path $TaskRoot 'Feature\doc\evidence\caves20261118\author-live\final-save-result.json') -Raw | ConvertFrom-Json
if(-not $TaskSaved.success){throw 'Successful bounded final save is required'}
$TaskExposureSaved=Get-Content -LiteralPath (Join-Path $TaskRoot 'Feature\doc\evidence\caves20261118\author-live\exposure-save-result.json') -Raw | ConvertFrom-Json
if(-not $TaskExposureSaved.success -or $TaskExposureSaved.requested_packages.Count -ne 2){throw 'Successful exact-two exposure checkpoint required'}
$TaskCameraSaved=Get-Content -LiteralPath (Join-Path $TaskRoot 'Feature\doc\evidence\caves20261118\author-live\camera-relief-save-result.json') -Raw | ConvertFrom-Json
if(-not $TaskCameraSaved.success -or $TaskCameraSaved.requested_packages.Count -ne 1){throw 'Successful exact-one Q camera relief checkpoint required'}
$TaskMetadata=Get-Content -LiteralPath (Join-Path $TaskRoot 'Feature\doc\evidence\caves20261118\cold-locale-metadata-preflight.json') -Raw | ConvertFrom-Json
$TaskColdSha=(Get-FileHash -LiteralPath (Join-Path $TaskRoot 'Scripts\Editor\ColdCaves20261118.py')).Hash.ToLowerInvariant()
if(-not $TaskMetadata.passed -or $TaskMetadata.cold_script_sha256 -ne $TaskColdSha){throw 'Same current cold helper must pass the live read-only metadata preflight first'}
New-Item -ItemType Directory -Path $TaskOut | Out-Null
$TaskArgs=@(('"'+$TaskRoot+'\Rogue10m.uproject"'),'/Game/Rogue10m/Maps/Production/WorldLayoutCompactCrop20261114A',
 '-NoLiveCoding','-NoSplash','-NoSound','-unattended','-nullrhi','-language=en','-locale=en','-RogueCaveStage=cold',
 '-ini:EditorPerProjectUserSettings:[/Script/UnrealEd.EditorLoadingSavingSettings]:bAutoSaveEnable=False',
 '-ini:EditorPerProjectUserSettings:[/Script/ModelContextProtocolEngine.ModelContextProtocolSettings]:bAutoStartServer=False',
 ('"-AbsLog='+(Join-Path $TaskOut 'engine.log')+'"'),
 '"-ExecCmds=py D:/Project/Rogue10m/Scripts/Editor/ColdCaves20261118.py"')
$TaskWatch=[Diagnostics.Stopwatch]::StartNew()
$TaskProcess=Start-Process -FilePath $TaskExe -ArgumentList $TaskArgs -WorkingDirectory $TaskRoot -WindowStyle Hidden -PassThru
[ordered]@{pid=$TaskProcess.Id;exe=$TaskExe;arguments=$TaskArgs;startedUtc=[DateTime]::UtcNow.ToString('o');
 dllSha256=(Get-FileHash -LiteralPath (Join-Path $TaskRoot 'Binaries\Win64\UnrealEditor-Rogue10m.dll')).Hash} |
 ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $TaskOut 'launch.json') -Encoding utf8
Write-Output ('Independent read-only cold PID '+$TaskProcess.Id)
while(-not $TaskProcess.HasExited){
 if($TaskWatch.Elapsed.TotalSeconds -gt 600){throw ('Cold validation watchdog exceeded; preserve and inspect owned PID '+$TaskProcess.Id)}
 Start-Sleep -Seconds 2
 $TaskProcess.Refresh()
}
$TaskProcess.WaitForExit()
[ordered]@{pid=$TaskProcess.Id;exit_code=$TaskProcess.ExitCode;exitCode=$TaskProcess.ExitCode;
 elapsed_seconds=$TaskWatch.Elapsed.TotalSeconds;observedUtc=[DateTime]::UtcNow.ToString('o');
 process_handle_retained=$true;assets_saved_by_launcher=$false} | ConvertTo-Json |
 Set-Content -LiteralPath (Join-Path $TaskOut 'process-exit.json') -Encoding utf8
Write-Output ('Cold process exit '+$TaskProcess.ExitCode)
if(-not (Test-Path -LiteralPath (Join-Path $TaskOut 'engine.log'))){throw 'Process ended before an Unreal log existed; not semantic validation'}
exit $TaskProcess.ExitCode
