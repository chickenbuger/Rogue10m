#requires -Version 7.0
<#
Launches only a new read-only, null-RHI editor and returns immediately.
The caller keeps the allowed visible editor idle (no PIE, edits or saves)
until Poll reports that the owned headless process is absent. Poll enforces
the 600-second deadline only against the exact process this file launched.
Neither mode closes, stops or changes the existing visible editor.
#>
[CmdletBinding()]
param(
    [ValidateSet('Launch','Poll')][string]$Mode = 'Launch',
    [ValidatePattern('^[A-Za-z0-9-]{1,64}$')][string]$RunTag = 'RoadSurface20261117A',
    [ValidateRange(0,2147483647)][int]$AllowedIdleEditorPid = 0,
    [string]$SaveProof = '',
    [string]$IdleProof = '',
    [string]$DiskProof = 'Feature/doc/evidence/landscape-resume-20261117/road-save-completed-files.json'
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$TaskRoot = [IO.Path]::GetFullPath((Split-Path -Parent $PSScriptRoot)).TrimEnd('\')
if ($TaskRoot -ine 'D:\Project\Rogue10m') { throw 'Exact physical Rogue10m checkout required.' }
$TaskTarget = '/Game/Rogue10m/Maps/Production/WorldLayoutCompactCrop20261114A'
$TaskMaterial = '/Game/Rogue10m/World/Production/WorldLayoutCompactCrop20261114A/RoadSurface20261117/M_RoadSurface'
$TaskEditor = 'D:\Program Files\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$TaskVisibleEditor = 'D:\Program Files\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$TaskProject = Join-Path $TaskRoot 'Rogue10m.uproject'
$TaskLoader = Join-Path $TaskRoot 'Scripts\Editor\ColdVerifyLandscapeRoadSurface20261117.py'
$TaskVerifier = Join-Path $TaskRoot 'Scripts\Editor\VerifyLandscapeRoadSurface20261117.py'
$TaskRoad = Join-Path $TaskRoot 'Feature\doc\evidence\landscape-road-surface20261117'
$TaskResume = Join-Path $TaskRoot 'Feature\doc\evidence\landscape-resume-20261117'
$TaskOwnerLaunch = Join-Path $TaskResume 'workbench\launch.json'
$TaskOut = Join-Path $TaskRoad ('cold-headless-' + $RunTag)
$TaskLaunch = Join-Path $TaskOut 'launch.json'
$TaskLoaderResult = Join-Path $TaskRoad ('cold-loader-' + $RunTag + '\result.json')
$TaskComparison = Join-Path $TaskRoad 'final-cold\comparison.json'
$TaskFolderRelative = 'Content/__ExternalObjects__/Rogue10m/Maps/Production/WorldLayoutCompactCrop20261114A/4/U0/7LSR0EPX24RNQB6FXJ4SNF.uasset'
$TaskUserApproval = '현재 에디터 변경 패키지 전체 저장 후 검증 승인'

function Assert-PhysicalPath([string]$Path) {
    $TaskCursor = [IO.Path]::GetFullPath($Path)
    while ($TaskCursor) {
        if (Test-Path -LiteralPath $TaskCursor) {
            if ((Get-Item -LiteralPath $TaskCursor -Force).Attributes -band [IO.FileAttributes]::ReparsePoint) {
                throw ('Reparse/symlink path is forbidden: ' + $TaskCursor)
            }
        }
        $TaskParent = [IO.Path]::GetDirectoryName($TaskCursor)
        if ($TaskParent -eq $TaskCursor) { break }
        $TaskCursor = $TaskParent
    }
}
function Resolve-EvidenceFile([string]$Path) {
    if ([string]::IsNullOrWhiteSpace($Path)) { throw 'A concrete owner-supplied MCP proof file is required.' }
    $TaskAbsolute = if ([IO.Path]::IsPathRooted($Path)) { [IO.Path]::GetFullPath($Path) } else { [IO.Path]::GetFullPath((Join-Path $TaskRoot $Path)) }
    $TaskEvidenceRoot = (Join-Path $TaskRoot 'Feature\doc\evidence') + '\'
    if (-not $TaskAbsolute.StartsWith($TaskEvidenceRoot,[StringComparison]::OrdinalIgnoreCase)) { throw 'Proof must be within project evidence.' }
    Assert-PhysicalPath $TaskAbsolute
    if (-not (Test-Path -LiteralPath $TaskAbsolute -PathType Leaf)) { throw ('Missing proof: ' + $TaskAbsolute) }
    return $TaskAbsolute
}
function Read-Json([string]$Path) {
    # Keep ISO timestamps intact. Re-parsing a ConvertFrom-Json DateTime after
    # its implicit string conversion loses the offset and adds nine hours.
    return Get-Content -LiteralPath $Path -Raw -Encoding UTF8 | ConvertFrom-Json -AsHashtable -DateKind String
}
function Read-McpReturn([string]$Path) {
    $TaskDocument = Read-Json $Path
    if ($TaskDocument['jsonrpc'] -ne '2.0' -or $TaskDocument.ContainsKey('error') -or -not $TaskDocument.ContainsKey('result')) { throw 'Invalid/error JSON-RPC proof.' }
    $TaskResult = $TaskDocument['result']
    if ($TaskResult.ContainsKey('isError') -and $TaskResult['isError'] -ne $false) { throw 'MCP proof reports an error.' }
    $TaskText = @($TaskResult['content'] | Where-Object { $_['type'] -eq 'text' })
    if ($TaskText.Count -ne 1 -or @($TaskResult['content']).Count -ne 1) { throw 'One unambiguous MCP text result is required.' }
    $TaskValue = $TaskText[0]['text'] | ConvertFrom-Json -AsHashtable
    if (-not $TaskValue.ContainsKey('returnValue')) { throw 'Missing MCP returnValue.' }
    $TaskReturn = $TaskValue['returnValue']
    if ($TaskReturn -is [string]) { return $TaskReturn | ConvertFrom-Json -AsHashtable }
    return $TaskReturn
}
function Write-JsonNew([string]$Path, $Value) {
    $TaskStream = [IO.FileStream]::new($Path,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::Read)
    try {
        $TaskWriter = [IO.StreamWriter]::new($TaskStream,[Text.UTF8Encoding]::new($false))
        try { $TaskWriter.WriteLine(($Value | ConvertTo-Json -Depth 16)); $TaskWriter.Flush() } finally { $TaskWriter.Dispose() }
    } finally { $TaskStream.Dispose() }
}
function Get-Fingerprint([string]$Path) {
    return [ordered]@{path=$Path;bytes=(Get-Item -LiteralPath $Path).Length;sha256=(Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash}
}
function Assert-OwnedHeadless($Process, $Record) {
    $TaskExpectedTime = [DateTimeOffset]::Parse($Record['processStartUtc']).UtcDateTime
    $TaskObservedTime = ([DateTime]$Process.CreationDate).ToUniversalTime()
    $TaskCommand = [string]$Process.CommandLine
    if ([int]$Process.ProcessId -ne [int]$Record['pid'] -or $Process.Name -ine 'UnrealEditor-Cmd.exe' -or
        [IO.Path]::GetFullPath([string]$Process.ExecutablePath) -ine $TaskEditor -or
        [Math]::Abs(($TaskObservedTime-$TaskExpectedTime).TotalSeconds) -gt 2 -or
        @([regex]::Matches($TaskCommand,'(?:^|\s)-RogueRoadSurfaceColdRun=' + [regex]::Escape($RunTag) + '(?=\s|$)')).Count -ne 1 -or
        -not $TaskCommand.Contains([string]$Record['log'],[StringComparison]::OrdinalIgnoreCase) -or
        -not $TaskCommand.Replace('/','\').Contains($TaskProject,[StringComparison]::OrdinalIgnoreCase)) {
        throw 'PID identity changed or is not this exact owned headless process; no stop is allowed.'
    }
}

foreach ($TaskPath in @($TaskRoot,(Join-Path $TaskRoot 'Content'),$TaskOut,$TaskLaunch,$TaskLoaderResult,$TaskComparison)) { Assert-PhysicalPath $TaskPath }
if ($Mode -eq 'Poll') {
    if (-not (Test-Path -LiteralPath $TaskLaunch -PathType Leaf)) { throw 'Launch record is absent; Poll cannot adopt a process.' }
    $TaskRecord = Read-Json $TaskLaunch
    if ($TaskRecord['runTag'] -cne $RunTag -or $TaskRecord['executable'] -ine $TaskEditor -or $TaskRecord['project'] -ine $TaskRoot) { throw 'Launch record scope mismatch.' }
    $TaskOwned = @(Get-CimInstance Win32_Process -Filter ('ProcessId=' + [int]$TaskRecord['pid']))
    $TaskPoll = [ordered]@{observedUtc=[DateTime]::UtcNow.ToString('o');pid=[int]$TaskRecord['pid'];runTag=$RunTag;deadlineUtc=$TaskRecord['deadlineUtc'];visibleEditorTouched=$false;waited=$false;exitCode=$null}
    if ($TaskOwned.Count) {
        Assert-OwnedHeadless $TaskOwned[0] $TaskRecord
        if ([DateTimeOffset]::UtcNow -ge [DateTimeOffset]::Parse($TaskRecord['deadlineUtc'])) {
            $TaskPoll['status']='failed_owned_process_timed_out';$TaskPoll['passed']=$false
            # Recheck immediately before stopping only our own Cmd process.
            $TaskRecheck = @(Get-CimInstance Win32_Process -Filter ('ProcessId=' + [int]$TaskRecord['pid']))
            if ($TaskRecheck.Count) { Assert-OwnedHeadless $TaskRecheck[0] $TaskRecord; Stop-Process -Id ([int]$TaskRecord['pid']) -ErrorAction Stop }
            $TaskPoll['ownedHeadlessStopped']=$true
        } else { $TaskPoll['status']='running';$TaskPoll['passed']=$null }
    } else {
        $TaskLoaderProof = if (Test-Path -LiteralPath $TaskLoaderResult -PathType Leaf) { Read-Json $TaskLoaderResult } else { @{} }
        $TaskFinalProof = if (Test-Path -LiteralPath $TaskComparison -PathType Leaf) { Read-Json $TaskComparison } else { @{} }
        $TaskBeforeDeadline=($TaskLoaderProof.ContainsKey('finished_utc') -and
            [DateTimeOffset]::Parse($TaskLoaderProof['finished_utc']) -le [DateTimeOffset]::Parse($TaskRecord['deadlineUtc']))
        $TaskPoll['verificationPassed']=($TaskLoaderProof['passed'] -eq $true -and $TaskFinalProof['passed'] -eq $true -and
            $TaskLoaderProof['phase'] -ceq 'complete' -and $TaskFinalProof['status'] -ceq 'passed_read_only_saved_data' -and
            [int]$TaskLoaderProof['process_id'] -eq [int]$TaskRecord['pid'] -and [int]$TaskFinalProof['process_id'] -eq [int]$TaskRecord['pid'] -and $TaskBeforeDeadline)
        $TaskPoll['completedBeforeDeadline']=$TaskBeforeDeadline
        $TaskPoll['status']=if ($TaskPoll['verificationPassed']) { 'strict_data_passed_process_absent_exit_code_unobserved' } else { 'failed_or_missing_verification_process_absent' }
        $TaskPoll['passed']=$TaskPoll['verificationPassed']
        $TaskPoll['exitCodeNote']='Launch returned immediately; a later poll cannot recover the exited process handle. Inspect preserved engine log and strict proofs; no exit-code claim.'
    }
    $TaskPollPath = Join-Path $TaskOut ('poll-' + [DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss-fffffff') + '.json')
    Write-JsonNew $TaskPollPath $TaskPoll
    $TaskPoll | ConvertTo-Json -Depth 8
    return
}

if ($AllowedIdleEditorPid -le 0) { throw 'Pass the exact owner-confirmed idle workbench PID; no editor is adopted implicitly.' }
$TaskSavePath = Resolve-EvidenceFile $SaveProof
$TaskIdlePath = Resolve-EvidenceFile $IdleProof
$TaskDiskPath = Resolve-EvidenceFile $DiskProof
$TaskSave = Read-McpReturn $TaskSavePath
$TaskIdle = Read-McpReturn $TaskIdlePath
$TaskDisk = Read-Json $TaskDiskPath
if ($TaskIdle -isnot [bool] -or $TaskIdle -ne $false) { throw 'Fresh direct IsPIERunning:false MCP proof is required.' }
$TaskNow = [DateTime]::UtcNow
$TaskIdleTime = (Get-Item -LiteralPath $TaskIdlePath).LastWriteTimeUtc
$TaskSaveTime = (Get-Item -LiteralPath $TaskSavePath).LastWriteTimeUtc
$TaskDiskTime = (Get-Item -LiteralPath $TaskDiskPath).LastWriteTimeUtc
if (($TaskNow-$TaskIdleTime).TotalSeconds -gt 120 -or ($TaskNow-$TaskIdleTime).TotalSeconds -lt -5 -or $TaskIdleTime -lt $TaskDiskTime -or $TaskDiskTime -lt $TaskSaveTime) { throw 'Idle proof must be fresh (120s), after completed-save disk proof, which must follow the save-call result.' }
if ($TaskSave['status'] -cne 'official_owned_dirty_save_requested' -or
    $TaskSave['native_save_wrapper_returned'] -ne $true -or $TaskSave['all_dirty_assets_save_called'] -ne $true -or
    $TaskSave['requires_independent_cold_validation'] -ne $true -or
    $TaskSave['new_actor_individual_save_failure_preserved'] -ne $true -or
    @($TaskSave['requested_actors']).Count -ne 692 -or @($TaskSave['saved_assets']).Count -ne 2 -or
    ((@($TaskSave['saved_assets'] | Sort-Object) -join '|') -cne ((@($TaskMaterial,$TaskTarget) | Sort-Object) -join '|'))) { throw 'Exact user-approved official dirty-save request proof required; wrapper true alone is not a persistence proof.' }
$TaskBaselinePath = Resolve-EvidenceFile ([string]$TaskSave['baseline_clean_proof'])
if ($TaskBaselinePath -ine (Join-Path $TaskResume 'workbench\cold-reload\comparison.json')) { throw 'Wrong clean workbench baseline proof.' }
$TaskBaseline = Read-Json $TaskBaselinePath
if ($TaskBaseline['passed'] -ne $true -or $TaskBaseline['stage'] -cne 'cold-reload' -or
    @($TaskBaseline['dirtyAfter']['maps']).Count -ne 0 -or @($TaskBaseline['dirtyAfter']['content']).Count -ne 0) { throw 'The referenced strict workbench baseline must have passed with no dirty packages.' }
$TaskRefs = @($TaskSave['requested_actors'] | ForEach-Object { $_['refPath'] })
if (@($TaskRefs | Sort-Object -Unique).Count -ne 692 -or @($TaskRefs | Where-Object { -not ([string]$_).StartsWith($TaskTarget + '.WorldLayoutCompactCrop20261114A:PersistentLevel.',[StringComparison]::Ordinal) }).Count) { throw 'Requested actor paths are duplicate or outside the exact candidate.' }
if ($TaskDisk['status'] -cne 'completed_native_save_disk_scope_requires_cold' -or
    $TaskDisk['before_count'] -ne 1423 -or $TaskDisk['after_count'] -ne 1453 -or
    @($TaskDisk['changed']).Count -ne 663 -or @($TaskDisk['added']).Count -ne 30 -or @($TaskDisk['removed']).Count -ne 0 -or
    $TaskDisk['material_exists'] -ne $true -or $TaskDisk['requires_cold_actor_package_whitelist'] -ne $true -or
    @($TaskDisk['current_files']).Count -ne 1453) { throw 'Only the completed 1423-to-1453 disk proof (663 changed, 29 new EA, one exact new folder EO, removed0) is accepted.' }
$TaskEAPrefix = 'Content/__ExternalActors__/Rogue10m/Maps/Production/WorldLayoutCompactCrop20261114A/'
$TaskChanges = @($TaskDisk['changed']); $TaskAdded = @($TaskDisk['added'])
if (@($TaskChanges | Sort-Object -Unique).Count -ne 663 -or @($TaskAdded | Sort-Object -Unique).Count -ne 30 -or
    @($TaskChanges | Where-Object { -not ([string]$_).StartsWith($TaskEAPrefix,[StringComparison]::Ordinal) -or $_ -notmatch '\.uasset$' -or $_ -match '\.\.' }).Count -or
    @($TaskAdded | Where-Object { $_ -cne $TaskFolderRelative -and (-not ([string]$_).StartsWith($TaskEAPrefix,[StringComparison]::Ordinal) -or $_ -notmatch '\.uasset$' -or $_ -match '\.\.') }).Count -or
    @($TaskAdded | Where-Object { $_ -ceq $TaskFolderRelative }).Count -ne 1 -or
    @($TaskChanges | Where-Object { $TaskAdded -ccontains $_ }).Count) { throw 'Completed disk proof package paths/uniqueness are outside the narrow candidate whitelist.' }

foreach ($TaskPath in @($TaskEditor,$TaskProject,$TaskLoader,$TaskVerifier,$TaskOwnerLaunch,
    (Join-Path $TaskRoot 'Content\Rogue10m\Maps\Production\WorldLayoutCompactCrop20261114A.umap'),
    (Join-Path $TaskRoot 'Content\Rogue10m\World\Production\WorldLayoutCompactCrop20261114A\RoadSurface20261117\M_RoadSurface.uasset'))) {
    Assert-PhysicalPath $TaskPath
    if (-not (Test-Path -LiteralPath $TaskPath -PathType Leaf)) { throw ('Missing required saved input: ' + $TaskPath) }
}
if ((Test-Path -LiteralPath $TaskOut) -or (Test-Path -LiteralPath (Join-Path $TaskRoad ('cold-loader-' + $RunTag))) -or (Test-Path -LiteralPath (Join-Path $TaskRoad 'final-cold'))) { throw 'Preserve existing cold evidence; no overwriting or replay.' }
$TaskProcesses = @(Get-CimInstance Win32_Process)
$TaskBusy = @($TaskProcesses | Where-Object {
    $_.Name -match '^(UnrealEditor-Cmd|UnrealBuildTool|UnrealHeaderTool|AutomationTool|MSBuild)\.exe$' -or
    ($_.Name -ieq 'dotnet.exe' -and ([string]::IsNullOrWhiteSpace($_.CommandLine) -or $_.CommandLine -match 'UnrealBuildTool|AutomationTool|UnrealHeaderTool|Rogue10m'))
})
if ($TaskBusy.Count) { throw 'An existing Cmd editor/build process is active; none is stopped by Launch.' }
$TaskVisible = @($TaskProcesses | Where-Object { $_.Name -ieq 'UnrealEditor.exe' })
if ($TaskVisible.Count -ne 1 -or [int]$TaskVisible[0].ProcessId -ne $AllowedIdleEditorPid) { throw 'Only the one explicitly allowed visible editor may remain open.' }
$TaskOwner = Read-Json $TaskOwnerLaunch
$TaskVisibleTime = ([DateTime]$TaskVisible[0].CreationDate).ToUniversalTime()
if ([int]$TaskOwner['pid'] -ne $AllowedIdleEditorPid -or [IO.Path]::GetFullPath([string]$TaskVisible[0].ExecutablePath) -ine $TaskVisibleEditor -or
    [Math]::Abs(($TaskVisibleTime-[DateTimeOffset]::Parse($TaskOwner['startedUtc']).UtcDateTime).TotalSeconds) -gt 5 -or
    $TaskSaveTime -lt $TaskVisibleTime -or -not ([string]$TaskVisible[0].CommandLine).Replace('/','\').Contains($TaskProject,[StringComparison]::OrdinalIgnoreCase) -or
    -not ([string]$TaskVisible[0].CommandLine).Contains($TaskTarget,[StringComparison]::Ordinal)) { throw 'Allowed visible workbench identity/launch proof does not match.' }
foreach ($TaskFlag in @('-RogueProfileSaveSlot=Rogue10m_Town_QA_Landscape20261116A','-RogueProgressionSavePrefix=Rogue10m_Progression_QA_Landscape20261116A','-RogueSkillKeySaveSlot=Rogue10m_Fighter_QA_Landscape20261116A')) {
    if (@([regex]::Matches([string]$TaskVisible[0].CommandLine,'(?i)(?:^|\s)' + [regex]::Escape($TaskFlag) + '(?=\s|$)')).Count -ne 1) { throw ('Allowed editor namespace mismatch: ' + $TaskFlag) }
}

New-Item -ItemType Directory -Path $TaskOut -ErrorAction Stop | Out-Null
$TaskLog = Join-Path $TaskOut 'engine.log'
$TaskExec = "py '" + $TaskLoader.Replace('\','/') + "'"
$TaskArguments = @(('"'+$TaskProject+'"'),$TaskTarget,'-unattended','-nullrhi','-nosound','-nosplash','-NoLiveCoding','-NoSourceControl','-SCCProvider=None',
    '-language=en','-locale=en',('-RogueRoadSurfaceColdRun='+$RunTag),
    '-RogueProfileSaveSlot=Rogue10m_Town_QA_Landscape20261116A',
    '-RogueProgressionSavePrefix=Rogue10m_Progression_QA_Landscape20261116A',
    '-RogueSkillKeySaveSlot=Rogue10m_Fighter_QA_Landscape20261116A',
    '-ini:EditorPerProjectUserSettings:[/Script/ModelContextProtocolEngine.ModelContextProtocolSettings]:bAutoStartServer=False',
    '-ini:EditorPerProjectUserSettings:[/Script/UnrealEd.EditorLoadingSavingSettings]:bAutoSaveEnable=False',
    ('"-AbsLog='+$TaskLog+'"'),('"-ExecCmds='+$TaskExec+'"'))
$TaskStarted = [DateTime]::UtcNow
$TaskRecord = [ordered]@{status='prepared_not_started';project=$TaskRoot;target=$TaskTarget;runTag=$RunTag;executable=$TaskEditor;pid=$null;processStartUtc=$null;
    startedUtc=$TaskStarted.ToString('o');deadlineUtc=$TaskStarted.AddSeconds(600).ToString('o');timeoutSeconds=600;deadlineEnforcedBy='subsequent -Mode Poll calls; no background supervisor';
    log=$TaskLog;stdout=(Join-Path $TaskOut 'stdout.log');stderr=(Join-Path $TaskOut 'stderr.log');arguments=$TaskArguments;
    allowedIdleEditorPid=$AllowedIdleEditorPid;visibleEditorTouched=$false;callerMustKeepVisibleEditorIdle=$true;
    proofBinding='caller-supplied direct MCP idle proof; separate visible PID/creation/commandline validation';
    saveCallProofStatus=$TaskSave['status'];saveCallIsDiskProof=$false;diskAndColdProofStillRequired=$true;
    explicitUserApproval=$TaskUserApproval;approvalSource='2026-11-17 user message relayed by root; save already performed by root';
    completedDiskProofStatus=$TaskDisk['status'];newFolderPackageStillRequiresColdRegistryProof=$TaskFolderRelative;
    inputFingerprints=@((Get-Fingerprint $TaskSavePath),(Get-Fingerprint $TaskIdlePath),(Get-Fingerprint $TaskDiskPath),(Get-Fingerprint $TaskBaselinePath),(Get-Fingerprint $TaskOwnerLaunch),(Get-Fingerprint $TaskLoader),(Get-Fingerprint $TaskVerifier),
        (Get-Fingerprint (Join-Path $TaskRoot 'Binaries\Win64\UnrealEditor-Rogue10m.dll')));
    exitCode=$null;waited=$false;assetsSavedByLauncher=$false}
Write-JsonNew (Join-Path $TaskOut 'prepared.json') $TaskRecord
$TaskProcess = Start-Process -FilePath $TaskEditor -ArgumentList $TaskArguments -WorkingDirectory $TaskRoot -WindowStyle Hidden -PassThru `
    -RedirectStandardOutput $TaskRecord['stdout'] -RedirectStandardError $TaskRecord['stderr']
$TaskRecord['pid']=$TaskProcess.Id
$TaskRecord['processStartUtc']=$TaskProcess.StartTime.ToUniversalTime().ToString('o')
$TaskRecord['status']='launched_unverified_poll_required'
Write-JsonNew $TaskLaunch $TaskRecord
$TaskRecord | ConvertTo-Json -Depth 10
