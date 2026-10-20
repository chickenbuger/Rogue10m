param([string]$BlendFile)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$blenderExe = Join-Path $projectRoot 'tmp\apollo-tools\blender-4.5.14-windows-x64\blender.exe'
$sourceAddon = Join-Path $projectRoot 'tmp\codex-blender-mcp-source\blender_addon\__init__.py'
$userScripts = Join-Path $projectRoot 'tmp\blender-mcp-user\scripts'
$userConfig = Join-Path $projectRoot 'tmp\blender-mcp-user\config'
$addonPackage = Join-Path $userScripts 'addons\codex_blender_mcp_bridge'
if (-not (Test-Path -LiteralPath $blenderExe)) { throw 'Project portable Blender is missing.' }
if (-not (Test-Path -LiteralPath $sourceAddon)) { throw 'Clone the attributed upstream MCP source first.' }
New-Item -ItemType Directory -Force -Path $addonPackage,$userConfig | Out-Null
Copy-Item -LiteralPath $sourceAddon -Destination (Join-Path $addonPackage '__init__.py')
if (Get-NetTCPConnection -LocalAddress '127.0.0.1' -LocalPort 9877 -State Listen -ErrorAction SilentlyContinue) {
    throw 'Port 9877 is already listening. Inspect its owner instead of starting a second bridge.'
}
$oldScripts = $env:BLENDER_USER_SCRIPTS
$oldConfig = $env:BLENDER_USER_CONFIG
try {
    $env:BLENDER_USER_SCRIPTS = $userScripts
    $env:BLENDER_USER_CONFIG = $userConfig
    $arguments = @()
    if ($BlendFile) {
        $resolvedBlend = (Resolve-Path -LiteralPath $BlendFile).Path
        $arguments += ('"' + $resolvedBlend + '"')
    }
    $arguments += @('--python', ('"' + (Join-Path $PSScriptRoot 'Blender\BootstrapMCP.py') + '"'))
    $process = Start-Process -FilePath $blenderExe -ArgumentList $arguments -WorkingDirectory $projectRoot -WindowStyle Hidden -PassThru
    Write-Output ('Blender MCP process: ' + $process.Id)
} finally {
    $env:BLENDER_USER_SCRIPTS = $oldScripts
    $env:BLENDER_USER_CONFIG = $oldConfig
}
