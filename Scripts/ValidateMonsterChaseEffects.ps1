param([string]$Label='', [ValidateRange(60,600)][int]$TimeoutSeconds=240,
 [string]$ProjectRoot=(Split-Path -Parent $PSScriptRoot))
$ErrorActionPreference='Stop'
if ([string]::IsNullOrWhiteSpace($Label)) {$Label=[DateTime]::UtcNow.ToString('yyyyMMdd_HHmmss')}
if ($Label -notmatch '^[A-Za-z0-9_]{1,35}$') {throw 'Use a fresh, short QA label.'}
$fxSlot='Rogue10m_Town_QA_MonsterFX_'+$Label
$fxOutput=Join-Path $ProjectRoot ('Feature/doc/evidence/monster-effects-chase-20261011/'+$fxSlot)
if ((Test-Path -LiteralPath $fxOutput) -or (Test-Path -LiteralPath (Join-Path $ProjectRoot ('Saved/SaveGames/'+$fxSlot+'.sav')))) {throw 'QA label exists.'}
New-Item -ItemType Directory -Path $fxOutput -Force | Out-Null
$fxLog=Join-Path $fxOutput 'runtime.log'
$fxArguments=@(('"'+(Join-Path $ProjectRoot 'Rogue10m.uproject')+'"'),'/Game/Rogue10m/Maps/OpenRunMap',
 '-game','-RenderOffscreen','-windowed','-ForceRes','-ResX=1280','-ResY=720','-unattended','-nosound','-nosplash','-NoLiveCoding','-NoRemoteShaderCompile',
 ('-RogueProfileSaveSlot='+$fxSlot),('-RogueProgressionSavePrefix=Rogue10m_Progression_QA_MonsterFX_'+$Label),
 ('-RogueSkillKeySaveSlot=Rogue10m_Fighter_QA_MonsterFX_'+$Label),'-ExecCmds="Rogue10m.TestMonsterChaseEffects"',('-abslog="'+$fxLog+'"'))
$fxProcess=$null
try {
 $fxProcess=Start-Process -FilePath 'D:/Program Files/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe' -ArgumentList $fxArguments -WindowStyle Hidden -PassThru
 Write-Output ('Monster chase/effects QA pid='+$fxProcess.Id+' log='+$fxLog)
 $fxDeadline=[DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
 while (-not $fxProcess.WaitForExit(1000)) {if ([DateTime]::UtcNow -gt $fxDeadline) {throw ('QA timeout: '+$fxLog)}}
 if ($fxProcess.ExitCode -ne 0 -or -not ([IO.File]::ReadAllText($fxLog).Contains('RESULT=MONSTER_FX_PASSED'))) {throw ('Monster chase/effects QA failed: '+$fxLog)}
 Write-Output ('RESULT=MONSTER_FX_PASSED evidence='+$fxOutput)
} finally {if ($null -ne $fxProcess -and -not $fxProcess.HasExited) {Stop-Process -Id $fxProcess.Id; $fxProcess.WaitForExit(5000) | Out-Null}}
