param(
    [Parameter(Mandatory=$true)][ValidatePattern('^[A-Za-z0-9_]{1,24}$')][string]$Label,
    [ValidateRange(1300,1800)][int]$MaxSeconds=1560,
    [switch]$Visible,
    [switch]$PreparedBossStrategy,
    [switch]$AnimationEvidence,
    [string]$ProjectRoot='D:\Project\Rogue10m'
)
# DRAFT launcher: integrate/review/build before running. No gameplay-state fixtures.
$ErrorActionPreference='Stop'

function Assert-PhysicalDirectory([string]$Path){
    $entry=Get-Item -LiteralPath $Path -Force
    if(-not $entry.PSIsContainer){throw ('Required directory is missing: '+$Path)}
    $cursor=$entry
    while($null -ne $cursor){
        if(($cursor.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0){throw ('Linked directory is not allowed: '+$cursor.FullName)}
        $cursor=$cursor.Parent
    }
    return [IO.Path]::GetFullPath($entry.FullName).TrimEnd('\')
}
function Get-SaveInventory([string]$Directory){
    $root=Assert-PhysicalDirectory $Directory
    $pending=New-Object 'System.Collections.Generic.Stack[string]'
    $pending.Push($root)
    $rows=New-Object 'System.Collections.Generic.List[object]'
    while($pending.Count -gt 0){
        $current=$pending.Pop()
        foreach($entry in @(Get-ChildItem -LiteralPath $current -Force)){
            if(($entry.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0){throw ('Linked save entry is not allowed: '+$entry.FullName)}
            if($entry.PSIsContainer){$pending.Push($entry.FullName); continue}
            if($entry.Extension -ieq '.sav'){
                $rows.Add([pscustomobject][ordered]@{
                    RelativePath=$entry.FullName.Substring($root.Length+1)
                    Bytes=$entry.Length
                    SHA256=(Get-FileHash -LiteralPath $entry.FullName -Algorithm SHA256).Hash
                    LastWriteUtc=$entry.LastWriteTimeUtc.ToString('o')
                })
            }
        }
    }
    return @($rows.ToArray()|Sort-Object RelativePath)
}
function Save-Json([string]$Path,$Value){
    $json=ConvertTo-Json -InputObject $Value -Depth 12
    [IO.File]::WriteAllText($Path,$json,[Text.UTF8Encoding]::new($false))
}
function Read-DriverSummary([string]$Path,[bool]$ExpectPrepared=$false,[bool]$ExpectAnimation=$false){
    $values=[ordered]@{}
    $issues=New-Object 'System.Collections.Generic.List[string]'
    if(-not(Test-Path -LiteralPath $Path -PathType Leaf)){
        $issues.Add('driver_summary_missing')
    }else{
        foreach($line in [IO.File]::ReadAllLines($Path)){
            $at=$line.IndexOf('=')
            if($at -lt 1){continue}
            $key=$line.Substring(0,$at).Trim()
            $value=$line.Substring($at+1).Trim()
            if($values.Contains($key)){$issues.Add('duplicate_summary_key:'+ $key); continue}
            $values[$key]=$value
        }
        if(-not($values.Contains('outcome')) -or $values['outcome'] -notin @('returned_victory','returned_death','returned_timeout','blocked')){$issues.Add('invalid_or_missing_outcome')}
        if(-not($values.Contains('full_boss_completion')) -or $values['full_boss_completion'] -notin @('0','1')){$issues.Add('invalid_or_missing_full_boss_completion')}
        if(-not($values.Contains('physical_input')) -or $values['physical_input'] -cne 'not_performed'){$issues.Add('invalid_or_missing_physical_input_disclosure')}
        if($values['full_boss_completion'] -ceq '1' -and $values['outcome'] -cne 'returned_victory'){$issues.Add('boss_completion_contradicts_outcome')}
        if($ExpectPrepared -and (-not $values.Contains('prepared_boss_strategy') -or $values['prepared_boss_strategy'] -cne '1')){$issues.Add('requested_prepared_strategy_not_confirmed_by_driver')}
        if($ExpectAnimation -and (-not $values.Contains('animation_evidence') -or $values['animation_evidence'] -cne 'opportunistic_read_only')){$issues.Add('requested_animation_observation_not_confirmed_by_driver')}
        if(-not $ExpectAnimation -and $values.Contains('animation_evidence')){$issues.Add('unexpected_animation_observation_in_baseline_run')}
        if(-not $ExpectPrepared -and $values.Contains('prepared_boss_strategy') -and $values['prepared_boss_strategy'] -cne '0'){$issues.Add('unexpected_prepared_strategy_in_baseline_run')}
    }
    return [pscustomobject]@{Values=$values;Issues=@($issues.ToArray());Valid=($issues.Count -eq 0)}
}

$expectedRoot=[IO.Path]::GetFullPath('D:\Project\Rogue10m').TrimEnd('\')
if([IO.Path]::GetFullPath($ProjectRoot).TrimEnd('\') -ine $expectedRoot){throw 'Only D:\Project\Rogue10m is supported by this launcher.'}
$ProjectRoot=Assert-PhysicalDirectory $ProjectRoot
$null=Assert-PhysicalDirectory (Join-Path $ProjectRoot 'Content')
$saveDirectory=Assert-PhysicalDirectory (Join-Path $ProjectRoot 'Saved\SaveGames')
$null=Assert-PhysicalDirectory (Join-Path $ProjectRoot 'DevLog')
$projectFile=Join-Path $ProjectRoot 'Rogue10m.uproject'
if(-not(Test-Path -LiteralPath $projectFile -PathType Leaf)){throw 'Reviewed Rogue10m.uproject was not found.'}
$projectInfo=Get-Item -LiteralPath $projectFile -Force
if(($projectInfo.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0){throw 'Linked project file is not allowed.'}
$existingUE=@(Get-Process -Name 'UnrealEditor','UnrealEditor-Cmd','UnrealBuildTool','ShaderCompileWorker','CrashReportClient' -ErrorAction SilentlyContinue)
if($existingUE.Count -gt 0){throw ('Existing UE-related processes must be coordinated first; refusing launch. PIDs='+ (($existingUE|ForEach-Object{$_.Id}) -join ','))}

$profile='Rogue10m_Town_QA_BasicRun_'+$Label
$prefix='Rogue10m_Progression_QA_BasicRun_'+$Label
$keys='Rogue10m_Fighter_QA_BasicRun_'+$Label
$qaRoot=Join-Path $ProjectRoot 'DevLog\BasicRunPlayQA'
if(Test-Path -LiteralPath $qaRoot){$null=Assert-PhysicalDirectory $qaRoot}
$session=Join-Path $qaRoot $Label
$driverOutput=Join-Path $session 'Driver'
$log=Join-Path $session 'game.log'
if(Test-Path -LiteralPath $session){throw 'Existing session evidence must be preserved; choose a fresh Label.'}
$before=@(Get-SaveInventory $saveDirectory)
if(@($before|Where-Object{
    [IO.Path]::GetFileName($_.RelativePath) -ceq ($profile+'.sav') -or
    [IO.Path]::GetFileName($_.RelativePath) -ceq ($keys+'.sav') -or
    [IO.Path]::GetFileName($_.RelativePath).StartsWith($prefix+'_',[StringComparison]::OrdinalIgnoreCase)
}).Count -gt 0){throw 'Fresh-label QA save collision; no files have been deleted or reset.'}

$exe='D:\Program Files\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
if(-not(Test-Path -LiteralPath $exe -PathType Leaf)){throw 'Installed UE 5.8 editor executable is missing.'}
$arguments=@(('"'+$projectFile+'"'),'/Game/Rogue10m/Maps/L_Menu',
    '-game','-d3d11','-windowed','-ForceRes','-ResX=1280','-ResY=720',
    '-unattended','-nosplash','-NoLiveCoding','-NoRemoteShaderCompile',
    ('-RogueProfileSaveSlot='+$profile),('-RogueProgressionSavePrefix='+$prefix),('-RogueSkillKeySaveSlot='+$keys),
    ('-RogueBasicRunLabel='+$Label),('-RogueBasicRunOutput="'+$driverOutput+'"'),('-RogueBasicRunMaxSeconds='+$MaxSeconds),
    '-ExecCmds="t.MaxFPS 60,Rogue10m.TestBasicRunPlay"',('-abslog="'+$log+'"'))
if($PreparedBossStrategy){$arguments+='-RogueBasicRunPreparedBoss'}
if($AnimationEvidence){$arguments+='-RogueBasicRunAnimationEvidence'}
$windowStyle='Hidden'
if($Visible){$windowStyle='Normal'}else{$arguments+='-RenderOffscreen'}
if(-not(Test-Path -LiteralPath $qaRoot)){New-Item -ItemType Directory -Path $qaRoot|Out-Null}
$null=Assert-PhysicalDirectory $qaRoot
New-Item -ItemType Directory -Path $session|Out-Null
$null=Assert-PhysicalDirectory $session
Save-Json (Join-Path $session 'before-saves.json') $before
$metadata=[ordered]@{
    Schema='Rogue10m.BasicRunLauncher.v1'; Label=$Label; ProjectRoot=$ProjectRoot
    StartedUtc=[DateTime]::UtcNow.ToString('o'); State='prepared'; Visible=[bool]$Visible
    MaxSeconds=$MaxSeconds; WatchdogSeconds=($MaxSeconds+45); Executable=$exe; Arguments=$arguments
    ProfileSlot=$profile; ProgressionPrefix=$prefix; KeySlot=$keys; DriverOutput=$driverOutput
    PhysicalInput='not_performed'; GameplayShortcuts='none_added_by_launcher'; PreparedBossStrategy=[bool]$PreparedBossStrategy; AnimationEvidence=[bool]$AnimationEvidence
    BeforeSaveCount=$before.Count; OwnedProcess=$null; ProcessExitConfirmed=$false; ProcessExitCode=$null
    WatchdogStopAttempted=$false; WatchdogStopVerified=$false; Preservation='pending'; Error=$null
}
Save-Json (Join-Path $session 'session.json') $metadata
Save-Json (Join-Path $session 'manifest.json') ([ordered]@{
    Schema='Rogue10m.BasicRunLaunchManifest.v1';Label=$Label;ProjectRoot=$ProjectRoot;ProjectFile=$projectFile
    ProjectFileSHA256=(Get-FileHash -LiteralPath $projectFile -Algorithm SHA256).Hash
    Executable=$exe;Arguments=$arguments;MaxSeconds=$MaxSeconds;WatchdogSeconds=($MaxSeconds+45)
    ProfileSlot=$profile;ProgressionPrefix=$prefix;KeySlot=$keys;DriverOutput=$driverOutput
    SaveInventoryBefore='before-saves.json';SaveInventoryAfter='after-saves.json';Log='game.log'
    PhysicalInput='not_performed';NormalRoutePolicy='UI and driver-generated gameplay input only; no state injection or direct progression/pickup/travel APIs'
})
$process=$null
$launched=$false
$ownedId=0
$ownedStartTicks=0L
$ownedPath=''
$timedOut=$false
$exitStatus=1

try{
    # Final independent process check after evidence preparation; do not overlap another UE.
    $racingUE=@(Get-Process -Name 'UnrealEditor','UnrealEditor-Cmd','UnrealBuildTool','ShaderCompileWorker','CrashReportClient' -ErrorAction SilentlyContinue)
    if($racingUE.Count -gt 0){throw 'A UE process appeared during preflight; launch not performed.'}
    $process=Start-Process -FilePath $exe -ArgumentList $arguments -WindowStyle $windowStyle -PassThru
    $launched=$true
    $ownedId=$process.Id
    $ownedStartTicks=$process.StartTime.ToUniversalTime().Ticks
    $ownedPath=[IO.Path]::GetFullPath($process.Path)
    if($ownedPath -ine [IO.Path]::GetFullPath($exe)){throw 'Owned process path does not match the reviewed UE executable.'}
    $metadata.OwnedProcess=[ordered]@{PID=$ownedId;StartUtcTicks=$ownedStartTicks;Path=$ownedPath}
    $metadata.State='running'
    Save-Json (Join-Path $session 'session.json') $metadata
    Write-Output ('BASIC_RUN_LAUNCHED pid='+$ownedId+' evidence='+$session)
    $clock=[Diagnostics.Stopwatch]::StartNew()
    $nextNotice=30.0
    while(-not $process.WaitForExit(1000)){
        if($clock.Elapsed.TotalSeconds -ge ($MaxSeconds+45)){$timedOut=$true; throw 'Owned basic-run process exceeded its bounded watchdog.'}
        if($clock.Elapsed.TotalSeconds -ge $nextNotice){
            Write-Output ('BASIC_RUN_RUNNING pid='+$ownedId+' elapsed_seconds='+[int]$clock.Elapsed.TotalSeconds)
            $nextNotice+=30.0
        }
    }
}catch{
    $metadata.Error=$_.Exception.Message
    $metadata.State='attention_required'
}finally{
    if($launched -and $null -ne $process){
        try{
            $process.Refresh()
            if(-not $process.HasExited){
                # Never enumerate/kill arbitrary UE workers. Revalidate our exact original process.
                $live=Get-Process -Id $ownedId -ErrorAction Stop
                $sameOwner=($live.Id -eq $ownedId -and $live.StartTime.ToUniversalTime().Ticks -eq $ownedStartTicks -and
                    [IO.Path]::GetFullPath($live.Path) -ieq $ownedPath -and $ownedPath -ieq [IO.Path]::GetFullPath($exe))
                if(-not $sameOwner){throw 'Process ownership changed or cannot be proved; no process was killed.'}
                $metadata.WatchdogStopAttempted=$true
                $metadata.State='owned_process_stop_pending'
                Save-Json (Join-Path $session 'watchdog-stop.json') ([ordered]@{
                    AtUtc=[DateTime]::UtcNow.ToString('o');TimedOut=$timedOut;PID=$ownedId;StartUtcTicks=$ownedStartTicks;Path=$ownedPath
                    DriverSummary=(Join-Path $driverOutput 'summary.txt');GameLog=$log
                    EvidencePolicy='existing driver csv/summary/log retained; no deletion or game-state repair'
                })
                Save-Json (Join-Path $session 'session.json') $metadata
                $process.Kill()
                $metadata.WatchdogStopVerified=$process.WaitForExit(5000)
            }
            $process.Refresh()
            $metadata.ProcessExitConfirmed=$process.HasExited
            if($process.HasExited){$metadata.ProcessExitCode=$process.ExitCode}
        }catch{
            $metadata.Error=($metadata.Error+'; '+$_.Exception.Message).Trim('; ')
            $metadata.ProcessExitConfirmed=$false
            # The process may have exited naturally between HasExited and identity inspection.
            try{
                $process.Refresh()
                if($process.HasExited){$metadata.ProcessExitConfirmed=$true;$metadata.ProcessExitCode=$process.ExitCode}
            }catch{}
        }
    }else{
        # No launch means there is no owned process to stop; still verify unchanged saves.
        $metadata.ProcessExitConfirmed=$true
    }

    if($metadata.ProcessExitConfirmed){
        try{
            $after=@(Get-SaveInventory $saveDirectory)
            Save-Json (Join-Path $session 'after-saves.json') $after
            $beforeMap=@{}; foreach($row in $before){$beforeMap[$row.RelativePath]=$row}
            $afterMap=@{}; foreach($row in $after){$afterMap[$row.RelativePath]=$row}
            $changed=New-Object 'System.Collections.Generic.List[object]'
            $missing=New-Object 'System.Collections.Generic.List[string]'
            foreach($row in $before){
                if(-not $afterMap.ContainsKey($row.RelativePath)){$missing.Add($row.RelativePath)}
                elseif($afterMap[$row.RelativePath].SHA256 -cne $row.SHA256){
                    $changed.Add([pscustomobject]@{RelativePath=$row.RelativePath;BeforeSHA256=$row.SHA256;AfterSHA256=$afterMap[$row.RelativePath].SHA256})
                }
            }
            $newAllowed=New-Object 'System.Collections.Generic.List[object]'
            $newUnknown=New-Object 'System.Collections.Generic.List[object]'
            $progressionPattern='^'+[regex]::Escape($prefix)+'_[0-9A-Fa-f]{32}_v5\.sav$'
            foreach($row in $after){
                if($beforeMap.ContainsKey($row.RelativePath)){continue}
                $isRootFile=([IO.Path]::GetFileName($row.RelativePath) -ceq $row.RelativePath)
                $isOwned=$isRootFile -and ($row.RelativePath -ceq ($profile+'.sav') -or
                    $row.RelativePath -ceq ($keys+'.sav') -or $row.RelativePath -cmatch $progressionPattern)
                if($isOwned){$newAllowed.Add($row)}else{$newUnknown.Add($row)}
            }
            $preserved=($changed.Count -eq 0 -and $missing.Count -eq 0)
            $comparison=[ordered]@{
                BeforeCount=$before.Count;AfterCount=$after.Count;OriginalBytesPreserved=$preserved
                ChangedOriginals=@($changed.ToArray());MissingOriginals=@($missing.ToArray())
                NewAllowedQASaves=@($newAllowed.ToArray());UnknownNewSaves=@($newUnknown.ToArray())
                UnknownSaveAction='flag_no_delete';AnyRestoreDeleteOrAttributeChange=$false
            }
            Save-Json (Join-Path $session 'save-comparison.json') $comparison
            $metadata.Preservation=if($preserved -and $newUnknown.Count -eq 0){'verified'}else{'attention_required_no_files_deleted_or_restored'}
            $metadata.AfterSaveCount=$after.Count
        }catch{
            $metadata.Preservation='uncertain_inventory_or_hash_error'
            $metadata.Error=($metadata.Error+'; '+$_.Exception.Message).Trim('; ')
        }
    }else{
        $metadata.Preservation='uncertain_process_still_running_or_identity_unproved'
        $metadata.State='uncertain'
    }

    try{
        $summary=Read-DriverSummary (Join-Path $driverOutput 'summary.txt') ([bool]$PreparedBossStrategy) ([bool]$AnimationEvidence)
    }catch{
        $summary=[pscustomobject]@{Values=[ordered]@{};Issues=@('driver_summary_read_error:'+ $_.Exception.Message);Valid=$false}
        $metadata.Error=($metadata.Error+'; '+$_.Exception.Message).Trim('; ')
    }
    Save-Json (Join-Path $session 'driver-summary-parsed.json') $summary
    $metadata.DriverSummary=$summary.Values
    $metadata.DriverSummaryIssues=$summary.Issues
    $metadata.FinishedUtc=[DateTime]::UtcNow.ToString('o')
    $observed=$launched -and $metadata.ProcessExitConfirmed -and $metadata.ProcessExitCode -eq 0 -and
        $summary.Valid -and $metadata.Preservation -ceq 'verified' -and -not $timedOut
    if($observed){
        $metadata.State='recorded'
        if($summary.Values['outcome'] -ceq 'blocked'){$exitStatus=1}else{$exitStatus=0}
    }elseif($metadata.State -cne 'uncertain'){$metadata.State='attention_required'}
    Save-Json (Join-Path $session 'session.json') $metadata
    # Do not turn a death/timeout/blocked run or exit code 0 into a boss-completion claim.
    Write-Output ('BASIC_RUN_RECORDED outcome='+$summary.Values['outcome']+' full_boss_completion='+$summary.Values['full_boss_completion']+
        ' physical_input='+$summary.Values['physical_input']+' preservation='+$metadata.Preservation+' state='+$metadata.State+' evidence='+$session)
}
exit $exitStatus
