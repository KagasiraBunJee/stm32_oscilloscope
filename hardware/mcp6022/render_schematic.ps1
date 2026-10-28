Add-Type -AssemblyName System.Drawing
$scene=Get-Content -Raw -Encoding UTF8 -LiteralPath hardware/mcp6022/schematic_scene.json | ConvertFrom-Json
$bmp=New-Object System.Drawing.Bitmap 2000,1900
$gfx=[System.Drawing.Graphics]::FromImage($bmp)
$gfx.SmoothingMode=[System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
$gfx.TextRenderingHint=[System.Drawing.Text.TextRenderingHint]::AntiAliasGridFit
foreach($s in $scene){
 $color=[System.Drawing.ColorTranslator]::FromHtml($s.color)
 switch($s.kind){
 'line' {$pen=New-Object System.Drawing.Pen $color,([single]$s.width);$gfx.DrawLine($pen,[single]$s.x1,[single]$s.y1,[single]$s.x2,[single]$s.y2);$pen.Dispose()}
 'text' {$font=New-Object System.Drawing.Font 'Segoe UI',([single]$s.size),([System.Drawing.FontStyle]::Regular),([System.Drawing.GraphicsUnit]::Pixel);$brush=New-Object System.Drawing.SolidBrush $color;$gfx.DrawString($s.text,$font,$brush,[single]$s.x,[single]$s.y);$font.Dispose();$brush.Dispose()}
 'rect' {$fill=New-Object System.Drawing.SolidBrush ([System.Drawing.ColorTranslator]::FromHtml($s.fill));$pen=New-Object System.Drawing.Pen $color,([single]$s.width);$gfx.FillRectangle($fill,[single]$s.x,[single]$s.y,[single]$s.w,[single]$s.h);$gfx.DrawRectangle($pen,[single]$s.x,[single]$s.y,[single]$s.w,[single]$s.h);$fill.Dispose();$pen.Dispose()}
 'dot' {$brush=New-Object System.Drawing.SolidBrush $color;$gfx.FillEllipse($brush,[single]($s.x-5),[single]($s.y-5),10,10);$brush.Dispose()}
 'poly' {$points=[System.Drawing.PointF[]]@($s.points | ForEach-Object {New-Object System.Drawing.PointF ([single]$_[0]),([single]$_[1])});$fill=New-Object System.Drawing.SolidBrush ([System.Drawing.ColorTranslator]::FromHtml($s.fill));$pen=New-Object System.Drawing.Pen $color,([single]$s.width);$gfx.FillPolygon($fill,$points);$gfx.DrawPolygon($pen,$points);$fill.Dispose();$pen.Dispose()}
 }
}
$bmp.Save((Join-Path (Get-Location) 'hardware/mcp6022/mcp6022_1n4148_schematic.png'),[System.Drawing.Imaging.ImageFormat]::Png)
$gfx.Dispose();$bmp.Dispose()
