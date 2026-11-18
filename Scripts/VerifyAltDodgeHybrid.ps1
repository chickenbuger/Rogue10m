param(
    [Parameter(Mandatory=$true)][ValidatePattern('^[A-Za-z0-9_]{1,24}$')][string]$Label,
    [ValidateSet('runtime','read')][string]$Phase='runtime',
    [ValidateRange(60,360)][int]$TimeoutSeconds=300,
    [string]$ProjectRoot='D:\Project\Rogue10m'
)
# Isolated runtime/read harness; use a fresh label after building the reviewed source.
# This is programmatic InputKey/Slate integration, not external OS-input play.
$ErrorActionPreference='Stop'
$ProjectRoot=(Resolve-Path -LiteralPath $ProjectRoot).Path
$project=Join-Path $ProjectRoot 'Rogue10m.uproject'
$editor='D:\Program Files\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
if(-not(Test-Path -LiteralPath $project) -or -not(Test-Path -LiteralPath $editor)){throw 'Reviewed Rogue10m checkout and installed UE executable required.'}
if(Get-Process -Name 'UnrealEditor','UnrealEditor-Cmd' -ErrorAction SilentlyContinue){throw 'Finish or explicitly coordinate the existing UE process before this isolated QA.'}
$town='Rogue10m_Town_QA_AltDodge_'+$Label
$prefix='Rogue10m_Progression_QA_AltDodge_'+$Label
$keys='Rogue10m_Fighter_QA_AltDodge_'+$Label
$saves=Join-Path $ProjectRoot 'Saved\SaveGames'
$base=Join-Path $ProjectRoot ('Feature\doc\evidence\hybrid-tree-input-polish-20261020\'+$Label)
$output=Join-Path $base $Phase
$log=Join-Path $base ($Phase+'.log')
$beforePath=Join-Path $base ($Phase+'.saves-before.csv')
$afterPath=Join-Path $base ($Phase+'.saves-after.csv')
if((Test-Path -LiteralPath $output) -or (Test-Path -LiteralPath $log) -or
   (Test-Path -LiteralPath $beforePath) -or (Test-Path -LiteralPath $afterPath)){
    throw 'Existing evidence must be preserved; choose a fresh label/phase.'
}
$existing=@(Get-ChildItem -LiteralPath $saves -File -Filter ($prefix+'_*.sav') -ErrorAction SilentlyContinue)
if($Phase -eq 'runtime'){
    if((Test-Path -LiteralPath (Join-Path $saves ($town+'.sav'))) -or
       (Test-Path -LiteralPath (Join-Path $saves ($keys+'.sav'))) -or $existing.Count){
        throw 'Runtime requires entirely fresh exact QA namespaces.'
    }
}else{
    if(-not(Test-Path -LiteralPath (Join-Path $base 'runtime\runtime-results.txt')) -or
       -not(Test-Path -LiteralPath (Join-Path $saves ($town+'.sav'))) -or $existing.Count -lt 1){
        throw 'Read phase requires the prior successful isolated runtime fixture for this label.'
    }
    $runtimeLog=Join-Path $base 'runtime.log'
    $runtimeReceipt=Join-Path $base 'runtime.completed.json'
    if(-not(Test-Path -LiteralPath $runtimeLog) -or -not(Test-Path -LiteralPath $runtimeReceipt)){
        throw 'Read requires a completed runtime wrapper receipt, including save preservation.'
    }
    $receipt=Get-Content -LiteralPath $runtimeReceipt -Raw | ConvertFrom-Json
    if($receipt.label -ne $Label -or $receipt.phase -ne 'runtime' -or
       $receipt.project -ne $ProjectRoot -or $receipt.town_slot -ne $town -or
       $receipt.progression_prefix -ne $prefix -or $receipt.key_slot -ne $keys -or
       $receipt.preservation_passed -ne $true -or $receipt.exit_code -ne 0 -or
       $receipt.result -ne 'RESULT=HYBRID_INPUT_RUNTIME_PASSED' -or
       $receipt.log_sha256 -ne (Get-FileHash -LiteralPath $runtimeLog -Algorithm SHA256).Hash -or
       -not([IO.File]::ReadAllText($runtimeLog).Contains($receipt.result))){
        throw 'Runtime receipt does not match this label, namespace, successful result and preservation.'
    }
}
function SaveInventory {
    @(Get-ChildItem -LiteralPath $saves -Recurse -File -Filter '*.sav' | Sort-Object FullName | ForEach-Object {
        [pscustomobject]@{Path=$_.FullName;SHA256=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash}
    })
}
$before=@(SaveInventory)
New-Item -ItemType Directory -Path $base -Force | Out-Null
$before | Export-Csv -LiteralPath $beforePath -Encoding utf8 -NoTypeInformation
$arguments=@(('"'+$project+'"'),'/Game/Rogue10m/Maps/StartHub','-game','-unattended','-nosplash','-nosound','-NoLiveCoding','-NoRemoteShaderCompile',
    '-d3d11','-RenderOffscreen','-windowed','-ForceRes','-ResX=1920','-ResY=1080',
    ('-RogueProfileSaveSlot='+$town),('-RogueProgressionSavePrefix='+$prefix),('-RogueSkillKeySaveSlot='+$keys),
    '-RogueHybridInputQA',('-RogueHybridEvidenceLabel='+$Label),'-ExecCmds="Rogue10m.TestHybridSkillTree"',('-abslog="'+$log+'"'))
if($Phase -eq 'read'){$arguments+='-RogueHybridRead'}
$process=$null;$failure=$null;$preservationFailure=$null
try{
    $process=Start-Process -FilePath $editor -ArgumentList $arguments -WindowStyle Hidden -PassThru
    Write-Output ('Owned Alt hybrid QA PID='+$process.Id+' phase='+$Phase+' label='+$Label+' log='+$log)
    if(-not $process.WaitForExit($TimeoutSeconds*1000)){throw 'Alt hybrid QA timed out.'}
    $process.Refresh()
    $marker=if($Phase -eq 'read'){'RESULT=HYBRID_INPUT_READ_PASSED'}else{'RESULT=HYBRID_INPUT_RUNTIME_PASSED'}
    if($process.ExitCode -ne 0 -or -not(Test-Path -LiteralPath $log) -or
       -not([IO.File]::ReadAllText($log).Contains($marker))){throw ('Alt hybrid QA failed; exit='+$process.ExitCode+' log='+$log)}
}catch{$failure=$_}finally{
    if($null -ne $process -and -not $process.HasExited){Stop-Process -Id $process.Id;$process.WaitForExit(5000)|Out-Null}
    try{
        $after=@(SaveInventory);$after | Export-Csv -LiteralPath $afterPath -Encoding utf8 -NoTypeInformation
        $lookup=@{};foreach($row in $after){$lookup[$row.Path]=$row.SHA256}
        $isOwn={param($path)$name=[IO.Path]::GetFileName($path);$name -eq ($town+'.sav') -or $name -eq ($keys+'.sav') -or $name -like ($prefix+'_*.sav')}
        # Read mode intentionally changes only its own QA session/key files.
        $preserved=@($before|Where-Object {-not(& $isOwn $_.Path)})
        $changed=@($preserved|Where-Object {-not $lookup.ContainsKey($_.Path) -or $lookup[$_.Path] -ne $_.SHA256})
        if($changed.Count){throw ('Non-QA save preservation failed: '+$changed.Count)}
        $prior=@{};foreach($row in $before){$prior[$row.Path]=$true}
        $unexpected=@($after|Where-Object {-not $prior.ContainsKey($_.Path) -and -not(& $isOwn $_.Path)})
        if($unexpected.Count){throw ('Unexpected save created outside exact QA namespaces: '+$unexpected.Count)}
        Write-Output ('Preserved non-QA saves='+$preserved.Count+'; before/after SHA256 verified.')
    }catch{$preservationFailure=$_}
}
if($null -ne $preservationFailure){throw $preservationFailure}
if($null -ne $failure){throw $failure}

# Publish success only after process completion AND preservation verification.
$completion=Join-Path $base ($Phase+'.completed.json')
if(Test-Path -LiteralPath $completion){throw 'Completion receipt already exists; it must be preserved.'}
[ordered]@{
    label=$Label;phase=$Phase;project=$ProjectRoot;town_slot=$town;progression_prefix=$prefix;key_slot=$keys
    result=$marker;exit_code=$process.ExitCode;preservation_passed=$true
    log_sha256=(Get-FileHash -LiteralPath $log -Algorithm SHA256).Hash
    preserved_non_qa_saves=$preserved.Count;completed_utc=[DateTime]::UtcNow.ToString('o')
} | ConvertTo-Json | Set-Content -LiteralPath $completion -Encoding utf8
Write-Output ('PASS '+$marker+'; wrapper save preservation passed; programmatic InputKey/Slate, external OS input not performed.')
