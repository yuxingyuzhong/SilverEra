$ErrorActionPreference = 'Continue'
Import-Module "D:\ViusalStudioCommunity2026\Common7\Tools\Microsoft.VisualStudio.DevShell.dll"
Enter-VsDevShell -VsInstallPath "D:\ViusalStudioCommunity2026" -SkipAutomaticLocation -DevCmdArguments '-arch=x64 -host_arch=x64' | Out-Null

$d = "d:\代码存储\代码仓库\白银纪元\out\_tryc"
New-Item -ItemType Directory -Path $d -Force | Out-Null
Set-Content -Path "$d\testCCompiler.c" -Value "int main(void){return 0;}" -Encoding Ascii
Set-Location $d

$asciiTmp = "D:\agent\working_station\silver_era\_tmp_cltest"
New-Item -ItemType Directory -Path $asciiTmp -Force | Out-Null

function Try-Cl {
    param([string]$Name, [string[]]$Flags)
    $out = Join-Path $d "$Name.obj"
    if (Test-Path $out) { Remove-Item $out -Force }
    $r = & cl.exe /nologo @Flags /Fo"$out" -c "$d\testCCompiler.c" 2>&1
    Write-Host ("{0,-20} exit={1}  {2}" -f $Name, $LASTEXITCODE, ($r | Select-Object -Last 1))
}

Write-Host ("env size = " + ((Get-ChildItem env: | ForEach-Object { $_.Name.Length + $_.Value.Length + 2 } | Measure-Object -Sum).Sum))

Write-Host "--- baseline /Zi ---"
Try-Cl -Name 'base_Zi' -Flags @('/Zi')

Write-Host "--- with ASCII TMP/TEMP ---"
$env:TMP = $asciiTmp
$env:TEMP = $asciiTmp
Try-Cl -Name 'asciitmp_Zi' -Flags @('/Zi')

Write-Host "--- trimmed env (only PATH/INCLUDE/LIB/TMP + SystemRoot) ---"
$keep = @('PATH', 'INCLUDE', 'LIB', 'LIBPATH', 'TMP', 'TEMP', 'SystemRoot', 'windir', 'PATHEXT', 'ComSpec')
foreach ($name in @(Get-ChildItem env: | Select-Object -ExpandProperty Name)) {
    if ($keep -notcontains $name) { Remove-Item "env:$name" -ErrorAction SilentlyContinue }
}
Write-Host ("trimmed env size = " + ((Get-ChildItem env: | ForEach-Object { $_.Name.Length + $_.Value.Length + 2 } | Measure-Object -Sum).Sum))
Try-Cl -Name 'trimmed_Zi' -Flags @('/Zi')