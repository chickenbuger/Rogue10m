param([int]$Port = 4317, [switch]$NoBrowser)
$ErrorActionPreference = 'Stop'
$ProjectRoot = Split-Path -Parent $PSScriptRoot
$StudioRoot = Join-Path $ProjectRoot 'Studio'
$StudioUrl = "http://127.0.0.1:$Port"
function Test-Studio {
    try {
        $Snapshot = Invoke-RestMethod -Uri "$StudioUrl/api/snapshot" -TimeoutSec 3
        return ($Snapshot.project -eq 'Rogue10m' -and $null -ne $Snapshot.entries)
    } catch { return $false }
}
if (-not (Test-Studio)) {
    $NodeCommand = Get-Command node -ErrorAction SilentlyContinue
    if (-not $NodeCommand) {
        $BundledNode = Join-Path $env:USERPROFILE '.cache/codex-runtimes/codex-primary-runtime/dependencies/node/bin/node.exe'
        if (Test-Path -LiteralPath $BundledNode) { $NodePath = $BundledNode }
        else { throw 'Node.js 20 이상이 필요합니다. Node.js 설치 후 다시 실행하세요.' }
    } else { $NodePath = $NodeCommand.Source }
    $DataDirectory = Join-Path $StudioRoot 'data'
    New-Item -ItemType Directory -Path $DataDirectory -Force | Out-Null
    $PreviousPort = $env:STUDIO_PORT
    try {
        $env:STUDIO_PORT = "$Port"
        $ServerProcess = Start-Process -FilePath $NodePath -ArgumentList 'server.mjs' -WorkingDirectory $StudioRoot -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $DataDirectory 'server.out.log') -RedirectStandardError (Join-Path $DataDirectory 'server.err.log')
    } finally { $env:STUDIO_PORT = $PreviousPort }
    $Ready = $false
    for ($Attempt = 0; $Attempt -lt 20; $Attempt++) {
        if (Test-Studio) { $Ready = $true; break }
        if ($ServerProcess.HasExited) { break }
        Start-Sleep -Milliseconds 300
    }
    if (-not $Ready) { throw "스튜디오를 시작하지 못했습니다. $DataDirectory/server.err.log를 확인하세요." }
    Set-Content -LiteralPath (Join-Path $DataDirectory 'server.pid') -Value $ServerProcess.Id -Encoding ascii
}
Write-Host "Rogue10m Studio: $StudioUrl"
if (-not $NoBrowser) { Start-Process $StudioUrl }
