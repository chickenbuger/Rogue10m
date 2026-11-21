param(
    [Parameter(Mandatory)][ValidatePattern('^[A-Za-z][A-Za-z0-9_]{0,23}$')][string]$Label,
    [ValidateSet('720p','1080p')][string]$Resolution = '720p',
    [ValidateRange(30,900)][int]$TimeoutSeconds = 330,
    [string]$EngineRoot = 'D:\Program Files\UE_5.8',
    [switch]$RequireAuthoredSword,
    [switch]$LogicOnly,
    [switch]$CapstoneBrace,
    [switch]$CapstoneBraceRender
)

$ErrorActionPreference = 'Stop'
if ($CapstoneBraceRender -and $LogicOnly) { throw 'Capstone brace rendering requires D3D11.' }
if ($CapstoneBraceRender) { $CapstoneBrace = $true }
if ($CapstoneBrace -and $RequireAuthoredSword) { throw 'Choose a single fixture mode.' }
$projectRoot = (Resolve-Path -LiteralPath (Split-Path -Parent $PSScriptRoot)).Path
$projectFile = Join-Path $projectRoot 'Rogue10m.uproject'
$editor = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor.exe'
$python = Join-Path $EngineRoot 'Engine\Binaries\ThirdParty\Python3\Win64\python.exe'
$output = Join-Path $projectRoot "tmp\MeleeVfxQARuns\$Label"
if (Test-Path -LiteralPath $output) { throw "Use a fresh QA label. Existing evidence: $output" }
New-Item -ItemType Directory -Path $output | Out-Null
$log = Join-Path $output 'engine.log'
$width, $height = if ($Resolution -eq '1080p') { 1920, 1080 } else { 1280, 720 }

function Get-SaveHashes {
    $saveRoot = Join-Path $projectRoot 'Saved\SaveGames'
    $hashes = @{}
    if (Test-Path -LiteralPath $saveRoot) {
        Get-ChildItem -LiteralPath $saveRoot -Recurse -File -Filter '*.sav' | ForEach-Object {
            $hashes[$_.FullName] = (Get-FileHash -LiteralPath $_.FullName).Hash
        }
    }
    return $hashes
}

$before = Get-SaveHashes
$exec = if ($CapstoneBraceRender) { 't.MaxFPS 60,r.MotionBlurQuality 0,r.ScreenPercentage 50,Rogue10m.TestWeaponThirdPerson' }
else { 't.MaxFPS 60,Rogue10m.TestWeaponThirdPerson' }
$argsList = @(
    ('"' + $projectFile + '"'), '/Game/Rogue10m/Maps/StartHub',
    '-game', '-DisablePython', '-unattended', '-nosplash', '-nosound', '-NoLiveCoding', '-NoRemoteShaderCompile',
    '-language=en', '-locale=en-US', '-windowed', '-ForceRes', "-ResX=$width", "-ResY=$height", '-RenderOffScreen',
    ('-AbsLog="' + $log + '"'), "-RogueProfileSaveSlot=Rogue10m_Town_QA_MeleeVfx_$Label",
    "-RogueProgressionSavePrefix=Rogue10m_Progression_QA_MeleeVfx_$Label", "-RogueSkillKeySaveSlot=Rogue10m_Fighter_QA_MeleeVfx_$Label",
    "-RogueWeaponLabel=$Label", '-RogueMeleeVfxRun', ('-ExecCmds="' + $exec + '"')
)
if ($LogicOnly) { $argsList += @('-nullrhi', '-RogueMeleeVfxLogicOnly') }
else { $argsList += '-d3d11' }
if ($RequireAuthoredSword) { $argsList += '-RogueRequireAuthoredSword' }
if ($CapstoneBrace) { $argsList += '-RogueCapstoneBraceQA' }
if ($CapstoneBraceRender) { $argsList += @('-RogueCapstoneBraceRender', '-AllowSoftwareRendering', '-graphicsadapter=1') }

$process = $null
$started = $null
try {
    $process = Start-Process -FilePath $editor -ArgumentList $argsList -WindowStyle Hidden -PassThru
    $started = $process.StartTime
    Write-Output "Melee VFX QA PID=$($process.Id) log=$log"
    if (-not $process.WaitForExit($TimeoutSeconds * 1000)) { throw 'Melee VFX QA timed out.' }
    $process.Refresh()
    $logReport = Join-Path $output 'log-check.json'
    & $python -B (Join-Path $PSScriptRoot 'CheckUnrealOutputLog.py') --log $log --after-line 0 --json-output $logReport
    $logCheck = $LASTEXITCODE
    if ($CapstoneBraceRender) {
        $adapter = Select-String -LiteralPath $log -Pattern 'LogRHI:\s+Name:\s+Microsoft Basic Render Driver' | Select-Object -First 1
        if (-not $adapter) { throw 'Capstone brace rendering must use the software adapter.' }
        $errors = (Get-Content -LiteralPath $logReport -Raw | ConvertFrom-Json).new_errors
        if ($logCheck -eq 1 -and $errors.Count -eq 1 -and $errors[0].line -lt $adapter.LineNumber -and
            $errors[0].category -eq 'LogD3D11RHI' -and $errors[0].message.Trim() -eq 'Result failed with error 887A0004') {
            Write-Warning "The hardware probe failed before WARP startup. Preserved engine log line $($errors[0].line) and the complete error report."
            & $python -B (Join-Path $PSScriptRoot 'CheckUnrealOutputLog.py') --log $log --after-line $adapter.LineNumber --json-output (Join-Path $output 'post-adapter-log-check.json')
            $logCheck = $LASTEXITCODE
        }
    }
    $result = if ($CapstoneBraceRender) { 'CAPSTONE_BRACE_RENDER' } elseif ($CapstoneBrace) { 'CAPSTONE_BRACE' }
    elseif ($LogicOnly) { 'MELEE_VFX_LOGIC' } else { 'MELEE_VFX' }
    $expectedCases = if ($CapstoneBraceRender) { 2 } elseif ($CapstoneBrace) { 4 } else { 14 }
    if ($process.ExitCode -ne 0 -or $logCheck -ne 0 -or
        -not (Select-String -LiteralPath $log -SimpleMatch "RESULT=${result}_PASSED completed=$expectedCases failures=0" -Quiet)) {
        throw "Melee VFX runtime failed. Exit=$($process.ExitCode) LogCheck=$logCheck Evidence=$output"
    }

    if ($CapstoneBrace -and -not $CapstoneBraceRender) {
        $evidence = @(Import-Csv -LiteralPath (Join-Path $output 'capstone-brace.csv'))
        foreach ($required in @('stock:release','brace:release','early:early_special',
            'pitch_cancel:pitch_zero','pitch_cancel:pitch_high','pitch_cancel:pitch_low','pitch_cancel:cancelled')) {
            $case, $phase = $required -split ':'
            if (-not ($evidence | Where-Object { $_.case -eq $case -and $_.phase -eq $phase })) {
                throw "Missing capstone sampling evidence for $required."
            }
        }
        Select-String -LiteralPath $log -Pattern 'CAPSTONE_BRACE (HOLD|FEET|DAMAGE|RELEASE)' | ForEach-Object { $_.Line }
        Write-Output "Capstone brace numerical gates passed using simulated RMB input. Evidence is under $output."
        return
    }

    $captures = @(Import-Csv -LiteralPath (Join-Path $output 'captures.csv'))
    $requests = @($captures | Where-Object event -eq 'request')
    $processed = @($captures | Where-Object event -eq 'processed')
    $expectedCaptures = if ($CapstoneBraceRender) { 2 } elseif ($LogicOnly) { 0 } else { 46 }
    if ($requests.Count -ne $expectedCaptures -or $processed.Count -ne $expectedCaptures -or
        ($expectedCaptures -gt 0 -and @($requests.file | Select-Object -Unique).Count -ne $expectedCaptures)) {
        throw "Expected $expectedCaptures unique requested and processed screenshots."
    }
    foreach ($capture in $processed) {
        if ([long]$capture.bytes -lt 1024 -or (Get-Item -LiteralPath $capture.file).Length -ne [long]$capture.bytes) {
            throw "Invalid rendered capture: $($capture.file)"
        }
    }
    if ($CapstoneBraceRender) {
        $evidence = @(Import-Csv -LiteralPath (Join-Path $output 'capstone-brace.csv'))
        $culture = [Globalization.CultureInfo]::InvariantCulture
        foreach ($case in @('stock','brace')) {
            $peaks = @($evidence | Where-Object { $_.case -eq $case -and $_.phase -eq 'hold_peak' })
            $sampled = @($processed | Where-Object { $_.skill -eq $case -and $_.phase -eq 'hold_peak' })
            $request = @($requests | Where-Object { $_.skill -eq $case -and $_.phase -eq 'hold_peak' })
            if ($peaks.Count -ne 1 -or $sampled.Count -ne 1 -or $request.Count -ne 1 -or
                [int]$request[0].fixture_frame -ne [int]$peaks[0].frame -or
                [math]::Abs([double]::Parse($peaks[0].hold_s, $culture) - 20.0 / 60.0) -gt .001 -or
                [math]::Abs([int]$sampled[0].fixture_frame - [int]$request[0].fixture_frame) -gt 1) {
                throw "Missing matched peak capture for capstone case $case."
            }
            Write-Output "Capstone brace $case hold $($peaks[0].hold_s)s captured at $($sampled[0].file)"
        }
        return
    }
    Get-Content -LiteralPath (Join-Path $output 'metrics.txt') | Select-Object -First 8
    Select-String -LiteralPath $log -Pattern 'MELEE_VFX CASE' | ForEach-Object { $_.Line }
    if ($LogicOnly) { Write-Output "Melee VFX numeric gates passed without rendering. Evidence is under $output." }
    else { Write-Output "Melee VFX numerical and rendered capture gates passed. Visual inspection uses raw screenshots under $output." }
} finally {
    if ($process -and -not $process.HasExited) {
        $owned = Get-Process -Id $process.Id -ErrorAction SilentlyContinue
        if ($owned -and $owned.StartTime -eq $started -and $owned.Path -eq $editor) {
            Stop-Process -InputObject $owned -Force
            $null = $process.WaitForExit(10000)
        }
    }
    $after = Get-SaveHashes
    foreach ($path in $before.Keys) {
        if ($before[$path] -ne $after[$path]) { throw "Existing save modified during verification: $path" }
    }
    Write-Output "Existing saves preserved=$($before.Count)"
}
