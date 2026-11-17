param(
    [string]$EngineRoot = 'D:\Program Files\UE_5.8',
    [ValidatePattern('^[A-Za-z0-9_]{1,20}$')][string]$Label = 'CatalogA',
    [ValidateRange(60,600)][int]$TimeoutSeconds = 240,
    [switch]$IncludeGrowth,
    [switch]$PreflightOnly
)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$out = Join-Path $root ('Feature\doc\evidence\catalog-consumption-20261110\' + $Label)
if (Test-Path -LiteralPath $out) { throw 'Choose a fresh label; previous evidence is preserved.' }
New-Item -ItemType Directory -Path $out | Out-Null
$stages = @(
    @{ Name='Catalog'; Filter='Rogue10m.SkillLoadout.ActiveTechniqueCatalog'; Count=1 },
    @{ Name='Consumption'; Filter='Rogue10m.SkillLoadout.CatalogConsumption'; Count=1 },
    @{ Name='Policy'; Filter='Rogue10m.SkillLoadout.SlotPolicy'; Count=1 }
)
if ($IncludeGrowth) { $stages += @{ Name='Growth'; Filter='Rogue10m.HybridSkillTree'; Count=3 } }
$editorPath = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$projectPath = Join-Path $root 'Rogue10m.uproject'
$modulePath = Join-Path $root 'Binaries\Win64\UnrealEditor-Rogue10m.dll'
# The existing legacy migration test resolves F.Ultimate through this real package.
# Presence is a prerequisite only; it does not certify that dependencies load or a catalog is active.
$growthPackage = '/Game/DataAsset/AttackSkill/StoneFist/DA_Attack_StoneFist_ChargedShockwave'
$growthFile = Join-Path $root 'Content\DataAsset\AttackSkill\StoneFist\DA_Attack_StoneFist_ChargedShockwave.uasset'
$missingContent = @()
if ($IncludeGrowth -and -not (Test-Path -LiteralPath $growthFile -PathType Leaf)) { $missingContent += $growthPackage }
$preflight = [ordered]@{
    checked_at_utc = [DateTime]::UtcNow.ToString('o')
    project = $projectPath
    project_exists = (Test-Path -LiteralPath $projectPath -PathType Leaf)
    editor = $editorPath
    editor_exists = (Test-Path -LiteralPath $editorPath -PathType Leaf)
    module_binary_exists = (Test-Path -LiteralPath $modulePath -PathType Leaf)
    build_required = (-not (Test-Path -LiteralPath $modulePath -PathType Leaf))
    growth_requested = [bool]$IncludeGrowth
    expected_test_count = if ($IncludeGrowth) { 6 } else { 3 }
    required_growth_package = if ($IncludeGrowth) { $growthPackage } else { $null }
    required_growth_file = if ($IncludeGrowth) { $growthFile } else { $null }
    missing_content_packages = @($missingContent)
    preflight_only = [bool]$PreflightOnly
    process_created = $false
    actual_catalog_opt_in_verified = $false
}
$preflightPath = Join-Path $out 'preflight.json'
$preflight | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $preflightPath -Encoding utf8
if (-not $preflight.project_exists) { throw ('Project file missing: ' + $projectPath) }
if (-not $preflight.editor_exists) { throw ('Unreal Editor executable missing: ' + $editorPath) }
if ($missingContent.Count) {
    throw ('Growth regression requires real content before Editor launch: ' + ($missingContent -join ', ') + '. Core checks can run without -IncludeGrowth; do not count this as a gameplay regression.')
}
if ($PreflightOnly) {
    Write-Output ('RESULT=CATALOG_PREFLIGHT_PASSED expected_tests=' + $preflight.expected_test_count + ' build_required=' + $preflight.build_required + ' editor_started=false evidence=' + $out)
    return
}
if ($preflight.build_required) { throw ('Build the isolated Editor target before launch: module missing: ' + $modulePath) }
$results = @()
foreach ($stage in $stages) {
    $dest = Join-Path $out $stage.Name
    New-Item -ItemType Directory -Path $dest | Out-Null
    $arguments = @(
        ('"' + (Join-Path $root 'Rogue10m.uproject') + '"'), '/Engine/Maps/Entry',
        '-nullrhi', '-unattended', '-nosplash', '-nosound', '-NoLiveCoding', '-NoRemoteShaderCompile', '-DisablePython',
        '-stdout', '-FullStdOutLogOutput', '-UTF8Output',
        '-language=en', '-locale=en-US',
        ('-RogueProfileSaveSlot=Rogue10m_Town_QA_Catalog_' + $Label),
        ('-RogueProgressionSavePrefix=Rogue10m_Progression_QA_Catalog_' + $Label),
        ('-RogueSkillKeySaveSlot=Rogue10m_Fighter_QA_Catalog_' + $Label),
        ('-ExecCmds="Automation RunTests ' + $stage.Filter + '"'),
        '-TestExit="Automation Test Queue Empty"',
        ('-ReportExportPath="' + $dest + '\report"'), ('-abslog="' + $dest + '\automation.log"')
    )
    $arguments | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $dest 'arguments.json') -Encoding utf8
    $process = Start-Process -FilePath $editorPath -ArgumentList $arguments -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $dest 'stdout.log') -RedirectStandardError (Join-Path $dest 'stderr.log')
    try {
        $preflight.process_created = $true
        $preflight | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $preflightPath -Encoding utf8
        Write-Output ('Owned catalog QA stage=' + $stage.Name + ' PID=' + $process.Id)
        if (-not $process.WaitForExit($TimeoutSeconds * 1000)) { throw 'Automation timed out.' }
        $reportPath = Join-Path $dest 'report\index.json'
        [pscustomobject]@{pid=$process.Id;exit=$process.ExitCode;report_exists=(Test-Path -LiteralPath $reportPath);physical_input=$false;full_run=$false;growth_requested=[bool]$IncludeGrowth;growth_prerequisites_checked=[bool]$IncludeGrowth;growth_prerequisites_present=$(if ($IncludeGrowth) { $missingContent.Count -eq 0 } else { $null })} |
            ConvertTo-Json | Set-Content -LiteralPath (Join-Path $dest 'launcher.json') -Encoding utf8
        if ($process.ExitCode -ne 0) { throw ('Editor exit=' + $process.ExitCode) }
        if (-not (Test-Path -LiteralPath $reportPath)) { throw ('Editor produced no automation report; inspect captured stdout/stderr: ' + $dest) }
        $report = Get-Content -LiteralPath $reportPath -Raw -Encoding utf8 | ConvertFrom-Json
        if ($report.succeeded -ne $stage.Count -or $report.succeededWithWarnings -ne 0 -or $report.failed -ne 0 -or $report.notRun -ne 0 -or @($report.tests).Count -ne $stage.Count) {
            throw ('Unexpected test counts: ' + $stage.Name)
        }
        foreach ($test in $report.tests) {
            if ($test.state -ne 'Success' -or $test.errors -ne 0) { throw ('Failed test: ' + $test.fullTestPath) }
            $results += $test | Select-Object fullTestPath,state,errors
        }
    } finally {
        if (-not $process.HasExited) { Stop-Process -Id $process.Id; $process.WaitForExit(5000) | Out-Null }
    }
}
$results | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $out 'summary.json') -Encoding utf8
Write-Output ('RESULT=CATALOG_CONSUMPTION_PASSED tests=' + $results.Count + ' evidence=' + $out)
