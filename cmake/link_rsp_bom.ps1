# =============================================================================
# 链接响应文件 BOM 包装器（MSVC + Ninja 专用）
# -----------------------------------------------------------------------------
# 背景：Ninja 写出的链接响应文件（*.rsp）是 UTF-8 无 BOM，MSVC 的 link.exe 在
#       没有 BOM 时按系统 ANSI 代码页解读文件内容，中文对象名会被解成乱码，
#       链接阶段报 LNK1104。
# 用法：由 CMAKE_<LANG>_LINKER_LAUNCHER 前置调用，本脚本第一个参数是真实的
#       link.exe，其余参数原样透传（其中可能包含 @xxxx.rsp）。
# 做法：给响应文件补 UTF-8 BOM（已带 BOM 则不动），再调用真实链接器。
# =============================================================================

param(
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$LinkArgs
)

$ErrorActionPreference = "Stop"

if (-not $LinkArgs -or $LinkArgs.Count -lt 1) {
    throw "链接包装器：缺少 link.exe 路径"
}

# 真实链接器与其余参数
$linker = $LinkArgs[0]
$rest = @()
if ($LinkArgs.Count -gt 1) {
    $rest = $LinkArgs[1..($LinkArgs.Count - 1)]
}

# 给所有 @*.rsp 响应文件补 BOM
foreach ($arg in $rest) {
    if ($arg -match '^@(.+\.rsp)$') {
        $rsp = $Matches[1]
        if (Test-Path $rsp) {
            $bytes = [IO.File]::ReadAllBytes($rsp)
            $hasBom = ($bytes.Length -ge 3 -and $bytes[0] -eq 239 -and $bytes[1] -eq 187 -and $bytes[2] -eq 191)
            if (-not $hasBom) {
                $text = [Text.Encoding]::UTF8.GetString($bytes)
                [IO.File]::WriteAllText($rsp, $text, (New-Object Text.UTF8Encoding($true)))
            }
        }
    }
}

# 调用真实链接器
& $linker @rest
exit $LASTEXITCODE
