#requires -Version 7.0
[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][ValidatePattern('^[A-Za-z0-9_]{1,24}$')][string]$Label,
    [string]$Project='D:\Project\Rogue10m\Rogue10m.uproject',
    [string]$EditorCmd='D:\Program Files\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe',
    [Parameter(Mandatory=$true)][string]$LogDirectory,
    [ValidateRange(100,240)][int]$TimeoutSeconds=120
)
$ErrorActionPreference='Stop'
$ProjectFile=(Resolve-Path -LiteralPath $Project).Path
$EditorFile=(Resolve-Path -LiteralPath $EditorCmd).Path
$ProjectRoot=Split-Path -Parent $ProjectFile
$Town='Rogue10m_Town_QA_MonsterLoot_'+$Label
$Prefix='Rogue10m_Progression_QA_MonsterLoot_'+$Label
$Keys='Rogue10m_Fighter_QA_MonsterLoot_'+$Label
$SaveRoot=Join-Path $ProjectRoot 'Saved\SaveGames'
$Evidence=Join-Path $ProjectRoot ('DevLog\MonsterLootQA\'+$Label)
$LogRoot=[IO.Path]::GetFullPath($LogDirectory)
if(Test-Path -LiteralPath $LogRoot){throw 'A fresh, nonexistent LogDirectory is required.'}
if(Test-Path -LiteralPath $Evidence){throw 'Existing runtime evidence must be preserved; choose a fresh Label.'}
$Existing=@()
if(Test-Path -LiteralPath $SaveRoot){
    $Existing=@(Get-ChildItem -LiteralPath $SaveRoot -File -Filter '*.sav' |
        Where-Object {$_.Name -ceq ($Town+'.sav') -or $_.Name -ceq ($Keys+'.sav') -or $_.Name.StartsWith($Prefix+'_',[StringComparison]::OrdinalIgnoreCase)})
}
if($Existing.Count -ne 0){throw 'Existing QA saves match this Label; they will not be overwritten or removed.'}
[IO.Directory]::CreateDirectory($LogRoot)|Out-Null
$Journal=[ordered]@{label=$Label;project=$ProjectFile;editor=$EditorFile;townSlot=$Town;progressionPrefix=$Prefix;skillSlot=$Keys;startedUtc=[DateTime]::UtcNow.ToString('o');runs=@();note='Controlled real collision and persistence fixture; guaranteed transient table; not balance validation.'}
$JournalFile=Join-Path $LogRoot 'run-journal.json'
$Journal|ConvertTo-Json -Depth 6|Set-Content -LiteralPath $JournalFile -Encoding utf8NoBOM
foreach($Mode in @('Write','Read')){
    $Log=Join-Path $LogRoot ($Mode.ToLowerInvariant()+'.log')
    $Arguments=@(
        ('"'+$ProjectFile+'"'),'/Game/Rogue10m/Maps/StartHub','-game','-unattended','-nullrhi','-nosound','-nosplash',
        ('-RogueMonsterLootLabel='+$Label),('-RogueProfileSaveSlot='+$Town),('-RogueProgressionSavePrefix='+$Prefix),('-RogueSkillKeySaveSlot='+$Keys),
        ('-ExecCmds="Rogue10m.TestMonsterLoot'+$Mode+'"'),('-abslog="'+$Log+'"')
    )
    $Record=[ordered]@{mode=$Mode;command=$Arguments;startedUtc=[DateTime]::UtcNow.ToString('o');pid=$null;exitCode=$null;timedOut=$false;resultVerified=$false}
    $Process=Start-Process -FilePath $EditorFile -ArgumentList $Arguments -WorkingDirectory $ProjectRoot -WindowStyle Hidden -PassThru
    $Record.pid=$Process.Id
    $Stopwatch=[Diagnostics.Stopwatch]::StartNew()
    while(-not $Process.WaitForExit(1000)){
        if($Stopwatch.Elapsed.TotalSeconds -ge $TimeoutSeconds){
            $Record.timedOut=$true
            # This is exclusively the process tree launched immediately above.
            $Process.Kill($true)
            $Process.WaitForExit(10000)|Out-Null
            break
        }
    }
    $Process.Refresh()
    if($Process.HasExited){$Record.exitCode=$Process.ExitCode}
    $Record.endedUtc=[DateTime]::UtcNow.ToString('o')
    # Never read a live log: the launched process has fully exited before this read.
    $Text=if(Test-Path -LiteralPath $Log){[IO.File]::ReadAllText($Log)}else{''}
    $Pattern='RESULT=MONSTER_LOOT_RUNTIME_PASSED mode='+$Mode.ToLowerInvariant()+' '
    $Record.resultVerified=($Record.exitCode -eq 0 -and -not $Record.timedOut -and $Text.Contains($Pattern))
    $Journal.runs+=@($Record)
    $Journal|ConvertTo-Json -Depth 6|Set-Content -LiteralPath $JournalFile -Encoding utf8NoBOM
    if(-not $Record.resultVerified){throw ('Monster loot runtime '+$Mode+' failed or was not proven; inspect '+$Log+'. Existing saves and evidence were preserved.')}
}
$Journal.completedUtc=[DateTime]::UtcNow.ToString('o')
$Journal|ConvertTo-Json -Depth 6|Set-Content -LiteralPath $JournalFile -Encoding utf8NoBOM
Write-Output ('Monster loot write/read runtime passed; evidence '+$Evidence+'; logs '+$LogRoot)
