param(
    [Parameter(Mandatory=$true)][ValidatePattern('^[A-Za-z0-9_]{1,20}$')][string]$Label,
    [Parameter(Mandatory=$true)][ValidateSet('BasicRun1101H','BasicRun1101I')][string]$SourceLabel,
    [ValidateRange(120,600)][int]$MaxSeconds=300,
    [switch]$Visible,
    [string]$ProjectRoot='D:\Project\Rogue10m'
)
# DRAFT ONLY: writing/parsing this file does not authorize or perform a clone or launch.
# No reset, overwrite, delete, save-byte edit, attribute edit, reward grant or legacy import.
$ErrorActionPreference='Stop'

# Startup observation only. No process is stopped by this function.
function Wait-OwnedStartupIdentity([Diagnostics.Process]$Process,[int]$ExpectedId,[string]$ExpectedExecutable){
    $expected=[IO.Path]::GetFullPath($ExpectedExecutable)
    $clock=[Diagnostics.Stopwatch]::StartNew();$lastError='path_or_start_time_not_yet_available'
    while($clock.Elapsed.TotalMilliseconds -lt 5000){
        try{
            $Process.Refresh()
            if($Process.Id -ne $ExpectedId){throw 'The original process object changed identity.'}
            if($Process.HasExited){
                return [pscustomobject]@{Confirmed=$false;PID=$ExpectedId;StartUtcTicks=$null;Path=$null;ExitedDuringStartup=$true;WaitMilliseconds=[int]$clock.Elapsed.TotalMilliseconds;Error='owned_process_exited_before_identity_observed'}
            }
            $candidatePath=$Process.Path
            $candidateTicks=$Process.StartTime.ToUniversalTime().Ticks
            if(-not [string]::IsNullOrWhiteSpace($candidatePath) -and $candidateTicks -gt 0){
                $candidatePath=[IO.Path]::GetFullPath($candidatePath)
                if($candidatePath -ine $expected){
                    return [pscustomobject]@{Confirmed=$false;PID=$ExpectedId;StartUtcTicks=$candidateTicks;Path=$candidatePath;ExitedDuringStartup=$false;WaitMilliseconds=[int]$clock.Elapsed.TotalMilliseconds;Error='startup_executable_path_mismatch_no_stop_permitted'}
                }
                return [pscustomobject]@{Confirmed=$true;PID=$ExpectedId;StartUtcTicks=$candidateTicks;Path=$candidatePath;ExitedDuringStartup=$false;WaitMilliseconds=[int]$clock.Elapsed.TotalMilliseconds;Error=$null}
            }
        }catch{$lastError=$_.Exception.Message}
        $remaining=5000-[int]$clock.Elapsed.TotalMilliseconds
        if($remaining -gt 0){Start-Sleep -Milliseconds ([Math]::Min(100,$remaining))}
    }
    return [pscustomobject]@{Confirmed=$false;PID=$ExpectedId;StartUtcTicks=$null;Path=$null;ExitedDuringStartup=$false;WaitMilliseconds=[int]$clock.Elapsed.TotalMilliseconds;Error=('startup_identity_not_confirmed_no_stop_permitted: '+$lastError)}
}

function Require([bool]$Condition,[string]$Reason){if(-not $Condition){throw $Reason}}
function Full-Path([string]$Path){return [IO.Path]::GetFullPath($Path).TrimEnd('\')}
function Physical-Directory([string]$Path){
    $item=Get-Item -LiteralPath $Path -Force
    Require $item.PSIsContainer ('Missing directory: '+$Path)
    $cursor=$item
    while($null -ne $cursor){
        Require (($cursor.Attributes -band [IO.FileAttributes]::ReparsePoint) -eq 0) ('Linked directory refused: '+$cursor.FullName)
        $cursor=$cursor.Parent
    }
    return Full-Path $item.FullName
}
function Physical-File([string]$Path){
    $item=Get-Item -LiteralPath $Path -Force
    Require (-not $item.PSIsContainer) ('File expected: '+$Path)
    Require (($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -eq 0) ('Linked file refused: '+$Path)
    $null=Physical-Directory $item.DirectoryName
    return $item.FullName
}
function Read-Json([string]$Path){return ([IO.File]::ReadAllText((Physical-File $Path))|ConvertFrom-Json)}
function Write-Json([string]$Path,$Value){[IO.File]::WriteAllText($Path,(ConvertTo-Json -InputObject $Value -Depth 16),[Text.UTF8Encoding]::new($false))}
function Hash-File([string]$Path){return (Get-FileHash -LiteralPath (Physical-File $Path) -Algorithm SHA256).Hash}
function Read-KV([string]$Path){
    $map=[ordered]@{}
    foreach($line in [IO.File]::ReadAllLines((Physical-File $Path))){
        $at=$line.IndexOf('=');if($at -lt 1){continue}
        $key=$line.Substring(0,$at).Trim();$value=$line.Substring($at+1).Trim()
        Require (-not $map.Contains($key)) ('Duplicate summary key: '+$key)
        $map[$key]=$value
    }
    return $map
}
function Save-Inventory([string]$Directory){
    $base=Physical-Directory $Directory
    $pending=New-Object 'System.Collections.Generic.Stack[string]';$pending.Push($base)
    $rows=New-Object 'System.Collections.Generic.List[object]'
    while($pending.Count -gt 0){
        foreach($entry in @(Get-ChildItem -LiteralPath $pending.Pop() -Force)){
            Require (($entry.Attributes -band [IO.FileAttributes]::ReparsePoint) -eq 0) ('Linked save entry refused: '+$entry.FullName)
            if($entry.PSIsContainer){$pending.Push($entry.FullName);continue}
            if($entry.Extension -ieq '.sav'){$rows.Add([pscustomobject][ordered]@{
                RelativePath=$entry.FullName.Substring($base.Length+1);Bytes=$entry.Length
                SHA256=(Get-FileHash -LiteralPath $entry.FullName -Algorithm SHA256).Hash
                LastWriteUtc=$entry.LastWriteTimeUtc.ToString('o')
            })}
        }
    }
    return @($rows.ToArray()|Sort-Object RelativePath)
}
function Assert-ExactArgument($Arguments,[string]$Name,[string]$Expected){
    $values=@($Arguments|Where-Object{$_ -is [string] -and $_.StartsWith($Name+'=',[StringComparison]::Ordinal)})
    Require ($values.Count -eq 1 -and $values[0] -ceq ($Name+'='+$Expected)) ('Source argument mismatch: '+$Name)
}
function Source-Proof([string]$Root,[string]$SessionLabel){
    Require ($SessionLabel -cin @('BasicRun1101H','BasicRun1101I')) 'Only the two specifically reviewed closed BasicRun QA sources are allowed.'
    $pins=@{
        BasicRun1101H=@{Profile='6D6E889F488B6018AD63EA9FC88E1496';TownSHA='D71305E8B55D6458E7B8CC5192FDDA1448644FDC3FAFF2F70364E8E1AC06CE37';ProgressionSHA='85F37A2321368062A22D8E5A066B6AF219D00521FDA59BA33AAD5527F69879BF'}
        BasicRun1101I=@{Profile='A76E3E60414F23F030287DB1123C6BB4';TownSHA='144E8B76A01099CD7F164C1071148E5466773C5464D8FCC8FEA4B54D584BC880';ProgressionSHA='359BEEAF3E2E83D945697D29A775E2370C8695D1468292CD754A061BCFB912C5'}
    }
    $pin=$pins[$SessionLabel]
    $folder=Physical-Directory (Join-Path $Root ('DevLog\BasicRunPlayQA\'+$SessionLabel))
    $paths=[ordered]@{
        Manifest=(Join-Path $folder 'manifest.json');Session=(Join-Path $folder 'session.json')
        Comparison=(Join-Path $folder 'save-comparison.json');Before=(Join-Path $folder 'before-saves.json')
        After=(Join-Path $folder 'after-saves.json');Summary=(Join-Path $folder 'Driver\summary.txt')
    }
    $manifest=Read-Json $paths.Manifest;$session=Read-Json $paths.Session;$comparison=Read-Json $paths.Comparison
    $old=@(Read-Json $paths.Before);$after=@(Read-Json $paths.After);$summary=Read-KV $paths.Summary
    $town='Rogue10m_Town_QA_BasicRun_'+$SessionLabel
    $prefix='Rogue10m_Progression_QA_BasicRun_'+$SessionLabel
    $keys='Rogue10m_Fighter_QA_BasicRun_'+$SessionLabel
    Require ($manifest.Schema -ceq 'Rogue10m.BasicRunLaunchManifest.v1' -and $session.Schema -ceq 'Rogue10m.BasicRunLauncher.v1') 'Source schema is not the reviewed BasicRun launcher.'
    foreach($doc in @($manifest,$session)){
        Require ($doc.Label -ceq $SessionLabel -and (Full-Path $doc.ProjectRoot) -ieq $Root) 'Source label/project mismatch.'
        Require ($doc.ProfileSlot -ceq $town -and $doc.ProgressionPrefix -ceq $prefix -and $doc.KeySlot -ceq $keys) 'Source QA namespaces mismatch.'
        Require ((Full-Path $doc.DriverOutput) -ieq (Full-Path (Join-Path $folder 'Driver'))) 'Source driver path mismatch.'
        Assert-ExactArgument $doc.Arguments '-RogueProfileSaveSlot' $town
        Assert-ExactArgument $doc.Arguments '-RogueProgressionSavePrefix' $prefix
        Assert-ExactArgument $doc.Arguments '-RogueSkillKeySaveSlot' $keys
    }
    Require ($session.State -ceq 'recorded' -and $session.ProcessExitConfirmed -is [bool] -and $session.ProcessExitConfirmed -and
        $session.ProcessExitCode -eq 0 -and $session.Preservation -ceq 'verified' -and $null -eq $session.Error -and
        $session.WatchdogStopAttempted -is [bool] -and -not $session.WatchdogStopAttempted) 'Source is not a verified naturally ended, preserved run.'
    # PowerShell 7 can deserialize ISO JSON timestamps directly to DateTime(Utc).
    # Do not stringify/reparse that value: doing so loses its UTC offset in a local timezone.
    $stamp=$session.FinishedUtc;$finished=[DateTimeOffset]::MinValue;$validStamp=$false
    if($stamp -is [DateTime] -and $stamp.Kind -ne [DateTimeKind]::Unspecified){
        $finished=[DateTimeOffset]$stamp;$validStamp=$true
    }elseif($stamp -is [DateTimeOffset]){
        $finished=$stamp;$validStamp=$true
    }elseif($stamp -is [string] -and $stamp -cmatch '^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}(?:\.\d{1,7})?(?:Z|[+-]\d{2}:\d{2})$'){
        $validStamp=[DateTimeOffset]::TryParse($stamp,[Globalization.CultureInfo]::InvariantCulture,[Globalization.DateTimeStyles]::None,[ref]$finished)
    }
    Require ($validStamp -and $finished.UtcDateTime -gt ([DateTimeOffset]'2026-11-01T00:00:00Z').UtcDateTime) 'Source completion timestamp is missing/invalid.'
    Require ($comparison.OriginalBytesPreserved -is [bool] -and $comparison.OriginalBytesPreserved -and
        @($comparison.ChangedOriginals).Count -eq 0 -and @($comparison.MissingOriginals).Count -eq 0 -and
        @($comparison.UnknownNewSaves).Count -eq 0 -and @($session.DriverSummaryIssues).Count -eq 0) 'Source save preservation evidence is incomplete.'
    Require ($old.Count -eq $comparison.BeforeCount -and $after.Count -eq $comparison.AfterCount -and
        $old.Count -eq $session.BeforeSaveCount -and $after.Count -eq $session.AfterSaveCount) 'Source save inventory counts do not match the session.'
    Require ($summary['profile'] -ceq $pin.Profile -and $session.DriverSummary.profile -ceq $pin.Profile -and
        $summary['outcome'] -ceq 'returned_death' -and $session.DriverSummary.outcome -ceq 'returned_death' -and
        $summary['returned_reset_observed'] -ceq '1' -and $summary['no_gameplay_fixtures'] -ceq '1' -and
        $summary['physical_input'] -ceq 'not_performed' -and $summary['full_boss_completion'] -ceq '0') 'Source profile/outcome/normal-return proof differs from the reviewed H/I run.'
    $guid=[guid]::Empty;Require ([guid]::TryParseExact($pin.Profile,'N',[ref]$guid) -and $guid -ne [guid]::Empty) 'Pinned source profile GUID is invalid.'
    $townFile=$town+'.sav';$progressionFile=$prefix+'_'+$pin.Profile+'_v5.sav'
    $expected=@{profile=@{Name=$townFile;SHA=$pin.TownSHA};progression=@{Name=$progressionFile;SHA=$pin.ProgressionSHA}}
    $allowed=@($comparison.NewAllowedQASaves)
    Require ($allowed.Count -eq 2) 'Reviewed H/I sources have exactly two NewAllowedQASaves; do not infer or synthesize a fighter-key save.'
    Require (@($old|Where-Object{[IO.Path]::GetFileName($_.RelativePath) -ceq $townFile -or
        [IO.Path]::GetFileName($_.RelativePath) -ceq ($keys+'.sav') -or
        [IO.Path]::GetFileName($_.RelativePath).StartsWith($prefix+'_',[StringComparison]::OrdinalIgnoreCase)}).Count -eq 0) 'Source namespace was not fresh in its before inventory.'
    $records=New-Object 'System.Collections.Generic.List[object]'
    foreach($role in @('profile','progression')){
        $spec=$expected[$role];$rows=@($allowed|Where-Object{$_.RelativePath -ceq $spec.Name})
        $diskRows=@($after|Where-Object{$_.RelativePath -ceq $spec.Name})
        Require ($rows.Count -eq 1 -and $diskRows.Count -eq 1 -and $rows[0].SHA256 -ceq $spec.SHA -and
            $diskRows[0].SHA256 -ceq $spec.SHA -and $rows[0].Bytes -gt 0 -and $rows[0].Bytes -eq $diskRows[0].Bytes) 'Source NewAllowedQASaves/after inventory/pinned bytes disagree.'
        $records.Add([pscustomobject]@{Role=$role;SourceRelativePath=$spec.Name;Bytes=[long]$rows[0].Bytes;SourceSHA256=$spec.SHA})
    }
    $hashes=[ordered]@{};foreach($key in $paths.Keys){$hashes[$key]=Hash-File $paths[$key]}
    return [pscustomobject]@{Label=$SessionLabel;Folder=$folder;Profile=$pin.Profile;Town=$town;Prefix=$prefix;Keys=$keys
        CompletedUtc=$session.FinishedUtc;Records=@($records.ToArray());MetadataPaths=$paths;MetadataHashes=$hashes}
}

Require ($Label -cmatch '^[A-Za-z0-9_]{1,20}$') 'Fresh destination Label must be 1..20 ASCII letters/digits/underscores.'
$root=Full-Path 'D:\Project\Rogue10m';Require ((Full-Path $ProjectRoot) -ieq $root) 'Only the reviewed D: project is supported.'
$root=Physical-Directory $root;$null=Physical-Directory (Join-Path $root 'Content')
$saveRoot=Physical-Directory (Join-Path $root 'Saved\SaveGames');$null=Physical-Directory (Join-Path $root 'DevLog')
$project=Physical-File (Join-Path $root 'Rogue10m.uproject')
$source=Source-Proof $root $SourceLabel
$profile='Rogue10m_Town_QA_LobbyUpgrade_'+$Label
$prefix='Rogue10m_Progression_QA_LobbyUpgrade_'+$Label
$keys='Rogue10m_Fighter_QA_LobbyUpgrade_'+$Label
$qaRoot=Join-Path $root 'DevLog\LobbyUpgradeRoundTripQA';if(Test-Path -LiteralPath $qaRoot){$null=Physical-Directory $qaRoot}
$sessionRoot=Join-Path $qaRoot $Label;$driver=Join-Path $sessionRoot 'Driver'
Require (-not(Test-Path -LiteralPath $sessionRoot)) 'Destination session exists; preserve it and choose a new Label.'
$before=@(Save-Inventory $saveRoot);Require ($before.Count -ge 342) 'Current preservation baseline has fewer than the known 342 save files; investigate before cloning.'
Require (@(Get-ChildItem -LiteralPath $saveRoot -File -Recurse -Force|Where-Object{
    $n=$_.Name
    $n.StartsWith($profile,[StringComparison]::OrdinalIgnoreCase) -or
    $n.StartsWith($keys,[StringComparison]::OrdinalIgnoreCase) -or $n.StartsWith($prefix,[StringComparison]::OrdinalIgnoreCase)
}).Count -eq 0) 'Destination QA prefix already exists; no overwrite/reset is permitted.'
$beforeMap=@{};foreach($r in $before){$beforeMap[$r.RelativePath]=$r}
foreach($r in $source.Records){Require ($beforeMap.ContainsKey($r.SourceRelativePath) -and
    $beforeMap[$r.SourceRelativePath].SHA256 -ceq $r.SourceSHA256 -and $beforeMap[$r.SourceRelativePath].Bytes -eq $r.Bytes) 'Current source QA save bytes differ from the reviewed closed run.'}
Require (-not $beforeMap.ContainsKey($source.Keys+'.sav')) 'Unlisted source fighter-key save exists; this source is no longer the reviewed two-file clone.'
$busy=@(Get-Process -Name 'UnrealEditor','UnrealEditor-Cmd','UnrealBuildTool','ShaderCompileWorker','CrashReportClient' -ErrorAction SilentlyContinue)
Require ($busy.Count -eq 0) 'Another UE-related process is active; coordinate without killing it.'
$exe=Physical-File 'D:\Program Files\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
if(-not(Test-Path -LiteralPath $qaRoot)){New-Item -ItemType Directory -Path $qaRoot|Out-Null}
$null=Physical-Directory $qaRoot;New-Item -ItemType Directory -Path $sessionRoot|Out-Null
$null=Physical-Directory $sessionRoot
Write-Json (Join-Path $sessionRoot 'before-saves.json') $before
$metadata=[ordered]@{Schema='Rogue10m.LobbyUpgradeLauncher.v1';Label=$Label;SourceLabel=$SourceLabel;ExpectedProfile=$source.Profile
    State='prepared';StartedUtc=[DateTime]::UtcNow.ToString('o');ProjectRoot=$root;ProfileSlot=$profile;ProgressionPrefix=$prefix;KeySlot=$keys
    DriverOutput=$driver;MaxSeconds=$MaxSeconds;WatchdogSeconds=($MaxSeconds+45);Visible=[bool]$Visible
    CloneVerified=$false;OwnedProcess=$null;ProcessExitConfirmed=$false;ProcessExitCode=$null;Preservation='pending';Error=$null
    PhysicalInput='not_performed';SourceBytesEdited=$false;ExistingSaveRestoreDeleteOrAttributeChange=$false}
Write-Json (Join-Path $sessionRoot 'session.json') $metadata
$process=$null;$launched=$false;$ownedId=0;$ownedTicks=0L;$ownedPath='';$identityConfirmed=$false;$timedOut=$false;$exitStatus=2
try{
    $cloneRows=New-Object 'System.Collections.Generic.List[object]'
    foreach($r in $source.Records){
        $destination=if($r.Role -ceq 'profile'){$profile+'.sav'}else{$prefix+'_'+$source.Profile+'_v5.sav'}
        $sourceFile=Physical-File (Join-Path $saveRoot $r.SourceRelativePath);$targetFile=Join-Path $saveRoot $destination
        Require (-not(Test-Path -LiteralPath $targetFile)) 'Destination appeared during preflight; no overwrite was performed.'
        # Byte copy only. false forbids overwriting; serialized profile GUID/data are not rewritten.
        [IO.File]::Copy($sourceFile,$targetFile,$false)
        $cloneHash=Hash-File $targetFile;$sourceHash=Hash-File $sourceFile
        Require ($cloneHash -ceq $r.SourceSHA256 -and $sourceHash -ceq $r.SourceSHA256 -and
            (Get-Item -LiteralPath $targetFile).Length -eq $r.Bytes) 'Initial byte-identical clone verification failed; retain partial namespace and stop.'
        $cloneRows.Add([pscustomobject][ordered]@{Role=$r.Role;SourceRelativePath=$r.SourceRelativePath;DestinationRelativePath=$destination
            Bytes=$r.Bytes;SourceSHA256=$r.SourceSHA256;InitialCloneSHA256=$cloneHash})
    }
    foreach($key in $source.MetadataPaths.Keys){Require ((Hash-File $source.MetadataPaths[$key]) -ceq $source.MetadataHashes[$key]) 'Closed source evidence changed during clone preparation.'}
    $cloneManifest=[ordered]@{Schema='Rogue10m.LobbyUpgradeClone.v1';Label=$Label;SourceLabel=$SourceLabel;ExpectedProfile=$source.Profile
        ProfileSlot=$profile;ProgressionPrefix=$prefix;KeySlot=$keys;DriverOutput=$driver;CloneVerified=$true;KeyClonePresent=$false
        SourceCompletedUtc=$source.CompletedUtc;SourceSessionDir=$source.Folder;SourceRecords=@($cloneRows.ToArray())
        SourceMetadataHashes=$source.MetadataHashes;CloneCompletedUtc=[DateTime]::UtcNow.ToString('o')
        ByteIdentityScope='initial clone before UE launch only; normal game serialization may subsequently change destination QA bytes'}
    $clonePath=Join-Path $sessionRoot 'clone-manifest.json';Write-Json $clonePath $cloneManifest
    $metadata.CloneVerified=$true;$metadata.CloneManifestSHA256=Hash-File $clonePath
    $arguments=@(('"'+$project+'"'),'/Game/Rogue10m/Maps/L_Menu','-game','-d3d11','-windowed','-ForceRes','-ResX=1280','-ResY=720',
        '-unattended','-nosplash','-NoLiveCoding','-NoRemoteShaderCompile',
        ('-RogueProfileSaveSlot='+$profile),('-RogueProgressionSavePrefix='+$prefix),('-RogueSkillKeySaveSlot='+$keys),
        ('-RogueLobbyUpgradeLabel='+$Label),('-RogueLobbyUpgradeSourceLabel='+$SourceLabel),('-RogueLobbyUpgradeExpectedProfile='+$source.Profile),
        ('-RogueLobbyUpgradeOutput="'+$driver+'"'),('-RogueLobbyUpgradeMaxSeconds='+$MaxSeconds),
        '-ExecCmds="t.MaxFPS 60,Rogue10m.TestLobbyUpgradeRoundTrip"',('-abslog="'+(Join-Path $sessionRoot 'game.log')+'"'))
    $window='Hidden';if($Visible){$window='Normal'}else{$arguments+='-RenderOffscreen'}
    $metadata.Arguments=$arguments;$metadata.Executable=$exe;Write-Json (Join-Path $sessionRoot 'manifest.json') $metadata
    $racing=@(Get-Process -Name 'UnrealEditor','UnrealEditor-Cmd','UnrealBuildTool','ShaderCompileWorker','CrashReportClient' -ErrorAction SilentlyContinue)
    Require ($racing.Count -eq 0) 'Another UE process appeared after cloning; clone preserved and launch refused.'
    $process=Start-Process -FilePath $exe -ArgumentList $arguments -WindowStyle $window -PassThru;$launched=$true
    $ownedId=$process.Id
    Write-Output ('LOBBY_UPGRADE_STARTED pid='+$ownedId+' identity=pending')
    $metadata.OwnedProcess=[ordered]@{PID=$ownedId;StartUtcTicks=$null;Path=$null;IdentityConfirmed=$false}
    $metadata.State='startup_identity_pending';Write-Json (Join-Path $sessionRoot 'session.json') $metadata
    $identity=Wait-OwnedStartupIdentity $process $ownedId $exe
    $metadata.StartupIdentity=$identity
    Write-Json (Join-Path $sessionRoot 'session.json') $metadata
    Require $identity.Confirmed $identity.Error
    $ownedTicks=[long]$identity.StartUtcTicks;$ownedPath=$identity.Path;$identityConfirmed=$true
    $metadata.OwnedProcess=[ordered]@{PID=$ownedId;StartUtcTicks=$ownedTicks;Path=$ownedPath;IdentityConfirmed=$true};$metadata.State='running'
    Write-Json (Join-Path $sessionRoot 'session.json') $metadata
    Write-Output ('LOBBY_UPGRADE_LAUNCHED pid='+$ownedId+' source='+$SourceLabel+' evidence='+$sessionRoot)
    $clock=[Diagnostics.Stopwatch]::StartNew();$notice=30.0
    while(-not $process.WaitForExit(1000)){
        if($clock.Elapsed.TotalSeconds -ge ($MaxSeconds+45)){$timedOut=$true;throw 'Owned process exceeded the bounded round-trip watchdog.'}
        if($clock.Elapsed.TotalSeconds -ge $notice){Write-Output ('LOBBY_UPGRADE_RUNNING pid='+$ownedId+' elapsed='+[int]$clock.Elapsed.TotalSeconds);$notice+=30.0}
    }
}catch{$metadata.Error=$_.Exception.Message;$metadata.State='attention_required'}
finally{
    if($launched -and $null -ne $process){
        try{
            $process.Refresh()
            if(-not $process.HasExited){
                Require $identityConfirmed 'Startup identity was not confirmed; no process was killed.'
                $live=Get-Process -Id $ownedId -ErrorAction Stop
                Require ($live.Id -eq $ownedId -and $live.StartTime.ToUniversalTime().Ticks -eq $ownedTicks -and
                    (Full-Path $live.Path) -ieq $ownedPath -and $ownedPath -ieq (Full-Path $exe)) 'Cannot prove exact process ownership; do not kill any process.'
                Write-Json (Join-Path $sessionRoot 'watchdog-stop.json') ([ordered]@{PID=$ownedId;StartUtcTicks=$ownedTicks;Path=$ownedPath
                    AtUtc=[DateTime]::UtcNow.ToString('o');TimedOut=$timedOut;Evidence='Driver csv/summary/game.log retained before owned-process stop'})
                $process.Kill();$null=$process.WaitForExit(5000)
            }
            $process.Refresh();$metadata.ProcessExitConfirmed=$process.HasExited
            if($process.HasExited){$metadata.ProcessExitCode=$process.ExitCode}
        }catch{
            $metadata.Error=($metadata.Error+'; '+$_.Exception.Message).Trim('; ');$metadata.ProcessExitConfirmed=$false
            try{$process.Refresh();if($process.HasExited){$metadata.ProcessExitConfirmed=$true;$metadata.ProcessExitCode=$process.ExitCode}}catch{}
        }
    }else{$metadata.ProcessExitConfirmed=$true}
    if($metadata.ProcessExitConfirmed){
        try{
            $after=@(Save-Inventory $saveRoot);Write-Json (Join-Path $sessionRoot 'after-saves.json') $after
            $afterMap=@{};foreach($r in $after){$afterMap[$r.RelativePath]=$r}
            $changed=New-Object 'System.Collections.Generic.List[object]';$missing=New-Object 'System.Collections.Generic.List[string]'
            foreach($r in $before){
                if(-not $afterMap.ContainsKey($r.RelativePath)){$missing.Add($r.RelativePath)}
                elseif($afterMap[$r.RelativePath].SHA256 -cne $r.SHA256){$changed.Add([pscustomobject]@{RelativePath=$r.RelativePath;BeforeSHA256=$r.SHA256;AfterSHA256=$afterMap[$r.RelativePath].SHA256})}
            }
            $allowedNames=@(($profile+'.sav'),($keys+'.sav'),($prefix+'_'+$source.Profile+'_v5.sav'))
            $newAllowed=New-Object 'System.Collections.Generic.List[object]';$unknown=New-Object 'System.Collections.Generic.List[object]'
            foreach($r in $after){if(-not $beforeMap.ContainsKey($r.RelativePath)){
                if($r.RelativePath -cin $allowedNames){$newAllowed.Add($r)}else{$unknown.Add($r)}
            }}
            $evidenceChanged=New-Object 'System.Collections.Generic.List[string]'
            foreach($key in $source.MetadataPaths.Keys){if((Hash-File $source.MetadataPaths[$key]) -cne $source.MetadataHashes[$key]){$evidenceChanged.Add($key)}}
            $preserved=($changed.Count -eq 0 -and $missing.Count -eq 0 -and $evidenceChanged.Count -eq 0)
            Write-Json (Join-Path $sessionRoot 'save-comparison.json') ([ordered]@{BeforeCount=$before.Count;AfterCount=$after.Count;OriginalBytesPreserved=$preserved
                ChangedOriginals=@($changed.ToArray());MissingOriginals=@($missing.ToArray());SourceMetadataChanged=@($evidenceChanged.ToArray())
                NewAllowedQASaves=@($newAllowed.ToArray());UnknownNewSaves=@($unknown.ToArray());UnknownSaveAction='flag_no_delete'
                AnyExistingSaveRestoreDeleteOrAttributeChange=$false;InitialCloneProof='clone-manifest.json'})
            $metadata.Preservation=if($preserved -and $unknown.Count -eq 0){'verified'}else{'attention_required_no_restore_or_delete'}
        }catch{$metadata.Preservation='uncertain_inventory_or_source_evidence_error';$metadata.Error=($metadata.Error+'; '+$_.Exception.Message).Trim('; ')}
    }else{$metadata.Preservation='uncertain_process_not_confirmed_stopped';$metadata.State='uncertain'}
    $issues=New-Object 'System.Collections.Generic.List[string]';$summary=[ordered]@{}
    try{
        $summary=Read-KV (Join-Path $driver 'summary.txt')
        if($summary['failures'] -notmatch '^\d+$'){$issues.Add('missing_or_invalid_failures')}
        if($summary['outcome'] -cnotin @('completed','blocked')){$issues.Add('missing_or_invalid_outcome')}
        if($summary['scope'] -cne 'normal_UI_upgrade_roundtrip_programmatic'){$issues.Add('scope_mismatch')}
        if($summary['physical_input'] -cne 'not_performed'){$issues.Add('physical_input_disclosure_mismatch')}
        if($summary['profile'] -cne $source.Profile -or $summary['source_label'] -cne $SourceLabel){$issues.Add('profile_or_source_mismatch')}
        if($summary['outcome'] -ceq 'completed'){
            foreach($key in @('normal_menu_ui','actual_NPC_service_ui','normal_power_upgrade','purchase_saved','next_run_reset_and_trait','abandon_UI','returned_hub','discovery_preserved','no_state_injection')){
                if($summary[$key] -cne '1'){$issues.Add('completed_milestone_missing:'+ $key)}
            }
        }
    }catch{$issues.Add('summary_read_error:'+ $_.Exception.Message)}
    $metadata.DriverSummary=$summary;$metadata.DriverSummaryIssues=@($issues.ToArray());$metadata.FinishedUtc=[DateTime]::UtcNow.ToString('o')
    $recorded=$launched -and $identityConfirmed -and $metadata.CloneVerified -and $metadata.ProcessExitConfirmed -and $metadata.ProcessExitCode -eq 0 -and [string]::IsNullOrEmpty($metadata.Error) -and
        -not $timedOut -and $metadata.Preservation -ceq 'verified' -and $issues.Count -eq 0 -and $summary['failures'] -ceq '0' -and $summary['outcome'] -ceq 'completed'
    if($recorded){$metadata.State='recorded';$exitStatus=0}elseif($metadata.State -cne 'uncertain'){$metadata.State='attention_required'}
    Write-Json (Join-Path $sessionRoot 'session.json') $metadata
    Write-Output ('LOBBY_UPGRADE_RECORDED outcome='+$summary['outcome']+' failures='+$summary['failures']+' source='+$SourceLabel+
        ' physical_input=not_performed preservation='+$metadata.Preservation+' state='+$metadata.State+' evidence='+$sessionRoot)
}
exit $exitStatus
