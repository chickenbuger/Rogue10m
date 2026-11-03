param(
    [Parameter(Mandatory=$true)][ValidatePattern('^[A-Za-z0-9_]{1,24}$')][string]$Label,
    [ValidateRange(1300,1800)][int]$MaxSeconds=1560,
    [switch]$Visible,
    [switch]$PreparedBossStrategy,
    [switch]$OrbitBossStrategy,
    [switch]$AnimationEvidence,
    [switch]$DodgeObserve,
    [switch]$GearUI,
    [switch]$GreatSwordGrowth,
    [switch]$NextRunConnection,
    [switch]$RecordWindowVideo,
    [string]$ProjectRoot='D:\Project\Rogue10m'
)
# DRAFT launcher: integrate/review/build before running. No gameplay-state fixtures.
$ErrorActionPreference='Stop'
if($DodgeObserve -and -not $PreparedBossStrategy){throw 'DodgeObserve requires -PreparedBossStrategy; no launch or profile creation.'}
if($OrbitBossStrategy -and -not $PreparedBossStrategy){throw 'OrbitBossStrategy requires -PreparedBossStrategy; no launch or profile creation.'}
if($GearUI -and -not $Visible){throw 'GearUI requires -Visible for the native Slate hit-test path; no offscreen fallback or launch.'}
if($GreatSwordGrowth -and -not $GearUI){throw 'GreatSwordGrowth requires explicit -GearUI and its -Visible UI path; nothing was started.'}
if($NextRunConnection -and -not $Visible){throw 'NextRunConnection requires explicit -Visible; no launch or profile creation.'}
if($RecordWindowVideo -and -not $Visible){throw 'RecordWindowVideo requires explicit -Visible; no launch or profile creation.'}

# Startup observation only. No process is stopped by this function.
function Wait-OwnedStartupIdentity([Diagnostics.Process]$Process,[int]$ExpectedId,[string]$ExpectedExecutable){
    $expected=[IO.Path]::GetFullPath($ExpectedExecutable)
    $clock=[Diagnostics.Stopwatch]::StartNew();$lastError='path_or_start_time_not_yet_available'
    while($clock.Elapsed.TotalMilliseconds -lt 5000){
        try{
            $Process.Refresh()
            if($Process.Id -ne $ExpectedId){throw 'The original process object changed identity.'}
            if($Process.HasExited){
                return [pscustomobject]@{Confirmed=$false;PID=$ExpectedId;StartUtcTicks=$null;Path=$null;ExitedDuringStartup=$true;WaitMilliseconds=[int]$clock.Elapsed.TotalMilliseconds;Error='owned_process_exited_before_identity_observed'}
            }
            $candidatePath=$Process.Path
            $candidateTicks=$Process.StartTime.ToUniversalTime().Ticks
            if(-not [string]::IsNullOrWhiteSpace($candidatePath) -and $candidateTicks -gt 0){
                $candidatePath=[IO.Path]::GetFullPath($candidatePath)
                if($candidatePath -ine $expected){
                    return [pscustomobject]@{Confirmed=$false;PID=$ExpectedId;StartUtcTicks=$candidateTicks;Path=$candidatePath;ExitedDuringStartup=$false;WaitMilliseconds=[int]$clock.Elapsed.TotalMilliseconds;Error='startup_executable_path_mismatch_no_stop_permitted'}
                }
                return [pscustomobject]@{Confirmed=$true;PID=$ExpectedId;StartUtcTicks=$candidateTicks;Path=$candidatePath;ExitedDuringStartup=$false;WaitMilliseconds=[int]$clock.Elapsed.TotalMilliseconds;Error=$null}
            }
        }catch{$lastError=$_.Exception.Message}
        $remaining=5000-[int]$clock.Elapsed.TotalMilliseconds
        if($remaining -gt 0){Start-Sleep -Milliseconds ([Math]::Min(100,$remaining))}
    }
    return [pscustomobject]@{Confirmed=$false;PID=$ExpectedId;StartUtcTicks=$null;Path=$null;ExitedDuringStartup=$false;WaitMilliseconds=[int]$clock.Elapsed.TotalMilliseconds;Error=('startup_identity_not_confirmed_no_stop_permitted: '+$lastError)}
}

function Assert-PhysicalDirectory([string]$Path){
    $entry=Get-Item -LiteralPath $Path -Force
    if(-not $entry.PSIsContainer){throw ('Required directory is missing: '+$Path)}
    $cursor=$entry
    while($null -ne $cursor){
        if(($cursor.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0){throw ('Linked directory is not allowed: '+$cursor.FullName)}
        $cursor=$cursor.Parent
    }
    return [IO.Path]::GetFullPath($entry.FullName).TrimEnd('\')
}
function Get-SaveInventory([string]$Directory){
    $root=Assert-PhysicalDirectory $Directory
    $pending=New-Object 'System.Collections.Generic.Stack[string]'
    $pending.Push($root)
    $rows=New-Object 'System.Collections.Generic.List[object]'
    while($pending.Count -gt 0){
        $current=$pending.Pop()
        foreach($entry in @(Get-ChildItem -LiteralPath $current -Force)){
            if(($entry.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0){throw ('Linked save entry is not allowed: '+$entry.FullName)}
            if($entry.PSIsContainer){$pending.Push($entry.FullName); continue}
            if($entry.Extension -ieq '.sav'){
                $rows.Add([pscustomobject][ordered]@{
                    RelativePath=$entry.FullName.Substring($root.Length+1)
                    Bytes=$entry.Length
                    SHA256=(Get-FileHash -LiteralPath $entry.FullName -Algorithm SHA256).Hash
                    LastWriteUtc=$entry.LastWriteTimeUtc.ToString('o')
                })
            }
        }
    }
    return @($rows.ToArray()|Sort-Object RelativePath)
}
function Save-Json([string]$Path,$Value){
    $json=ConvertTo-Json -InputObject $Value -Depth 12
    [IO.File]::WriteAllText($Path,$json,[Text.UTF8Encoding]::new($false))
}
function Read-DriverSummary([string]$Path,[bool]$ExpectPrepared=$false,[bool]$ExpectAnimation=$false,[bool]$ExpectGear=$false,[bool]$ExpectOrbit=$false,[bool]$ExpectGreatSwordGrowth=$false,[bool]$ExpectDodgeObserve=$false,[bool]$ExpectNextRun=$false,[bool]$ExpectRecording=$false){
    $values=[ordered]@{}
    $issues=New-Object 'System.Collections.Generic.List[string]'
    if(-not(Test-Path -LiteralPath $Path -PathType Leaf)){
        $issues.Add('driver_summary_missing')
    }else{
        foreach($line in [IO.File]::ReadAllLines($Path)){
            $at=$line.IndexOf('=')
            if($at -lt 1){continue}
            $key=$line.Substring(0,$at).Trim()
            $value=$line.Substring($at+1).Trim()
            if($values.Contains($key)){$issues.Add('duplicate_summary_key:'+ $key); continue}
            $values[$key]=$value
        }
        if(-not($values.Contains('outcome')) -or $values['outcome'] -notin @('returned_victory','returned_death','returned_timeout','blocked')){$issues.Add('invalid_or_missing_outcome')}
        if(-not($values.Contains('full_boss_completion')) -or $values['full_boss_completion'] -notin @('0','1')){$issues.Add('invalid_or_missing_full_boss_completion')}
        if(-not($values.Contains('physical_input')) -or $values['physical_input'] -cne 'not_performed'){$issues.Add('invalid_or_missing_physical_input_disclosure')}
        if($values['full_boss_completion'] -ceq '1' -and $values['outcome'] -cne 'returned_victory'){$issues.Add('boss_completion_contradicts_outcome')}
        if($ExpectPrepared -and (-not $values.Contains('prepared_boss_strategy') -or $values['prepared_boss_strategy'] -cne '1')){$issues.Add('requested_prepared_strategy_not_confirmed_by_driver')}
        if($ExpectOrbit -and (-not $values.Contains('orbit_boss_strategy') -or $values['orbit_boss_strategy'] -cne '1')){$issues.Add('requested_orbit_strategy_not_confirmed_by_driver')}
        if(-not $ExpectOrbit -and $values.Contains('orbit_boss_strategy') -and $values['orbit_boss_strategy'] -cne '0'){$issues.Add('unexpected_orbit_strategy_in_baseline_run')}
        if($ExpectAnimation -and (-not $values.Contains('animation_evidence') -or $values['animation_evidence'] -cne 'opportunistic_read_only')){$issues.Add('requested_animation_observation_not_confirmed_by_driver')}
        if(-not $ExpectAnimation -and $values.Contains('animation_evidence')){$issues.Add('unexpected_animation_observation_in_baseline_run')}
        if($ExpectGear -and (-not $values.Contains('gear_ui_requested') -or $values['gear_ui_requested'] -cne '1')){$issues.Add('requested_gear_ui_not_confirmed_by_driver')}
        if($ExpectGear){
            foreach($key in @('gear_ui_click_routed','gear_ui_equipped_verified')){
                if(-not $values.Contains($key) -or $values[$key] -notin @('0','1')){$issues.Add('invalid_or_missing_'+$key)}
            }
            if($values['gear_ui_equipped_verified'] -ceq '1' -and $values['gear_ui_click_routed'] -cne '1'){$issues.Add('gear_equipment_without_ui_input_confirmation')}
            if($values['full_boss_completion'] -ceq '1' -and $values['gear_ui_equipped_verified'] -cne '1'){$issues.Add('boss_completion_without_requested_gear_ui_verification')}
        }
        if($ExpectGreatSwordGrowth){
            if(-not $ExpectGear -or $values['gear_ui_requested'] -cne '1'){$issues.Add('growth_requires_requested_gear_ui')}
            if(-not $values.Contains('great_sword_growth_requested') -or $values['great_sword_growth_requested'] -cne '1'){$issues.Add('requested_great_sword_growth_not_confirmed')}
            if($values['requested_learning_node'] -cne 'I.Mana.Entry'){$issues.Add('growth_wrong_requested_learning_node')}
            foreach($key in @('great_sword_growth_ready','great_sword_growth_ui_click','great_sword_growth_learning_verified')){
                if(-not $values.Contains($key) -or $values[$key] -notin @('0','1')){$issues.Add('invalid_or_missing_'+$key)}
            }
            if($values['great_sword_growth_ui_click'] -ceq '1' -and $values['great_sword_growth_ready'] -cne '1'){$issues.Add('growth_ui_click_without_preparation')}
            if($values['normal_UMG_run_learning'] -ceq '1' -and $values['great_sword_growth_learning_verified'] -cne '1'){$issues.Add('growth_normal_learning_without_paid_verification')}
            if($values['great_sword_growth_learning_verified'] -ceq '1'){
                foreach($key in @('great_sword_growth_ready','great_sword_growth_ui_click','normal_UMG_run_learning','gear_ui_equipped_verified')){
                    if($values[$key] -cne '1'){$issues.Add('growth_learning_without_'+$key)}
                }
                if($values['confirmed_learning_node'] -cne 'I.Mana.Entry'){$issues.Add('growth_wrong_confirmed_learning_node')}
                $growthNumbers=[ordered]@{}
                foreach($key in @('great_sword_growth_level','great_sword_growth_mastery','great_sword_growth_node_cost','great_sword_growth_points_before','great_sword_growth_points_after','great_sword_growth_spent_before','great_sword_growth_spent_after')){
                    [int]$parsedGrowthValue=0
                    if(-not $values.Contains($key) -or -not [int]::TryParse([string]$values[$key],[ref]$parsedGrowthValue)){$issues.Add('growth_invalid_integer_'+$key)}
                    else{$growthNumbers[$key]=$parsedGrowthValue}
                }
                if($growthNumbers.Count -eq 7){
                    if($growthNumbers['great_sword_growth_level'] -lt 3 -or $growthNumbers['great_sword_growth_mastery'] -lt 3){$issues.Add('growth_preparation_milestones_missing')}
                    $growthActualCost=$growthNumbers['great_sword_growth_node_cost']
                    if($growthNumbers['great_sword_growth_points_after'] -lt 0 -or $growthNumbers['great_sword_growth_spent_before'] -lt 0 -or $growthNumbers['great_sword_growth_spent_after'] -lt 0){$issues.Add('growth_negative_points_ledger')}
                    if($growthActualCost -lt 1 -or $growthNumbers['great_sword_growth_points_before'] -lt $growthActualCost -or
                        $growthNumbers['great_sword_growth_spent_after']-$growthNumbers['great_sword_growth_spent_before'] -ne $growthActualCost){$issues.Add('growth_paid_points_not_verified')}
                }
            }
            if($values['full_boss_completion'] -ceq '1' -and $values['great_sword_growth_learning_verified'] -cne '1'){$issues.Add('boss_completion_without_requested_GS_learning')}
        }elseif($values.Contains('great_sword_growth_requested') -and $values['great_sword_growth_requested'] -cne '0'){$issues.Add('unexpected_great_sword_growth_mode')}
        if(-not $ExpectGear -and $values.Contains('gear_ui_requested') -and $values['gear_ui_requested'] -cne '0'){$issues.Add('unexpected_gear_ui_in_baseline_run')}
        if(-not $ExpectPrepared -and $values.Contains('prepared_boss_strategy') -and $values['prepared_boss_strategy'] -cne '0'){$issues.Add('unexpected_prepared_strategy_in_baseline_run')}
    }
    if($ExpectDodgeObserve -and $values['dodge_observe_requested'] -cne '1'){$issues.Add('requested_dodge_observer_not_confirmed_by_driver')}
    if($ExpectNextRun){
        if($values['next_run_connection_requested'] -cne '1'){$issues.Add('requested_next_run_connection_not_confirmed')}
        $nextBooleanKeys=@('first_run_full_gate_frozen','next_run_hub_verified','next_run_field_verified','next_run_timer_verified','next_run_cancel_focus_observed','next_run_cancel_action_verified','next_run_abandon_ui_submitted','next_run_saved_receipt_observed','next_run_returned_hub','next_run_same_profile_verified','next_run_connection_complete','next_run_returned_slot_reset_verified')
        foreach($key in $nextBooleanKeys){if(-not $values.Contains($key) -or $values[$key] -notin @('0','1')){$issues.Add('invalid_or_missing_'+$key)}}
        if($values['first_run_full_gate_frozen'] -ceq '1'){
            if($values['outcome'] -cne 'returned_victory' -or $values['full_boss_completion'] -cne '1' -or $values['first_run_frozen_outcome'] -cne 'returned_victory' -or $values['first_run_frozen_terminal'] -cne 'Victory'){$issues.Add('next_run_without_preserved_first_full_victory')}
            if($values['next_run_connection_complete'] -cne '1'){$issues.Add('next_run_connection_incomplete_after_first_victory')}
        }else{
            foreach($key in $nextBooleanKeys | Where-Object {$_ -cne 'first_run_full_gate_frozen'}){if($values[$key] -ceq '1'){$issues.Add('next_run_milestone_without_first_victory:'+ $key)}}
            if($values['full_boss_completion'] -ceq '1'){$issues.Add('first_full_victory_not_frozen_for_requested_next_run')}
        }
        foreach($key in @('next_run_slot_initial_baseline_recorded','next_run_slot_reset_verified')){
            if(-not $values.Contains($key) -or $values[$key] -notin @('0','1')){$issues.Add('invalid_or_missing_'+$key)}
        }
        if($values['next_run_slot_comparison_scope'] -cne 'four_effective_stable_asset_or_null_and_IsAttackSkillUnlocked'){$issues.Add('next_run_slot_comparison_scope_missing')}
        if($values['next_run_slot_reset_verified'] -ceq '1' -and ($values['next_run_slot_initial_baseline_recorded'] -cne '1' -or $values['first_run_full_gate_frozen'] -cne '1')){$issues.Add('next_run_slot_verification_without_initial_baseline_or_first_victory')}
        if($values['next_run_field_verified'] -ceq '1' -and $values['next_run_slot_reset_verified'] -cne '1'){$issues.Add('next_run_field_without_slot_reset_verification')}
        $nextDependencies=@{
            next_run_field_verified=@('next_run_hub_verified')
            next_run_timer_verified=@('next_run_field_verified')
            next_run_cancel_action_verified=@('next_run_timer_verified','next_run_cancel_focus_observed')
            next_run_abandon_ui_submitted=@('next_run_timer_verified','next_run_cancel_focus_observed','next_run_cancel_action_verified')
            next_run_saved_receipt_observed=@('next_run_abandon_ui_submitted')
            next_run_returned_slot_reset_verified=@('next_run_slot_reset_verified','next_run_saved_receipt_observed')
            next_run_returned_hub=@('next_run_saved_receipt_observed','next_run_returned_slot_reset_verified')
            next_run_connection_complete=@('first_run_full_gate_frozen','next_run_hub_verified','next_run_field_verified','next_run_timer_verified','next_run_cancel_focus_observed','next_run_cancel_action_verified','next_run_abandon_ui_submitted','next_run_saved_receipt_observed','next_run_returned_hub','next_run_same_profile_verified','next_run_slot_initial_baseline_recorded','next_run_slot_reset_verified','next_run_returned_slot_reset_verified')
        }
        foreach($key in $nextDependencies.Keys){if($values[$key] -ceq '1'){foreach($needed in $nextDependencies[$key]){if($values[$needed] -cne '1'){$issues.Add('next_run_dependency_missing:'+ $key+':'+$needed)}}}}
        if($values['next_run_connection_complete'] -ceq '1'){
            if($values['next_run_status'] -cne 'complete'){$issues.Add('next_run_completed_with_wrong_status')}
            $nextParsedIds=@{}
            foreach($key in @('next_run_first_id','next_run_id','next_run_after_return_id')){
                $nextParsedGuid=[guid]::Empty
                if(-not [guid]::TryParseExact([string]$values[$key],'N',[ref]$nextParsedGuid) -or $nextParsedGuid -eq [guid]::Empty){$issues.Add('next_run_invalid_RunId:'+ $key)}
                else{$nextParsedIds[$key]=$nextParsedGuid}
            }
            if($nextParsedIds.Count -eq 3 -and @($nextParsedIds.Values | Select-Object -Unique).Count -ne 3){$issues.Add('next_run_RunIds_not_distinct_at_Hub_boundaries')}
        }
    }elseif($values.Contains('next_run_connection_requested') -and $values['next_run_connection_requested'] -cne '0'){$issues.Add('unexpected_next_run_connection_in_baseline_run')}
    if($ExpectRecording){
        if($values['recording_required'] -cne '1'){$issues.Add('requested_recording_gate_not_confirmed')}
        if(-not $values.Contains('recording_ready_observed') -or $values['recording_ready_observed'] -notin @('0','1')){$issues.Add('invalid_or_missing_recording_ready_observed')}
        if($values['recording_ready_observed'] -ceq '1' -and $values['recording_ready_source_scope'] -cne 'ready_JSON_window_zero_audio_first_chunk_fields_validated'){$issues.Add('recording_ready_scope_not_confirmed')}
        if($values['full_boss_completion'] -ceq '1' -and $values['recording_ready_observed'] -cne '1'){$issues.Add('boss_completion_without_requested_recording_ready')}
    }elseif($values.Contains('recording_required') -and $values['recording_required'] -cne '0'){$issues.Add('unexpected_recording_gate_in_baseline_run')}
    if($values['recording_ready_observed'] -ceq '1' -and $values['recording_required'] -cne '1'){$issues.Add('recording_ready_without_recording_required')}
    return [pscustomobject]@{Values=$values;Issues=@($issues.ToArray());Valid=($issues.Count -eq 0)}
}

$expectedRoot=[IO.Path]::GetFullPath('D:\Project\Rogue10m').TrimEnd('\')
if([IO.Path]::GetFullPath($ProjectRoot).TrimEnd('\') -ine $expectedRoot){throw 'Only D:\Project\Rogue10m is supported by this launcher.'}
$ProjectRoot=Assert-PhysicalDirectory $ProjectRoot
$null=Assert-PhysicalDirectory (Join-Path $ProjectRoot 'Content')
$saveDirectory=Assert-PhysicalDirectory (Join-Path $ProjectRoot 'Saved\SaveGames')
$null=Assert-PhysicalDirectory (Join-Path $ProjectRoot 'DevLog')
$projectFile=Join-Path $ProjectRoot 'Rogue10m.uproject'
if(-not(Test-Path -LiteralPath $projectFile -PathType Leaf)){throw 'Reviewed Rogue10m.uproject was not found.'}
$projectInfo=Get-Item -LiteralPath $projectFile -Force
if(($projectInfo.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0){throw 'Linked project file is not allowed.'}
$existingUE=@(Get-Process -Name 'UnrealEditor','UnrealEditor-Cmd','UnrealBuildTool','ShaderCompileWorker','CrashReportClient' -ErrorAction SilentlyContinue)
if($existingUE.Count -gt 0){throw ('Existing UE-related processes must be coordinated first; refusing launch. PIDs='+ (($existingUE|ForEach-Object{$_.Id}) -join ','))}

$profile='Rogue10m_Town_QA_BasicRun_'+$Label
$prefix='Rogue10m_Progression_QA_BasicRun_'+$Label
$keys='Rogue10m_Fighter_QA_BasicRun_'+$Label
$qaRoot=Join-Path $ProjectRoot 'DevLog\BasicRunPlayQA'
if(Test-Path -LiteralPath $qaRoot){$null=Assert-PhysicalDirectory $qaRoot}
$session=Join-Path $qaRoot $Label
$driverOutput=Join-Path $session 'Driver'
$RecordingToken=$null
if($RecordWindowVideo){$RecordingToken=[guid]::NewGuid().ToString('N').ToLowerInvariant()}
$RecordingReadyFile=Join-Path $driverOutput 'window-recording-ready.json'
$log=Join-Path $session 'game.log'
if(Test-Path -LiteralPath $session){throw 'Existing session evidence must be preserved; choose a fresh Label.'}
$before=@(Get-SaveInventory $saveDirectory)
if(@($before|Where-Object{
    [IO.Path]::GetFileName($_.RelativePath) -ceq ($profile+'.sav') -or
    [IO.Path]::GetFileName($_.RelativePath) -ceq ($keys+'.sav') -or
    [IO.Path]::GetFileName($_.RelativePath).StartsWith($prefix+'_',[StringComparison]::OrdinalIgnoreCase)
}).Count -gt 0){throw 'Fresh-label QA save collision; no files have been deleted or reset.'}

$exe='D:\Program Files\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
if(-not(Test-Path -LiteralPath $exe -PathType Leaf)){throw 'Installed UE 5.8 editor executable is missing.'}
$arguments=@(('"'+$projectFile+'"'),'/Game/Rogue10m/Maps/L_Menu',
    '-game','-d3d11','-windowed','-ForceRes','-ResX=1280','-ResY=720',
    '-unattended','-nosplash','-NoLiveCoding','-NoRemoteShaderCompile',
    ('-RogueProfileSaveSlot='+$profile),('-RogueProgressionSavePrefix='+$prefix),('-RogueSkillKeySaveSlot='+$keys),
    ('-RogueBasicRunLabel='+$Label),('-RogueBasicRunOutput="'+$driverOutput+'"'),('-RogueBasicRunMaxSeconds='+$MaxSeconds),
    '-ExecCmds="t.MaxFPS 60,Rogue10m.TestBasicRunPlay"',('-abslog="'+$log+'"'))
if($PreparedBossStrategy){$arguments+='-RogueBasicRunPreparedBoss'}
if($OrbitBossStrategy){$arguments+='-RogueBasicRunOrbitBoss'}
if($AnimationEvidence){$arguments+='-RogueBasicRunAnimationEvidence'}
if($DodgeObserve){$arguments+='-RogueBasicRunDodgeObserve'}
if($GearUI){$arguments+='-RogueBasicRunGearUI'}
if($GreatSwordGrowth){$arguments+='-RogueBasicRunGreatSwordGrowth'}
if($NextRunConnection){$arguments+='-RogueBasicRunNextRunConnection'}
if($RecordWindowVideo){$arguments+='-RogueBasicRunRequireRecording';$arguments+=('-RogueBasicRunRecordingToken='+$RecordingToken)}
$windowStyle='Hidden'
if($Visible){$windowStyle='Normal'}else{$arguments+='-RenderOffscreen'}
if(-not(Test-Path -LiteralPath $qaRoot)){New-Item -ItemType Directory -Path $qaRoot|Out-Null}
$null=Assert-PhysicalDirectory $qaRoot
New-Item -ItemType Directory -Path $session|Out-Null
$null=Assert-PhysicalDirectory $session
Save-Json (Join-Path $session 'before-saves.json') $before
$metadata=[ordered]@{
    Schema='Rogue10m.BasicRunLauncher.v1'; Label=$Label; ProjectRoot=$ProjectRoot
    StartedUtc=[DateTime]::UtcNow.ToString('o'); State='prepared'; Visible=[bool]$Visible
    MaxSeconds=$MaxSeconds; WatchdogSeconds=($MaxSeconds+45); Executable=$exe; Arguments=$arguments
    ProfileSlot=$profile; ProgressionPrefix=$prefix; KeySlot=$keys; DriverOutput=$driverOutput
    RecordWindowVideo=[bool]$RecordWindowVideo;RecordingToken=$RecordingToken;RecordingReadyFile=$RecordingReadyFile
    PhysicalInput='not_performed'; GameplayShortcuts='none_added_by_launcher'; PreparedBossStrategy=[bool]$PreparedBossStrategy; OrbitBossStrategy=[bool]$OrbitBossStrategy; AnimationEvidence=[bool]$AnimationEvidence; DodgeObserve=[bool]$DodgeObserve; GearUI=[bool]$GearUI; GreatSwordGrowth=[bool]$GreatSwordGrowth; NextRunConnection=[bool]$NextRunConnection
    BeforeSaveCount=$before.Count; OwnedProcess=$null; ProcessExitConfirmed=$false; ProcessExitCode=$null
    WatchdogStopAttempted=$false; WatchdogStopVerified=$false; Preservation='pending'; Error=$null
}
Save-Json (Join-Path $session 'session.json') $metadata
Save-Json (Join-Path $session 'manifest.json') ([ordered]@{
    Schema='Rogue10m.BasicRunLaunchManifest.v1';Label=$Label;ProjectRoot=$ProjectRoot;ProjectFile=$projectFile
    ProjectFileSHA256=(Get-FileHash -LiteralPath $projectFile -Algorithm SHA256).Hash
    Executable=$exe;Arguments=$arguments;MaxSeconds=$MaxSeconds;WatchdogSeconds=($MaxSeconds+45)
    ProfileSlot=$profile;ProgressionPrefix=$prefix;KeySlot=$keys;DriverOutput=$driverOutput
    RecordWindowVideo=[bool]$RecordWindowVideo;RecordingToken=$RecordingToken;RecordingReadyFile=$RecordingReadyFile
    SaveInventoryBefore='before-saves.json';SaveInventoryAfter='after-saves.json';Log='game.log'
    PhysicalInput='not_performed';NormalRoutePolicy='UI and driver-generated gameplay input only; no state injection or direct progression/pickup/travel APIs'
})
$process=$null
$launched=$false
$ownedId=0
$ownedStartTicks=0L
$ownedPath=''
$identityConfirmed=$false
$timedOut=$false
$exitStatus=1

try{
    # Final independent process check after evidence preparation; do not overlap another UE.
    $racingUE=@(Get-Process -Name 'UnrealEditor','UnrealEditor-Cmd','UnrealBuildTool','ShaderCompileWorker','CrashReportClient' -ErrorAction SilentlyContinue)
    if($racingUE.Count -gt 0){throw 'A UE process appeared during preflight; launch not performed.'}
    $process=Start-Process -FilePath $exe -ArgumentList $arguments -WindowStyle $windowStyle -PassThru
    $launched=$true
    $ownedId=$process.Id
    Write-Output ('BASIC_RUN_STARTED pid='+$ownedId+' identity=pending')
    $metadata.OwnedProcess=[ordered]@{PID=$ownedId;StartUtcTicks=$null;Path=$null;IdentityConfirmed=$false}
    $metadata.State='startup_identity_pending';Save-Json (Join-Path $session 'session.json') $metadata
    $identity=Wait-OwnedStartupIdentity $process $ownedId $exe
    $metadata.StartupIdentity=$identity
    Save-Json (Join-Path $session 'session.json') $metadata
    if(-not $identity.Confirmed){throw $identity.Error}
    $ownedStartTicks=[long]$identity.StartUtcTicks;$ownedPath=$identity.Path;$identityConfirmed=$true
    $metadata.OwnedProcess=[ordered]@{PID=$ownedId;StartUtcTicks=$ownedStartTicks;Path=$ownedPath;IdentityConfirmed=$true}
    $metadata.State='running'
    Save-Json (Join-Path $session 'session.json') $metadata
    Write-Output ('BASIC_RUN_LAUNCHED pid='+$ownedId+' evidence='+$session)
    $clock=[Diagnostics.Stopwatch]::StartNew()
    $nextNotice=30.0
    while(-not $process.WaitForExit(1000)){
        if($clock.Elapsed.TotalSeconds -ge ($MaxSeconds+45)){$timedOut=$true; throw 'Owned basic-run process exceeded its bounded watchdog.'}
        if($clock.Elapsed.TotalSeconds -ge $nextNotice){
            Write-Output ('BASIC_RUN_RUNNING pid='+$ownedId+' elapsed_seconds='+[int]$clock.Elapsed.TotalSeconds)
            $nextNotice+=30.0
        }
    }
}catch{
    $metadata.Error=$_.Exception.Message
    $metadata.State='attention_required'
}finally{
    if($launched -and $null -ne $process){
        try{
            $process.Refresh()
            if(-not $process.HasExited){
                # An unconfirmed startup is an observation/reconciliation case, never permission to stop a PID.
                if(-not $identityConfirmed){throw 'Startup identity was not confirmed; no process was killed.'}
                # Never enumerate/kill arbitrary UE workers. Revalidate our exact original process.
                $live=Get-Process -Id $ownedId -ErrorAction Stop
                $sameOwner=($live.Id -eq $ownedId -and $live.StartTime.ToUniversalTime().Ticks -eq $ownedStartTicks -and
                    [IO.Path]::GetFullPath($live.Path) -ieq $ownedPath -and $ownedPath -ieq [IO.Path]::GetFullPath($exe))
                if(-not $sameOwner){throw 'Process ownership changed or cannot be proved; no process was killed.'}
                $metadata.WatchdogStopAttempted=$true
                $metadata.State='owned_process_stop_pending'
                Save-Json (Join-Path $session 'watchdog-stop.json') ([ordered]@{
                    AtUtc=[DateTime]::UtcNow.ToString('o');TimedOut=$timedOut;PID=$ownedId;StartUtcTicks=$ownedStartTicks;Path=$ownedPath
                    DriverSummary=(Join-Path $driverOutput 'summary.txt');GameLog=$log
                    EvidencePolicy='existing driver csv/summary/log retained; no deletion or game-state repair'
                })
                Save-Json (Join-Path $session 'session.json') $metadata
                $process.Kill()
                $metadata.WatchdogStopVerified=$process.WaitForExit(5000)
            }
            $process.Refresh()
            $metadata.ProcessExitConfirmed=$process.HasExited
            if($process.HasExited){$metadata.ProcessExitCode=$process.ExitCode}
        }catch{
            $metadata.Error=($metadata.Error+'; '+$_.Exception.Message).Trim('; ')
            $metadata.ProcessExitConfirmed=$false
            # The process may have exited naturally between HasExited and identity inspection.
            try{
                $process.Refresh()
                if($process.HasExited){$metadata.ProcessExitConfirmed=$true;$metadata.ProcessExitCode=$process.ExitCode}
            }catch{}
        }
    }else{
        # No launch means there is no owned process to stop; still verify unchanged saves.
        $metadata.ProcessExitConfirmed=$true
    }

    if($metadata.ProcessExitConfirmed){
        try{
            $after=@(Get-SaveInventory $saveDirectory)
            Save-Json (Join-Path $session 'after-saves.json') $after
            $beforeMap=@{}; foreach($row in $before){$beforeMap[$row.RelativePath]=$row}
            $afterMap=@{}; foreach($row in $after){$afterMap[$row.RelativePath]=$row}
            $changed=New-Object 'System.Collections.Generic.List[object]'
            $missing=New-Object 'System.Collections.Generic.List[string]'
            foreach($row in $before){
                if(-not $afterMap.ContainsKey($row.RelativePath)){$missing.Add($row.RelativePath)}
                elseif($afterMap[$row.RelativePath].SHA256 -cne $row.SHA256){
                    $changed.Add([pscustomobject]@{RelativePath=$row.RelativePath;BeforeSHA256=$row.SHA256;AfterSHA256=$afterMap[$row.RelativePath].SHA256})
                }
            }
            $newAllowed=New-Object 'System.Collections.Generic.List[object]'
            $newUnknown=New-Object 'System.Collections.Generic.List[object]'
            $progressionPattern='^'+[regex]::Escape($prefix)+'_[0-9A-Fa-f]{32}_v5\.sav$'
            foreach($row in $after){
                if($beforeMap.ContainsKey($row.RelativePath)){continue}
                $isRootFile=([IO.Path]::GetFileName($row.RelativePath) -ceq $row.RelativePath)
                $isOwned=$isRootFile -and ($row.RelativePath -ceq ($profile+'.sav') -or
                    $row.RelativePath -ceq ($keys+'.sav') -or $row.RelativePath -cmatch $progressionPattern)
                if($isOwned){$newAllowed.Add($row)}else{$newUnknown.Add($row)}
            }
            $preserved=($changed.Count -eq 0 -and $missing.Count -eq 0)
            $comparison=[ordered]@{
                BeforeCount=$before.Count;AfterCount=$after.Count;OriginalBytesPreserved=$preserved
                ChangedOriginals=@($changed.ToArray());MissingOriginals=@($missing.ToArray())
                NewAllowedQASaves=@($newAllowed.ToArray());UnknownNewSaves=@($newUnknown.ToArray())
                UnknownSaveAction='flag_no_delete';AnyRestoreDeleteOrAttributeChange=$false
            }
            Save-Json (Join-Path $session 'save-comparison.json') $comparison
            $metadata.Preservation=if($preserved -and $newUnknown.Count -eq 0){'verified'}else{'attention_required_no_files_deleted_or_restored'}
            $metadata.AfterSaveCount=$after.Count
        }catch{
            $metadata.Preservation='uncertain_inventory_or_hash_error'
            $metadata.Error=($metadata.Error+'; '+$_.Exception.Message).Trim('; ')
        }
    }else{
        $metadata.Preservation='uncertain_process_still_running_or_identity_unproved'
        $metadata.State='uncertain'
    }

    try{
        $summary=Read-DriverSummary (Join-Path $driverOutput 'summary.txt') ([bool]$PreparedBossStrategy) ([bool]$AnimationEvidence) ([bool]$GearUI) ([bool]$OrbitBossStrategy) ([bool]$GreatSwordGrowth) ([bool]$DodgeObserve) ([bool]$NextRunConnection) ([bool]$RecordWindowVideo)
    }catch{
        $summary=[pscustomobject]@{Values=[ordered]@{};Issues=@('driver_summary_read_error:'+ $_.Exception.Message);Valid=$false}
        $metadata.Error=($metadata.Error+'; '+$_.Exception.Message).Trim('; ')
    }
    Save-Json (Join-Path $session 'driver-summary-parsed.json') $summary
    $metadata.DriverSummary=$summary.Values
    $metadata.DriverSummaryIssues=$summary.Issues
    $metadata.FinishedUtc=[DateTime]::UtcNow.ToString('o')
    $observed=$launched -and $identityConfirmed -and [string]::IsNullOrEmpty($metadata.Error) -and $metadata.ProcessExitConfirmed -and $metadata.ProcessExitCode -eq 0 -and
        $summary.Valid -and $metadata.Preservation -ceq 'verified' -and -not $timedOut
    if($observed){
        $metadata.State='recorded'
        if($summary.Values['outcome'] -ceq 'blocked'){$exitStatus=1}else{$exitStatus=0}
    }elseif($metadata.State -cne 'uncertain'){$metadata.State='attention_required'}
    Save-Json (Join-Path $session 'session.json') $metadata
    # Do not turn a death/timeout/blocked run or exit code 0 into a boss-completion claim.
    Write-Output ('BASIC_RUN_RECORDED outcome='+$summary.Values['outcome']+' full_boss_completion='+$summary.Values['full_boss_completion']+
        ' great_sword_growth='+$summary.Values['great_sword_growth_requested']+' learning_node='+$summary.Values['confirmed_learning_node']+
        ' next_run_connection='+$summary.Values['next_run_connection_complete']+' next_run_status='+$summary.Values['next_run_status']+
        ' recording_ready='+$summary.Values['recording_ready_observed']+' recording_scope='+$summary.Values['recording_ready_source_scope']+
        ' physical_input='+$summary.Values['physical_input']+' preservation='+$metadata.Preservation+' state='+$metadata.State+' evidence='+$session)
}
exit $exitStatus
