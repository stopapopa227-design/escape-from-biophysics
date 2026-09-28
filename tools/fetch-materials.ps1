$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
$destination = Join-Path $PSScriptRoot '..\assets\materials'
New-Item -ItemType Directory -Force -Path $destination | Out-Null
$records = @()
foreach ($asset in @('beige_wall_001','terrazzo_tiles','wood_table_001')) {
    $meta = Invoke-RestMethod -Uri "https://api.polyhaven.com/files/$asset"
    foreach ($map in @('Diffuse','nor_gl','Rough')) {
        $entry = $meta.$map.'1k'.jpg
        if (-not $entry) { throw "Missing texture: $asset $map" }
        $name = "$asset-$map.jpg"
        $path = Join-Path $destination $name
        Invoke-WebRequest -Uri $entry.url -OutFile $path
        $actual = (Get-FileHash -LiteralPath $path -Algorithm MD5).Hash.ToLowerInvariant()
        if ($actual -ne $entry.md5) { throw "Checksum mismatch: $name" }
        # Normalize metadata: some source JPEG EXIF blocks are rejected by WIC.
        $image = [System.Drawing.Image]::FromFile($path)
        $bitmap = [System.Drawing.Bitmap]::new($image)
        $bitmap.Save((Join-Path $destination "$asset-$map.png"), [System.Drawing.Imaging.ImageFormat]::Png)
        $bitmap.Dispose(); $image.Dispose()
        $records += [PSCustomObject]@{file=$name;source=$entry.url;md5=$actual;license='CC0';asset="https://polyhaven.com/a/$asset"}
        Write-Output "Verified $name"
    }
}
$records | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $destination 'sources.json') -Encoding utf8
