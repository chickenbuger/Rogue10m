#requires -Version 7.0
[CmdletBinding()]
param(
    [Parameter(Mandatory)][ValidatePattern('^[A-Za-z0-9_]{1,20}$')][string]$Label,
    [switch]$Execute,
    [string]$EditorHandoff = '',
    [string]$ProjectRoot = (Split-Path -Parent $PSScriptRoot),
    [string]$EditorExe = 'D:\Program Files\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe',
    [ValidateRange(90,600)][int]$TimeoutSeconds = 240
)
$ErrorActionPreference = 'Stop'

function New-VillageQuestPlan {
    param([Parameter(Mandatory)][ValidatePattern('^[A-Za-z0-9_]{1,20}$')][string]$Label,
        [Parameter(Mandatory)][string]$ProjectRoot, [Parameter(Mandatory)][string]$EditorExe,
        [ValidateRange(90,600)][int]$TimeoutSeconds = 240)
    $root = [IO.Path]::TrimEndingDirectorySeparator([IO.Path]::GetFullPath($ProjectRoot))
    $project = Join-Path $root 'Rogue10m.uproject'
    $evidence = Join-Path $root ('Feature/doc/evidence/village-quests-runtime-20261111/' + $Label)
    $runtime = Join-Path $evidence 'runtime'
    $profile = 'Rogue10m_Town_QA_VillageQuest_' + $Label
    $progression = 'Rogue10m_Progression_QA_VillageQuest_' + $Label
    $fighter = 'Rogue10m_Fighter_QA_VillageQuest_' + $Label
    [pscustomobject]@{
        status = 'plan_only_not_executed'; label = $Label; project_root = $root; project = $project
        editor = [IO.Path]::GetFullPath($EditorExe); evidence = $evidence; runtime = $runtime
        engine_log = Join-Path $evidence 'engine.log'; results = Join-Path $runtime 'results.json'
        save_root = Join-Path $root 'Saved/SaveGames'; profile_slot = $profile
        progression_prefix = $progression; fighter_slot = $fighter; timeout_seconds = $TimeoutSeconds
        fixture = Join-Path $root 'Source/Rogue10m/Tests/Rogue10mVillageQuestRuntimeTest.cpp'
        module = Join-Path $root 'Binaries/Win64/UnrealEditor-Rogue10m.dll'
        physical_input = $false; full_run = $false; facts_injected = $false; rendered = $true
        fixture_positioning = 'teleport_in_candidate_only'; normal_portal_entry = $true
        execution_target = 'UnrealEditor.exe -game rendered offscreen only'
        editor_handoff_required = $true
        arguments = @(
            ('"' + $project + '"'), '/Game/Rogue10m/Maps/StartHub', '-game', '-unattended',
            '-DisablePython', '-NoSplash', '-NoSound', '-NoLiveCoding', '-NoRemoteShaderCompile',
            '-culture=en', '-language=en', '-locale=en-US', '-windowed', '-ForceRes', '-ResX=1280', '-ResY=720',
            '-d3d11', '-RenderOffScreen', ('-RogueVillageQuestLabel=' + $Label),
            ('-RogueProfileSaveSlot=' + $profile), ('-RogueProgressionSavePrefix=' + $progression),
            ('-RogueSkillKeySaveSlot=' + $fighter), '-ExecCmds="t.MaxFPS 60,Rogue10m.TestVillageQuests"',
            ('-abslog="' + (Join-Path $evidence 'engine.log') + '"')
        )
    }
}

function Get-VillageQuestActiveProcess {
    @(Get-Process -Name UnrealEditor,UnrealEditor-Cmd,UnrealBuildTool,dotnet -ErrorAction SilentlyContinue)
}

function Assert-VillageQuestNoSharedUE {
    if (@(Get-VillageQuestActiveProcess).Count) {
        throw 'Shared UE/UBT/dotnet process is active. No existing process was stopped.'
    }
}

function Get-VillageQuestEditorIdentity {
    param([Parameter(Mandatory)][string]$Path)
    [Diagnostics.FileVersionInfo]::GetVersionInfo($Path)
}

function Get-VillageQuestSaveSnapshot {
    param([Parameter(Mandatory)][string]$SaveRoot)
    if (-not (Test-Path -LiteralPath $SaveRoot)) { return }
    Get-ChildItem -LiteralPath $SaveRoot -Filter '*.sav' -File -Recurse -Force -ErrorAction Stop |
        Sort-Object FullName | ForEach-Object {
            [pscustomobject]@{ path = $_.FullName; sha256 = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash }
        }
}

function Write-VillageQuestSnapshot {
    param([Parameter(Mandatory)][string]$Path, [Parameter(Mandatory)][AllowEmptyCollection()][object[]]$Snapshot)
    ConvertTo-Json -InputObject $Snapshot -Depth 5 | Set-Content -LiteralPath $Path -Encoding utf8NoBOM
}

function Get-VillageQuestCaptureNames {
    @('01_v1_offer.png','02_v1_ready_after_revisit.png','03_v1_rewarded.png',
        '04_v4_preacceptance_ready.png','05_v3_actual_kill_ready.png','06_all_five_complete_input_restored.png')
}

function Get-VillageQuestMilestoneNames {
    @('normal_portal_entry','actor_layout','f_and_slate','interrupted_revisit','npc_recreated',
        'director_recreated_without_encounter_respawn','preacceptance','attributed_kill','all_five_claimed',
        'duplicate_claim_blocked','input_restored','third_person')
}

function Test-VillageQuestOwnedSave {
    param([Parameter(Mandatory)][string]$Path, [Parameter(Mandatory)]$Plan)
    $file = [IO.Path]::GetFullPath($Path)
    $root = [IO.Path]::TrimEndingDirectorySeparator([IO.Path]::GetFullPath($Plan.save_root))
    if (-not [string]::Equals([IO.Path]::GetDirectoryName($file), $root, [StringComparison]::OrdinalIgnoreCase)) { return $false }
    $name = [IO.Path]::GetFileName($file)
    if ($name -ieq ($Plan.profile_slot + '.sav') -or $name -ieq ($Plan.fighter_slot + '.sav')) { return $true }
    return [regex]::IsMatch($name, ('^' + [regex]::Escape($Plan.progression_prefix) + '_[A-Fa-f0-9]{32}\.sav$'),
        [Text.RegularExpressions.RegexOptions]::IgnoreCase)
}

function Get-VillageQuestContentSnapshot {
    param([Parameter(Mandatory)][string]$ProjectRoot)
    $files = @(
        foreach ($folder in @('Source','Config')) {
            $path = Join-Path $ProjectRoot $folder
            if (Test-Path -LiteralPath $path) {
                Get-ChildItem -LiteralPath $path -Recurse -File -Force | Where-Object {
                    ($folder -eq 'Source' -and $_.Extension -in @('.cpp','.h','.cs')) -or
                    ($folder -eq 'Config' -and $_.Extension -eq '.ini')
                }
            }
        }
        foreach ($relative in @('Rogue10m.uproject','Content/Rogue10m/Maps/StartHub.umap',
            'Content/Rogue10m/Maps/Prototypes/FiveVillageGraybox20261110.umap')) {
            $path = Join-Path $ProjectRoot $relative
            if (Test-Path -LiteralPath $path -PathType Leaf) { Get-Item -LiteralPath $path }
        }
    )
    $files | Sort-Object FullName -Unique | ForEach-Object {
        [pscustomobject]@{ path = $_.FullName; sha256 = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash }
    }
}

function Compare-VillageQuestSnapshots {
    param([AllowEmptyCollection()][object[]]$Before = @(), [AllowEmptyCollection()][object[]]$After = @(),
        [Parameter(Mandatory)]$Plan, [switch]$AllowOwnedSaves)
    $indices = @(
        foreach ($which in @('Before','After')) {
            $snapshot = if ($which -eq 'Before') { $Before } else { $After }
            $index = [Collections.Generic.Dictionary[string,string]]::new([StringComparer]::OrdinalIgnoreCase)
            foreach ($row in $snapshot) {
                if ($row.path -isnot [string] -or $row.sha256 -isnot [string] -or $row.sha256 -cnotmatch '^[A-Fa-f0-9]{64}$') {
                    throw 'Malformed snapshot path or SHA256.'
                }
                $path = [IO.Path]::GetFullPath($row.path)
                if ($index.ContainsKey($path)) { throw 'Duplicate snapshot path.' }
                $index.Add($path, $row.sha256)
            }
            ,$index
        }
    )
    $beforeIndex = $indices[0]; $afterIndex = $indices[1]
    $changed = @($beforeIndex.Keys | Where-Object {
        -not $afterIndex.ContainsKey($_) -or $beforeIndex[$_] -ine $afterIndex[$_]
    })
    $new = @($afterIndex.Keys | Where-Object { -not $beforeIndex.ContainsKey($_) })
    $unexpected = @($new | Where-Object { -not $AllowOwnedSaves -or -not (Test-VillageQuestOwnedSave -Path $_ -Plan $Plan) })
    [pscustomobject]@{ preserved = ($changed.Count -eq 0 -and $unexpected.Count -eq 0)
        changed_or_missing = $changed; unexpected_new = $unexpected; new_files = $new }
}

function Assert-VillageQuestExecution {
    param([Parameter(Mandatory)]$Plan, [string]$EditorHandoff)
    if ([string]::IsNullOrWhiteSpace($EditorHandoff)) { throw 'Execute requires the shared editor/build EditorHandoff.' }
    if (-not $IsWindows) { throw 'Execution requires Windows.' }
    if (-not (Test-Path -LiteralPath $Plan.project -PathType Leaf)) { throw 'Valid Rogue10m.uproject is required.' }
    try { $projectData = Get-Content -LiteralPath $Plan.project -Raw | ConvertFrom-Json -ErrorAction Stop }
    catch { throw 'Valid Rogue10m.uproject JSON is required.' }
    if ($null -eq $projectData.FileVersion -or -not @($projectData.Modules | Where-Object { $_.Name -ceq 'Rogue10m' -and $_.Type -ceq 'Runtime' }).Count) {
        throw 'Valid Rogue10m.uproject runtime module is required.'
    }
    if (-not (Test-Path -LiteralPath $Plan.fixture -PathType Leaf) -or
        -not ([IO.File]::ReadAllText($Plan.fixture)).Contains('Rogue10m.TestVillageQuests')) {
        throw 'Installed VillageQuest runtime fixture is required. This launcher never installs or builds it.'
    }
    if (-not (Test-Path -LiteralPath $Plan.module -PathType Leaf)) { throw 'Compiled project DLL is required.' }
    $newest = Get-ChildItem -LiteralPath (Join-Path $Plan.project_root 'Source') -Recurse -File |
        Where-Object { $_.Extension -in @('.cpp','.h','.cs') } | Sort-Object LastWriteTimeUtc -Descending | Select-Object -First 1
    if (-not $newest -or (Get-Item -LiteralPath $Plan.module).LastWriteTimeUtc -lt $newest.LastWriteTimeUtc) {
        throw 'Project DLL predates installed Source; shared build owner must build/restart first.'
    }
    if ([IO.Path]::GetFileName($Plan.editor) -ine 'UnrealEditor.exe' -or -not (Test-Path -LiteralPath $Plan.editor -PathType Leaf)) {
        throw 'Actual UnrealEditor.exe is required; packaged game and commandlet executables are rejected.'
    }
    $identity = Get-VillageQuestEditorIdentity $Plan.editor
    if ($identity.OriginalFilename -ine 'UnrealEditor.exe' -or $identity.ProductName -ine 'UnrealEditor') {
        throw 'Executable metadata does not identify the actual UnrealEditor.'
    }
    Assert-VillageQuestNoSharedUE
    if (Test-Path -LiteralPath $Plan.evidence) { throw 'Fresh nonexistent evidence Label required; existing evidence is preserved.' }
    $saves = @(Get-VillageQuestSaveSnapshot $Plan.save_root)
    if (@($saves | Where-Object { Test-VillageQuestOwnedSave $_.path $Plan }).Count) {
        throw 'Fresh exact QA save namespace required; existing matching saves are preserved.'
    }
}

function Test-VillageQuestRuntimeResult {
    param([Parameter(Mandatory)][string]$Path, [Parameter(Mandatory)][string]$Label)
    $reasons = [Collections.Generic.List[string]]::new()
    try {
        $data = Get-Content -LiteralPath $Path -Raw -Encoding utf8 -ErrorAction Stop | ConvertFrom-Json -NoEnumerate -ErrorAction Stop
        if ($null -eq $data -or $data -is [array] -or $data -isnot [pscustomobject]) { throw 'Expected a JSON object.' }
    } catch { return [pscustomobject]@{ passed = $false; reasons = @('unreadable_result: ' + $_.Exception.Message) } }
    foreach ($name in @('status','label','checks','failures','physical_input','full_run','facts_injected',
        'rendered','normal_portal_entry','optional_story_completed','fixture_positioning','reported_quests','milestones','captures','assertions')) {
        if (@($data.PSObject.Properties | Where-Object { $_.Name -ceq $name }).Count -ne 1) { $reasons.Add('missing exact field: ' + $name) }
    }
    if ($data.status -isnot [string] -or $data.label -isnot [string] -or
        $data.status -cne 'passed' -or $data.label -cne $Label) { $reasons.Add('status or label mismatch') }
    foreach ($name in @('checks','failures')) {
        $value = $data.$name
        $integer = $value -is [int] -or $value -is [long] -or $value -is [uint32] -or $value -is [uint64]
        if (-not $integer -or ($name -eq 'checks' -and ($value -le 0 -or $value -gt [int]::MaxValue)) -or
            ($name -eq 'failures' -and $value -ne 0)) { $reasons.Add('invalid integer: ' + $name) }
    }
    foreach ($name in @('physical_input','full_run','facts_injected','rendered','normal_portal_entry','optional_story_completed')) {
        $expected = $name -in @('rendered','normal_portal_entry','optional_story_completed')
        if ($data.$name -isnot [bool] -or $data.$name -ne $expected) { $reasons.Add('invalid boolean: ' + $name) }
    }
    if ($data.fixture_positioning -isnot [string] -or $data.fixture_positioning -cne 'teleport_in_candidate_only') {
        $reasons.Add('fixture positioning mismatch')
    }
    if (($data.reported_quests -isnot [int] -and $data.reported_quests -isnot [long]) -or $data.reported_quests -ne 5) {
        $reasons.Add('reported_quests must be integer5')
    }
    if ($data.milestones -isnot [pscustomobject] -or $data.milestones -is [array]) { $reasons.Add('milestones must be an object') }
    foreach ($name in @(Get-VillageQuestMilestoneNames)) {
        $property = @($data.milestones.PSObject.Properties | Where-Object { $_.Name -ceq $name })
        if ($property.Count -ne 1 -or $property[0].Value -isnot [bool] -or -not $property[0].Value) {
            $reasons.Add('missing/false milestone: ' + $name)
        }
    }
    $expectedCaptures = @(Get-VillageQuestCaptureNames)
    if ($data.captures -isnot [array] -or $data.captures.Count -ne $expectedCaptures.Count) {
        # The explicit set comparison below also rejects duplicate, extra and non-string entries.
        $reasons.Add('captures must contain exactly six names')
    }
    foreach ($capture in $expectedCaptures) {
        if (@($data.captures | Where-Object { $_ -is [string] -and $_ -ceq $capture }).Count -ne 1) {
            $reasons.Add('missing/duplicate capture: ' + $capture)
        }
    }
    if ($data.assertions -isnot [array] -or $data.assertions.Count -ne $data.checks) { $reasons.Add('assertions count must match checks') }
    foreach ($assertion in @($data.assertions)) {
        $property = @($assertion.PSObject.Properties | Where-Object { $_.Name -ceq 'passed' })
        if ($assertion -isnot [pscustomobject] -or $property.Count -ne 1 -or $property[0].Value -isnot [bool] -or -not $property[0].Value) {
            $reasons.Add('every assertion must have passed booleantrue')
        }
    }
    [pscustomobject]@{ passed = ($reasons.Count -eq 0); reasons = @($reasons.ToArray()) }
}

function Test-VillageQuestRuntimeLog {
    param([Parameter(Mandatory)][AllowEmptyString()][string]$Text)
    $markers = [regex]::Matches($Text, '(?<![A-Za-z0-9_])RESULT=VILLAGE_QUEST_RUNTIME_PASSED(?![A-Za-z0-9_])').Count
    $red = @($Text -split '\r?\n' | Where-Object { $_ -match '(?i)FAILED|\bError\s*:|\bFatal(?:\s+error)?\b|\bensure\b' })
    [pscustomobject]@{ passed = ($markers -eq 1 -and $red.Count -eq 0); marker_count = $markers; red_lines = $red }
}

function Test-VillageQuestRuntimeArtifacts {
    param([Parameter(Mandatory)][string]$RuntimeDirectory)
    $expectedNames = @(Get-VillageQuestCaptureNames)
    $events = Join-Path $RuntimeDirectory 'events.csv'
    $eventLines = if (Test-Path -LiteralPath $events -PathType Leaf) { @(Get-Content -LiteralPath $events | Where-Object { -not [string]::IsNullOrWhiteSpace($_) }) } else { @() }
    $pngs = @(Get-ChildItem -LiteralPath $RuntimeDirectory -Filter '*.png' -File -ErrorAction SilentlyContinue)
    $missing = @($expectedNames | Where-Object { -not (Test-Path -LiteralPath (Join-Path $RuntimeDirectory $_) -PathType Leaf) })
    $invalid = @($pngs | Where-Object {
        $bytes = [IO.File]::ReadAllBytes($_.FullName)
        if ($bytes.Length -lt 67) { return $true }
        [Convert]::ToHexString($bytes[0..7]) -cne '89504E470D0A1A0A' -or
            [Text.Encoding]::ASCII.GetString($bytes,12,4) -cne 'IHDR' -or
            [Convert]::ToHexString($bytes[($bytes.Length-12)..($bytes.Length-1)]) -cne '0000000049454E44AE426082'
    })
    [pscustomobject]@{ passed = ($eventLines.Count -gt 1 -and $missing.Count -eq 0 -and $invalid.Count -eq 0)
        event_lines = $eventLines.Count; png_count = $pngs.Count; missing_pngs = $missing; invalid_pngs = @($invalid.FullName) }
}

$plan = New-VillageQuestPlan -Label $Label -ProjectRoot $ProjectRoot -EditorExe $EditorExe -TimeoutSeconds $TimeoutSeconds
if (-not $Execute) { $plan | ConvertTo-Json -Depth 8; return }
Assert-VillageQuestExecution -Plan $plan -EditorHandoff $EditorHandoff
$beforeSaves = @(Get-VillageQuestSaveSnapshot $plan.save_root)
$beforeContent = @(Get-VillageQuestContentSnapshot $plan.project_root)
Assert-VillageQuestNoSharedUE
[IO.Directory]::CreateDirectory($plan.evidence) | Out-Null
$journalPath = Join-Path $plan.evidence 'launcher.json'
$journal = [ordered]@{ status = 'running'; label = $Label; editor_handoff = $EditorHandoff
    started_utc = [DateTime]::UtcNow.ToString('o'); plan = $plan; pid = $null; exit_code = $null
    timed_out = $false; result = $null; log = $null; artifacts = $null; preservation = $null
    content_snapshot_scope = 'Source cpp/h/cs, Config ini, project file, StartHub and candidate umap'
    module_sha256 = (Get-FileHash -LiteralPath $plan.module -Algorithm SHA256).Hash }
function Write-VillageQuestJournal { $journal | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath $journalPath -Encoding utf8NoBOM }
function Stop-VillageQuestOwnedProcess {
    if (-not $process -or $process.HasExited) { return }
    $current = Get-Process -Id $process.Id -ErrorAction SilentlyContinue
    if (-not $current -or $current.StartTime -ne $ownedStart -or $current.Path -ine $plan.editor) {
        throw 'Owned PID identity is uncertain; no unverified process was terminated.'
    }
    $process.Kill()
    if (-not $process.WaitForExit(10000)) { throw 'Owned UE did not exit during bounded cleanup.' }
}
Write-VillageQuestSnapshot -Path (Join-Path $plan.evidence 'saves-before.json') -Snapshot $beforeSaves
Write-VillageQuestSnapshot -Path (Join-Path $plan.evidence 'content-before.json') -Snapshot $beforeContent
$process = $null; $ownedStart = $null; $failure = $null; $cleanupFailure = $null
Write-VillageQuestJournal
try {
    $process = Start-Process -FilePath $plan.editor -ArgumentList $plan.arguments -WorkingDirectory $plan.project_root -WindowStyle Hidden -PassThru
    $ownedStart = $process.StartTime; $journal.pid = $process.Id; Write-VillageQuestJournal
    $clock = [Diagnostics.Stopwatch]::StartNew()
    while (-not $process.WaitForExit(1000)) {
        if ($clock.Elapsed.TotalSeconds -ge $TimeoutSeconds) { $journal.timed_out = $true; Stop-VillageQuestOwnedProcess; break }
    }
    $process.Refresh(); $journal.exit_code = $process.ExitCode
    $journal.result = Test-VillageQuestRuntimeResult -Path $plan.results -Label $Label
    $text = if (Test-Path -LiteralPath $plan.engine_log -PathType Leaf) { [IO.File]::ReadAllText($plan.engine_log) } else { '' }
    $journal.log = Test-VillageQuestRuntimeLog -Text $text
    $journal.artifacts = Test-VillageQuestRuntimeArtifacts -RuntimeDirectory $plan.runtime
    if ($journal.timed_out -or $journal.exit_code -ne 0 -or -not $journal.result.passed -or
        -not $journal.log.passed -or -not $journal.artifacts.passed) { throw 'Runtime result, rendered artifacts, process exit or engine log did not pass.' }
} catch { $failure = $_ } finally {
    try { Stop-VillageQuestOwnedProcess } catch { $cleanupFailure = $_ }
    if (-not $cleanupFailure) {
        try {
            $afterSaves = @(Get-VillageQuestSaveSnapshot $plan.save_root)
            $afterContent = @(Get-VillageQuestContentSnapshot $plan.project_root)
            Write-VillageQuestSnapshot -Path (Join-Path $plan.evidence 'saves-after.json') -Snapshot $afterSaves
            Write-VillageQuestSnapshot -Path (Join-Path $plan.evidence 'content-after.json') -Snapshot $afterContent
            $journal.preservation = [ordered]@{
                saves = Compare-VillageQuestSnapshots -Before $beforeSaves -After $afterSaves -Plan $plan -AllowOwnedSaves
                content = Compare-VillageQuestSnapshots -Before $beforeContent -After $afterContent -Plan $plan
            }
            if (-not $journal.preservation.saves.preserved -or -not $journal.preservation.content.preserved) {
                if (-not $failure) { $failure = 'Existing saves or source/config/map hashes changed, or unexpected files appeared.' }
            }
        } catch { if (-not $failure) { $failure = $_ }; $journal.preservation = @{ preserved = $false; error = $_.ToString() } }
    } else { $journal.preservation = @{ preserved = $null; reason = 'Owned PID cleanup uncertain; no post-run preservation certified.' } }
    $journal.status = if ($cleanupFailure) { 'cleanup_uncertain' } elseif ($failure) { 'failed' } else { 'passed' }
    $journal.failure = if ($failure) { $failure.ToString() } else { $null }
    $journal.cleanup_failure = if ($cleanupFailure) { $cleanupFailure.ToString() } else { $null }
    $journal.ended_utc = [DateTime]::UtcNow.ToString('o'); Write-VillageQuestJournal
}
if ($cleanupFailure) { throw $cleanupFailure }
if ($failure) { throw $failure }
Write-Output ('PASSED VillageQuest rendered runtime fixture; physical input and full run were not performed. Evidence=' + $plan.evidence)
