param(
    [Parameter(Mandatory)][string]$ProjectRoot,
    [Parameter(Mandatory)][string]$PriorPacketRoot,
    [Parameter(Mandatory)][string]$OutputPath,
    [string]$HistoricalAutomationLog = '',
    [string]$HistoricalRunnerRecord = ''
)
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'QA\MonsterAITargetReadiness.psm1') -Force -DisableNameChecking
$controllerPath = 'Source/Rogue10m/Enemy/AI/Rogue10mMonsterAIController.cpp'
$testsPath = 'Source/Rogue10m/Tests/Rogue10mMonsterAITargetLifecycleTests.cpp'
$headerPath = 'Source/Rogue10m/Character/Rogue10mCharacter.h'
$paths = @($controllerPath, $testsPath, $headerPath)
if (Test-Path -LiteralPath $OutputPath) { throw 'A fresh output file is required.' }
function Read([string]$Path) { return [IO.File]::ReadAllText($Path) }
function ReadCurrent([string]$RelativePath) {
    $path = Join-Path $ProjectRoot $RelativePath
    if (Test-Path -LiteralPath $path -PathType Leaf) { return Read $path }
    return ''
}
function Fingerprints {
    @($paths | ForEach-Object {
        $path = Join-Path $ProjectRoot $_
        $exists = Test-Path -LiteralPath $path -PathType Leaf
        [pscustomobject]@{ path = $_; exists = $exists; sha256 = $(if ($exists) { (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash } else { $null }) }
    })
}
$before = @(Fingerprints)
$controller = ReadCurrent $controllerPath
$tests = ReadCurrent $testsPath
$characterHeader = ReadCurrent $headerPath
$source = Get-MonsterAITargetSourceState -CurrentController $controller -CurrentTests $tests `
    -BaselineController (Read (Join-Path $PriorPacketRoot ('baseline/' + $controllerPath))) `
    -PreparedController (Read (Join-Path $PriorPacketRoot ('candidate/' + $controllerPath))) `
    -PreparedTests (Read (Join-Path $PriorPacketRoot ('candidate/' + $testsPath)))
$fixture = [ordered]@{
    native_character_abstract = [regex]::IsMatch($characterHeader, '(?s)UCLASS\([^)]*\babstract\b[^)]*\)\s*class\s+ROGUE10M_API\s+ARogue10mCharacter')
    concrete_humanmale_class = $tests.Contains('/Game/Character/Customization/Characters/BP_Rogue10m_HumanMaleCharacter.BP_Rogue10m_HumanMaleCharacter_C')
    nonabstract_class_preflight = $tests.Contains('TargetClass->HasAnyClassFlags(CLASS_Abstract)')
    target_spawn_uses_loaded_class = [regex]::IsMatch($tests, 'SpawnActorDeferred<ARogue10mCharacter>\(\s*TargetClass\s*,')
    playerstate_attached = [regex]::IsMatch($tests, 'Target->SetPlayerState\(State\)')
    attribute_set_registered = [regex]::IsMatch($tests, 'ASC->AddAttributeSetSubobject\(Attributes\)')
    owner_avatar_initialized = [regex]::IsMatch($tests, 'ASC->InitAbilityActorInfo\(State,\s*Target\)')
    concrete_blueprint_file_exists = Test-Path -LiteralPath (Join-Path $ProjectRoot 'Content/Character/Customization/Characters/BP_Rogue10m_HumanMaleCharacter.uasset')
    scope = 'static source contracts and file existence, not UObject/asset load validation'
}
$fixtureReady = $fixture.concrete_humanmale_class -and $fixture.nonabstract_class_preflight `
    -and $fixture.target_spawn_uses_loaded_class -and $fixture.playerstate_attached `
    -and $fixture.attribute_set_registered -and $fixture.owner_avatar_initialized -and $fixture.concrete_blueprint_file_exists
$historical = $null
if ($HistoricalAutomationLog) {
    $historical = [ordered]@{
        log_path = $HistoricalAutomationLog
        log_sha256 = (Get-FileHash -LiteralPath $HistoricalAutomationLog -Algorithm SHA256).Hash
        # The old receipt does not persist the OS process exit code. Do not invent it.
        completion = Get-MonsterAITargetAutomationResult -LogText (Read $HistoricalAutomationLog)
        current_source_binding_verified = $false
        engine_executed_here = $false
    }
    if ($HistoricalRunnerRecord) {
        $record = Read $HistoricalRunnerRecord | ConvertFrom-Json
        $historical.runner_record_path = $HistoricalRunnerRecord
        $historical.runner_record_failure = $record.failure
        $historical.recorded_module_sha256 = $record.module_sha256
    }
}
$after = @(Fingerprints)
$same = @($before | Where-Object {
    $item = $_
    @($after | Where-Object { $_.path -ceq $item.path -and $_.exists -eq $item.exists -and $_.sha256 -ceq $item.sha256 }).Count -ne 1
}).Count -eq 0
$sourceReady = $same -and $source.ControllerFixPresent -and $fixtureReady `
    -and $source.TestState -in @('prepared', 'evolved')
$result = [ordered]@{
    status = $(if ($sourceReady) { 'source_ready' } else { 'source_review_required' })
    at_utc = [DateTime]::UtcNow.ToString('o')
    source = $source
    fixture_contracts = $fixture
    fingerprints = $after
    source_snapshot_stable = $same
    historical_evidence = $historical
    shared_source_or_assets_modified = $false
    engine_or_ubt_started = $false
    runtime_test_passed_here = $false
    legacy_patch_action = 'Do not overwrite current code with the old patch; reconcile each file first.'
    remaining = @('Coordinate any new build/Unreal run with its owner.', 'Historical results are not proof of the current module/source combination.')
}
New-Item -ItemType Directory -Path (Split-Path -Parent $OutputPath) -Force | Out-Null
$result | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath $OutputPath -Encoding UTF8
if (-not $sourceReady) { throw ('Source readiness needs review: ' + $OutputPath) }
if ($historical -and -not $historical.completion.LogPassed) { throw ('Historical automation log failed strict validation: ' + $OutputPath) }
Write-Output ('SOURCE_READINESS_PASSED legacy_patch=' + $source.LegacyPatchDisposition + ' engine_executed=false')
Write-Output ('Evidence: ' + $OutputPath)
