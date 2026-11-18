param(
    [ValidatePattern('^[A-Za-z0-9_]{1,20}$')][string]$RunLabel = 'C1110A',
    [ValidateRange(60,600)][int]$TimeoutSeconds = 240
)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$evidence=Join-Path $root ('Feature\doc\evidence\weapon-skill-loadouts-20261110\'+$RunLabel)
if(Test-Path -LiteralPath $evidence){throw 'Evidence exists. Choose a fresh RunLabel; previous evidence is preserved.'}
New-Item -ItemType Directory -Path $evidence | Out-Null
$stages=@(
    @{ Name='SkillLoadout'; Filter='Rogue10m.SkillLoadout'; Expected=@('Rogue10m.SkillLoadout.SlotPolicy','Rogue10m.SkillLoadout.PresetsAndNovice','Rogue10m.SkillLoadout.ActiveTechniqueCatalog') },
    @{ Name='HybridSkillTree'; Filter='Rogue10m.HybridSkillTree'; Expected=@('Rogue10m.HybridSkillTree.MasteryAlternativesAndLegacy','Rogue10m.HybridSkillTree.MasteryAndRewards','Rogue10m.HybridSkillTree.SharedLedgerAndValidation') },
    @{ Name='HumanoidContact'; Filter='Rogue10m.Monster.HumanoidAnimation'; Expected=@('Rogue10m.Monster.HumanoidAnimation.ContactTiming','Rogue10m.Monster.HumanoidAnimation.ProfileTransitions') }
)
$results=@()
foreach($stage in $stages){
    $dest=Join-Path $evidence $stage.Name
    New-Item -ItemType Directory -Path $dest | Out-Null
    $tag='Skill_'+$RunLabel+'_'+$stage.Name
    $qaProfile='Rogue10m_Town_QA_'+$tag
    $qaProgression='Rogue10m_Progression_QA_'+$tag
    $qaKeys='Rogue10m_Fighter_QA_'+$tag
    $saves=Join-Path $root 'Saved\SaveGames'
    if((Test-Path -LiteralPath (Join-Path $saves ($qaProfile+'.sav'))) -or (Test-Path -LiteralPath (Join-Path $saves ($qaKeys+'.sav'))) -or (Get-ChildItem -LiteralPath $saves -Filter ($qaProgression+'_*') -ErrorAction SilentlyContinue | Select-Object -First 1)){throw 'QA slots already exist. Choose a new label.'}
    $arguments=@(('"'+(Join-Path $root 'Rogue10m.uproject')+'"'),'/Engine/Maps/Entry','-nullrhi','-unattended','-nosplash','-nosound','-NoLiveCoding','-NoRemoteShaderCompile','-language=en','-locale=en-US',('-RogueProfileSaveSlot='+$qaProfile),('-RogueProgressionSavePrefix='+$qaProgression),('-RogueSkillKeySaveSlot='+$qaKeys),('-ExecCmds="Automation RunTests '+$stage.Filter+'"'),'-TestExit="Automation Test Queue Empty"',('-ReportExportPath="'+$dest+'\report"'),('-abslog="'+$dest+'\automation.log"'))
    $process=Start-Process -FilePath 'D:\Program Files\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' -ArgumentList $arguments -WindowStyle Hidden -PassThru
    Write-Output ('Owned automation stage='+$stage.Name+' PID='+$process.Id)
    try{
        if(-not $process.WaitForExit($TimeoutSeconds*1000)){throw 'Automation timed out.'}
        if($process.ExitCode -ne 0){throw ('Editor exit='+$process.ExitCode)}
        $report=Get-Content -LiteralPath (Join-Path $dest 'report\index.json') -Raw | ConvertFrom-Json
        if($report.succeeded -ne $stage.Expected.Count -or $report.succeededWithWarnings -ne 0 -or $report.failed -ne 0 -or $report.notRun -ne 0 -or $report.inProcess -ne 0 -or @($report.tests).Count -ne $stage.Expected.Count){throw ('Unexpected automation counts for '+$stage.Name)}
        foreach($name in $stage.Expected){$test=@($report.tests | Where-Object fullTestPath -eq $name);if($test.Count -ne 1 -or $test[0].state -ne 'Success' -or $test[0].errors -ne 0){throw ('Failed test='+$name)}}
        $results+=@($report.tests | Select-Object fullTestPath,state,errors)
    } finally {if(-not $process.HasExited){Stop-Process -Id $process.Id;$process.WaitForExit(5000) | Out-Null}}
}
$results | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $evidence 'summary.json') -Encoding UTF8
$results | ConvertTo-Json
Write-Output ('RESULT=SKILL_LOADOUT_AND_HUMANOID_CONTACT_PASSED EVIDENCE='+$evidence)