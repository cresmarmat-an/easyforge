# Makes the expected pixels for the JPEG test files with Windows' own decoder
# (GDI+), and adds gdiplus.jpg, a JPEG written by Windows' encoder. Run it on
# Windows after generate.py:
#
#     powershell -ExecutionPolicy Bypass -File reference_jpeg.ps1
#
# With -Check it also decodes the PNG and BMP test files with GDI+ and reports any
# difference from the expected pixels generate.py wrote, as a check on generate.py.

param([switch]$Check)

Add-Type -AssemblyName System.Drawing
$images = Join-Path $PSScriptRoot "images"

function Save-Pixels([System.Drawing.Bitmap]$bitmap, [string]$path)
{
    $bytes = New-Object byte[] (8 + $bitmap.Width * $bitmap.Height * 4)
    [BitConverter]::GetBytes([uint32]$bitmap.Width).CopyTo($bytes, 0)
    [BitConverter]::GetBytes([uint32]$bitmap.Height).CopyTo($bytes, 4)
    $index = 8
    for ($y = 0; $y -lt $bitmap.Height; $y++)
    {
        for ($x = 0; $x -lt $bitmap.Width; $x++)
        {
            $color = $bitmap.GetPixel($x, $y)
            $bytes[$index] = $color.R
            $bytes[$index + 1] = $color.G
            $bytes[$index + 2] = $color.B
            $bytes[$index + 3] = $color.A
            $index += 4
        }
    }
    [IO.File]::WriteAllBytes($path, $bytes)
}

function Read-Pixels([string]$path)
{
    return [IO.File]::ReadAllBytes($path)
}

# Windows' encoder, from the pattern image.
$pattern = Read-Pixels (Join-Path $images "pattern.rgba")
$width = [int][BitConverter]::ToUInt32($pattern, 0)
$height = [int][BitConverter]::ToUInt32($pattern, 4)
$source = New-Object System.Drawing.Bitmap $width, $height
for ($y = 0; $y -lt $height; $y++)
{
    for ($x = 0; $x -lt $width; $x++)
    {
        $at = 8 + ($y * $width + $x) * 4
        $source.SetPixel($x, $y, [System.Drawing.Color]::FromArgb(255, $pattern[$at], $pattern[$at + 1], $pattern[$at + 2]))
    }
}
$codec = [System.Drawing.Imaging.ImageCodecInfo]::GetImageEncoders() | Where-Object { $_.MimeType -eq "image/jpeg" }
$parameters = New-Object System.Drawing.Imaging.EncoderParameters 1
$parameters.Param[0] = New-Object System.Drawing.Imaging.EncoderParameter ([System.Drawing.Imaging.Encoder]::Quality), 90L
$source.Save((Join-Path $images "gdiplus.jpg"), $codec, $parameters)
$source.Dispose()

foreach ($file in Get-ChildItem $images -Filter *.jpg)
{
    if ($file.BaseName -eq "progressive") { continue }
    $bitmap = New-Object System.Drawing.Bitmap $file.FullName
    Save-Pixels $bitmap (Join-Path $images ($file.BaseName + "_jpg.rgba"))
    $bitmap.Dispose()
    Write-Output "reference for $($file.Name)"
}

if ($Check)
{
    foreach ($file in Get-ChildItem $images | Where-Object { $_.Extension -eq ".png" -or $_.Extension -eq ".bmp" })
    {
        $expectedPath = Join-Path $images ($file.BaseName + "_" + $file.Extension.TrimStart(".") + ".rgba")
        if (-not (Test-Path $expectedPath)) { continue }
        $expected = Read-Pixels $expectedPath
        $bitmap = New-Object System.Drawing.Bitmap $file.FullName
        $largest = 0
        for ($y = 0; $y -lt $bitmap.Height; $y++)
        {
            for ($x = 0; $x -lt $bitmap.Width; $x++)
            {
                $color = $bitmap.GetPixel($x, $y)
                $at = 8 + ($y * $bitmap.Width + $x) * 4
                $differences = @([Math]::Abs($color.R - $expected[$at]), [Math]::Abs($color.G - $expected[$at + 1]),
                    [Math]::Abs($color.B - $expected[$at + 2]), [Math]::Abs($color.A - $expected[$at + 3]))
                $largest = [Math]::Max($largest, ($differences | Measure-Object -Maximum).Maximum)
            }
        }
        $bitmap.Dispose()
        Write-Output ("check {0,-24} largest difference {1}" -f $file.Name, $largest)
    }
}
