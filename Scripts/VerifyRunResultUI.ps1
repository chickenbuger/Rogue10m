param(
    [ValidatePattern('^[A-Za-z0-9_]{1,24}$')][string]$Label,
    [ValidateSet('Normal','Failure','Zero','Cap','AbandonGain','AbandonSpend','Interrupted')][string]$Mode='Normal',
    [switch]$Rendered,
    [ValidateRange(60,600)][int]$TimeoutSeconds=240,
    [string]$ProjectRoot=(Split-Path -Parent $PSScriptRoot)
)
$ErrorActionPreference='Stop'
if([string]::IsNullOrEmpty($Label)){throw 'A fresh explicit Label is required.'}
$profile='Rogue10m_Town_QA_ResultUI_'+$Label
$prefix='Rogue10m_Progression_QA_ResultUI_'+$Label
$keys='Rogue10m_Fighter_QA_ResultUI_'+$Label
$saveDirectory=Join-Path $ProjectRoot 'Saved\SaveGames'
$evidence=Join-Path $ProjectRoot 'Feature\doc\evidence\run-result-ui-20261029'
$output=Join-Path $evidence $Label
$images=Join-Path $ProjectRoot ('DevLog\20261029_RunResultUI_'+$Label)
$log=Join-Path $evidence ($Label+'.log')
if((Test-Path -LiteralPath $output) -or (Test-Path -LiteralPath $images) -or (Test-Path -LiteralPath $log) -or
    (Test-Path -LiteralPath (Join-Path $saveDirectory ($profile+'.sav'))) -or
    (Test-Path -LiteralPath (Join-Path $saveDirectory ($keys+'.sav'))) -or
    (Get-ChildItem -LiteralPath $saveDirectory -Filter ($prefix+'_*') -ErrorAction SilentlyContinue|Select-Object -First 1)){
    throw 'Existing QA evidence or saves must be preserved; choose a new label.'
}
New-Item -ItemType Directory -Force -Path $evidence|Out-Null
$arguments=@(('"'+(Join-Path $ProjectRoot 'Rogue10m.uproject')+'"'),'/Game/Rogue10m/Maps/StartHub',
    '-game','-unattended','-nosplash','-nosound','-NoLiveCoding','-NoRemoteShaderCompile',
    ('-RogueProfileSaveSlot='+$profile),('-RogueProgressionSavePrefix='+$prefix),('-RogueSkillKeySaveSlot='+$keys),
    ('-RogueResultUILabel='+$Label),('-RogueResultUIMode='+$Mode),
    '-ExecCmds="t.MaxFPS 60,Rogue10m.TestRunResultUI"',('-abslog="'+$log+'"'))
$exe='D:\Program Files\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$window='Hidden'
if($Rendered){
    $exe='D:\Program Files\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'; $window='Normal'
    $arguments+=@('-d3d11','-windowed','-ForceRes','-ResX=1280','-ResY=720')
}else{$arguments+='-nullrhi'}
$process=$null
try{
    $process=Start-Process -FilePath $exe -ArgumentList $arguments -WindowStyle $window -PassThru
    Write-Output ('Owned run result UI QA PID='+$process.Id+' log='+$log)
    if(-not $process.WaitForExit($TimeoutSeconds*1000)){throw 'Run result UI QA timed out.'}
    $result=Join-Path $output 'results.txt'
    if($process.ExitCode -ne 0 -or -not(Test-Path -LiteralPath $result) -or
        -not([IO.File]::ReadAllText($log).Contains('RESULT=RUN_RESULT_UI_PASSED'))){throw ('Run result UI QA failed; exit='+$process.ExitCode+' log='+$log)}
    $resultText=[IO.File]::ReadAllText($result)
    if(-not $resultText.Contains('failures=0') -or -not $resultText.Contains('mode='+$Mode)){throw 'Result file does not confirm requested mode.'}
    $expectedPhotos=if($Mode -eq 'Interrupted'){2}elseif($Mode.StartsWith('Abandon')){4}else{3}
    if($Rendered -and @(Get-ChildItem -LiteralPath $images -File -Filter '*.png').Count -ne $expectedPhotos){throw 'Rendered screenshot evidence incomplete.'}
    Get-Content -LiteralPath $result -Encoding UTF8
    Write-Output ('RESULT=RUN_RESULT_UI_PASSED mode='+$Mode+' rendered='+$Rendered)
}finally{
    if($null -ne $process -and -not $process.HasExited){Stop-Process -Id $process.Id; $process.WaitForExit(5000)|Out-Null}
}
