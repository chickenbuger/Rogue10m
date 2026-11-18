param(
    [Parameter(Mandatory=$true)][ValidatePattern('^[A-Za-z0-9_]{1,20}$')][string]$Label,
    [switch]$Interactive,
    [switch]$Hold,
    [ValidateRange(640,3840)][int]$Width = 1920,
    [ValidateRange(480,2160)][int]$Height = 1080,
    [string]$PythonExecutable
)
$ErrorActionPreference = 'Stop'
$TaskRoot = [IO.Path]::GetFullPath((Split-Path -Parent $PSScriptRoot)).TrimEnd('\')
$TaskExe = 'D:\Program Files\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$TaskPython = if ($PythonExecutable) { $PythonExecutable } else { Join-Path $TaskRoot 'tmp\blender-mcp-venv\Scripts\python.exe' }
if (!(Test-Path -LiteralPath $TaskPython -PathType Leaf)) {
    throw 'Python 3 is required for the log check. Pass the Python executable path with -PythonExecutable.'
}
$TaskPython = (Resolve-Path -LiteralPath $TaskPython).Path
$TaskSaveRoot = Join-Path $TaskRoot 'Saved\SaveGames'
$TaskRecordRoot = Join-Path $TaskRoot ('tmp\CharacterLobbyStyle20261119\' + $Label)
$TaskDriverRoot = Join-Path $TaskRoot ('tmp\CharacterLobbyStyleQA\' + $Label)
$TaskProfile = 'Rogue10m_Town_QA_LobbyStyle_' + $Label
$TaskPrefix = 'Rogue10m_Progression_QA_LobbyStyle_' + $Label
$TaskKeys = 'Rogue10m_Fighter_QA_LobbyStyle_' + $Label
function Write-TaskJson([string]$Path, $Value) {
    [IO.File]::WriteAllText($Path, (ConvertTo-Json -InputObject $Value -Depth 12), [Text.UTF8Encoding]::new($false))
}
function Assert-TaskPhysical([string]$Path) {
    $TaskEntry = Get-Item -LiteralPath $Path -Force
    while ($null -ne $TaskEntry) {
        if (($TaskEntry.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) { throw ('Linked path: ' + $TaskEntry.FullName) }
        $TaskEntry = $TaskEntry.Parent
    }
}
function Get-TaskSaves {
    return @(Get-ChildItem -LiteralPath $TaskSaveRoot -Recurse -File -Filter '*.sav' | ForEach-Object {
        if (($_.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) { throw 'Linked save file' }
        [pscustomobject]@{Path=$_.FullName;Bytes=$_.Length;SHA256=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash}
    } | Sort-Object Path)
}
Assert-TaskPhysical $TaskRoot
Assert-TaskPhysical (Join-Path $TaskRoot 'Content')
Assert-TaskPhysical $TaskSaveRoot
if ((Test-Path -LiteralPath $TaskRecordRoot) -or (Test-Path -LiteralPath $TaskDriverRoot)) { throw 'Choose a fresh label; existing evidence is preserved.' }
$TaskBefore = @(Get-TaskSaves)
if (@($TaskBefore | Where-Object { [IO.Path]::GetFileName($_.Path) -eq ($TaskProfile+'.sav') -or [IO.Path]::GetFileName($_.Path) -eq ($TaskKeys+'.sav') -or [IO.Path]::GetFileName($_.Path).StartsWith($TaskPrefix+'_',[StringComparison]::OrdinalIgnoreCase) }).Count) { throw 'QA save namespace already exists.' }
New-Item -ItemType Directory -Path $TaskRecordRoot -Force | Out-Null
Assert-TaskPhysical $TaskRecordRoot
Write-TaskJson (Join-Path $TaskRecordRoot 'before-saves.json') $TaskBefore
$TaskLog = Join-Path $TaskRecordRoot 'game.log'
$TaskArguments = @(('"'+(Join-Path $TaskRoot 'Rogue10m.uproject')+'"'), '/Game/Rogue10m/Maps/StartHub?game=/Script/Rogue10m.Rogue10mMenuGameMode',
    '-game','-windowed','-ForceRes',('-ResX='+$Width),('-ResY='+$Height),'-nosplash','-nosound','-NoLiveCoding',
    '-NoRemoteShaderCompile','-DisablePython','-language=en','-locale=en','-SCCProvider=None',
    '-ini:EditorPerProjectUserSettings:[/Script/ModelContextProtocolEngine.ModelContextProtocolSettings]:bAutoStartServer=False',
    ('-RogueProfileSaveSlot='+$TaskProfile),('-RogueProgressionSavePrefix='+$TaskPrefix),('-RogueSkillKeySaveSlot='+$TaskKeys),
    '-RogueCharacterLobbyStyleQA',('-RogueCharacterLobbyStyleLabel='+$Label),('-RogueCharacterLobbyStyleOutput="'+$TaskDriverRoot+'"'),
    ('-AbsLog="'+$TaskLog+'"'))
# Native Slate hit-path validation needs a real game window, even for automation.
if (-not $Interactive) { $TaskArguments += '-unattended' }
if ($Hold) { $TaskArguments += '-RogueCharacterLobbyStyleHold' }
$TaskRecord = [ordered]@{Label=$Label;Scope='programmatic_rendered_lobby_ui';PhysicalInput='not_claimed';StartedUtc=[DateTime]::UtcNow.ToString('o');OwnedPID=$null;ExitCode=$null;ExistingSaves=$TaskBefore.Count;Preserved=$false;Error=$null;Log=$TaskLog}
Write-TaskJson (Join-Path $TaskRecordRoot 'session.json') $TaskRecord
$TaskProcess = $null
try {
    $TaskWindowStyle = if ($Interactive) { 'Normal' } else { 'Hidden' }
    $TaskProcess = Start-Process -FilePath $TaskExe -ArgumentList $TaskArguments -WindowStyle $TaskWindowStyle -PassThru
    $TaskRecord.OwnedPID = $TaskProcess.Id
    Write-TaskJson (Join-Path $TaskRecordRoot 'session.json') $TaskRecord
    Write-Output ('CHARACTER_LOBBY_QA_LAUNCHED pid='+$TaskProcess.Id+' log='+$TaskLog)
    $TaskDeadline = [DateTime]::UtcNow.AddSeconds(240)
    while (-not $TaskProcess.WaitForExit(1000)) {
        if ([DateTime]::UtcNow -gt $TaskDeadline) { throw 'Owned QA process exceeded240 seconds.' }
    }
    $TaskRecord.ExitCode = $TaskProcess.ExitCode
    if ($TaskProcess.ExitCode -ne 0) { throw ('QA process exited '+$TaskProcess.ExitCode) }
} catch {
    $TaskRecord.Error = $_.Exception.Message
} finally {
    if ($null -ne $TaskProcess -and -not $TaskProcess.HasExited) { $TaskProcess.Kill(); $TaskProcess.WaitForExit(10000) | Out-Null }
    if ($null -ne $TaskProcess -and $TaskProcess.HasExited) { $TaskRecord.ExitCode = $TaskProcess.ExitCode }
    $TaskAfter = @(Get-TaskSaves)
    $TaskChanged = @()
    foreach ($TaskOld in $TaskBefore) {
        $TaskNew = @($TaskAfter | Where-Object Path -CEQ $TaskOld.Path)
        if ($TaskNew.Count -ne 1 -or $TaskNew[0].SHA256 -cne $TaskOld.SHA256 -or $TaskNew[0].Bytes -ne $TaskOld.Bytes) { $TaskChanged += $TaskOld.Path }
    }
    $TaskAdded = @($TaskAfter | Where-Object { $_.Path -cnotin $TaskBefore.Path })
    $TaskRecord.Preserved = $TaskChanged.Count -eq 0
    Write-TaskJson (Join-Path $TaskRecordRoot 'save-preservation.json') ([ordered]@{Before=$TaskBefore.Count;After=$TaskAfter.Count;ChangedExisting=$TaskChanged;Added=$TaskAdded;NoRestoreOrDeletePerformed=$true})
    & $TaskPython -X utf8 -B (Join-Path $TaskRoot 'Scripts\CheckUnrealOutputLog.py') --log $TaskLog --after-line 0 --json-output (Join-Path $TaskRecordRoot 'output-log-check.json')
    $TaskRecord.LogCheckExitCode = $LASTEXITCODE
    $TaskSummaryPath = Join-Path $TaskDriverRoot 'summary.txt'
    $TaskSummary = @{}
    if (Test-Path -LiteralPath $TaskSummaryPath) {
        foreach ($TaskLine in [IO.File]::ReadAllLines($TaskSummaryPath)) {
            $TaskEquals = $TaskLine.IndexOf('=')
            if ($TaskEquals -lt 1) { continue }
            $TaskKey = $TaskLine.Substring(0, $TaskEquals)
            if ($TaskSummary.ContainsKey($TaskKey)) { throw 'Duplicate driver summary key' }
            $TaskSummary[$TaskKey] = $TaskLine.Substring($TaskEquals + 1)
        }
    }
    $TaskRecord.DriverSummary = $TaskSummary
    $TaskCases = @(if (Test-Path -LiteralPath (Join-Path $TaskDriverRoot 'checks.csv')) { Import-Csv -LiteralPath (Join-Path $TaskDriverRoot 'checks.csv') -Encoding utf8 })
    $TaskRecord.DriverPassed = $TaskSummary['state'] -ceq 'passed' -and $TaskSummary['failures'] -ceq '0' -and $TaskSummary['checks'] -match '^[1-9][0-9]*$' -and $TaskCases.Count -eq [int]$TaskSummary['checks'] -and @($TaskCases | Where-Object result -CNE 'PASS').Count -eq 0
    Write-TaskJson (Join-Path $TaskRecordRoot 'session.json') $TaskRecord
}
Write-Output (ConvertTo-Json -InputObject $TaskRecord -Depth 5)
if ($TaskRecord.Error -or -not $TaskRecord.Preserved -or -not $TaskRecord.DriverPassed -or $TaskRecord.ExitCode -ne 0 -or $TaskRecord.LogCheckExitCode -ne 0) { exit 1 }
exit 0
