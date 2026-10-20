param(
    [ValidateSet('runtime','render')][string]$Phase = 'runtime',
    [string]$Label = '',
    [switch]$Baseline,
    [switch]$AllAppearances,
    [string]$CharacterClass = '/Game/Character/Customization/Characters/BP_Rogue10m_HumanFemaleCharacter.BP_Rogue10m_HumanFemaleCharacter_C',
    [string]$Map = '/Game/Rogue10m/Maps/StartHub',
    [int]$TimeoutSeconds = 600,
    [string]$EngineRoot = 'D:\Program Files\UE_5.8',
    [string]$ProjectRoot = 'D:\Project\Rogue10m'
)
$ErrorActionPreference = 'Stop'
if ([string]::IsNullOrEmpty($Label)) {
    $Label = 'walk-palm-20261009-' + $Phase
    if ($Baseline) { $Label += '-baseline' }
}
if ($Label -notmatch '^[A-Za-z0-9_-]+$') { throw 'Invalid evidence label.' }
if ($Map -notmatch '^/Game/[A-Za-z0-9_/]+$') { throw 'Invalid package map path.' }
if ($TimeoutSeconds -lt 30 -or $TimeoutSeconds -gt 900) { throw 'Timeout must be 30..900 seconds per appearance.' }
$runs = @([pscustomobject]@{ Tag = 'single'; Class = $CharacterClass; Label = $Label })
if ($AllAppearances) {
    $runs = @(foreach ($tag in @('HumanFemale','HumanMale','DwarfFemale','DwarfMale','OrcFemale','OrcMale')) {
        $base = 'BP_Rogue10m_' + $tag + 'Character'
        [pscustomobject]@{ Tag = $tag; Class = '/Game/Character/Customization/Characters/' + $base + '.' + $base + '_C'; Label = $Label + '-' + $tag }
    })
}
$evidence = Join-Path $ProjectRoot 'Feature\doc\evidence\walking-palm-reference-20261009'
foreach ($run in $runs) {
    if ($run.Class -notmatch '^/Game/[A-Za-z0-9_/]+\.[A-Za-z0-9_]+_C$') { throw 'Invalid character generated-class path.' }
    $runLog = Join-Path $evidence ($run.Label + '.log')
    $runMetrics = Join-Path $ProjectRoot ('tmp\walk-palm\' + $run.Label + '\metrics.txt')
    $runImages = Join-Path $ProjectRoot ('Saved\Screenshots\WindowsEditor\WalkPalm\' + $run.Label)
    if ((Test-Path -LiteralPath $runLog) -or (Test-Path -LiteralPath $runMetrics) -or (Test-Path -LiteralPath $runImages)) {
        throw ('Existing evidence: choose a fresh label. ' + $run.Label)
    }
}
New-Item -ItemType Directory -Path $evidence -Force | Out-Null
$editor = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
foreach ($run in $runs) {
    $log = Join-Path $evidence ($run.Label + '.log')
    $metrics = Join-Path $ProjectRoot ('tmp\walk-palm\' + $run.Label + '\metrics.txt')
    $images = Join-Path $ProjectRoot ('Saved\Screenshots\WindowsEditor\WalkPalm\' + $run.Label)
    $arguments = @(('"' + (Join-Path $ProjectRoot 'Rogue10m.uproject') + '"'),
        $Map,'-game','-unattended','-nosplash','-nosound','-NoLiveCoding','-NoRemoteShaderCompile',
        '-windowed','-ForceRes','-ResX=1280','-ResY=720',
        ('-RogueSkillKeySaveSlot=Rogue10m_Fighter_QA_WalkPalm_' + $run.Tag + '_' + $Phase),
        '-ExecCmds="Rogue10m.TestWalkPalm"',('-RogueWalkPalmLabel=' + $run.Label),
        ('-RogueWalkPalmCharacterClass=' + $run.Class),('-abslog="' + $log + '"'))
    if ($Baseline) { $arguments += '-RogueWalkPalmBaseline' }
    if ($Phase -eq 'render') {
        $arguments += @('-RogueWalkPalmCapture','-ini:Engine:[/Script/Engine.RendererSettings]:r.RayTracing=0,r.PathTracing=0')
    } else { $arguments += '-nullrhi' }
    $process = $null
    try {
        $process = Start-Process -FilePath $editor -ArgumentList $arguments -WindowStyle Hidden -PassThru
        Write-Output ($Phase + ' appearance=' + $run.Tag + ' pid=' + $process.Id + ' log=' + $log)
        $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
        while (-not $process.WaitForExit(1000)) {
            if ([DateTime]::UtcNow -gt $deadline) { throw ($Phase + ' timed out; log=' + $log) }
        }
        $expected = 'RESULT=WALK_PALM_PASSED'
        if ($process.ExitCode -ne 0 -or -not (Test-Path -LiteralPath $log) -or
            -not ([IO.File]::ReadAllText($log).Contains($expected)) -or -not (Test-Path -LiteralPath $metrics)) {
            throw ($Phase + ' failed; exit=' + $process.ExitCode + ' log=' + $log)
        }
        if ($Phase -eq 'render') {
            foreach ($imageName in @('idle_firstperson','walk110_front','walk210_front','walk210_left_hand_close','walk210_right_hand_close','run600_front','yaw90_walk210_front','moving_jab_front','stop_idle_firstperson')) {
                if (-not (Test-Path -LiteralPath (Join-Path $images ($imageName + '.png')))) { throw ('Missing rendered evidence: ' + $imageName + '; images=' + $images) }
            }
        }
        Write-Output ($expected + ' appearance=' + $run.Tag + ' exit=' + $process.ExitCode + ' metrics=' + $metrics + ' images=' + $images)
    } finally {
        if ($null -ne $process -and -not $process.HasExited) {
            Stop-Process -Id $process.Id
            $process.WaitForExit(5000) | Out-Null
        }
    }
}
