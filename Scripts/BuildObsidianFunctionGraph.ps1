param(
    [string]$ProjectRoot = (Split-Path -Parent $PSScriptRoot),
    [string]$OutputRelativePath = 'Docs/Obsidian/ArchitectureMetrics'
)
$ErrorActionPreference = 'Stop'
$utf8 = New-Object System.Text.UTF8Encoding($false)
$project = [IO.Path]::GetFullPath($ProjectRoot).TrimEnd('\', '/')
$output = [IO.Path]::GetFullPath((Join-Path $project $OutputRelativePath))
if (-not $output.StartsWith($project + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Function graph output must be inside the project.'
}
$functionsRoot = Join-Path $output 'Functions'
$metrics = Get-Content -LiteralPath (Join-Path $output 'metrics.json') -Raw -Encoding UTF8 | ConvertFrom-Json
$groups = @(Get-Content -LiteralPath (Join-Path $PSScriptRoot 'ObsidianFunctionGroups.json') -Raw -Encoding UTF8 | ConvertFrom-Json)
. (Join-Path $PSScriptRoot 'GetObsidianFeatureEvidence.ps1')
$evidenceReport = Get-ObsidianFeatureEvidence -ProjectRoot $project -SourceRoot $metrics.sourceRoot -SourcePaths @($metrics.files.Path) -Groups $groups
$records = @($evidenceReport.functions)
$names = @{}
foreach ($record in $records) {
    if ([string]::IsNullOrWhiteSpace($record.name) -or $record.name.IndexOfAny([IO.Path]::GetInvalidFileNameChars()) -ge 0 -or $names.ContainsKey($record.name)) {
        throw "Invalid or duplicate function name: $($record.name)"
    }
    $names[$record.name] = $true
    foreach ($member in $record.sources) {
        if (-not (Test-Path -LiteralPath (Join-Path $output ('Files/' + $member + '.md')) -PathType Leaf)) { throw "Source note missing: $member" }
    }
}
[IO.Directory]::CreateDirectory($functionsRoot) | Out-Null
foreach ($record in $records) {
    $lines = @(
        '---', 'generated: true', 'generated_by: rogue10m-function-graph', 'node_kind: function', '---',
        ('# ' + $record.name), '', $record.description, '',
        '> 기능 → 소스 연결은 Feature 본문에서 확인한 소스 언급입니다. 소스 → 소스 연결은 정적 include 의존성입니다.',
        '> 설계/과거 결과/제거 계획의 언급도 포함될 수 있으며, 현재 호출 관계나 구현 완료를 보증하지 않습니다.',
        '', '문서 주제 분류: Scripts/ObsidianFunctionGroups.json · 전체 행별 근거: functions.json', '', '## 관련 소스', ''
    )
    foreach ($member in $record.sources) {
        $lines += ('- [[' + $OutputRelativePath.Replace('\', '/') + '/Files/' + $member + '|' + $member + ']]')
        $proofs = @($record.evidence | Where-Object source -eq $member | Sort-Object @{Expression={ if ($_.kind -eq 'result-document') { 0 } else { 1 } }}, @{Expression='document';Descending=$true}, line)
        foreach ($proof in ($proofs | Select-Object -First 3)) {
            $label = if ($proof.kind -eq 'plan') { '설계' } else { '결과 문서' }
            $lines += ('  - 근거 (' + $label + '): ' + [char]96 + $proof.document + ':' + $proof.line + [char]96 + ' — ' + [char]96 + $proof.mention + [char]96)
        }
        if ($proofs.Count -gt 3) { $lines += ('  - 추가 근거 ' + ($proofs.Count - 3) + '건은 functions.json에 보관.') }
    }
    [IO.File]::WriteAllText((Join-Path $functionsRoot ($record.name + '.md')), (($lines -join [Environment]::NewLine) + [Environment]::NewLine), $utf8)
}
# Only remove obsolete notes owned by this generator, within this exact output folder.
foreach ($old in Get-ChildItem -LiteralPath $functionsRoot -File -Filter '*.md') {
    if (-not $names.ContainsKey($old.BaseName)) {
        $text = [IO.File]::ReadAllText($old.FullName)
        if ($text -match '(?m)^generated_by: rogue10m-function-graph\r?$') {
            Remove-Item -LiteralPath $old.FullName
        }
    }
}
$report = [ordered]@{
    basis = 'Feature document mentions of existing source files and owned symbols'
    functions = $records
    functionCount = $records.Count
    membershipCount = @($records | ForEach-Object { $_.sources }).Count
    scannedDocumentCount = $evidenceReport.scannedDocumentCount
    selectedDocumentCount = $evidenceReport.selectedDocumentCount
    unassignedDocuments = $evidenceReport.unassignedDocuments
    unreferencedSources = $evidenceReport.unreferencedSources
}
[IO.File]::WriteAllText((Join-Path $output 'functions.json'), (($report | ConvertTo-Json -Depth 8) + [Environment]::NewLine), $utf8)
Write-Output "Function nodes: $($report.functionCount), source memberships: $($report.membershipCount)"
