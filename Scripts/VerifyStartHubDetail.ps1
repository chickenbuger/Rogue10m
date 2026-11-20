param(
    [Parameter(Mandatory)][string]$OutputDir,
    [string]$ProjectRoot = (Split-Path -Parent $PSScriptRoot),
    [string]$EngineRoot = 'D:\Program Files\UE_5.8',
    [string]$LayoutJson,
    [ValidateRange(30, 900)][int]$TimeoutSeconds = 240
)

$ErrorActionPreference = 'Stop'
$projectRootPath = (Resolve-Path -LiteralPath $ProjectRoot).Path
$projectFile = Join-Path $projectRootPath 'Rogue10m.uproject'
$editor = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$validator = Join-Path $PSScriptRoot 'Editor\ValidateSavedStartHub.py'
$outputPath = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($OutputDir)
foreach ($source in @($projectFile, $editor, $validator)) {
    if (-not (Test-Path -LiteralPath $source -PathType Leaf)) { throw "Missing required path: $source" }
}
if (Test-Path -LiteralPath $outputPath) { throw "Output directory already exists: $outputPath" }

$log = Join-Path $outputPath 'cold-editor.log'
$privateUserDir = Join-Path $outputPath 'private-user'
$report = Join-Path $outputPath 'cold-validation.json'
$arguments = @(
    ('"' + $projectFile + '"'), '-run=pythonscript', ('-Script="' + $validator + '"'),
    '-nullrhi', '-unattended', '-nosplash', '-nosound', '-NoLiveCoding', '-NoRemoteShaderCompile',
    '-SCCProvider=None', '-language=en', '-locale=en',
    '-ini:EditorPerProjectUserSettings:[/Script/ModelContextProtocolEngine.ModelContextProtocolSettings]:bAutoStartServer=False',
    ('-RogueStartHubQaOutput="' + $outputPath + '"'), ('-UserDir="' + $privateUserDir + '"'), ('-AbsLog="' + $log + '"'))
if ($LayoutJson) {
    $layoutPath = (Resolve-Path -LiteralPath $LayoutJson).Path
    if (-not (Test-Path -LiteralPath $layoutPath -PathType Leaf)) { throw "Layout JSON must be a file: $layoutPath" }
    $arguments += ('-RogueStartHubQaLayout="' + $layoutPath + '"')
}

[void](New-Item -ItemType Directory -Path $outputPath)
$process = Start-Process -FilePath $editor -ArgumentList $arguments -WorkingDirectory $projectRootPath -WindowStyle Hidden -PassThru
Write-Output "StartHub cold QA PID=$($process.Id) log=$log report=$report"
try {
    if (-not $process.WaitForExit($TimeoutSeconds * 1000)) { throw "StartHub commandlet timed out. Inspect $log" }
    if (-not (Test-Path -LiteralPath $report -PathType Leaf)) {
        throw "Commandlet exit=$($process.ExitCode) without a report. Inspect $log"
    }
    $result = Get-Content -LiteralPath $report -Raw -Encoding utf8 | ConvertFrom-Json
    if ($process.ExitCode -ne 0 -or -not $result.passed) {
        $failed = @($result.checks | Where-Object { -not $_.passed } | ForEach-Object { $_.name }) -join ', '
        throw "Commandlet exit=$($process.ExitCode) failed: $failed. Inspect $report and $log"
    }
    Write-Output "START_HUB_DETAIL_COLD_QA_PASSED checks=$($result.checks.Count)"
} finally {
    if (-not $process.HasExited) {
        Stop-Process -InputObject $process
        [void]$process.WaitForExit(5000)
    }
}
