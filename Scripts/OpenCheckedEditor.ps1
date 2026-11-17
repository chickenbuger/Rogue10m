param(
    [string]$Map = '',
    [ValidatePattern('^[A-Za-z0-9-]+$')][string]$EditorLanguage = 'ko',
    [ValidatePattern('^[A-Za-z0-9-]+$')][string]$EditorLocale = 'ko-KR'
)
$ErrorActionPreference = 'Stop'
$TaskProjectRoot = [IO.Path]::GetFullPath((Split-Path -Parent $PSScriptRoot)).TrimEnd('\')
$TaskProject = Join-Path $TaskProjectRoot 'Rogue10m.uproject'
$TaskEditor = 'D:\Program Files\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$TaskRestore = Join-Path $PSScriptRoot 'Editor\RestoreEditorCultureAfterSmoke.py'
foreach ($TaskPath in @($TaskProject, $TaskEditor, $TaskRestore)) {
    if (-not (Test-Path -LiteralPath $TaskPath)) { throw "Required file missing: $TaskPath" }
}
foreach ($TaskPath in @($TaskProjectRoot, (Join-Path $TaskProjectRoot 'Content'))) {
    if ((Get-Item -LiteralPath $TaskPath).Attributes -band [IO.FileAttributes]::ReparsePoint) { throw 'Use a physical project checkout.' }
}
if ($Map -and $Map -notmatch '^/(Game|Engine)/[A-Za-z0-9_/]+$') { throw 'Map must be an Unreal package path.' }
function Test-RogueProjectSession($TaskCandidate, [string]$TaskRoot) {
    if ($TaskCandidate.Name -notmatch '^(UnrealEditor.*|dotnet|UnrealBuildTool|AutomationTool)\.exe$') { return $false }
    if ([string]::IsNullOrWhiteSpace($TaskCandidate.CommandLine)) { return $true }
    $TaskCommandLine = $TaskCandidate.CommandLine.Replace('/', '\')
    $TaskNormalizedRoot = $TaskRoot.Replace('/', '\')
    return $TaskCommandLine.Contains($TaskNormalizedRoot, [StringComparison]::OrdinalIgnoreCase)
}
$TaskActive = @(Get-CimInstance Win32_Process | Where-Object { Test-RogueProjectSession $_ $TaskProjectRoot })
if ($TaskActive.Count) { throw 'This project already has an editor/build session; preserve it instead of opening a duplicate.' }
$TaskEvidence = Join-Path $TaskProjectRoot ('Feature\doc\evidence\output-log-errors-20261116\editor-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
New-Item -ItemType Directory -Path $TaskEvidence | Out-Null
$TaskLog = Join-Path $TaskEvidence 'editor.log'
# Keep all startup smoke tests enabled. Their pinned English expectations need en;
# the deferred Python command restores only this editor session to the requested UI culture.
$TaskExec = "py '" + ($TaskRestore -replace '\\', '/') + "'"
$TaskArguments = @(('"' + $TaskProject + '"'))
if ($Map) { $TaskArguments += $Map }
$TaskArguments += @('-NoLiveCoding', '-NoSplash', '-language=en', '-locale=en',
    ('-RogueEditorLanguage=' + $EditorLanguage), ('-RogueEditorLocale=' + $EditorLocale),
    '"-LogCmds=LogAutomationTest Log"', ('"-AbsLog=' + $TaskLog + '"'),
    ('"-ExecCmds=' + $TaskExec + '"'))
$TaskProcess = Start-Process -FilePath $TaskEditor -ArgumentList $TaskArguments -WorkingDirectory $TaskProjectRoot -WindowStyle Normal -PassThru
[ordered]@{pid=$TaskProcess.Id;log=$TaskLog;arguments=$TaskArguments;startedUtc=[DateTime]::UtcNow.ToString('o');startupVerified=$false} |
    ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $TaskEvidence 'launch.json') -Encoding utf8
Write-Output ("Editor PID {0}; verify the startup and ROGUE_EDITOR_CULTURE record in: {1}" -f $TaskProcess.Id, $TaskLog)
Write-Output ('Use CheckUnrealOutputLog.py --log "' + $TaskLog + '" --watch --duration-seconds 60 during development.')
