$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
$reportDir = Join-Path $PSScriptRoot '..\assets\reports'
[IO.Directory]::CreateDirectory($reportDir) | Out-Null
$reportDir = [IO.Path]::GetFullPath($reportDir)
$topics = @(
    @('Математический маятник', 'Зависимость периода от длины подвеса', 'l, м', 'T², с²', 'g = (9,81 ± 0,12) м/с²'),
    @('Проверка закона Ома', 'Вольт-амперная характеристика резистора', 'U, В', 'I, мА', 'R = (102,4 ± 1,8) Ом'),
    @('Дифракция света', 'Определение периода дифракционной решётки', 'k', 'sin φ', 'd = (2,00 ± 0,04) мкм'),
    @('Теплоёмкость твёрдого тела', 'Обработка результатов калориметрии', 'ΔT, К', 'Q, Дж', 'c = (385 ± 12) Дж/(кг·К)'),
    @('Внешний фотоэффект', 'Зависимость задерживающего напряжения от частоты', 'ν, 10¹⁴ Гц', 'U, В', 'h = (6,62 ± 0,16) · 10⁻³⁴ Дж·с')
)
$brushes = @{}
function Ink($hex) {
    if (!$brushes.ContainsKey($hex)) { $brushes[$hex] = [Drawing.SolidBrush]::new([Drawing.ColorTranslator]::FromHtml($hex)) }
    return $brushes[$hex]
}
function Text($value, $x, $y, $size = 25, $color = '#303236', $style = 'Regular', $family = 'Times New Roman') {
    $font = [Drawing.Font]::new($family, [single]$size, [Drawing.FontStyle]::$style, [Drawing.GraphicsUnit]::Pixel)
    $g.DrawString([string]$value, $font, (Ink $color), [single]$x, [single]$y)
    $font.Dispose()
}
function Rule($x, $y, $xx, $yy, $color = '#9b9a94', $width = 1) {
    $pen = [Drawing.Pen]::new([Drawing.ColorTranslator]::FromHtml($color), [single]$width)
    $g.DrawLine($pen, [single]$x, [single]$y, [single]$xx, [single]$yy)
    $pen.Dispose()
}
for ($page = 0; $page -lt 5; $page++) {
    $bmp = [Drawing.Bitmap]::new(1024, 1448, [Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode = [Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $g.TextRenderingHint = [Drawing.Text.TextRenderingHint]::AntiAliasGridFit
    $g.Clear([Drawing.ColorTranslator]::FromHtml('#eeeae0'))
    # Subtle stock variation, with no baked lighting or luminous border.
    $rng = [Random]::new(412 + $page)
    for ($i = 0; $i -lt 5500; $i++) {
        $x = $rng.Next(1024); $y = $rng.Next(1448)
        $g.FillRectangle((Ink '#e8e4db'), $x, $y, 1, 1)
    }
    Text 'МОСКОВСКИЙ ГОСУДАРСТВЕННЫЙ УНИВЕРСИТЕТ' 103 79 25
    Text 'имени М. В. Ломоносова  /  Физический факультет' 165 113 25
    Rule 84 160 940 160 '#676963' 1.5
    Text ('ЛАБОРАТОРНАЯ РАБОТА № ' + (201 + $page * 13)) 119 189 37 '#303236' 'Bold'
    Text $topics[$page][0] 84 254 36 '#303236' 'Bold'
    Text $topics[$page][1] 84 304 25 '#555750'
    Text 'Студент: Иванов И. И.                         Группа: 203' 84 367 27
    Text 'Дата измерений: 24.09.2026        Допуск: ____________' 84 409 26
    Text '1. Результаты измерений' 84 485 30 '#303236' 'Bold'
    Text 'Приборы: лабораторная установка, цифровой измеритель.' 84 532 25
    $cols = @(84, 185, 426, 667, 940)
    for ($i = 0; $i -le 6; $i++) { Rule 84 (589 + $i * 48) 940 (589 + $i * 48) '#92948e' 1.3 }
    foreach ($x in $cols) { Rule $x 589 $x 877 '#92948e' 1.3 }
    Text '№' 114 597 26
    Text $topics[$page][2] 249 597 26
    Text $topics[$page][3] 489 597 26
    Text 'Погрешность' 716 597 26
    $xvals = @(@('0,20','0,30','0,40','0,50','0,60'), @('0,50','1,00','1,50','2,00','2,50'), @('1','2','3','4','5'), @('2,0','4,0','6,0','8,0','10,0'), @('5,50','6,00','6,50','7,00','7,50'))
    $yvals = @(@('0,81','1,20','1,62','2,01','2,42'), @('4,88','9,76','14,6','19,5','24,4'), @('0,16','0,32','0,47','0,63','0,79'), @('154','308','462','616','770'), @('0,08','0,29','0,50','0,70','0,91'))
    for ($row = 0; $row -lt 5; $row++) {
        $y = 645 + $row * 48
        Text ($row + 1) 116 $y 24 '#303236'
        Text $xvals[$page][$row] 265 $y 24 '#263d64' 'Regular' 'Segoe Print'
        Text $yvals[$page][$row] 502 $y 24 '#263d64' 'Regular' 'Segoe Print'
        Text '± 0,02' 747 $y 24 '#263d64' 'Regular' 'Segoe Print'
    }
    Text '2. Графическая обработка' 84 915 30 '#303236' 'Bold'
    for ($i = 0; $i -le 10; $i++) { Rule (148 + $i * 71) 989 (148 + $i * 71) 1195 '#c9cec9' }
    for ($i = 0; $i -le 6; $i++) { Rule 148 (989 + $i * 34) 858 (989 + $i * 34) '#c9cec9' }
    Rule 148 980 148 1195 '#595d5c' 2
    Rule 148 1195 877 1195 '#595d5c' 2
    Rule 179 1181 828 1008 '#364866' 2
    for ($i = 0; $i -lt 5; $i++) {
        $x = 221 + $i * 137; $y = 1170 - $i * 37 + $rng.Next(-3, 4)
        Rule $x ($y - 7) $x ($y + 7) '#263d64' 2
        Rule ($x - 5) ($y - 7) ($x + 5) ($y - 7) '#263d64' 2
        Rule ($x - 5) ($y + 7) ($x + 5) ($y + 7) '#263d64' 2
        $g.FillEllipse((Ink '#263d64'), [single]($x - 3), [single]($y - 3), 6, 6)
    }
    Text $topics[$page][3] 91 962 21
    Text $topics[$page][2] 819 1200 21
    Text ('Результат:  ' + $topics[$page][4]) 84 1250 28 '#263d64' 'Italic'
    Text 'Вывод: зависимость согласуется с расчётной в пределах ошибки.' 84 1300 24
    Rule 84 1359 940 1359 '#b4b3aa'
    Text 'Общий физический практикум' 84 1376 21 '#71746f'
    Text ('стр. 1 / 3    ·    ' + (201 + $page * 13)) 735 1376 21 '#71746f'
    $bmp.Save((Join-Path $reportDir ('report-' + ($page + 1) + '.png')), [Drawing.Imaging.ImageFormat]::Png)
    $g.Dispose(); $bmp.Dispose()
}
foreach ($brush in $brushes.Values) { $brush.Dispose() }
Write-Output 'Generated 5 laboratory report pages (1024 x 1448).'
