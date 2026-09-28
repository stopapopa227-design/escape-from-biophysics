param([string]$Directory = (Join-Path $PSScriptRoot '..\build'))
Add-Type -AssemblyName System.Drawing
foreach ($name in @('title','gameplay','pursuit','corridor','portrait','poster','hall','door') + (0..11 | ForEach-Object { "walk-$_" })) {
    $source = Join-Path $Directory "$name.tga"
    if (-not (Test-Path -LiteralPath $source)) { continue }
    $bytes = [IO.File]::ReadAllBytes($source)
    $w = [int]$bytes[12] + 256 * [int]$bytes[13]
    $h = [int]$bytes[14] + 256 * [int]$bytes[15]
    $bmp = [System.Drawing.Bitmap]::new($w, $h, [System.Drawing.Imaging.PixelFormat]::Format24bppRgb)
    $data = $bmp.LockBits([System.Drawing.Rectangle]::new(0,0,$w,$h), [System.Drawing.Imaging.ImageLockMode]::WriteOnly, [System.Drawing.Imaging.PixelFormat]::Format24bppRgb)
    for ($y=0; $y -lt $h; $y++) {
        [Runtime.InteropServices.Marshal]::Copy($bytes, 18+($h-1-$y)*$w*3, [IntPtr]::Add($data.Scan0,$y*$data.Stride), $w*3)
    }
    $bmp.UnlockBits($data)
    $bmp.Save([IO.Path]::GetFullPath((Join-Path $Directory "$name.png")))
    $bmp.Dispose()
}
