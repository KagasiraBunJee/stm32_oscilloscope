Add-Type -AssemblyName System.Drawing
$data=Get-Content -Raw "$PSScriptRoot\output\rev3_reopened.json" | ConvertFrom-Json
$tracks=Get-Content -Raw "$PSScriptRoot\output\rev3_reopened_tracks.json" | ConvertFrom-Json
$bmp=New-Object System.Drawing.Bitmap 1400,1800
$g=[System.Drawing.Graphics]::FromImage($bmp)
$g.Clear([System.Drawing.Color]::Black)
$scale=.48
function X([double]$v){return [single](30+$v*$scale)}
function Y([double]$v){return [single](30-$v*$scale)}
$font=New-Object System.Drawing.Font 'Segoe UI',10
foreach($t in $tracks){
 $color=if($t.net -eq 'GND'){[System.Drawing.Color]::LimeGreen}elseif($t.net -eq 'VBIAS'){[System.Drawing.Color]::Cyan}elseif($t.net -eq '3V3_A'){[System.Drawing.Color]::Orange}else{[System.Drawing.Color]::FromArgb(130,65,65)}
 $pen=New-Object System.Drawing.Pen $color,([single]($t.lineWidth*$scale))
 $g.DrawLine($pen,(X $t.startX),(Y $t.startY),(X $t.endX),(Y $t.endY));$pen.Dispose()
}
foreach($c in $data.components){
 foreach($p in $c.pads){
  $b=if($p.net -eq 'GND'){[System.Drawing.Brushes]::LimeGreen}elseif($p.net -eq 'VBIAS'){[System.Drawing.Brushes]::Cyan}else{[System.Drawing.Brushes]::Gold}
  $g.FillRectangle($b,(X ($p.x-$p.width/2)),(Y ($p.y+$p.height/2)),[single]($p.width*$scale),[single]($p.height*$scale))
 }
 $g.DrawString($c.designator,$font,[System.Drawing.Brushes]::White,(X $c.x),(Y $c.y))
}
$bmp.Save("$PSScriptRoot\output\rev3_copper.png",[System.Drawing.Imaging.ImageFormat]::Png)
$g.Dispose();$bmp.Dispose()
