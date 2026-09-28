param(
    [string]$ProjectRoot = (Split-Path -Parent $PSScriptRoot),
    [string]$SourceRelativePath = 'Source/Rogue10m',
    [string]$OutputRelativePath = 'Docs/Obsidian/ArchitectureMetrics'
)

$ErrorActionPreference = 'Stop'
$ProjectRoot = [System.IO.Path]::GetFullPath($ProjectRoot)
$SourceRoot = Join-Path $ProjectRoot $SourceRelativePath
$OutputRoot = Join-Path $ProjectRoot $OutputRelativePath
$problems = [System.Collections.Generic.List[string]]::new()

foreach ($required in @('Dashboard.md', 'Rogue10m Architecture.canvas', 'metrics.json', 'functions.json')) {
    if (-not (Test-Path -LiteralPath (Join-Path $OutputRoot $required) -PathType Leaf)) {
        $problems.Add("필수 생성물이 없습니다: $required")
    }
}
if ($problems.Count -gt 0) { $problems | ForEach-Object { Write-Error $_ }; exit 1 }

$metrics = Get-Content -LiteralPath (Join-Path $OutputRoot 'metrics.json') -Raw -Encoding UTF8 | ConvertFrom-Json
$canvas = Get-Content -LiteralPath (Join-Path $OutputRoot 'Rogue10m Architecture.canvas') -Raw -Encoding UTF8 | ConvertFrom-Json
$sourceCount = @(Get-ChildItem -LiteralPath $SourceRoot -Recurse -File | Where-Object { $_.Extension -in @('.h', '.cpp') -and $_.Name -notmatch '\.(generated\.h|gen\.cpp)$' }).Count
$noteCount = @(Get-ChildItem -LiteralPath (Join-Path $OutputRoot 'Files') -Recurse -File -Filter '*.md').Count

if ($metrics.sourceFileCount -ne $sourceCount) { $problems.Add("원본 파일 수와 metrics.json이 다릅니다: $sourceCount / $($metrics.sourceFileCount)") }
if ($noteCount -ne $sourceCount) { $problems.Add("원본 파일 수와 생성 노트 수가 다릅니다: $sourceCount / $noteCount") }
if (@($canvas.nodes).Count -ne $metrics.folderCount) { $problems.Add('Canvas 노드 수와 폴더 수가 다릅니다.') }
if ($metrics.dependencyEdgeCount -lt 1) { $problems.Add('프로젝트 내부 include 관계를 찾지 못했습니다.') }

foreach ($file in $metrics.files) {
    $notePath = Join-Path $OutputRoot ('Files/' + $file.path + '.md')
    if (-not (Test-Path -LiteralPath $notePath -PathType Leaf)) {
        $problems.Add("원본에 대응하는 노트가 없습니다: $($file.path)")
        continue
    }
    $noteText = Get-Content -LiteralPath $notePath -Raw -Encoding UTF8
    $actualTargets = @([regex]::Matches($noteText, '\[\[([^\]|#]+)(?:\|[^\]]+)?\]\]') | ForEach-Object { $_.Groups[1].Value } | Sort-Object)
    $expectedTargets = @($file.outgoing | ForEach-Object { $OutputRelativePath.Replace('\', '/') + '/Files/' + $_ } | Sort-Object)
    if (($actualTargets -join [Environment]::NewLine) -cne ($expectedTargets -join [Environment]::NewLine)) {
        $problems.Add("노트 링크가 실제 나가는 include와 다릅니다: $($file.path)")
    }
    if ($file.afferent -lt 0 -or $file.efferent -lt 0 -or $file.totalCoupling -ne ($file.afferent + $file.efferent)) {
        $problems.Add("파일 결합도 값이 올바르지 않습니다: $($file.path)")
    }
    if ($null -ne $file.localDependencyRatio -and ($file.localDependencyRatio -lt 0 -or $file.localDependencyRatio -gt 1)) {
        $problems.Add("파일 로컬 의존 비율 범위가 올바르지 않습니다: $($file.path)")
    }
}
foreach ($folder in $metrics.folders) {
    if ($null -ne $folder.cohesion -and ($folder.cohesion -lt 0 -or $folder.cohesion -gt 1)) {
        $problems.Add("폴더 응집도 범위가 올바르지 않습니다: $($folder.folder)")
    }
    if ($null -ne $folder.instability -and ($folder.instability -lt 0 -or $folder.instability -gt 1)) {
        $problems.Add("폴더 불안정도 범위가 올바르지 않습니다: $($folder.folder)")
    }
}

$functionReport = Get-Content -LiteralPath (Join-Path $OutputRoot 'functions.json') -Raw -Encoding UTF8 | ConvertFrom-Json
$functionRules = @(Get-Content -LiteralPath (Join-Path $PSScriptRoot 'ObsidianFunctionGroups.json') -Raw -Encoding UTF8 | ConvertFrom-Json)
$functionNotes = @(Get-ChildItem -LiteralPath (Join-Path $OutputRoot 'Functions') -File -Filter '*.md')
if ($functionNotes.Count -ne $functionRules.Count -or $functionReport.functionCount -ne $functionRules.Count) {
    $problems.Add('기능 노드 수와 분류 규칙 수가 다릅니다.')
}
. (Join-Path $PSScriptRoot 'GetObsidianFeatureEvidence.ps1')
$expectedFunctions = Get-ObsidianFeatureEvidence -ProjectRoot $ProjectRoot -SourceRoot $SourceRoot -SourcePaths @($metrics.files.Path) -Groups $functionRules
$membershipCount = 0
foreach ($rule in $functionRules) {
    $records = @($functionReport.functions | Where-Object name -eq $rule.name)
    if ($records.Count -ne 1) { $problems.Add("기능 보고서 누락/중복: $($rule.name)"); continue }
    $record = $records[0]
    $expectedRecord = @($expectedFunctions.functions | Where-Object name -eq $rule.name)[0]
    $expectedSources = @($expectedRecord.sources)
    if (($record.evidence | ConvertTo-Json -Depth 6 -Compress) -cne ($expectedRecord.evidence | ConvertTo-Json -Depth 6 -Compress)) {
        $problems.Add("Feature 문서 근거가 최신 본문과 다릅니다: $($rule.name)")
    }
    foreach ($source in $record.sources) {
        if (@($record.evidence | Where-Object source -eq $source).Count -eq 0) {
            $problems.Add("Feature 근거 없는 기능 연결: $($rule.name) -> $source")
        }
    }
    foreach ($proof in $record.evidence) {
        $document = Join-Path $ProjectRoot $proof.document
        if (-not (Test-Path -LiteralPath $document -PathType Leaf)) { $problems.Add("근거 문서 없음: $($proof.document)"); continue }
        $documentLines = [IO.File]::ReadAllLines($document)
        if ($proof.line -lt 1 -or $proof.line -gt $documentLines.Count -or -not $documentLines[$proof.line - 1].Contains($proof.mention)) {
            $problems.Add("근거 행/언급 불일치: $($proof.document):$($proof.line)")
        }
        if ($proof.source -notin $metrics.files.Path) { $problems.Add("존재하지 않는 소스 근거: $($proof.source)") }
    }
    if ($expectedSources.Count -eq 0 -or (($record.sources | Sort-Object) -join '|') -cne ($expectedSources -join '|')) {
        $problems.Add("기능별 소스 목록 불일치: $($rule.name)")
    }
    $membershipCount += $expectedSources.Count
    $notePath = Join-Path $OutputRoot ('Functions/' + $rule.name + '.md')
    if (-not (Test-Path -LiteralPath $notePath -PathType Leaf)) { $problems.Add("기능 노트 누락: $($rule.name)"); continue }
    $noteText = Get-Content -LiteralPath $notePath -Raw -Encoding UTF8
    $actualTargets = @([regex]::Matches($noteText, '\[\[([^\]|#]+)(?:\|[^\]]+)?\]\]') | ForEach-Object { $_.Groups[1].Value } | Sort-Object)
    $expectedTargets = @($expectedSources | ForEach-Object { $OutputRelativePath.Replace('\', '/') + '/Files/' + $_ } | Sort-Object)
    if (($actualTargets -join '|') -cne ($expectedTargets -join '|')) {
        $problems.Add("기능 노트 링크 불일치: $($rule.name)")
    }
}
if ($functionReport.membershipCount -ne $membershipCount) { $problems.Add('기능-소스 관계 수가 다릅니다.') }

$generatedMarkdown = @(Get-ChildItem -LiteralPath $OutputRoot -Recurse -File -Filter '*.md')
foreach ($file in $generatedMarkdown) {
    $text = Get-Content -LiteralPath $file.FullName -Raw -Encoding UTF8
    foreach ($match in [regex]::Matches($text, '\[\[([^\]|#]+)(?:#[^\]|]+)?(?:\|[^\]]+)?\]\]')) {
        $target = $match.Groups[1].Value.Replace('/', '\')
        $candidateMdPath = if ($target.EndsWith('.md')) { $target } else { $target + '.md' }
        $candidateCanvasPath = if ($target.EndsWith('.canvas')) { $target } else { $target + '.canvas' }
        $candidateMd = Join-Path $ProjectRoot $candidateMdPath
        $candidateCanvas = Join-Path $ProjectRoot $candidateCanvasPath
        if (-not (Test-Path -LiteralPath $candidateMd) -and -not (Test-Path -LiteralPath $candidateCanvas)) {
            $problems.Add("깨진 생성 Wiki Link: $($file.FullName) -> $target")
        }
    }
}

if ($problems.Count -gt 0) {
    $problems | Sort-Object -Unique | ForEach-Object { Write-Error $_ }
    exit 1
}

Write-Output 'Obsidian architecture metrics validation passed.'
Write-Output "Source files: $sourceCount, notes: $noteCount, edges: $($metrics.dependencyEdgeCount), folders: $($metrics.folderCount)"
