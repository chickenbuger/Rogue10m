$ErrorActionPreference='Stop'
$TaskRoot='D:\Project\Rogue10m'
$TaskExe='D:\Program Files\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$TaskOut=Join-Path $TaskRoot 'Feature\doc\evidence\landscape-natural-boundary20261117\author-host'
foreach ($TaskPath in @($TaskRoot,(Join-Path $TaskRoot 'Content'))) {
    if ((Get-Item -LiteralPath $TaskPath).Attributes -band [IO.FileAttributes]::ReparsePoint) { throw 'Independent physical checkout required' }
}
$TaskActive=@(Get-CimInstance Win32_Process | Where-Object {
    $_.Name -match '^(UnrealEditor.*|UnrealBuildTool|AutomationTool)\.exe$'
})
if ($TaskActive.Count) { throw 'Existing editor/build must be preserved' }
if (Test-Path -LiteralPath $TaskOut) { throw 'Existing author launch evidence; no automatic retry' }
New-Item -ItemType Directory -Path $TaskOut | Out-Null
$TaskLog=Join-Path $TaskOut 'engine.log'
$TaskArgs=@(('"'+$TaskRoot+'\Rogue10m.uproject"'),'/Game/Rogue10m/Maps/Production/WorldLayoutCompactCrop20261114A',
 '-NoLiveCoding','-NoSplash','-NoSound','-language=en','-locale=en','-RogueEditorLanguage=ko','-RogueEditorLocale=ko-KR',
 '-RogueNaturalStage=author',
 '-RogueProfileSaveSlot=Rogue10m_Town_QA_Landscape20261116A',
 '-RogueProgressionSavePrefix=Rogue10m_Progression_QA_Landscape20261116A',
 '-RogueSkillKeySaveSlot=Rogue10m_Fighter_QA_Landscape20261116A',
 '-ini:EditorPerProjectUserSettings:[/Script/UnrealEd.EditorLoadingSavingSettings]:bAutoSaveEnable=False',
 '-ini:EditorPerProjectUserSettings:[/Script/ModelContextProtocolEngine.ModelContextProtocolSettings]:bAutoStartServer=False',
 ('"-AbsLog='+$TaskLog+'"'),
 '"-ExecCmds=ModelContextProtocol.StartServer 8001,py D:/Project/Rogue10m/Scripts/Editor/AuthorNaturalWorld20261117.py"')
$TaskProcess=Start-Process -FilePath $TaskExe -ArgumentList $TaskArgs -WorkingDirectory $TaskRoot -WindowStyle Hidden -PassThru
[ordered]@{pid=$TaskProcess.Id;exe=$TaskExe;arguments=$TaskArgs;log=$TaskLog;startedUtc=[DateTime]::UtcNow.ToString('o');
 dllSha256=(Get-FileHash -LiteralPath (Join-Path $TaskRoot 'Binaries\Win64\UnrealEditor-Rogue10m.dll')).Hash} |
 ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $TaskOut 'launch.json') -Encoding utf8
Write-Output ('Owned editor PID '+$TaskProcess.Id+'; '+$TaskLog)
