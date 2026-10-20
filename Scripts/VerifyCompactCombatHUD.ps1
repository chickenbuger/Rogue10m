param(
    [string]$EngineRoot = 'D:\Program Files\UE_5.8',
    [string]$ProjectRoot = 'D:\Project\Rogue10m',
    [string]$Map = '/Game/FirstPerson/Lvl_FirstPerson',
    [ValidateRange(30, 600)][int]$TimeoutSeconds = 180
)
$ErrorActionPreference = 'Stop'
$projectDirectory = [IO.Path]::GetFullPath($ProjectRoot)
$editor = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$project = Join-Path $projectDirectory 'Rogue10m.uproject'
if (-not (Test-Path -LiteralPath $editor -PathType Leaf)) { throw "Editor missing: $editor" }
if (-not (Test-Path -LiteralPath $project -PathType Leaf)) { throw "Project missing: $project" }
if ($Map -notmatch '^/Game/[A-Za-z0-9_/]+$') { throw 'Map must be a /Game package path.' }
$evidence = Join-Path $projectDirectory 'Feature\doc\evidence\compact-hud-20261005'
New-Item -ItemType Directory -Path $evidence -Force | Out-Null
$log = Join-Path $evidence 'runtime.log'
$arguments = @('"' + $project + '"', $Map, '-game', '-windowed', '-ForceRes', '-ResX=1280', '-ResY=720',
    '-unattended', '-nosplash', '-nosound', '-NoLiveCoding', '-NoRemoteShaderCompile',
    '-ExecCmds="Rogue10m.TestCompactCombatHUD"', ('-abslog="' + $log + '"'),
    '-RogueSkillKeySaveSlot=Rogue10m_Fighter_QA_CompactHUD_20261005')
$started = Get-Date
$process = Start-Process -FilePath $editor -ArgumentList $arguments -WindowStyle Hidden -PassThru
Write-Output ('Compact HUD runtime QA pid=' + $process.Id)
while (-not $process.WaitForExit(1000)) {
    if (((Get-Date) - $started).TotalSeconds -ge $TimeoutSeconds) {
        Stop-Process -Id $process.Id
        throw "Compact HUD verification timed out; inspect $log"
    }
}
if (-not (Test-Path -LiteralPath $log -PathType Leaf)) { throw 'Runtime log missing.' }
$body = Get-Content -LiteralPath $log -Raw
if ($process.ExitCode -ne 0 -or -not $body.Contains('RESULT=COMPACT_COMBAT_HUD_PASSED cases=6')) {
    throw "Compact HUD runtime verification failed; inspect $log"
}

# Check captures are from this run and retain the requested viewport dimensions.
$cases = @(@(1280, 720), @(1920, 1080), @(2560, 1440), @(1920, 800), @(800, 600), @(1920, 1080))
for ($index = 0; $index -lt $cases.Count; $index++) {
    $width = $cases[$index][0]
    $height = $cases[$index][1]
    $path = Join-Path $evidence ('CompactCombatHUD_{0:D2}_{1}x{2}.png' -f $index, $width, $height)
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Capture missing: $path" }
    $file = Get-Item -LiteralPath $path
    if ($file.LastWriteTime -lt $started -or $file.Length -le 24) { throw "Capture stale or empty: $path" }
    $bytes = [IO.File]::ReadAllBytes($path)
    if ($bytes[0] -ne 137 -or $bytes[1] -ne 80 -or $bytes[2] -ne 78 -or $bytes[3] -ne 71) {
        throw "Capture is not PNG: $path"
    }
    $capturedWidth = [uint32]$bytes[16] * 16777216 + [uint32]$bytes[17] * 65536 + [uint32]$bytes[18] * 256 + [uint32]$bytes[19]
    $capturedHeight = [uint32]$bytes[20] * 16777216 + [uint32]$bytes[21] * 65536 + [uint32]$bytes[22] * 256 + [uint32]$bytes[23]
    if ($capturedWidth -ne $width -or $capturedHeight -ne $height) { throw "Capture size incorrect: $path" }
}
Write-Output 'RESULT=COMPACT_COMBAT_HUD_PASSED cases=6; six current game captures verified.'
Write-Output ('Evidence: ' + $evidence)
