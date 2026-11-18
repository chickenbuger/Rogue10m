param(
    [Parameter(Mandatory)][ValidatePattern('^[A-Za-z0-9_]{1,24}$')][string]$Label,
    [Parameter(Mandatory)]
    [ValidateSet('Legacy1', 'Legacy2', 'Legacy3', 'Omitted4', 'Omitted5', 'Invalid0', 'Future999', 'CopiedLegacy')]
    [string]$Scenario,
    [ValidateRange(30, 900)][int]$TimeoutSeconds = 240,
    [string]$EngineRoot = 'D:\Program Files\UE_5.8',
    [switch]$UseLiveCoding
)
$ErrorActionPreference = 'Stop'
$ProjectRoot = Split-Path -Parent $PSScriptRoot
$ProjectFile = Join-Path $ProjectRoot 'Rogue10m.uproject'
$EditorName = if ($UseLiveCoding) { 'UnrealEditor.exe' } else { 'UnrealEditor-Cmd.exe' }
$Editor = Join-Path $EngineRoot "Engine\Binaries\Win64\$EditorName"
$Map = '/Game/Rogue10m/Maps/StartHub'
$IsCopiedReplay = $Scenario -eq 'CopiedLegacy'
$PrivateUserDir = Join-Path $ProjectRoot "tmp\LegacyPortal20261118\replays\$Label"
$Output = if ($IsCopiedReplay) { Join-Path $PrivateUserDir 'verification' } else {
    Join-Path $ProjectRoot "Feature\doc\evidence\portal-progression-recovery-20261118\$Label\$Scenario"
}
$Log = Join-Path $Output 'runtime.log'
$Suffix = $Label + '_' + $Scenario
$ProfileSlot = 'Rogue10m_Town_QA_LegacyPortal_' + $Suffix
$Prefix = 'Rogue10m_Progression_QA_LegacyPortal_' + $Suffix
$KeySlot = 'Rogue10m_Fighter_QA_LegacyPortal_' + $Suffix

if (-not (Test-Path -LiteralPath $Editor -PathType Leaf) -or -not (Test-Path -LiteralPath $ProjectFile -PathType Leaf)) {
    throw 'UnrealEditor-Cmd or Rogue10m.uproject is unavailable.'
}
if (Test-Path -LiteralPath $Output) { throw 'This label and scenario already have evidence. Choose a new label.' }

$SessionSaveArguments = @(
    ('-RogueProfileSaveSlot=' + $ProfileSlot), ('-RogueProgressionSavePrefix=' + $Prefix), ('-RogueSkillKeySaveSlot=' + $KeySlot)
)
$CopiedInputHashes = @{}
if ($IsCopiedReplay) {
    # Only exact physical scratch copies are allowed. Do not start a game when
    # a copied path is missing, redirected, or contains an already-migrated slot.
    $Directory = $ProjectRoot
    foreach ($Segment in @('', 'tmp', 'LegacyPortal20261118', 'replays', $Label, 'Saved', 'SaveGames')) {
        if ($Segment) { $Directory = Join-Path $Directory $Segment }
        $DirectoryItem = Get-Item -LiteralPath $Directory -Force
        if (-not $DirectoryItem.PSIsContainer -or ($DirectoryItem.Attributes -band [IO.FileAttributes]::ReparsePoint)) {
            throw 'The replay UserDir must be a physical project scratch directory.'
        }
    }
    $InputFiles = @(Get-ChildItem -LiteralPath $Directory -Force)
    $RosterFiles = @($InputFiles | Where-Object { $_.Name -ceq 'Rogue10mCharacterProfiles.sav' })
    $LegacyFiles = @($InputFiles | Where-Object { $_.Name -cmatch '^Rogue10mProgression_[0-9A-Fa-f]{32}\.sav$' })
    if ($InputFiles.Count -ne 2 -or $RosterFiles.Count -ne 1 -or $LegacyFiles.Count -ne 1) {
        throw 'Replay needs exactly one copied default roster and one bare progression save in its scratch SaveGames.'
    }
    foreach ($InputFile in $InputFiles) {
        if ($InputFile.PSIsContainer -or ($InputFile.Attributes -band [IO.FileAttributes]::ReparsePoint) -or $InputFile.Length -le 0) {
            throw 'The replay inputs must be non-empty regular save files.'
        }
        $CopiedInputHashes[$InputFile.FullName] = (Get-FileHash -LiteralPath $InputFile.FullName -Algorithm SHA256).Hash
    }
    $SessionSaveArguments = @(('"-UserDir=' + $PrivateUserDir + '"'))
}

$LiveCodingFlag = if ($UseLiveCoding) { '-LiveCoding' } else { '-NoLiveCoding' }
$Arguments = @(
    ('"' + $ProjectFile + '"'), $Map, '-game', '-nullrhi', '-unattended', '-nosplash', '-nosound',
    $LiveCodingFlag, '-NoRemoteShaderCompile', '-DisablePython', '-language=en', '-locale=en-US',
    '-ini:EditorPerProjectUserSettings:[/Script/ModelContextProtocolEngine.ModelContextProtocolSettings]:bAutoStartServer=False',
    ('-RogueLegacyPortalLabel=' + $Label), ('-RogueLegacyPortalCase=' + $Scenario), '-RogueLegacyPortalAutoStart',
    ('"-AbsLog=' + $Log + '"')
) + $SessionSaveArguments
[void](New-Item -ItemType Directory -Path $Output -Force)
$Process = Start-Process -FilePath $Editor -ArgumentList $Arguments -WorkingDirectory $ProjectRoot -WindowStyle Hidden -PassThru
Write-Output "Legacy portal $Scenario PID=$($Process.Id) log=$Log"
$Watch = [Diagnostics.Stopwatch]::StartNew()
try {
    while (-not $Process.WaitForExit(1000)) {
        if ($Watch.Elapsed.TotalSeconds -ge $TimeoutSeconds) { throw "Legacy portal scenario $Scenario timed out." }
    }
    $Process.WaitForExit()
    if (-not (Test-Path -LiteralPath $Log -PathType Leaf)) { throw 'The owned process exited without an Unreal log.' }
    $LogText = [IO.File]::ReadAllText($Log)
    $ResultPattern = 'RESULT=LEGACY_PORTAL_PASSED case=' + [Regex]::Escape($Scenario) + ' checks=(\d+) actual_portal=([01]) walk_input=([01])'
    $Marker = [Regex]::Match($LogText, $ResultPattern)
    $EnteredPortal = $Scenario -in @('Legacy1', 'Legacy3', 'CopiedLegacy')
    $PortalRow = "Run portal opened level '/Game/Rogue10m/Maps/OpenRunMap' with options 'StartRun=1'."
    $ObservedPortal = $LogText.Contains($PortalRow)
    $CopiedInputsPreserved = $true
    foreach ($Path in $CopiedInputHashes.Keys) {
        if (-not (Test-Path -LiteralPath $Path -PathType Leaf) -or (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash -cne $CopiedInputHashes[$Path]) {
            $CopiedInputsPreserved = $false
        }
    }
    $Passed = $Process.ExitCode -eq 0 -and $Marker.Success -and $ObservedPortal -eq $EnteredPortal `
        -and [int]$Marker.Groups[2].Value -eq [int]$EnteredPortal -and [int]$Marker.Groups[3].Value -eq [int]$EnteredPortal -and $CopiedInputsPreserved
    [ordered]@{
        passed = $Passed
        scenario = $Scenario
        pid = $Process.Id
        exit_code = $Process.ExitCode
        checks = if ($Marker.Success) { [int]$Marker.Groups[1].Value } else { 0 }
        elapsed_seconds = $Watch.Elapsed.TotalSeconds
        actual_authored_portal_travel = $ObservedPortal
        actual_overlap_uses_programmatic_movement = $ObservedPortal -and $Marker.Success
        physical_input = $false
        live_coding_requested = [bool]$UseLiveCoding
        private_user_dir = $IsCopiedReplay
        copied_input_files_preserved = if ($IsCopiedReplay) { $CopiedInputsPreserved } else { $null }
        log = [IO.Path]::GetRelativePath($ProjectRoot, $Log).Replace('\', '/')
    } | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $Output 'result.json') -Encoding utf8
    if (-not $Passed) { throw "Legacy portal $Scenario failed, exit=$($Process.ExitCode). See $Log" }
    Write-Output $Marker.Value
} finally {
    if (-not $Process.HasExited) {
        Stop-Process -InputObject $Process
        [void]$Process.WaitForExit(5000)
    }
}
