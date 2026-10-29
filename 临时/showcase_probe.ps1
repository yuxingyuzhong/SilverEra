param(
    [string]$OutDir = $env:BYJY_SHOT_DIR,
    [int]$WarmSeconds = 15,
    [int]$HoldSeconds = 3,
    [int]$TailSeconds = 30
)
Add-Type -AssemblyName System.Drawing
Add-Type @"
using System;
using System.Runtime.InteropServices;
public struct RECT { public int Left; public int Top; public int Right; public int Bottom; }
public class UiTool {
    [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr hWnd, out RECT lpRect);
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr hWnd);
    [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr hWnd, int nCmdShow);
    [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr hWnd, IntPtr hWndInsertAfter, int X, int Y, int cx, int cy, uint uFlags);
    [DllImport("user32.dll")] public static extern void keybd_event(byte bVk, byte bScan, uint dwFlags, UIntPtr dwExtraInfo);
    [DllImport("user32.dll")] public static extern bool MoveWindow(IntPtr hWnd, int X, int Y, int nWidth, int nHeight, bool bRepaint);
}
"@
[UiTool]::SetProcessDPIAware() | Out-Null

function Shot([IntPtr]$h, [string]$name) {
    $rect = New-Object RECT
    [UiTool]::GetWindowRect($h, [ref]$rect) | Out-Null
    $w = $rect.Right - $rect.Left
    $ht = $rect.Bottom - $rect.Top
    if ($w -le 0 -or $ht -le 0) { Write-Output "shot skipped ${name}: ${w}x${ht}"; return }
    $bmp = New-Object System.Drawing.Bitmap $w, $ht
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.CopyFromScreen($rect.Left, $rect.Top, 0, 0, $bmp.Size)
    $path = Join-Path $OutDir $name
    $bmp.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
    $g.Dispose()
    $bmp.Dispose()
    Write-Output "shot: $path (${w}x${ht})"
}

$exe = $env:BYJY_TEST_EXE
$work = $env:BYJY_TEST_WORK
$proc = Start-Process -FilePath $exe -WorkingDirectory $work -ArgumentList '--selector=showcase' -RedirectStandardOutput (Join-Path $OutDir 'showcase_out.log') -RedirectStandardError (Join-Path $OutDir 'showcase_err.log') -PassThru
Start-Sleep -Seconds $WarmSeconds
if ($proc.HasExited) { Write-Output "EXITED early code=$($proc.ExitCode)"; exit }
$h = $proc.MainWindowHandle
[UiTool]::ShowWindow($h, 9) | Out-Null
[UiTool]::SetWindowPos($h, [IntPtr](-1), 0, 0, 0, 0, 0x0003) | Out-Null
[UiTool]::SetForegroundWindow($h) | Out-Null
Start-Sleep -Seconds 2
Shot $h 'a_初始.png'
# hold A (0x41): move left to inspect zone 1
[UiTool]::keybd_event(0x41, 0, 0, [UIntPtr]::Zero)
Start-Sleep -Seconds $HoldSeconds
[UiTool]::keybd_event(0x41, 0, 2, [UIntPtr]::Zero)
Start-Sleep -Milliseconds 800
Shot $h 'b_左移.png'
# hold W (0x57): move forward
[UiTool]::keybd_event(0x57, 0, 0, [UIntPtr]::Zero)
Start-Sleep -Seconds $HoldSeconds
[UiTool]::keybd_event(0x57, 0, 2, [UIntPtr]::Zero)
Start-Sleep -Milliseconds 800
Shot $h 'c_前进.png'
# resize window to trigger the resize branch
[UiTool]::MoveWindow($h, 100, 100, 1100, 720, $true) | Out-Null
Start-Sleep -Seconds 2
Shot $h 'd_缩放后.png'
Start-Sleep -Seconds $TailSeconds
if ($proc.HasExited) { Write-Output "EXITED after run code=$($proc.ExitCode)" } else { Write-Output "ALIVE after extra ${TailSeconds}s" }
Shot $h 'e_结束前.png'
Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue
Write-Output 'done'
