#requires -Version 7.0
[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$launcher = Join-Path (Split-Path -Parent $PSScriptRoot) 'VerifyVillageQuests.ps1'
$suiteRoot = Join-Path ([IO.Path]::GetTempPath()) ('Rogue10mVillageQuestLauncherTests_' + [guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($suiteRoot) | Out-Null
$script:checks = 0; $script:failures = 0; $script:launchAttempts = 0
function Check([bool]$Condition, [string]$Name) {
    ++$script:checks
    if (-not $Condition) { ++$script:failures; Write-Output ('FAIL ' + $Name) }
}
function Throws([scriptblock]$Work, [string]$Pattern, [string]$Name) {
    $caught = $null
    try { & $Work | Out-Null } catch { $caught = $_.Exception.Message }
    Check ($null -ne $caught -and $caught -match $Pattern -and $caught -notmatch 'SELFTEST_FORBIDS_PROCESS_START') $Name
}
function Write-TestFile([string]$Path, [string]$Text) {
    [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($Path)) | Out-Null
    [IO.File]::WriteAllText($Path, $Text)
}
function Start-Process { ++$script:launchAttempts; throw 'SELFTEST_FORBIDS_PROCESS_START' }

try {
    $parseTokens = $null; $parseErrors = $null
    $ast = [Management.Automation.Language.Parser]::ParseFile($launcher, [ref]$parseTokens, [ref]$parseErrors)
    Check ($parseErrors.Count -eq 0) 'launcher parses without errors'
    $processCalls = @($ast.FindAll({ param($node) $node -is [Management.Automation.Language.CommandAst] -and $node.GetCommandName() -eq 'Start-Process' }, $true))
    Check ($processCalls.Count -eq 1) 'exactly one owned process launch site'
    $testProject = Join-Path $suiteRoot 'Project With Spaces'
    $fakeEditor = Join-Path $suiteRoot 'Engine/Binaries/Win64/UnrealEditor.exe'
    # Dot-sourcing the default plan loads pure helpers without preflight, mutation or execution.
    $json = . $launcher -Label 'LauncherQA_1' -ProjectRoot $testProject -EditorExe $fakeEditor
    $plan = $json | ConvertFrom-Json
    Check ($plan.status -ceq 'plan_only_not_executed') 'default is plan only'
    Check (-not (Test-Path -LiteralPath $testProject)) 'default plan creates no project or evidence directories'
    Check ($script:launchAttempts -eq 0) 'default plan never launches a process'
    Check ($plan.label -ceq 'LauncherQA_1') 'plan retains exact label'
    Check ($plan.timeout_seconds -eq 240) 'default watchdog is bounded240 seconds'
    Check ($plan.profile_slot -ceq 'Rogue10m_Town_QA_VillageQuest_LauncherQA_1') 'exact Town namespace'
    Check ($plan.progression_prefix -ceq 'Rogue10m_Progression_QA_VillageQuest_LauncherQA_1') 'exact progression namespace'
    Check ($plan.fighter_slot -ceq 'Rogue10m_Fighter_QA_VillageQuest_LauncherQA_1') 'exact Fighter namespace'
    Check ($plan.arguments.Count -eq 24) 'fixed argument list has24 tokens'
    $expectedArgs = @(
        ('"' + (Join-Path $testProject 'Rogue10m.uproject') + '"'), '/Game/Rogue10m/Maps/StartHub', '-game', '-unattended',
        '-DisablePython', '-NoSplash', '-NoSound', '-NoLiveCoding', '-NoRemoteShaderCompile', '-culture=en',
        '-language=en', '-locale=en-US', '-windowed', '-ForceRes', '-ResX=1280', '-ResY=720', '-d3d11', '-RenderOffScreen',
        '-RogueVillageQuestLabel=LauncherQA_1', '-RogueProfileSaveSlot=Rogue10m_Town_QA_VillageQuest_LauncherQA_1',
        '-RogueProgressionSavePrefix=Rogue10m_Progression_QA_VillageQuest_LauncherQA_1',
        '-RogueSkillKeySaveSlot=Rogue10m_Fighter_QA_VillageQuest_LauncherQA_1',
        '-ExecCmds="t.MaxFPS 60,Rogue10m.TestVillageQuests"', ('-abslog="' + (Join-Path $plan.evidence 'engine.log') + '"')
    )
    for ($index = 0; $index -lt $expectedArgs.Count; ++$index) { Check ($plan.arguments[$index] -ceq $expectedArgs[$index]) ('exact argument' + $index) }
    Check ($plan.runtime -ceq (Join-Path $testProject 'Feature/doc/evidence/village-quests-runtime-20261111/LauncherQA_1/runtime')) 'exact runtime evidence directory'
    Check ($plan.fixture_positioning -ceq 'teleport_in_candidate_only' -and $plan.normal_portal_entry) 'explicit portal and fixture movement scope'
    Check ($plan.rendered -and -not $plan.physical_input -and -not $plan.full_run -and -not $plan.facts_injected) 'plan makes no physical/full-run/injected-fact claims'
    foreach ($badLabel in @('','../x','a-b','with space','x"','x;y',('x'*21),"a`nb")) {
        Throws { & $launcher -Label $badLabel -ProjectRoot $testProject -EditorExe $fakeEditor } '.' ('reject malformed label: ' + $badLabel)
    }
    foreach ($timeout in @(-1,0,89,601,2147483647)) {
        Throws { & $launcher -Label Valid -ProjectRoot $testProject -EditorExe $fakeEditor -TimeoutSeconds $timeout } '.' ('reject watchdog' + $timeout)
    }
    foreach ($timeout in @(90,600)) {
        $bounded = & $launcher -Label Valid -ProjectRoot $testProject -EditorExe $fakeEditor -TimeoutSeconds $timeout | ConvertFrom-Json
        Check ($bounded.timeout_seconds -eq $timeout) ('accept watchdog boundary' + $timeout)
    }
    Throws { & $launcher -Label Valid -Execute -ProjectRoot $testProject -EditorExe $fakeEditor } 'EditorHandoff' 'execution rejects missing ownership handoff before touching project'
    Throws { Assert-VillageQuestExecution $plan 'integrator_handoff' } 'uproject' 'execution rejects missing project'
    Write-TestFile $plan.project 'not JSON'
    Throws { Assert-VillageQuestExecution $plan 'integrator_handoff' } 'JSON' 'execution rejects malformed project'
    Write-TestFile $plan.project '{"FileVersion":3,"Modules":[{"Name":"Other","Type":"Runtime"}]}'
    Throws { Assert-VillageQuestExecution $plan 'integrator_handoff' } 'runtime module' 'execution rejects unrelated project module'
    Write-TestFile $plan.project '{"FileVersion":3,"Modules":[{"Name":"Rogue10m","Type":"Runtime"}]}'
    Throws { Assert-VillageQuestExecution $plan 'integrator_handoff' } 'fixture' 'execution rejects uninstalled runtime fixture'
    Write-TestFile $plan.fixture '// WrongFixture'
    Throws { Assert-VillageQuestExecution $plan 'integrator_handoff' } 'fixture' 'execution rejects wrong runtime console fixture'
    Write-TestFile $plan.fixture '// Rogue10m.TestVillageQuests'
    Throws { Assert-VillageQuestExecution $plan 'integrator_handoff' } 'DLL' 'execution rejects missing module DLL'
    Write-TestFile $plan.module 'fake DLL for preflight tests only'
    (Get-Item -LiteralPath $plan.module).LastWriteTimeUtc = [DateTime]::UtcNow.AddMinutes(-5)
    Throws { Assert-VillageQuestExecution $plan 'integrator_handoff' } 'predates' 'execution rejects DLL older than installed source'
    (Get-Item -LiteralPath $plan.module).LastWriteTimeUtc = [DateTime]::UtcNow.AddMinutes(5)
    Throws { Assert-VillageQuestExecution $plan 'integrator_handoff' } 'UnrealEditor.exe' 'execution rejects nonexistent editor'
    Write-TestFile $fakeEditor 'fake renamed executable'
    Throws { Assert-VillageQuestExecution $plan 'integrator_handoff' } 'metadata' 'execution rejects renamed fake editor metadata'
    $packagedPlan = New-VillageQuestPlan -Label Package -ProjectRoot $testProject -EditorExe (Join-Path $suiteRoot 'Rogue10m.exe')
    Throws { Assert-VillageQuestExecution $packagedPlan 'integrator_handoff' } 'UnrealEditor.exe' 'packaged executable is rejected'
    $cmdPlan = New-VillageQuestPlan -Label Cmd -ProjectRoot $testProject -EditorExe (Join-Path $suiteRoot 'UnrealEditor-Cmd.exe')
    Throws { Assert-VillageQuestExecution $cmdPlan 'integrator_handoff' } 'UnrealEditor.exe' 'commandlet executable is rejected for rendered fixture'
    function Get-VillageQuestEditorIdentity { [pscustomobject]@{ OriginalFilename = 'UnrealEditor.exe'; ProductName = 'UnrealEditor' } }
    $script:fakeSharedProcesses = @()
    function Get-VillageQuestActiveProcess { $script:fakeSharedProcesses }
    Assert-VillageQuestExecution $plan 'integrator_handoff'
    Check ($script:launchAttempts -eq 0) 'successful pure preflight never launches a process'
    foreach ($processName in @('UnrealEditor','UnrealEditor-Cmd','UnrealBuildTool','dotnet')) {
        $script:fakeSharedProcesses = @([pscustomobject]@{ ProcessName = $processName; Id = 1234 })
        Throws { Assert-VillageQuestExecution $plan 'integrator_handoff' } 'Shared UE/UBT/dotnet' ('busy process is rejected: ' + $processName)
    }
    $script:fakeSharedProcesses = @()
    [IO.Directory]::CreateDirectory($plan.evidence) | Out-Null
    Throws { Assert-VillageQuestExecution $plan 'integrator_handoff' } 'Fresh nonexistent evidence' 'existing evidence is never overwritten'
    Remove-Item -LiteralPath $plan.evidence
    $guid32 = '0123456789abcdef0123456789abcdef'
    foreach ($ownedName in @(($plan.profile_slot + '.sav'),($plan.fighter_slot + '.sav'),($plan.progression_prefix + '_' + $guid32 + '.sav'))) {
        $save = Join-Path $plan.save_root $ownedName
        Write-TestFile $save 'existing exact QA save'
        Check (Test-VillageQuestOwnedSave $save $plan) ('recognize exact owned save' + $ownedName)
        Throws { Assert-VillageQuestExecution $plan 'integrator_handoff' } 'Fresh exact QA save namespace' 'existing exact QA save namespace rejects execution'
        Remove-Item -LiteralPath $save
    }
    foreach ($badName in @('Rogue10m_Town.sav',($plan.profile_slot + '_extra.sav'),($plan.progression_prefix + '.sav'),
        ($plan.progression_prefix + '_abc.sav'),($plan.progression_prefix + '_' + ('g'*32) + '.sav'),
        ($plan.progression_prefix + '_' + ('a'*31) + '.sav'),($plan.progression_prefix + '_' + ('a'*33) + '.sav'))) {
        Check (-not (Test-VillageQuestOwnedSave (Join-Path $plan.save_root $badName) $plan)) ('reject save alias' + $badName)
    }
    Check (-not (Test-VillageQuestOwnedSave (Join-Path $plan.save_root ('nested/' + $plan.profile_slot + '.sav')) $plan)) 'owned basename in nested directory is rejected'
    $originalSave = Join-Path $plan.save_root 'OrdinaryPlayer.sav'; Write-TestFile $originalSave 'original player data'
    $before = @(Get-VillageQuestSaveSnapshot $plan.save_root)
    $snapshotPath = Join-Path $suiteRoot 'snapshot.json'
    Write-VillageQuestSnapshot -Path $snapshotPath -Snapshot @()
    Check (([IO.File]::ReadAllText($snapshotPath)).Trim() -ceq '[]') 'empty save snapshot writes a JSON array'
    Write-VillageQuestSnapshot -Path $snapshotPath -Snapshot $before
    $snapshotRoundtrip = Get-Content -LiteralPath $snapshotPath -Raw | ConvertFrom-Json -NoEnumerate
    Check ($snapshotRoundtrip -is [array] -and $snapshotRoundtrip.Count -eq 1) 'one-file snapshot retains array shape'
    $validNewSave = Join-Path $plan.save_root ($plan.progression_prefix + '_' + $guid32 + '.sav'); Write-TestFile $validNewSave 'new fixture profile progress'
    $after = @(Get-VillageQuestSaveSnapshot $plan.save_root)
    Check (Compare-VillageQuestSnapshots -Before $before -After $after -Plan $plan -AllowOwnedSaves).preserved 'unchanged original and only exact owned new save passes'
    Check (-not (Compare-VillageQuestSnapshots -Before $before -After $after -Plan $plan).preserved) 'same new file is not permitted for immutable content'
    Write-TestFile $originalSave 'changed player data'
    Check (-not (Compare-VillageQuestSnapshots -Before $before -After @(Get-VillageQuestSaveSnapshot $plan.save_root) -Plan $plan -AllowOwnedSaves).preserved) 'changed original save fails preservation'
    Remove-Item -LiteralPath $originalSave
    Check (-not (Compare-VillageQuestSnapshots -Before $before -After @(Get-VillageQuestSaveSnapshot $plan.save_root) -Plan $plan -AllowOwnedSaves).preserved) 'missing original save fails preservation'
    Write-TestFile $originalSave 'original player data'
    $unexpectedSave = Join-Path $plan.save_root 'unexpected.sav'; Write-TestFile $unexpectedSave 'unexpected data'
    Check (-not (Compare-VillageQuestSnapshots -Before $before -After @(Get-VillageQuestSaveSnapshot $plan.save_root) -Plan $plan -AllowOwnedSaves).preserved) 'unexpected new save fails preservation'
    $duplicate = @($before[0],$before[0])
    Throws { Compare-VillageQuestSnapshots -Before $duplicate -After $after -Plan $plan } 'Duplicate' 'duplicate snapshot paths rejected'
    Throws { Compare-VillageQuestSnapshots -Before @([pscustomobject]@{path=$originalSave;sha256='bad'}) -After @() -Plan $plan } 'Malformed' 'invalid SHA snapshot rejected'
    $config = Join-Path $testProject 'Config/DefaultGame.ini'; Write-TestFile $config '[Game]'
    $map = Join-Path $testProject 'Content/Rogue10m/Maps/StartHub.umap'; Write-TestFile $map 'test map bytes'
    $contentBefore = @(Get-VillageQuestContentSnapshot $testProject)
    Check (@($contentBefore | Where-Object { $_.path -eq $plan.fixture }).Count -eq 1) 'content snapshot includes installed Source'
    Check (@($contentBefore | Where-Object { $_.path -eq $config }).Count -eq 1) 'content snapshot includes Config'
    Check (@($contentBefore | Where-Object { $_.path -eq $map }).Count -eq 1) 'content snapshot includes map'
    Check (Compare-VillageQuestSnapshots -Before $contentBefore -After @(Get-VillageQuestContentSnapshot $testProject) -Plan $plan).preserved 'unchanged content passes'
    Write-TestFile $config '[Changed]'
    Check (-not (Compare-VillageQuestSnapshots -Before $contentBefore -After @(Get-VillageQuestContentSnapshot $testProject) -Plan $plan).preserved) 'changed config fails content preservation'

    $resultPath = Join-Path $suiteRoot 'results.json'
    $captureNames = @('01_v1_offer.png','02_v1_ready_after_revisit.png','03_v1_rewarded.png',
        '04_v4_preacceptance_ready.png','05_v3_actual_kill_ready.png','06_all_five_complete_input_restored.png')
    $milestones = [ordered]@{normal_portal_entry=$true;actor_layout=$true;f_and_slate=$true;interrupted_revisit=$true
        npc_recreated=$true;director_recreated_without_encounter_respawn=$true;preacceptance=$true;attributed_kill=$true
        all_five_claimed=$true;duplicate_claim_blocked=$true;input_restored=$true;third_person=$true}
    $validResult = [ordered]@{status='passed';label='LauncherQA_1';checks=3;failures=0;physical_input=$false;full_run=$false
        facts_injected=$false;rendered=$true;fixture_positioning='teleport_in_candidate_only';normal_portal_entry=$true
        reported_quests=5;optional_story_completed=$true;milestones=$milestones;captures=$captureNames
        assertions=@(@{name='one';passed=$true},@{name='two';passed=$true},@{name='three';passed=$true})}
    Write-TestFile $resultPath ($validResult | ConvertTo-Json)
    Check (Test-VillageQuestRuntimeResult $resultPath $plan.label).passed 'exact result schema passes'
    foreach ($name in $validResult.Keys) {
        $copy = [ordered]@{}; foreach ($key in $validResult.Keys) { if ($key -cne $name) { $copy[$key] = $validResult[$key] } }
        Write-TestFile $resultPath ($copy | ConvertTo-Json)
        Check (-not (Test-VillageQuestRuntimeResult $resultPath $plan.label).passed) ('missing required result field fails: ' + $name)
    }
    $badFields = @(@('status','PASSED'),@('label','Other'),@('checks',0),@('checks',1.5),@('checks','51'),@('checks',$true),
        @('failures',1),@('failures','0'),@('physical_input',$true),@('full_run',$true),@('facts_injected',$true),
        @('rendered',$false),@('normal_portal_entry',$false),@('optional_story_completed',$false),
        @('optional_story_completed','true'),@('fixture_positioning','walked'),@('rendered','true'),
        @('reported_quests',4),@('reported_quests','5'),@('milestones','all passed'),@('captures',@()),@('assertions',@()))
    foreach ($bad in $badFields) {
        $copy = [ordered]@{}; foreach ($key in $validResult.Keys) { $copy[$key] = $validResult[$key] }; $copy[$bad[0]] = $bad[1]
        Write-TestFile $resultPath ($copy | ConvertTo-Json)
        Check (-not (Test-VillageQuestRuntimeResult $resultPath $plan.label).passed) ('invalid result value rejected: ' + $bad[0] + '=' + $bad[1])
    }
    foreach ($name in $milestones.Keys) {
        foreach ($badValue in @($false,'true')) {
            $copy = [ordered]@{}; foreach ($key in $validResult.Keys) { $copy[$key] = $validResult[$key] }
            $copy.milestones = [ordered]@{}; foreach ($key in $milestones.Keys) { $copy.milestones[$key] = $milestones[$key] }
            $copy.milestones[$name] = $badValue
            Write-TestFile $resultPath ($copy | ConvertTo-Json -Depth 8)
            Check (-not (Test-VillageQuestRuntimeResult $resultPath $plan.label).passed) ('false or nonboolean milestone rejected: ' + $name)
        }
    }
    $captureCases = @(
        [pscustomobject]@{values=$captureNames[0..4]},
        [pscustomobject]@{values=@($captureNames[0],$captureNames[0],$captureNames[2],$captureNames[3],$captureNames[4],$captureNames[5])},
        [pscustomobject]@{values=@($captureNames + 'extra.png')}
    )
    foreach ($case in $captureCases) {
        $copy = [ordered]@{}; foreach ($key in $validResult.Keys) { $copy[$key] = $validResult[$key] }; $copy.captures = $case.values
        Write-TestFile $resultPath ($copy | ConvertTo-Json -Depth 8)
        Check (-not (Test-VillageQuestRuntimeResult $resultPath $plan.label).passed) 'missing/duplicate/extra capture set rejected'
    }
    $assertionCases = @(
        [pscustomobject]@{values=@(@{passed=$true},@{passed=$false},@{passed=$true})},
        [pscustomobject]@{values=@(@{passed=$true},@{passed='true'},@{passed=$true})},
        [pscustomobject]@{values=@(@{passed=$true},@{passed=$true})}
    )
    foreach ($case in $assertionCases) {
        $copy = [ordered]@{}; foreach ($key in $validResult.Keys) { $copy[$key] = $validResult[$key] }; $copy.assertions = $case.values
        Write-TestFile $resultPath ($copy | ConvertTo-Json -Depth 8)
        Check (-not (Test-VillageQuestRuntimeResult $resultPath $plan.label).passed) 'failed/nonboolean/truncated assertion list rejected'
    }
    foreach ($badJson in @('[]','null','1','not json')) {
        Write-TestFile $resultPath $badJson
        Check (-not (Test-VillageQuestRuntimeResult $resultPath $plan.label).passed) ('nonobject result rejected: ' + $badJson)
    }
    Check (Test-VillageQuestRuntimeLog 'LogRogue10m: Display: RESULT=VILLAGE_QUEST_RUNTIME_PASSED').passed 'exact one success marker passes'
    foreach ($text in @('', 'RESULT=VILLAGE_QUEST_RUNTIME_PASSED_EXTRA', "RESULT=VILLAGE_QUEST_RUNTIME_PASSED`nRESULT=VILLAGE_QUEST_RUNTIME_PASSED",
        "RESULT=VILLAGE_QUEST_RUNTIME_PASSED`nRESULT=VILLAGE_QUEST_RUNTIME_FAILED", "RESULT=VILLAGE_QUEST_RUNTIME_PASSED`nLogTemp: Error: bad",
        "RESULT=VILLAGE_QUEST_RUNTIME_PASSED`nFatal error: bad", "RESULT=VILLAGE_QUEST_RUNTIME_PASSED`nEnsure condition failed")) {
        Check (-not (Test-VillageQuestRuntimeLog $text).passed) 'missing/duplicate/tainted success log rejected'
    }
    $runtime = Join-Path $suiteRoot 'runtime'; [IO.Directory]::CreateDirectory($runtime) | Out-Null
    Check (-not (Test-VillageQuestRuntimeArtifacts $runtime).passed) 'absent events/screenshots rejected'
    Write-TestFile (Join-Path $runtime 'events.csv') "stage,value`nentered_candidate,1`n"
    foreach ($name in $captureNames) {
        [IO.File]::WriteAllBytes((Join-Path $runtime $name),[Convert]::FromBase64String('iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mP8/x8AAwMCAO+nm3sAAAAASUVORK5CYII='))
    }
    Check (Test-VillageQuestRuntimeArtifacts $runtime).passed 'all six completed named PNGs and populated events pass artifact check'
    $pngPath = Join-Path $runtime $captureNames[0]
    Remove-Item -LiteralPath $pngPath
    Check (-not (Test-VillageQuestRuntimeArtifacts $runtime).passed) 'one missing required capture fails artifact check'
    [IO.File]::WriteAllBytes($pngPath,[byte[]]@())
    Check (-not (Test-VillageQuestRuntimeArtifacts $runtime).passed) 'empty PNG rejected without certifying render'
    [IO.File]::WriteAllBytes($pngPath,[byte[]](1..100))
    Check (-not (Test-VillageQuestRuntimeArtifacts $runtime).passed) 'non-PNG byte stream rejected'
    Check ($script:launchAttempts -eq 0) 'all static and guard tests completed without launching any process'
} finally {
    $resolved = [IO.Path]::GetFullPath($suiteRoot)
    $tempBoundary = [IO.Path]::TrimEndingDirectorySeparator([IO.Path]::GetFullPath([IO.Path]::GetTempPath())) + [IO.Path]::DirectorySeparatorChar
    if ($resolved.StartsWith($tempBoundary,[StringComparison]::OrdinalIgnoreCase) -and
        [IO.Path]::GetFileName($resolved) -cmatch '^Rogue10mVillageQuestLauncherTests_[a-f0-9]{32}$') {
        Remove-Item -LiteralPath $resolved -Recurse -Force
    } else { throw 'Selftest cleanup target escaped its exact temporary namespace.' }
}
Write-Output ('RESULT=VILLAGE_QUEST_LAUNCHER_SELFTEST_' + $(if ($script:failures) {'FAILED'} else {'PASSED'}) +
    ' checks=' + $script:checks + ' failures=' + $script:failures + ' process_launches=' + $script:launchAttempts)
if ($script:failures) { throw 'VillageQuest launcher selftests failed.' }
