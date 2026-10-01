$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$source = [IO.File]::ReadAllText((Join-Path $root 'src\world.h'))
$block = [regex]::Match($source, 'posterDepartments=\{\{([\s\S]*?)\}\};').Groups[1].Value
$rows = [regex]::Matches($block, '\{\{([^}]+)\}\}')
$emblem = [Drawing.Image]::FromFile((Join-Path $root 'assets\signage\faculty-emblem.png'))
$ink = [Drawing.SolidBrush]::new([Drawing.Color]::FromArgb(25,61,80))
$format = [Drawing.StringFormat]::new()
$format.Alignment = [Drawing.StringAlignment]::Center
$format.LineAlignment = [Drawing.StringAlignment]::Center
function Label($s, $y, $h, $size, $bold = $false) {
    $style = if ($bold) { [Drawing.FontStyle]::Bold } else { [Drawing.FontStyle]::Regular }
    $f = [Drawing.Font]::new('Segoe UI', [single]$size, $style, [Drawing.GraphicsUnit]::Pixel)
    $g.DrawString($s, $f, $ink, [Drawing.RectangleF]::new(45,$y,934,$h), $format)
    $f.Dispose()
}
for ($i=0; $i -lt $rows.Count; $i++) {
    $lines = @([regex]::Matches($rows[$i].Value, 'L"([^"]+)"') | ForEach-Object { $_.Groups[1].Value })
    $bmp = [Drawing.Bitmap]::new(1024,1448)
    $g = [Drawing.Graphics]::FromImage($bmp)
    $g.Clear([Drawing.Color]::White)
    $g.SmoothingMode = [Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $g.TextRenderingHint = [Drawing.Text.TextRenderingHint]::AntiAliasGridFit
    $g.InterpolationMode = [Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $g.DrawImage($emblem, [Drawing.Rectangle]::new(350,135,324,263))
    Label 'КАФЕДРА' 473 90 52 $true
    $max = ($lines | ForEach-Object { $_.Length } | Measure-Object -Maximum).Maximum
    $size = [Math]::Min(53, 1500 / $max)
    for ($j=0; $j -lt $lines.Count; $j++) { Label $lines[$j] (720 - ($lines.Count-1)*47 + $j*94) 94 $size $true }
    Label 'ФИЗИЧЕСКИЙ ФАКУЛЬТЕТ МГУ' 1202 80 29
    Label 'МОСКОВСКИЙ ГОСУДАРСТВЕННЫЙ УНИВЕРСИТЕТ' 1304 60 20
    $bmp.Save((Join-Path $root ('assets\signage\department-' + ($i+1) + '.png')), [Drawing.Imaging.ImageFormat]::Png)
    $g.Dispose(); $bmp.Dispose()
}
$emblem.Dispose(); $ink.Dispose(); $format.Dispose()
if ($rows.Count -ne 12) { throw 'Expected 12 departments' }
Write-Output 'Generated 12 department posters.'
