$ErrorActionPreference = 'Continue'
Import-Module "D:\ViusalStudioCommunity2026\Common7\Tools\Microsoft.VisualStudio.DevShell.dll"
Enter-VsDevShell -VsInstallPath "D:\ViusalStudioCommunity2026" -SkipAutomaticLocation -DevCmdArguments '-arch=x64 -host_arch=x64' | Out-Null

$d = "d:\代码存储\代码仓库\白银纪元\out\_tryc"
New-Item -ItemType Directory -Path $d -Force | Out-Null
Set-Content -Path "$d\testCCompiler.c" -Value "int main(void){return 0;}" -Encoding Ascii
Set-Location $d

function Try-Cl {
    param([string]$Name, [string[]]$Flags)
    $out = Join-Path $d "$Name.obj"
    if (Test-Path $out) { Remove-Item $out -Force }
    $r = & cl.exe /nologo @Flags /Fo"$out" -c "$d\testCCompiler.c" 2>&1
    $code = $LASTEXITCODE
    $tail = ($r | Select-Object -Last 1)
    Write-Host ("{0,-22} exit={1}  {2}" -f $Name, $code, $tail)
}

Try-Cl -Name 'none'      -Flags @()
Try-Cl -Name 'Zi'        -Flags @('/Zi')
Try-Cl -Name 'Od'        -Flags @('/Od')
Try-Cl -Name 'Zi_Od'     -Flags @('/Zi', '/Od')
Try-Cl -Name 'Zi_Ob0'    -Flags @('/Zi', '/Ob0')
Try-Cl -Name 'RTC1'      -Flags @('/RTC1')
Try-Cl -Name 'MDd'       -Flags @('-MDd')
Try-Cl -Name 'MD'        -Flags @('-MD')
Try-Cl -Name 'D_MBCS'    -Flags @('-D_MBCS', '/DWIN32', '/D_WINDOWS')
Try-Cl -Name 'Zi_Fd'     -Flags @('/Zi', "/Fd$d\v.pdb")