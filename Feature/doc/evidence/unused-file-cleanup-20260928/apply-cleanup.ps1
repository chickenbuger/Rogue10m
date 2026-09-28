param([switch]$Apply)
$ErrorActionPreference='Stop'
$cleanupRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../../..')).TrimEnd('\')
if($cleanupRoot -ne 'D:\Project\Rogue10m'){throw 'Unexpected cleanup root'}
$cleanupTmp=$cleanupRoot+'\tmp\'
$manifestPath=Join-Path $PSScriptRoot 'selected-cleanup.json'
$manifest=Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
function Get-SafeCleanupFile([string]$Relative,[bool]$MustBeTemporary){
 $absolute=[IO.Path]::GetFullPath((Join-Path $cleanupRoot $Relative))
 if(-not $absolute.StartsWith($cleanupRoot+'\',[StringComparison]::OrdinalIgnoreCase)){throw "Outside workspace: $Relative"}
 if($MustBeTemporary -and -not $absolute.StartsWith($cleanupTmp,[StringComparison]::OrdinalIgnoreCase)){throw "Outside tmp: $Relative"}
 $item=Get-Item -LiteralPath $absolute
 if($item.PSIsContainer){throw "Not a file: $Relative"}
 $ancestor=$item
 while($null -ne $ancestor -and $ancestor.FullName -ne $cleanupRoot){
  if(($ancestor.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0){throw "Reparse path: $Relative"}
  $ancestor=if($ancestor.PSIsContainer){$ancestor.Parent}else{$ancestor.Directory}
 }
 return $absolute
}
$tracked=[Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
Push-Location $cleanupRoot
try{git -c core.quotepath=false ls-files | ForEach-Object {[void]$tracked.Add($_)}; if($LASTEXITCODE -ne 0){throw 'git inventory failed'}}finally{Pop-Location}
$checked=[Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
$retainedHashes=@{}
foreach($dependency in $manifest.retained_dependencies){
 $file=Get-SafeCleanupFile $dependency.path $false
 if((Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash -ne $dependency.sha256){throw "Retained dependency changed: $($dependency.path)"}
 [void]$checked.Add($dependency.path)
 $retainedHashes[$dependency.path]=$dependency.sha256
}
$seen=[Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
foreach($entry in $manifest.files){
 $file=Get-SafeCleanupFile $entry.original $true
 if(-not $seen.Add($entry.original)){throw 'Duplicate deletion target'}
 if($tracked.Contains($entry.original)){throw "Tracked file: $($entry.original)"}
 if($entry.original -match '(?i)(^|/)(before[^/]*|[^/]*backup[^/]*|decoder|tools?|runtimes?|dependencies)(/|$)'){throw 'Protected path'}
 if($entry.kind -eq 'regeneratable_frame' -and $entry.original -notmatch '/labeled[^/]*/frame_[0-9]+\.png$'){throw 'Unexpected generated frame'}
 if($entry.kind -notin @('duplicate','regeneratable_frame')){throw 'Unknown cleanup kind'}
 if($entry.kind -eq 'duplicate' -and (-not $checked.Contains($entry.keep) -or $retainedHashes[$entry.keep] -ne $entry.sha256)){throw 'Unverified or non-identical retained copy'}
 if((Get-Item -LiteralPath $file).Length -ne $entry.bytes -or (Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash -ne $entry.sha256){throw "Original changed: $($entry.original)"}
}
foreach($dependency in $manifest.retained_dependencies){if($seen.Contains($dependency.path)){throw 'Retained file overlaps deletion set'}}
$bytes=($manifest.files | Measure-Object bytes -Sum).Sum
Write-Output ("Verified {0} files, {1:N0} bytes, {2} retained dependencies." -f $manifest.files.Count,$bytes,$manifest.retained_dependencies.Count)
if(-not $Apply){Write-Output 'Preview only. No files removed.';return}
$receiptPath=Join-Path $PSScriptRoot 'deleted-files.jsonl'
if(Test-Path -LiteralPath $receiptPath){throw 'Receipt already exists; inspect prior run before repeating'}
foreach($entry in $manifest.files){
 $file=Get-SafeCleanupFile $entry.original $true
 if((Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash -ne $entry.sha256){throw "Changed during cleanup: $($entry.original)"}
 Remove-Item -LiteralPath $file
 [PSCustomObject]@{original=$entry.original;bytes=$entry.bytes;sha256=$entry.sha256;kind=$entry.kind;removed_at=(Get-Date).ToString('o')} | ConvertTo-Json -Compress | Add-Content -LiteralPath $receiptPath -Encoding utf8
}
foreach($entry in $manifest.files){if(Test-Path -LiteralPath (Join-Path $cleanupRoot $entry.original)){throw 'Removal verification failed'}}
foreach($dependency in $manifest.retained_dependencies){
 $file=Get-SafeCleanupFile $dependency.path $false
 if((Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash -ne $dependency.sha256){throw 'Post-cleanup dependency hash mismatch'}
}
Write-Output ("CLEANUP_PASSED files={0} bytes={1}" -f $manifest.files.Count,$bytes)
