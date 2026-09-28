param([string]$ProjectRoot = (Split-Path -Parent $PSScriptRoot))
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'GetObsidianFeatureEvidence.ps1')
$fixture = Join-Path $ProjectRoot ('tmp/obsidian-feature-tests/' + [guid]::NewGuid().ToString('N'))
$sourceRoot = Join-Path $fixture 'Source/Rogue10m'
$utf8 = New-Object System.Text.UTF8Encoding($false)
function Write-Fixture([string]$Relative, [string]$Content) {
    $file = Join-Path $fixture $Relative
    [IO.Directory]::CreateDirectory((Split-Path -Parent $file)) | Out-Null
    [IO.File]::WriteAllText($file, $Content, $utf8)
}
Write-Fixture 'Source/Rogue10m/Data/Rogue10mSkillLoadoutDataAsset.h' ('class URogue10mForwardOnly;' + [Environment]::NewLine + 'class ROGUE10M_API URogue10mWeaponSkillProfileDataAsset : public UDataAsset {};')
Write-Fixture 'Source/Rogue10m/Data/Rogue10mSkillLoadoutDataAsset.cpp' ''
Write-Fixture 'Source/Rogue10m/UI/Rogue10mHudWidgetParts.h' 'class ROGUE10M_API URogue10mTestWidget : public UObject {};'
Write-Fixture 'Source/Rogue10m/UI/Rogue10mHudWidgetParts.cpp' ''
Write-Fixture 'Source/Rogue10m/A/Rogue10mDuplicateComponent.h' ''
Write-Fixture 'Source/Rogue10m/B/Rogue10mDuplicateComponent.h' ''
Write-Fixture 'Feature/doc/fixture-skill.md' (@('URogue10mWeaponSkillProfileDataAsset','HudWidgetParts.cpp/.h','URogue10mForwardOnly Rogue10mMissingComponent Rogue10mDuplicateComponent','Rogue10mHudWidgetParts.cpp') -join [Environment]::NewLine)
Write-Fixture 'Feature/architect/fixture-skill.md' 'HudWidgetParts.h/.cpp'
Write-Fixture 'Feature/doc/README.md' 'Rogue10mDuplicateComponent'
$paths = @('Data/Rogue10mSkillLoadoutDataAsset.h','Data/Rogue10mSkillLoadoutDataAsset.cpp','UI/Rogue10mHudWidgetParts.h','UI/Rogue10mHudWidgetParts.cpp','A/Rogue10mDuplicateComponent.h','B/Rogue10mDuplicateComponent.h')
$groups = @([pscustomobject]@{name='Skill';description='Fixture';documentPatterns=@('*skill*')})
$report = Get-ObsidianFeatureEvidence -ProjectRoot $fixture -SourceRoot $sourceRoot -SourcePaths $paths -Groups $groups
$record = $report.functions[0]
if ($record.sources.Count -ne 4) { throw "Expected four supported sources, got $($record.sources.Count)." }
if (@($record.evidence | Where-Object { $_.document -eq 'Feature/doc/fixture-skill.md' -and $_.line -eq 1 }).Count -ne 2) { throw 'Owned symbol must resolve to its containing header and implementation.' }
if (@($record.evidence | Where-Object { $_.document -eq 'Feature/doc/fixture-skill.md' -and $_.line -eq 2 }).Count -ne 2) { throw 'Reverse paired shorthand must resolve both files.' }
if (@($record.evidence | Where-Object { $_.document -eq 'Feature/doc/fixture-skill.md' -and $_.line -eq 3 }).Count -ne 0) { throw 'Forward-only, absent and ambiguous references must not create edges.' }
if (@($record.evidence | Where-Object { $_.document -eq 'Feature/doc/fixture-skill.md' -and $_.line -eq 4 }).Count -ne 1) { throw 'An explicit .cpp reference must not add a header edge.' }
if (@($record.evidence | Where-Object kind -eq 'plan').Count -ne 2) { throw 'Plan evidence must remain distinguishable.' }
if ($report.scannedDocumentCount -ne 2) { throw 'README must not be scanned.' }
Write-Output 'Feature evidence regression checks passed: owned symbols, paired shorthand, ambiguity, missing sources, forward declarations, explicit suffixes, plan/result distinction.'
