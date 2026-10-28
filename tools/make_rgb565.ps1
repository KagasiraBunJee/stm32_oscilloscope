param(
    [Parameter(Mandatory = $true)]
    [string]$InputImage,

    [Parameter(Mandatory = $true)]
    [string]$OutputPreview,

    [Parameter(Mandatory = $true)]
    [string]$OutputSource,

    [int]$Width = 320,
    [int]$Height = 240
)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

$source = [System.Drawing.Image]::FromFile($InputImage)
$bitmap = New-Object System.Drawing.Bitmap($Width, $Height,
    [System.Drawing.Imaging.PixelFormat]::Format24bppRgb)
$graphics = [System.Drawing.Graphics]::FromImage($bitmap)
$graphics.InterpolationMode =
    [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
$graphics.PixelOffsetMode =
    [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
$graphics.DrawImage($source, 0, 0, $Width, $Height)
$graphics.Dispose()
$source.Dispose()
$bitmap.Save($OutputPreview, [System.Drawing.Imaging.ImageFormat]::Png)

$builder = New-Object System.Text.StringBuilder
[void]$builder.AppendLine('#include "test_image.h"')
[void]$builder.AppendLine('')
[void]$builder.AppendLine('const uint16_t g_test_image_rgb565[TEST_IMAGE_PIXEL_COUNT]')
[void]$builder.AppendLine('    __attribute__((aligned(4))) = {')

for ($y = 0; $y -lt $Height; ++$y) {
    [void]$builder.Append('    ')
    for ($x = 0; $x -lt $Width; ++$x) {
        $pixel = $bitmap.GetPixel($x, $y)
        $rgb565 = (($pixel.R -band 0xF8) -shl 8) -bor
                  (($pixel.G -band 0xFC) -shl 3) -bor
                  ($pixel.B -shr 3)
        [void]$builder.Append(('0x{0:X4}U' -f $rgb565))
        if (($x -ne ($Width - 1)) -or ($y -ne ($Height - 1))) {
            [void]$builder.Append(',')
        }
        if (($x % 12) -eq 11 -and $x -ne ($Width - 1)) {
            [void]$builder.AppendLine()
            [void]$builder.Append('    ')
        } else {
            [void]$builder.Append(' ')
        }
    }
    [void]$builder.AppendLine()
}

[void]$builder.AppendLine('};')
[System.IO.File]::WriteAllText($OutputSource, $builder.ToString(),
    (New-Object System.Text.UTF8Encoding($false)))
$bitmap.Dispose()

Write-Output "Generated $OutputPreview and $OutputSource ($Width x $Height RGB565)"
