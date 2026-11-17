#requires -Version 7.0
[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$taskRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
. (Join-Path $taskRoot 'Scripts/FiveVillageTravelContract.ps1')
$taskTemp = Join-Path $env:TEMP ('Rogue10mFiveVillageContract_' + [guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($taskTemp) | Out-Null
$taskResults = [Collections.Generic.List[object]]::new()
$taskInput = Join-Path $taskTemp 'results.json'

function Check-Contract([string]$Name, [bool]$Condition) {
    $taskResults.Add([pscustomobject]@{name=$Name;passed=$Condition})
    if (-not $Condition) { Write-Host ('FAIL ' + $Name) }
}

function Check-Throws([string]$Name, [scriptblock]$Work) {
    $caught = $false
    try { $null = & $Work } catch { $caught = $true }
    Check-Contract $Name $caught
}


function New-ValidMovement {
    $result=@{}
    foreach($phase in @('hub','candidate')) {
        $result[$phase]=@{authored_data_walk=600;base_walk=600;base_sprint=900;gear_bonus=20;gear_multiplier_bonus=0;
            combined_bonus=20;combined_multiplier_bonus=0;derived_walk=620;derived_sprint=920;actual_max_walk=620;identity_multiplier=1;
            identity_active=$false;sprinting=$false;third_person=$true;
            pawn_class='/Game/Character/Customization/Characters/BP_Rogue10m_HumanMaleCharacter.BP_Rogue10m_HumanMaleCharacter_C';
            character_data='/Game/DataAsset/Character/DA_Character_Default.DA_Character_Default';
            equipped_items=@(@('DA_Item_IronHelmet',0),@('DA_Item_LeatherArmor',0),@('DA_Item_LeatherBoots',20),@('DA_Item_NoviceRing',0)) | ForEach-Object {
                @{asset=('/Game/DataAsset/Item/Starter/'+$_[0]+'.'+$_[0]);move_speed_bonus=$_[1];move_speed_multiplier_bonus=0}
            }}
    }
    return $result
}

function New-ValidTravelResult([string]$Scenario = 'Direct') {
    $profile = [guid]::NewGuid().ToString('N')
    $entry = [guid]::NewGuid().ToString('N')
    return @{
        schema_version=1;label='UnitA';scenario=$Scenario;status='passed';checks=30;failures=0
        milestones=@{normal_entry=$true;timer_running=$true;third_person=$true;route_observations=$true;
                     settled=$true;saved=$true;returned_hub=$true;profile_preserved=$true}
        actual_walking=$true;physical_input=$false;full_run=$false;rendered=$true;outcome='Abandoned'
        movement_method='AddMovementInput native CharacterMovement'
        ids=@{profile_id=$profile;entry_run_id=$entry;return_profile_id=$profile;
              return_run_id=[guid]::NewGuid().ToString('N');settled_run_id=$entry}
        timing=@{run_duration_seconds=1200;timer_advanced_seconds=2;boss_crossing_active_seconds=52.0;maximum_field_speed_cm_per_second=620;field_walked_distance_cm=30000}
        movement_diagnostics=(New-ValidMovement)
        visits=if($Scenario -eq 'Direct'){@{V1=0;V2=0;V3=0;V4=0;V5=0}}else{@{V1=2;V2=0;V3=1;V4=2;V5=0}}
        currency=@{baseline_restored=$true;lobby_award=0}
    }
}

function Write-Input($Data) {
    ConvertTo-Json -InputObject $Data -Depth 12 | Set-Content -LiteralPath $taskInput -Encoding utf8NoBOM
}

function Check-RejectedResult([string]$Name, [scriptblock]$Mutate, [string]$Scenario = 'Direct') {
    $data = New-ValidTravelResult $Scenario
    $null = & $Mutate $data
    Write-Input $data
    $result = Test-FiveVillageTravelResult -Path $taskInput -Label UnitA -Scenario $Scenario -ExpectedRendered $true
    Check-Contract $Name (-not $result.passed -and $result.reasons.Count -gt 0)
}

$taskFatal = $null
try {
    $direct = Get-FiveVillageTravelContract -ProjectRoot $taskTemp -Label UnitA -Scenario Direct
    $revisit = Get-FiveVillageTravelContract -ProjectRoot $taskTemp -Label UnitA -Scenario Revisit
    Check-Contract 'exact bounded save namespaces' ($direct.ProfileSlot -ceq 'Rogue10m_Town_QA_FiveVillage_UnitA_Direct' -and
        $direct.ProgressionPrefix -ceq 'Rogue10m_Progression_QA_FiveVillage_UnitA_Direct' -and
        $direct.SkillSlot -ceq 'Rogue10m_Fighter_QA_FiveVillage_UnitA_Direct')
    Check-Contract 'contract planning does not create runtime directories' (-not (Test-Path -LiteralPath $direct.CaseDirectory))
    Check-Contract 'canonical scenario spelling' ((Get-FiveVillageTravelContract -ProjectRoot $taskTemp -Label UnitA -Scenario direct).Scenario -ceq 'Direct')
    Check-Throws 'label path traversal rejected' { Get-FiveVillageTravelContract -ProjectRoot $taskTemp -Label '../UnitA' -Scenario Direct }
    Check-Throws 'unicode label rejected' { Get-FiveVillageTravelContract -ProjectRoot $taskTemp -Label '한글' -Scenario Direct }
    Check-Throws 'overlong label rejected' { Get-FiveVillageTravelContract -ProjectRoot $taskTemp -Label ('A'*21) -Scenario Direct }

    foreach ($scenario in @('Direct','Revisit')) {
        Write-Input (New-ValidTravelResult $scenario)
        $result = Test-FiveVillageTravelResult -Path $taskInput -Label UnitA -Scenario $scenario -ExpectedRendered $true
        Check-Contract ('complete valid ' + $scenario + ' result accepted') ($result.passed -and $result.reasons.Count -eq 0)
    }
    $headless = New-ValidTravelResult
    $headless.rendered = $false
    Write-Input $headless
    Check-Contract 'headless physics result uses explicit render expectation' ((Test-FiveVillageTravelResult -Path $taskInput -Label UnitA -Scenario Direct -ExpectedRendered $false).passed)
    Check-Contract 'render mode mismatch rejected' (-not (Test-FiveVillageTravelResult -Path $taskInput -Label UnitA -Scenario Direct -ExpectedRendered $true).passed)
    Check-RejectedResult 'missing return milestone rejected' {param($d) $d.milestones.Remove('returned_hub')}
    Check-RejectedResult 'string milestone cannot certify execution' {param($d) $d.milestones.saved='true'}
    Check-RejectedResult 'numeric milestone cannot certify execution' {param($d) $d.milestones.normal_entry=1}
    Check-RejectedResult 'singleton boolean array cannot certify milestone' {param($d) $d.milestones.normal_entry=@($true)}
    Check-RejectedResult 'singleton boolean array cannot certify walking' {param($d) $d.actual_walking=@($true)}
    Check-RejectedResult 'singleton count array rejected' {param($d) $d.checks=@(30)}
    Check-RejectedResult 'singleton status array rejected' {param($d) $d.status=@('passed')}
    Check-RejectedResult 'singleton label array rejected' {param($d) $d.label=@('UnitA')}
    Check-RejectedResult 'singleton outcome array rejected' {param($d) $d.outcome=@('Abandoned')}
    Check-RejectedResult 'singleton failure-count array rejected' {param($d) $d.failures=@(0)}
    Check-RejectedResult 'singleton milestone-container array rejected' {param($d) $d.milestones=@($d.milestones)}
    Check-RejectedResult 'singleton ID-container array rejected' {param($d) $d.ids=@($d.ids)}
    Check-RejectedResult 'singleton timing-container array rejected' {param($d) $d.timing=@($d.timing)}
    Check-RejectedResult 'singleton visit-container array rejected' {param($d) $d.visits=@($d.visits)}
    Check-RejectedResult 'singleton currency-container array rejected' {param($d) $d.currency=@($d.currency)}
    Check-RejectedResult 'partial runtime status rejected' {param($d) $d.status='started'}
    Check-RejectedResult 'wrong scenario rejected' {param($d) $d.scenario='Revisit'}
    Check-RejectedResult 'stale label rejected' {param($d) $d.label='Other'}
    Check-RejectedResult 'runtime failures override success status' {param($d) $d.failures=1}
    Check-RejectedResult 'string zero failure count rejected' {param($d) $d.failures='0'}
    Check-RejectedResult 'zero checks rejected' {param($d) $d.checks=0}
    Check-RejectedResult 'string check count rejected' {param($d) $d.checks='30'}
    Check-RejectedResult 'fractional check count rejected' {param($d) $d.checks=30.5}
    Check-RejectedResult 'schema string rejected' {param($d) $d.schema_version='1'}
    Check-RejectedResult 'claimed physical input rejected' {param($d) $d.physical_input=$true}
    Check-RejectedResult 'claimed full run rejected' {param($d) $d.full_run=$true}
    Check-RejectedResult 'teleport result rejected' {param($d) $d.movement_method='teleport'}
    Check-RejectedResult 'nonwalking result rejected' {param($d) $d.actual_walking=$false}
    Check-RejectedResult 'victory cannot substitute for abandon settlement' {param($d) $d.outcome='Victory'}
    Check-RejectedResult 'empty profile GUID rejected' {param($d) $d.ids.profile_id=[guid]::Empty.ToString()}
    Check-RejectedResult 'malformed run GUID rejected' {param($d) $d.ids.entry_run_id='bad'}
    Check-RejectedResult 'different returned profile rejected' {param($d) $d.ids.return_profile_id=[guid]::NewGuid().ToString()}
    Check-RejectedResult 'wrong run settlement receipt rejected' {param($d) $d.ids.settled_run_id=[guid]::NewGuid().ToString()}
    Check-RejectedResult 'same run on returned waiting hub rejected' {param($d) $d.ids.return_run_id=$d.ids.entry_run_id}
    Check-RejectedResult 'test duration override rejected' {param($d) $d.timing.run_duration_seconds=30}
    Check-RejectedResult 'string duration rejected' {param($d) $d.timing.run_duration_seconds='1200'}
    Check-RejectedResult 'stationary timer rejected' {param($d) $d.timing.timer_advanced_seconds=0}
    Check-RejectedResult 'negative crossing time rejected' {param($d) $d.timing.boss_crossing_active_seconds=-1}
    Check-RejectedResult 'direct active budget over60 rejected' {param($d) $d.timing.boss_crossing_active_seconds=60.01}
    Check-RejectedResult 'direct path village visit rejected' {param($d) $d.visits.V1=1}
    Check-RejectedResult 'missing village counter rejected' {param($d) $d.visits.Remove('V5')}
    Check-RejectedResult 'negative visit count rejected' {param($d) $d.visits.V1=-1}
    Check-RejectedResult 'incomplete revisit rejected' {param($d) $d.visits.V1=1} Revisit
    Check-RejectedResult 'second target revisit missing rejected' {param($d) $d.visits.V4=1} Revisit
    Check-RejectedResult 'unexpected skipped village entered rejected' {param($d) $d.visits.V2=1} Revisit
    Check-RejectedResult 'baseline restoration missing rejected' {param($d) $d.currency.baseline_restored=$false}
    Check-RejectedResult 'string baseline flag rejected' {param($d) $d.currency.baseline_restored='true'}
    Check-RejectedResult 'abandon reward rejected' {param($d) $d.currency.lobby_award=1}


    Check-RejectedResult 'missing movement diagnostics rejected' {param($d) $d.Remove('movement_diagnostics')}
    foreach($phase in @('hub','candidate')) {
        foreach($key in @('authored_data_walk','base_walk','gear_bonus','gear_multiplier_bonus','combined_bonus','combined_multiplier_bonus','derived_walk','actual_max_walk','identity_multiplier')) {
            Check-RejectedResult ("tampered $phase $key rejected") {param($d) $d.movement_diagnostics[$phase][$key]+=1}
            Check-RejectedResult ("missing $phase $key rejected") {param($d) $d.movement_diagnostics[$phase].Remove($key)}
        }
    }
    Check-RejectedResult 'bogus620 without justified boots rejected' {param($d) $d.movement_diagnostics.candidate.equipped_items[2].move_speed_bonus=0}
    Check-RejectedResult 'same arbitrary speed both maps rejected' {param($d) foreach($p in @('hub','candidate')){$d.movement_diagnostics[$p].actual_max_walk=700;$d.movement_diagnostics[$p].derived_walk=700}}
    Check-RejectedResult 'missing candidate rejected' {param($d) $d.movement_diagnostics.Remove('candidate')}
    Check-RejectedResult 'sprint rejected' {param($d) $d.movement_diagnostics.candidate.sprinting=$true}
    Check-RejectedResult 'identity active rejected' {param($d) $d.movement_diagnostics.hub.identity_active=$true}
    Check-RejectedResult 'string speed rejected' {param($d) $d.movement_diagnostics.hub.actual_max_walk='620'}
    Check-RejectedResult 'array movement rejected' {param($d) $d.movement_diagnostics=@($d.movement_diagnostics)}
    Check-RejectedResult 'duplicate boots rejected' {param($d) $d.movement_diagnostics.hub.equipped_items[0]=$d.movement_diagnostics.hub.equipped_items[2]}
    Check-RejectedResult 'velocity above retained650 rejected' {param($d) $d.timing.maximum_field_speed_cm_per_second=650.01}
    Check-RejectedResult 'missing measured velocity rejected' {param($d) $d.timing.Remove('maximum_field_speed_cm_per_second')}
    Check-RejectedResult 'insufficient direct distance rejected' {param($d) $d.timing.field_walked_distance_cm=29499}

    Check-Contract 'missing results file fails with explanation' (-not (Test-FiveVillageTravelResult -Path (Join-Path $taskTemp 'missing.json') -Label UnitA -Scenario Direct -ExpectedRendered $true).passed)
    [IO.File]::WriteAllText($taskInput, '{bad-json')
    Check-Contract 'malformed JSON fails with explanation' (-not (Test-FiveVillageTravelResult -Path $taskInput -Label UnitA -Scenario Direct -ExpectedRendered $true).passed)
    Write-Input @()
    Check-Contract 'array cannot substitute for JSON object' (-not (Test-FiveVillageTravelResult -Path $taskInput -Label UnitA -Scenario Direct -ExpectedRendered $true).passed)
    Write-Input @(New-ValidTravelResult)
    Check-Contract 'singleton object array cannot substitute for JSON object' (-not (Test-FiveVillageTravelResult -Path $taskInput -Label UnitA -Scenario Direct -ExpectedRendered $true).passed)

    $saveRoot = Join-Path $taskTemp 'Saved/SaveGames'
    [IO.Directory]::CreateDirectory((Join-Path $saveRoot 'nested')) | Out-Null
    $hidden = Join-Path $saveRoot '.production-hidden.sav'
    [IO.File]::WriteAllText($hidden, 'hidden-original')
    [IO.File]::SetAttributes($hidden, [IO.FileAttributes]::Hidden)
    [IO.File]::WriteAllText((Join-Path $saveRoot 'production.sav'), 'visible-original')
    [IO.File]::WriteAllText((Join-Path $saveRoot 'profile.json'), 'metadata-original')
    [IO.File]::WriteAllText((Join-Path $saveRoot 'nested/other.sav'), 'nested-original')
    $before = @(Get-FiveVillageSaveSnapshot -SaveRoot $saveRoot)
    Check-Contract 'snapshot includes hidden nested and non-sav files' ($before.Count -eq 4 -and @($before | Where-Object path -eq $hidden).Count -eq 1)
    $same = Compare-FiveVillageSaveSnapshots -Before $before -After $before -SaveRoot $saveRoot -Contracts @($direct,$revisit)
    Check-Contract 'unchanged original files preserved' ($same.preserved -and $same.changed.Count -eq 0 -and $same.missing.Count -eq 0)
    foreach ($name in @(($direct.ProfileSlot+'.sav'),($direct.SkillSlot+'.sav'),($direct.ProgressionPrefix+'_ABC012.sav'))) {
        [IO.File]::WriteAllText((Join-Path $saveRoot $name), 'new-owned-qa')
    }
    $after = @(Get-FiveVillageSaveSnapshot -SaveRoot $saveRoot)
    $owned = Compare-FiveVillageSaveSnapshots -Before $before -After $after -SaveRoot $saveRoot -Contracts @($direct)
    Check-Contract 'only exact owned new QA saves permitted' ($owned.preserved -and $owned.owned_new.Count -eq 3 -and $owned.unexpected_new.Count -eq 0)
    # OPEN_EXISTING preserves the hidden attribute; CREATE_ALWAYS rejects hidden
    # files on Windows even when the caller owns this temporary fixture.
    $hiddenBytes = [Text.Encoding]::UTF8.GetBytes('changed-hidden-original')
    $hiddenStream = [IO.File]::Open($hidden, [IO.FileMode]::Open, [IO.FileAccess]::Write, [IO.FileShare]::Read)
    try { $hiddenStream.SetLength(0); $hiddenStream.Write($hiddenBytes, 0, $hiddenBytes.Length) }
    finally { $hiddenStream.Dispose() }
    $changed = Compare-FiveVillageSaveSnapshots -Before $before -After @(Get-FiveVillageSaveSnapshot $saveRoot) -SaveRoot $saveRoot -Contracts @($direct)
    Check-Contract 'hidden original hash change detected' (-not $changed.preserved -and $changed.changed.Count -eq 1)
    $missing = Compare-FiveVillageSaveSnapshots -Before $before -After @($before | Where-Object path -ne $hidden) -SaveRoot $saveRoot -Contracts @($direct)
    Check-Contract 'missing original detected' (-not $missing.preserved -and $missing.missing.Count -eq 1)
    $foreign = [pscustomobject]@{path=(Join-Path $saveRoot 'unrelated.sav');sha256=('A'*64)}
    $unexpected = Compare-FiveVillageSaveSnapshots -Before $before -After @($before + $foreign) -SaveRoot $saveRoot -Contracts @($direct)
    Check-Contract 'unexpected new save detected' (-not $unexpected.preserved -and $unexpected.unexpected_new.Count -eq 1)
    $caseOnly = @($before | ForEach-Object {[pscustomobject]@{path=$_.path.ToUpperInvariant();sha256=$_.sha256.ToLowerInvariant()}})
    Check-Contract 'snapshot comparison is case-insensitive for Windows paths and hashes' ((Compare-FiveVillageSaveSnapshots -Before $before -After $caseOnly -SaveRoot $saveRoot -Contracts @($direct)).preserved)
    Check-Throws 'duplicate before paths rejected' {Compare-FiveVillageSaveSnapshots -Before @($before+$before[0]) -After $before -SaveRoot $saveRoot -Contracts @($direct)}
    Check-Throws 'duplicate after paths differing only by case rejected' {Compare-FiveVillageSaveSnapshots -Before $before -After @($before+$caseOnly[0]) -SaveRoot $saveRoot -Contracts @($direct)}
    Check-Throws 'invalid snapshot hash rejected' {Compare-FiveVillageSaveSnapshots -Before @([pscustomobject]@{path=$hidden;sha256='bad'}) -After $before -SaveRoot $saveRoot -Contracts @($direct)}
    Check-Throws 'snapshot path outside save root rejected' {Compare-FiveVillageSaveSnapshots -Before @([pscustomobject]@{path=(Join-Path $taskTemp 'outside.sav');sha256=('B'*64)}) -After $before -SaveRoot $saveRoot -Contracts @($direct)}
    Check-Contract 'case-insensitive exact slot is owned' (Test-FiveVillageOwnedSave -Path (Join-Path $saveRoot ($direct.ProfileSlot.ToUpperInvariant()+'.SAV')) -SaveRoot $saveRoot -Contracts @($direct))
    foreach ($pair in @(
        @('nested same slot is not owned',(Join-Path $saveRoot ('nested/'+$direct.ProfileSlot+'.sav'))),
        @('slot backup is not owned',(Join-Path $saveRoot ($direct.ProfileSlot+'_backup.sav'))),
        @('progression prefix extension is not owned',(Join-Path $saveRoot ($direct.ProgressionPrefix+'Evil_ABC.sav'))),
        @('empty progression suffix is not owned',(Join-Path $saveRoot ($direct.ProgressionPrefix+'_.sav'))),
        @('non-ASCII progression suffix is not owned',(Join-Path $saveRoot ($direct.ProgressionPrefix+'_한글.sav'))),
        @('sav backup extension is not owned',(Join-Path $saveRoot ($direct.SkillSlot+'.sav.bak'))),
        @('different scenario is not owned',(Join-Path $saveRoot ($revisit.ProfileSlot+'.sav')))
    )) { Check-Contract $pair[0] (-not (Test-FiveVillageOwnedSave -Path $pair[1] -SaveRoot $saveRoot -Contracts @($direct))) }
    $unsafe = $direct.PSObject.Copy()
    $unsafe.ProfileSlot = 'ProductionSlot'
    Check-Throws 'forged ownership contract rejected' {Test-FiveVillageOwnedSave -Path $hidden -SaveRoot $saveRoot -Contracts @($unsafe)}
    $unsafe = $direct.PSObject.Copy()
    $unsafe.ProfileSlot = @($direct.ProfileSlot)
    Check-Throws 'array-wrapped ownership slot rejected' {Test-FiveVillageOwnedSave -Path $hidden -SaveRoot $saveRoot -Contracts @($unsafe)}
} catch {
    $taskFatal = $_.ToString()
    Check-Contract ('unexpected test exception: ' + $taskFatal) $false
} finally {
    $failed = @($taskResults | Where-Object {-not $_.passed})
    $outputRoot = 'C:\Users\PC\tmp\FiveVillageSpeedContractE'
    [IO.Directory]::CreateDirectory($outputRoot) | Out-Null
    $reportPath = Join-Path $outputRoot 'offline-contract-tests.json'
    if (Test-Path -LiteralPath $reportPath) {
        Copy-Item -LiteralPath $reportPath -Destination (Join-Path $outputRoot ('offline-contract-tests-prior-'+[DateTime]::UtcNow.Ticks+'.json'))
    }
    [ordered]@{
        schema_version=1
        status=if($failed.Count){'offline_contract_tests_failed'}else{'offline_contract_tests_passed'}
        recorded_utc=[DateTime]::UtcNow.ToString('o');checks=$taskResults.Count;failures=$failed.Count
        test_scope='synthetic result contracts and temporary save files only'
        shared_source_modified=$false;unreal_editor='not_run';unreal_build_tool='not_run';actual_travel='not_run'
        contract_path='Scripts/FiveVillageTravelContract.ps1'
        contract_sha256=(Get-FileHash -LiteralPath (Join-Path $taskRoot 'Scripts/FiveVillageTravelContract.ps1') -Algorithm SHA256).Hash
        tests_path='Scripts/Tests/TestFiveVillageTravelContract20261111.ps1'
        tests_sha256=(Get-FileHash -LiteralPath $PSCommandPath -Algorithm SHA256).Hash
        temporary_directory=$taskTemp;unexpected_exception=$taskFatal;results=@($taskResults.ToArray())
    } | ConvertTo-Json -Depth 7 | Set-Content -LiteralPath $reportPath -Encoding utf8NoBOM
    Write-Output ('Offline five-village contract checks='+$taskResults.Count+' failures='+$failed.Count)
}
if ($failed.Count -gt 0) { throw 'Offline travel contract tests failed. Evidence retained.' }
