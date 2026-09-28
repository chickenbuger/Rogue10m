param(
    [string]$ProjectRoot = (Split-Path -Parent $PSScriptRoot)
)

$ErrorActionPreference = 'Stop'
$configRoot = Join-Path ([System.IO.Path]::GetFullPath($ProjectRoot)) '.obsidian'
$graphPath = Join-Path $configRoot 'graph.json'
$backupPath = Join-Path $configRoot 'graph.before-source-only.json.bak'
[System.IO.Directory]::CreateDirectory($configRoot) | Out-Null
$graph = [pscustomobject]@{}
if (Test-Path -LiteralPath $graphPath) {
    $graph = Get-Content -LiteralPath $graphPath -Raw -Encoding UTF8 | ConvertFrom-Json
    if ($null -eq $graph -or $graph -isnot [pscustomobject]) { throw 'Invalid Obsidian graph configuration.' }
    if (-not (Test-Path -LiteralPath $backupPath)) {
        Copy-Item -LiteralPath $graphPath -Destination $backupPath
    }
}
$settings = [ordered]@{
    search = '(path:"Docs/Obsidian/ArchitectureMetrics/Files/" OR path:"Docs/Obsidian/ArchitectureMetrics/Functions/") -path:"Feature/"'
    'collapse-filter' = $false
    showTags = $false
    showAttachments = $false
    hideUnresolved = $true
    showOrphans = $true
    showArrow = $true
}
foreach ($setting in $settings.GetEnumerator()) {
    $graph | Add-Member -NotePropertyName $setting.Key -NotePropertyValue $setting.Value -Force
}
$functionQuery = 'path:"Docs/Obsidian/ArchitectureMetrics/Functions/"'
$existingGroups = @($graph.colorGroups | Where-Object { $null -ne $_ -and $_.query -ne $functionQuery })
$functionGroup = [pscustomobject]@{ query = $functionQuery; color = [pscustomobject]@{ a = 1; rgb = 16763955 } }
$graph | Add-Member -NotePropertyName colorGroups -NotePropertyValue (@($functionGroup) + $existingGroups) -Force
$graph | Add-Member -NotePropertyName 'collapse-color-groups' -NotePropertyValue $false -Force
$utf8 = New-Object System.Text.UTF8Encoding($false)
[System.IO.File]::WriteAllText($graphPath, (($graph | ConvertTo-Json -Depth 20) + [Environment]::NewLine), $utf8)
Write-Output 'Source graph filter saved. Reload Obsidian if the current graph still uses the old filter.'
Write-Output $settings.search
