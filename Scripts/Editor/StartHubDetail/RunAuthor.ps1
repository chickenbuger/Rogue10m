param(
    [Parameter(Mandatory)][ValidateSet('Plan', 'Apply')][string]$Phase,
    [Parameter(Mandatory)][string]$OutputDirectory,
    [string]$EngineRoot = 'D:\Program Files\UE_5.8'
)

$ErrorActionPreference = 'Stop'
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..')).Path
$editor = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$python = Join-Path $EngineRoot 'Engine\Binaries\ThirdParty\Python3\Win64\python.exe'
$project = Join-Path $projectRoot 'Rogue10m.uproject'
$taskOutput = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($OutputDirectory)
if (Test-Path -LiteralPath $taskOutput) { throw "Use a new output directory: $taskOutput" }
New-Item -ItemType Directory -Path $taskOutput | Out-Null
$taskLog = Join-Path $taskOutput 'author.log'
$taskUserDir = Join-Path $taskOutput 'User'
$env:ROGUE_START_HUB_DETAIL_REPORT = Join-Path $taskOutput 'author.json'
$taskScript = Join-Path $PSScriptRoot 'Author.py'
$arguments = @(
    ('"' + $project + '"'),
    '-run=pythonscript',
    ('"-Script=' + $taskScript + '"'),
    ('-RogueStartHubDetail' + $Phase),
    '-nullrhi', '-unattended', '-nosplash', '-nosound', '-NoLiveCoding', '-NoRemoteShaderCompile',
    '-SCCProvider=None', '-language=en', '-locale=en',
    '-ini:EditorPerProjectUserSettings:[/Script/ModelContextProtocolEngine.ModelContextProtocolSettings]:bAutoStartServer=False',
    ('"-UserDir=' + $taskUserDir + '"'),
    ('"-AbsLog=' + $taskLog + '"')
)
$process = Start-Process -FilePath $editor -ArgumentList $arguments -WorkingDirectory $projectRoot -WindowStyle Hidden -PassThru
Write-Output "StartHub $Phase PID $($process.Id), log $taskLog"
try {
    if (!$process.WaitForExit(380000)) { throw "Owned StartHub authoring commandlet timed out." }
    $editorExit = $process.ExitCode
} finally {
    if (!$process.HasExited) { Stop-Process -InputObject $process }
    Remove-Item Env:ROGUE_START_HUB_DETAIL_REPORT -ErrorAction SilentlyContinue
}
if (Test-Path -LiteralPath $taskLog) {
    & $python -B (Join-Path $projectRoot 'Scripts\CheckUnrealOutputLog.py') --log $taskLog --json-output (Join-Path $taskOutput 'log-check.json')
    $logCheckExit = $LASTEXITCODE
}
if ($editorExit -ne 0 -or !(Test-Path -LiteralPath (Join-Path $taskOutput 'author.json'))) {
    throw "StartHub $Phase did not complete. Editor exit $editorExit. See $taskLog."
}
exit $logCheckExit
