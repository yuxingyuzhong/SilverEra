chcp 65001 | Out-Null
Import-Module "D:\ViusalStudioCommunity2026\Common7\Tools\Microsoft.VisualStudio.DevShell.dll"
Enter-VsDevShell -VsInstallPath "D:\ViusalStudioCommunity2026" -SkipAutomaticLocation -DevCmdArguments '-arch=x64 -host_arch=x64' | Out-Null

Write-Host ("PATH length: " + $env:PATH.Length)
Write-Host ("INCLUDE set: " + [bool]$env:INCLUDE)
Write-Host ("ConsoleOutputEncoding: " + [Console]::OutputEncoding.WebName)

$d1 = "D:\agent\working_station\silver_era\_tmp_cltest"
New-Item -ItemType Directory -Path $d1 -Force | Out-Null
Set-Content -Path "$d1\t.c" -Value "int main(void){return 0;}" -Encoding Ascii

Write-Host "--- ASCII path, /Zi ---"
& cl /nologo /c /Zi /Fo"$d1\t1.obj" "$d1\t.c"
Write-Host "exit=$LASTEXITCODE"

Write-Host "--- ASCII path, /Z7 ---"
& cl /nologo /c /Z7 /Fo"$d1\t2.obj" "$d1\t.c"
Write-Host "exit=$LASTEXITCODE"

$d2 = "d:\代码存储\代码仓库\白银纪元\out\_cltest"
New-Item -ItemType Directory -Path $d2 -Force | Out-Null
Set-Content -Path "$d2\t.c" -Value "int main(void){return 0;}" -Encoding Ascii

Write-Host "--- Chinese path, /Zi ---"
& cl /nologo /c /Zi /Fo"$d2\t1.obj" "$d2\t.c"
Write-Host "exit=$LASTEXITCODE"

Write-Host "--- Chinese path, /Z7 ---"
& cl /nologo /c /Z7 /Fo"$d2\t2.obj" "$d2\t.c"
Write-Host "exit=$LASTEXITCODE"