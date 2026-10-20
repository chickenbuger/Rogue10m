param(
    [ValidateSet('runtime','render')][string]$Phase = 'runtime',
    [string]$Label = '',
    [int]$TimeoutSeconds = 600,
    [string]$Map = '/Game/Rogue10m/Maps/StartHub',
    [string]$EngineRoot = 'D:\Program Files\UE_5.8',
    [string]$ProjectRoot = 'D:\Project\Rogue10m'
)
$ErrorActionPreference = 'Stop'
if ([string]::IsNullOrEmpty($Label)) { $Label = 'camera-forward-20261009-' + $Phase }
if ($Label -notmatch '^[A-Za-z0-9_-]+$') { throw 'Invalid evidence label.' }
if ($Map -notmatch '^/Game/[A-Za-z0-9_/]+$') { throw 'Invalid package map path.' }
if ($TimeoutSeconds -lt 30 -or $TimeoutSeconds -gt 900) { throw 'Timeout must be 30..900 seconds.' }
$evidence = Join-Path $ProjectRoot 'Feature\doc\evidence\camera-forward-attacks-20261009'
$log = Join-Path $evidence ($Label + '.log')
$metrics = Join-Path $ProjectRoot ('tmp\camera-forward\' + $Label + '\metrics.txt')
$images = Join-Path $ProjectRoot ('Saved\Screenshots\WindowsEditor\CameraForward\' + $Label)
if ((Test-Path -LiteralPath $log) -or (Test-Path -LiteralPath $metrics) -or (Test-Path -LiteralPath $images)) {
    throw 'Existing evidence: choose a fresh label.'
}
New-Item -ItemType Directory -Path $evidence -Force | Out-Null
$arguments = @(('"' + (Join-Path $ProjectRoot 'Rogue10m.uproject') + '"'),
    $Map,'-game','-unattended','-nosplash','-nosound','-NoLiveCoding','-NoRemoteShaderCompile',
    '-windowed','-ForceRes','-ResX=1280','-ResY=720',
    ('-RogueSkillKeySaveSlot=Rogue10m_Fighter_QA_CameraForward_' + $Phase),
    ('-RogueProgressionSavePrefix=Rogue10m_Progression_QA_CameraForward_' + $Phase),
    '-ExecCmds="Rogue10m.TestCameraForward"',('-RogueCameraForwardLabel=' + $Label),('-abslog="' + $log + '"'))
if ($Phase -eq 'render') {
    $arguments += @('-RogueCameraForwardCapture','-ini:Engine:[/Script/Engine.RendererSettings]:r.RayTracing=0,r.PathTracing=0')
} else { $arguments += '-nullrhi' }
$editor = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$process = $null
try {
    $process = Start-Process -FilePath $editor -ArgumentList $arguments -WindowStyle Hidden -PassThru
    Write-Output ($Phase + ' pid=' + $process.Id + ' map=' + $Map + ' log=' + $log)
    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    while (-not $process.WaitForExit(1000)) {
        if ([DateTime]::UtcNow -gt $deadline) { throw ($Phase + ' timed out; log=' + $log) }
    }
    $expected = 'RESULT=CAMERA_FORWARD_PASSED'
    if ($process.ExitCode -ne 0 -or -not (Test-Path -LiteralPath $log) -or
        -not ([IO.File]::ReadAllText($log).Contains($expected)) -or -not (Test-Path -LiteralPath $metrics)) {
        throw ($Phase + ' failed; exit=' + $process.ExitCode + ' log=' + $log)
    }
    if ($Phase -eq 'render') {
        $results = @(Import-Csv -LiteralPath (Join-Path $ProjectRoot ('tmp\camera-forward\' + $Label + '\results.csv')))
        $contactImages = @(Get-ChildItem -LiteralPath $images -Filter 'contact_*.png' -File)
        if (-not (Test-Path -LiteralPath (Join-Path $images 'idle_old_eye4.png')) -or
            -not (Test-Path -LiteralPath (Join-Path $images 'idle_current_eye_minus3.png')) -or
            $contactImages.Count -ne $results.Count) { throw ('Rendered contact/idle evidence incomplete; images=' + $images) }
    }
    Write-Output ($expected + ' exit=' + $process.ExitCode + ' metrics=' + $metrics + ' images=' + $images)
} finally {
    if ($null -ne $process -and -not $process.HasExited) {
        Stop-Process -Id $process.Id
        $process.WaitForExit(5000) | Out-Null
    }
}
