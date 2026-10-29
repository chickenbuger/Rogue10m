param(
    [Parameter(Mandatory=$true)][ValidatePattern('^[A-Za-z0-9_]{1,24}$')][string]$Label,
    [switch]$Rendered,
    [ValidateRange(0.5,3.0)][float]$AttackSpeed=1.0,
    [ValidateSet(30,60)][int]$MaxFPS=60,
    [ValidateRange(60,600)][int]$TimeoutSeconds=270,
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
$ProjectRoot=(Resolve-Path -LiteralPath $ProjectRoot).Path
$profile='Rogue10m_Town_QA_Heavy_'+$Label
$prefix='Rogue10m_Progression_QA_Heavy_'+$Label
$keys='Rogue10m_Fighter_QA_Heavy_'+$Label
$saveDirectory=Join-Path $ProjectRoot 'Saved\SaveGames'
$evidence=Join-Path $ProjectRoot 'Feature\doc\evidence\weighted-first-person-20261029'
$output=Join-Path $evidence $Label
$images=Join-Path $ProjectRoot ('DevLog\20261029_WeightedFirstPerson_'+$Label)
$log=Join-Path $evidence ($Label+'.log')
$beforeCsv=Join-Path $evidence ($Label+'.saves-before.csv')
$afterCsv=Join-Path $evidence ($Label+'.saves-after.csv')
if((Test-Path -LiteralPath $output) -or (Test-Path -LiteralPath $images) -or (Test-Path -LiteralPath $log) -or
    (Test-Path -LiteralPath $beforeCsv) -or (Test-Path -LiteralPath $afterCsv) -or
    (Test-Path -LiteralPath (Join-Path $saveDirectory ($profile+'.sav'))) -or
    (Test-Path -LiteralPath (Join-Path $saveDirectory ($keys+'.sav'))) -or
    (Get-ChildItem -LiteralPath $saveDirectory -Filter ($prefix+'_*') -File -ErrorAction SilentlyContinue | Select-Object -First 1)) {
    throw 'Preserve existing QA saves/evidence; use a fresh Heavy label.'
}
$exe='D:\Program Files\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$window='Hidden'
if($Rendered){$exe='D:\Program Files\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe';$window='Normal'}
$project=Join-Path $ProjectRoot 'Rogue10m.uproject'
if(-not(Test-Path -LiteralPath $exe) -or -not(Test-Path -LiteralPath $project)){throw 'Expected UE 5.8 executable or project file is missing.'}
$before=@(Get-SaveInventory $saveDirectory)
New-Item -ItemType Directory -Force -Path $evidence | Out-Null
$before | Export-Csv -LiteralPath $beforeCsv -NoTypeInformation -Encoding utf8
$arguments=@(('"'+$project+'"'),'/Game/Rogue10m/Maps/StartHub','-game','-unattended','-nosplash','-nosound',
    '-NoLiveCoding','-NoRemoteShaderCompile',('-RogueProfileSaveSlot='+$profile),('-RogueProgressionSavePrefix='+$prefix),
    ('-RogueSkillKeySaveSlot='+$keys),('-RogueWeightedFPLabel='+$Label),
    ('-RogueWeightedFPAttackSpeed='+$AttackSpeed.ToString([Globalization.CultureInfo]::InvariantCulture)),
    ('-ExecCmds="t.MaxFPS '+$MaxFPS+',Rogue10m.TestWeightedFirstPerson"'),('-abslog="'+$log+'"'))
if($Rendered){$arguments+=@('-d3d11','-windowed','-ForceRes','-ResX=1280','-ResY=720')}else{$arguments+='-nullrhi'}
$process=$null
$failure=$null
try {
    $process=Start-Process -FilePath $exe -ArgumentList $arguments -WindowStyle $window -PassThru
    Write-Output ('Owned weighted QA PID='+$process.Id+' rendered='+$Rendered+' input=programmatic_InputKey_and_DoAim log='+$log)
    if(-not $process.WaitForExit($TimeoutSeconds*1000)){throw 'Weighted QA timed out.'}
    $result=Join-Path $output 'results.txt'
    $frames=Join-Path $output 'frames.csv'
    if($process.ExitCode -ne 0 -or -not(Test-Path -LiteralPath $result) -or -not(Test-Path -LiteralPath $log) -or
        -not((Read-BomText $log).Contains('RESULT=WEIGHTED_FIRST_PERSON_PASSED'))){throw ('Weighted QA failed; exit='+$process.ExitCode+' log='+$log)}
    $resultText=Read-BomText $result
    $renderedValue=if($Rendered){1}else{0}
    if($resultText -notmatch '(?m)^failures=0\s*$' -or $resultText -notmatch ('(?m)^rendered='+$renderedValue+'\s*$') -or
        -not $resultText.Contains('input=programmatic_InputKey_and_DoAim') -or -not(Test-Path -LiteralPath $frames)) {throw 'Results do not confirm the requested QA mode.'}
    if($Rendered){
        $expected=@('01_weighted_idle.png','02_weighted_walk.png','03_weighted_sprint.png','04_weighted_attack_recovered.png',
            '05_settings_boundary.png','06_inspection_profile_skipped.png','07_greatsword_boundary.png','08_original_profile_restored.png','09_actual_knuckle_attack.png')
        foreach($name in $expected){$path=Join-Path $images $name;if(-not(Test-Path -LiteralPath $path) -or (Get-Item -LiteralPath $path).Length -le 16){throw ('Missing viewport screenshot: '+$name)}}
    }
    Write-Output $resultText
} catch {$failure=$_} finally {
    # Only the Process returned by this invocation is owned. Never terminate editors by name.
    if($null -ne $process -and -not $process.HasExited){$process.Kill();$process.WaitForExit(5000)|Out-Null}
    $after=@(Get-SaveInventory $saveDirectory)
    $after | Export-Csv -LiteralPath $afterCsv -NoTypeInformation -Encoding utf8
    $lookup=@{};foreach($entry in $after){$lookup[$entry.Path]=$entry.SHA256}
    $changed=@($before | Where-Object {-not $lookup.ContainsKey($_.Path) -or $lookup[$_.Path] -ne $_.SHA256})
    if($changed.Count -gt 0){throw ('Original save preservation failed for '+$changed.Count+' path(s); inspect '+$beforeCsv+' and '+$afterCsv)}
    Write-Output ('Original saves preserved='+$before.Count+' (all existing QA saves included).')
}
if($null -ne $failure){throw $failure}
Write-Output ('RESULT=WEIGHTED_FIRST_PERSON_PASSED rendered='+$Rendered+' evidence='+$output+' images='+$images)
