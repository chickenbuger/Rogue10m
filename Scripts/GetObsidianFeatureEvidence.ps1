# Document mentions are evidence of relevance, not proof of runtime calls or completed work.
function Get-ObsidianFeatureEvidence {
    param(
        [string]$ProjectRoot,
        [string]$SourceRoot,
        [string[]]$SourcePaths,
        [object[]]$Groups
    )
    $aliases = [Collections.Generic.Dictionary[string,object]]::new([StringComparer]::Ordinal)
    function Add-SourceAlias([string]$Alias, [string[]]$Targets) {
        if (-not $aliases.ContainsKey($Alias)) { $aliases[$Alias] = @() }
        $aliases[$Alias] = @($aliases[$Alias] + $Targets | Sort-Object -Unique)
    }
    $pairs = @{}
    foreach ($path in $SourcePaths) {
        $stem = [IO.Path]::GetFileNameWithoutExtension($path)
        if (-not $pairs.ContainsKey($stem)) { $pairs[$stem] = @() }
        $pairs[$stem] += $path
        Add-SourceAlias ([IO.Path]::GetFileName($path)) @($path)
    }
    foreach ($stem in $pairs.Keys) {
        $targets = @($pairs[$stem])
        if ($stem -ne 'Rogue10m') {
            Add-SourceAlias $stem $targets
            Add-SourceAlias ($stem + '.*') $targets
            Add-SourceAlias ($stem + '.h/.cpp') $targets
            Add-SourceAlias ($stem + '.h/cpp') $targets
            Add-SourceAlias ($stem + '.cpp/.h') $targets
            Add-SourceAlias ($stem + '.cpp/h') $targets
            $short = $stem -replace '^Rogue10m', ''
            if ($short.Length -ge 10) {
                Add-SourceAlias $short $targets
                foreach ($suffix in @('.*', '.h/.cpp', '.h/cpp', '.cpp/.h', '.cpp/h')) {
                    Add-SourceAlias ($short + $suffix) $targets
                }
                foreach ($extension in @('.h', '.cpp')) {
                    Add-SourceAlias ($short + $extension) @($targets | Where-Object { $_.EndsWith($extension) })
                }
            }
        }
    }
    foreach ($path in ($SourcePaths | Where-Object { $_.EndsWith('.h') })) {
        $text = [IO.File]::ReadAllText((Join-Path $SourceRoot $path))
        # Definitions only: forward declarations must not claim ownership of a symbol.
        $text = [regex]::Replace($text, '(?s)/\*.*?\*/|(?m)//[^\r\n]*', '')
        foreach ($match in [regex]::Matches($text, '(?m)^\s*(?:class|struct)\s+(?:ROGUE10M_API\s+)?([A-Za-z_]\w*)\s*(?:final\s*)?(?=[:{])')) {
            $symbol = $match.Groups[1].Value
            $stem = [IO.Path]::GetFileNameWithoutExtension($path)
            $targets = @($pairs[$stem])
            Add-SourceAlias $symbol $targets
            $short = $symbol -replace '^[AUFIE]Rogue10m', ''
            if ($short -ne $symbol -and $short.Length -ge 10) { Add-SourceAlias $short $targets }
        }
    }
    # Reject ambiguous basenames/aliases spanning more than one source pair.
    foreach ($alias in @($aliases.Keys)) {
        $owners = @($aliases[$alias] | ForEach-Object { $_ -replace '\.(h|cpp)$', '' } | Sort-Object -Unique)
        if ($owners.Count -ne 1) { $aliases.Remove($alias) | Out-Null }
    }
    $documents = @()
    foreach ($folder in @('architect', 'doc')) {
        $dir = Join-Path $ProjectRoot ('Feature/' + $folder)
        foreach ($file in (Get-ChildItem -LiteralPath $dir -File -Filter '*.md' | Sort-Object Name)) {
            if ($file.Name -eq 'README.md' -or $file.Name -match 'obsidian-') { continue }
            $hits = @()
            $lines = [IO.File]::ReadAllLines($file.FullName)
            for ($i = 0; $i -lt $lines.Count; $i++) {
                # Paired suffixes and wildcards are handled before bare identifiers.
                foreach ($match in [regex]::Matches($lines[$i], '[A-Za-z_][A-Za-z0-9_]*(?:\.h/\.?cpp|\.cpp/\.?h|\.\*|\.(?:h|cpp))?(?![A-Za-z0-9_])')) {
                    $token = $match.Value
                    if (-not $aliases.ContainsKey($token)) { continue }
                    foreach ($source in $aliases[$token]) {
                        $hits += [pscustomobject]@{
                            source = $source
                            document = ('Feature/' + $folder + '/' + $file.Name)
                            line = $i + 1
                            mention = $token
                            kind = if ($folder -eq 'architect') { 'plan' } else { 'result-document' }
                        }
                    }
                }
            }
            $documents += [pscustomobject]@{ name = $file.Name; path = ('Feature/' + $folder + '/' + $file.Name); hits = @($hits) }
        }
    }
    $records = @()
    foreach ($group in $Groups) {
        $selected = @($documents | Where-Object {
            $name = $_.name
            @($group.documentPatterns | Where-Object { $name -like $_ }).Count -gt 0
        })
        $evidence = @($selected | ForEach-Object { $_.hits } | Sort-Object source, document, line, mention -Unique)
        $sources = @($evidence | ForEach-Object source | Sort-Object -Unique)
        if ($sources.Count -eq 0) { throw "No Feature source evidence for function: $($group.name)" }
        $records += [pscustomobject]@{
            name = $group.name
            description = $group.description
            sources = $sources
            documents = @($selected.path)
            documentsWithoutSourceMentions = @($selected | Where-Object { $_.hits.Count -eq 0 } | ForEach-Object path)
            evidence = $evidence
        }
    }
    $selectedPaths = @($records | ForEach-Object { $_.documents } | Sort-Object -Unique)
    return [pscustomobject]@{
        functions = $records
        scannedDocumentCount = $documents.Count
        selectedDocumentCount = $selectedPaths.Count
        unassignedDocuments = @($documents | Where-Object { $_.path -notin $selectedPaths } | ForEach-Object path)
        unreferencedSources = @($SourcePaths | Where-Object { $_ -notin @($records | ForEach-Object { $_.sources }) })
    }
}
