$ErrorActionPreference = 'Stop'
$PSNativeCommandUseErrorActionPreference = $false
$Project = 'D:\Project\Rogue10m'
$Run = $PSScriptRoot
$Allowed = Join-Path $Project 'Feature\implementation\map-resume-20261113\combined-g08-vfx'
if (-not ([IO.Path]::GetFullPath($Run).StartsWith([IO.Path]::GetFullPath($Allowed) + '\', [StringComparison]::OrdinalIgnoreCase))) { throw 'Unexpected output boundary' }
$G08Patch = Join-Path $Project 'tmp\SkillTiming20261113\Delivery\g08-full-source-20261113.patch'
$VfxPacket = 'C:\Users\PC\Documents\Codex\2026-11-11\task-2\vfx-refresh-20261113-delivery-v2'
$VfxPatch = Join-Path $VfxPacket 'vfx-full-20261113.patch'
$G08 = Get-Content -LiteralPath (Join-Path $Project 'tmp\SkillTiming20261113\Delivery\validation.json') -Raw | ConvertFrom-Json
$Vfx = Get-Content -LiteralPath (Join-Path $VfxPacket 'full-manifest.json') -Raw | ConvertFrom-Json
$Encoding = [Text.UTF8Encoding]::new($false)
function WriteJson($Path, $Value) { [IO.File]::WriteAllText($Path, ($Value | ConvertTo-Json -Depth 20) + "`n", $Encoding) }
function Sha($Path) { if (Test-Path -LiteralPath $Path -PathType Leaf) { return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant() }; return $null }
function PathUnder($Root, $Relative) {
    if ($Relative -match '(^[/\\]|(^|[/\\])\.\.([/\\]|$)|:)') { throw "Unsafe relative path: $Relative" }
    $Result = [IO.Path]::GetFullPath((Join-Path $Root $Relative))
    if (-not $Result.StartsWith([IO.Path]::GetFullPath($Root) + '\', [StringComparison]::OrdinalIgnoreCase)) { throw 'Path escaped boundary' }
    return $Result
}
function PlainCopy($From, $To) {
    $Item = Get-Item -LiteralPath $From
    if ($Item.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw "Refusing linked file: $From" }
    New-Item -ItemType Directory -Path (Split-Path $To -Parent) -Force | Out-Null
    Copy-Item -LiteralPath $From -Destination $To
}
function Git($Cwd, $Name, [string[]]$GitArgs) {
    $Lines = @(& git -C $Cwd @GitArgs 2>&1 | ForEach-Object { $_.ToString() })
    $Code = $LASTEXITCODE
    [IO.File]::WriteAllText((Join-Path $Run ($Name + '.log')), ($Lines -join "`n") + "`n", $Encoding)
    return [pscustomobject]@{ name=$Name; arguments=$GitArgs; exit_code=$Code; log=($Name + '.log') }
}
$Baseline = Join-Path $Run 'Baseline'
$Candidate = Join-Path $Run 'G08ThenVFX'
if ((Test-Path -LiteralPath $Baseline) -or (Test-Path -LiteralPath $Candidate)) { throw 'Preserve existing run; choose a new label' }
New-Item -ItemType Directory -Path $Baseline,$Candidate | Out-Null
$Paths = @(@($G08.files.path) + @($Vfx.entries.path) | Sort-Object -Unique)
$Records = @()
foreach ($Path in $Paths) {
    $Current = PathUnder $Project $Path
    $G = @($G08.files | Where-Object path -eq $Path)
    $V = @($Vfx.entries | Where-Object path -eq $Path)
    $Hash = Sha $Current
    if ($G.Count -and $Hash -ne $G[0].shared_baseline_sha256) { throw "G08 preimage mismatch: $Path" }
    if ($V.Count -and $Hash -ne $V[0].base_sha256) { throw "VFX preimage mismatch: $Path" }
    if ($Hash) {
        PlainCopy $Current (PathUnder $Baseline $Path)
        PlainCopy $Current (PathUnder $Candidate $Path)
    }
    $Records += [pscustomobject]@{path=$Path; baseline_sha256=$Hash; g08_candidate_sha256=if($G.Count){$G[0].candidate_sha256}else{$null}; vfx_candidate_sha256=if($V.Count){$V[0].candidate_sha256}else{$null}; common=($G.Count -gt 0 -and $V.Count -gt 0); preserved_helper=($V.Count -gt 0 -and $V[0].base_sha256 -and $V[0].base_sha256 -eq $V[0].candidate_sha256)}
}
WriteJson (Join-Path $Run 'baseline-manifest.json') $Records
Copy-Item -LiteralPath (Join-Path $VfxPacket 'full-manifest.json') -Destination (Join-Path $Run 'original-vfx-full-manifest.json')
Copy-Item -LiteralPath (Join-Path $Project 'tmp\SkillTiming20261113\Delivery\validation.json') -Destination (Join-Path $Run 'original-g08-validation.json')
$Results = @()
$Results += Git $Candidate '01-init' @('init','--initial-branch=Sprint#9-33-combined-g08-vfx')
if ($Results[-1].exit_code -ne 0) { throw 'Local git init failed' }
$Results += Git $Candidate '02-config-autocrlf' @('config','--local','core.autocrlf','true')
$Results += Git $Candidate '03-stage-baseline' @('add','--all')
$Results += Git $Candidate '04-g08-check' @('apply','--check',$G08Patch)
if ($Results[-1].exit_code -ne 0) { WriteJson (Join-Path $Run 'sequence-results.json') $Results; throw 'G08 check failed' }
$Results += Git $Candidate '05-g08-apply' @('apply',$G08Patch)
if ($Results[-1].exit_code -ne 0) { WriteJson (Join-Path $Run 'sequence-results.json') $Results; throw 'G08 apply failed' }
$Results += Git $Candidate '06-vfx-after-g08-check' @('apply','--check',$VfxPatch)
if ($Results[-1].exit_code -eq 0) { $Results += Git $Candidate '07-vfx-after-g08-apply' @('apply',$VfxPatch) }
WriteJson (Join-Path $Run 'sequence-results.json') $Results
WriteJson (Join-Path $Run 'run-metadata.json') ([ordered]@{created_utc=[DateTime]::UtcNow.ToString('o');project_read_only=$Project;run=$Run;baseline=$Baseline;candidate=$Candidate;branch='Sprint#9-33-combined-g08-vfx';g08_patch=$G08Patch;g08_patch_sha256=(Sha $G08Patch);vfx_patch=$VfxPatch;vfx_patch_sha256=(Sha $VfxPatch);union_files=$Paths.Count;shared_writes=0;ue_ubt_editor_game_calls=0;commits_pushes=0;source_content_links=0;order='G08 then VFX';sequence_passed=($Results[-1].exit_code -eq 0)})
$Results | ConvertTo-Json -Depth 6
