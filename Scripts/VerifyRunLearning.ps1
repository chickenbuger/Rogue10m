param(
    [ValidatePattern('^[A-Za-z0-9_]{1,24}$')][string]$Label,
    [ValidateSet('Normal','Abandon','Defeat')][string]$Mode='Normal',
    [switch]$Rendered,
    [ValidateRange(60,600)][int]$TimeoutSeconds=300,
    [string]$ProjectRoot=(Split-Path -Parent $PSScriptRoot)
)
# Uses three fresh QA-only save namespaces; never reuses an existing label.
$ErrorActionPreference='Stop'
if([string]::IsNullOrEmpty($Label)){throw 'A fresh explicit Label is required.'}
$profile='Rogue10m_Town_QA_Learning_'+$Label
$prefix='Rogue10m_Progression_QA_Learning_'+$Label
$keys='Rogue10m_Fighter_QA_Learning_'+$Label
$saveDirectory=Join-Path $ProjectRoot 'Saved\SaveGames'
$evidence=Join-Path $ProjectRoot 'Feature\doc\evidence\run-learning-20261101'
$output=Join-Path $evidence $Label
$images=Join-Path $ProjectRoot ('DevLog\20261101_RunLearning_'+$Label)
$log=Join-Path $evidence ($Label+'.log')
if(-not(Test-Path -LiteralPath (Join-Path $ProjectRoot 'Rogue10m.uproject'))){throw 'ProjectRoot must identify the reviewed Rogue10m checkout.'}
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
    ('-RogueRunLearningLabel='+$Label),('-RogueRunLearningMode='+$Mode),
    '-ExecCmds="t.MaxFPS 60,Rogue10m.TestRunLearning"',('-abslog="'+$log+'"'))
$exe='D:\Program Files\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
if($Rendered){
    $exe='D:\Program Files\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
    $arguments+=@('-d3d11','-RenderOffscreen','-windowed','-ForceRes','-ResX=1280','-ResY=720')
}else{$arguments+='-nullrhi'}
$process=$null
$ownedProcessStarted=$false
try{
    $process=Start-Process -FilePath $exe -ArgumentList $arguments -WindowStyle Hidden -PassThru
    $ownedProcessStarted=$true
    Write-Output ('Owned run learning QA PID='+$process.Id+' log='+$log)
    if(-not $process.WaitForExit($TimeoutSeconds*1000)){throw 'Run learning QA timed out.'}
    $result=Join-Path $output 'results.txt'
    if($process.ExitCode -ne 0 -or -not(Test-Path -LiteralPath $result) -or
        -not(Test-Path -LiteralPath $log) -or
        -not([IO.File]::ReadAllText($log).Contains('RESULT=RUN_LEARNING_PASSED'))){throw ('Run learning QA failed; exit='+$process.ExitCode+' log='+$log)}
    $resultText=[IO.File]::ReadAllText($result)
    if(-not $resultText.Contains('failures=0') -or -not $resultText.Contains('mode='+$Mode)){throw 'Result file does not confirm the requested mode.'}
    if($Rendered -and @(Get-ChildItem -LiteralPath $images -File -Filter '*.png').Count -lt 3){throw 'Rendered screenshot evidence incomplete.'}
    Get-Content -LiteralPath $result
    Write-Output ('RESULT=RUN_LEARNING_PASSED mode='+$Mode+' rendered='+$Rendered)
}finally{
    if($null -ne $process -and -not $process.HasExited){Stop-Process -Id $process.Id; $process.WaitForExit(5000)|Out-Null}
    # A watchdog/failed assertion may exit before the C++ fixture's Finish path.
    # Fresh-prefix preflight plus successful owned process launch scopes cleanup.
    if($ownedProcessStarted -and $null -ne $process -and $process.HasExited){
        $qaSaveDirectory=[IO.Path]::GetFullPath($saveDirectory).TrimEnd('\')
        $ownedPattern='^'+[regex]::Escape($prefix)+'_[0-9A-Fa-f]{32}_v5\.sav$'
        foreach($qaFile in @(Get-ChildItem -LiteralPath $qaSaveDirectory -File -Filter ($prefix+'_*') -ErrorAction SilentlyContinue)){
            if($qaFile.Name -cmatch $ownedPattern -and [IO.Path]::GetFullPath($qaFile.DirectoryName).TrimEnd('\') -eq $qaSaveDirectory){
                if($qaFile.IsReadOnly){$qaFile.IsReadOnly=$false; Write-Output ('Released owned QA save readonly attribute: '+$qaFile.Name)}
            }
        }
    }
}
