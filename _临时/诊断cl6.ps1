$ErrorActionPreference = 'Continue'
Import-Module "D:\ViusalStudioCommunity2026\Common7\Tools\Microsoft.VisualStudio.DevShell.dll"
Enter-VsDevShell -VsInstallPath "D:\ViusalStudioCommunity2026" -SkipAutomaticLocation -DevCmdArguments '-arch=x64 -host_arch=x64' | Out-Null

Write-Host ("TMP=" + $env:TMP)
Write-Host ("TEMP=" + $env:TEMP)
Write-Host ("TMP EXISTS=" + (Test-Path $env:TMP))

$d = "d:\代码存储\代码仓库\白银纪元\out\_tryc"
New-Item -ItemType Directory -Path $d -Force | Out-Null
Set-Content -Path "$d\testCCompiler.c" -Value "int main(void){return 0;}" -Encoding Ascii
Set-Location $d

function Try-Cl {
    param([string]$Name, [string[]]$Flags)
    $out = Join-Path $d "$Name.obj"
    if (Test-Path $out) { Remove-Item $out -Force }
    $r = & cl.exe /nologo @Flags /Fo"$out" -c "$d\testCCompiler.c" 2>&1
    Write-Host ("{0,-16} exit={1}  {2}" -f $Name, $LASTEXITCODE, ($r | Select-Object -Last 1))
}

Try-Cl -Name 'Z7'      -Flags @('/Z7')
Try-Cl -Name 'Zi_nofd' -Flags @('/Zi')
Try-Cl -Name 'Zi_fd_ascii' -Flags @('/Zi', '/FdD:\agent\working_station\silver_era\_tmp_cltest\dbg.pdb')
Try-Cl -Name 'nodb'    -Flags @('/Ob0', '/Od', '/RTC1')

Write-Host "--- mspdbsrv.exe present? ---"
Get-ChildItem "D:\ViusalStudioCommunity2026\VC\Tools\MSVC\14.51.36231\bin\Hostx64\x64" -Filter "mspdbsrv.exe" | Select-Object -ExpandProperty Name
Get-ChildItem "D:\ViusalStudioCommunity2026\VC\Tools\MSVC\14.51.36231\bin\Hostx64\x64" -Filter "c1.dll" | Select-Object -ExpandProperty Name
Get-ChildItem "D:\ViusalStudioCommunity2026\VC\Tools\MSVC\14.51.36231\bin\Hostx64\x64" -Filter "c1xx.dll" | Select-Object -ExpandProperty Name