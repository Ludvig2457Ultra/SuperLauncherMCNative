# Снимок окна SuperLauncherNative.exe в PNG
param([string]$Path = "$env:TEMP\sl_shot.png")
$p = Start-Process -FilePath ".\SuperLauncherNative.exe" -PassThru
Start-Sleep -Seconds 3
Add-Type -AssemblyName System.Drawing
Add-Type -AssemblyName System.Windows.Forms
$r = [System.Windows.Forms.Screen]::PrimaryScreen.Bounds
$bmp = New-Object System.Drawing.Bitmap $r.Width, $r.Height
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.CopyFromScreen($r.Location, [System.Drawing.Point]::Empty, $r.Size)
$g.Dispose()
$bmp.Save($Path, [System.Drawing.Imaging.ImageFormat]::Png)
$bmp.Dispose()
Stop-Process -Id $p.Id -Force
Write-Output $Path