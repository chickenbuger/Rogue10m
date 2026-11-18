param(
    [Parameter(Mandatory=$true)][ValidatePattern('^[A-Za-z0-9_]{1,24}$')][string]$Label,
    [ValidateSet('Normal','Weighted')][string]$Mode='Normal',
    [ValidateSet(1280,1920)][int]$Width=1280,
    [ValidateSet(720,1080)][int]$Height=720,
    [ValidateSet('D3D11','D3D12')][string]$Renderer='D3D11',
    [switch]$VisibleWindow,
    [ValidateRange(40,180)][int]$TimeoutSeconds=100,
    [string]$ProjectRoot=(Split-Path -Parent $PSScriptRoot),
    [string]$EngineRoot='D:\Program Files\UE_5.8'
)
$ErrorActionPreference='Stop'
if(-not (($Width -eq 1280 -and $Height -eq 720) -or ($Width -eq 1920 -and $Height -eq 1080))){throw 'Use1280x720 or1920x1080.'}
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
$profile='Rogue10m_Town_QA_Crosshair_'+$Label
$prefix='Rogue10m_Progression_QA_Crosshair_'+$Label
$keys='Rogue10m_Fighter_QA_Crosshair_'+$Label
$saveDirectory=Join-Path $ProjectRoot 'Saved\SaveGames'
$evidence=Join-Path $ProjectRoot 'Feature\doc\evidence\camera-crosshair-20261103'
$output=Join-Path $evidence $Label
$images=Join-Path $ProjectRoot ('DevLog\20261103_CameraCrosshair_'+$Label)
$log=Join-Path $evidence ($Label+'.log')
$beforeCsv=Join-Path $evidence ($Label+'.saves-before.csv')
$afterCsv=Join-Path $evidence ($Label+'.saves-after.csv')
foreach($candidate in @($output,$images,$log,$beforeCsv,$afterCsv,
    (Join-Path $saveDirectory ($profile+'.sav')),(Join-Path $saveDirectory ($keys+'.sav')))) {
    if(Test-Path -LiteralPath $candidate){throw ('Preserve existing QA saves/evidence; choose a fresh label. Existing: '+$candidate)}
}
if(Get-ChildItem -LiteralPath $saveDirectory -Filter ($prefix+'_*') -File -ErrorAction SilentlyContinue | Select-Object -First 1){throw 'Fresh progression namespace required.'}
$exe=Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor.exe'
$project=Join-Path $ProjectRoot 'Rogue10m.uproject'
if(-not(Test-Path -LiteralPath $exe) -or -not(Test-Path -LiteralPath $project)){throw 'UE executable or project file missing.'}
# Never start a second gameplay process against the same project/save directory.
$otherGame=@(Get-CimInstance Win32_Process | Where-Object {
    $_.Name -like 'UnrealEditor*' -and $_.CommandLine -and
    $_.CommandLine.Contains($project) -and $_.CommandLine -match '(?i)(?:^|\s)-game(?:\s|$)'
})
if($otherGame.Count){throw 'A gameplay process for this project is already active; finish it before QA.'}
$before=@(Get-SaveInventory $saveDirectory)
New-Item -ItemType Directory -Force -Path $evidence | Out-Null
$before | Export-Csv -LiteralPath $beforeCsv -NoTypeInformation -Encoding utf8
$arguments=@(('"'+$project+'"'),'/Game/Rogue10m/Maps/StartHub','-game','-unattended','-nosplash','-nosound',
    '-NoLiveCoding','-NoRemoteShaderCompile','-windowed','-ForceRes',('-ResX='+$Width),('-ResY='+$Height),
    ('-'+$Renderer.ToLowerInvariant()),('-RogueProfileSaveSlot='+$profile),('-RogueProgressionSavePrefix='+$prefix),
    ('-RogueSkillKeySaveSlot='+$keys),('-RogueCameraCrosshairLabel='+$Label),
    '-ExecCmds="t.MaxFPS 60,Rogue10m.TestCameraCrosshair"',('-abslog="'+$log+'"'))
if($Mode -eq 'Weighted'){$arguments+='-RogueCameraCrosshairWeighted'}
$window='Hidden';if($VisibleWindow){$window='Normal'}
$process=$null;$failure=$null;$preservationFailure=$null
try {
    $process=Start-Process -FilePath $exe -ArgumentList $arguments -WindowStyle $window -PassThru
    Write-Output ('Owned camera/crosshair QA PID='+$process.Id+' mode='+$Mode+' resolution='+$Width+'x'+$Height+' log='+$log)
    $timer=[Diagnostics.Stopwatch]::StartNew()
    while(-not $process.WaitForExit(1000)) {
        if($timer.Elapsed.TotalSeconds -ge $TimeoutSeconds){throw 'Camera/crosshair QA wall timeout.'}
    }
    $process.Refresh()
    $result=Join-Path $output 'results.txt'
    $observations=Join-Path $output 'observations.csv'
    if($process.ExitCode -ne 0 -or -not(Test-Path -LiteralPath $result) -or -not(Test-Path -LiteralPath $observations) -or
        -not(Test-Path -LiteralPath $log) -or -not((Read-BomText $log).Contains('RESULT=CAMERA_CROSSHAIR_PASSED'))){throw ('Runtime QA failed; exit='+$process.ExitCode+' log='+$log)}
    $resultText=Read-BomText $result
    $weightedValue=if($Mode -eq 'Weighted'){1}else{0}
    foreach($expected in @('failures=0','rendered=1',('weighted='+$weightedValue),('width='+$Width),('height='+$Height),
        'stat_and_HP_fixtures=none','visual_pixels=external_PNG_review_required',
        'fixture=isolated_StartHub_HumanMale_profile_neutral_control_before_selected_pawn_spawn')) {
        if($resultText -notmatch ('(?m)^'+[Regex]::Escape($expected)+'\s*$')){throw ('Missing or mismatched result field: '+$expected)}
    }
    $rows=@((Read-BomText $observations) | ConvertFrom-Csv)
    $expectedNames=@('01_initial_first_person.png','02_after_ordinary_look.png','03_inventory_hidden_reticle.png','04_inspection_hidden_reticle.png','05_returned_first_person.png')
    $expectedVisible=@(1,1,0,0,1)
    if($rows.Count -ne $expectedNames.Count){throw 'Expected five observations and screenshots.'}
    Add-Type -AssemblyName System.Drawing
    for($index=0;$index -lt $expectedNames.Count;$index++) {
        $row=$rows[$index]
        if($row.screenshot -ne $expectedNames[$index] -or [int]$row.reticle_visible -ne $expectedVisible[$index] -or
            [int]$row.width -ne $Width -or [int]$row.height -ne $Height -or [int]$row.weighted -ne $weightedValue){throw 'Screenshot observation mode/state mismatch.'}
        $png=Join-Path $images $expectedNames[$index]
        if(-not(Test-Path -LiteralPath $png)){throw ('Missing actual rendered PNG: '+$png)}
        $bitmap=[Drawing.Image]::FromFile($png)
        try {if($bitmap.Width -ne $Width -or $bitmap.Height -ne $Height){throw ('Rendered PNG size mismatch: '+$png)}} finally {$bitmap.Dispose()}
    }
    Write-Output ('Runtime data checks passed. NativePaint pixels still require external review. Images='+$images)
} catch {$failure=$_} finally {
    if($process -and -not $process.HasExited){
        # Kill only the exact child owned by this launcher, never another editor or gameplay process.
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
