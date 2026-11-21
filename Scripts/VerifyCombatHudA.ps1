param(
    [string]$EngineRoot = 'D:\Program Files\UE_5.8',
    [ValidatePattern('^[A-Za-z0-9_]{1,40}$')]
    [string]$Label = ('HudA_' + [DateTime]::UtcNow.ToString('yyyyMMdd_HHmmss_fff')),
    [ValidateRange(30, 600)]
    [int]$TimeoutSeconds = 180
)

$ErrorActionPreference = 'Stop'
$projectRoot = [IO.Path]::GetFullPath((Split-Path -Parent $PSScriptRoot))
$project = Join-Path $projectRoot 'Rogue10m.uproject'
$editor = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$python = Join-Path $EngineRoot 'Engine\Binaries\ThirdParty\Python3\Win64\python.exe'
foreach ($path in @($project, $editor, $python)) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Required file missing: $path" }
}

$active = @(Get-CimInstance Win32_Process -Filter "Name LIKE 'UnrealEditor%' OR Name='Rogue10m.exe' OR Name='dotnet.exe' OR Name='UnrealBuildTool.exe'" |
    Where-Object {
        if ($_.Name -eq 'Rogue10m.exe') { return $true }
        if ([string]::IsNullOrEmpty($_.CommandLine)) { return $_.Name -ne 'dotnet.exe' }
        return $_.CommandLine.Replace('/', '\').Contains($project, [StringComparison]::OrdinalIgnoreCase)
    })
if ($active.Count) { throw 'Rogue10m already has an editor, game or build session. Preserve it before starting this fixture.' }

$profileSlot = 'Rogue10m_Town_QA_HudA_' + $Label
$progressionPrefix = 'Rogue10m_Progression_QA_HudA_' + $Label
$skillsSlot = 'Rogue10m_Fighter_QA_HudA_' + $Label
$saveDirectory = Join-Path $projectRoot 'Saved\SaveGames'
$evidence = Join-Path $projectRoot ('tmp\combat-hud-a-qa\' + $Label)
if (Test-Path -LiteralPath $evidence) { throw "Use a new label. Evidence already exists at $evidence" }

function Test-IsolatedSave([string]$SaveName) {
    $name = [IO.Path]::GetFileNameWithoutExtension($SaveName)
    return $name -eq $profileSlot -or $name -eq $skillsSlot -or $name.StartsWith($progressionPrefix + '_', [StringComparison]::OrdinalIgnoreCase)
}

function Get-SaveManifest {
    $manifest = @{}
    if (Test-Path -LiteralPath $saveDirectory -PathType Container) {
        foreach ($file in Get-ChildItem -LiteralPath $saveDirectory -File -Filter '*.sav') {
            $manifest[$file.Name] = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash
        }
    }
    return $manifest
}

function Get-KnownBootstrapTraces([string[]]$LogLines, [object[]]$Errors, [int]$FixtureLine, [string]$EnginePath) {
    # Exact frame-zero trace signatures in UE 5.8.4 -game. Fingerprint the full normalized traceback,
    # then require the matching plugin's startup envelope and leave any changed or extra error unclassified.
    $known = @{
        'Toolsets/StateTreeToolset' = @{ Count = 9; Type = 'ToolsetDefinition'; Hash = '59E6859960DF8375BB570D5F0C4C8B77E99D06E5DA2945438A0FECE1A3465275' }
        'Toolsets/NiagaraToolsets' = @{ Count = 7; Type = 'AgentSkill'; Hash = 'DA9341B566EFC148CA9566BA0E79D1E03F59563155A9663F1A50AECA35E0DD95' }
        'Toolsets/EditorToolset' = @{ Count = 10; Type = 'AgentSkill'; Hash = '4BA78F8D217F4C3C89241BAF061B6CCAABCA3AFAFDED1B54EF7FEA438766A2ED' }
        'Toolsets/ConversationToolset' = @{ Count = 9; Type = 'ToolsetDefinition'; Hash = '1DDD2B4918D1B8B63F965C334E07131A1AD2BFDAC1405024D314E0DF10453C99' }
        'Toolsets/AnimationAssistantToolset' = @{ Count = 9; Type = 'ToolsetDefinition'; Hash = '8DA50AF5491EFE9E05B9641DC3A808F6199EE9631248CB59FC25DC8A6E98B77B' }
        'Toolsets/AIModuleToolset' = @{ Count = 9; Type = 'ToolsetDefinition'; Hash = '69E1A3C25DDF5C68E3156A451DDC479E1052165BEC6DC8F6277C665FA66DBF4A' }
        'ToolsetRegistry' = @{ Count = 5; Type = 'PythonTestRunner'; Hash = '1A5F67E40BE93DAA3035497AA739016AB88A549199B62DE1400D2FE6CC21D5D1' }
    }
    $errorsByLine = @{}
    foreach ($issue in $Errors) { $errorsByLine[[int]$issue.line] = $issue }
    $prefix = '^(?:\[[^\]]+\])\[\s*0\]LogPython: Display: Running start-up script '
    $engine = [IO.Path]::GetFullPath($EnginePath).Replace('\', '/').TrimEnd('/')
    foreach ($plugin in $known.Keys) {
        $startScript = [Regex]::Escape("$engine/Engine/Plugins/Experimental/$plugin/Content/Python/init_unreal.py")
        $starts = @(for ($index = 0; $index -lt $FixtureLine - 1; ++$index) {
            if ($LogLines[$index].Replace('\', '/') -match ($prefix + $startScript + '\.\.\. started\.\.\.$')) { $index + 1 }
        })
        if ($starts.Count -ne 1) { continue }
        $start = $starts[0]
        $shape = $known[$plugin]
        $end = $start + $shape.Count + 1
        $duration = '\d+(?:\.\d+)? (?:ms|sec)'
        $finish = $prefix + $startScript + "\.\.\. took $duration(?: \(total: $duration\))?$"
        if ($end -ge $FixtureLine -or $LogLines[$end - 1].Replace('\', '/') -notmatch $finish) { continue }
        $lines = (($start + 1)..($end - 1))
        $trace = @(foreach ($line in $lines) {
            $issue = $errorsByLine[$line]
            if ($issue.category -ceq 'LogPython' -and $issue.severity -ceq 'Error') {
                $issue.message.Replace('\', '/').Replace($engine + '/', '<engine>/', [StringComparison]::OrdinalIgnoreCase)
            }
        })
        if ($trace.Count -ne $shape.Count -or $trace[0] -cne 'Traceback (most recent call last):' -or
                $trace[-1] -cne "AttributeError: module 'unreal' has no attribute '$($shape.Type)'") { continue }
        $hash = [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData([Text.Encoding]::UTF8.GetBytes($trace -join "`n")))
        if ($hash -ceq $shape.Hash) {
            [pscustomobject]@{ plugin = $plugin; missingType = $shape.Type; lines = $lines }
        }
    }
}

$before = Get-SaveManifest
if (@($before.Keys | Where-Object { Test-IsolatedSave $_ }).Count) { throw 'Use a new label. A QA save already exists.' }

New-Item -ItemType Directory -Path $evidence | Out-Null
$engineLog = Join-Path $evidence 'runtime.log'
$report = Join-Path $evidence 'runtime-contract.txt'
$arguments = @(
    ('"' + $project + '"'), '/Game/Rogue10m/Maps/StartHub', '-game', '-windowed', '-ForceRes', '-ResX=1672', '-ResY=941',
    '-unattended', '-nosplash', '-nosound', '-NoLiveCoding', '-NoRemoteShaderCompile', '-language=en', '-locale=en',
    ('"-AbsLog=' + $engineLog + '"'), '-ExecCmds=Rogue10m.TestCombatHudA',
    ('-RogueCombatHudALabel=' + $Label), ('-RogueProfileSaveSlot=' + $profileSlot),
    ('-RogueProgressionSavePrefix=' + $progressionPrefix), ('-RogueSkillKeySaveSlot=' + $skillsSlot)
)
$process = Start-Process -FilePath $editor -ArgumentList $arguments -WorkingDirectory $projectRoot -WindowStyle Hidden -PassThru
Write-Output "Combat HUD A QA PID $($process.Id), runtime log $engineLog"
$timedOut = -not $process.WaitForExit($TimeoutSeconds * 1000)
if ($timedOut -and -not $process.HasExited) {
    $process.Kill()
    $process.WaitForExit()
}

$after = Get-SaveManifest
$saveProblems = @(
    foreach ($name in $before.Keys) {
        if (-not $after.ContainsKey($name) -or $before[$name] -ne $after[$name]) { "Prior save changed or missing: $name" }
    }
    foreach ($name in $after.Keys) {
        if (-not $before.ContainsKey($name) -and -not (Test-IsolatedSave $name)) { "Unexpected new save: $name" }
    }
)
$saveReport = Join-Path $evidence 'save-preservation.txt'
if ($saveProblems.Count) {
    $saveProblems | Set-Content -LiteralPath $saveReport -Encoding utf8
    throw "Save preservation failed. Read $saveReport"
}
"PASS prior_save_SHA256_preserved count=$($before.Count)" | Set-Content -LiteralPath $saveReport -Encoding utf8

if (-not (Test-Path -LiteralPath $engineLog -PathType Leaf)) { throw "Runtime log missing: $engineLog" }
$logSummary = Join-Path $evidence 'output-check.json'
$checker = Join-Path $PSScriptRoot 'CheckUnrealOutputLog.py'
& $python -X utf8 -B $checker --log $engineLog --json-output $logSummary
$fullLogStatus = $LASTEXITCODE
if ($timedOut) { throw "Runtime timed out. Read $engineLog" }

$mapStart = @(Select-String -LiteralPath $engineLog -Pattern 'LogWorld: Bringing World /Game/Rogue10m/Maps/StartHub\.StartHub up for play \(')
$fixtureStart = @(Select-String -LiteralPath $engineLog -Pattern 'LogRogue10m: Display: HUD_A QA START real_StartHub$')
if ($mapStart.Count -ne 1 -or $fixtureStart.Count -ne 1 -or $mapStart[0].LineNumber -ge $fixtureStart[0].LineNumber) {
    throw "Could not establish the real StartHub and HUD fixture boundaries. Read $engineLog"
}
$fixtureLine = $fixtureStart[0].LineNumber
$runtimeSummary = Join-Path $evidence 'output-check-runtime.json'
& $python -X utf8 -B $checker --log $engineLog --after-line $fixtureLine --json-output $runtimeSummary
$runtimeLogStatus = $LASTEXITCODE
$allIssues = Get-Content -LiteralPath $logSummary -Raw | ConvertFrom-Json
$logLines = Get-Content -LiteralPath $engineLog -Encoding utf8
$bootstrap = @(Get-KnownBootstrapTraces $logLines $allIssues.new_errors $fixtureLine $EngineRoot)
$classifiedLines = @($bootstrap | ForEach-Object { $_.lines })
$unexpected = @($allIssues.new_errors | Where-Object { $_.line -notin $classifiedLines })
$logClassification = Join-Path $evidence 'output-classification.json'
[ordered]@{
    map_start_line = $mapStart[0].LineNumber
    qa_start_line = $fixtureLine
    full_error_count = $allIssues.total_error_count
    verified_bootstrap_error_count = $classifiedLines.Count
    other_error_count = $unexpected.Count
    verified_bootstrap_traces = $bootstrap
    other_errors = $unexpected
} | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $logClassification -Encoding utf8
Write-Output "Full output errors: $($allIssues.total_error_count); exact frame-zero UE bootstrap: $($classifiedLines.Count); other: $($unexpected.Count)."
$passed = 'RESULT=COMBAT_HUD_A_PASSED cases=2'
if ($process.ExitCode -ne 0 -or -not (Test-Path -LiteralPath $report -PathType Leaf) -or
        -not (Select-String -LiteralPath $report -SimpleMatch $passed -Quiet) -or
        -not (Select-String -LiteralPath $engineLog -SimpleMatch $passed -Quiet) -or
        $fullLogStatus -notin @(0, 1) -or $runtimeLogStatus -ne 0 -or $unexpected.Count -ne 0) {
    throw "Runtime or log verification failed. Read $report, $engineLog and $logClassification"
}

Add-Type -AssemblyName System.Drawing
$captures = @(
    @{ Name = 'CombatHudA_1672x941.png'; Width = 1672; Height = 941 },
    @{ Name = 'CombatHudA_1280x720.png'; Width = 1280; Height = 720 },
    @{ Name = 'CombatHudA_Unarmed_1672x941.png'; Width = 1672; Height = 941 },
    @{ Name = 'CombatHudA_Knuckle_1280x720.png'; Width = 1280; Height = 720 }
)
foreach ($capture in $captures) {
    $imagePath = Join-Path $evidence $capture.Name
    $image = [Drawing.Bitmap]::new($imagePath)
    try {
        if ($image.Width -ne $capture.Width -or $image.Height -ne $capture.Height -or
                $image.RawFormat.Guid -ne [Drawing.Imaging.ImageFormat]::Png.Guid) {
            throw "Capture is not a PNG at the requested resolution: $imagePath"
        }
        $colors = [Collections.Generic.HashSet[int]]::new()
        foreach ($row in 1..5) {
            foreach ($column in 1..5) {
                [void]$colors.Add($image.GetPixel([int]($image.Width * $column / 6), [int]($image.Height * $row / 6)).ToArgb())
            }
        }
        if ($colors.Count -lt 4) { throw "Capture appears blank: $imagePath" }
    } finally {
        $image.Dispose()
    }
}
Write-Output "$passed. Live widgets, rendered PNGs, log and existing saves verified. Evidence $evidence"
