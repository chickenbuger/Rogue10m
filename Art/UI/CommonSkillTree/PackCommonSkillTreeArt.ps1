$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

$repo = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../..')).Path
$generated = Join-Path $PSScriptRoot 'Medallions.generated.png'
$approved = Join-Path $repo 'Feature/doc/evidence/common-skill-web-20261120/approved-concept.png'
$png = [Drawing.Imaging.ImageFormat]::Png
$pixel = [Drawing.GraphicsUnit]::Pixel
$rgba = [Drawing.Imaging.PixelFormat]::Format32bppArgb
$tileSize = 256
$gridSide = 4
$center = ($tileSize - 1) / 2.0

# Register artwork in ERogue10mCommonSkill order, excluding None.
$centers = @(
    [Drawing.PointF]::new(166,168), [Drawing.PointF]::new(467,169), [Drawing.PointF]::new(768,168), [Drawing.PointF]::new(1075,168),
    [Drawing.PointF]::new(166,475), [Drawing.PointF]::new(466,475), [Drawing.PointF]::new(768,475), [Drawing.PointF]::new(1074,475),
    [Drawing.PointF]::new(164,783), [Drawing.PointF]::new(464,784), [Drawing.PointF]::new(768,783), [Drawing.PointF]::new(1074,783),
    [Drawing.PointF]::new(164,1092), [Drawing.PointF]::new(466,1092), [Drawing.PointF]::new(767,1092), [Drawing.PointF]::new(1075,1092)
)

$master = [Drawing.Bitmap]::new($generated)
$atlas = [Drawing.Bitmap]::new($tileSize * $gridSide, $tileSize * $gridSide, $rgba)
$writer = [Drawing.Graphics]::FromImage($atlas)
try {
    if ($master.Width -ne 1244 -or $master.Height -ne 1264) { throw 'Unexpected medallion source dimensions.' }
    $writer.CompositingMode = [Drawing.Drawing2D.CompositingMode]::SourceCopy
    for ($index = 0; $index -lt $centers.Count; $index++) {
        $coin = $centers[$index]
        $cell = [Drawing.Bitmap]::new($tileSize, $tileSize, $rgba)
        try {
            $sampler = [Drawing.Graphics]::FromImage($cell)
            try {
                $sampler.CompositingMode = [Drawing.Drawing2D.CompositingMode]::SourceCopy
                $sampler.InterpolationMode = [Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
                $sampler.DrawImage($master, [Drawing.Rectangle]::new(9, 9, 238, 238),
                    [int]$coin.X - 141, [int]$coin.Y - 141, 282, 282, $pixel)
            } finally { $sampler.Dispose() }

            # Constrain only the exterior matte fringe. The rendered bevel and glyph stay intact.
            for ($y = 9; $y -lt 247; $y++) {
                for ($x = 9; $x -lt 247; $x++) {
                    $dx = $x - $center
                    $dy = $y - $center
                    $radiusSquared = $dx * $dx + $dy * $dy
                    if ($radiusSquared -ge 119.0 * 119.0) {
                        $cell.SetPixel($x, $y, [Drawing.Color]::FromArgb(0, 0, 0, 0))
                    } elseif ($radiusSquared -gt 116.0 * 116.0) {
                        $color = $cell.GetPixel($x, $y)
                        $alpha = [int][Math]::Round($color.A * (119.0 - [Math]::Sqrt($radiusSquared)) / 3.0)
                        $cell.SetPixel($x, $y, [Drawing.Color]::FromArgb($alpha, $color.R, $color.G, $color.B))
                    }
                }
            }
            $column = $index % $gridSide
            $row = [int][Math]::Floor($index / $gridSide)
            $writer.DrawImageUnscaled($cell, $column * $tileSize, $row * $tileSize)
        } finally { $cell.Dispose() }
    }
    $atlas.Save((Join-Path $PSScriptRoot 'T_CommonSkillMedallions.png'), $png)
} finally {
    $writer.Dispose()
    $atlas.Dispose()
    $master.Dispose()
}

# The upper-right of the approved panel has clean material, free of node or frame pixels.
$concept = [Drawing.Bitmap]::new($approved)
try {
    if ($concept.Width -ne 1672 -or $concept.Height -ne 941) { throw 'Unexpected approved concept dimensions.' }
    $patch = $concept.Clone([Drawing.Rectangle]::new(1335, 155, 256, 256), $rgba)
    try { $patch.Save((Join-Path $PSScriptRoot 'T_CommonSkillCharcoal.png'), $png) }
    finally { $patch.Dispose() }
} finally { $concept.Dispose() }
