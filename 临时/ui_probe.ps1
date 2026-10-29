param(
    [int]$WarmSeconds = 12,
    [int]$EnterWait = 10,
    [int]$BackWait = 4
)
Add-Type -AssemblyName System.Drawing
Add-Type @"
using System;
using System.Runtime.InteropServices;
public struct RECT { public int Left; public int Top; public int Right; public int Bottom; }
public class UiProbe {
    [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr hWnd, out RECT lpRect);
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr hWnd);
    [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr hWnd, int nCmdShow);
    [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr hWnd, IntPtr hWndInsertAfter, int X, int Y, int cx, int cy, uint uFlags);
    [DllImport("user32.dll")] public static extern bool SetCursorPos(int X, int Y);
    [DllImport("user32.dll")] public static extern void mouse_event(uint dwFlags, uint dx, uint dy, uint dwData, UIntPtr dwExtraInfo);
    [DllImport("user32.dll")] public static extern void keybd_event(byte bVk, byte bScan, uint dwFlags, UIntPtr dwExtraInfo);
    [DllImport("user32.dll")] public static extern bool MoveWindow(IntPtr hWnd, int X, int Y, int nWidth, int nHeight, bool bRepaint);
}
"@
[UiProbe]::SetProcessDPIAware() | Out-Null

$outDir = $env:BYJY_SHOT_DIR
function Shot([IntPtr]$h, [string]$name) {
    $rect = New-Object RECT
    [UiProbe]::GetWindowRect($h, [ref]$rect) | Out-Null
    $w = $rect.Right - $rect.Left
    $ht = $rect.Bottom - $rect.Top
    if ($w -le 0 -or $ht -le 0) { Write-Output "skip $name"; return $null }
    $bmp = New-Object System.Drawing.Bitmap $w, $ht
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.CopyFromScreen($rect.Left, $rect.Top, 0, 0, $bmp.Size)
    $path = Join-Path $outDir $name
    $bmp.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
    $g.Dispose(); $bmp.Dispose()
    Write-Output "shot $name ($w x $ht) at ($($rect.Left),$($rect.Top))"
    return $rect
}
function ClickAt([int]$x, [int]$y) {
    [UiProbe]::SetCursorPos($x, $y) | Out-Null
    Start-Sleep -Milliseconds 250
    [UiProbe]::mouse_event(0x0002, 0, 0, 0, [UIntPtr]::Zero)
    Start-Sleep -Milliseconds 120
    [UiProbe]::mouse_event(0x0004, 0, 0, 0, [UIntPtr]::Zero)
}

$proc = Start-Process -FilePath $env:BYJY_TEST_EXE -WorkingDirectory $env:BYJY_TEST_WORK -PassThru
Start-Sleep -Seconds $WarmSeconds
if ($proc.HasExited) { Write-Output "EXITED early code=$($proc.ExitCode)"; exit }
$h = $proc.MainWindowHandle
[UiProbe]::ShowWindow($h, 9) | Out-Null
[UiProbe]::SetWindowPos($h, [IntPtr](-1), 0, 0, 0, 0, 0x0003) | Out-Null
[UiProbe]::SetForegroundWindow($h) | Out-Null
Start-Sleep -Seconds 2
$rect = Shot $h 'ui_main.png'
# entry button sits at the right end of the control row (window content coordinates)
$btnX = $rect.Left + ($rect.Right - $rect.Left) - 130
$btnY = $rect.Top + 158
Write-Output "click entry at ($btnX,$btnY)"
ClickAt $btnX $btnY
Start-Sleep -Seconds $EnterWait
Shot $h 'ui_entered.png' | Out-Null
# press ESC to return to test UI (single short press)
[UiProbe]::keybd_event(0x1B, 0, 0, [UIntPtr]::Zero)
Start-Sleep -Milliseconds 60
[UiProbe]::keybd_event(0x1B, 0, 2, [UIntPtr]::Zero)
Start-Sleep -Seconds $BackWait
Shot $h 'ui_back.png' | Out-Null
# resize while on test UI
[UiProbe]::MoveWindow($h, 100, 100, 1100, 760, $true) | Out-Null
Start-Sleep -Seconds 3
Shot $h 'ui_resized.png' | Out-Null
Start-Sleep -Seconds 5
if ($proc.HasExited) { Write-Output "EXITED after flow code=$($proc.ExitCode)" } else { Write-Output 'ALIVE after flow' }
Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue
Write-Output 'done'
