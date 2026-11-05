#
# =============================================================================
# Application/Test 源文件列表（生成物，勿手改）
# -----------------------------------------------------------------------------
# 生成方式
#     由 Engine/EngineCore/cmake/生成源文件列表.cmake 在每轮配置时自动维护：把
#     file(GLOB_RECURSE) 的扫描结果与本文件比对（按路径集合），仅在集合有差异时
#     整体写回。
#
# 为什么要这个文件
#     CMakeLists 用 GLOB 收集源文件时，构建脚本里没有任何「具体文件名」。
#     Visual Studio 的文件视图改名 / 迁移操作，靠 File API 的 backtrace 找
#     「可改写的引用」；backtrace 只落在「把源文件直接加入 target 的那条命令」
#     上。本文件用 target_sources 直接列出文件名的字面量，于是 VS 的改写就会落到
#     本文件上；而本文件被 CMakeLists include，属配置依赖，被改写即触发重新配置。
#
# 维护
#     - 不要手工编辑。新增 / 删除 / 改名文件后，下一轮配置自动刷新。
#     - 路径一律相对本层根目录（Application/Test），与 VS 改写后的写法一致。
#     - 这里是这些文件的「唯一」加入点：CMakeLists 的 add_library 不再传这些源文件，
#       否则 backtrace 会归到 add_library 上，VS 就又无法改写了。
# =============================================================================
#
target_sources(TestLauncher PRIVATE
)

# 头文件不参与编译，只为让 IDE 建立文件索引；同样经 target_sources 列出，
# 使头文件改名 / 迁移时 VS 也能改写引用
target_sources(TestLauncher PRIVATE
    "src/主调/gtest输出镜像.h"
    "src/主调/图形操作界面.h"
    "src/主调/引擎日志屏蔽.h"
    "src/主调/控制台选择菜单.h"
    "src/主调/测试界面.h"
    "src/主调/测试运行器.h"
    "src/主调/测试运行记录.h"
    "src/主调/测试选择模型.h"
    "src/主调/退出等待判定.h"
)
