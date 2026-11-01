[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][ValidatePattern('^[A-Za-z0-9_]{1,24}$')][string]$Label,
    [switch]$Visible,
    [ValidateRange(1,1800)][int]$ObservationSeconds=1800,
    [string]$ProjectRoot=(Split-Path -Parent $PSScriptRoot),
    [string]$EngineRoot='D:\Program Files\UE_5.8'
)
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest

# Deliberately no input injection, test fixtures, automatic profile selection,
# game exit, process kill, save reset, or unattended/offscreen launch option.
if(-not $Visible){throw 'Explicit -Visible is required. Arrange exclusive desktop input control before launch; nothing was started.'}
function Assert-PhysicalDirectory([string]$Path){
    $current=Get-Item -LiteralPath $Path -ErrorAction Stop
    if(-not $current.PSIsContainer){throw ('Expected directory: '+$Path)}
    while($null -ne $current){
        if(($current.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0){throw ('Refusing linked directory in QA path: '+$current.FullName)}
        $current=$current.Parent
    }
}
function Get-UnrealProcesses {
    return @(Get-Process | Where-Object { $_.ProcessName -like 'UnrealEditor*' -or $_.ProcessName -like 'UE4Editor*' -or $_.ProcessName -like 'Rogue10m*' })
}
function Get-SaveInventory([string]$Directory){
    if(-not(Test-Path -LiteralPath $Directory)){return @()}
    return @(Get-ChildItem -LiteralPath $Directory -Filter '*.sav' -File -Recurse | Sort-Object FullName | ForEach-Object {
        [pscustomobject]@{Path=$_.FullName;Bytes=$_.Length;LastWriteTimeUtc=$_.LastWriteTimeUtc.ToString('o');SHA256=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash}
    })
}

$ProjectRoot=(Resolve-Path -LiteralPath $ProjectRoot).Path.TrimEnd('\','/')
if(-not [string]::Equals($ProjectRoot,'D:\Project\Rogue10m',[StringComparison]::OrdinalIgnoreCase)){throw 'This launcher is scoped to D:\Project\Rogue10m only.'}
Assert-PhysicalDirectory $ProjectRoot
Assert-PhysicalDirectory (Join-Path $ProjectRoot 'Content')
$project=Join-Path $ProjectRoot 'Rogue10m.uproject'
$configuration=Get-Content -LiteralPath $project -Raw -Encoding utf8 | ConvertFrom-Json
if($configuration.EngineAssociation -ne '5.8' -or -not($configuration.Modules | Where-Object Name -eq 'Rogue10m')){throw 'Expected UE5.8 Rogue10m project/module not found.'}
foreach($map in @('L_Menu','StartHub','OpenRunMap')){
    if(-not(Test-Path -LiteralPath (Join-Path $ProjectRoot ('Content\Rogue10m\Maps\'+$map+'.umap')) -PathType Leaf)){throw ('Missing expected gameplay map '+$map)}
}
$EngineRoot=(Resolve-Path -LiteralPath $EngineRoot).Path
$version=Get-Content -LiteralPath (Join-Path $EngineRoot 'Engine\Build\Build.version') -Raw -Encoding utf8 | ConvertFrom-Json
if($version.MajorVersion -ne 5 -or $version.MinorVersion -ne 8){throw 'Installed engine must actually identify as UE5.8.'}
$exe=Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor.exe'
$module=Join-Path $ProjectRoot 'Binaries\Win64\UnrealEditor-Rogue10m.dll'
if(-not(Test-Path -LiteralPath $exe -PathType Leaf) -or -not(Test-Path -LiteralPath $module -PathType Leaf)){throw 'Expected editor executable and built Rogue10m editor module are required. Build first.'}
$running=@(Get-UnrealProcesses)
if($running.Count){throw ('Existing UE/game process detected; refusing a second launch. PIDs: '+(($running.Id|ForEach-Object{[string]$_}) -join ','))}

$town='Rogue10m_Town_QA_Manual_'+$Label
$prefix='Rogue10m_Progression_QA_Manual_'+$Label
$keys='Rogue10m_Fighter_QA_Manual_'+$Label
$saves=Join-Path $ProjectRoot 'Saved\SaveGames'
if(Test-Path -LiteralPath $saves){Assert-PhysicalDirectory $saves}
$base=Join-Path $ProjectRoot 'DevLog\ManualPlayQA'
$session=Join-Path $base $Label
$output=Join-Path $session 'Observation'
if(Test-Path -LiteralPath $base){Assert-PhysicalDirectory $base}else{Assert-PhysicalDirectory (Join-Path $ProjectRoot 'DevLog')}
if(Test-Path -LiteralPath $session){throw 'Session/evidence path already exists. Preserve it and choose a fresh label; do not retry an uncertain launch.'}
$before=@(Get-SaveInventory $saves)
$occupied=@($before | Where-Object {
    $name=[IO.Path]::GetFileName($_.Path)
    $name -eq ($town+'.sav') -or $name -eq ($keys+'.sav') -or $name.StartsWith($prefix+'_',[StringComparison]::OrdinalIgnoreCase)
})
if($occupied.Count){throw 'Exact QA_Manual profile, key slot or progression prefix already exists. Preserve existing evidence and choose a fresh label.'}

# Reserve a new session without touching existing files. The observer alone
# creates its fresh Observation subdirectory, after it verifies normal L_Menu UI.
[IO.Directory]::CreateDirectory($session)|Out-Null
$utf8=[Text.UTF8Encoding]::new($false)
[IO.File]::WriteAllText((Join-Path $session 'saves-before.json'),(ConvertTo-Json -InputObject $before -Depth 4),$utf8)
$log=Join-Path $session 'game.log'
$arguments=@(('"'+$project+'"'),'/Game/Rogue10m/Maps/L_Menu','-game','-nosplash','-NoLiveCoding','-NoRemoteShaderCompile',
    '-d3d11','-windowed','-ForceRes','-ResX=1280','-ResY=720',
    ('-RogueProfileSaveSlot='+$town),('-RogueProgressionSavePrefix='+$prefix),('-RogueSkillKeySaveSlot='+$keys),
    ('-RogueManualPlayLabel='+$Label),('-RogueManualPlayOutput="'+$output+'"'),
    ('-RogueManualPlayTimeoutSeconds='+$ObservationSeconds),'-ExecCmds="t.MaxFPS 60,Rogue10m.ObserveManualPlay"',('-abslog="'+$log+'"'))
$manifest=[ordered]@{
    status='launch_prepared_not_yet_started';prepared_utc=[DateTime]::UtcNow.ToString('o');label=$Label
    project=$project;map='/Game/Rogue10m/Maps/L_Menu';executable=$exe
    engine_version=('{0}.{1}.{2}' -f $version.MajorVersion,$version.MinorVersion,$version.PatchVersion)
    engine_changelist=$version.Changelist;module_sha256=(Get-FileHash -LiteralPath $module -Algorithm SHA256).Hash
    module_last_write_utc=(Get-Item -LiteralPath $module).LastWriteTimeUtc.ToString('o')
    town_slot=$town;progression_prefix=$prefix;key_slot=$keys;observation_output=$output
    observation_seconds=$ObservationSeconds;preexisting_saves=$before.Count;arguments=$arguments
    physical_input='not_assessed';normal_full_run='not_assessed'
    note='Visible normal profile UI; observer records only. t.MaxFPS 60 is a process-local console command, without writing config or changing system/global settings. No input automation, fixture, rewards, profile selection, forced exit or process kill. After game exit compare all saves-before hashes and check new files stay in exact three-slot namespace. A 30-minute observation limit never exits the game. Inspect PID/output before retrying any uncertain launch.'
}
[IO.File]::WriteAllText((Join-Path $session 'launch.prepared.json'),($manifest|ConvertTo-Json -Depth 5),$utf8)
if(@(Get-UnrealProcesses).Count){throw 'A UE/game process appeared during preflight. Launch refused; prepared evidence preserved.'}
try{
    # The explicit -Visible option is the only route to this visible launch.
    $process=Start-Process -FilePath $exe -ArgumentList $arguments -WorkingDirectory $ProjectRoot -WindowStyle Normal -PassThru
    $started=[ordered]@{status='process_started_observer_not_yet_confirmed';pid=$process.Id;started_utc=[DateTime]::UtcNow.ToString('o');observation_output=$output;log=$log}
    [IO.File]::WriteAllText((Join-Path $session 'launch.started.json'),($started|ConvertTo-Json),$utf8)
    Write-Output ('Visible manual-play game PID='+$process.Id+'; output='+$session)
    Write-Output 'Observer startup is not confirmed until Observation/summary.txt and MANUAL_OBSERVER_STARTED exist. It never creates a physical-input or full-run PASS.'
    Write-Output 'Close the game normally after testing. The launcher does not wait, inject input, stop the game, or claim post-exit save preservation. Compare saves-before.json afterward.'
}catch{
    [IO.File]::WriteAllText((Join-Path $session 'launch.error.txt'),('utc='+[DateTime]::UtcNow.ToString('o')+"`n"+$_.Exception.Message+"`nInspect process/PID/evidence before retrying; no process was killed or evidence deleted."),$utf8)
    throw
}
