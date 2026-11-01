param([Parameter(Mandatory=$true)][ValidatePattern('^[A-Za-z0-9_]{1,20}$')][string]$Label)
$ErrorActionPreference='Stop'
$projectRoot='D:\Project\Rogue10m'
$exe='D:\Program Files\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$saveRoot=Join-Path $projectRoot 'Saved\SaveGames'
$output=Join-Path $projectRoot ('DevLog\LobbyTraitDisplayQA\'+$Label)
$driverOutput=Join-Path $output 'Driver'
function Physical([string]$Path){
    $entry=Get-Item -LiteralPath $Path -Force
    while($null -ne $entry){
        if(($entry.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0){throw ('Linked path is not allowed: '+$entry.FullName)}
        $entry=$entry.Parent
    }
}
function Json([string]$Path,$Value){[IO.File]::WriteAllText($Path,(ConvertTo-Json -InputObject $Value -Depth 10),[Text.UTF8Encoding]::new($false))}
function Inventory{
    $rows=@(Get-ChildItem -LiteralPath $saveRoot -Recurse -Force | ForEach-Object {
        if(($_.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0){throw 'Linked save entry; no QA launch.'}
        if(-not $_.PSIsContainer -and $_.Extension -ieq '.sav'){
            [pscustomobject]@{RelativePath=$_.FullName.Substring($saveRoot.Length+1);Bytes=$_.Length;SHA256=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash}
        }
    })
    return @($rows | Sort-Object RelativePath)
}
Physical $projectRoot;Physical (Join-Path $projectRoot 'Content');Physical $saveRoot;Physical (Join-Path $projectRoot 'DevLog')
if(Test-Path -LiteralPath $output){throw 'Existing evidence must be preserved; choose a fresh label.'}
if(Get-Process -Name 'UnrealEditor','UnrealEditor-Cmd','UnrealBuildTool','ShaderCompileWorker','CrashReportClient' -ErrorAction SilentlyContinue){throw 'Coordinate existing UE-related processes first.'}
$profile='Rogue10m_Town_QA_LobbyDisplay_'+$Label
$prefix='Rogue10m_Progression_QA_LobbyDisplay_'+$Label
$keys='Rogue10m_Fighter_QA_LobbyDisplay_'+$Label
$before=@(Inventory)
if(@($before | Where-Object { $_.RelativePath -ceq ($profile+'.sav') -or $_.RelativePath -ceq ($keys+'.sav') -or $_.RelativePath.StartsWith($prefix+'_',[StringComparison]::OrdinalIgnoreCase) }).Count){throw 'QA save namespace collision; no deletion or reset.'}
$parent=Split-Path $output -Parent
if(Test-Path -LiteralPath $parent){Physical $parent}else{New-Item -ItemType Directory -Path $parent | Out-Null}
New-Item -ItemType Directory -Path $output | Out-Null
Physical $output
Json (Join-Path $output 'before-saves.json') $before
$arguments=@(('"'+(Join-Path $projectRoot 'Rogue10m.uproject')+'"'),'/Game/Rogue10m/Maps/StartHub','-game','-d3d11','-RenderOffscreen','-windowed','-ForceRes','-ResX=1280','-ResY=720','-unattended','-nosplash','-NoLiveCoding','-NoRemoteShaderCompile',('-RogueProfileSaveSlot='+$profile),('-RogueProgressionSavePrefix='+$prefix),('-RogueSkillKeySaveSlot='+$keys),('-RogueLobbyTraitDisplayLabel='+$Label),('-RogueLobbyTraitDisplayOutput="'+$driverOutput+'"'),'-ExecCmds="Rogue10m.TestLobbyTraitDisplay"',('-abslog="'+$output+'\game.log"'))
$record=[ordered]@{Label=$Label;Scope='display_fixture_only';PhysicalInput='not_performed';StartedUtc=[DateTime]::UtcNow.ToString('o');OwnedPID=$null;ProcessExitConfirmed=$false;ProcessExitCode=$null;Preservation='pending';Result='pending';Error=$null}
Json (Join-Path $output 'session.json') $record
$process=$null
try{
    if(Get-Process -Name 'UnrealEditor','UnrealEditor-Cmd','UnrealBuildTool','ShaderCompileWorker','CrashReportClient' -ErrorAction SilentlyContinue){throw 'UE process appeared during preflight; no launch.'}
    $process=Start-Process -FilePath $exe -ArgumentList $arguments -WindowStyle Hidden -PassThru
    $record.OwnedPID=$process.Id
    Json (Join-Path $output 'session.json') $record
    Write-Output ('LOBBY_DISPLAY_LAUNCHED pid='+$process.Id+' output='+$output)
    if(-not $process.WaitForExit(180000)){throw 'Owned display fixture exceeded its180-second watchdog.'}
    if($process.ExitCode -ne 0){throw ('Display fixture exited='+$process.ExitCode)}
}catch{
    $record.Error=$_.Exception.Message
}finally{
    if($null -ne $process){
        if(-not $process.HasExited){$process.Kill();$process.WaitForExit(5000) | Out-Null}
        $record.ProcessExitConfirmed=$process.HasExited
        if($process.HasExited){$record.ProcessExitCode=$process.ExitCode}
    }else{$record.ProcessExitConfirmed=$true}
    if($record.ProcessExitConfirmed){
        $after=@(Inventory)
        Json (Join-Path $output 'after-saves.json') $after
        $differences=@(Compare-Object $before $after -Property RelativePath,SHA256,Bytes)
        Json (Join-Path $output 'save-comparison.json') ([ordered]@{Before=$before.Count;After=$after.Count;Differences=$differences;NoRestoreOrDeletePerformed=$true})
        $record.Preservation=if($differences.Count -eq 0){'verified'}else{'attention_required_no_restore_or_delete'}
    }
    $summaryPath=Join-Path $driverOutput 'summary.txt'
    if($record.ProcessExitConfirmed -and (Test-Path -LiteralPath $summaryPath)){
        $summary=@{}
        foreach($line in [IO.File]::ReadAllLines($summaryPath)){
            $at=$line.IndexOf('=');if($at -lt 1){continue}
            $key=$line.Substring(0,$at);if($summary.ContainsKey($key)){throw 'Duplicate summary key.'};$summary[$key]=$line.Substring($at+1)
        }
        $record.Summary=$summary
        $casesPath=Join-Path $driverOutput 'cases.csv'
        $coverage=$false
        if(Test-Path -LiteralPath $casesPath){
            $cases=@(Import-Csv -LiteralPath $casesPath -Encoding UTF8)
            $expected=@(@{Name='highest20_budget20';Locked='0';Allowed='1'},@{Name='highest1_budget20';Locked='1';Allowed='0'},@{Name='highest20_budget0';Locked='0';Allowed='0'})
            $coverage=$cases.Count -eq $expected.Count
            foreach($item in $expected){
                $row=@($cases | Where-Object case -CEQ $item.Name)
                if($row.Count -ne 1 -or $row[0].card_locked -cne $item.Locked -or $row[0].backend_allowed -cne $item.Allowed -or $row[0].button_enabled -cne $item.Allowed){$coverage=$false}
            }
        }
        $record.ExactCaseCoverage=$coverage
        if($record.Error -eq $null -and $record.ProcessExitCode -eq 0 -and $record.Preservation -eq 'verified' -and $coverage -and $summary['checks'] -match '^[1-9][0-9]*$' -and $summary['failures'] -ceq '0' -and $summary['scope'] -ceq 'display_fixture_only' -and $summary['profile_mutation'] -ceq 'none' -and $summary['physical_input'] -ceq 'not_performed' -and $summary['cleanup'] -ceq 'restored_original_Growth_Town_budget_highest_owned_NPC_widget_only'){$record.Result='passed'}else{$record.Result='attention_required'}
    }else{$record.Result='attention_required'}
    Json (Join-Path $output 'session.json') $record
}
Write-Output ('LOBBY_DISPLAY_RESULT='+$record.Result+' preservation='+$record.Preservation+' output='+$output)
if($record.Result -ne 'passed'){exit 1}

exit 0
