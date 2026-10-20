param(
    [string]$Label = '',
    [ValidateRange(640,3840)][int]$Width = 1280,
    [ValidateRange(480,2160)][int]$Height = 720,
    [ValidateSet('d3d11','d3d12')][string]$GraphicsApi = 'd3d12',
    [ValidateRange(30,900)][int]$TimeoutSeconds = 240,
    [string]$EngineRoot = 'D:\Program Files\UE_5.8',
    [string]$ProjectRoot = (Split-Path -Parent $PSScriptRoot)
)
$ErrorActionPreference = 'Stop'
if ([string]::IsNullOrWhiteSpace($Label)) { $Label = [DateTime]::UtcNow.ToString('yyyyMMdd_HHmmss') }
if ($Label -notmatch '^[A-Za-z0-9_]{1,50}$') { throw 'QA label must be 1..50 letters, digits or underscores.' }
$profileSlot = 'Rogue10m_Town_QA_' + $Label
$evidence = Join-Path $ProjectRoot ('Feature\doc\evidence\town-services-persistence-20261011\' + $profileSlot)
if ((Test-Path -LiteralPath $evidence) -or (Test-Path -LiteralPath (Join-Path $ProjectRoot ('Saved\SaveGames\' + $profileSlot + '.sav')))) {
    throw 'QA label already exists. Prior saves and evidence are preserved.'
}
New-Item -ItemType Directory -Path $evidence -Force | Out-Null
$log = Join-Path $evidence 'visual.log'
$editor = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor.exe'
$project = Join-Path $ProjectRoot 'Rogue10m.uproject'
$arguments = @(
    ('"' + $project + '"'), '/Game/Rogue10m/Maps/StartHub', '-game', '-RenderOffscreen', '-windowed',
    ('-ResX=' + $Width), ('-ResY=' + $Height), ('-' + $GraphicsApi), '-ForceRes', '-unattended', '-nosound', '-nosplash', '-NoLiveCoding', '-NoRemoteShaderCompile',
    ('-RogueProfileSaveSlot=' + $profileSlot), ('-RogueProgressionSavePrefix=Rogue10m_Progression_QA_' + $Label),
    ('-RogueSkillKeySaveSlot=Rogue10m_Fighter_QA_Town_' + $Label),
    '-ExecCmds="Rogue10m.TestTownVisual"', ('-abslog="' + $log + '"')
)
$process = $null
try {
    $process = Start-Process -FilePath $editor -ArgumentList $arguments -WindowStyle Hidden -PassThru
    Write-Output ('Visual pid=' + $process.Id + ' log=' + $log)
    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    while (-not $process.WaitForExit(1000)) {
        if ([DateTime]::UtcNow -gt $deadline) { throw ('Visual QA timeout; log=' + $log) }
    }
    if ($process.ExitCode -ne 0 -or -not ([IO.File]::ReadAllText($log).Contains('RESULT=TOWN_VISUAL_PASSED')) -or
        -not (Test-Path -LiteralPath (Join-Path $evidence 'town-services.png'))) { throw ('Visual QA failed; log=' + $log) }
    Write-Output ('RESULT=TOWN_VISUAL_PASSED exit=' + $process.ExitCode + ' evidence=' + $evidence)
} finally {
    if ($null -ne $process -and -not $process.HasExited) { Stop-Process -Id $process.Id; $process.WaitForExit(5000) | Out-Null }
}
