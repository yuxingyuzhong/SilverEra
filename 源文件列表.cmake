#
# =============================================================================
# Engine/EngineFoundation 源文件列表（生成物，勿手改）
# -----------------------------------------------------------------------------
# 生成方式
#     由 Engine/EngineFoundation/cmake/生成源文件列表.cmake 在每轮配置时自动维护：把
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
#     - 路径一律相对本层根目录（Engine/EngineFoundation），与 VS 改写后的写法一致。
#     - 这里是这些文件的「唯一」加入点：CMakeLists 的 add_library 不再传这些源文件，
#       否则 backtrace 会归到 add_library 上，VS 就又无法改写了。
# =============================================================================
#
target_sources(EngineFoundation PRIVATE
    "src/core/config/Config_Loader/core/事件处理.cpp"
    "src/core/config/Config_Loader/core/缓存清除.cpp"
    "src/core/config/Config_Loader/core/脏标记.cpp"
    "src/core/config/Config_Loader/core/路径与文件.cpp"
    "src/core/config/Config_Loader/core/路由文件操作.cpp"
    "src/core/config/Config_Loader/core/配置加载.cpp"
    "src/core/config/bridge/Logging_Config_Bridge/core/日志配置桥接.cpp"
    "src/core/effect/core/粒子数据.cpp"
    "src/core/effect/core/粒子系统.cpp"
    "src/core/event/Event_Broker/core/事件中转器.cpp"
    "src/core/event/Event_Terminal/core/事件终端.cpp"
    "src/core/object/Object_Pool/core/对象池实例化.cpp"
    "src/core/spatial/partition/Quadtree/core/四叉树实例化.cpp"
    "src/core/spatial/partition/Quadtree_Manager/core/四叉树管理器实例化.cpp"
    "src/tools/Mesh_Loader/core/网格加载器.cpp"
    "src/tools/Timer/core/计时器.cpp"
)

# 头文件不参与编译，只为让 IDE 建立文件索引；同样经 target_sources 列出，
# 使头文件改名 / 迁移时 VS 也能改写引用
target_sources(EngineFoundation PRIVATE
    "common/前置头文件包含.h"
    "common/引擎.h"
    "src/core/config/Config_Loader/局部命名空间使用.h"
    "src/core/config/Config_Loader/配置加载器.h"
    "src/core/config/bridge/Logging_Config_Bridge/局部命名空间使用.h"
    "src/core/config/bridge/Logging_Config_Bridge/日志配置桥接.h"
    "src/core/effect/粒子数据.h"
    "src/core/effect/粒子系统.h"
    "src/core/event/Event/事件.h"
    "src/core/event/Event_Broker/事件中转器.h"
    "src/core/event/Event_Broker/局部命名空间使用.h"
    "src/core/event/Event_Terminal/事件终端.h"
    "src/core/event/Event_Terminal/局部命名空间使用.h"
    "src/core/event/Event_Terminal/终端端口.h"
    "src/core/event/事件系统运行包.h"
    "src/core/object/Object/对象.h"
    "src/core/object/Object_Pool/对象池.h"
    "src/core/object/对象系统运行包.h"
    "src/core/spatial/math/坐标类型.h"
    "src/core/spatial/partition/Quadtree/core/区块信息检索.hpp"
    "src/core/spatial/partition/Quadtree/core/导航与范围计算.hpp"
    "src/core/spatial/partition/Quadtree/core/节点创建与树扩大.hpp"
    "src/core/spatial/partition/Quadtree/core/设置与交互.hpp"
    "src/core/spatial/partition/Quadtree/函数预声明.h"
    "src/core/spatial/partition/Quadtree/四叉树.h"
    "src/core/spatial/partition/Quadtree/数据结构.h"
    "src/core/spatial/partition/Quadtree_Manager/core/区块信息检索.hpp"
    "src/core/spatial/partition/Quadtree_Manager/core/四叉树合并.hpp"
    "src/core/spatial/partition/Quadtree_Manager/core/四叉树扩大回调管理.hpp"
    "src/core/spatial/partition/Quadtree_Manager/core/四叉树智能创建.hpp"
    "src/core/spatial/partition/Quadtree_Manager/core/基础操作.hpp"
    "src/core/spatial/partition/Quadtree_Manager/core/相邻四叉树查找.hpp"
    "src/core/spatial/partition/Quadtree_Manager/core/设置与交互.hpp"
    "src/core/spatial/partition/Quadtree_Manager/函数预声明.h"
    "src/core/spatial/partition/Quadtree_Manager/四叉树管理器.h"
    "src/core/spatial/partition/Quadtree_Manager/数据结构.h"
    "src/tools/Detail/json字段可用性校验.h"
    "src/tools/Detail/package/数据校验工具.h"
    "src/tools/Detail/package/路径操作工具.h"
    "src/tools/Detail/二分查找.h"
    "src/tools/Detail/哈希混合.h"
    "src/tools/Detail/文件路径可用性校验.h"
    "src/tools/Detail/文件路径规范化.h"
    "src/tools/Detail/标准库format特化.h"
    "src/tools/Detail/路径字符串转换.h"
    "src/tools/Engine_Env/引擎环境.h"
    "src/tools/Logging/Log/日志系统.h"
    "src/tools/Logging/Node_Cache/节点缓存.h"
    "src/tools/Logging/Rule/日志规则.h"
    "src/tools/Logging/Stream_Sink/输出目标.h"
    "src/tools/Logging/Stream_Tree/流节点树.h"
    "src/tools/Logging/日志系统运行包.h"
    "src/tools/Mesh_Loader/局部命名空间使用.h"
    "src/tools/Mesh_Loader/网格加载器.h"
    "src/tools/Number_Allocator/数值分配器.h"
    "src/tools/Random_Generator/随机数生成器.h"
    "src/tools/Timer/局部命名空间使用.h"
    "src/tools/Timer/计时器.h"
)
