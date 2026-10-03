param(
    [Parameter(Mandatory = $true)][string]$Layer,
    [switch]$ConfigureOnly,
    [string[]]$Extra = @()
)
# Single-layer configure + build (Ninja + MSVC). ASCII-only on purpose.
# Two environment workarounds required on this machine:
#   1) chcp must run AFTER Enter-VsDevShell (otherwise cl.exe fails with D8050).
#   2) TMP/TEMP must point at an ASCII-only directory: the default
#      C:\Users\<non-ASCII>\AppData\Local\Temp makes cl.exe fail with
#      "D8050 ... could not put command line into the debug record" whenever
#      debug info (/Zi or /Z7) is requested.
$ErrorActionPreference = 'Continue'

Import-Module "D:\ViusalStudioCommunity2026\Common7\Tools\Microsoft.VisualStudio.DevShell.dll"
Enter-VsDevShell -VsInstallPath "D:\ViusalStudioCommunity2026" -SkipAutomaticLocation -DevCmdArguments '-arch=x64 -host_arch=x64' | Out-Null
chcp 65001 | Out-Null

$asciiTmp = "D:\agent\working_station\silver_era\_tmp_build"
New-Item -ItemType Directory -Path $asciiTmp -Force | Out-Null
$env:TMP = $asciiTmp
$env:TEMP = $asciiTmp

$cmake = "D:\ViusalStudioCommunity2026\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
$env:PATH = "D:\ViusalStudioCommunity2026\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja;$env:PATH"
$build = Join-Path $Layer 'out\build\x64-Debug'

Write-Host "===== CONFIGURE: $Layer ====="
& $cmake -S $Layer -B $build -G Ninja @Extra
Write-Host "CONFIGURE EXIT: $LASTEXITCODE"
if ($LASTEXITCODE -ne 0) { exit 1 }
if ($ConfigureOnly) { exit 0 }

Write-Host "===== BUILD: $Layer ====="
& $cmake --build $build
Write-Host "BUILD EXIT: $LASTEXITCODE"
exit $LASTEXITCODE