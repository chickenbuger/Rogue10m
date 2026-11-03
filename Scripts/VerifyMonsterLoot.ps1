param([Parameter(Mandatory)][ValidatePattern('^[A-Za-z0-9_]{1,24}$')][string]$Label,[string]$ProjectRoot=(Split-Path -Parent $PSScriptRoot))
$ErrorActionPreference='Stop'
$r=(Resolve-Path -LiteralPath $ProjectRoot).Path;$project=Join-Path $r 'Rogue10m.uproject'
$out=Join-Path $r ('Feature\doc\evidence\monster-loot-20261103\launchers\'+$Label)
if(Test-Path -LiteralPath $out){throw 'Fresh output required'}
if(@(Get-CimInstance Win32_Process | Where-Object {$_.Name -like 'UnrealEditor*' -and $_.CommandLine -and $_.CommandLine.Contains($project)}).Count){throw 'Project process already running'}
New-Item -ItemType Directory -Path $out | Out-Null
function Saves {
 @(Get-ChildItem -LiteralPath (Join-Path $r 'Saved\SaveGames') -Recurse -Filter '*.sav' -File | ForEach-Object {[pscustomobject]@{path=$_.FullName;sha256=(Get-FileHash -LiteralPath $_.FullName).Hash}})
}
$before=@(Saves);$before | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $out 'saves-before.json')
$failure=$null
try {
 & (Join-Path $PSScriptRoot 'RunMonsterLootRuntime.ps1') -Label $Label -Project $project -LogDirectory (Join-Path $out 'runtime') -TimeoutSeconds 220
} catch {$failure=$_} finally {
 $after=@(Saves);$after | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $out 'saves-after.json')
 $lookup=@{};foreach($s in $after){$lookup[$s.path]=$s.sha256}
 $changed=@($before | Where-Object {-not $lookup.ContainsKey($_.path) -or $lookup[$_.path] -ne $_.sha256})
 $oldPaths=@{};foreach($s in $before){$oldPaths[$s.path]=$true}
 $new=@($after | Where-Object {-not $oldPaths.ContainsKey($_.path)})
 $town='Rogue10m_Town_QA_MonsterLoot_'+$Label+'.sav';$keys='Rogue10m_Fighter_QA_MonsterLoot_'+$Label+'.sav';$prefix='Rogue10m_Progression_QA_MonsterLoot_'+$Label+'_'
 $unexpected=@($new | Where-Object {$n=[IO.Path]::GetFileName($_.path); $n -cne $town -and $n -cne $keys -and -not $n.StartsWith($prefix,[StringComparison]::Ordinal)})
 [pscustomobject]@{label=$Label;before=$before.Count;after=$after.Count;originalChanged=$changed.Count;unexpectedNew=$unexpected.Count;failure=if($failure){$failure.ToString()}else{$null}} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $out 'launcher.json')
 if($changed.Count -or $unexpected.Count){throw 'Save preservation/namespace verification failed'}
}
if($failure){throw $failure}
Write-Output ('Loot runtime completed; original saves unchanged '+$before.Count+' -> '+$after.Count)
