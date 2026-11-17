#requires -Version 7.0
[CmdletBinding()]
param(
 [ValidateSet('Development','Shipping')][string]$Configuration='Development',
 [string]$ArchiveDirectory='',
 [string]$EngineRoot='D:\Program Files\UE_5.8',
 [string]$ProjectRoot='',
 [string]$EvidenceDirectory='',
 [string[]]$Maps=@('/Game/Rogue10m/Maps/L_Menu','/Game/Rogue10m/Maps/StartHub','/Game/Rogue10m/Maps/OpenRunMap','/Game/Rogue10m/Maps/Prototypes/FiveVillageGraybox20261110'),
 [switch]$PreflightOnly
)
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
if(-not$ProjectRoot){$ProjectRoot=Split-Path -Parent $PSScriptRoot}
$root=(Resolve-Path -LiteralPath $ProjectRoot).Path
$project=Join-Path $root 'Rogue10m.uproject'
$uat=Join-Path $EngineRoot 'Engine\Build\BatchFiles\RunUAT.bat'
$stamp=[DateTime]::UtcNow.ToString('yyyyMMddTHHmmssfffZ')
if(-not$ArchiveDirectory){$ArchiveDirectory=Join-Path $root ('Build\Windows_'+$stamp)}
$ArchiveDirectory=[IO.Path]::GetFullPath($ArchiveDirectory)
if(-not$EvidenceDirectory){$EvidenceDirectory=Join-Path $root ('Feature\doc\evidence\windows-packaging-20261111\'+$stamp)}
$EvidenceDirectory=[IO.Path]::GetFullPath($EvidenceDirectory)
foreach($f in @($project,$uat,(Join-Path $root 'Source\Rogue10m.Target.cs'))){if(-not(Test-Path -LiteralPath $f -PathType Leaf)){throw ('Required Windows build input missing: '+$f)}}
if(Test-Path -LiteralPath $ArchiveDirectory){throw 'Choose a fresh ArchiveDirectory; prior builds are never overwritten or cleaned.'}
if(Test-Path -LiteralPath $EvidenceDirectory){throw 'Choose fresh EvidenceDirectory; prior evidence is preserved.'}
if(-not$Maps.Count-or@($Maps|Select-Object -Unique).Count-ne$Maps.Count){throw 'Require distinct explicit cook maps.'}
foreach($map in $Maps){
 if($map-notmatch '^/Game/[A-Za-z0-9_]+(?:/[A-Za-z0-9_]+)*$'){throw ('Require plain /Game package map path: '+$map)}
 if(-not(Test-Path -LiteralPath (Join-Path $root ('Content\'+$map.Substring(6).Replace('/','\')+'.umap')))){throw ('Cook map missing: '+$map)}
}
function AssertIdle {
 $busy=@(Get-CimInstance Win32_Process|Where-Object {$_.Name-like'UnrealEditor*'-or($_.Name-match'UnrealBuildTool|AutomationTool|MSBuild'-or($_.Name-eq'dotnet.exe'-and$_.CommandLine-match'UnrealBuildTool|AutomationTool'))})
 if($busy.Count){throw ('Coordinate exclusive Editor/build ownership first. PIDs='+($busy.ProcessId-join','))}
}
$arguments=@('BuildCookRun',('-project='+$project),'-noP4','-platform=Win64',('-clientconfig='+$Configuration),'-target=Rogue10m','-build','-cook','-stage','-pak','-iostore','-package','-prereqs','-archive',('-archivedirectory='+$ArchiveDirectory),('-map='+($Maps-join'+')),'-unattended','-utf8output','-installed','-AdditionalCookerOptions=-DisablePython -NoSound -NoRemoteShaderCompile -language=en -locale=en-US',('-CookOutputDir='+(Join-Path $EvidenceDirectory 'build-work\Cooked\Windows')),('-stagingdirectory='+(Join-Path $EvidenceDirectory 'build-work\Staged')))
AssertIdle
New-Item -ItemType Directory -Path $EvidenceDirectory|Out-Null
$record=[ordered]@{status='preflight_passed';project=$project;engine=$EngineRoot;target='Rogue10m';platform='Win64';configuration=$Configuration;maps=$Maps;archive=$ArchiveDirectory;preflight_only=[bool]$PreflightOnly;started_utc=[DateTime]::UtcNow.ToString('o');finished_utc=$null;sdk_exit=$null;buildcookrun_exit=$null;executable_files=@();failure=$null;editor_build_is_not_game_package=$true}
function Journal {$record|ConvertTo-Json -Depth 7|Set-Content -LiteralPath (Join-Path $EvidenceDirectory 'package-result.json') -Encoding utf8NoBOM}
Journal
$arguments|ConvertTo-Json|Set-Content -LiteralPath (Join-Path $EvidenceDirectory 'arguments.json') -Encoding utf8NoBOM
if($PreflightOnly){Write-Output ('RESULT=WINDOWS_PACKAGE_PREFLIGHT_PASSED target=Rogue10m Win64 maps='+$Maps.Count);return}
try {
 AssertIdle
 $record.status='verifying_installed_sdk';Journal
 # Verify only: never ask Turnkey to install or update SDK/software implicitly.
 & $uat Turnkey '-command=VerifySdk' '-platform=Win64' ('-project='+$project) '-unattended' '-utf8output' 2>&1|Tee-Object -FilePath (Join-Path $EvidenceDirectory 'sdk-verify.log')
 $record.sdk_exit=$LASTEXITCODE;Journal
 if($record.sdk_exit-ne0){throw ('Installed Win64 SDK verification failed: '+$record.sdk_exit)}
 AssertIdle;$record.status='building_cooking_staging_archiving';Journal
 & $uat @arguments 2>&1|Tee-Object -FilePath (Join-Path $EvidenceDirectory 'buildcookrun.log')
 $record.buildcookrun_exit=$LASTEXITCODE;Journal
 if($record.buildcookrun_exit-ne0){throw ('Win64 BuildCookRun failed: '+$record.buildcookrun_exit)}
 $exes=@(Get-ChildItem -LiteralPath $ArchiveDirectory -Recurse -File -Filter 'Rogue10m*.exe'|ForEach-Object{[pscustomobject]@{path=$_.FullName;bytes=$_.Length;sha256=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash}})
 if(-not$exes.Count){throw 'UAT returned success but the archive has no Rogue10m executable.'}
 $record.executable_files=$exes;$record.status='package_built_runtime_validation_pending';$record.finished_utc=[DateTime]::UtcNow.ToString('o');Journal
 Write-Output ('RESULT=WINDOWS_PACKAGE_BUILT archive='+$ArchiveDirectory+'; launch and packaged-content checks remain.')
} catch {$record.status='failed';$record.failure=$_.ToString();$record.finished_utc=[DateTime]::UtcNow.ToString('o');Journal;throw}
