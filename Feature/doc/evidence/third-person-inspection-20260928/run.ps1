param([string]$Command='Rogue10m.TestThirdPersonInspection',[string]$Label='preview',[string]$Marker='RESULT=THIRD_PERSON_INSPECTION_PASSED',[string[]]$ExtraArgs=@())
$ErrorActionPreference='Stop'
$runArgs=@('D:/Project/Rogue10m/Rogue10m.uproject','/Game/FirstPerson/Lvl_FirstPerson','-game','-windowed','-ForceRes','-ResX=1280','-ResY=720','-unattended','-nosplash','-nosound','-NoLiveCoding','-NoRemoteShaderCompile',('-ExecCmds="'+$Command+'"'),('-abslog=D:/Project/Rogue10m/tmp/third-person-inspection/'+$Label+'.log'))
$runArgs += $ExtraArgs
$proc=Start-Process 'D:\Program Files\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe' -ArgumentList $runArgs -WindowStyle Hidden -PassThru
Write-Output ($Label+' pid='+$proc.Id)
if(-not $proc.WaitForExit(240000)) { throw ('Runtime timeout pid='+$proc.Id) }
$runLog=Get-Content ('tmp/third-person-inspection/'+$Label+'.log') -Raw
if($proc.ExitCode -ne 0 -or -not $runLog.Contains($Marker)) { throw ($Label+' verification failed') }
Write-Output $Marker
