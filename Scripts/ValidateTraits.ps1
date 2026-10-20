param(
    [ValidateSet('Effects','UI')][string]$Mode = 'Effects',
    [string]$Label = '',
    [ValidateRange(640,3840)][int]$Width = 1280,
    [ValidateRange(480,2160)][int]$Height = 720,
    [ValidateRange(30,900)][int]$TimeoutSeconds = 180,
    [string]$EngineRoot = 'D:\Program Files\UE_5.8',
    [string]$ProjectRoot = (Split-Path -Parent $PSScriptRoot)
)
$ErrorActionPreference = 'Stop'
if ([string]::IsNullOrWhiteSpace($Label)) { $Label = 'trait_' + $Mode.ToLowerInvariant() + '_' + [DateTime]::UtcNow.ToString('yyyyMMdd_HHmmss') }
if ($Label -notmatch '^[A-Za-z0-9_]{1,50}$') { throw 'QA label must use 1..50 letters, digits or underscores.' }
$traitProfileSlot = 'Rogue10m_Town_QA_' + $Label
$traitSavePrefix = 'Rogue10m_Progression_QA_' + $Label
$traitEvidence = Join-Path $ProjectRoot ('Feature\doc\evidence\trait16-runtime-20261011\' + $traitProfileSlot)
if ((Test-Path -LiteralPath $traitEvidence) -or (Test-Path -LiteralPath (Join-Path $ProjectRoot ('Saved\SaveGames\' + $traitProfileSlot + '.sav')))) {
    throw 'QA label already exists; prior saves and evidence are preserved.'
}
$traitEditorName = if ($Mode -eq 'UI') { 'UnrealEditor.exe' } else { 'UnrealEditor-Cmd.exe' }
$traitEditor = Join-Path $EngineRoot ('Engine\Binaries\Win64\' + $traitEditorName)
$traitProject = Join-Path $ProjectRoot 'Rogue10m.uproject'
if (-not (Test-Path -LiteralPath $traitEditor) -or -not (Test-Path -LiteralPath $traitProject)) { throw 'Editor or project is missing.' }
New-Item -ItemType Directory -Path $traitEvidence -Force | Out-Null
$traitLog = Join-Path $traitEvidence ($Mode.ToLowerInvariant() + '.log')
$traitExpected = if ($Mode -eq 'UI') { 'RESULT=TRAIT_UI_PASSED' } else { 'RESULT=TRAIT_EFFECTS_PASSED' }
$traitArguments = @(
    ('"' + $traitProject + '"'), '/Game/Rogue10m/Maps/StartHub', '-game',
    '-unattended', '-nosound', '-nosplash', '-NoLiveCoding', '-NoRemoteShaderCompile',
    ('-RogueProfileSaveSlot=' + $traitProfileSlot), ('-RogueProgressionSavePrefix=' + $traitSavePrefix),
    ('-RogueSkillKeySaveSlot=Rogue10m_Fighter_QA_Trait_' + $Label),
    ('-ExecCmds="Rogue10m.TestTrait' + $Mode + '"'), ('-abslog="' + $traitLog + '"')
)
if ($Mode -eq 'UI') {
    $traitArguments += @('-RenderOffscreen','-windowed','-d3d12','-ForceRes',('-ResX=' + $Width),('-ResY=' + $Height))
} else { $traitArguments += '-nullrhi' }
$traitProcess = $null
try {
    $traitProcess = Start-Process -FilePath $traitEditor -ArgumentList $traitArguments -WindowStyle Hidden -PassThru
    Write-Output ($Mode + ' pid=' + $traitProcess.Id + ' log=' + $traitLog)
    $traitDeadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    while (-not $traitProcess.WaitForExit(1000)) {
        if ([DateTime]::UtcNow -gt $traitDeadline) { throw ('Trait ' + $Mode + ' timed out; log=' + $traitLog) }
    }
    if ($traitProcess.ExitCode -ne 0 -or -not (Test-Path -LiteralPath $traitLog) -or
        -not ([IO.File]::ReadAllText($traitLog).Contains($traitExpected))) {
        throw ('Trait ' + $Mode + ' failed; exit=' + $traitProcess.ExitCode + ' log=' + $traitLog)
    }
    Write-Output ($traitExpected + ' exit=' + $traitProcess.ExitCode + ' evidence=' + $traitEvidence)
} finally {
    if ($null -ne $traitProcess -and -not $traitProcess.HasExited) { Stop-Process -Id $traitProcess.Id; $traitProcess.WaitForExit(5000) | Out-Null }
}
