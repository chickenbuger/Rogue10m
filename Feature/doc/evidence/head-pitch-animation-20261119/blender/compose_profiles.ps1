$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
$inputFolder = Join-Path $PSScriptRoot 'skeleton-only'
$report = Get-Content -LiteralPath (Join-Path $inputFolder 'blender-metrics.json') -Raw | ConvertFrom-Json
$bitmap = [System.Drawing.Bitmap]::new(1152, 892)
$graphics = [System.Drawing.Graphics]::FromImage($bitmap)
$graphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
$graphics.TextRenderingHint = [System.Drawing.Text.TextRenderingHint]::AntiAliasGridFit
$graphics.Clear([System.Drawing.Color]::FromArgb(24, 30, 38))
$white = [System.Drawing.SolidBrush]::new([System.Drawing.Color]::FromArgb(241, 245, 249))
$muted = [System.Drawing.SolidBrush]::new([System.Drawing.Color]::FromArgb(168, 184, 201))
$title = [System.Drawing.Font]::new('Malgun Gothic', 24, [System.Drawing.FontStyle]::Bold)
$heading = [System.Drawing.Font]::new('Malgun Gothic', 14, [System.Drawing.FontStyle]::Bold)
$caption = [System.Drawing.Font]::new('Malgun Gothic', 11)
try {
    $graphics.DrawString('Blender에서 비교한 머리·목 동작', $title, $white, 24, 19)
    $graphics.DrawString('현재 플레이어 클립의 본으로 만든 옆모습 · 각 행의 자세와 카메라 위치 고정', $caption, $muted, 26, 65)
    $moves = @('walk', 'jog')
    $settings = @('raw', 'existing', 'moderate')
    $columns = @('원본 클립', '기존 15% · 2° / 4°', '보정안 35% · 5° / 7°')
    for ($row = 0; $row -lt $moves.Length; ++$row) {
        $move = $moves[$row]
        $label = if ($move -eq 'walk') { '걷기' } else { '달리기' }
        for ($column = 0; $column -lt $settings.Length; ++$column) {
            $setting = $settings[$column]
            $x = 26 + 370 * $column
            $y = 109 + 383 * $row
            $graphics.DrawString("$label · $($columns[$column])", $heading, $white, $x, $y)
            $path = Join-Path $inputFolder "$move-$setting-profile.png"
            $profile = [System.Drawing.Image]::FromFile($path)
            try {
                $graphics.DrawImage($profile, $x, $y + 30, 322, 322)
            } finally { $profile.Dispose() }
            $headSpan = $report.results.$move.$setting.head.animated_rotation_span_deg
            $graphics.DrawString(('한 주기 머리 회전 폭 {0:0.00}°' -f $headSpan), $caption, $muted, $x, $y + 355)
        }
    }
    $bitmap.Save((Join-Path $inputFolder 'profile-comparison.png'), [System.Drawing.Imaging.ImageFormat]::Png)
} finally {
    $caption.Dispose()
    $heading.Dispose()
    $title.Dispose()
    $muted.Dispose()
    $white.Dispose()
    $graphics.Dispose()
    $bitmap.Dispose()
}
