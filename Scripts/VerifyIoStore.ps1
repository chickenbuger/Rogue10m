#requires -Version 7.0
[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$PakDir,
    [Parameter(Mandatory=$true)][string]$CsvOut,
    [Parameter(Mandatory=$true)][string]$UnrealPakExe,
    [ValidateRange(10,3600)][int]$WallTimeoutSeconds=180
)
$ErrorActionPreference='Stop'

function Assert-NoReparseAncestor([string]$Path,[string]$Boundary) {
    $current=$Path
    while($current -and $current.Length -ge $Boundary.Length) {
        if(Test-Path -LiteralPath $current) {
            $item=Get-Item -LiteralPath $current -Force
            if($item.Attributes -band [IO.FileAttributes]::ReparsePoint){throw "Reparse output ancestor refused: $current"}
        }
        if($current.Equals($Boundary,[StringComparison]::OrdinalIgnoreCase)){break}
        $current=Split-Path -Parent $current
    }
}

function ConvertTo-PackageIdentity([string]$Value,[switch]$Filename) {
    if([string]::IsNullOrWhiteSpace($Value)){return $null}
    $value=$Value.Trim().Trim('"').Replace('\','/')
    $package=$null
    if($Filename) {
        # Exact project Content root, with only a legal relative or absolute prefix.
        $relative='^(?:\.\./)*Rogue10m/Content/(?<asset>.+)$'
        $absolute='^(?:[A-Za-z]:/|/)(?:[^/]+/)*Rogue10m/Content/(?<asset>.+)$'
        if($value -match $relative -or $value -match $absolute) {
            $asset=$Matches.asset
            if($asset -notmatch '\.(?:uasset|umap)$'){return $null}
            $package='/Game/'+($asset -replace '\.(?:uasset|umap)$','')
        } else {return $null}
    } else {
        # Long package names and explicit UE object paths only; no basename search.
        if($value -match "^(?:[A-Za-z_][A-Za-z0-9_./]*')?(?<asset>/Game/[^']+)'?$") {
            $package=$Matches.asset
            if($package -match '\.(?:uasset|umap)$') {
                $package=$package -replace '\.(?:uasset|umap)$',''
            } elseif($package -match '^(?<pkg>/Game/(?:[^/.]+/)*)(?<leaf>[^/.]+)\.(?<object>[^/.:]+)(?::[^/]+)?$') {
                $leaf=$Matches.leaf
                if($Matches.object -ine $leaf -and $Matches.object -ine ($leaf+'_C')){return $null}
                $package=$Matches.pkg+$leaf
            }
        } else {return $null}
    }
    if($package -notmatch '^/Game/[^/]+(?:/[^/]+)*$'){return $null}
    if($package.Contains('//') -or $package -match '(^|/)\.{1,2}(/|$)' -or $package -match '[.:\x00-\x1f]'){return $null}
    return $package
}

$csvPath=[IO.Path]::GetFullPath($CsvOut)
$outDir=Split-Path -Parent $csvPath
$outputRoot=[IO.Path]::GetPathRoot($outDir)
if($outDir.TrimEnd('\','/').Equals($outputRoot.TrimEnd('\','/'),[StringComparison]::OrdinalIgnoreCase)){throw 'Filesystem root cannot be an evidence directory'}
if([IO.Path]::GetExtension($csvPath) -ine '.csv'){throw 'CsvOut must be an explicit .csv file, not a directory'}
Assert-NoReparseAncestor $outDir $outputRoot
if(Test-Path -LiteralPath $csvPath){throw 'Preserve existing CSV output'}
if(Test-Path -LiteralPath $outDir) {
    if(-not(Test-Path -LiteralPath $outDir -PathType Container)){throw 'Output parent is not a directory'}
    if(@(Get-ChildItem -LiteralPath $outDir -Force).Count){throw 'Require a new or empty dedicated evidence directory; preserve existing contents'}
}

$pakPath=(Get-Item -LiteralPath $PakDir -Force).FullName
if(-not(Test-Path -LiteralPath $pakPath -PathType Container)){throw 'PakDir must be an existing directory'}
$exe=(Get-Item -LiteralPath $UnrealPakExe -Force).FullName
if(-not(Test-Path -LiteralPath $exe -PathType Leaf) -or [IO.Path]::GetFileName($exe) -ine 'UnrealPak.exe'){throw 'Require the installed UnrealPak.exe file'}
$utocs=@(Get-ChildItem -LiteralPath $pakPath -File -Filter '*.utoc')
if(-not $utocs.Count){throw 'PakDir contains no direct .utoc files; wildcard listing is nonrecursive'}
if($pakPath.Contains('"') -or $csvPath.Contains('"')){throw 'Unsupported quote in command-line path'}
if(-not(Test-Path -LiteralPath $outDir)){New-Item -ItemType Directory -Path $outDir | Out-Null}
Assert-NoReparseAncestor $outDir $outputRoot

$userDir=Join-Path $outDir 'runtime-user'
New-Item -ItemType Directory -Path $userDir | Out-Null
$stdout=Join-Path $outDir 'unrealpak-stdout.log'
$stderr=Join-Path $outDir 'unrealpak-stderr.log'
$engineLog=Join-Path $outDir 'unrealpak.log'
$journal=Join-Path $outDir 'iostore-verification.json'
$wildcard=(Join-Path $pakPath '*.utoc').Replace('\','/')
$arguments=@(('-ListContainer="'+$wildcard+'"'),('-Csv="'+$csvPath+'"'),'-unattended','-utf8output',('-UserDir="'+$userDir+'"'),('-abslog="'+$engineLog+'"'))
$required=@(
    '/Game/Rogue10m/Maps/StartHub',
    '/Game/Rogue10m/Maps/OpenRunMap',
    '/Game/Rogue10m/World/OpenWorld/Loot/DA_Loot_OW_VerdantSlime_PotionExample',
    '/Game/DataAsset/Item/Starter/DA_Item_HealthPotion',
    '/Game/Rogue10m/World/OpenWorld/Monsters/DA_OW_AbyssOverlordAzathor',
    '/Game/Rogue10m/Characters/Boss/AzathorHuman20261110/DA_HumanBoss_AttackProfile',
    '/Game/Rogue10m/World/OpenWorld/Monsters/Azathor/M_Azathor_BasicConeWarning_1103',
    '/Game/Weapons/Staff20261110/SM_BattleWizardStaff01',
    '/Game/Weapons/Starter20261110/SM_StarterGreatSword',
    '/Game/Weapons/Starter20261110/SM_StarterKnuckle',
    '/Game/Rogue10m/Animation/Combat/GreatSword20261110/ABP_Human_GreatSword',
    '/Game/Rogue10m/Animation/Combat/GreatSword20261110/A_GreatSword_Idle',
    '/Game/Rogue10m/Animation/Combat/GreatSword20261110/BS_GreatSword_Ground',
    '/Game/Rogue10m/Animation/Combat/FighterStance/A_Fighter_CombatIdle',
    '/Game/Rogue10m/Animation/Locomotion/PalmReference/BS_Unarmed_PalmReference',
    '/Game/DataAsset/Character/Animation/DA_CommonCharacterMotion',
    '/Game/DataAsset/Item/Prototype/DA_Item_Prototype_1x1',
    '/Game/DataAsset/Item/Prototype/DA_Item_Prototype_2x3',
    '/Game/DataAsset/Item/Prototype/DA_Item_Prototype_4x3',
    '/Game/DataAsset/Item/Starter/DA_Item_GuardianHelmet',
    '/Game/DataAsset/Item/Starter/DA_Item_IronHelmet',
    '/Game/DataAsset/Item/Starter/DA_Item_LeatherArmor',
    '/Game/DataAsset/Item/Starter/DA_Item_LeatherBoots',
    '/Game/DataAsset/Item/Starter/DA_Item_NoviceRing',
    '/Game/DataAsset/Item/Starter/DA_Item_StarterKnuckle',
    '/Game/DataAsset/SkillTree/DA_HybridSkillTreeCatalog',
    '/Game/Rogue10m/Animation/Preview/CloseCombat/A_CC_BackEvade_Manny',
    '/Game/Rogue10m/Animation/Preview/CloseCombat/A_CC_FrontEvade_Manny',
    '/Game/Rogue10m/Animation/Preview/CloseCombat/A_CC_LowRightThaiKick_Manny',
    '/Game/Rogue10m/Animation/Preview/CloseCombat/A_CC_StraightKick_Manny',
    '/Game/Rogue10m/Animation/Preview/Martelo/A_Martelo2_Manny',
    '/Game/Rogue10m/VFX/Character/Combat/Unarmed/M_BrawlerHitFlash',
    '/Game/UI/Traits/T_TraitCatalogAtlas',
    '/Game/Widget/Lobby/WBP_CharacterLobby',
    '/Game/Widget/Menu/WBP_MainMenu',
    '/Game/Widget/WBP_Rogue10mMainHUD',
    '/Game/Rogue10m/VFX/Character/Combat/MagicCandidates20261110/MagicMcp01/NS_MagicTelegraph_Preparation_Candidate',
    '/Game/Rogue10m/VFX/Character/Combat/MagicCandidates20261110/MagicMcp01/NS_MagicImpact_DiamondStar_Candidate',
    '/Game/DataAsset/AttackSkill/ClassCombat/Staff/DA_Attack_Staff_Primary01',
    '/Game/Material/UI/M_CharacterPreviewTransparent'
)
$record=[ordered]@{
    schemaVersion=1;phase='starting';startedUtc=[DateTime]::UtcNow.ToString('o');endedUtc=$null
    pakDir=$pakPath;csvOut=$csvPath;exe=$exe;exeSHA256=(Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash
    arguments=$arguments;utocCount=$utocs.Count;utocs=@($utocs | ForEach-Object {[ordered]@{path=$_.FullName;bytes=$_.Length;lastWriteUtc=$_.LastWriteTimeUtc.ToString('o')}})
    pid=$null;processStartedUtc=$null;exitCode=$null;columns=@();rowCount=0;exportRows=0;requiredPackages=@();worldPartitionCells=@();worldPartitionCellCount=0
}
function Write-Journal {$record | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath $journal -Encoding utf8NoBOM}
Write-Journal
$process=$null;$failure=$null
try {
    $process=Start-Process -FilePath $exe -ArgumentList $arguments -WorkingDirectory (Split-Path -Parent $exe) -WindowStyle Hidden -RedirectStandardOutput $stdout -RedirectStandardError $stderr -PassThru
    $record.pid=$process.Id;$record.processStartedUtc=$process.StartTime.ToUniversalTime().ToString('o');$record.phase='listing';Write-Journal
    Write-Output ('Owned UnrealPak PID='+$process.Id+' CSV='+$csvPath)
    $timer=[Diagnostics.Stopwatch]::StartNew()
    while(-not $process.WaitForExit(1000)){if($timer.Elapsed.TotalSeconds -gt $WallTimeoutSeconds){throw 'Owned UnrealPak exceeded wall timeout'}}
    $process.Refresh();$record.exitCode=$process.ExitCode;$record.wallSeconds=$timer.Elapsed.TotalSeconds
    if($process.ExitCode -ne 0){throw ('UnrealPak exit='+$process.ExitCode)}
    if(-not(Test-Path -LiteralPath $csvPath -PathType Leaf) -or (Get-Item -LiteralPath $csvPath).Length -eq 0){throw 'Fresh nonempty CSV was not produced'}
    $record.csvSHA256=(Get-FileHash -LiteralPath $csvPath -Algorithm SHA256).Hash
    $logText=@($stdout,$stderr,$engineLog | Where-Object {Test-Path -LiteralPath $_} | ForEach-Object {[IO.File]::ReadAllText($_)}) -join "`n"
    $badLines=@($logText -split '\r?\n' | Where-Object {$_ -match '(?i)(?:LogIoStore|LogPakFile):\s*(?:Error|Fatal):|Fatal error:|Assertion failed:|LowLevelFatalError|Failed to read (?:container|.*\btoc\b)|Missing argument -'})
    $record.toolFailureLines=$badLines
    if($badLines.Count){throw 'UnrealPak log reported a container/read/list failure despite its exit code'}

    $rows=@(Import-Csv -LiteralPath $csvPath)
    if(-not $rows.Count){throw 'CSV has no data rows'}
    $record.rowCount=$rows.Count
    $fields=@{}
    foreach($property in $rows[0].PSObject.Properties) {
        $name=$property.Name.Trim()
        if($fields.ContainsKey($name)){throw 'Ambiguous duplicate CSV column'}
        $fields[$name]=$property.Name
    }
    $record.columns=@($fields.Keys | Sort-Object)
    if(-not $fields.ContainsKey('PackageName') -and -not $fields.ContainsKey('Filename')){throw 'CSV schema has neither PackageName nor Filename'}
    $record.hasChunkType=$fields.ContainsKey('ChunkType')
    if(-not $record.hasChunkType){throw 'Require ChunkType column to prove ExportBundleData, not merely a filename mention'}
    $identityRows=@{};$conflicts=[Collections.Generic.List[object]]::new();$populated=0
    for($index=0;$index -lt $rows.Count;$index++) {
        $row=$rows[$index]
        $packageName=if($fields.ContainsKey('PackageName')){[string]$row.($fields.PackageName)}else{''}
        $filename=if($fields.ContainsKey('Filename')){[string]$row.($fields.Filename)}else{''}
        if(-not [string]::IsNullOrWhiteSpace($packageName) -or -not [string]::IsNullOrWhiteSpace($filename)){$populated++}
        $chunkType=if($fields.ContainsKey('ChunkType')){([string]$row.($fields.ChunkType)).Trim()}else{''}
        if($fields.ContainsKey('ChunkType') -and $chunkType -ine 'ExportBundleData'){continue}
        $record.exportRows++
        $fromPackage=ConvertTo-PackageIdentity $packageName
        $fromFilename=ConvertTo-PackageIdentity $filename -Filename
        if($fromPackage -and $fromFilename -and $fromPackage -ine $fromFilename) {
            $conflicts.Add([ordered]@{row=$index+2;packageName=$packageName;filename=$filename})
            continue
        }
        $identity=if($fromPackage){$fromPackage}else{$fromFilename}
        if($identity) {
            if(-not $identityRows.ContainsKey($identity)){$identityRows[$identity]=[Collections.Generic.List[object]]::new()}
            $identityRows[$identity].Add([ordered]@{row=$index+2;packageName=$packageName;filename=$filename;chunkType=$chunkType})
        }
    }
    $record.populatedIdentityRows=$populated;$record.normalizedPackageCount=$identityRows.Count;$record.identityConflicts=@($conflicts.ToArray())
    if(-not $populated){throw 'PackageName and Filename data are empty'}
    if($conflicts.Count){throw 'PackageName and Filename disagree after exact normalization'}
    $record.requiredPackages=@(foreach($name in $required){[ordered]@{package=$name;found=$identityRows.ContainsKey($name);evidenceRows=if($identityRows.ContainsKey($name)){@($identityRows[$name].ToArray())}else{@()}}})
    $record.worldPartitionCells=@($identityRows.Keys | Where-Object {$_ -match '^/Game/.+/_Generated_/[^/]+(?:/[^/]+)*$'} | Sort-Object)
    $record.worldPartitionCellCount=$record.worldPartitionCells.Count
    $missing=@($record.requiredPackages | Where-Object {-not $_.found} | ForEach-Object {$_.package})
    $record.missingRequiredPackages=$missing
    if($missing.Count){throw ('Missing exact required package(s): '+($missing -join ', '))}
    $record.phase='iostore_required_packages_passed_cells_reported'
} catch {
    $failure=$_;$record.failure=$failure.ToString();$record.phase='failed'
} finally {
    try {
        if($process -and -not $process.HasExited) {
            $process.Kill($true)
            if(-not $process.WaitForExit(10000)){throw 'Owned UnrealPak cleanup did not finish within 10 seconds'}
        }
    } catch {
        $record.cleanupFailure=$_.ToString();$record.phase='failed'
        if(-not $failure){$failure=$_;$record.failure=$_.ToString()}
    }
    $record.endedUtc=[DateTime]::UtcNow.ToString('o');Write-Journal
}
if($failure){throw $failure}
Write-Output ('IoStore exact required package checks passed; generated cells='+$record.worldPartitionCellCount+'. '+$journal)
