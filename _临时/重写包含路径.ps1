# 一次性包含路径重写脚本（阶段 D，任务<2>）
# 规则：只改写「行首为 #include "」的语句，避免误伤注释与字符串字面量；
#       保留原行尾与编码（UTF-8 无 BOM）。
$ErrorActionPreference = 'Stop'
$root = 'd:\代码存储\代码仓库\白银纪元'
$utf8 = New-Object System.Text.UTF8Encoding($false)

function Rewrite-Tree {
    param(
        [string]   $Tree,
        [string[]] $Exts,
        [object[]] $Rules,
        [string[]] $SkipSegments
    )
    if (-not (Test-Path -LiteralPath $Tree)) { return 0 }
    $changedFiles = 0
    $files = Get-ChildItem -LiteralPath $Tree -Recurse -File | Where-Object { $Exts -contains $_.Extension }
    foreach ($f in $files) {
        $rel = $f.FullName.Substring($Tree.Length)
        $skip = $false
        foreach ($seg in $SkipSegments) { if ($rel -like "*$seg*") { $skip = $true; break } }
        if ($skip) { continue }

        $text = [System.IO.File]::ReadAllText($f.FullName)
        $orig = $text
        foreach ($r in $Rules) {
            $pat = '(?m)^(\s*#include\s+")' + [regex]::Escape([string]$r[0])
            $rep = '${1}' + [string]$r[1]
            $text = [regex]::Replace($text, $pat, $rep)
        }
        if ($text -ne $orig) {
            [System.IO.File]::WriteAllText($f.FullName, $text, $utf8)
            $changedFiles++
        }
    }
    return $changedFiles
}

# ---- EngineCore：common/ 与 src/ 一律补 Engine/EngineCore/ 前缀 ----
$coreRules = @(
    , @('common/', 'Engine/EngineCore/common/')
    , @('src/', 'Engine/EngineCore/src/')
)
$n1 = Rewrite-Tree -Tree "$root\Engine\EngineCore" -Exts @('.h', '.hpp', '.cpp', '.c') -Rules $coreRules -SkipSegments @('external')
"EngineCore 改写文件数: $n1"

# ---- EngineSystem：区分「引 EngineCore」与「引自身」 ----
$sysRules = @(
    , @('../../引擎层/', 'Engine/EngineCore/')
    , @('common/', 'Engine/EngineSystem/common/')
    , @('src/core/', 'Engine/EngineCore/src/core/')
    , @('src/tools/', 'Engine/EngineCore/src/tools/')
    , @('src/prop/', 'Engine/EngineSystem/src/prop/')
    , @('src/entity/', 'Engine/EngineSystem/src/entity/')
    , @('src/effect/', 'Engine/EngineSystem/src/effect/')
    , @('src/gui/', 'Engine/EngineSystem/src/gui/')
    , @('entity/', 'Engine/EngineSystem/src/entity/')
    , @('effect/', 'Engine/EngineSystem/src/effect/')
    , @('gui/', 'Engine/EngineSystem/src/gui/')
)
$n2 = Rewrite-Tree -Tree "$root\Engine\EngineSystem" -Exts @('.h', '.hpp', '.cpp', '.c') -Rules $sysRules -SkipSegments @('external')
"EngineSystem 改写文件数: $n2"

# ---- Test：引 EngineCore + 引自身主调 ----
$testRules = @(
    , @('src/core/', 'Engine/EngineCore/src/core/')
    , @('src/tools/', 'Engine/EngineCore/src/tools/')
    , @('src/主调/', 'Application/Test/src/主调/')
)
$n3 = Rewrite-Tree -Tree "$root\Application\Test" -Exts @('.h', '.hpp', '.cpp', '.c') -Rules $testRules -SkipSegments @('external')
"Test 改写文件数: $n3"

"---- 残留检查（应为 0 行）----"
$left = Select-String -Path "$root\Engine\EngineCore\*\*.h", "$root\Engine\EngineCore\*\*.cpp" -Pattern '^\s*#include\s+"(common/|src/)' -ErrorAction SilentlyContinue
"EngineCore 残留: " + (($left | Measure-Object).Count)