param(
    [Parameter(Mandatory=$true)][ValidatePattern('^[A-Za-z0-9_]{1,24}$')][string]$Label,
    [string]$ProjectRoot=(Split-Path -Parent $PSScriptRoot)
)
$RunPath=Join-Path (Join-Path $ProjectRoot 'DevLog\BasicRunPlayQA') $Label
$ErrorActionPreference='Stop'
function Read-SharedSnapshot([string]$Path) {
    if(-not(Test-Path -LiteralPath $Path)){return ''}
    $stream=$null;$reader=$null
    try {
        $stream=[IO.File]::Open($Path,[IO.FileMode]::Open,[IO.FileAccess]::Read,([IO.FileShare]::ReadWrite -bor [IO.FileShare]::Delete))
        $reader=[IO.StreamReader]::new($stream,[Text.Encoding]::UTF8,$true)
        $text=$reader.ReadToEnd()
        $last=$text.LastIndexOf("`n")
        if($last -ge 0){return $text.Substring(0,$last+1)}
        return ''
    } catch [IO.IOException] {
        return ''
    } finally {
        if($reader){$reader.Dispose()}elseif($stream){$stream.Dispose()}
    }
}
$samples=Read-SharedSnapshot (Join-Path $RunPath 'Driver\samples.csv')
if($samples){$samples|ConvertFrom-Csv|Select-Object -Last 1 wall,stage,map,phase,remaining,hp,level,xp,mastery,target,target_hp|ConvertTo-Json -Compress}
$events=Read-SharedSnapshot (Join-Path $RunPath 'Driver\events.csv')
if($events){$events -split "`n"|Where-Object {$_ -match 'stage|boss_reached|great_sword_growth_|next_run_|blocked'}|Select-Object -Last 4}
