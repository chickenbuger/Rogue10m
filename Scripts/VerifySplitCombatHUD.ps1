param(
    [string]$EngineRoot = 'D:\Program Files\UE_5.8',
    [string]$ProjectRoot = 'D:\Project\Rogue10m',
    [string]$Map = '/Game/FirstPerson/Lvl_FirstPerson',
    [ValidatePattern('^[A-Za-z0-9_]{1,40}$')][string]$Label = ('split_' + (Get-Date).ToUniversalTime().ToString('yyyyMMdd_HHmmss')),
    [ValidateRange(30, 600)][int]$TimeoutSeconds = 180
)
$ErrorActionPreference = 'Stop'
$projectDirectory = [IO.Path]::GetFullPath($ProjectRoot)
$editor = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$project = Join-Path $projectDirectory 'Rogue10m.uproject'
if (-not (Test-Path -LiteralPath $editor -PathType Leaf)) { throw "Editor missing: $editor" }
if (-not (Test-Path -LiteralPath $project -PathType Leaf)) { throw "Project missing: $project" }
if ($Map -notmatch '^/Game/[A-Za-z0-9_/]+$') { throw 'Map must be a /Game package path.' }

# The seven-case fixture also checks the independently managed save snapshots.
foreach ($requiredHeader in @('Rogue10mProgressionSubsystem.h', 'Rogue10mProgressionSaveGame.h')) {
    $requiredPath = Join-Path $projectDirectory ('Source\Rogue10m\Core\' + $requiredHeader)
    if (-not (Test-Path -LiteralPath $requiredPath -PathType Leaf)) {
        throw "Split HUD integration QA requires the separate progression feature: $requiredHeader"
    }
}

$profileSlot = 'Rogue10m_Town_QA_' + $Label
$progressionPrefix = 'Rogue10m_Progression_QA_' + $Label
$skillSlot = 'Rogue10m_Fighter_QA_' + $Label
$saveDirectory = Join-Path $projectDirectory 'Saved\SaveGames'
$evidence = Join-Path $projectDirectory ('Feature\doc\evidence\split-hud-20261014\' + $profileSlot)
if (Test-Path -LiteralPath $evidence) { throw "Use a fresh label; evidence already exists: $evidence" }
if (Test-Path -LiteralPath $saveDirectory -PathType Container) {
    $previousQaFiles = @(Get-ChildItem -LiteralPath $saveDirectory -File -Filter '*.sav' | Where-Object {
        $_.BaseName -eq $profileSlot -or $_.BaseName -eq $skillSlot -or $_.BaseName.StartsWith($progressionPrefix + '_')
    })
    if ($previousQaFiles.Count -ne 0) { throw 'Use a fresh label; an isolated QA save already exists.' }
}

function Get-SaveManifest {
    $manifest = @{}
    if (Test-Path -LiteralPath $saveDirectory -PathType Container) {
        foreach ($saveFile in Get-ChildItem -LiteralPath $saveDirectory -File -Filter '*.sav') {
            $manifest[$saveFile.Name] = [pscustomobject]@{
                Length = $saveFile.Length
                SHA256 = (Get-FileHash -LiteralPath $saveFile.FullName -Algorithm SHA256).Hash
            }
        }
    }
    return $manifest
}

New-Item -ItemType Directory -Path $evidence | Out-Null
$beforeSaves = Get-SaveManifest
$beforeSaves | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $evidence 'saves-before.json') -Encoding utf8
$log = Join-Path $evidence 'runtime.log'
$arguments = @('"' + $project + '"', $Map, '-game', '-windowed', '-ForceRes', '-ResX=1280', '-ResY=720',
    '-unattended', '-nosplash', '-nosound', '-NoLiveCoding', '-NoRemoteShaderCompile',
    '-ExecCmds="Rogue10m.TestSplitCombatHUD"', ('-abslog="' + $log + '"'),
    ('-RogueProfileSaveSlot=' + $profileSlot), ('-RogueProgressionSavePrefix=' + $progressionPrefix),
    ('-RogueSkillKeySaveSlot=' + $skillSlot))
$started = Get-Date
$process = Start-Process -FilePath $editor -ArgumentList $arguments -WindowStyle Hidden -PassThru
Write-Output ('Split HUD runtime QA pid=' + $process.Id + ' label=' + $Label)
$timedOut = $false
while (-not $process.WaitForExit(1000)) {
    if (((Get-Date) - $started).TotalSeconds -ge $TimeoutSeconds) {
        Stop-Process -Id $process.Id
        $process.WaitForExit()
        $timedOut = $true
        break
    }
}

# Read only: compare every save that existed before launch, including other QA runs.
# This never copies, deletes, restores or edits player save files.
$afterSaves = Get-SaveManifest
$afterSaves | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $evidence 'saves-after.json') -Encoding utf8
$savePreservationIssues = @()
foreach ($saveName in $beforeSaves.Keys) {
    if (-not $afterSaves.ContainsKey($saveName) -or
        $beforeSaves[$saveName].Length -ne $afterSaves[$saveName].Length -or
        $beforeSaves[$saveName].SHA256 -ne $afterSaves[$saveName].SHA256) {
        $savePreservationIssues += 'Changed or missing pre-existing save: ' + $saveName
    }
}
foreach ($saveName in $afterSaves.Keys) {
    if ($beforeSaves.ContainsKey($saveName)) { continue }
    $baseName = [IO.Path]::GetFileNameWithoutExtension($saveName)
    if ($baseName -ne $profileSlot -and $baseName -ne $skillSlot -and -not $baseName.StartsWith($progressionPrefix + '_')) {
        $savePreservationIssues += 'Unexpected new save outside isolated QA slots: ' + $saveName
    }
}
if ($savePreservationIssues.Count -ne 0) {
    $savePreservationIssues | Set-Content -LiteralPath (Join-Path $evidence 'save-preservation.txt') -Encoding utf8
    throw "Save preservation failed; inspect $evidence"
}
('PASS existing_save_bytes_preserved count=' + $beforeSaves.Count) |
    Set-Content -LiteralPath (Join-Path $evidence 'save-preservation.txt') -Encoding utf8
if ($timedOut) { throw "Split HUD verification timed out; inspect $log" }
if (-not (Test-Path -LiteralPath $log -PathType Leaf)) { throw 'Runtime log missing.' }
$body = Get-Content -LiteralPath $log -Raw
if ($process.ExitCode -ne 0 -or -not $body.Contains('RESULT=SPLIT_COMBAT_HUD_PASSED cases=7')) {
    throw "Split HUD runtime verification failed; inspect $log"
}
$report = Join-Path $evidence 'runtime-contract.txt'
if (-not (Test-Path -LiteralPath $report -PathType Leaf) -or
    -not (Get-Content -LiteralPath $report -Raw).Contains('RESULT=SPLIT_COMBAT_HUD_PASSED cases=7')) {
    throw "Runtime assertion report missing or incomplete: $report"
}

# PNG evidence must be new, correctly encoded and at the actual requested resolution.
Add-Type -AssemblyName System.Drawing
$cases = @(@(1280, 720), @(1920, 1080), @(2560, 1440), @(1920, 800), @(800, 600), @(1920, 1080), @(1920, 1080))
for ($index = 0; $index -lt $cases.Count; $index++) {
    $width = $cases[$index][0]
    $height = $cases[$index][1]
    $path = Join-Path $evidence ('SplitCombatHUD_{0:D2}_{1}x{2}.png' -f $index, $width, $height)
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Capture missing: $path" }
    $file = Get-Item -LiteralPath $path
    if ($file.LastWriteTime -lt $started -or $file.Length -le 24) { throw "Capture stale or empty: $path" }
    $bytes = [IO.File]::ReadAllBytes($path)
    $signature = @(137, 80, 78, 71, 13, 10, 26, 10)
    for ($byteIndex = 0; $byteIndex -lt $signature.Count; $byteIndex++) {
        if ($bytes[$byteIndex] -ne $signature[$byteIndex]) { throw "Capture is not PNG: $path" }
    }
    $capturedWidth = [uint32]$bytes[16] * 16777216 + [uint32]$bytes[17] * 65536 + [uint32]$bytes[18] * 256 + [uint32]$bytes[19]
    $capturedHeight = [uint32]$bytes[20] * 16777216 + [uint32]$bytes[21] * 65536 + [uint32]$bytes[22] * 256 + [uint32]$bytes[23]
    if ($capturedWidth -ne $width -or $capturedHeight -ne $height) { throw "Capture size incorrect: $path" }
    $bitmap = [System.Drawing.Bitmap]::new($path)
    try {
        $sampleColors = [System.Collections.Generic.HashSet[int]]::new()
        for ($sampleY = 0; $sampleY -lt 10; $sampleY++) {
            for ($sampleX = 0; $sampleX -lt 10; $sampleX++) {
                $pixelX = [int][Math]::Min($width - 1, [Math]::Floor(($sampleX + 0.5) * $width / 10))
                $pixelY = [int][Math]::Min($height - 1, [Math]::Floor(($sampleY + 0.5) * $height / 10))
                [void]$sampleColors.Add($bitmap.GetPixel($pixelX, $pixelY).ToArgb())
            }
        }
        if ($sampleColors.Count -lt 4) { throw "Capture lacks rendered game/UI content: $path" }
    } finally {
        $bitmap.Dispose()
    }
}
Write-Output 'RESULT=SPLIT_COMBAT_HUD_PASSED cases=7; live HUD assertions, current PNG captures and existing save bytes verified.'
Write-Output ('Evidence: ' + $evidence)
