$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

$tileSize = 256
$side = 4
$sourcePath = Join-Path $PSScriptRoot 'WeaponMedallions.generated.png'
$source = [Drawing.Bitmap]::new($sourcePath)
$atlas = [Drawing.Bitmap]::new($tileSize * $side, $tileSize * $side, [Drawing.Imaging.PixelFormat]::Format32bppArgb)
$graphics = [Drawing.Graphics]::FromImage($atlas)
try {
    if ($source.Width -ne 1254 -or $source.Height -ne 1254) {
        throw 'Unexpected weapon medallion source dimensions.'
    }
    $graphics.CompositingMode = [Drawing.Drawing2D.CompositingMode]::SourceCopy
    $graphics.InterpolationMode = [Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $sourceCellSize = $source.Width / [float]$side
    for ($row = 0; $row -lt $side; $row++) {
        for ($column = 0; $column -lt $side; $column++) {
            $cell = [Drawing.Rectangle]::new($column * $tileSize, $row * $tileSize, $tileSize, $tileSize)
            $graphics.DrawImage($source, $cell, $column * $sourceCellSize, $row * $sourceCellSize,
                $sourceCellSize, $sourceCellSize, [Drawing.GraphicsUnit]::Pixel)
        }
    }

    # Mask the generated halo outside the circular rim and retain a two-pixel antialiased edge.
    $center = ($tileSize - 1) / 2.0
    $clipRadius = 121.0
    for ($y = 0; $y -lt $atlas.Height; $y++) {
        $dy = $y % $tileSize - $center
        for ($x = 0; $x -lt $atlas.Width; $x++) {
            $dx = $x % $tileSize - $center
            $radiusSquared = $dx * $dx + $dy * $dy
            if ($radiusSquared -ge $clipRadius * $clipRadius) {
                $atlas.SetPixel($x, $y, [Drawing.Color]::Transparent)
            } elseif ($radiusSquared -gt 119.0 * 119.0) {
                $color = $atlas.GetPixel($x, $y)
                $alpha = [int][Math]::Round($color.A * ($clipRadius - [Math]::Sqrt($radiusSquared)) / 2.0)
                $atlas.SetPixel($x, $y, [Drawing.Color]::FromArgb($alpha, $color.R, $color.G, $color.B))
            }
        }
    }
    $atlas.Save((Join-Path $PSScriptRoot 'T_WeaponSkillMedallions.png'), [Drawing.Imaging.ImageFormat]::Png)
} finally {
    $graphics.Dispose()
    $atlas.Dispose()
    $source.Dispose()
}
