# Renders breadboard_scene.json (from draw_breadboard.py) to receive_audio_breadboard.png.
Add-Type -AssemblyName System.Drawing
$dir=Split-Path -Parent $MyInvocation.MyCommand.Path
$scene=Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $dir 'breadboard_scene.json') | ConvertFrom-Json
$bmp=New-Object System.Drawing.Bitmap ([int]$scene.width),([int]$scene.height)
$gfx=[System.Drawing.Graphics]::FromImage($bmp)
$gfx.SmoothingMode=[System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
$gfx.TextRenderingHint=[System.Drawing.Text.TextRenderingHint]::AntiAliasGridFit
$gfx.Clear([System.Drawing.Color]::White)
function Col($h){[System.Drawing.ColorTranslator]::FromHtml($h)}
foreach($s in $scene.items){
 switch($s.kind){
 'line' {$pen=New-Object System.Drawing.Pen (Col $s.color),([single]$s.width);$pen.StartCap='Round';$pen.EndCap='Round';$gfx.DrawLine($pen,[single]$s.x1,[single]$s.y1,[single]$s.x2,[single]$s.y2);$pen.Dispose()}
 'rect' {$fill=New-Object System.Drawing.SolidBrush (Col $s.fill);$gfx.FillRectangle($fill,[single]$s.x,[single]$s.y,[single]$s.w,[single]$s.h);$fill.Dispose()
         if([single]$s.width -gt 0){$pen=New-Object System.Drawing.Pen (Col $s.color),([single]$s.width);$gfx.DrawRectangle($pen,[single]$s.x,[single]$s.y,[single]$s.w,[single]$s.h);$pen.Dispose()}}
 'circle' {$r=[single]$s.r;$fill=New-Object System.Drawing.SolidBrush (Col $s.fill);$gfx.FillEllipse($fill,[single]($s.x-$r),[single]($s.y-$r),2*$r,2*$r);$fill.Dispose()
         if([single]$s.width -gt 0){$pen=New-Object System.Drawing.Pen (Col $s.color),([single]$s.width);$gfx.DrawEllipse($pen,[single]($s.x-$r),[single]($s.y-$r),2*$r,2*$r);$pen.Dispose()}}
 'text' {$style=if($s.bold){[System.Drawing.FontStyle]::Bold}else{[System.Drawing.FontStyle]::Regular}
         $font=New-Object System.Drawing.Font 'Segoe UI',([single]$s.size),$style,([System.Drawing.GraphicsUnit]::Pixel)
         $brush=New-Object System.Drawing.SolidBrush (Col $s.color);$fmt=New-Object System.Drawing.StringFormat
         $fmt.Alignment=switch($s.anchor){'middle'{'Center'}'end'{'Far'}default{'Near'}};$fmt.LineAlignment='Far'
         $gfx.DrawString($s.text,$font,$brush,[single]$s.x,[single]($s.y+[single]$s.size*0.28),$fmt);$font.Dispose();$brush.Dispose();$fmt.Dispose()}
 }
}
$bmp.Save((Join-Path $dir 'receive_audio_breadboard.png'),[System.Drawing.Imaging.ImageFormat]::Png)
$gfx.Dispose();$bmp.Dispose()
