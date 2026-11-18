param(
    [Parameter(Mandatory=$true)][ValidatePattern('^[A-Za-z0-9_]{1,24}$')][string]$Label,
    [switch]$Rendered,
    [switch]$VisibleWindow,
    [switch]$RequireLiveSkeletal,
    [ValidateSet('D3D11','D3D12')][string]$Renderer='D3D11',
    [ValidateRange(60,240)][int]$TimeoutSeconds=130,
    [string]$ProjectRoot=(Split-Path -Parent $PSScriptRoot),
    [string]$EngineRoot='D:\Program Files\UE_5.8'
)
$ErrorActionPreference='Stop'
if($VisibleWindow -and -not $Rendered){throw 'VisibleWindow requires Rendered.'}
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
function Assert-OriginalSaves([object[]]$Before,[object[]]$After) {
    $lookup=@{};foreach($entry in $After){$lookup[$entry.Path]=$entry.SHA256}
    $changed=@($Before | Where-Object {-not $lookup.ContainsKey($_.Path) -or $lookup[$_.Path] -ne $_.SHA256})
    if($changed.Count){throw ('Original save preservation failed for '+$changed.Count+' path(s).')}
}
$ProjectRoot=(Resolve-Path -LiteralPath $ProjectRoot).Path
$profile='Rogue10m_Town_QA_Spatial_'+$Label
$prefix='Rogue10m_Progression_QA_Spatial_'+$Label
$keys='Rogue10m_Fighter_QA_Spatial_'+$Label
$saveDirectory=Join-Path $ProjectRoot 'Saved\SaveGames'
$evidence=Join-Path $ProjectRoot 'Feature\doc\evidence\combat-spatial-debug-20261103'
$output=Join-Path $evidence $Label
$images=Join-Path $ProjectRoot ('DevLog\20261103_SpatialDebug_'+$Label)
$log=Join-Path $evidence ($Label+'.log')
$beforeCsv=Join-Path $evidence ($Label+'.saves-before.csv')
$afterCsv=Join-Path $evidence ($Label+'.saves-after.csv')
foreach($candidate in @($output,$images,$log,$beforeCsv,$afterCsv,
    (Join-Path $saveDirectory ($profile+'.sav')),(Join-Path $saveDirectory ($keys+'.sav')))) {
    if(Test-Path -LiteralPath $candidate){throw ('Preserve existing QA saves/evidence; choose a fresh label. Existing: '+$candidate)}
}
if(Get-ChildItem -LiteralPath $saveDirectory -Filter ($prefix+'_*') -File -ErrorAction SilentlyContinue | Select-Object -First 1){throw 'Fresh progression namespace required.'}
$exe=Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
if($Rendered){$exe=Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor.exe'}
$project=Join-Path $ProjectRoot 'Rogue10m.uproject'
if(-not(Test-Path -LiteralPath $exe) -or -not(Test-Path -LiteralPath $project)){throw 'UE executable or project file missing.'}
$otherGame=@(Get-CimInstance Win32_Process | Where-Object {
    $_.Name -like 'UnrealEditor*' -and $_.CommandLine -and
    $_.CommandLine.Contains($project) -and $_.CommandLine -match '(?i)(?:^|\s)-game(?:\s|$)'
})
if($otherGame.Count){throw 'A gameplay process for this project is active; finish it before QA.'}
$before=@(Get-SaveInventory $saveDirectory)
New-Item -ItemType Directory -Force -Path $evidence | Out-Null
$before | Export-Csv -LiteralPath $beforeCsv -NoTypeInformation -Encoding utf8
$arguments=@(('"'+$project+'"'),'/Game/Rogue10m/Maps/StartHub','-game','-unattended','-nosplash','-nosound',
    '-NoLiveCoding','-NoRemoteShaderCompile',('-RogueProfileSaveSlot='+$profile),('-RogueProgressionSavePrefix='+$prefix),
    ('-RogueSkillKeySaveSlot='+$keys),('-RogueSpatialDebugLabel='+$Label),
    '-ExecCmds="t.MaxFPS 60,Rogue10m.TestSpatialDebug"',('-abslog="'+$log+'"'))
if($Rendered){$arguments+=@(('-'+$Renderer.ToLowerInvariant()),'-windowed','-ForceRes','-ResX=1280','-ResY=720')}else{$arguments+='-nullrhi'}
$window='Hidden';if($VisibleWindow){$window='Normal'}
$process=$null;$failure=$null;$preservationFailure=$null
try {
    $process=Start-Process -FilePath $exe -ArgumentList $arguments -WindowStyle $window -PassThru
    Write-Output ('Owned spatial-debug QA PID='+$process.Id+' rendered='+$Rendered+' log='+$log)
    $timer=[Diagnostics.Stopwatch]::StartNew()
    while(-not $process.WaitForExit(1000)) {
        if($timer.Elapsed.TotalSeconds -ge $TimeoutSeconds){throw 'Spatial-debug QA wall timeout.'}
    }
    $process.Refresh()
    $result=Join-Path $output 'results.txt'
    $measurements=Join-Path $output 'measurements.csv'
    if($process.ExitCode -ne 0 -or -not(Test-Path -LiteralPath $result) -or -not(Test-Path -LiteralPath $measurements) -or
        -not(Test-Path -LiteralPath $log) -or -not((Read-BomText $log).Contains('RESULT=SPATIAL_DEBUG_PASSED'))){throw ('Runtime fixture failed; exit='+$process.ExitCode+' log='+$log)}
    $resultText=Read-BomText $result
    $renderedValue=if($Rendered){1}else{0}
    foreach($expected in @('failures=0',('rendered='+$renderedValue),'stat_and_HP_seeding=none',
        'entrypoint=real_StartAttackHitSequence_ExecuteAttackHitPulse_GatherAttackTargets_ApplyAttackDamage',
        'input_and_cost_acceptance=not_tested','physical_input=not_performed','visual_pixels=external_review_required')) {
        if($resultText -notmatch ('(?m)^'+[Regex]::Escape($expected)+'\s*$')){throw ('Missing/mismatched fixture result field: '+$expected)}
    }
    $liveExercised=($resultText -match '(?m)^live_skeletal_exercised=1\s*$')
    if($RequireLiveSkeletal -and -not $liveExercised){throw 'Native live skeletal shape transform path was unexercised; this run cannot satisfy RequireLiveSkeletal.'}
    $rows=@((Read-BomText $measurements) | ConvertFrom-Csv)
    $expectedNames=@('00_spawned_target_wide.png','01_live_colliders.png','02_linear_query_hit.png','02b_actual_query_inspection_reference.png','03_circle_query_hit.png',
        '04_arc_sphere_query_and_filter.png','05_projectile_segment.png','06_locked_reuse_no_new_query.png',
        '07_authored_reference_on.png','08_debug_disabled_paused.png','09_all_test_targets_cleared.png')
    if($rows.Count -ne $expectedNames.Count){throw 'Expected eleven completed evidence stages.'}
    for($index=0;$index -lt $expectedNames.Count;$index++) {
        if($rows[$index].stage -ne $expectedNames[$index]){throw 'Evidence stage sequence mismatch.'}
    }
    $byName=@{};foreach($row in $rows){$byName[$row.stage]=$row}
    $locked=$byName['06_locked_reuse_no_new_query.png'];$reference=$byName['07_authored_reference_on.png']
    $disabled=$byName['08_debug_disabled_paused.png'];$cleared=$byName['09_all_test_targets_cleared.png']
    if([int]$locked.locked_reuse -ne 1 -or [int]$locked.raw_components -ne 0 -or [int]$locked.accepted -lt 1 -or
        [int]$reference.reference_requested -ne 1 -or [int]$reference.viewport_collision -ne 1 -or
        [int]$disabled.owned_lines -ne 0 -or [int]$disabled.spatial -ne 0 -or [int]$disabled.reference_requested -ne 0 -or
        [int]$cleared.owned_lines -ne 0 -or [int]$cleared.spatial -ne 0){throw 'Locked reuse, viewport reference or paused cleanup measurements mismatch.'}
    if($Rendered){
        Add-Type -AssemblyName System.Drawing
        foreach($name in $expectedNames) {
            $png=Join-Path $images $name
            if(-not(Test-Path -LiteralPath $png)){throw ('Missing actual rendered PNG: '+$png)}
            $bitmap=[Drawing.Image]::FromFile($png)
            try {if($bitmap.Width -ne 1280 -or $bitmap.Height -ne 720){throw ('Rendered PNG size mismatch: '+$png)}} finally {$bitmap.Dispose()}
        }
    }
    Write-Output ('Native fixture checks passed. live_skeletal_exercised='+[int]$liveExercised+' rendered='+$renderedValue+' output='+$output)
    if(-not $liveExercised){Write-Output 'Live skeletal scene shape path UNEXERCISED on this selected asset; do not report it as verified.'}
    if($Rendered){Write-Output ('Actual PNGs require separate visual review: '+$images)}
} catch {$failure=$_} finally {
    if($process -and -not $process.HasExited){
        # Terminate through the original owned process handle; a recycled PID cannot select another process.
        $process.Kill()
        $null=$process.WaitForExit(10000)
    }
    try {
        $after=@(Get-SaveInventory $saveDirectory)
        $after | Export-Csv -LiteralPath $afterCsv -NoTypeInformation -Encoding utf8
        Assert-OriginalSaves $before $after
    } catch {$preservationFailure=$_}
}
if($preservationFailure){throw $preservationFailure}
if($failure){throw $failure}
