param(
    [switch]$SkipRefresh,
    [string]$ProjectRoot = (Split-Path -Parent $PSScriptRoot)
)

$ErrorActionPreference = 'Stop'
$ProjectRoot = [System.IO.Path]::GetFullPath($ProjectRoot)
& (Join-Path $PSScriptRoot 'SetObsidianSourceGraph.ps1') -ProjectRoot $ProjectRoot
if (-not $SkipRefresh) {
    & (Join-Path $PSScriptRoot 'BuildObsidianArchitectureMetrics.ps1') -ProjectRoot $ProjectRoot
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & (Join-Path $PSScriptRoot 'TestObsidianArchitectureMetrics.ps1') -ProjectRoot $ProjectRoot
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

$vaultName = Split-Path -Leaf $ProjectRoot.TrimEnd('\', '/')
$vault = [System.Uri]::EscapeDataString($vaultName)
$file = [System.Uri]::EscapeDataString('Docs/Obsidian/ArchitectureMetrics/Dashboard.md')
$uri = "obsidian://open?vault=$vault&file=$file"
Start-Process $uri
