# 关卡测试拆分脚本（第 562 轮临时脚本，执行后删除）
$src = "D:\代码存储\代码仓库\白银纪元\Application\Test\src\单元测试\Renderer\渲染关卡测试.cpp"
$dst = "D:\代码存储\代码仓库\白银纪元\Application\Test\src\单元测试\Renderer\渲染聚光阴影测试.cpp"
$lines = Get-Content -LiteralPath $src -Encoding UTF8

# 头部说明
$header = @'
/*
渲染聚光阴影测试（第五百六十二轮 · 由 渲染关卡测试.cpp 迁出）
覆盖：八盏聚光灯阴影（前向 / Cluster 路径）、质量参数组合、偏置自校准（关卡行 / 门面）、按需重绘、
分辨率切换、软度开销、与方向光共存、MSAA 组合、逐瓦片互异性（前向 ＋ 簇路径）、点光上限边界。
口径：只移动代码、不改断言；套件名保持 Renderer_Level_Test；临时文件写在 out/关卡测试/。
*/
#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include "Application/Renderer/src/render/Level/关卡文件.h"
#include "Application/Renderer/src/render/Renderer/渲染器.h"

namespace
{
'@

# 辅助函数片段（原文件 32–48 / 50–57 / 65–84 / 105–131 / 133–165 行）
$part1 = ($lines[31..47] -join "`n")
$part2 = ($lines[49..56] -join "`n")
$part3 = ($lines[64..83] -join "`n")
$part4 = ($lines[100..130] -join "`n")
$part5 = ($lines[132..164] -join "`n")

# 迁出块（含注释）：1528–2054 / 2372–2863 / 3497–3519
$block1 = ($lines[1527..2053] -join "`n")
$block2 = ($lines[2371..2862] -join "`n")
$block3 = ($lines[3496..3518] -join "`n")

$body = $header + "`n" + $part1 + "`n`n" + $part2 + "`n`n" + $part3 + "`n`n" + $part4 + "`n`n" + $part5 + "`n}`n`n" + $block1 + "`n`n" + $block2 + "`n`n" + $block3 + "`n"

$utf8 = New-Object System.Text.UTF8Encoding($false)
[System.IO.File]::WriteAllText($dst, $body, $utf8)

# 原文件删除三个块（自下而上）
$remove = @(3496..3518) + @(2371..2862) + @(1527..2053)
$keep = for($i = 0; $i -lt $lines.Count; $i++){ if($remove -notcontains $i){ $lines[$i] } }
$orig = $keep -join "`n"
[System.IO.File]::WriteAllText($src, $orig + "`n", $utf8)

Write-Output ("新文件行数 " + (Get-Content -LiteralPath $dst -Encoding UTF8).Count)
Write-Output ("原文件行数 " + (Get-Content -LiteralPath $src -Encoding UTF8).Count)
