param(
    [Parameter(Mandatory=$true)][ValidatePattern('^[A-Za-z0-9_]{1,24}$')][string]$Label,
    [Parameter(Mandatory=$true)][ValidateSet('Original','Weighted')][string]$Profile,
    [switch]$Rendered,
    [switch]$Offscreen,
    [switch]$VisibleWindow,
    [ValidateSet(30,60)][int]$MaxFPS=60,
    [ValidateRange(60,240)][int]$TimeoutSeconds=220,
    [string]$ProjectRoot=(Split-Path -Parent $PSScriptRoot)
)
$ErrorActionPreference='Stop'
function Read-BomText([string]$Path) {
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
$town='Rogue10m_Town_QA_FpAttack_'+$Label
$prefix='Rogue10m_Progression_QA_FpAttack_'+$Label
$keys='Rogue10m_Fighter_QA_FpAttack_'+$Label
$saveDirectory=Join-Path $ProjectRoot 'Saved\SaveGames'
$evidence=Join-Path $ProjectRoot 'Feature\doc\evidence\first-attack-frames-20261029'
$output=Join-Path $evidence $Label
$log=Join-Path $evidence ($Label+'.log')
$beforeCsv=Join-Path $evidence ($Label+'.saves-before.csv')
$afterCsv=Join-Path $evidence ($Label+'.saves-after.csv')
if((Test-Path -LiteralPath $output) -or (Test-Path -LiteralPath $log) -or (Test-Path -LiteralPath $beforeCsv) -or
    (Test-Path -LiteralPath $afterCsv) -or (Test-Path -LiteralPath (Join-Path $saveDirectory ($town+'.sav'))) -or
    (Test-Path -LiteralPath (Join-Path $saveDirectory ($keys+'.sav'))) -or
    (Get-ChildItem -LiteralPath $saveDirectory -Filter ($prefix+'_*') -File -ErrorAction SilentlyContinue | Select-Object -First 1)) {
    throw 'Preserve prior FpAttack saves/evidence; choose a fresh Label.'
}
$exe='D:\Program Files\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
if($Rendered){$exe='D:\Program Files\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'}
$window=if($VisibleWindow){'Normal'}else{'Hidden'}
$project=Join-Path $ProjectRoot 'Rogue10m.uproject'
if(-not(Test-Path -LiteralPath $exe) -or -not(Test-Path -LiteralPath $project)){throw 'Expected UE 5.8 executable or project file is missing.'}
$before=@(Get-SaveInventory $saveDirectory)
New-Item -ItemType Directory -Force -Path $evidence | Out-Null
$before | Export-Csv -LiteralPath $beforeCsv -NoTypeInformation -Encoding utf8
$arguments=@(('"'+$project+'"'),'/Game/Rogue10m/Maps/StartHub','-game','-unattended','-nosplash','-nosound',
    '-NoLiveCoding','-NoRemoteShaderCompile',('-RogueProfileSaveSlot='+$town),('-RogueProgressionSavePrefix='+$prefix),
    ('-RogueSkillKeySaveSlot='+$keys),('-RogueFirstAttackLabel='+$Label),('-RogueFirstAttackProfile='+$Profile),
    ('-ExecCmds="t.MaxFPS '+$MaxFPS+',Rogue10m.MeasureFirstAttackFrames"'),('-abslog="'+$log+'"'))
if($Rendered){$arguments+=@('-d3d11','-windowed','-ForceRes','-ResX=1280','-ResY=720')}else{$arguments+='-nullrhi'}
if($Offscreen){$arguments+='-RenderOffScreen'}
$process=$null
$failure=$null
$preservationFailure=$null
try {
    $process=Start-Process -FilePath $exe -ArgumentList $arguments -WindowStyle $window -PassThru
    Write-Output ('Owned first-attack frame probe PID='+$process.Id+' profile='+$Profile+' rendered='+$Rendered+' offscreen='+$Offscreen+' log='+$log)
    # No hash polling, screenshots or extra file writes while the attack probe runs.
    if(-not $process.WaitForExit($TimeoutSeconds*1000)){throw 'First-attack frame probe timed out.'}
    $process.Refresh()
    $result=Join-Path $output 'results.json'
    $frames=Join-Path $output 'frames.csv'
    if(-not(Test-Path -LiteralPath $result) -or -not(Test-Path -LiteralPath $frames) -or -not(Test-Path -LiteralPath $log)) {
        throw ('Probe did not produce complete results; exit='+$process.ExitCode+' log='+$log)
    }
    $resultText=Read-BomText $result
    $data=$resultText | ConvertFrom-Json
    Write-Output $resultText
    $logText=Read-BomText $log
    if([int]$data.probe_schema_version -ne 2 -or -not $data.viewport_input_observer_available -or
        $data.contamination_contract -ne 'official_flush_or_any_viewport_key_event_or_control_rotation_deviation_gt_0p01deg' -or
        $data.profile -ne $Profile -or [bool]$data.rendered -ne [bool]$Rendered -or [bool]$data.offscreen -ne [bool]$Offscreen -or
        $data.measurement_kind -ne 'fresh_process_first_use_and_same_skill_repeat_not_cleared_cache' -or
        $data.cache_cleared -or $data.physical_input_performed -or $data.manual_field_run -or
        -not $data.official_flush_hook_available -or @($data.trials).Count -ne 2) {throw 'Probe metadata/integrity does not match requested measurement.'}
    $observedRhi=if($Rendered -and $logText -match '(?m)LogD3D11RHI:'){'D3D11'}elseif(-not $Rendered){'NullRHI_requested'}else{'unverified'}
    if($Rendered -and $observedRhi -ne 'D3D11'){throw 'Rendered probe lacks D3D11 RHI evidence in its engine log.'}
    $rows=@((Read-BomText $frames) | ConvertFrom-Csv)
    if($rows.Count -eq 0 -or $rows.Count -ne [int]$data.row_count){throw 'Raw frame row count mismatch.'}
    foreach($index in @(0,1)) {
        $trial=$data.trials[$index]
        $required=@('viewport_key_events','viewport_non_simulated_key_events','viewport_lmb_release_events','viewport_event_count_start','viewport_event_count_end',
            'control_rotation_deviation_samples','max_control_rotation_deviation_deg','control_rotation_invalid','contamination_reasons',
            'complete_begin_frame_intervals','boundary_begin_frame_intervals','boundary_max_begin_frame_gap_ms')
        foreach($field in $required){if($trial.PSObject.Properties.Name -notcontains $field){throw ('Missing integrity field: '+$field)}}
        $expectedContamination=([double]$trial.flush_count_end -ne [double]$trial.flush_count_start) -or
            ([double]$trial.viewport_key_events -gt 0) -or ([int]$trial.control_rotation_deviation_samples -gt 0) -or [bool]$trial.control_rotation_invalid
        $viewportRows=@($rows | Where-Object {$_.trial -eq [string]$index -and $_.event -eq 'viewport_input'})
        $beginRows=@($rows | Where-Object {$_.trial -eq [string]$index -and $_.event -eq 'begin_frame'})
        $completeRows=@($beginRows | Where-Object {$_.begin_interval_fully_in_window -eq '1'})
        if([bool]$trial.input_contaminated -ne [bool]$expectedContamination -or $viewportRows.Count -ne [int]$trial.viewport_key_events -or
            [double]$trial.viewport_event_count_end - [double]$trial.viewport_event_count_start -ne [double]$trial.viewport_key_events -or
            $beginRows.Count -ne [int]$trial.begin_frame_samples -or $completeRows.Count -ne [int]$trial.complete_begin_frame_intervals -or
            $beginRows.Count - $completeRows.Count -ne [int]$trial.boundary_begin_frame_intervals -or $completeRows.Count -lt 1){throw ('Integrity counters do not match raw observations: '+$index)}
        if([int]$trial.trial -ne $index -or -not $trial.complete -or [int]$trial.uses_after -ne [int]$trial.uses_before+1 -or
            [double]$trial.target_damage -le 0 -or [int]$trial.montage_samples -lt 2 -or -not $trial.montage_rate_correct -or
            @($rows | Where-Object {$_.trial -eq [string]$index -and $_.event -eq 'begin_frame'}).Count -eq 0 -or
            @($rows | Where-Object {$_.trial -eq [string]$index -and $_.event -eq 'world_post'}).Count -eq 0) {throw ('Attack trial incomplete/functional validation failed: '+$index)}
    }
    $envelope=[pscustomobject]@{Label=$Label;Profile=$Profile;Rendered=[bool]$Rendered;Offscreen=[bool]$Offscreen;MaxFPS=$MaxFPS;
        RhiObservedFromEngineLog=$observedRhi;Input='programmatic_Controller_InputKey';CacheCleared=$false;PhysicalInput=$false;
        SaveHashMonitoring='before_and_after_only_no_attack_window_polling';NoPerformanceThreshold=$true}
    $envelope | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $output 'run-envelope.json') -Encoding utf8
    if([bool]$data.input_contaminated -ne [bool](@($data.trials | Where-Object {$_.input_contaminated}).Count -gt 0)){throw 'Aggregate contamination status does not match trials.'}
    if($data.input_contaminated){throw 'Attack observation is input-contaminated (official flush, viewport key event, or control rotation deviation); no blocking/correction/healing/reinjection was applied.'}
    if($process.ExitCode -ne 0 -or -not $data.probe_passed -or -not $data.functional_passed -or [int]$data.failures -ne 0 -or
        -not $logText.Contains('RESULT=FIRST_ATTACK_FRAMES_PASSED')) {throw ('First-attack frame probe failed; exit='+$process.ExitCode+' log='+$log)}
    Write-Output 'Frame gaps are raw observations, not a performance improvement or a cleared-cache test. No physical input/manual run claim.'
} catch {$failure=$_} finally {
    # Only this exact owned Process may be terminated, never editors by name.
    if($null -ne $process -and -not $process.HasExited){$process.Kill();$process.WaitForExit(5000)|Out-Null}
    try {
        $after=@(Get-SaveInventory $saveDirectory)
        $after | Export-Csv -LiteralPath $afterCsv -NoTypeInformation -Encoding utf8
        $lookup=@{};foreach($entry in $after){$lookup[$entry.Path]=$entry.SHA256}
        $changed=@($before | Where-Object {-not $lookup.ContainsKey($_.Path) -or $lookup[$_.Path] -ne $_.SHA256})
        if($changed.Count -gt 0){throw ('Original save preservation failed for '+$changed.Count+' path(s).')}
        $original=@{};foreach($entry in $before){$original[$entry.Path]=$true}
        $unexpected=@($after | Where-Object {-not $original.ContainsKey($_.Path) -and
            [IO.Path]::GetFileName($_.Path) -ne ($town+'.sav') -and [IO.Path]::GetFileName($_.Path) -ne ($keys+'.sav') -and
            [IO.Path]::GetFileName($_.Path) -notlike ($prefix+'_*.sav')})
        if($unexpected.Count -gt 0){throw ('Unexpected new save paths outside exact FpAttack namespace: '+$unexpected.Count)}
        Write-Output ('Original saves preserved='+$before.Count+'; hashes checked only before/after probe to avoid observation-window IO.')
    } catch {$preservationFailure=$_}
}
if($null -ne $preservationFailure){throw $preservationFailure}
if($null -ne $failure){throw $failure}
Write-Output ('RESULT=FIRST_ATTACK_FRAMES_PASSED profile='+$Profile+' rendered='+$Rendered+' offscreen='+$Offscreen+' evidence='+$output)
