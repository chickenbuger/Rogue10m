param(
    [Parameter(Mandatory=$true)][ValidatePattern('^[A-Za-z0-9_]{1,24}$')][string]$Label,
    [ValidateSet('Play','Reload')][string]$Phase = 'Play',
    [ValidateSet('1080','720')][string]$Height = '1080',
    [string]$EngineRoot = 'D:\Program Files\UE_5.8',
    [ValidateRange(60,300)][int]$TimeoutSeconds = 180
)

$ErrorActionPreference = 'Stop'
$project = Split-Path -Parent $PSScriptRoot
$exe = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$python = Join-Path $EngineRoot 'Engine\Binaries\ThirdParty\Python3\Win64\python.exe'
$saveDirectory = Join-Path $project 'Saved\SaveGames'
$evidence = Join-Path $project ('tmp\inventory-ui-qa\' + $Label)
$phasePath = Join-Path $evidence $Phase.ToLowerInvariant()
$log = Join-Path $evidence ($Phase.ToLowerInvariant() + '.log')
$profile = 'Rogue10m_Town_QA_InventoryUI_' + $Label
$prefix = 'Rogue10m_Progression_QA_InventoryUI_' + $Label
$skill = 'Rogue10m_Fighter_QA_InventoryUI_' + $Label

function Get-SaveHashes {
    $entries = @{}
    if (Test-Path -LiteralPath $saveDirectory) {
        foreach ($file in Get-ChildItem -LiteralPath $saveDirectory -File -Recurse -Filter '*.sav') {
            $entries[$file.FullName] = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash
        }
    }
    return $entries
}

if ((Test-Path -LiteralPath $phasePath) -or (Test-Path -LiteralPath $log)) {
    throw 'Choose a fresh test label to preserve the existing result.'
}
if ($Phase -eq 'Reload') {
    $playResult = Join-Path $evidence 'play\results.txt'
    if (-not (Test-Path -LiteralPath $playResult) -or
        [IO.File]::ReadAllText($playResult) -notmatch '(?m)^failures=0\r?$') {
        throw 'Reload requires the successful Play phase under the same label.'
    }
}
$before = Get-SaveHashes
$profilePath = Join-Path $saveDirectory ($profile + '.sav')
$qaPattern = '^' + [regex]::Escape($prefix) + '_[0-9A-Fa-f]{32}_v5\.sav$'
$qaProgression = @($before.Keys | Where-Object { [IO.Path]::GetFileName($_) -cmatch $qaPattern })
if ($Phase -eq 'Play' -and ($before.ContainsKey($profilePath) -or $qaProgression.Count -or
    $before.ContainsKey((Join-Path $saveDirectory ($skill + '.sav'))))) {
    throw 'Choose a fresh test label to preserve existing saves.'
}
if ($Phase -eq 'Reload' -and (-not $before.ContainsKey($profilePath) -or $qaProgression.Count -ne 1)) {
    throw 'Reload requires the isolated QA profile and its one progression save.'
}

$null = New-Item -Path $evidence -ItemType Directory -Force
$width = if ($Height -eq '1080') { 1920 } else { 1280 }
$arguments = @(
    ('"' + (Join-Path $project 'Rogue10m.uproject') + '"'),
    '/Game/Rogue10m/Maps/StartHub', '-game', '-unattended', '-nosplash', '-nosound',
    '-culture=en', '-locale=en', '-DisablePython',
    '-NoLiveCoding', '-NoRemoteShaderCompile', '-nomcp', '-d3d11', '-windowed', '-ForceRes',
    ('-ResX=' + $width), ('-ResY=' + $Height), ('-AbsLog="' + $log + '"'),
    ('-RogueInventoryQALabel=' + $Label), ('-RogueProfileSaveSlot=' + $profile),
    ('-RogueProgressionSavePrefix=' + $prefix), ('-RogueSkillKeySaveSlot=' + $skill),
    '-ExecCmds="t.MaxFPS 60,Rogue10m.TestInventoryUI"'
)
if ($Phase -eq 'Reload') { $arguments += '-RogueInventoryQARead' }

$process = Start-Process -FilePath $exe -ArgumentList $arguments -WindowStyle Hidden -PassThru
$startedTicks = $process.StartTime.ToUniversalTime().Ticks
Write-Output "Inventory $Phase started pid=$($process.Id) resolution=${width}x$Height log=$log"
if (-not $process.WaitForExit($TimeoutSeconds * 1000)) {
    $live = Get-Process -Id $process.Id -ErrorAction SilentlyContinue
    if ($live -and $live.StartTime.ToUniversalTime().Ticks -eq $startedTicks -and $live.Path -ieq $exe) {
        $process.Kill()
        $null = $process.WaitForExit(5000)
    }
    throw "Inventory $Phase timed out. See $log."
}
$exitCode = $process.ExitCode
$after = Get-SaveHashes
$knownQA = @($profile + '.sav'; $skill + '.sav')
foreach ($path in @($before.Keys) + @($after.Keys)) {
    $name = [IO.Path]::GetFileName($path)
    if ([IO.Path]::GetDirectoryName($path) -ieq $saveDirectory -and
        ($name -cin $knownQA -or $name -cmatch $qaPattern)) { continue }
    if (-not $before.ContainsKey($path) -or -not $after.ContainsKey($path) -or $before[$path] -cne $after[$path]) {
        throw "Unexpected non-QA save change: $path"
    }
}
Write-Output "Non-QA save preservation verified. Before=$($before.Count) After=$($after.Count)"

& $python -X utf8 -B (Join-Path $PSScriptRoot 'CheckUnrealOutputLog.py') --log $log
$logExit = $LASTEXITCODE
$resultPath = Join-Path $phasePath 'results.txt'
if (Test-Path -LiteralPath $resultPath) { Get-Content -LiteralPath $resultPath }
$marker = 'RESULT=INVENTORY_UI_' + $Phase.ToUpperInvariant() + '_PASSED'
if ($exitCode -ne 0 -or $logExit -ne 0 -or -not (Test-Path -LiteralPath $resultPath) -or
    -not ([IO.File]::ReadAllText($log).Contains($marker))) {
    throw "Inventory $Phase did not pass. Game exit=$exitCode output-log check=$logExit. See $log."
}
if ($Phase -eq 'Play') {
    Add-Type -AssemblyName System.Drawing
    $png = Join-Path $phasePath 'inventory-comparison.png'
    $image = [System.Drawing.Bitmap]::new($png)
    try {
        if ($image.Width -ne $width -or $image.Height -ne [int]$Height) {
            throw "Unexpected screenshot dimensions: $($image.Width)x$($image.Height)"
        }
        # Probe the empty second column and ninth row. A one-unit border can
        # vanish during DPI pixel snapping even when the widget geometry passes.
        $scale = $image.Height / 1080.0
        $emptyColumn = [int][Math]::Round((133 + 1.5 * 58) * $scale)
        $emptyRow = [int][Math]::Round((184 + 8.5 * 58) * $scale)
        foreach ($separator in 1..9) {
            $x = [int][Math]::Round((133 + $separator * 58) * $scale)
            $y = [int][Math]::Round((184 + $separator * 58) * $scale)
            $vertical = $false
            $horizontal = $false
            foreach ($offset in -3..1) {
                $pixel = $image.GetPixel($x + $offset, $emptyRow)
                $vertical = $vertical -or ($pixel.R -gt 50 -and ($pixel.R - $pixel.B) -gt 20)
                $pixel = $image.GetPixel($emptyColumn, $y + $offset)
                $horizontal = $horizontal -or ($pixel.R -gt 50 -and ($pixel.R - $pixel.B) -gt 20)
            }
            if (-not ($vertical -and $horizontal)) {
                throw "Missing visible grid separator $separator at ${width}x${Height}."
            }
        }
        Write-Output 'Screenshot shows all nine interior separators on both axes.'
    } finally { $image.Dispose() }
}
Write-Output $marker
