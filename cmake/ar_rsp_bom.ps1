# =============================================================================
# 归档响应文件 BOM 包装器
# -----------------------------------------------------------------------------
# 用途
#     Ninja 生成的归档响应文件（*.rsp）是 UTF-8 无 BOM。MSVC 的命令行工具在没有
#     BOM 时按系统 ANSI 代码页解读响应文件，文件里的中文对象名会被解成乱码，
#     lib.exe 找不到对应文件，报 LNK1181。
#     本脚本在调用真正的归档器之前，先给响应文件补上 UTF-8 BOM。
#
# 调用来源
#     CMake 模板 CMAKE_CXX_CREATE_STATIC_LIBRARY，命令行形如：
#         powershell -NoProfile -ExecutionPolicy Bypass -File 本脚本 <真实归档器> <归档器参数...>
#     第一个参数是真实归档器路径，其余参数原样转交给它。
#
# 处理规则
#     1. 找出以 @ 开头的参数，取出其后的响应文件路径
#     2. 文件已有 UTF-8 BOM，或内容全是 ASCII，则不改动
#     3. 否则在文件开头补上 EF BB BF 三个字节，其余字节原样保留
#     4. 调用真实归档器，并原样透传其退出码
#
# 诊断开关
#     环境变量 BYJY_AR_WRAPPER_DISABLE_BOM=1 可跳过补 BOM，仅用于故障对照实验。
# =============================================================================

$ErrorActionPreference = 'Stop'

# 第一个参数是真实归档器路径，其余是它的参数
$arExe = $args[0]
$arArgs = @()
if ($args.Count -gt 1) { $arArgs = $args[1..($args.Count - 1)] }

# 缺少归档器路径时直接失败，避免静默走错
if (-not $arExe) {
    Write-Error '归档包装器：缺少真实归档器路径参数'
    exit 2
}

# 诊断开关：仅对照实验使用
$skipBom = ($env:BYJY_AR_WRAPPER_DISABLE_BOM -eq '1')

if (-not $skipBom) {
    foreach ($arg in $arArgs) {
        # 只处理 @ 开头的响应文件参数
        if ($arg -and $arg.StartsWith('@')) {
            $rspPath = [System.IO.Path]::GetFullPath($arg.Substring(1))
            if ([System.IO.File]::Exists($rspPath)) {
                $bytes = [System.IO.File]::ReadAllBytes($rspPath)
                # 已经是带 BOM 的 UTF-8 时不再处理
                $hasBom = ($bytes.Length -ge 3 -and
                           $bytes[0] -eq 0xEF -and $bytes[1] -eq 0xBB -and $bytes[2] -eq 0xBF)
                # 内容全是 ASCII 时与代码页无关，也不必处理
                $hasNonAscii = $false
                foreach ($b in $bytes) {
                    if ($b -gt 0x7F) {
                        $hasNonAscii = $true
                        break
                    }
                }
                if (-not $hasBom -and $hasNonAscii) {
                    # 补 BOM：只动开头三个字节，其余原样搬运
                    $patched = New-Object byte[] ($bytes.Length + 3)
                    $patched[0] = 0xEF
                    $patched[1] = 0xBB
                    $patched[2] = 0xBF
                    [System.Array]::Copy($bytes, 0, $patched, 3, $bytes.Length)
                    [System.IO.File]::WriteAllBytes($rspPath, $patched)
                }
            }
        }
    }
}

# 调用真正的归档器，原样透传退出码
& $arExe @arArgs
exit $LASTEXITCODE
