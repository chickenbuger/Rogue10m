param(
    [Parameter(Mandatory=$true)][ValidatePattern('^[A-Za-z0-9_]{1,24}$')][string]$Label,
    [switch]$Rendered,
    [ValidateRange(60,180)][int]$TimeoutSeconds = 120,
    [string]$ProjectRoot = (Split-Path -Parent $PSScriptRoot)
)
$ErrorActionPreference = 'Stop'
$utf8 = [Text.UTF8Encoding]::new($false)
$project = [IO.Path]::GetFullPath($ProjectRoot)
$saveRoot = Join-Path $project 'Saved\SaveGames'
$packet = Join-Path $project 'Feature\doc\evidence\safe-menu-pause-20261103'
$output = Join-Path $packet $Label
$images = Join-Path $project ('DevLog\20261103_SafeMenuPause_'+$Label)
$log = Join-Path $packet ($Label+'.log')
$receipt = Join-Path $packet ($Label+'-launcher.json')
$profile = 'Rogue10m_Town_QA_MenuPause_'+$Label
$prefix = 'Rogue10m_Progression_QA_MenuPause_'+$Label
$keys = 'Rogue10m_Fighter_QA_MenuPause_'+$Label
function Inventory {
    $root = Get-Item -LiteralPath $saveRoot
    if (($root.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) { throw 'Linked save directory refused.' }
    $rows = @{}
    foreach ($file in @(Get-ChildItem -LiteralPath $saveRoot -File -Recurse -Filter '*.sav')) {
        if (($file.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) { throw 'Linked save file refused.' }
        $rows[$file.FullName] = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash
    }
    return $rows
}
foreach ($path in @($output,$images,$log,$receipt)) {
    if (Test-Path -LiteralPath $path) { throw 'Existing evidence must be preserved; choose a fresh label.' }
}
foreach ($name in @(($profile+'.sav'),($keys+'.sav'))) {
    if (Test-Path -LiteralPath (Join-Path $saveRoot $name)) { throw 'Existing QA save namespace refused.' }
}
if (@(Get-ChildItem -LiteralPath $saveRoot -Filter ($prefix+'_*') -File).Count) { throw 'Existing QA progression namespace refused.' }
if (@(Get-Process -Name 'UnrealEditor','UnrealEditor-Cmd','UnrealBuildTool' -ErrorAction SilentlyContinue).Count) {
    throw 'Another Unreal task is running. No launch or process termination performed.'
}
[IO.Directory]::CreateDirectory($packet) | Out-Null
$before = Inventory
$exe = 'D:\Program Files\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$arguments = @(('"'+(Join-Path $project 'Rogue10m.uproject')+'"'),'/Game/Rogue10m/Maps/StartHub',
    '-game','-unattended','-nosplash','-nosound','-NoLiveCoding','-NoRemoteShaderCompile',
    ('-RogueProfileSaveSlot='+$profile),('-RogueProgressionSavePrefix='+$prefix),('-RogueSkillKeySaveSlot='+$keys),
    ('-RogueSafeMenuPauseLabel='+$Label),'-ExecCmds="t.MaxFPS 60,Rogue10m.TestSafeMenuPause"',('-abslog="'+$log+'"'))
if ($Rendered) { $arguments += @('-d3d11','-RenderOffscreen','-windowed','-ForceRes','-ResX=1280','-ResY=720') }
else { $arguments += '-nullrhi' }
$meta = [ordered]@{Label=$Label;Rendered=[bool]$Rendered;StartedUtc=[DateTime]::UtcNow.ToString('o');BeforeCount=$before.Count;OwnedPID=$null;OwnedStartTicks=$null;OwnedPath=$null;ExitCode=$null;Exited=$false;Preserved=$false;Changed=@();Missing=@();UnknownNew=@();NewQA=@();Error=$null;Result='pending'}
function WriteReceipt { [IO.File]::WriteAllText($receipt,(ConvertTo-Json -InputObject $meta -Depth 8),$utf8) }
$owned = $null
$identity = $false
try {
    $owned = Start-Process -FilePath $exe -ArgumentList $arguments -WindowStyle Hidden -PassThru
    $meta.OwnedPID=$owned.Id
    $clock=[Diagnostics.Stopwatch]::StartNew()
    while (-not $identity -and $clock.Elapsed.TotalSeconds -lt 5) {
        $owned.Refresh()
        if ($owned.HasExited) { throw 'Owned QA exited before startup identity was observed.' }
        try {
            $meta.OwnedPath=$owned.Path
            $meta.OwnedStartTicks=$owned.StartTime.ToUniversalTime().Ticks
            $identity=([IO.Path]::GetFullPath($meta.OwnedPath) -ieq [IO.Path]::GetFullPath($exe)) -and $meta.OwnedStartTicks -gt 0
        } catch { $identity=$false }
        if (-not $identity) { Start-Sleep -Milliseconds 100 }
    }
    if (-not $identity) { throw 'Owned startup identity could not be proved; do not stop an unverified process.' }
    WriteReceipt
    Write-Output ('SAFE_MENU_QA_LAUNCHED pid='+$owned.Id+' label='+$Label+' rendered='+[bool]$Rendered)
    while (-not $owned.WaitForExit(1000)) {
        if ($clock.Elapsed.TotalSeconds -ge $TimeoutSeconds) { throw 'Owned safe-menu QA exceeded bounded watchdog.' }
    }
    $meta.Exited=$true; $meta.ExitCode=$owned.ExitCode
    $resultPath = Join-Path $output 'results.txt'
    if ($owned.ExitCode -ne 0 -or -not (Test-Path -LiteralPath $resultPath)) { throw ('Native QA failed; exit='+$owned.ExitCode) }
    $body=[IO.File]::ReadAllText($resultPath)
    if ($body -notmatch '(?m)^failures=0\r?$' -or $body -notmatch '(?m)^checks=(\d+)\r?$' -or [int]$Matches[1] -lt 45 -or $body -match '(?m)^FAIL ') { throw 'Native QA result is incomplete or failing.' }
    if (-not [IO.File]::ReadAllText($log).Contains('RESULT=SAFE_MENU_PAUSE_PASSED')) { throw 'Native PASS log missing.' }
    if ($Rendered -and @(Get-ChildItem -LiteralPath $images -File -Filter '*.png').Count -ne 3) { throw 'Rendered evidence requires three actual screenshots.' }
    $meta.Result='native_checks_passed'
} catch {
    $meta.Error=$_.Exception.Message
    $meta.Result='failed_or_incomplete'
} finally {
    if ($null -ne $owned) {
        $owned.Refresh()
        if (-not $owned.HasExited -and $identity) {
            $live=Get-Process -Id $owned.Id -ErrorAction SilentlyContinue
            if ($live -and $live.StartTime.ToUniversalTime().Ticks -eq $meta.OwnedStartTicks -and $live.Path -ieq $meta.OwnedPath) {
                $owned.Kill(); $owned.WaitForExit(5000) | Out-Null
            }
        }
        $owned.Refresh();$meta.Exited=$owned.HasExited
        if ($owned.HasExited) { $meta.ExitCode=$owned.ExitCode }
    }
    if ($null -eq $owned -or $meta.Exited) {
        $after=Inventory
        $meta.AfterCount=$after.Count
        $meta.Changed=@($before.Keys | Where-Object {$after.ContainsKey($_) -and $before[$_] -cne $after[$_]})
        $meta.Missing=@($before.Keys | Where-Object {-not $after.ContainsKey($_)})
        foreach ($path in @($after.Keys | Where-Object {-not $before.ContainsKey($_)})) {
            $name=[IO.Path]::GetFileName($path)
            $isRoot=([IO.Path]::GetDirectoryName($path) -ieq [IO.Path]::GetFullPath($saveRoot))
            $allowed=$isRoot -and ($name -ceq ($profile+'.sav') -or $name -ceq ($keys+'.sav') -or $name -cmatch ('^'+[regex]::Escape($prefix)+'_[0-9A-Fa-f]{32}_v5\.sav$'))
            if ($allowed) { $meta.NewQA+= $name } else { $meta.UnknownNew+= $name }
        }
        $meta.Preserved=$meta.Changed.Count -eq 0 -and $meta.Missing.Count -eq 0 -and $meta.UnknownNew.Count -eq 0
    }
    $meta.FinishedUtc=[DateTime]::UtcNow.ToString('o')
    WriteReceipt
}
if ($meta.Result -cne 'native_checks_passed' -or -not $meta.Preserved -or -not $meta.Exited) {
    Write-Output (ConvertTo-Json -InputObject $meta -Depth 6)
    throw ('Safe-menu QA incomplete: '+$meta.Error+' receipt='+$receipt)
}
Get-Content -LiteralPath (Join-Path $output 'results.txt')
Write-Output ('RESULT=SAFE_MENU_PAUSE_PASSED preservation=verified rendered='+[bool]$Rendered+' receipt='+$receipt)
