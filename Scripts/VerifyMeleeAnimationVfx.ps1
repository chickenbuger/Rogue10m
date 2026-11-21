param(
    [Parameter(Mandatory)][ValidatePattern('^[A-Za-z][A-Za-z0-9_]{0,23}$')][string]$Label,
    [ValidateSet('720p','1080p')][string]$Resolution = '720p',
    [ValidateRange(30,900)][int]$TimeoutSeconds = 330,
    [string]$EngineRoot = 'D:\Program Files\UE_5.8',
    [switch]$RequireAuthoredSword
)

$ErrorActionPreference = 'Stop'
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
$argsList = @(
    ('"' + $projectFile + '"'), '/Game/Rogue10m/Maps/StartHub',
    '-game', '-DisablePython', '-unattended', '-nosplash', '-nosound', '-NoLiveCoding', '-NoRemoteShaderCompile',
    '-language=en', '-locale=en-US', '-windowed', '-ForceRes', "-ResX=$width", "-ResY=$height", '-d3d11', '-RenderOffScreen',
    ('-AbsLog="' + $log + '"'), "-RogueProfileSaveSlot=Rogue10m_Town_QA_MeleeVfx_$Label",
    "-RogueProgressionSavePrefix=Rogue10m_Progression_QA_MeleeVfx_$Label", "-RogueSkillKeySaveSlot=Rogue10m_Fighter_QA_MeleeVfx_$Label",
    "-RogueWeaponLabel=$Label", '-RogueMeleeVfxRun', '-ExecCmds="t.MaxFPS 60,Rogue10m.TestWeaponThirdPerson"'
)
if ($RequireAuthoredSword) { $argsList += '-RogueRequireAuthoredSword' }

$process = $null
$started = $null
try {
    $process = Start-Process -FilePath $editor -ArgumentList $argsList -WindowStyle Hidden -PassThru
    $started = $process.StartTime
    Write-Output "Melee VFX QA PID=$($process.Id) log=$log"
    if (-not $process.WaitForExit($TimeoutSeconds * 1000)) { throw 'Melee VFX QA timed out.' }
    $process.Refresh()
    & $python -B (Join-Path $PSScriptRoot 'CheckUnrealOutputLog.py') --log $log --after-line 0 --json-output (Join-Path $output 'log-check.json')
    $logCheck = $LASTEXITCODE
    if ($process.ExitCode -ne 0 -or $logCheck -ne 0 -or
        -not (Select-String -LiteralPath $log -SimpleMatch 'RESULT=MELEE_VFX_PASSED completed=14 failures=0' -Quiet)) {
        throw "Melee VFX runtime failed. Exit=$($process.ExitCode) LogCheck=$logCheck Evidence=$output"
    }

    $captures = @(Import-Csv -LiteralPath (Join-Path $output 'captures.csv'))
    $requests = @($captures | Where-Object event -eq 'request')
    $processed = @($captures | Where-Object event -eq 'processed')
    if ($requests.Count -ne 46 -or $processed.Count -ne 46 -or @($requests.file | Select-Object -Unique).Count -ne 46) {
        throw 'Expected 46 unique requested and processed screenshots.'
    }
    foreach ($capture in $processed) {
        if ([long]$capture.bytes -lt 1024 -or (Get-Item -LiteralPath $capture.file).Length -ne [long]$capture.bytes) {
            throw "Invalid rendered capture: $($capture.file)"
        }
    }
    Get-Content -LiteralPath (Join-Path $output 'metrics.txt') | Select-Object -First 8
    Select-String -LiteralPath $log -Pattern 'MELEE_VFX CASE' | ForEach-Object { $_.Line }
    Write-Output "Melee VFX numerical and rendered capture gates passed. Visual inspection uses raw screenshots under $output."
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
