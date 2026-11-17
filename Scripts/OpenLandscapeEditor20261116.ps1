$ErrorActionPreference = 'Stop'
$ProjectRoot = [System.IO.Path]::GetFullPath((Split-Path -Parent $PSScriptRoot)).TrimEnd('\')
if ($ProjectRoot -ne 'D:\Project\Rogue10m') { throw 'This launcher is restricted to the normal Rogue10m project.' }
$EditorExe = 'D:\Program Files\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$ProjectFile = Join-Path $ProjectRoot 'Rogue10m.uproject'
$Map = '/Game/Rogue10m/Maps/Production/WorldLayoutCompactCrop20261114A'
$Evidence = Join-Path $ProjectRoot 'Feature\doc\evidence\landscape-production-20261116\editor-recovery'
if (-not (Test-Path -LiteralPath $EditorExe) -or -not (Test-Path -LiteralPath $ProjectFile)) { throw 'Engine or project missing.' }
if (-not (Test-Path -LiteralPath (Join-Path $ProjectRoot 'Content\Rogue10m\Maps\Production\WorldLayoutCompactCrop20261114A.umap'))) { throw 'Existing candidate map is missing; no new map is generated.' }
$Active = @(Get-CimInstance Win32_Process | Where-Object {
    $_.Name -match '^UnrealEditor.*\.exe$' -or
    ($_.Name -eq 'dotnet.exe' -and $_.CommandLine -match 'UnrealBuildTool|AutomationTool') -or
    $_.Name -match '^(cl|link)\.exe$'
})
if ($Active.Count) { $Active | Select-Object ProcessId,Name,CommandLine | Format-Table -Wrap; throw 'An editor or build is active; no duplicate launched.' }
New-Item -ItemType Directory -Path $Evidence -Force | Out-Null
$LaunchRecord = Join-Path $Evidence 'normal-editor-launch.json'
if (Test-Path -LiteralPath $LaunchRecord) { throw 'A launch record already exists. Inspect that process rather than replaying the launch.' }
$Log = Join-Path $Evidence 'normal-editor.log'
if (Test-Path -LiteralPath $Log) { throw 'Startup log already exists; preserve it.' }
$Arguments = @(('"' + $ProjectFile + '"'), $Map, '-NoLiveCoding', '-NoSplash', '-NoSound',
    ('"-AbsLog=' + $Log + '"'), '"-ExecCmds=ModelContextProtocol.StartServer 8001"')
$EditorProcess = Start-Process -FilePath $EditorExe -ArgumentList $Arguments -WorkingDirectory $ProjectRoot -PassThru
[ordered]@{pid=$EditorProcess.Id;startedKst=(Get-Date -Format o);executable=$EditorExe;arguments=$Arguments;
    candidateMap=$Map;gameModeArgument=$false;liveCodingDisabledForSession=$true;startupVerified=$false} |
    ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $LaunchRecord -Encoding utf8
Write-Output ('Launched normal editor PID ' + $EditorProcess.Id + '; wait for read-only project/map/MCP verification.')
