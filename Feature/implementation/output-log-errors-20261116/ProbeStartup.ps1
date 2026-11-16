param(
    [ValidateSet('en', 'ko')][string]$Culture = 'en',
    [ValidateRange(15, 180)][int]$TimeoutSeconds = 60,
    [ValidatePattern('^[a-zA-Z0-9-]+$')][string]$RunTag = 'sandbox',
    [switch]$RestoreKorean
)
$ErrorActionPreference = 'Stop'
$TaskProjectRoot = 'D:\Project\Rogue10m'
$TaskOutput = Join-Path $TaskProjectRoot ('Feature\doc\evidence\output-log-errors-20261116\startup-' + $Culture + '-' + $RunTag)
if (Test-Path -LiteralPath $TaskOutput) { throw 'Preserve existing diagnostic output; choose another run explicitly.' }
New-Item -ItemType Directory -Path $TaskOutput | Out-Null
$TaskEditor = 'D:\Program Files\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$TaskExec = 'QUIT_EDITOR'
if ($RestoreKorean) {
    if ($Culture -ne 'en') { throw 'Restore probe must start with English smoke tests.' }
    $TaskExec = "py 'D:/Project/Rogue10m/Scripts/Editor/RestoreEditorCultureAfterSmoke.py',QUIT_EDITOR"
}
$TaskArguments = @(
    '"D:\Project\Rogue10m\Rogue10m.uproject"', '/Engine/Maps/Entry',
    '-unattended', '-nullrhi', '-nosound', '-nosplash', '-nop4', '-NoLiveCoding', '-NoHotReload', '-NoAutoSave',
    ('-language=' + $Culture), ('-locale=' + $Culture),
    '"-LogCmds=LogAutomationTest Log"', ('"-ExecCmds=' + $TaskExec + '"'),
    '-ini:EditorPerProjectUserSettings:[/Script/ModelContextProtocolEngine.ModelContextProtocolSettings]:bAutoStartServer=False',
    ('"-UserDir=' + (Join-Path $TaskOutput 'User') + '"'),
    ('"-AbsLog=' + (Join-Path $TaskOutput 'startup.log') + '"')
)
if ($RestoreKorean) { $TaskArguments += @('-RogueEditorLanguage=ko', '-RogueEditorLocale=ko-KR') }
$TaskStarted = [DateTime]::UtcNow
$TaskProcess = Start-Process -FilePath $TaskEditor -ArgumentList $TaskArguments -WorkingDirectory $TaskProjectRoot -WindowStyle Hidden -PassThru
[ordered]@{pid=$TaskProcess.Id;startedUtc=$TaskStarted.ToString('o');executable=$TaskEditor;arguments=$TaskArguments;timeoutSeconds=$TimeoutSeconds} |
    ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $TaskOutput 'launch.json') -Encoding utf8
$TaskFinished = $TaskProcess.WaitForExit($TimeoutSeconds * 1000)
if (-not $TaskFinished) { & taskkill.exe /PID $TaskProcess.Id /T /F | Out-Null; $TaskProcess.WaitForExit(10000) | Out-Null }
$TaskProcess.Refresh()
$TaskLogExists = Test-Path -LiteralPath (Join-Path $TaskOutput 'startup.log')
[ordered]@{pid=$TaskProcess.Id;culture=$Culture;finished=$TaskFinished;exitCode=$TaskProcess.ExitCode;logExists=$TaskLogExists;elapsedSeconds=([DateTime]::UtcNow-$TaskStarted).TotalSeconds} |
    ConvertTo-Json | Set-Content -LiteralPath (Join-Path $TaskOutput 'result.json') -Encoding utf8
Get-Content -LiteralPath (Join-Path $TaskOutput 'result.json')
if (-not $TaskFinished) { exit 124 }
if (-not $TaskLogExists) { Write-Error 'No Unreal diagnostic log was produced. Process exit alone does not prove engine startup.'; exit 2 }
exit $TaskProcess.ExitCode
