$ErrorActionPreference = 'Stop'
$TaskRoot = [IO.Path]::GetFullPath((Split-Path -Parent $PSScriptRoot)).TrimEnd('\')
$TaskEditor = 'D:\Program Files\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$TaskProject = Join-Path $TaskRoot 'Rogue10m.uproject'
$TaskBootstrap = Join-Path $PSScriptRoot 'Editor\ResumeLandscapeQA20261117.py'
$TaskEvidence = Join-Path $TaskRoot 'Feature\doc\evidence\landscape-resume-20261117'
foreach ($TaskPath in @($TaskRoot,(Join-Path $TaskRoot 'Content'))) {
    if ((Get-Item -LiteralPath $TaskPath).Attributes -band [IO.FileAttributes]::ReparsePoint) { throw 'Physical checkout required' }
}
foreach ($TaskPath in @($TaskEditor,$TaskProject,$TaskBootstrap)) {
    if (-not (Test-Path -LiteralPath $TaskPath)) { throw ('Missing '+$TaskPath) }
}
$TaskActive = @(Get-CimInstance Win32_Process | Where-Object {
    $_.Name -match '^(UnrealEditor.*|UnrealBuildTool|dotnet|AutomationTool)\.exe$' -and
    ([string]::IsNullOrWhiteSpace($_.CommandLine) -or $_.CommandLine.Replace('/','\').Contains($TaskRoot,[StringComparison]::OrdinalIgnoreCase))
})
if ($TaskActive.Count) { throw 'Existing project editor/build; preserve it' }
if (Test-Path -LiteralPath (Join-Path $TaskEvidence 'launch.json')) { throw 'Preserve previous launch evidence' }
New-Item -ItemType Directory -Force -Path $TaskEvidence | Out-Null
$TaskLog = Join-Path $TaskEvidence 'editor.log'
$TaskExec = "ModelContextProtocol.StartServer 8001,py '"+($TaskBootstrap.Replace('\','/'))+"'"
$TaskArgs = @(('"'+$TaskProject+'"'),'/Game/Rogue10m/Maps/Production/WorldLayoutCompactCrop20261114A',
 '-NoLiveCoding','-NoSplash','-NoSound','-language=en','-locale=en','-RogueEditorLanguage=ko','-RogueEditorLocale=ko-KR',
 '-RogueProfileSaveSlot=Rogue10m_Town_QA_Landscape20261116A',
 '-RogueProgressionSavePrefix=Rogue10m_Progression_QA_Landscape20261116A',
 '-RogueSkillKeySaveSlot=Rogue10m_Fighter_QA_Landscape20261116A',
 '"-LogCmds=LogAutomationTest Log"',('"-AbsLog='+$TaskLog+'"'),('"-ExecCmds='+$TaskExec+'"'))
$TaskDll = Get-FileHash -LiteralPath (Join-Path $TaskRoot 'Binaries\Win64\UnrealEditor-Rogue10m.dll') -Algorithm SHA256
$TaskProcess = Start-Process -FilePath $TaskEditor -ArgumentList $TaskArgs -WorkingDirectory $TaskRoot -WindowStyle Hidden -PassThru
[ordered]@{pid=$TaskProcess.Id;log=$TaskLog;startedUtc=[DateTime]::UtcNow.ToString('o');arguments=$TaskArgs;dllSha256=$TaskDll.Hash;startupVerified=$false} |
 ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $TaskEvidence 'launch.json') -Encoding utf8
Write-Output ('Editor PID '+$TaskProcess.Id+'; log '+$TaskLog)
