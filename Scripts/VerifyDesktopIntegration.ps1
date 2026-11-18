param(
    [ValidatePattern('^[A-Za-z0-9_]{1,24}$')][string]$Label = 'DPlayA_20261027',
    [ValidateRange(180,600)][int]$TimeoutSeconds = 300,
    [ValidateRange(0,120)][int]$NativeHoldSeconds = 90,
    [string]$ProjectRoot = (Split-Path -Parent $PSScriptRoot)
)
$ErrorActionPreference='Stop'
$profile='Rogue10m_Town_QA_Desktop_'+$Label
$prefix='Rogue10m_Progression_QA_Desktop_'+$Label
$keys='Rogue10m_Fighter_QA_Desktop_'+$Label
$saveDirectory=Join-Path $ProjectRoot 'Saved\SaveGames'
$evidence=Join-Path $ProjectRoot 'Feature\doc\evidence\desktop-render-20261027'
$output=Join-Path $evidence $Label
$images=Join-Path $ProjectRoot ('DevLog\20261027_DesktopIntegration_'+$Label)
$log=Join-Path $evidence ($Label+'.log')
if((Test-Path -LiteralPath $output) -or (Test-Path -LiteralPath $images) -or (Test-Path -LiteralPath $log) -or
    (Test-Path -LiteralPath (Join-Path $saveDirectory ($profile+'.sav'))) -or
    (Test-Path -LiteralPath (Join-Path $saveDirectory ($keys+'.sav'))) -or
    (Get-ChildItem -LiteralPath $saveDirectory -Filter ($prefix+'_*') -ErrorAction SilentlyContinue|Select-Object -First 1)) {
    throw 'Preserve existing QA evidence/saves; choose a fresh label.'
}
New-Item -ItemType Directory -Force -Path $evidence|Out-Null
$arguments=@(('"'+(Join-Path $ProjectRoot 'Rogue10m.uproject')+'"'),'/Game/Rogue10m/Maps/StartHub',
    '-game','-d3d11','-windowed','-ForceRes','-ResX=1280','-ResY=720','-unattended','-nosplash','-nosound',
    '-NoLiveCoding','-NoRemoteShaderCompile',('-RogueProfileSaveSlot='+$profile),
    ('-RogueProgressionSavePrefix='+$prefix),('-RogueSkillKeySaveSlot='+$keys),('-RogueDesktopQALabel='+$Label),('-RogueDesktopQANativeHold='+$NativeHoldSeconds),
    '-ExecCmds="t.MaxFPS 60,r.ScreenPercentage 75,Rogue10m.TestDesktopWeaponIntegration"',('-abslog="'+$log+'"'))
$process=$null
try {
    # A visible standalone is required for the requested native keyboard/mouse confirmation.
    $process=Start-Process -FilePath 'D:\Program Files\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe' -ArgumentList $arguments -WindowStyle Normal -PassThru
    Write-Output ('Owned rendered QA PID='+$process.Id+' log='+$log)
    if(-not $process.WaitForExit($TimeoutSeconds*1000)){throw 'Rendered QA timed out.'}
    $result=Join-Path $output 'results.txt'
    if($process.ExitCode -ne 0 -or -not(Test-Path -LiteralPath $result) -or
        -not([IO.File]::ReadAllText($log).Contains('RESULT=DESKTOP_INTEGRATION_PASSED'))){throw ('Rendered QA failed; exit='+$process.ExitCode+' log='+$log)}
    $photos=@(Get-ChildItem -LiteralPath $images -Filter '*.png' -File)
    if($photos.Count -lt 5){throw 'Rendered viewport screenshots incomplete.'}
    Get-Content -LiteralPath $result
    Write-Output ('RESULT=DESKTOP_INTEGRATION_PASSED screenshots='+$photos.Count+' images='+$images)
} finally {
    if($null -ne $process -and -not $process.HasExited){Stop-Process -Id $process.Id; $process.WaitForExit(5000)|Out-Null}
}
