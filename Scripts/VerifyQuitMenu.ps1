#requires -Version 7.0
[CmdletBinding()]
param(
 [Parameter(Mandatory)][ValidatePattern('^[A-Za-z0-9_]{1,24}$')][string]$Label,
 [ValidateSet('Menu','Hub','Run','Restore','Failure','Terminal')][string]$Mode='Hub',
 [switch]$Rendered,
 [string]$ProjectRoot=(Split-Path -Parent $PSScriptRoot),
 [ValidateRange(90,240)][int]$TimeoutSeconds=150
)
$ErrorActionPreference='Stop'
$root=(Resolve-Path -LiteralPath $ProjectRoot).Path
$packet=Join-Path $root 'Feature\doc\evidence\quit-menu-20261111'
$evidenceLabel=if($Mode-eq'Restore'){$Label+'_restore'}else{$Label}
$output=Join-Path $packet $evidenceLabel
$log=Join-Path $packet ($evidenceLabel+'.log')
$receipt=Join-Path $packet ($evidenceLabel+'-launcher.json')
$profile='Rogue10m_Town_QA_Quit_'+$Label
$prefix='Rogue10m_Progression_QA_Quit_'+$Label
$keys='Rogue10m_Fighter_QA_Quit_'+$Label
foreach($path in @($output,$log,$receipt)) { if(Test-Path -LiteralPath $path){throw 'Existing evidence refused; use a fresh Label.'} }
$saveRoot=Join-Path $root 'Saved\SaveGames'
if($Mode-ne'Restore'-and@(Get-ChildItem -LiteralPath $saveRoot -Filter ('*'+$Label+'*') -ErrorAction SilentlyContinue).Count){throw 'Existing QA namespace refused.'}
function Inventory {
 $rows=@{}
 foreach($directory in @($saveRoot,'D:\Project\Rogue10m\Saved\SaveGames',(Join-Path $env:LOCALAPPDATA 'Rogue10m\Saved\SaveGames'))){
  if(Test-Path -LiteralPath $directory){
   foreach($file in @(Get-ChildItem -LiteralPath $directory -Recurse -File -Filter '*.sav')){
    $rows[$file.FullName]=(Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash
   }
  }
 }
 return $rows
}
function IsExpectedQA([string]$path) {
 $name=[IO.Path]::GetFileName($path)
 return [IO.Path]::GetDirectoryName($path)-ieq$saveRoot-and($name-ceq($profile+'.sav')-or$name-ceq($keys+'.sav')-or$name-cmatch('^'+[regex]::Escape($prefix)+'_[0-9A-Fa-f]{32}_v5\.sav$'))
}
$before=Inventory
New-Item -ItemType Directory -Path $packet -Force | Out-Null
$exe='D:\Program Files\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$map=if($Mode-eq'Menu'){'/Game/Rogue10m/Maps/L_Menu'}else{'/Game/Rogue10m/Maps/StartHub'}
$arguments=@(('"'+(Join-Path $root 'Rogue10m.uproject')+'"'),$map,'-game','-unattended','-nosplash','-nosound','-NoLiveCoding','-NoRemoteShaderCompile','-DisablePython',
 ('-RogueProfileSaveSlot='+$profile),('-RogueProgressionSavePrefix='+$prefix),('-RogueSkillKeySaveSlot='+$keys),
 ('-RogueQuitMenuLabel='+$Label),('-RogueQuitMenuMode='+$Mode),'-ExecCmds="t.MaxFPS 60,Rogue10m.TestQuitMenu"',('-abslog="'+$log+'"'))
if($Rendered){$arguments+=@('-d3d11','-RenderOffscreen','-windowed','-ForceRes','-ResX=1280','-ResY=720')}else{$arguments+='-nullrhi'}
$meta=[ordered]@{label=$Label;mode=$Mode;rendered=[bool]$Rendered;pid=$null;exe=$exe;exit=$null;status='starting';preserved=$false;changed=@();missing=@();unknownNew=@();startedUtc=[DateTime]::UtcNow.ToString('o')}
$process=$null
try {
 $process=Start-Process -FilePath $exe -ArgumentList $arguments -WindowStyle Hidden -PassThru
 $meta.pid=$process.Id
 $ownedTicks=$process.StartTime.ToUniversalTime().Ticks
 $null=$process.Handle
 Write-Output ('QUIT_MENU_QA_LAUNCHED mode='+$Mode+' pid='+$process.Id)
 if(-not$process.WaitForExit($TimeoutSeconds*1000)){throw 'Owned quit UI QA exceeded its watchdog.'}
 $meta.exit=$process.ExitCode
 $result=Join-Path $output 'results.txt'
 if($meta.exit-ne0-or-not(Test-Path -LiteralPath $result)){throw ('Runtime exit/result failed: '+$meta.exit)}
 $body=[IO.File]::ReadAllText($result)
 if($body-notmatch'(?m)^failures=0\r?$'-or$body-notmatch'(?m)^quit_requested=1\r?$'-or$body-match'(?m)^FAIL '){throw 'Native checks failed or production quit not requested.'}
 if(-not(Test-Path -LiteralPath (Join-Path $output 'exit_callback.txt'))){throw 'Engine pre-exit callback missing.'}
 if(-not[IO.File]::ReadAllText($log).Contains('RESULT=QUIT_MENU_PASSED')){throw 'Native PASS log missing.'}
 if($Rendered-and@(Get-ChildItem -LiteralPath $output -Filter '*.png' -File).Count-lt1){throw 'Rendered screenshot missing.'}
 $meta.status='passed_actual_quit'
} catch {$meta.status='failed';$meta.error=$_.Exception.Message}
finally {
 if($process-and-not$process.HasExited){
  $live=Get-Process -Id $process.Id -ErrorAction SilentlyContinue
  if($live-and$live.StartTime.ToUniversalTime().Ticks-eq$ownedTicks-and$live.Path-ieq$exe){$process.Kill();$process.WaitForExit(5000)|Out-Null}
 }
 $after=Inventory
 $modified=@($before.Keys|Where-Object{$after.ContainsKey($_)-and$after[$_]-cne$before[$_]})
 $meta.expectedQAChanges=@($modified|Where-Object{$Mode-eq'Restore'-and(IsExpectedQA $_)})
 $meta.changed=@($modified|Where-Object{-not($Mode-eq'Restore'-and(IsExpectedQA $_))})
 $meta.missing=@($before.Keys|Where-Object{-not$after.ContainsKey($_)})
 $meta.unknownNew=@($after.Keys|Where-Object{-not$before.ContainsKey($_)-and-not(IsExpectedQA $_)})
 $meta.preserved=$meta.changed.Count-eq0-and$meta.missing.Count-eq0-and$meta.unknownNew.Count-eq0
 $meta.finishedUtc=[DateTime]::UtcNow.ToString('o')
 $meta|ConvertTo-Json -Depth 6|Set-Content -LiteralPath $receipt -Encoding utf8NoBOM
}
if($meta.status-ne'passed_actual_quit'-or-not$meta.preserved){$meta|ConvertTo-Json -Depth 6;throw ('Quit QA failed: '+$receipt)}
Get-Content -LiteralPath (Join-Path $output 'results.txt')
Write-Output ('RESULT=QUIT_MENU_PASSED preservation=verified receipt='+$receipt)
