$ErrorActionPreference='Stop'
$TaskRoot='D:\Project\Rogue10m'
$TaskExe='D:\Program Files\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$TaskOut=Join-Path $TaskRoot 'Feature\doc\evidence\landscape-natural-boundary20261117\refinement-host'
$TaskActive=@(Get-CimInstance Win32_Process | Where-Object { $_.Name -match '^(UnrealEditor.*|UnrealBuildTool|AutomationTool)\.exe$' })
if($TaskActive.Count){throw 'Preserve existing editor/build'}
if(Test-Path -LiteralPath $TaskOut){throw 'Existing refinement launch; no automatic retry'}
New-Item -ItemType Directory -Path $TaskOut | Out-Null
$TaskArgs=@(('"'+$TaskRoot+'\Rogue10m.uproject"'),'/Game/Rogue10m/Maps/Production/WorldLayoutCompactCrop20261114A',
 '-NoLiveCoding','-NoSplash','-NoSound','-language=en','-locale=en','-RogueEditorLanguage=ko','-RogueEditorLocale=ko-KR','-RogueNaturalStage=refine',
 '-RogueProfileSaveSlot=Rogue10m_Town_QA_Landscape20261116A','-RogueProgressionSavePrefix=Rogue10m_Progression_QA_Landscape20261116A','-RogueSkillKeySaveSlot=Rogue10m_Fighter_QA_Landscape20261116A',
 '-ini:EditorPerProjectUserSettings:[/Script/UnrealEd.EditorLoadingSavingSettings]:bAutoSaveEnable=False',
 '-ini:EditorPerProjectUserSettings:[/Script/ModelContextProtocolEngine.ModelContextProtocolSettings]:bAutoStartServer=False',
 ('"-AbsLog='+(Join-Path $TaskOut 'engine.log')+'"'),
 '"-ExecCmds=ModelContextProtocol.StartServer 8001,py D:/Project/Rogue10m/Scripts/Editor/RefineNaturalWorld20261118.py"')
$TaskP=Start-Process -FilePath $TaskExe -ArgumentList $TaskArgs -WorkingDirectory $TaskRoot -WindowStyle Hidden -PassThru
[ordered]@{pid=$TaskP.Id;exe=$TaskExe;arguments=$TaskArgs;startedUtc=[DateTime]::UtcNow.ToString('o');
 dllSha256=(Get-FileHash -LiteralPath (Join-Path $TaskRoot 'Binaries\Win64\UnrealEditor-Rogue10m.dll')).Hash} |
 ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $TaskOut 'launch.json') -Encoding utf8
Write-Output ('Refinement editor PID '+$TaskP.Id)
