param(
    [Parameter(Mandatory=$true)][ValidatePattern('^[A-Za-z0-9_]{1,24}$')][string]$Label,
    [switch]$Rendered,
    [switch]$Offscreen,
    [switch]$VisibleWindow,
    [ValidateSet('D3D11','D3D12')][string]$Renderer='D3D11',
    [ValidateRange(60,260)][int]$TimeoutSeconds=240,
    [string]$ProjectRoot=(Split-Path -Parent $PSScriptRoot)
)
$ErrorActionPreference='Stop'
function Read-BomText([string]$Path) {
    # Unreal may write UTF-16 when localized text is present.
    $reader=[IO.StreamReader]::new($Path,[Text.Encoding]::UTF8,$true)
    try { return $reader.ReadToEnd() } finally { $reader.Dispose() }
}
function Get-SaveInventory([string]$Directory) {
    if(-not(Test-Path -LiteralPath $Directory)){return @()}
    return @(Get-ChildItem -LiteralPath $Directory -Filter '*.sav' -File -Recurse | Sort-Object FullName | ForEach-Object {
        [pscustomobject]@{Path=$_.FullName;SHA256=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash}
    })
}
if($Offscreen -and -not $Rendered){throw 'Offscreen requires Rendered.'}
if($VisibleWindow -and (-not $Rendered -or $Offscreen)){throw 'VisibleWindow requires native Rendered mode without Offscreen.'}
$ProjectRoot=(Resolve-Path -LiteralPath $ProjectRoot).Path
$town='Rogue10m_Town_QA_FpNpc_'+$Label
$prefix='Rogue10m_Progression_QA_FpNpc_'+$Label
$keys='Rogue10m_Fighter_QA_FpNpc_'+$Label
$saveDirectory=Join-Path $ProjectRoot 'Saved\SaveGames'
$evidence=Join-Path $ProjectRoot 'Feature\doc\evidence\npc-interaction-20261029'
$output=Join-Path $evidence $Label
$images=Join-Path $ProjectRoot ('DevLog\20261029_NpcInteraction_'+$Label)
$log=Join-Path $evidence ($Label+'.log')
$beforeCsv=Join-Path $evidence ($Label+'.saves-before.csv')
$afterCsv=Join-Path $evidence ($Label+'.saves-after.csv')
if((Test-Path -LiteralPath $output) -or (Test-Path -LiteralPath $images) -or (Test-Path -LiteralPath $log) -or
    (Test-Path -LiteralPath $beforeCsv) -or (Test-Path -LiteralPath $afterCsv) -or
    (Test-Path -LiteralPath (Join-Path $saveDirectory ($town+'.sav'))) -or
    (Test-Path -LiteralPath (Join-Path $saveDirectory ($keys+'.sav'))) -or
    (Get-ChildItem -LiteralPath $saveDirectory -Filter ($prefix+'_*') -File -ErrorAction SilentlyContinue | Select-Object -First 1)) {
    throw 'Preserve previous FpNpc slots, images and evidence; use a fresh Label.'
}
$exe='D:\Program Files\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
if($Rendered){$exe='D:\Program Files\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'}
$window=if($VisibleWindow){'Normal'}else{'Hidden'}
$project=Join-Path $ProjectRoot 'Rogue10m.uproject'
if(-not(Test-Path -LiteralPath $exe) -or -not(Test-Path -LiteralPath $project)){throw 'Expected existing UE 5.8 executable or Rogue10m project file is missing.'}
$before=@(Get-SaveInventory $saveDirectory)
New-Item -ItemType Directory -Force -Path $evidence | Out-Null
$before | Export-Csv -LiteralPath $beforeCsv -NoTypeInformation -Encoding utf8
$arguments=@(('"'+$project+'"'),'/Game/Rogue10m/Maps/StartHub','-game','-unattended','-nosplash','-nosound',
    '-DisablePython','-language=en','-locale=en-US','-NoLiveCoding','-NoRemoteShaderCompile',('-RogueProfileSaveSlot='+$town),('-RogueProgressionSavePrefix='+$prefix),
    ('-RogueSkillKeySaveSlot='+$keys),('-RogueNpcInteractionLabel='+$Label),
    '-ExecCmds="t.MaxFPS 60,Rogue10m.TestNpcInteraction"',('-abslog="'+$log+'"'))
if($Rendered){$arguments+=@(('-'+$Renderer.ToLowerInvariant()),'-windowed','-ForceRes','-ResX=1280','-ResY=720')}else{$arguments+='-nullrhi'}
if($Offscreen){$arguments+='-RenderOffScreen'}
$process=$null
$failure=$null
$preservationFailure=$null
try {
    $process=Start-Process -FilePath $exe -ArgumentList $arguments -WindowStyle $window -PassThru
    Write-Output ('Owned NPC interaction QA PID='+$process.Id+' rendered='+$Rendered+' offscreen='+$Offscreen+' log='+$log)
    # Wait only for this invocation's exact Process. Root orchestration may yield while this runs.
    $timer=[Diagnostics.Stopwatch]::StartNew()
    while(-not $process.WaitForExit(1000)) {
        if($timer.Elapsed.TotalSeconds -ge $TimeoutSeconds){throw 'NPC interaction QA timed out.'}
    }
    $process.Refresh()
    $result=Join-Path $output 'results.json'
    $events=Join-Path $output 'events.csv'
    if(-not(Test-Path -LiteralPath $result)) {throw ('NPC QA did not produce results; exit='+$process.ExitCode+' log='+$log)}
    $resultText=Read-BomText $result
    Write-Output $resultText
    $data=$resultText | ConvertFrom-Json
    $required=@('label','rendered','offscreen','input','physical_input_performed','manual_field_run',
        'checks','failures','probe_passed','f_binding_accepted','collision_detected','evidence_note','fixture_revision','q_binding_accepted','f_rejection_preserved_bindings','positive_control_key','selected_skill','f_binding_reason','probes')
    foreach($field in $required){if($data.PSObject.Properties.Name -notcontains $field){throw ('Missing NPC QA field: '+$field)}}
    if($data.label -ne $Label -or [bool]$data.rendered -ne [bool]$Rendered -or [bool]$data.offscreen -ne [bool]$Offscreen -or
        $data.input -ne 'programmatic_Controller_InputKey_actual_legacy_and_enhanced_stack' -or
        [bool]$data.physical_input_performed -or [bool]$data.manual_field_run -or
        [int]$data.checks -lt 1 -or [int]$data.failures -lt 0) {throw 'NPC QA metadata does not match the requested isolated execution.'}
    $engineText=if(Test-Path -LiteralPath $log){Read-BomText $log}else{''}
    if([bool]$data.collision_detected -or [int]$data.failures -ne 0 -or -not [bool]$data.probe_passed) {
        $photoCount=if(Test-Path -LiteralPath $images){@(Get-ChildItem -LiteralPath $images -File -Filter '*.png').Count}else{0}
        Write-Output ('Preserved failed NPC QA evidence='+$output+' available_PNGs='+$photoCount+' images='+$images)
        throw ('NPC interaction QA failed; collision_detected='+$data.collision_detected+' failures='+$data.failures+' exit='+$process.ExitCode+' log='+$log)
    }
    if($process.ExitCode -ne 0 -or -not $engineText.Contains('RESULT=NPC_INTERACTION_PASSED')) {
        throw ('NPC QA JSON success lacks a matching engine success/exit; exit='+$process.ExitCode+' log='+$log)
    }
    if($data.fixture_revision -ne 'equipped_knuckle_reserved_F_paid_QER_LeftShift_sprint_bound_20261111' -or
        $data.f_binding_accepted -ne $false -or $data.q_binding_accepted -ne $true -or
        $data.f_rejection_preserved_bindings -ne $true -or $data.positive_control_key -ne 'Q' -or
        [string]::IsNullOrWhiteSpace($data.selected_skill) -or [string]::IsNullOrWhiteSpace($data.f_binding_reason)) {
        throw 'Expected reserved-F rejection with preserved loadout and a real accepted common-Q binding.'
    }
    foreach($positiveName in @('outside_binding_positive_control','fixed_Q_bare_positive','fixed_E_bare_positive','fixed_R_bare_positive')) {
    $positive=@($data.probes | Where-Object name -eq $positiveName)
    if($positive.Count -ne 1){throw 'Exactly one actual Q positive-control observation required.'}
    $p=$positive[0]
    if($positiveName -match '^fixed_[QER]_bare_positive$') {
        if($p.selected_body_montage_observed -ne $true -or $p.inactive_FP_montage_observed -ne $false -or
            $p.busy_weapon_change_violation -ne $false -or $p.body_finished_weapon_change_recovered -ne $true) {
            throw ('Missing actual body montage, inactive-FP safety or natural weapon-change recovery: '+$positiveName)
        }
    }
    if($p.uses_after -ne ($p.uses_before+1) -or $p.total_uses_after -ne ($p.total_uses_before+1) -or
        $p.last_accepted_skill -ne $data.selected_skill -or $p.max_cooldown_s -le 0.001 -or
        -not ($p.stamina_min -lt ($p.stamina_before-0.001) -or $p.mana_min -lt ($p.mana_before-0.001) -or $p.identity_min -lt ($p.identity_before-0.001))) {
        throw 'Real Q execution lacks exact-one skill, cooldown or positive resource-spend evidence.'
    }
    }
    foreach($name in @('fixed_Q_LeftShift_blocked','fixed_E_LeftShift_blocked','fixed_R_LeftShift_blocked','fixed_Q_menu_blocked','fixed_E_menu_blocked','fixed_R_menu_blocked','fixed_Q_menu_close_release_only','fixed_E_menu_close_release_only','fixed_R_menu_close_release_only','common_Q_menu_must_not_cast','reserved_F_menu_must_not_cast_or_talk','inside_NPC_F_must_not_cast','common_Q_dialogue_must_not_cast','reserved_F_closes_without_attack')) {
        $rows=@($data.probes | Where-Object name -eq $name)
        if($rows.Count -ne 1){throw ('Missing unique combat-blocking probe: '+$name)}
        $p=$rows[0]
        $staminaAllowance=0.0
        if($name -match '^fixed_[QER]_LeftShift_blocked$') {
            foreach($field in @('accepted_skill_before','last_accepted_skill','sprint_cost_per_second','sprint_tick_interval_s','sprint_observed_duration_s','allowed_sprint_stamina_drain')) {
                if($p.PSObject.Properties.Name -notcontains $field){throw ('Missing sprint guard evidence '+$field)}
            }
            $rate=[double]$p.sprint_cost_per_second;$interval=[double]$p.sprint_tick_interval_s;$duration=[double]$p.sprint_observed_duration_s
            foreach($value in @($rate,$interval,$duration,[double]$p.allowed_sprint_stamina_drain)) {
                if([double]::IsNaN($value)-or [double]::IsInfinity($value)){throw 'Non-finite sprint guard metadata.'}
            }
            if($rate -lt 0 -or $interval -le 0 -or $duration -lt 0 -or $p.last_accepted_skill -ne $p.accepted_skill_before){throw 'Invalid sprint guard or accepted skill changed.'}
            $staminaAllowance=$rate*($duration+$interval)
            if([Math]::Abs([double]$p.allowed_sprint_stamina_drain-$staminaAllowance) -gt 0.00001){throw 'Sprint allowance does not match reflected rate and observed duration plus one tick.'}
        }
        if($p.uses_after -ne $p.uses_before -or $p.total_uses_after -ne $p.total_uses_before -or $p.max_cooldown_s -gt 0.001 -or $p.saw_charge_held -ne $false -or
            $p.stamina_min -lt ($p.stamina_before-$staminaAllowance-0.001) -or $p.mana_min -lt ($p.mana_before-0.001) -or $p.identity_min -lt ($p.identity_before-0.001)) {
            throw ('Unexpected combat during guarded probe: '+$name)
        }
    }
    if(-not(Test-Path -LiteralPath $events)){throw 'NPC QA did not produce event observations.'}
    $eventRows=@((Read-BomText $events) | ConvertFrom-Csv)
    if($eventRows.Count -lt 1){throw 'NPC QA event observations are empty.'}
    $rhi=if(-not $Rendered){'NullRHI_requested'}elseif($engineText -match ('(?m)Log'+$Renderer+'RHI:')){$Renderer}else{'unverified'}
    if($Rendered -and $rhi -ne $Renderer){throw 'Rendered NPC QA lacks the requested RHI evidence.'}
    if($Rendered) {
        $expected=@('01_f_prompt.png','02_f_dialogue.png','03_f_collision_probe.png')
        foreach($name in $expected) {
            $path=Join-Path $images $name
            if(-not(Test-Path -LiteralPath $path) -or (Get-Item -LiteralPath $path).Length -le 1024){throw ('Missing actual viewport PNG: '+$name)}
            $stream=[IO.File]::OpenRead($path)
            try {$signature=New-Object byte[] 8;[void]$stream.Read($signature,0,8)}finally{$stream.Dispose()}
            if([BitConverter]::ToString($signature) -ne '89-50-4E-47-0D-0A-1A-0A'){throw ('Invalid PNG signature: '+$name)}
        }
        if(@(Get-ChildItem -LiteralPath $images -File -Filter '*.png').Count -ne $expected.Count){throw 'Unexpected NPC viewport screenshot count.'}
    }
    [pscustomobject]@{Label=$Label;Rendered=[bool]$Rendered;Offscreen=[bool]$Offscreen;RhiObserved=$rhi;
        Input='programmatic_Controller_InputKey_actual_legacy_and_enhanced_stack';PhysicalInput=$false;ManualFieldRun=$false;
        SaveMonitoring='all_preexisting_sav_SHA256_before_and_after';EventRows=$eventRows.Count;
        FBindingAccepted=[bool]$data.f_binding_accepted;CollisionDetected=[bool]$data.collision_detected} |
        ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $output 'run-envelope.json') -Encoding utf8
    Write-Output 'Actual input-stack/UMG programmatic QA only; viewport pictures require visual review. No physical-input or field-completion claim.'
} catch {$failure=$_} finally {
    # Never stop processes by name or touch another editor/test.
    if($null -ne $process -and -not $process.HasExited){$process.Kill();$process.WaitForExit(5000)|Out-Null}
    try {
        $after=@(Get-SaveInventory $saveDirectory)
        $after | Export-Csv -LiteralPath $afterCsv -NoTypeInformation -Encoding utf8
        $lookup=@{};foreach($entry in $after){$lookup[$entry.Path]=$entry.SHA256}
        $changed=@($before | Where-Object {-not $lookup.ContainsKey($_.Path) -or $lookup[$_.Path] -ne $_.SHA256})
        if($changed.Count -gt 0){throw ('Preexisting save preservation failed for '+$changed.Count+' path(s).')}
        $original=@{};foreach($entry in $before){$original[$entry.Path]=$true}
        $unexpected=@($after | Where-Object {-not $original.ContainsKey($_.Path) -and
            [IO.Path]::GetFileName($_.Path) -ne ($town+'.sav') -and [IO.Path]::GetFileName($_.Path) -ne ($keys+'.sav') -and
            [IO.Path]::GetFileName($_.Path) -notlike ($prefix+'_*.sav')})
        if($unexpected.Count -gt 0){throw ('Unexpected save paths outside the exact FpNpc namespace: '+$unexpected.Count)}
        Write-Output ('Preexisting saves preserved='+$before.Count+'; SHA256 checked before/after. New slots restricted to this FpNpc label.')
    } catch {$preservationFailure=$_}
}
if($null -ne $preservationFailure){throw $preservationFailure}
if($null -ne $failure){throw $failure}
Write-Output ('RESULT=NPC_INTERACTION_PASSED rendered='+$Rendered+' offscreen='+$Offscreen+' evidence='+$output+' images='+$images)
