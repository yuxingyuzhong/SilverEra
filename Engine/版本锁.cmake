# =============================================================================
# 版本锁（全链唯一真源；可选）
# -----------------------------------------------------------------------------
# 位置：Engine/版本锁.cmake
#
# 作用
#     给上层声明「我要用下层的哪个版本」。
#       留空  → 跟随下层最新构建（读 <层>/out/build/x64-<配置>）
#       填值  → 锁定到 Engine/版本缓存/<层>/x64-<配置>/<版本哈希>/ 的冻结副本
#     版本哈希 = 该层版次锚点符号里的 12 位哈希（每配置不同，故按 Debug / Release 分开）。
#
# 为什么按配置分开
#     同一份源码在 Debug / Release 下产出的锚点哈希不同（哈希含构建类型与运行库口径），
#     所以锁必须区分配置，否则 Release 会拿着 Debug 的哈希找不到条目。
#
# 一致性要求（重要）
#     锁是「全链」的：锁了 Foundation 就必须保证所有消费 Foundation 的层都按同一份锁解析，
#     否则 System 用新 Foundation 构建、Renderer 却链旧 Foundation，链接期必然 LNK2019
#     （锚点符号对不上）。本文件被所有层共享读取，天然满足这一点；各层配置期还会做
#     「被锁层的依赖版本记录 vs 本次解析结果」的比对，不一致直接 FATAL_ERROR。
#
# 只读与升级
#     各层（含 Application/Test 下的三个测试子工程）只读本文件，不写。
#     升级必须成套：三层 × Debug/Release 六条一起改，用工作站脚本
#     D:\agent\working_station\silver_era\升级测试锁.ps1 一次性写入当前构建树版本；
#     锁值落后于构建树时，配置期会打印「版本锁滞后」WARNING（不阻断）。
#
# 使用示例
#     set(BYJY_LOCK_EngineFoundation_Debug "e046a4cd8ff3")
#     set(BYJY_LOCK_EngineFoundation_Release "")
# =============================================================================

# ---------- EngineFoundation ----------
set(BYJY_LOCK_EngineFoundation_Debug "")
set(BYJY_LOCK_EngineFoundation_Release "")

# ---------- EngineSystem ----------
set(BYJY_LOCK_EngineSystem_Debug   "")
set(BYJY_LOCK_EngineSystem_Release "")

# ---------- EngineRenderer ----------
set(BYJY_LOCK_EngineRenderer_Debug   "")
set(BYJY_LOCK_EngineRenderer_Release "")
