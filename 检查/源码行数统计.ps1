# 白银纪元源码行数统计
# 用法：powershell -ExecutionPolicy Bypass -File "D:\agent\working_station\silver_era\检查\源码行数统计.ps1"
# 口径：
#   总行数   = 文件全部行（含空行与纯空白行）
#   有效行   = 去掉空行 / 纯空白行后的行数
#   统计范围 = Engine/EngineCore、Engine/EngineSystem、Application/{Renderer,Test,Game,Editor} 下的源码文件
#   排除目录 = external（第三方副本）、out（构建产物）、.git / .vs / .cyrene / .trae 等工具目录
#   源码后缀 = .h .hpp .hxx .c .cpp .cc .cxx .inl .glsl .vert .frag .comp .geom .tesc .tese

param(
    [string]$Root = "d:\代码存储\代码仓库\白银纪元"
)

$extensions = @('.h', '.hpp', '.hxx', '.c', '.cpp', '.cc', '.cxx', '.inl',
                '.glsl', '.vert', '.frag', '.comp', '.geom', '.tesc', '.tese')
$excludeNames = @('external', 'out', 'build', '.git', '.vs', '.cyrene', '.trae', 'node_modules', '下层依赖')

# 各层（相对项目根）
$layers = @(
    'Engine\EngineCore',
    'Engine\EngineSystem',
    'Application\Renderer',
    'Application\Test',
    'Application\Game',
    'Application\Editor'
)

# 判断路径中是否包含被排除目录
function Test_Excluded([string]$relative_path)
{
    $parts = $relative_path -split '[\\/]'
    foreach ($part in $parts)
    {
        if ($excludeNames -contains $part.ToLower())
        {
            return $true
        }
    }
    return $false
}

# 收集某层下的源码文件（返回相对该层的路径）
function Get_Source_Files([string]$layer_full)
{
    $items = New-Object System.Collections.Generic.List[string]
    if (-not (Test-Path $layer_full))
    {
        return $items
    }
    $files = Get-ChildItem -Path $layer_full -Recurse -File -ErrorAction SilentlyContinue
    foreach ($file in $files)
    {
        if ($extensions -notcontains $file.Extension.ToLower())
        {
            continue
        }
        $relative = $file.FullName.Substring($layer_full.Length + 1)
        if (Test_Excluded $relative)
        {
            continue
        }
        $items.Add($relative)
    }
    return $items
}

# 统计单个文件
function Get_File_Counts([string]$full_path)
{
    $lines = [System.IO.File]::ReadAllLines($full_path, [System.Text.Encoding]::UTF8)
    $total = $lines.Length
    $effective = 0
    foreach ($line in $lines)
    {
        if ($line.Trim().Length -gt 0)
        {
            $effective += 1
        }
    }
    return @{ Total = $total; Effective = $effective }
}

# 分组长名（相对层路径 → 汇总分组）
function Get_Group_Name([string]$relative)
{
    $parts = $relative -split '[\\/]'
    if ($parts.Length -ge 3 -and $parts[0].ToLower() -eq 'src')
    {
        return ($parts[0] + '/' + $parts[1])
    }
    if ($parts.Length -ge 2 -and $parts[0].ToLower() -ne 'src')
    {
        return $parts[0]
    }
    return '(根)'
}

$overall_files = 0
$overall_total = 0
$overall_effective = 0
$layer_rows = New-Object System.Collections.Generic.List[object]
$group_rows = New-Object System.Collections.Generic.List[object]

foreach ($layer in $layers)
{
    $layer_full = Join-Path $Root $layer
    $relative_files = Get_Source_Files $layer_full
    $layer_files = 0
    $layer_total = 0
    $layer_effective = 0

    $groups = @{}
    foreach ($relative in $relative_files)
    {
        $counts = Get_File_Counts (Join-Path $layer_full $relative)
        $layer_files += 1
        $layer_total += $counts.Total
        $layer_effective += $counts.Effective

        $group = Get_Group_Name $relative
        if (-not $groups.ContainsKey($group))
        {
            $groups[$group] = @{ Files = 0; Total = 0; Effective = 0 }
        }
        $groups[$group].Files += 1
        $groups[$group].Total += $counts.Total
        $groups[$group].Effective += $counts.Effective
    }

    $overall_files += $layer_files
    $overall_total += $layer_total
    $overall_effective += $layer_effective

    $layer_rows.Add([pscustomobject]@{
        Layer = $layer
        Files = $layer_files
        Total = $layer_total
        Effective = $layer_effective
    })

    foreach ($key in ($groups.Keys | Sort-Object))
    {
        $group_rows.Add([pscustomobject]@{
            Layer = $layer
            Group = $key
            Files = $groups[$key].Files
            Total = $groups[$key].Total
            Effective = $groups[$key].Effective
        })
    }
}

Write-Output "===== 白银纪元 · 源码行数统计 ====="
Write-Output ("项目根：{0}" -f $Root)
Write-Output ("口径：总行数含空行；有效行 = 非空非纯空白行；已排除 external / out / 下层依赖快照 / 工具目录")
Write-Output ""

Write-Output "【总计】"
Write-Output ("  文件数   : {0}" -f $overall_files)
Write-Output ("  总行数   : {0}" -f $overall_total)
Write-Output ("  有效行   : {0}" -f $overall_effective)
Write-Output ""

Write-Output "【各层汇总】"
$layer_rows | Format-Table -AutoSize Layer, Files, Total, Effective
Write-Output ""

Write-Output "【各文件夹（层内二级目录）明细】"
$group_rows | Sort-Object Effective -Descending | Format-Table -AutoSize Layer, Group, Files, Total, Effective
