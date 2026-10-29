param(
    [ValidatePattern('^[A-Za-z0-9_]{1,24}$')][string]$Label,
    [ValidateSet('Normal','SaveFailure')][string]$Mode='Normal',
    [switch]$Rendered,
    [ValidateSet('D3D11','D3D12')][string]$Renderer='D3D11',
    [ValidateRange(60,600)][int]$TimeoutSeconds=270,
    [string]$ProjectRoot=(Split-Path -Parent $PSScriptRoot)
)
$ErrorActionPreference='Stop'
if([string]::IsNullOrEmpty($Label)){throw 'A fresh explicit Label is required.'}
$profile='Rogue10m_Town_QA_Menu_'+$Label
$prefix='Rogue10m_Progression_QA_Menu_'+$Label
$keys='Rogue10m_Fighter_QA_Menu_'+$Label
$saveDirectory=Join-Path $ProjectRoot 'Saved\SaveGames'
$evidence=Join-Path $ProjectRoot 'Feature\doc\evidence\menu-abandon-20261029'
$output=Join-Path $evidence $Label
$images=Join-Path $ProjectRoot ('DevLog\20261029_MenuAbandon_'+$Label)
$log=Join-Path $evidence ($Label+'.log')
if((Test-Path -LiteralPath $output) -or (Test-Path -LiteralPath $images) -or (Test-Path -LiteralPath $log) -or
    (Test-Path -LiteralPath (Join-Path $saveDirectory ($profile+'.sav'))) -or
    (Test-Path -LiteralPath (Join-Path $saveDirectory ($keys+'.sav'))) -or
    (Get-ChildItem -LiteralPath $saveDirectory -Filter ($prefix+'_*') -ErrorAction SilentlyContinue|Select-Object -First 1)){
    throw 'Preserve existing QA evidence and saves; choose a new label.'
}
New-Item -ItemType Directory -Force -Path $evidence|Out-Null
$arguments=@(('"'+(Join-Path $ProjectRoot 'Rogue10m.uproject')+'"'),'/Game/Rogue10m/Maps/StartHub',
    '-game','-unattended','-nosplash','-nosound','-NoLiveCoding','-NoRemoteShaderCompile',
    ('-RogueProfileSaveSlot='+$profile),('-RogueProgressionSavePrefix='+$prefix),('-RogueSkillKeySaveSlot='+$keys),
    ('-RogueMenuAbandonLabel='+$Label),('-RogueMenuAbandonMode='+$Mode),
    '-ExecCmds="t.MaxFPS 60,Rogue10m.TestMenuAbandon"',('-abslog="'+$log+'"'))
$exe='D:\Program Files\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$window='Hidden'
if($Rendered){
    $exe='D:\Program Files\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'; $window='Normal'
    $arguments+=@(('-'+$Renderer.ToLowerInvariant()),'-windowed','-ForceRes','-ResX=1280','-ResY=720')
}else{$arguments+='-nullrhi'}
$process=$null
try{
    $process=Start-Process -FilePath $exe -ArgumentList $arguments -WindowStyle $window -PassThru
    Write-Output ('Owned menu abandonment QA PID='+$process.Id+' log='+$log)
    if(-not $process.WaitForExit($TimeoutSeconds*1000)){throw 'Menu abandonment QA timed out.'}
    $result=Join-Path $output 'results.txt'
    if($process.ExitCode -ne 0 -or -not(Test-Path -LiteralPath $result) -or
        -not([IO.File]::ReadAllText($log).Contains('RESULT=MENU_ABANDON_PASSED'))){throw ('Menu abandonment QA failed; exit='+$process.ExitCode+' log='+$log)}
    $resultText=[IO.File]::ReadAllText($result)
    if(-not $resultText.Contains('failures=0') -or -not $resultText.Contains('mode='+$Mode)){throw 'Result file does not confirm requested mode.'}
    if($Rendered -and @(Get-ChildItem -LiteralPath $images -File -Filter '*.png').Count -ne 6){throw 'Rendered screenshot evidence incomplete.'}
    Write-Output $resultText
    Write-Output ('RESULT=MENU_ABANDON_PASSED mode='+$Mode+' rendered='+$Rendered)
}finally{
    if($null -ne $process -and -not $process.HasExited){Stop-Process -Id $process.Id; $process.WaitForExit(5000)|Out-Null}
}
