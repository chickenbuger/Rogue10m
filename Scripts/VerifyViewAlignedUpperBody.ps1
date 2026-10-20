param(
    [ValidateSet('baseline','runtime','render','negative')][string]$Phase='runtime',
    [string]$Label='',
    [switch]$Movie,
    [int]$TimeoutSeconds=300,
    [string]$EngineRoot='D:\Program Files\UE_5.8',
    [string]$ProjectRoot='D:\Project\Rogue10m'
)
$ErrorActionPreference='Stop'
if([string]::IsNullOrEmpty($Label)){$Label='upper-body-aim-20261011-'+$Phase}
if($Label -notmatch '^[A-Za-z0-9_-]{1,40}$'){throw 'Invalid evidence label: use 1..40 letters, digits, underscores or hyphens.'}
$qaSlotLabel=$Label.Replace('-','_')
if($TimeoutSeconds -lt 30 -or $TimeoutSeconds -gt 600){throw 'Timeout must be 30..600 seconds.'}
$evidence=Join-Path $ProjectRoot 'Feature\doc\evidence\view-aligned-upper-body-20261011'
$log=Join-Path $evidence ($Label+'.log')
$metrics=Join-Path $ProjectRoot ('tmp\view-aligned-upper-body\'+$Label+'\metrics.txt')
if((Test-Path -LiteralPath $log) -or (Test-Path -LiteralPath $metrics)){throw 'Existing evidence: choose a fresh label.'}
New-Item -ItemType Directory -Path $evidence -Force | Out-Null
$arguments=@(('"'+(Join-Path $ProjectRoot 'Rogue10m.uproject')+'"'),'/Game/FirstPerson/Lvl_FirstPerson','-game','-unattended','-nosplash','-nosound',
    '-NoLiveCoding','-NoRemoteShaderCompile','-windowed','-ForceRes','-ResX=1280','-ResY=720',
    ('-RogueSkillKeySaveSlot=Rogue10m_Fighter_QA_20261011_UpperBody_'+$qaSlotLabel),
    ('-RogueProgressionSavePrefix=Rogue10m_Progression_QA_ViewAim_'+$qaSlotLabel),
    '-ExecCmds="Rogue10m.TestViewAlignedUpperBody"',('-RogueUpperBodyAimLabel='+$Label),('-abslog="'+$log+'"'))
if($Phase -eq 'render'){$arguments+=@('-RogueUpperBodyAimCapture','-ini:Engine:[/Script/Engine.RendererSettings]:r.RayTracing=0,r.PathTracing=0');if($Movie){$arguments+='-RogueUpperBodyAimMovie'}}
else{$arguments+='-nullrhi'}
if($Phase -eq 'negative'){$arguments+='-RogueUpperBodyAimDisabledControl'}
if($Phase -eq 'baseline'){$arguments+='-RogueUpperBodyAimPairedBaselineControl'}
$editor=Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$process=Start-Process -FilePath $editor -ArgumentList $arguments -WindowStyle Hidden -PassThru
Write-Output ($Phase+' pid='+$process.Id+' log='+$log)
$deadline=[DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
while(-not $process.WaitForExit(1000)){if([DateTime]::UtcNow -gt $deadline){Stop-Process -Id $process.Id;throw ($Phase+' timed out; log='+$log)}}
$expected=if($Phase -eq 'negative'){'RESULT=VIEW_ALIGNED_UPPER_BODY_FAILED'}else{'RESULT=VIEW_ALIGNED_UPPER_BODY_PASSED'}
$validExit=if($Phase -eq 'negative'){$process.ExitCode -in @(0,1)}else{$process.ExitCode -eq 0}
if(-not $validExit -or -not (Test-Path -LiteralPath $log) -or -not ([IO.File]::ReadAllText($log).Contains($expected)) -or -not (Test-Path -LiteralPath $metrics))
{throw ($Phase+' failed; exit='+$process.ExitCode+' log='+$log)}
if($Phase -eq 'negative'){
    $observed=[IO.File]::ReadAllText($log)
    if(-not $observed.Contains('upper_arm_direction_does_not_follow_world_mouse_aim') -or -not $observed.Contains('applied_pitch_wrong_sign_or_clamp')){throw 'Disabled negative control failed for an unrelated reason.'}
}
Write-Output ($expected+' exit='+$process.ExitCode+' metrics='+$metrics)
