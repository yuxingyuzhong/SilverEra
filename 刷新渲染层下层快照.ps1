# =============================================================================
# 刷新渲染层下层快照
# -----------------------------------------------------------------------------
# 用途：把 EngineCore 的「头快照」与「静态库」从下层构建树拷贝到渲染层内部，
#       使渲染层构建不再依赖 Engine/EngineCore/out 的实时状态（支持并行开发引擎核心）。
#
# 目标布局（相对项目根）：
#   Application/Renderer/下层依赖/EngineCore/x64-Debug/{include/, lib/EngineCore.lib}
#   Application/Renderer/下层依赖/EngineCore/x64-Release/{include/, lib/EngineCore.lib}
#
# 说明：include 为 EngineCore 构建期导出的镜像（相对项目根保留层级）；
#       *.lib 已被 Application/Renderer/.gitignore 忽略，故库副本仅存本机。
#
# 触发时机：由用户指定；agent 不得在未获指示时自动刷新。
# =============================================================================

param(
    [string]$ProjectRoot = "d:\代码存储\代码仓库\白银纪元",
    [string[]]$Configs = @("Debug", "Release")
)

$ErrorActionPreference = "Stop"
$sourceRoot = Join-Path $ProjectRoot "Engine/EngineCore/out/build"
$targetRoot = Join-Path $ProjectRoot "Application/Renderer/下层依赖/EngineCore"

foreach ($config in $Configs) {
    $sourceTree = Join-Path $sourceRoot "x64-$config"
    $sourceInclude = Join-Path $sourceTree "include"
    $sourceLib = Join-Path $sourceTree "lib/EngineCore.lib"

    if (-not (Test-Path $sourceInclude)) {
        throw "缺少头快照：$sourceInclude（请先构建 EngineCore 的 $config 树）"
    }
    if (-not (Test-Path $sourceLib)) {
        throw "缺少静态库：$sourceLib（请先构建 EngineCore 的 $config 树）"
    }

    $targetTree = Join-Path $targetRoot "x64-$config"
    $targetInclude = Join-Path $targetTree "include"
    $targetLibDir = Join-Path $targetTree "lib"
    New-Item -ItemType Directory -Path $targetLibDir -Force | Out-Null

    # 头快照镜像同步（删除目标中已不存在的旧头）
    $null = robocopy $sourceInclude $targetInclude /MIR /NFL /NDL /NJH /NJS /NP
    if ($LASTEXITCODE -ge 8) {
        throw "头快照拷贝失败（robocopy 退出码 $LASTEXITCODE）"
    }

    # 静态库覆盖拷贝
    Copy-Item $sourceLib (Join-Path $targetLibDir "EngineCore.lib") -Force

    $headerCount = (Get-ChildItem $targetInclude -Recurse -File | Measure-Object).Count
    $libSize = [math]::Round((Get-Item (Join-Path $targetLibDir "EngineCore.lib")).Length / 1MB, 2)
    Write-Host ("[快照] {0}: 头 {1} 个 / 库 {2} MB → {3}" -f $config, $headerCount, $libSize, $targetTree)
}

Write-Host "[快照] 完成：渲染层已使用冻结的 EngineCore 头与库"
