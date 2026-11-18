#requires -Version 7.0
# Pure planning/result/save contracts. No process, editor, network, or save mutations.

function Get-FiveVillageField {
    param([AllowNull()]$Object, [Parameter(Mandatory)][string]$Name)
    if ($null -eq $Object) { return $null }
    if ($Object -is [System.Collections.IDictionary]) {
        foreach ($key in $Object.Keys) { if ($key -ceq $Name) { return ,($Object[$key]) } }
        return $null
    }
    foreach ($property in $Object.PSObject.Properties) {
        if ($property.Name -ceq $Name) { return ,($property.Value) }
    }
    return $null
}

function Get-FiveVillageNormalizedPath {
    param([Parameter(Mandatory)][string]$Path)
    if ([string]::IsNullOrWhiteSpace($Path)) { throw 'A nonempty path is required.' }
    return [IO.Path]::TrimEndingDirectorySeparator([IO.Path]::GetFullPath($Path))
}

function Get-FiveVillageTravelContract {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)][string]$ProjectRoot,
        [Parameter(Mandatory)][ValidatePattern('^[A-Za-z0-9_]{1,20}$')][string]$Label,
        [Parameter(Mandatory)][ValidateSet('Direct','Revisit')][string]$Scenario
    )
    $root = Get-FiveVillageNormalizedPath $ProjectRoot
    $mode = if ($Scenario -ieq 'Direct') { 'Direct' } else { 'Revisit' }
    $suffix = $Label + '_' + $mode
    $caseDirectory = Join-Path $root ('Feature/doc/evidence/five-village-travel-20261111/' + $Label + '/' + $mode)
    return [pscustomobject]@{
        Label = $Label; Scenario = $mode; ProjectRoot = $root
        CaseDirectory = $caseDirectory
        RuntimeDirectory = Join-Path $caseDirectory 'runtime'
        ProfileSlot = 'Rogue10m_Town_QA_FiveVillage_' + $suffix
        ProgressionPrefix = 'Rogue10m_Progression_QA_FiveVillage_' + $suffix
        SkillSlot = 'Rogue10m_Fighter_QA_FiveVillage_' + $suffix
    }
}

function Assert-FiveVillageContract {
    param([Parameter(Mandatory)]$Contract)
    $label = Get-FiveVillageField $Contract 'Label'
    $scenario = Get-FiveVillageField $Contract 'Scenario'
    if ($label -isnot [string] -or $label -cnotmatch '^[A-Za-z0-9_]{1,20}$' -or
        $scenario -isnot [string] -or $scenario -cnotin @('Direct','Revisit')) {
        throw 'An exact bounded five-village Label/Scenario contract is required.'
    }
    $suffix = $label + '_' + $scenario
    foreach ($pair in @(@('ProfileSlot','Rogue10m_Town_QA_FiveVillage_'),
                         @('ProgressionPrefix','Rogue10m_Progression_QA_FiveVillage_'),
                         @('SkillSlot','Rogue10m_Fighter_QA_FiveVillage_'))) {
        $slot = Get-FiveVillageField $Contract $pair[0]
        if ($slot -isnot [string] -or $slot -cne ($pair[1] + $suffix)) {
            throw ('Unsafe or inconsistent save contract: ' + $pair[0])
        }
    }
}

function Get-FiveVillageSaveSnapshot {
    [CmdletBinding()]
    param([Parameter(Mandatory)][string]$SaveRoot)
    $root = Get-FiveVillageNormalizedPath $SaveRoot
    if (-not (Test-Path -LiteralPath $root -ErrorAction Stop)) { return }
    if (-not (Test-Path -LiteralPath $root -PathType Container -ErrorAction Stop)) { throw 'SaveRoot must be a directory.' }
    Get-ChildItem -LiteralPath $root -Recurse -Force -File -ErrorAction Stop | Sort-Object FullName | ForEach-Object {
        [pscustomobject]@{
            path = Get-FiveVillageNormalizedPath $_.FullName
            sha256 = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256 -ErrorAction Stop).Hash
        }
    }
}

function Test-FiveVillageOwnedSave {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)][string]$Path,
        [Parameter(Mandatory)][string]$SaveRoot,
        [Parameter(Mandatory)][AllowEmptyCollection()][object[]]$Contracts
    )
    $root = Get-FiveVillageNormalizedPath $SaveRoot
    $file = Get-FiveVillageNormalizedPath $Path
    if (-not [string]::Equals([IO.Path]::GetDirectoryName($file), $root, [StringComparison]::OrdinalIgnoreCase)) {
        return $false
    }
    $name = [IO.Path]::GetFileName($file)
    foreach ($contract in $Contracts) {
        Assert-FiveVillageContract $contract
        if ([string]::Equals($name, $contract.ProfileSlot + '.sav', [StringComparison]::OrdinalIgnoreCase) -or
            [string]::Equals($name, $contract.SkillSlot + '.sav', [StringComparison]::OrdinalIgnoreCase)) {
            return $true
        }
        $pattern = '^' + [regex]::Escape($contract.ProgressionPrefix) + '_[A-Za-z0-9_]+\.sav$'
        if ([regex]::IsMatch($name, $pattern, [Text.RegularExpressions.RegexOptions]::IgnoreCase)) { return $true }
    }
    return $false
}

function Get-FiveVillageSnapshotIndex {
    param(
        [AllowEmptyCollection()][object[]]$Snapshot = @(),
        [Parameter(Mandatory)][string]$SaveRoot
    )
    $root = Get-FiveVillageNormalizedPath $SaveRoot
    $boundary = $root + [IO.Path]::DirectorySeparatorChar
    $index = [Collections.Generic.Dictionary[string,string]]::new([StringComparer]::OrdinalIgnoreCase)
    foreach ($entry in $Snapshot) {
        $path = Get-FiveVillageField $entry 'path'
        $hash = Get-FiveVillageField $entry 'sha256'
        if ($path -isnot [string] -or $hash -isnot [string] -or $hash -cnotmatch '^[A-Fa-f0-9]{64}$') {
            throw 'Invalid save snapshot path/hash.'
        }
        $normalized = Get-FiveVillageNormalizedPath $path
        if (-not $normalized.StartsWith($boundary, [StringComparison]::OrdinalIgnoreCase)) {
            throw 'Snapshot file is outside SaveRoot.'
        }
        if ($index.ContainsKey($normalized)) { throw 'Duplicate case-insensitive save snapshot path.' }
        $index.Add($normalized, $hash)
    }
    return ,$index
}

function Compare-FiveVillageSaveSnapshots {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)][AllowEmptyCollection()][object[]]$Before,
        [Parameter(Mandatory)][AllowEmptyCollection()][object[]]$After,
        [Parameter(Mandatory)][string]$SaveRoot,
        [Parameter(Mandatory)][AllowEmptyCollection()][object[]]$Contracts
    )
    foreach ($contract in $Contracts) { Assert-FiveVillageContract $contract }
    $a = Get-FiveVillageSnapshotIndex -Snapshot $Before -SaveRoot $SaveRoot
    $b = Get-FiveVillageSnapshotIndex -Snapshot $After -SaveRoot $SaveRoot
    $changed = [Collections.Generic.List[object]]::new()
    $missing = [Collections.Generic.List[object]]::new()
    $unexpected = [Collections.Generic.List[object]]::new()
    $owned = [Collections.Generic.List[object]]::new()
    foreach ($path in $a.Keys) {
        if (-not $b.ContainsKey($path)) { $missing.Add([pscustomobject]@{path=$path;sha256=$a[$path]}) }
        elseif (-not [string]::Equals($a[$path], $b[$path], [StringComparison]::OrdinalIgnoreCase)) {
            $changed.Add([pscustomobject]@{path=$path;before_sha256=$a[$path];after_sha256=$b[$path]})
        }
    }
    foreach ($path in $b.Keys) {
        if ($a.ContainsKey($path)) { continue }
        $row = [pscustomobject]@{path=$path;sha256=$b[$path]}
        if (Test-FiveVillageOwnedSave -Path $path -SaveRoot $SaveRoot -Contracts $Contracts) { $owned.Add($row) }
        else { $unexpected.Add($row) }
    }
    return [pscustomobject]@{
        preserved = ($changed.Count -eq 0 -and $missing.Count -eq 0 -and $unexpected.Count -eq 0)
        changed = @($changed.ToArray()); missing = @($missing.ToArray())
        unexpected_new = @($unexpected.ToArray()); owned_new = @($owned.ToArray())
    }
}

function Test-FiveVillageInteger {
    param([AllowNull()]$Value)
    return ($Value -is [int] -or $Value -is [long] -or $Value -is [short] -or
        $Value -is [byte] -or $Value -is [uint32] -or $Value -is [uint64])
}

function Test-FiveVillageNumber {
    param([AllowNull()]$Value)
    return ((Test-FiveVillageInteger $Value) -or $Value -is [double] -or $Value -is [single] -or $Value -is [decimal])
}

function Test-FiveVillageExactString {
    param([AllowNull()]$Value, [Parameter(Mandatory)][string]$Expected)
    return ($Value -is [string] -and [string]::Equals($Value, $Expected, [StringComparison]::Ordinal))
}

function Test-FiveVillageJsonObject {
    param([AllowNull()]$Value)
    if ($null -eq $Value -or $Value -is [array]) { return $false }
    return ($Value -is [System.Collections.IDictionary] -or $Value.GetType().FullName -ceq 'System.Management.Automation.PSCustomObject')
}

function Test-FiveVillageTravelResult {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)][string]$Path,
        [Parameter(Mandatory)][ValidatePattern('^[A-Za-z0-9_]{1,20}$')][string]$Label,
        [Parameter(Mandatory)][ValidateSet('Direct','Revisit')][string]$Scenario,
        [Parameter(Mandatory)][bool]$ExpectedRendered
    )
    $reasons = [Collections.Generic.List[string]]::new()
    try {
        if (-not (Test-Path -LiteralPath $Path -PathType Leaf -ErrorAction Stop)) { throw 'Missing runtime results.json.' }
        $data = Get-Content -LiteralPath $Path -Raw -Encoding UTF8 -ErrorAction Stop | ConvertFrom-Json -NoEnumerate -ErrorAction Stop
        if (-not (Test-FiveVillageJsonObject $data)) {
            throw 'Runtime result must be a JSON object.'
        }
    } catch {
        $reasons.Add('unreadable_result: ' + $_.Exception.Message)
        return [pscustomobject]@{passed=$false;reasons=[string[]]$reasons.ToArray()}
    }
    $schema = Get-FiveVillageField $data 'schema_version'
    if (-not (Test-FiveVillageInteger $schema) -or $schema -ne 1) { $reasons.Add('schema_version must be integer1') }
    if (-not (Test-FiveVillageExactString (Get-FiveVillageField $data 'label') $Label)) { $reasons.Add('label mismatch') }
    $mode = if ($Scenario -ieq 'Direct') { 'Direct' } else { 'Revisit' }
    if (-not (Test-FiveVillageExactString (Get-FiveVillageField $data 'scenario') $mode)) { $reasons.Add('scenario mismatch') }
    if (-not (Test-FiveVillageExactString (Get-FiveVillageField $data 'status') 'passed')) { $reasons.Add('runtime status is not passed') }
    $checks = Get-FiveVillageField $data 'checks'
    $failures = Get-FiveVillageField $data 'failures'
    if (-not (Test-FiveVillageInteger $checks) -or $checks -le 0 -or $checks -gt [int]::MaxValue) { $reasons.Add('checks must be a positive int32') }
    if (-not (Test-FiveVillageInteger $failures) -or $failures -ne 0) { $reasons.Add('failures must be integer0') }
    $milestones = Get-FiveVillageField $data 'milestones'
    if (-not (Test-FiveVillageJsonObject $milestones)) { $reasons.Add('milestones must be a JSON object') }
    foreach ($name in @('normal_entry','timer_running','third_person','route_observations','settled','saved','returned_hub','profile_preserved')) {
        $value = Get-FiveVillageField $milestones $name
        if ($value -isnot [bool] -or -not $value) { $reasons.Add('missing/false milestone: ' + $name) }
    }
    foreach ($pair in @(@('actual_walking',$true),@('physical_input',$false),@('full_run',$false),@('rendered',$ExpectedRendered))) {
        $value = Get-FiveVillageField $data $pair[0]
        if ($value -isnot [bool] -or $value -ne $pair[1]) { $reasons.Add('invalid boolean: ' + $pair[0]) }
    }
    if (-not (Test-FiveVillageExactString (Get-FiveVillageField $data 'outcome') 'Abandoned')) { $reasons.Add('outcome is not Abandoned') }
    if (-not (Test-FiveVillageExactString (Get-FiveVillageField $data 'movement_method') 'AddMovementInput native CharacterMovement')) { $reasons.Add('movement method mismatch') }
    $ids = Get-FiveVillageField $data 'ids'
    if (-not (Test-FiveVillageJsonObject $ids)) { $reasons.Add('ids must be a JSON object') }
    $parsedIds = @{}
    foreach ($name in @('profile_id','entry_run_id','return_profile_id','return_run_id','settled_run_id')) {
        $value = Get-FiveVillageField $ids $name
        $parsed = [guid]::Empty
        if ($value -isnot [string] -or -not [guid]::TryParse($value, [ref]$parsed) -or $parsed -eq [guid]::Empty) {
            $reasons.Add('invalid GUID: ' + $name)
        } else { $parsedIds[$name] = $parsed }
    }
    if ($parsedIds.ContainsKey('profile_id') -and $parsedIds.ContainsKey('return_profile_id') -and
        $parsedIds.profile_id -ne $parsedIds.return_profile_id) { $reasons.Add('profile changed on return') }
    if ($parsedIds.ContainsKey('entry_run_id') -and $parsedIds.ContainsKey('settled_run_id') -and
        $parsedIds.entry_run_id -ne $parsedIds.settled_run_id) { $reasons.Add('wrong run settlement receipt') }
    if ($parsedIds.ContainsKey('entry_run_id') -and $parsedIds.ContainsKey('return_run_id') -and
        $parsedIds.entry_run_id -eq $parsedIds.return_run_id) { $reasons.Add('return did not create a fresh waiting run') }
    $timing = Get-FiveVillageField $data 'timing'
    if (-not (Test-FiveVillageJsonObject $timing)) { $reasons.Add('timing must be a JSON object') }
    $duration = Get-FiveVillageField $timing 'run_duration_seconds'
    $advanced = Get-FiveVillageField $timing 'timer_advanced_seconds'
    $crossing = Get-FiveVillageField $timing 'boss_crossing_active_seconds'
    if (-not (Test-FiveVillageNumber $duration) -or $duration -ne 1200) { $reasons.Add('run duration is not1200') }
    if (-not (Test-FiveVillageNumber $advanced) -or -not [double]::IsFinite([double]$advanced) -or
        $advanced -le 0 -or $advanced -gt 1200) { $reasons.Add('timer did not advance validly') }
    if (-not (Test-FiveVillageNumber $crossing) -or -not [double]::IsFinite([double]$crossing) -or
        $crossing -lt 0 -or $crossing -gt 1200) { $reasons.Add('invalid boss crossing active time') }
    elseif ($mode -eq 'Direct' -and $crossing -gt 60) { $reasons.Add('direct crossing exceeded60 active seconds') }
    $visits = Get-FiveVillageField $data 'visits'
    if (-not (Test-FiveVillageJsonObject $visits)) { $reasons.Add('visits must be a JSON object') }
    $counts = @{}
    foreach ($name in @('V1','V2','V3','V4','V5')) {
        $value = Get-FiveVillageField $visits $name
        if (-not (Test-FiveVillageInteger $value) -or $value -lt 0 -or $value -gt [int]::MaxValue) { $reasons.Add('invalid visits: ' + $name) }
        else { $counts[$name] = $value }
    }
    if ($counts.Count -eq 5) {
        if ($mode -eq 'Direct' -and @($counts.Values | Where-Object { $_ -ne 0 }).Count) { $reasons.Add('direct path did not skip every village') }
        if ($mode -eq 'Revisit' -and ($counts.V1 -lt 2 -or $counts.V3 -lt 1 -or $counts.V4 -lt 2 -or $counts.V2 -ne 0 -or $counts.V5 -ne 0)) {
            $reasons.Add('revisit/skip observations incomplete')
        }
    }

    # Pinned to live TravelSpeedD + authored base/boots, never accept an arbitrary reported target.
    $movement = Get-FiveVillageField $data 'movement_diagnostics'
    if (-not (Test-FiveVillageJsonObject $movement)) { $reasons.Add('movement diagnostics missing/object required') }
    $normal = @{authored_data_walk=600;base_walk=600;base_sprint=900;gear_bonus=20;gear_multiplier_bonus=0;
        combined_bonus=20;combined_multiplier_bonus=0;derived_walk=620;derived_sprint=920;actual_max_walk=620;identity_multiplier=1}
    $expectedItems = @('DA_Item_IronHelmet','DA_Item_LeatherArmor','DA_Item_LeatherBoots','DA_Item_NoviceRing')
    foreach ($phase in @('hub','candidate')) {
        $row = Get-FiveVillageField $movement $phase
        if (-not (Test-FiveVillageJsonObject $row)) { $reasons.Add("missing movement phase $phase") }
        foreach ($key in $normal.Keys) {
            $v = Get-FiveVillageField $row $key
            if (-not (Test-FiveVillageNumber $v) -or -not [double]::IsFinite([double]$v) -or [math]::Abs([double]$v-$normal[$key]) -gt 0.0001) {
                $reasons.Add("invalid authored movement $phase.$key")
            }
        }
        foreach ($key in @('identity_active','sprinting','third_person')) {
            $v = Get-FiveVillageField $row $key
            if ($v -isnot [bool] -or $v -ne ($key -eq 'third_person')) { $reasons.Add("invalid movement state $phase.$key") }
        }
        foreach ($pair in @(@('pawn_class','/Game/Character/Customization/Characters/BP_Rogue10m_HumanMaleCharacter.BP_Rogue10m_HumanMaleCharacter_C'),
                @('character_data','/Game/DataAsset/Character/DA_Character_Default.DA_Character_Default'))) {
            if (-not (Test-FiveVillageExactString (Get-FiveVillageField $row $pair[0]) $pair[1])) { $reasons.Add("movement source differs $phase.$($pair[0])") }
        }
        $items = Get-FiveVillageField $row 'equipped_items'
        if ($items -isnot [array] -or $items.Count -ne 4) { $reasons.Add("four exact starter items required $phase") }
        foreach ($name in $expectedItems) {
            $asset = '/Game/DataAsset/Item/Starter/'+$name+'.'+$name
            $found = @($items | Where-Object { Test-FiveVillageExactString (Get-FiveVillageField $_ 'asset') $asset })
            if ($found.Count -ne 1) { $reasons.Add("missing/duplicate starter $phase.$name"); continue }
            $bonus = Get-FiveVillageField $found[0] 'move_speed_bonus'
            $multi = Get-FiveVillageField $found[0] 'move_speed_multiplier_bonus'
            $want = if ($name -ceq 'DA_Item_LeatherBoots') {20} else {0}
            if (-not (Test-FiveVillageNumber $bonus) -or $bonus -ne $want -or -not (Test-FiveVillageNumber $multi) -or $multi -ne 0) { $reasons.Add("wrong starter modifier $phase.$name") }
        }
    }
    $speed = Get-FiveVillageField $timing 'maximum_field_speed_cm_per_second'
    # Original650 ceiling retained:620 normal +30 velocity tolerance, not a free-form reported baseline.
    if (-not (Test-FiveVillageNumber $speed) -or -not [double]::IsFinite([double]$speed) -or $speed -le 0 -or $speed -gt (620+30)) { $reasons.Add('measured field speed outside justified normal620 plus30 envelope') }
    $distance = Get-FiveVillageField $timing 'field_walked_distance_cm'
    if (-not (Test-FiveVillageNumber $distance) -or -not [double]::IsFinite([double]$distance) -or $distance -le 0 -or ($mode -eq 'Direct' -and $distance -lt 29500)) { $reasons.Add('actual field distance missing or insufficient') }

    $currency = Get-FiveVillageField $data 'currency'
    if (-not (Test-FiveVillageJsonObject $currency)) { $reasons.Add('currency must be a JSON object') }
    $restored = Get-FiveVillageField $currency 'baseline_restored'
    $award = Get-FiveVillageField $currency 'lobby_award'
    if ($restored -isnot [bool] -or -not $restored) { $reasons.Add('entry currency baseline not restored') }
    if (-not (Test-FiveVillageInteger $award) -or $award -ne 0) { $reasons.Add('abandon awarded lobby currency') }
    return [pscustomobject]@{passed=($reasons.Count -eq 0);reasons=[string[]]$reasons.ToArray()}
}
