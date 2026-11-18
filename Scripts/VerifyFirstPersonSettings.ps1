param(
    [Parameter(Mandatory=$true)][ValidatePattern('^[A-Za-z0-9_]{1,24}$')][string]$Label,
    [switch]$Rendered,
    [switch]$VisibleWindow,
    [switch]$InputTrace,
    [switch]$FocusProbe,
    [switch]$Offscreen,
    [ValidateSet('D3D11','D3D12')][string]$Renderer='D3D11',
    [ValidateSet(30,60)][int]$MaxFPS=60,
    [ValidateRange(60,260)][int]$TimeoutSeconds=260,
    [string]$ProjectRoot=(Split-Path -Parent $PSScriptRoot)
)
$ErrorActionPreference='Stop'
function Read-BomText([string]$Path) {
    # Unreal SaveStringToFile can emit UTF-16 BOM; decode before parsing CSV or results.
    $reader=[IO.StreamReader]::new($Path,[Text.Encoding]::UTF8,$true)
    try { return $reader.ReadToEnd() } finally { $reader.Dispose() }
}
function Get-SaveInventory([string]$Directory) {
    if(-not(Test-Path -LiteralPath $Directory)){return @()}
    return @(Get-ChildItem -LiteralPath $Directory -Filter '*.sav' -File -Recurse | Sort-Object FullName | ForEach-Object {
        [pscustomobject]@{Path=$_.FullName;SHA256=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash}
    })
}
function Assert-OriginalSaves([object[]]$Before,[object[]]$Current) {
    $lookup=@{};foreach($entry in $Current){$lookup[$entry.Path]=$entry.SHA256}
    $changed=@($Before | Where-Object {-not $lookup.ContainsKey($_.Path) -or $lookup[$_.Path] -ne $_.SHA256})
    if($changed.Count -gt 0){throw ('Original save preservation failed for '+$changed.Count+' path(s).')}
}
if($VisibleWindow -and -not $Rendered){throw 'VisibleWindow requires Rendered.'}
if($Offscreen -and (-not $Rendered -or $VisibleWindow)){throw 'Offscreen requires Rendered and is mutually exclusive with VisibleWindow.'}
if($FocusProbe -and (-not $InputTrace -or -not $Rendered -or $Offscreen)){throw 'FocusProbe requires InputTrace and native Rendered mode (not Offscreen).'}
$ProjectRoot=(Resolve-Path -LiteralPath $ProjectRoot).Path
$profile='Rogue10m_Town_QA_FpUi_'+$Label
$prefix='Rogue10m_Progression_QA_FpUi_'+$Label
$keys='Rogue10m_Fighter_QA_FpUi_'+$Label
$saveDirectory=Join-Path $ProjectRoot 'Saved\SaveGames'
$evidence=Join-Path $ProjectRoot 'Feature\doc\evidence\first-person-settings-20261029'
$output=Join-Path $evidence $Label
$images=Join-Path $ProjectRoot ('DevLog\20261029_FirstPersonSettings_'+$Label)
$log=Join-Path $evidence ($Label+'.log')
$beforeCsv=Join-Path $evidence ($Label+'.saves-before.csv')
$afterCsv=Join-Path $evidence ($Label+'.saves-after.csv')
if((Test-Path -LiteralPath $output) -or (Test-Path -LiteralPath $images) -or (Test-Path -LiteralPath $log) -or
    (Test-Path -LiteralPath $beforeCsv) -or (Test-Path -LiteralPath $afterCsv) -or
    (Test-Path -LiteralPath (Join-Path $saveDirectory ($profile+'.sav'))) -or
    (Test-Path -LiteralPath (Join-Path $saveDirectory ($keys+'.sav'))) -or
    (Get-ChildItem -LiteralPath $saveDirectory -Filter ($prefix+'_*') -File -ErrorAction SilentlyContinue | Select-Object -First 1)) {
    throw 'Preserve existing QA saves/evidence; choose a fresh FpUi label.'
}
$exe='D:\Program Files\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$window='Hidden'
if($Rendered){$exe='D:\Program Files\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'}
if($VisibleWindow){$window='Normal'}
$project=Join-Path $ProjectRoot 'Rogue10m.uproject'
if(-not(Test-Path -LiteralPath $exe) -or -not(Test-Path -LiteralPath $project)){throw 'Expected UE 5.8 executable or project file is missing.'}
$before=@(Get-SaveInventory $saveDirectory)
New-Item -ItemType Directory -Force -Path $evidence | Out-Null
$before | Export-Csv -LiteralPath $beforeCsv -NoTypeInformation -Encoding utf8
$arguments=@(('"'+$project+'"'),'/Game/Rogue10m/Maps/StartHub','-game','-unattended','-nosplash','-nosound',
    '-NoLiveCoding','-NoRemoteShaderCompile',('-RogueProfileSaveSlot='+$profile),('-RogueProgressionSavePrefix='+$prefix),
    ('-RogueSkillKeySaveSlot='+$keys),('-RogueFirstPersonSettingsLabel='+$Label),
    ('-ExecCmds="t.MaxFPS '+$MaxFPS+',Rogue10m.TestFirstPersonSettings"'),('-abslog="'+$log+'"'))
if($Rendered){$arguments+=@(('-'+$Renderer.ToLowerInvariant()),'-windowed','-ForceRes','-ResX=1280','-ResY=720')}else{$arguments+='-nullrhi'}
if($InputTrace){$arguments+='-RogueFirstPersonSettingsInputTrace'}
if($FocusProbe){$arguments+='-RogueFirstPersonSettingsFocusProbe'}
if($Offscreen){$arguments+='-RenderOffScreen'}
$process=$null
$failure=$null
$preservationFailure=$null
try {
    $process=Start-Process -FilePath $exe -ArgumentList $arguments -WindowStyle $window -PassThru
    Write-Output ('Owned first-person settings QA PID='+$process.Id+' rendered='+$Rendered+' renderer='+$Renderer+' max_fps='+$MaxFPS+' input=programmatic_InputKey_and_actual_UMG_delegates log='+$log)
    $timer=[Diagnostics.Stopwatch]::StartNew()
    $nextHashCheck=10.0
    while(-not $process.WaitForExit(1000)) {
        if($timer.Elapsed.TotalSeconds -ge $TimeoutSeconds){throw 'First-person settings QA timed out.'}
        if($timer.Elapsed.TotalSeconds -ge $nextHashCheck){Assert-OriginalSaves $before @(Get-SaveInventory $saveDirectory);$nextHashCheck+=10.0}
    }
    $process.Refresh()
    $result=Join-Path $output 'results.txt'
    $frames=Join-Path $output 'frames.csv'
    $metrics=Join-Path $output 'metrics.csv'
    if($process.ExitCode -ne 0 -or -not(Test-Path -LiteralPath $result) -or -not(Test-Path -LiteralPath $log) -or
        -not((Read-BomText $log).Contains('RESULT=FIRST_PERSON_SETTINGS_PASSED'))){throw ('First-person settings QA failed; exit='+$process.ExitCode+' log='+$log)}
    $resultText=Read-BomText $result
    $renderedValue=if($Rendered){1}else{0}
    if($resultText -notmatch '(?m)^failures=0\s*$' -or $resultText -notmatch ('(?m)^rendered='+$renderedValue+'\s*$') -or
        -not $resultText.Contains('input=programmatic_InputKey_and_actual_UMG_delegates') -or
        -not $resultText.Contains('performance_assertions=none') -or -not(Test-Path -LiteralPath $frames) -or -not(Test-Path -LiteralPath $metrics)) {throw 'Results do not confirm the requested QA mode or evidence.'}
    $offscreenValue=if($Offscreen){1}else{0}
    $traceValue=if($InputTrace){1}else{0}
    $probeValue=if($FocusProbe){1}else{0}
    if($resultText -notmatch ('(?m)^offscreen='+$offscreenValue+'\s*$') -or
        $resultText -notmatch ('(?m)^input_trace='+$traceValue+'\s*$') -or
        $resultText -notmatch ('(?m)^focus_probe='+$probeValue+'\s*$')){throw 'Result mode differs from requested offscreen/input-trace/focus-probe fixture.'}
    if($InputTrace){
        $trace=Join-Path $output 'input-trace.csv'
        $stacks=Join-Path $output 'input-flush-stacks.txt'
        if(-not(Test-Path -LiteralPath $trace) -or -not(Test-Path -LiteralPath $stacks) -or
            $resultText -notmatch '(?m)^input_trace_dropped=0\s*$' -or
            $resultText -notmatch '(?m)^input_trace_measured_flushes=0\s*$' -or
            $resultText -notmatch '(?m)^input_trace_measured_viewport_key_events=0\s*$' -or
            $resultText -notmatch '(?m)^input_trace_measured_control_rotation_mismatches=0\s*$' -or
            $resultText -notmatch '(?m)^input_trace_flush_hook_available=1\s*$'){throw 'Input-trace evidence is missing, truncated, or records contaminated measured input.'}
        $traceRows=@((Read-BomText $trace)|ConvertFrom-Csv)
        if($traceRows.Count -eq 0 -or @($traceRows|Where-Object origin -eq 'qa_direct_before').Count -eq 0){throw 'Input trace does not contain the actual QA-direct key path.'}
        if($FocusProbe -and @($traceRows|Where-Object origin -eq 'flush_controlled_focus_probe').Count -ne 1){throw 'Controlled viewport-focus probe did not capture its exact input flush.'}
    }
    $frameRows=@((Read-BomText $frames) | ConvertFrom-Csv)
    $metricRows=@((Read-BomText $metrics) | ConvertFrom-Csv)
    if($frameRows.Count -eq 0 -or $metricRows.Count -ne 4){throw 'Frame observations or four ABBA metric rows are missing.'}
    $expectedProfiles=@('OFF','ON','ON','OFF')
    for($index=0;$index -lt 4;$index++) {
        $row=$metricRows[$index]
        if([int]$row.segment -ne $index -or $row.profile -ne $expectedProfiles[$index] -or [int]$row.samples -lt 1 -or
            @($frameRows | Where-Object {$_.segment -eq [string]$index -and $_.stage -eq 'measured_sprint'}).Count -lt 1){throw ('Missing ABBA observations for segment '+$index)}
    }
    if($Rendered){
        $expected=@('01_settings_off.png','02_settings_on.png','03_settings_intensity_zero.png','04_settings_intensity_point4.png',
            '05_settings_original_restored.png','06_settings_abandon_modal.png','07_original_sprint.png','08_weighted_sprint.png')
        foreach($name in $expected){
            $path=Join-Path $images $name
            if(-not(Test-Path -LiteralPath $path) -or (Get-Item -LiteralPath $path).Length -le 1024){throw ('Missing viewport screenshot: '+$name)}
            $stream=[IO.File]::OpenRead($path)
            try{$signature=New-Object byte[] 8;[void]$stream.Read($signature,0,8)}finally{$stream.Dispose()}
            if([BitConverter]::ToString($signature) -ne '89-50-4E-47-0D-0A-1A-0A'){throw ('Screenshot is not a PNG: '+$name)}
        }
        if(@(Get-ChildItem -LiteralPath $images -File -Filter '*.png').Count -ne $expected.Count){throw 'Unexpected rendered screenshot count.'}
    }
    Write-Output $resultText
    Write-Output 'Warmed locomotion observations only; no performance threshold, improvement, cold-first-attack, physical-input or manual-completion claim.'
    $metricRows | Format-Table -AutoSize | Out-String | Write-Output
} catch {$failure=$_} finally {
    # Terminate only the exact Process created by this invocation, never editor processes by name.
    if($null -ne $process -and -not $process.HasExited){$process.Kill();$process.WaitForExit(5000)|Out-Null}
    try {
        $after=@(Get-SaveInventory $saveDirectory)
        $after | Export-Csv -LiteralPath $afterCsv -NoTypeInformation -Encoding utf8
        Assert-OriginalSaves $before $after
        $beforePaths=@{};foreach($entry in $before){$beforePaths[$entry.Path]=$true}
        $unexpected=@($after | Where-Object {
            -not $beforePaths.ContainsKey($_.Path) -and
            [IO.Path]::GetFileName($_.Path) -ne ($profile+'.sav') -and [IO.Path]::GetFileName($_.Path) -ne ($keys+'.sav') -and
            [IO.Path]::GetFileName($_.Path) -notlike ($prefix+'_*.sav')
        })
        if($unexpected.Count -gt 0){throw ('Unexpected save slots created outside the exact FpUi namespace: '+$unexpected.Count)}
        Write-Output ('Original saves preserved='+$before.Count+'; hashes checked every 10s and after exit, including all prior QA saves. New saves restricted to this FpUi label.')
    }catch{$preservationFailure=$_}
}
if($null -ne $preservationFailure){throw $preservationFailure}
if($null -ne $failure){throw $failure}
Write-Output ('RESULT=FIRST_PERSON_SETTINGS_PASSED rendered='+$Rendered+' evidence='+$output+' images='+$images)
