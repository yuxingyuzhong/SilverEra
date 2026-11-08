# =============================================================================
# 白银纪元 · 项目导航新鲜度体检
# -----------------------------------------------------------------------------
# 用途：读取版次导航目录下的 导航清单.md，逐模块比对源码变更与文档更新日期，
#       输出“新鲜 / 源已变更 / 有未提交改动 / 路径缺失 / 无法比对”。
#
# 判定口径：
#   1. 源码路径存在性；
#   2. 源码范围内是否未提交/未入库改动（git status）；
#   3. 源码范围内最后一次提交日期是否晚于文档“更新日期”；
#   4. 层版次（git 短哈希）与清单记录是否一致，仅作提示，不单独判过期。
#
# 用法：
#   .\导航体检.ps1                      # 体检最新清单
#   .\导航体检.ps1 -NoFail              # 有落后项也返回 0
#   .\导航体检.ps1 -StampHeaders        # 先按清单写入模块文件头，再体检
#   .\导航体检.ps1 -NavDir "<版次目录>" # 指定版次目录
# =============================================================================

param(
    [string]$ProjectRoot = "D:\代码仓库\白银纪元",
    [string]$NavDir = "",
    [ValidateSet("Debug", "Release")]
    [string]$Config = "Debug",
    [switch]$NoFail,
    [switch]$StampHeaders
)

$ErrorActionPreference = "Stop"
$WorkstationRoot = $PSScriptRoot
$NavBase = Join-Path $WorkstationRoot "项目导航合集\main"

# ---------- 定位版次目录 ----------
if ([string]::IsNullOrWhiteSpace($NavDir)) {
    $candidates = Get-ChildItem -Path $NavBase -Directory | ForEach-Object {
        $manifest = Join-Path $_.FullName "导航清单.md"
        if (Test-Path $manifest) {
            [pscustomobject]@{ Dir = $_.FullName; Time = (Get-Item $manifest).LastWriteTime }
        }
    } | Sort-Object Time -Descending
    if (-not $candidates) {
        throw ("未找到任何 导航清单.md：{0}" -f $NavBase)
    }
    $NavDir = $candidates[0].Dir
}

$manifestPath = Join-Path $NavDir "导航清单.md"
if (-not (Test-Path $manifestPath)) {
    throw ("清单不存在：{0}" -f $manifestPath)
}

# ---------- 基础工具 ----------
function Read-Utf8([string]$path) {
    return [System.IO.File]::ReadAllText($path, [System.Text.Encoding]::UTF8)
}

function Get-RepoRoot([string]$path) {
    $current = $path
    if (-not (Test-Path $current)) {
        return $null
    }
    if (Test-Path $current -PathType Leaf) {
        $current = Split-Path $current -Parent
    }
    while ($current) {
        if (Test-Path (Join-Path $current ".git")) {
            return $current
        }
        $parent = Split-Path $current -Parent
        if ([string]::IsNullOrEmpty($parent) -or $parent -eq $current) {
            break
        }
        $current = $parent
    }
    return $null
}

# 层 → 仓库相对路径（工作区/构建系统取项目根仓库）
$layerRepo = @{
    "EngineFoundation" = "Engine\EngineFoundation"
    "EngineSystem"     = "Engine\EngineSystem"
    "EngineRenderer"   = "Engine\EngineRenderer"
    "Test"             = "Application\Test"
    "星屑弹幕"         = "Application\Game\星屑弹幕"
    "工作区"           = ""
    "构建系统"         = ""
}

function Get-LayerHash([string]$layer) {
    if (-not $layerRepo.ContainsKey($layer)) {
        return $null
    }
    $repo = if ($layerRepo[$layer]) { Join-Path $ProjectRoot $layerRepo[$layer] } else { $ProjectRoot }
    $hash = git -C $repo rev-parse --short=7 HEAD 2>$null
    if ($LASTEXITCODE -ne 0) {
        return $null
    }
    return $hash.Trim()
}

function Get-AnchorHash([string]$layer) {
    $anchor = Join-Path $ProjectRoot ("Engine\{0}\out\build\x64-{1}\include\Engine\{0}\版本锚点.h" -f $layer, $Config)
    if (-not (Test-Path $anchor)) {
        return "-"
    }
    $text = Read-Utf8 $anchor
    $match = [regex]::Match($text, "byjy_stamp_[a-z_]+_([0-9a-f]{12})")
    if ($match.Success) {
        return $match.Groups[1].Value
    }
    return "-"
}

# ---------- 读取清单 ----------
$manifestText = Read-Utf8 $manifestPath
$rows = @()
foreach ($line in ($manifestText -split "`r?`n")) {
    $trimmed = $line.Trim()
    if (-not $trimmed.StartsWith("|")) {
        continue
    }
    $cells = @($trimmed.Trim('|').Split('|') | ForEach-Object { $_.Trim() })
    if ($cells.Count -lt 6) {
        continue
    }
    if ($cells[0] -eq "模块") {
        continue
    }
    if ($cells[0] -match '^-+$') {
        continue
    }
    $sources = @($cells[1].Split(';') | ForEach-Object {
        $_.Trim().Trim('`').Trim()
    } | Where-Object { $_ })
    $rows += [pscustomobject]@{
        Module  = $cells[0]
        Sources = $sources
        Layer   = $cells[2]
        Version = $cells[3].Trim('`').Trim()
        Updated = $cells[4].Trim('`').Trim()
        Note    = $cells[5]
    }
}
if ($rows.Count -eq 0) {
    throw ("清单未解析出任何模块行：{0}" -f $manifestPath)
}

# ---------- 模块文档索引 ----------
$docIndex = @{}
$moduleRoot = Join-Path $NavDir "模块架构"
if (Test-Path $moduleRoot) {
    Get-ChildItem -Path $moduleRoot -Recurse -File -Filter "*.md" | ForEach-Object {
        if (-not $docIndex.ContainsKey($_.BaseName)) {
            $docIndex[$_.BaseName] = @()
        }
        $docIndex[$_.BaseName] += $_.FullName
    }
}

# ---------- 写入文件头 ----------
if ($StampHeaders) {
    foreach ($row in $rows) {
        if (-not $docIndex.ContainsKey($row.Module) -or $docIndex[$row.Module].Count -ne 1) {
            Write-Warning ("模块文档无法唯一定位，跳过：{0}" -f $row.Module)
            continue
        }
        $docPath = $docIndex[$row.Module][0]
        $basis = if ($row.Note -match "当前源码") { "工作区" } else { "冻结快照 465703f" }
        $header = "> 依据版次：{0} {1} ｜ 依据：{2} ｜ 更新：{3}" -f `
            $row.Layer, $row.Version, $basis, $row.Updated

        $bytes = [System.IO.File]::ReadAllBytes($docPath)
        $hasBom = ($bytes.Length -ge 3 -and $bytes[0] -eq 0xEF -and $bytes[1] -eq 0xBB -and $bytes[2] -eq 0xBF)
        $content = Read-Utf8 $docPath
        $newline = if ($content.Contains("`r`n")) { "`r`n" } else { "`n" }
        $lines = @($content -split "`r?`n")

        # 定位首个 H1
        $h1 = -1
        for ($i = 0; $i -lt $lines.Count; $i++) {
            if ($lines[$i] -match '^# ') {
                $h1 = $i
                break
            }
        }
        if ($h1 -lt 0) {
            Write-Warning ("模块文档缺少 H1，跳过：{0}" -f $docPath)
            continue
        }

        # H1 后首个非空行
        $next = $h1 + 1
        while ($next -lt $lines.Count -and $lines[$next].Trim() -eq "") {
            $next++
        }

        $newLines = New-Object System.Collections.ArrayList
        for ($i = 0; $i -le $h1; $i++) {
            [void]$newLines.Add($lines[$i])
        }
        [void]$newLines.Add("")
        [void]$newLines.Add($header)
        # 已有旧头则跳过旧头
        if ($next -lt $lines.Count -and $lines[$next] -match '^> 依据版次：') {
            $next++
        }
        # 文件头与正文之间保留一个空行
        if ($next -lt $lines.Count -and $lines[$next].Trim() -ne "") {
            [void]$newLines.Add("")
        }
        for ($i = $next; $i -lt $lines.Count; $i++) {
            [void]$newLines.Add($lines[$i])
        }

        $newText = ($newLines -join $newline)
        if ($newText -ne $content) {
            [System.IO.File]::WriteAllText($docPath, $newText,
                (New-Object System.Text.UTF8Encoding($hasBom)))
            Write-Host ("已写入文件头：{0}" -f (Split-Path $docPath -Leaf))
        }
    }
}

# ---------- 体检 ----------
$results = @()
foreach ($row in $rows) {
    $missing = @()
    $dirty = @()
    $lastDate = $null
    $noRepo = $false

    foreach ($source in $row.Sources) {
        $absolute = Join-Path $ProjectRoot $source
        if (-not (Test-Path $absolute)) {
            $missing += $source
            continue
        }
        $repo = Get-RepoRoot $absolute
        if (-not $repo) {
            $noRepo = $true
            continue
        }
        $relative = $absolute.Substring($repo.Length).TrimStart('\', '/')
        $date = git -C $repo log -1 --format=%cs -- $relative 2>$null
        if ($LASTEXITCODE -eq 0 -and $date) {
            $date = $date.Trim()
            if (-not $lastDate -or $date -gt $lastDate) {
                $lastDate = $date
            }
        }
        $changes = git -C $repo status --porcelain -- $relative 2>$null
        if ($changes) {
            $dirty += $source
        }
    }

    $status = "新鲜"
    if ($missing.Count -gt 0) {
        $status = "路径缺失"
    }
    elseif ($dirty.Count -gt 0) {
        $status = "有未提交改动"
    }
    elseif ($lastDate -and $lastDate -gt $row.Updated) {
        $status = "源已变更"
    }
    elseif ($noRepo -or -not $lastDate) {
        $status = "无法比对"
    }

    $current = Get-LayerHash $row.Layer
    $layerNote = ""
    if ($current -and $row.Version -ne $current) {
        $layerNote = ("层版次已变 当前{0}" -f $current)
    }
    $anchor = "-"
    if ($row.Layer -eq "EngineFoundation" -or $row.Layer -eq "EngineSystem" -or $row.Layer -eq "EngineRenderer") {
        $anchor = Get-AnchorHash $row.Layer
    }

    $results += [pscustomobject]@{
        Module   = $row.Module
        Status   = $status
        Layer    = $row.Layer
        Version  = $row.Version
        Current  = if ($current) { $current } else { "-" }
        Anchor   = $anchor
        Updated  = $row.Updated
        LastDate = if ($lastDate) { $lastDate } else { "-" }
        Note     = $layerNote
    }
}

# ---------- 输出 ----------
Write-Host ""
Write-Host ("导航体检：{0}" -f $NavDir) -ForegroundColor Cyan
Write-Host ("源码基准：{0}" -f $ProjectRoot)
Write-Host ""
Write-Host ("{0,-16} {1,-14} {2,-12} {3,-8} {4,-8} {5,-12} {6}" -f `
        "模块", "状态", "依据版次", "当前", "锚点", "源码最后提交", "提示")

foreach ($result in $results) {
    $color = switch ($result.Status) {
        "新鲜" { "Green" }
        "源已变更" { "Yellow" }
        "有未提交改动" { "Yellow" }
        "路径缺失" { "Red" }
        default { "DarkYellow" }
    }
    Write-Host ("{0,-16} {1,-14} {2,-12} {3,-8} {4,-8} {5,-12} {6}" -f `
            $result.Module, $result.Status, $result.Version, $result.Current,
            $result.Anchor, $result.LastDate, $result.Note) -ForegroundColor $color
}

$fresh = @($results | Where-Object { $_.Status -eq "新鲜" }).Count
$changed = @($results | Where-Object { $_.Status -eq "源已变更" }).Count
$dirtyCount = @($results | Where-Object { $_.Status -eq "有未提交改动" }).Count
$missingCount = @($results | Where-Object { $_.Status -eq "路径缺失" }).Count
$unknown = @($results | Where-Object { $_.Status -eq "无法比对" }).Count
$staleCount = $changed + $dirtyCount + $missingCount + $unknown

Write-Host ""
Write-Host ("汇总：新鲜 {0} ｜ 源已变更 {1} ｜ 有未提交改动 {2} ｜ 路径缺失 {3} ｜ 无法比对 {4}" -f `
        $fresh, $changed, $dirtyCount, $missingCount, $unknown) -ForegroundColor Cyan

if ($staleCount -gt 0 -and -not $NoFail) {
    exit 1
}
