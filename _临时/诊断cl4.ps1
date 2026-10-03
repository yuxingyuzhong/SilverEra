$ErrorActionPreference = 'Continue'
Import-Module "D:\ViusalStudioCommunity2026\Common7\Tools\Microsoft.VisualStudio.DevShell.dll"
Enter-VsDevShell -VsInstallPath "D:\ViusalStudioCommunity2026" -SkipAutomaticLocation -DevCmdArguments '-arch=x64 -host_arch=x64' | Out-Null

$d = "d:\代码存储\代码仓库\白银纪元\out\_tryc"
New-Item -ItemType Directory -Path $d -Force | Out-Null
Set-Content -Path "$d\testCCompiler.c" -Value "int main(void){return 0;}" -Encoding Ascii
Set-Location $d

Write-Host ("System ACP: " + [System.Text.Encoding]::Default.WebName + " | OEMCP via chcp: " + (chcp))

function Try-Cl {
    param([string]$Name, [int]$Cp, [string[]]$Flags)
    chcp $Cp | Out-Null
    $out = Join-Path $d "$Name.obj"
    & cl.exe /nologo @Flags /Fo"$out" -c "$d\testCCompiler.c" 2>&1 | Out-Null
    Write-Host ("{0,-28} CP={1}  exit={2}" -f $Name, $Cp, $LASTEXITCODE)
}

Try-Cl -Name 'zi_cp65001' -Cp 65001 -Flags @('/Zi', '/Od')
Try-Cl -Name 'z7_cp65001' -Cp 65001 -Flags @('/Z7', '/Od')
Try-Cl -Name 'zi_cp936'   -Cp 936   -Flags @('/Zi', '/Od')
Try-Cl -Name 'z7_cp936'   -Cp 936   -Flags @('/Z7', '/Od')
Try-Cl -Name 'zi_full_cp65001' -Cp 65001 -Flags @('-D_MBCS', '/DWIN32', '/D_WINDOWS', '/Zi', '/Ob0', '/Od', '/RTC1', '-MDd')
Try-Cl -Name 'z7_full_cp65001' -Cp 65001 -Flags @('-D_MBCS', '/DWIN32', '/D_WINDOWS', '/Z7', '/Ob0', '/Od', '/RTC1', '-MDd')
Try-Cl -Name 'zi_full_cp936'   -Cp 936   -Flags @('-D_MBCS', '/DWIN32', '/D_WINDOWS', '/Zi', '/Ob0', '/Od', '/RTC1', '-MDd')
Try-Cl -Name 'z7_full_cp936'   -Cp 936   -Flags @('-D_MBCS', '/DWIN32', '/D_WINDOWS', '/Z7', '/Ob0', '/Od', '/RTC1', '-MDd')