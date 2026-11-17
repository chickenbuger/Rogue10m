param(
    [ValidateSet('scanner-tests','profile-tests','candidate')][string]$Phase = 'scanner-tests',
    [ValidatePattern('^[A-Za-z0-9-]+$')][string]$RunTag = 'first',
    [ValidateRange(30, 300)][int]$TimeoutSeconds = 180
)
$ErrorActionPreference = 'Stop'
$TaskRoot = 'D:\Project\Rogue10m'
$TaskOutput = Join-Path $TaskRoot ('Feature\doc\evidence\reference-scanner-20261116\' + $Phase + '-' + $RunTag)
if (Test-Path -LiteralPath $TaskOutput) { throw 'Existing evidence must be preserved. Use a fresh RunTag.' }
New-Item -ItemType Directory -Path $TaskOutput | Out-Null
$TaskMap = '/Engine/Maps/Entry'
if ($Phase -eq 'candidate') { $TaskMap = '/Game/Rogue10m/Maps/Production/WorldLayoutCompactCrop20261114A' }
$TaskArguments = @('"D:\Project\Rogue10m\Rogue10m.uproject"', $TaskMap,
    '-unattended','-nullrhi','-nosplash','-nosound','-nop4','-NoLiveCoding','-NoHotReload','-NoAutoSave',
    '-language=en','-locale=en',
    '-ini:EditorPerProjectUserSettings:[/Script/ModelContextProtocolEngine.ModelContextProtocolSettings]:bAutoStartServer=False',
    ('"-AbsLog=' + (Join-Path $TaskOutput 'editor.log') + '"'),
    ('"-UserDir=' + (Join-Path $TaskOutput 'User') + '"'),
    '-RogueProfileSaveSlot=Rogue10m_Town_QA_RefScan20261116',
    '-RogueProgressionSavePrefix=Rogue10m_Progression_QA_RefScan20261116',
    '-RogueSkillKeySaveSlot=Rogue10m_Fighter_QA_RefScan20261116')
if ($Phase -eq 'candidate') {
    $TaskArguments += @('"-ExecCmds=py ''D:/Project/Rogue10m/Scripts/Editor/ValidateReferenceScanner20261116.py''"', ('-RogueReferenceScanRun=inspection-' + $RunTag))
} else {
    $TaskFilter = if ($Phase -eq 'scanner-tests') { 'Rogue10m.Editor.ReferenceScan' } else { 'Rogue10m.CharacterProfile.PossessionInitialization' }
    $TaskArguments += @(('"-ExecCmds=Automation RunTests ' + $TaskFilter + '"'), '"-TestExit=Automation Test Queue Empty"',
        ('"-ReportExportPath=' + (Join-Path $TaskOutput 'automation-report') + '"'))
}
$TaskStarted = [DateTime]::UtcNow
$TaskProcess = Start-Process -FilePath 'D:\Program Files\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' -ArgumentList $TaskArguments -WorkingDirectory $TaskRoot -WindowStyle Hidden -PassThru
[ordered]@{pid=$TaskProcess.Id;phase=$Phase;arguments=$TaskArguments;startedUtc=$TaskStarted.ToString('o');timeoutSeconds=$TimeoutSeconds} |
    ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $TaskOutput 'launch.json') -Encoding utf8
Write-Output ("Started owned validation PID {0}; {1}" -f $TaskProcess.Id, $TaskOutput)
$TaskFinished = $TaskProcess.WaitForExit($TimeoutSeconds * 1000)
if (-not $TaskFinished) { & taskkill.exe /PID $TaskProcess.Id /T /F | Out-Null; $TaskProcess.WaitForExit(10000) | Out-Null }
$TaskProcess.Refresh()
$TaskResult = [ordered]@{pid=$TaskProcess.Id;finished=$TaskFinished;exitCode=$TaskProcess.ExitCode;elapsedSeconds=([DateTime]::UtcNow-$TaskStarted).TotalSeconds}
$TaskResult | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $TaskOutput 'process-result.json') -Encoding utf8
$TaskResult | ConvertTo-Json
if (-not $TaskFinished) { exit 124 }
if (-not (Test-Path -LiteralPath (Join-Path $TaskOutput 'editor.log'))) { throw 'No Unreal log was produced; process exit alone is not verification.' }
exit $TaskProcess.ExitCode
