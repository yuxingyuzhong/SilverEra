<#
下层与测试层同步检查（显式检查）

用途
    引擎层改了公共 API 而测试层未同步时，断裂会「潜伏」到下次重配测试层才暴露
    （2026-10-18 第一轮即遇到 Object_Pool order_set 断裂）。本脚本把该风险变成
    一条显式命令：重建下层 → 重配并构建测试层；任一步失败即判定「不同步」。

执行
    powershell -ExecutionPolicy Bypass -File "D:\agent\working_station\silver_era\检查\下层与测试层同步检查.ps1"

结论
    退出码 0  → 下层与测试层同步（测试层可正常编译链接）
    退出码 1  → 不同步：日志尾部含编译器报错，须先迁移测试层再继续

说明
    TMP/TEMP 必须是纯 ASCII 目录（本机用户名含中文会让 cl.exe 带调试信息时失败）；
    chcp 65001 必须在进入 VS 开发人员环境之后执行（否则被开发环境脚本重置）。
#>

$env:TMP = 'D:\agent\_byjy_tmp'
$env:TEMP = 'D:\agent\_byjy_tmp'
New-Item -ItemType Directory -Force -Path 'D:\agent\_byjy_tmp' | Out-Null
New-Item -ItemType Directory -Force -Path 'D:\agent\working_station\silver_era\检查' | Out-Null

& 'D:\ViusalStudioCommunity2026\Common7\Tools\Launch-VsDevShell.ps1' -VsInstallationPath 'D:\ViusalStudioCommunity2026' -Arch amd64 -HostArch amd64 -SkipAutomaticLocation | Out-Null
chcp 65001 | Out-Null

$cmake = 'D:\ViusalStudioCommunity2026\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
$xiaceng = 'd:\代码存储\代码仓库\白银纪元\Engine\EngineCore'
$test    = 'd:\代码存储\代码仓库\白银纪元\Application\Test'
$log     = 'D:\agent\working_station\silver_era\检查\同步检查日志.txt'

"==== 下层与测试层同步检查：$(Get-Date -Format 'yyyy-MM-dd HH:mm:ss') ====" | Out-File -Encoding utf8 $log

# ① 重建下层（EngineCore，Debug 树；连带刷新其对外头快照）
"---- [1/4] 配置下层 EngineCore ----" | Out-File -Encoding utf8 -Append $log
& $cmake -S $xiaceng -B "$xiaceng\out\build\x64-Debug" 2>&1 | Out-File -Encoding utf8 -Append $log
$rc1 = $LASTEXITCODE
"---- [2/4] 构建下层 EngineCore ----" | Out-File -Encoding utf8 -Append $log
& $cmake --build "$xiaceng\out\build\x64-Debug" 2>&1 | Out-File -Encoding utf8 -Append $log
$rc2 = $LASTEXITCODE
# 产出校验：构建返回 0 也可能**没有产物**（2026-10-19 实测遇到 lib 目录为空）；库缺失即判不同步，不进入测试层构建
$xiaceng_lib = "$xiaceng\out\build\x64-Debug\lib\EngineCore.lib"
if (-not (Test-Path $xiaceng_lib))
{
    "结论：不同步（下层 Debug 静态库缺失：$xiaceng_lib）" | Out-File -Encoding utf8 -Append $log
    "结论：不同步 —— 下层 Debug 静态库缺失，请检查下层构建：$xiaceng_lib"
    exit 1
}

# ② 重配并构建测试层：能否编译通过即为「同步」判据
"---- [3/4] 配置测试层（本步会读取下层头快照） ----" | Out-File -Encoding utf8 -Append $log
& $cmake -S $test -B "$test\out\build\x64-Debug" 2>&1 | Out-File -Encoding utf8 -Append $log
$rc3 = $LASTEXITCODE
"---- [4/4] 构建测试层 ----" | Out-File -Encoding utf8 -Append $log
& $cmake --build "$test\out\build\x64-Debug" 2>&1 | Out-File -Encoding utf8 -Append $log
$rc4 = $LASTEXITCODE

$failed = ($rc1 -ne 0) -or ($rc2 -ne 0) -or ($rc3 -ne 0) -or ($rc4 -ne 0)
if ($failed)
{
    "结论：不同步（下层配置 $rc1 / 下层构建 $rc2 / 测试层配置 $rc3 / 测试层构建 $rc4）" | Out-File -Encoding utf8 -Append $log
    "结论：不同步 —— 引擎层 API 与测试层不一致，请先迁移测试层；详见 $log"
    exit 1
}
"结论：同步（下层与测试层均构建通过）" | Out-File -Encoding utf8 -Append $log
"结论：同步 —— 下层与测试层均构建通过；日志 $log"
exit 0
