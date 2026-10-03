$d1 = "D:\agent\working_station\silver_era\_tmp_cltest"
New-Item -ItemType Directory -Path $d1 -Force | Out-Null
Set-Content -Path "$d1\t.c" -Value "int main(void){return 0;}" -Encoding Ascii

function Get-EnvSize {
    $n = 0
    foreach ($k in [System.Environment]::GetEnvironmentVariables().Keys) {
        $n += $k.Length + ("$([System.Environment]::GetEnvironmentVariable($k))").Length + 2
    }
    return $n
}

Write-Host ("=== A) default code page (no chcp) ===")
Write-Host ("Console CP: " + (chcp))
Import-Module "D:\ViusalStudioCommunity2026\Common7\Tools\Microsoft.VisualStudio.DevShell.dll"
Enter-VsDevShell -VsInstallPath "D:\ViusalStudioCommunity2026" -SkipAutomaticLocation -DevCmdArguments '-arch=x64 -host_arch=x64' | Out-Null
Write-Host ("Env block size: " + (Get-EnvSize) + " ; PATH len: " + $env:PATH.Length)
& cl /nologo /c /Fo"$d1\a.obj" "$d1\t.c"
Write-Host "exit=$LASTEXITCODE"

Write-Host ("=== B) chcp 65001 then cl ===")
chcp 65001 | Out-Null
& cl /nologo /c /Fo"$d1\b.obj" "$d1\t.c"
Write-Host "exit=$LASTEXITCODE"

Write-Host ("=== C) chcp 936 then cl ===")
chcp 936 | Out-Null
& cl /nologo /c /Fo"$d1\c.obj" "$d1\t.c"
Write-Host "exit=$LASTEXITCODE"