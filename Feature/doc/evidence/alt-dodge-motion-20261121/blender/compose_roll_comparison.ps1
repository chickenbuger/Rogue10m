param([ValidatePattern('^v\d+$')][string]$Label = 'v4')

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
$inputFolder = Join-Path $PSScriptRoot $Label
$bitmap = [System.Drawing.Bitmap]::new(1500, 691)
$graphics = [System.Drawing.Graphics]::FromImage($bitmap)
$graphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
$graphics.TextRenderingHint = [System.Drawing.Text.TextRenderingHint]::AntiAliasGridFit
$graphics.Clear([System.Drawing.Color]::FromArgb(24, 30, 38))
$white = [System.Drawing.SolidBrush]::new([System.Drawing.Color]::FromArgb(243, 246, 249))
$muted = [System.Drawing.SolidBrush]::new([System.Drawing.Color]::FromArgb(168, 184, 201))
$title = [System.Drawing.Font]::new('Malgun Gothic', 24, [System.Drawing.FontStyle]::Bold)
$heading = [System.Drawing.Font]::new('Malgun Gothic', 13, [System.Drawing.FontStyle]::Bold)
$caption = [System.Drawing.Font]::new('Malgun Gothic', 11)
try {
    $graphics.DrawString('Alt 구르기 · 어깨를 거치는 자세', $title, $white, 23, 15)
    $graphics.DrawString('Blender Manny 골격 비교 · 동일 카메라와 기준 바닥 · 주황색은 오른쪽 팔다리', $caption, $muted, 26, 66)
    $views = @('side-07', 'side-10', 'side-13', 'three-quarter-13', 'side-16', 'side-19')
    $labels = @('24% · 준비', '36% · 손 짚기', '48% · 턱 접기', '48% · 사선 시점', '60% · 어깨 지지', '72% · 일어나기')
    for ($column = 0; $column -lt $views.Length; ++$column) {
        $x = 25 + 245 * $column
        $graphics.DrawString($labels[$column], $heading, $white, $x, 97)
        $variants = @('existing', 'refined')
        for ($row = 0; $row -lt $variants.Length; ++$row) {
            $y = 148 + 270 * $row
            $path = Join-Path $inputFolder "$($variants[$row])-$($views[$column]).png"
            $profile = [System.Drawing.Image]::FromFile($path)
            try { $graphics.DrawImage($profile, $x, $y, 224, 224) }
            finally { $profile.Dispose() }
        }
    }
    $graphics.DrawString('기존 절차적 구르기', $caption, $muted, 26, 126)
    $graphics.DrawString('Blender 보정 자세', $caption, $muted, 26, 396)
    $graphics.DrawString('검증용 표시와 바닥으로 본 포즈 비교', $caption, $muted, 26, 654)
    $bitmap.Save((Join-Path $inputFolder 'roll-profile-comparison.png'), [System.Drawing.Imaging.ImageFormat]::Png)
} finally {
    $caption.Dispose()
    $heading.Dispose()
    $title.Dispose()
    $muted.Dispose()
    $white.Dispose()
    $graphics.Dispose()
    $bitmap.Dispose()
}
