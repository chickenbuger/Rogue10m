param(
    [string]$Label = '',
    [ValidateRange(60, 1200)][int]$TimeoutSeconds = 600,
    [string]$EngineRoot = 'D:\Program Files\UE_5.8',
    [string]$ProjectRoot = (Split-Path -Parent $PSScriptRoot),
    [ValidateSet('All', 'Victory', 'Failure', 'Timeout', 'Death')][string]$Case = 'All'
)
$ErrorActionPreference = 'Stop'
if ([string]::IsNullOrWhiteSpace($Label)) { $Label = [DateTime]::UtcNow.ToString('yyyyMMdd_HHmmss') }
if ($Label -notmatch '^[A-Za-z0-9_]{1,24}$') { throw 'Label must contain 1..24 letters, digits, or underscores.' }
$project = Join-Path $ProjectRoot 'Rogue10m.uproject'
$editor = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
if (-not (Test-Path -LiteralPath $project) -or -not (Test-Path -LiteralPath $editor)) { throw 'Editor or project is missing.' }
$cases = if ($Case -eq 'All') { @('Victory', 'Failure', 'Timeout', 'Death') } else { @($Case) }
foreach ($scenario in $cases) {
    $suffix = $Label + '_' + $scenario.ToLowerInvariant()
    $profileSlot = 'Rogue10m_Town_QA_RunCompletion_' + $suffix
    $savePrefix = 'Rogue10m_Progression_QA_RunCompletion_' + $suffix
    $keySlot = 'Rogue10m_Fighter_QA_RunCompletion_' + $suffix
    if ($savePrefix.Length -gt 80 -or $profileSlot.Length -gt 100 -or $keySlot.Length -gt 100) { throw 'Bounded QA save override is too long.' }
    $evidence = Join-Path $ProjectRoot ('Feature\doc\evidence\boss-run-completion-20261020\' + $profileSlot)
    $saveDirectory = Join-Path $ProjectRoot 'Saved\SaveGames'
    if ((Test-Path -LiteralPath $evidence) -or
        (Test-Path -LiteralPath (Join-Path $saveDirectory ($profileSlot + '.sav'))) -or
        (Get-ChildItem -LiteralPath $saveDirectory -Filter ($savePrefix + '_*') -ErrorAction SilentlyContinue | Select-Object -First 1)) {
        throw ('QA label exists for ' + $scenario + '. Choose a fresh label; previous evidence and saves are preserved.')
    }
    New-Item -ItemType Directory -Path $evidence -Force | Out-Null
    $phases = switch ($scenario) {
        'Victory' { @('Write', 'Read') }
        'Failure' { @('Failure', 'FailureRead') }
        'Timeout' { @('Timeout') }
        'Death' { @('Death') }
    }
    foreach ($phase in $phases) {
        $log = Join-Path $evidence ($phase.ToLowerInvariant() + '.log')
        # A rendered standalone process is required for the actual native result widget and Slate key route.
        $arguments = @(
            ('"' + $project + '"'), '/Game/Rogue10m/Maps/StartHub', '-game', '-d3d11', '-RenderOffscreen',
            '-ResX=1280', '-ResY=720', '-Windowed', '-unattended', '-nosplash', '-nosound',
            '-NoLiveCoding', '-NoRemoteShaderCompile',
            ('-RogueProfileSaveSlot=' + $profileSlot), ('-RogueProgressionSavePrefix=' + $savePrefix),
            ('-RogueSkillKeySaveSlot=' + $keySlot),
            ('-ExecCmds="t.MaxFPS 60,r.ScreenPercentage 75,Rogue10m.TestRunCompletion' + $phase + '"'), ('-abslog="' + $log + '"')
        )
        $process = $null
        try {
            # The Editor build already validated the Windows toolchain. Gameplay QA does not
            # need a platform-SDK discovery UBT job queued behind another feature's compiler.
            # UE 5.8 TargetPlatformManagerModule reads this process-only environment variable.
            $previousSdkDiscovery = [Environment]::GetEnvironmentVariable('UE_SKIP_UBT_SDK_SETUP', 'Process')
            try {
                [Environment]::SetEnvironmentVariable('UE_SKIP_UBT_SDK_SETUP', '1', 'Process')
                $process = Start-Process -FilePath $editor -ArgumentList $arguments -WindowStyle Hidden -PassThru
            } finally {
                [Environment]::SetEnvironmentVariable('UE_SKIP_UBT_SDK_SETUP', $previousSdkDiscovery, 'Process')
            }
            Write-Output ($scenario + '/' + $phase + ' pid=' + $process.Id + ' log=' + $log)
            $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
            while (-not $process.WaitForExit(1000)) {
                if ([DateTime]::UtcNow -gt $deadline) { throw ($phase + ' timed out; log=' + $log) }
            }
            $expected = 'RESULT=RUN_COMPLETION_PASSED mode=' + $phase.ToLowerInvariant()
            $result = Join-Path $evidence ($phase.ToLowerInvariant() + '-results.txt')
            if ($process.ExitCode -ne 0 -or -not (Test-Path -LiteralPath $log) -or
                -not ([IO.File]::ReadAllText($log).Contains($expected)) -or -not (Test-Path -LiteralPath $result)) {
                throw ($phase + ' failed; exit=' + $process.ExitCode + ' log=' + $log)
            }
            Write-Output ($expected + ' exit=' + $process.ExitCode + ' results=' + $result)
        } finally {
            # This script owns only the exact standalone child process it started.
            if ($null -ne $process -and -not $process.HasExited) {
                Stop-Process -Id $process.Id
                $process.WaitForExit(5000) | Out-Null
            }
        }
    }
}
Write-Output ('Rendered native run completion cases verified. Evidence=' + (Join-Path $ProjectRoot 'Feature\doc\evidence\boss-run-completion-20261020'))
