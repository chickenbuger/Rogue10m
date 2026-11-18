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
function Assert-NoProjectEditor {
    $running=@(Get-CimInstance Win32_Process | Where-Object {
        $_.Name -like 'UnrealEditor*' -and (-not $_.CommandLine -or
            $_.CommandLine.Replace('/','\').IndexOf($ProjectFile,[StringComparison]::OrdinalIgnoreCase) -ge 0 -or
            $_.CommandLine.IndexOf([IO.Path]::GetFileName($ProjectFile),[StringComparison]::OrdinalIgnoreCase) -ge 0)
    })
    if($running.Count){throw 'Project UE process is running, or its command line cannot be inspected; coordinate before QA.'}
}
function Get-SaveHashes {
    if(Test-Path -LiteralPath $SaveRoot){
        Get-ChildItem -LiteralPath $SaveRoot -Recurse -File | ForEach-Object {
            [pscustomobject]@{path=$_.FullName;sha256=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash}
        }
    }
}
function Test-OwnedSave([string]$Path) {
    $name=[IO.Path]::GetFileName($Path)
    return [IO.Path]::GetDirectoryName($Path) -ieq $SaveRoot -and
        ($name -ieq ($Town+'.sav') -or $name -ieq ($Keys+'.sav') -or
        ($name.StartsWith($Prefix+'_',[StringComparison]::OrdinalIgnoreCase) -and $name.EndsWith('.sav',[StringComparison]::OrdinalIgnoreCase)))
}
Assert-NoProjectEditor
$Before=@(Get-SaveHashes)
if(@($Before | Where-Object { Test-OwnedSave $_.path }).Count){throw 'Existing QA saves match this Label; they will not be overwritten or removed.'}
[IO.Directory]::CreateDirectory($LogRoot)|Out-Null
ConvertTo-Json -InputObject $Before -Depth 4 | Set-Content -LiteralPath (Join-Path $LogRoot 'saves-before.json') -Encoding utf8NoBOM
$Journal=[ordered]@{label=$Label;project=$ProjectFile;editor=$EditorFile;townSlot=$Town;progressionPrefix=$Prefix;skillSlot=$Keys;startedUtc=[DateTime]::UtcNow.ToString('o');runs=@();note='Controlled real collision and persistence fixture; guaranteed transient table; not balance validation.'}
$JournalFile=Join-Path $LogRoot 'run-journal.json'
$Journal|ConvertTo-Json -Depth 6|Set-Content -LiteralPath $JournalFile -Encoding utf8NoBOM
$Process=$null
$Failure=$null
try {
    foreach($Mode in @('Write','Read')){
        Assert-NoProjectEditor
        $Log=Join-Path $LogRoot ($Mode.ToLowerInvariant()+'.log')
        $Arguments=@(
            ('"'+$ProjectFile+'"'),'/Game/Rogue10m/Maps/StartHub','-game','-unattended','-nullrhi','-nosound','-nosplash','-DisablePython',
            ('-RogueMonsterLootLabel='+$Label),('-RogueProfileSaveSlot='+$Town),('-RogueProgressionSavePrefix='+$Prefix),('-RogueSkillKeySaveSlot='+$Keys),
            ('-ExecCmds="Rogue10m.TestMonsterLoot'+$Mode+'"'),('-abslog="'+$Log+'"')
        )
        $Record=[ordered]@{mode=$Mode;command=$Arguments;startedUtc=[DateTime]::UtcNow.ToString('o');pid=$null;exitCode=$null;timedOut=$false;resultVerified=$false;redLogCount=0}
        $Process=Start-Process -FilePath $EditorFile -ArgumentList $Arguments -WorkingDirectory $ProjectRoot -WindowStyle Hidden -PassThru
        $Record.pid=$Process.Id
        $Stopwatch=[Diagnostics.Stopwatch]::StartNew()
        while(-not $Process.WaitForExit(1000)){
            if($Stopwatch.Elapsed.TotalSeconds -ge $TimeoutSeconds){
                $Record.timedOut=$true
                # This is exclusively the process tree launched immediately above.
                $Process.Kill($true)
                if(-not $Process.WaitForExit(10000)){throw 'Owned QA process did not exit after timeout; log is still unverified.'}
                break
            }
        }
        $Process.Refresh()
        $Record.exitCode=$Process.ExitCode
        $Record.endedUtc=[DateTime]::UtcNow.ToString('o')
        # Read only after the owned process has exited. A success marker never overrides errors.
        $Text=if(Test-Path -LiteralPath $Log){[IO.File]::ReadAllText($Log)}else{''}
        $RedLines=@($Text -split '\r?\n' | Where-Object {
            $_ -match '(?i)\bLog[^\s:]*:\s*(?:Error|Fatal)\s*:|\bFatal error\b|\b(?:Handled\s+)?ensure\s+(?:condition|failed|failure)|\bTraceback \(most recent call last\)|\bAttributeError:'
        })
        $Record.redLogCount=$RedLines.Count
        ConvertTo-Json -InputObject $RedLines | Set-Content -LiteralPath (Join-Path $LogRoot ($Mode.ToLowerInvariant()+'-red-log.json')) -Encoding utf8NoBOM
        $Pattern='RESULT=MONSTER_LOOT_RUNTIME_PASSED mode='+$Mode.ToLowerInvariant()+' '
        $Record.resultVerified=($Record.exitCode -eq 0 -and -not $Record.timedOut -and $Text.Contains($Pattern) -and $RedLines.Count -eq 0)
        $Journal.runs+=@($Record)
        $Journal|ConvertTo-Json -Depth 6|Set-Content -LiteralPath $JournalFile -Encoding utf8NoBOM
        if(-not $Record.resultVerified){throw ('Monster loot runtime '+$Mode+' failed or was not proven; inspect '+$Log+'. Evidence was retained.')}
    }
} catch { $Failure=$_ } finally {
    if($Process -and -not $Process.HasExited){
        $Process.Kill($true)
        if(-not $Process.WaitForExit(10000)){throw 'Owned QA process remains active; save hashes cannot be certified.'}
    }
    $After=@(Get-SaveHashes)
    ConvertTo-Json -InputObject $After -Depth 4 | Set-Content -LiteralPath (Join-Path $LogRoot 'saves-after.json') -Encoding utf8NoBOM
    $AfterByPath=@{}; foreach($save in $After){$AfterByPath[$save.path]=$save.sha256}
    $BeforeByPath=@{}; foreach($save in $Before){$BeforeByPath[$save.path]=$save.sha256}
    $Changed=@($Before | Where-Object {-not $AfterByPath.ContainsKey($_.path) -or $AfterByPath[$_.path] -ne $_.sha256})
    $Unexpected=@($After | Where-Object {-not $BeforeByPath.ContainsKey($_.path) -and -not (Test-OwnedSave $_.path)})
    $Journal.originalChanged=@($Changed | ForEach-Object { $_.path })
    $Journal.unexpectedNew=@($Unexpected | ForEach-Object { $_.path })
    $Journal.failure=if($Failure){$Failure.ToString()}else{$null}
    $Journal.completedUtc=[DateTime]::UtcNow.ToString('o')
    $Journal|ConvertTo-Json -Depth 6|Set-Content -LiteralPath $JournalFile -Encoding utf8NoBOM
}
if($Changed.Count -or $Unexpected.Count){throw 'Save preservation or namespace check failed; inspect the retained journal and hashes.'}
if($Failure){throw $Failure}
Write-Output ('Monster loot write/read runtime passed with clean error logs and preserved saves; evidence '+$Evidence+'; logs '+$LogRoot)
