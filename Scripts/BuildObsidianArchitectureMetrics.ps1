param(
    [string]$ProjectRoot = (Split-Path -Parent $PSScriptRoot),
    [string]$SourceRelativePath = 'Source/Rogue10m',
    [string]$OutputRelativePath = 'Docs/Obsidian/ArchitectureMetrics'
)

$ErrorActionPreference = 'Stop'
$Utf8NoBom = New-Object System.Text.UTF8Encoding($false)
$ProjectRoot = [System.IO.Path]::GetFullPath($ProjectRoot)
$SourceRoot = [System.IO.Path]::GetFullPath((Join-Path $ProjectRoot $SourceRelativePath))
$OutputRoot = [System.IO.Path]::GetFullPath((Join-Path $ProjectRoot $OutputRelativePath))
$FilesRoot = Join-Path $OutputRoot 'Files'

if (-not $SourceRoot.StartsWith($ProjectRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "프로젝트 외부 소스 경로는 분석할 수 없습니다: $SourceRoot"
}
if (-not $OutputRoot.StartsWith($ProjectRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "프로젝트 외부 출력 경로는 사용할 수 없습니다: $OutputRoot"
}
if (-not (Test-Path -LiteralPath $SourceRoot -PathType Container)) {
    throw "소스 경로가 없습니다: $SourceRoot"
}

function Get-RelativePath([string]$BasePath, [string]$FullPath) {
    $baseUri = [System.Uri]::new(($BasePath.TrimEnd('\') + '\'))
    $fullUri = [System.Uri]::new($FullPath)
    return [System.Uri]::UnescapeDataString($baseUri.MakeRelativeUri($fullUri).ToString()).Replace('\', '/')
}

function Format-Ratio($Value) {
    if ($null -eq $Value) { return 'N/A' }
    return ('{0:P1}' -f [double]$Value)
}

function Escape-MermaidLabel([string]$Value) {
    return $Value.Replace('"', "'").Replace('[', '(').Replace(']', ')')
}

function Get-NoteRelativePath([string]$SourceRelative) {
    return ('Files/' + $SourceRelative + '.md')
}

function Get-WikiTarget([string]$SourceRelative) {
    return ($OutputRelativePath.Replace('\', '/') + '/' + (Get-NoteRelativePath $SourceRelative).Substring(0, (Get-NoteRelativePath $SourceRelative).Length - 3))
}

if (Test-Path -LiteralPath $FilesRoot) {
    $resolvedFilesRoot = [System.IO.Path]::GetFullPath($FilesRoot)
    if ($resolvedFilesRoot -ne (Join-Path $OutputRoot 'Files')) {
        throw "생성 파일 정리 경로가 예상과 다릅니다: $resolvedFilesRoot"
    }
    Remove-Item -LiteralPath $resolvedFilesRoot -Recurse -Force
}
[System.IO.Directory]::CreateDirectory($FilesRoot) | Out-Null

$sourceFiles = @(Get-ChildItem -LiteralPath $SourceRoot -Recurse -File | Where-Object {
    $_.Extension -in @('.h', '.cpp') -and $_.Name -notmatch '\.(generated\.h|gen\.cpp)$'
} | Sort-Object FullName)

$fileByRelative = @{}
$filesByName = @{}
foreach ($file in $sourceFiles) {
    $relative = Get-RelativePath $SourceRoot $file.FullName
    $fileByRelative[$relative.ToLowerInvariant()] = $file
    $nameKey = $file.Name.ToLowerInvariant()
    if (-not $filesByName.ContainsKey($nameKey)) {
        $filesByName[$nameKey] = [System.Collections.Generic.List[object]]::new()
    }
    $filesByName[$nameKey].Add($file)
}

function Resolve-ProjectInclude([string]$IncludePath) {
    $normalized = $IncludePath.Replace('\', '/').TrimStart('./')
    $key = $normalized.ToLowerInvariant()
    if ($key.StartsWith('rogue10m/')) { $key = $key.Substring('rogue10m/'.Length) }
    if ($fileByRelative.ContainsKey($key)) { return $fileByRelative[$key] }
    $nameKey = [System.IO.Path]::GetFileName($normalized).ToLowerInvariant()
    if ($filesByName.ContainsKey($nameKey) -and $filesByName[$nameKey].Count -eq 1) {
        return $filesByName[$nameKey][0]
    }
    return $null
}

$edges = [System.Collections.Generic.List[object]]::new()
$edgeKeys = @{}
foreach ($file in $sourceFiles) {
    $sourceRelative = Get-RelativePath $SourceRoot $file.FullName
    $sourceFolder = (Split-Path -Parent $sourceRelative).Replace('\', '/')
    if ([string]::IsNullOrWhiteSpace($sourceFolder)) { $sourceFolder = '(Root)' }
    $text = [System.IO.File]::ReadAllText($file.FullName)
    foreach ($match in [regex]::Matches($text, '(?m)^\s*#\s*include\s*["<]([^">]+)[">]')) {
        $target = Resolve-ProjectInclude $match.Groups[1].Value
        if ($null -eq $target) { continue }
        $targetRelative = Get-RelativePath $SourceRoot $target.FullName
        if ($sourceRelative -eq $targetRelative) { continue }
        $edgeKey = ($sourceRelative + '|' + $targetRelative).ToLowerInvariant()
        if ($edgeKeys.ContainsKey($edgeKey)) { continue }
        $edgeKeys[$edgeKey] = $true
        $targetFolder = (Split-Path -Parent $targetRelative).Replace('\', '/')
        if ([string]::IsNullOrWhiteSpace($targetFolder)) { $targetFolder = '(Root)' }
        $edges.Add([pscustomobject]@{
            Source = $sourceRelative
            Target = $targetRelative
            SourceFolder = $sourceFolder
            TargetFolder = $targetFolder
        })
    }
}

$fileMetrics = [System.Collections.Generic.List[object]]::new()
foreach ($file in $sourceFiles) {
    $relative = Get-RelativePath $SourceRoot $file.FullName
    $folder = (Split-Path -Parent $relative).Replace('\', '/')
    if ([string]::IsNullOrWhiteSpace($folder)) { $folder = '(Root)' }
    $outgoing = @($edges | Where-Object Source -eq $relative | Sort-Object Target)
    $incoming = @($edges | Where-Object Target -eq $relative | Sort-Object Source)
    $localOutgoing = @($outgoing | Where-Object TargetFolder -eq $folder).Count
    $localRatio = if ($outgoing.Count -gt 0) { $localOutgoing / [double]$outgoing.Count } else { $null }
    $fileMetrics.Add([pscustomobject]@{
        Path = $relative
        Folder = $folder
        Afferent = $incoming.Count
        Efferent = $outgoing.Count
        TotalCoupling = $incoming.Count + $outgoing.Count
        LocalDependencyRatio = $localRatio
        Incoming = @($incoming | ForEach-Object Source)
        Outgoing = @($outgoing | ForEach-Object Target)
    })
}

$folders = @($fileMetrics.Folder | Sort-Object -Unique)
$folderMetrics = [System.Collections.Generic.List[object]]::new()
foreach ($folder in $folders) {
    $internal = @($edges | Where-Object { $_.SourceFolder -eq $folder -and $_.TargetFolder -eq $folder }).Count
    $outgoing = @($edges | Where-Object { $_.SourceFolder -eq $folder -and $_.TargetFolder -ne $folder }).Count
    $incoming = @($edges | Where-Object { $_.SourceFolder -ne $folder -and $_.TargetFolder -eq $folder }).Count
    $cohesionDenominator = $internal + $outgoing
    $instabilityDenominator = $incoming + $outgoing
    $cohesion = if ($cohesionDenominator -gt 0) { $internal / [double]$cohesionDenominator } else { $null }
    $instability = if ($instabilityDenominator -gt 0) { $outgoing / [double]$instabilityDenominator } else { $null }
    $folderMetrics.Add([pscustomobject]@{
        Folder = $folder
        Files = @($fileMetrics | Where-Object Folder -eq $folder).Count
        InternalEdges = $internal
        IncomingEdges = $incoming
        OutgoingEdges = $outgoing
        Cohesion = $cohesion
        Instability = $instability
    })
}

foreach ($metric in $fileMetrics) {
    $noteRelative = Get-NoteRelativePath $metric.Path
    $notePath = Join-Path $OutputRoot $noteRelative
    $noteDirectory = Split-Path -Parent $notePath
    [System.IO.Directory]::CreateDirectory($noteDirectory) | Out-Null
    $outgoingLinks = if ($metric.Outgoing.Count -gt 0) {
        ($metric.Outgoing | ForEach-Object { "- [[$(Get-WikiTarget $_)|$_]]" }) -join "`r`n"
    } else { '- 없음' }
    $incomingLinks = if ($metric.Incoming.Count -gt 0) {
        ($metric.Incoming | ForEach-Object { "- ``$_``" }) -join "`r`n"
    } else { '- 없음' }
    $sourcePath = ($SourceRelativePath.TrimEnd('/', '\') + '/' + $metric.Path).Replace('\', '/')
    $localYaml = if ($null -eq $metric.LocalDependencyRatio) { 'null' } else { ([double]$metric.LocalDependencyRatio).ToString('0.0000', [System.Globalization.CultureInfo]::InvariantCulture) }
    $content = @"
---
title: $($metric.Path)
tags:
  - rogue10m
  - architecture-metrics
  - generated
source_path: $sourcePath
folder: $($metric.Folder)
afferent_coupling: $($metric.Afferent)
efferent_coupling: $($metric.Efferent)
total_coupling: $($metric.TotalCoupling)
local_dependency_ratio: $localYaml
generated: true
---
# $($metric.Path)

> [!info] 자동 생성 노트
> `Scripts/BuildObsidianArchitectureMetrics.ps1` 실행 시 다시 작성됩니다. 원본 코드는 `$sourcePath`입니다.

## 지표

| Ca | Ce | 총 결합도 | 같은 폴더 의존 비율 |
| ---: | ---: | ---: | ---: |
| $($metric.Afferent) | $($metric.Efferent) | $($metric.TotalCoupling) | $(Format-Ratio $metric.LocalDependencyRatio) |

## 나가는 의존성

$outgoingLinks

## 들어오는 의존성

아래 목록은 참조 정보입니다. 그래프의 화살표는 나가는 include만 나타내며, 유입 탐색은 Obsidian 백링크를 사용할 수 있습니다.

$incomingLinks
"@
    [System.IO.File]::WriteAllText($notePath, ($content.Trim() + "`r`n"), $Utf8NoBom)
}

$folderTable = ($folderMetrics | Sort-Object @{Expression='Cohesion'; Descending=$false}, Folder | ForEach-Object {
    "| ``$($_.Folder)`` | $($_.Files) | $($_.InternalEdges) | $($_.IncomingEdges) | $($_.OutgoingEdges) | $(Format-Ratio $_.Cohesion) | $(Format-Ratio $_.Instability) |"
}) -join "`r`n"

$topCoupling = ($fileMetrics | Sort-Object @{Expression='TotalCoupling'; Descending=$true}, Path | Select-Object -First 20 | ForEach-Object {
    "| [[$(Get-WikiTarget $_.Path)|$($_.Path)]] | $($_.Afferent) | $($_.Efferent) | $($_.TotalCoupling) | $(Format-Ratio $_.LocalDependencyRatio) |"
}) -join "`r`n"

$folderId = @{}
for ($i = 0; $i -lt $folders.Count; $i++) { $folderId[$folders[$i]] = "F$i" }
$mermaidNodes = ($folderMetrics | Sort-Object Folder | ForEach-Object {
    "    $($folderId[$_.Folder])[`"$(Escape-MermaidLabel $_.Folder)<br/>응집도 $(Format-Ratio $_.Cohesion)`"]"
}) -join "`r`n"
$folderEdges = @($edges | Where-Object { $_.SourceFolder -ne $_.TargetFolder } | Group-Object SourceFolder, TargetFolder | Sort-Object Name)
$mermaidEdges = ($folderEdges | ForEach-Object {
    $first = $_.Group[0]
    "    $($folderId[$first.SourceFolder]) -->|$($_.Count)| $($folderId[$first.TargetFolder])"
}) -join "`r`n"

$generatedAt = (Get-Date).ToString('yyyy-MM-dd HH:mm:ss K')
$dashboard = @"
---
title: Rogue10m 아키텍처 지표
tags:
  - rogue10m
  - architecture
  - generated
generated_at: $generatedAt
source_files: $($sourceFiles.Count)
dependency_edges: $($edges.Count)
---
# Rogue10m 아키텍처 지표

> [!warning] 해석 범위
> 이 대시보드는 프로젝트 내부 `#include` 정적 관계만 분석합니다. 런타임 호출, Blueprint 참조, 리플렉션·에셋 의존성은 포함하지 않으며 점수는 설계 품질의 절대 판정이 아닙니다.

## 요약

- 분석 파일: **$($sourceFiles.Count)**개
- 프로젝트 내부 include 관계: **$($edges.Count)**개
- 분석 폴더: **$($folderMetrics.Count)**개
- 시각 탐색: [[Docs/Obsidian/ArchitectureMetrics/Rogue10m Architecture.canvas|Rogue10m Architecture Canvas]]

## 폴더 의존 그래프

~~~mermaid
flowchart LR
$mermaidNodes
$mermaidEdges
~~~

선의 숫자는 폴더 사이의 고유 include 관계 수입니다.

## 폴더 응집도와 안정성

| 폴더 | 파일 | 내부선 | 유입선 | 유출선 | 응집도 | 불안정도 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
$folderTable

## 결합도가 높은 파일

| 파일 | Ca | Ce | 총 결합도 | 같은 폴더 의존 비율 |
| --- | ---: | ---: | ---: | ---: |
$topCoupling

## 계산 기준

- **Ca**: 이 파일을 include하는 프로젝트 파일 수
- **Ce**: 이 파일이 include하는 프로젝트 파일 수
- **총 결합도**: `Ca + Ce`
- **파일 로컬 의존 비율**: 같은 폴더로 나가는 include / 전체 프로젝트 내부 include
- **폴더 응집도**: 내부선 / (내부선 + 외부 유출선)
- **폴더 불안정도**: 외부 유출선 / (외부 유입선 + 외부 유출선)
- 분모가 0이면 `N/A`로 표시합니다.

## 소스 파일 그래프 보기

Obsidian 전체 그래프의 검색 필터에 다음 값을 사용합니다. README, DevLog, Feature 문서, 대시보드와 Canvas는 그래프에서 제외됩니다.

~~~text
(path:"$($OutputRelativePath.Replace('\', '/'))/Files/" OR path:"$($OutputRelativePath.Replace('\', '/'))/Functions/") -path:"Feature/"
~~~

소스 노드는 C++ .h/.cpp 파일에 대응합니다. 소스 A → 소스 B는 include 관계이며, 기능 → 소스는 해당 기능의 Feature 본문에서 확인한 소스 언급이며, 근거 문서와 행은 기능 노트에서 확인합니다. 공유 소스는 여러 기능에 연결되고, 연결이 없는 소스 파일도 표시합니다.
태그·첨부파일은 숨기고 화살표를 켭니다. 기본 경로를 사용할 때 OpenObsidianArchitecture.bat가 이 설정을 저장합니다.
이미 열린 Obsidian에 반영되지 않으면 그래프 검색창에 위 필터를 입력하거나 앱을 다시 불러옵니다.

## 갱신

~~~powershell
.\Scripts\BuildObsidianArchitectureMetrics.ps1
.\Scripts\TestObsidianArchitectureMetrics.ps1
~~~
"@
[System.IO.Directory]::CreateDirectory($OutputRoot) | Out-Null
[System.IO.File]::WriteAllText((Join-Path $OutputRoot 'Dashboard.md'), ($dashboard.Trim() + "`r`n"), $Utf8NoBom)

$canvasNodes = [System.Collections.Generic.List[object]]::new()
$canvasEdges = [System.Collections.Generic.List[object]]::new()
$sortedFolders = @($folderMetrics | Sort-Object Folder)
for ($i = 0; $i -lt $sortedFolders.Count; $i++) {
    $metric = $sortedFolders[$i]
    $column = $i % 3
    $row = [Math]::Floor($i / 3)
    $canvasNodes.Add([ordered]@{
        id = $folderId[$metric.Folder]
        type = 'text'
        text = "# $($metric.Folder)`n파일 $($metric.Files)개`n응집도 $(Format-Ratio $metric.Cohesion)`nCa $($metric.IncomingEdges) · Ce $($metric.OutgoingEdges)"
        x = [int]($column * 440)
        y = [int]($row * 260)
        width = 360
        height = 180
    })
}
for ($i = 0; $i -lt $folderEdges.Count; $i++) {
    $group = $folderEdges[$i]
    $first = $group.Group[0]
    $canvasEdges.Add([ordered]@{
        id = "E$i"
        fromNode = $folderId[$first.SourceFolder]
        fromSide = 'right'
        toNode = $folderId[$first.TargetFolder]
        toSide = 'left'
        label = [string]$group.Count
    })
}
$canvas = [ordered]@{ nodes = $canvasNodes; edges = $canvasEdges } | ConvertTo-Json -Depth 8
[System.IO.File]::WriteAllText((Join-Path $OutputRoot 'Rogue10m Architecture.canvas'), ($canvas + "`r`n"), $Utf8NoBom)

$report = [ordered]@{
    generatedAt = $generatedAt
    projectRoot = $ProjectRoot
    sourceRoot = $SourceRoot
    outputRoot = $OutputRoot
    sourceFileCount = $sourceFiles.Count
    dependencyEdgeCount = $edges.Count
    folderCount = $folderMetrics.Count
    files = $fileMetrics
    folders = $folderMetrics
    folderEdges = @($folderEdges | ForEach-Object {
        [ordered]@{ source = $_.Group[0].SourceFolder; target = $_.Group[0].TargetFolder; count = $_.Count }
    })
}
$reportJson = $report | ConvertTo-Json -Depth 8
[System.IO.File]::WriteAllText((Join-Path $OutputRoot 'metrics.json'), ($reportJson + "`r`n"), $Utf8NoBom)

& (Join-Path $PSScriptRoot 'BuildObsidianFunctionGraph.ps1') -ProjectRoot $ProjectRoot -OutputRelativePath $OutputRelativePath

Write-Output "Obsidian architecture metrics generated."
Write-Output "Files: $($sourceFiles.Count), edges: $($edges.Count), folders: $($folderMetrics.Count)"
Write-Output "Dashboard: $(Join-Path $OutputRoot 'Dashboard.md')"
