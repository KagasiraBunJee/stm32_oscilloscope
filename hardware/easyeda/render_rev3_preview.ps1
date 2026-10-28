Add-Type -AssemblyName System.Drawing
$data = Get-Content -Raw -LiteralPath "$PSScriptRoot\output\rev3_candidate.json" | ConvertFrom-Json
$bmp = New-Object System.Drawing.Bitmap 1000,1300
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.SmoothingMode = 'AntiAlias'
$g.Clear([System.Drawing.Color]::FromArgb(20,26,34))
$scale = 0.30
function X([double]$v) { return [single](85+$v*$scale) }
function Y([double]$v) { return [single](110-$v*$scale) }
$white = [System.Drawing.Brushes]::White
$font = New-Object System.Drawing.Font 'Segoe UI',10
$title = New-Object System.Drawing.Font 'Segoe UI',17
$g.DrawString('OSCILLOSCOPE - TOP-ONLY PLACEMENT PREVIEW',$title,$white,35,20)
$g.DrawString('70 x 90 mm | 3.2 mm M3 holes | lines = connection guides, NOT copper',$font,$white,35,60)
$pen = New-Object System.Drawing.Pen ([System.Drawing.Color]::Silver),2
$g.DrawRectangle($pen,(X 0),(Y 0),[single](70/0.0254*$scale),[single](90/0.0254*$scale))
foreach ($hole in $data.holes) {
 $r=3/0.0254*$scale
 $g.DrawEllipse([System.Drawing.Pens]::DimGray,[single]((X $hole[0])-$r),[single]((Y $hole[1])-$r),[single](2*$r),[single](2*$r))
 $r=1.6/0.0254*$scale
 $g.DrawEllipse($pen,[single]((X $hole[0])-$r),[single]((Y $hole[1])-$r),[single](2*$r),[single](2*$r))
}
$netpen=New-Object System.Drawing.Pen ([System.Drawing.Color]::FromArgb(70,110,130)),1
foreach($e in $data.edges){$g.DrawLine($netpen,(X $e.a.x),(Y $e.a.y),(X $e.b.x),(Y $e.b.y))}
foreach($c in $data.components){
 $b=$c.bbox
 $g.FillRectangle([System.Drawing.Brushes]::Black,(X $b.minX),(Y $b.maxY),[single](($b.maxX-$b.minX)*$scale),[single](($b.maxY-$b.minY)*$scale))
 $g.DrawRectangle([System.Drawing.Pens]::DarkGray,(X $b.minX),(Y $b.maxY),[single](($b.maxX-$b.minX)*$scale),[single](($b.maxY-$b.minY)*$scale))
 foreach($p in $c.pads){
  $brush=if($p.net -eq 'GND'){[System.Drawing.Brushes]::SlateGray}elseif($p.net -eq '3V3_A'){[System.Drawing.Brushes]::Orange}else{[System.Drawing.Brushes]::Gold}
  $g.FillRectangle($brush,(X ($p.x-$p.width/2)),(Y ($p.y+$p.height/2)),[single]($p.width*$scale),[single]($p.height*$scale))
 }
 $g.DrawString($c.designator,$font,$white,(X $b.minX),[single]((Y $b.maxY)-19))
}
$g.DrawString('J9 PA1 | J10 PC0 | J11 PA2 | J12 PC1 | J13 PA3 | J14 PC2 | all pin 2 = GND',$font,$white,35,1200)
$g.DrawString('J1/J3/J5 direct inputs | J2/J4/J6 conditioned inputs | J7 3.3 V power',$font,$white,35,1225)
$g.DrawString('Draft only: no tracks applied; native routing and DRC still required.',$font,$white,35,1250)
$bmp.Save("$PSScriptRoot\output\rev3_placement_preview.png",[System.Drawing.Imaging.ImageFormat]::Png)
$g.Dispose(); $bmp.Dispose()
