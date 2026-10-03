$ErrorActionPreference = 'Continue'
Import-Module "D:\ViusalStudioCommunity2026\Common7\Tools\Microsoft.VisualStudio.DevShell.dll"
Enter-VsDevShell -VsInstallPath "D:\ViusalStudioCommunity2026" -SkipAutomaticLocation -DevCmdArguments '-arch=x64 -host_arch=x64' | Out-Null
chcp 65001 | Out-Null

$d = "d:\代码存储\代码仓库\白银纪元\out\_tryc"
New-Item -ItemType Directory -Path $d -Force | Out-Null
Set-Content -Path "$d\testCCompiler.c" -Value "int main(void){return 0;}" -Encoding Ascii
Set-Location $d

Write-Host "=== 1) minimal flags, cwd=Chinese ==="
& cl.exe /nologo -c "$d\testCCompiler.c" /Fo"$d\a.obj"
Write-Host "exit=$LASTEXITCODE"

Write-Host "=== 2) exact ninja flags (with /showIncludes) ==="
& cl.exe /nologo -D_MBCS /DWIN32 /D_WINDOWS /Zi /Ob0 /Od /RTC1 -MDd /showIncludes /Fo"$d\b.obj" -c "$d\testCCompiler.c"
Write-Host "exit=$LASTEXITCODE"

Write-Host "=== 3) same flags but WITHOUT /showIncludes ==="
& cl.exe /nologo -D_MBCS /DWIN32 /D_WINDOWS /Zi /Ob0 /Od /RTC1 -MDd /Fo"$d\c.obj" -c "$d\testCCompiler.c"
Write-Host "exit=$LASTEXITCODE"

Write-Host "=== 4) with /showIncludes only ==="
& cl.exe /nologo /showIncludes /Fo"$d\d.obj" -c "$d\testCCompiler.c"
Write-Host "exit=$LASTEXITCODE"