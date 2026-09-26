# 白银纪元 · 引擎层

引擎层是「白银纪元」四层仓库中**拓扑最底层的基础能力层**：向上提供事件、对象、空间分区、碰撞与通用工具集，自身**不包含任何具体游戏逻辑**，不感知任何上层存在。本层是独立 git 仓库，向远端 `engine` 分支推送。

---

## 一、本层在依赖链中的位置

```
引擎层  ←  系统层  ←  游戏层          测试层（横跨各层，唯一可执行文件产地）
```

- 依赖方向**单向**：`引擎层 ← 系统层 ← 游戏层`。系统层只消费引擎层，游戏层只消费系统层，反向依赖不存在。
- **测试层横跨各层**：测试层同时接入引擎层与系统层，用于承载单元测试与唯一可执行入口。
- 层与层之间**只链接下层已构建的静态库**，绝不通过 `add_subdirectory` 回退编译下层源码。
- 本层**不感知任何上层**：引擎层内部无 `系统层` / `游戏层` / `测试层` 的任何引用；上层对本层的消费方式是「源码 include 本层公共头 + 链接 `EngineCore.lib`」。

---

## 二、目录结构

```
引擎层/
├── CMakeLists.txt              # 构建 EngineCore 静态库（CMake ≥ 3.20，C++20）
├── CMakeSettings.json
├── LICENSE.txt
├── README.md
├── cmake/
│   ├── 对外接口.cmake          # 本层对外面自描述（BYJY_ENGINE_OUT_*）
│   └── ar_rsp_bom.ps1          # 归档响应文件 BOM 包装脚本（MSVC + Ninja 专用）
├── common/
│   ├── 前置头文件包含.h        # 统一预编译头：集中引入标准库与第三方库
│   └── 引擎.h                  # 引擎总入口：聚合事件系统运行包与对象系统
├── external/                   # 第三方库副本：Json / bullet3 / glfw / glm
└── src/
    ├── core/                   # 核心能力
    │   ├── event/              # 事件系统（事件、事件终端、事件中转器）
    │   ├── object/             # 对象系统（Object 基类、Object_Pool 对象池）
    │   └── spatial/            # 空间系统
    │       ├── common/         # 坐标类型与依赖库封装
    │       ├── collision/      # 碰撞系统（碰撞体、碰撞空间、碰撞代理器）
    │       └── partition/      # 空间分区（四叉树、四叉树管理器）
    └── tools/                  # 工具模块群
```

说明：`external/` 下的第三方源码（尤其 bullet3）会被 `GLOB_RECURSE` 一并编入 `EngineCore`，其内部文件不逐一列举。每个模块内部按 `xxx.h`（公共头）＋ `局部命名空间使用.h`（内部 `using` 别名）＋ `core/`（实现编译单元）组织。

---

## 三、模块清单

| 模块 | 路径 | 功能 |
| --- | --- | --- |
| 事件系统 | `src/core/event/` | `event` 结构体（发起者/目标/大类/标签/JSON 载荷）、`Event_Terminal` 事件终端（ACL 密钥权限隔离）、`Terminal_Interface` 终端接口注册表、`Event_Broker` 事件中转器（订阅-发布中枢）。模块间通信的枢纽 |
| 对象系统 | `src/core/object/` | `Object` 基类（编号与合法性标记）与 `Object_Pool<T, Key>` 对象池（受 `std::is_base_of_v<Object, T>` 约束，稳定/排序双模式） |
| 四叉树 | `src/core/spatial/partition/Quadtree/` | `Quadtree<T>` 二维四叉树模板：区块划分、单点/范围检索、按需扩大，内部几何运算走 64 位整数 |
| 四叉树管理器 | `src/core/spatial/partition/Quadtree_Manager/` | `Quadtree_Manager<T>` 多树编排：建树/卸载、相邻树三级筛选查找、四叉树合并与自适应扩大回调 |
| 碰撞体 | `src/core/spatial/collision/Collider/` | `Collider` 结构体与 `Detection_Mode` 检测方式：支持盒体/球体/胶囊/圆柱/圆锥/OBJ 网格六种形状，含位移向量与豁免标记 |
| 碰撞空间 | `src/core/spatial/collision/Collision_Region/` | `Collision_Region`（bullet3 碰撞世界后端、编号分配、空间边界）与 `Collision_Result` 碰撞结果；执行扫掠＋离散两阶段检测 |
| 碰撞代理器 | `src/core/spatial/collision/Collision_Proxy/` | `Collision_Proxy` 对外门面：管理多碰撞空间、维护「碰撞体编号 → 归属空间」映射，订阅 `Config`/`Collision` 指令并发布检测结果事件 |
| 坐标类型与依赖封装 | `src/core/spatial/common/` | `Point2<T>`/`Rect2<T>` 模板与精度别名（浮点走 ULP 容差比较）；将 bullet3 类型收敛为 `engine` 命名空间别名并提供 `Collision_Backend`；`碰撞系统运行包.h` 单点聚合 |
| 数据校验器 | `src/tools/Data_Validator/` | `Data_Validator` 字段类型/存在性校验与路径有效性校验 |
| 配置加载器 | `src/tools/Config_Loader/` | `Config_Loader` 扫描路由文件，按模块读取配置并发 `Config/Load` 事件 |
| 日志系统 | `src/tools/Logging/` | `Log` 分级（info/warn/error/debug）格式化输出，支持文件流切换 |
| 网格加载器 | `src/tools/Mesh_Loader/` | `Mesh_Loader` 解析 OBJ 网格为 `Mesh_Data`（顶点＋三角面索引），供碰撞网格形状使用 |
| 辅助算法 | `src/tools/Auxi_Algorithm/` | `binary_search`/`range_binary_search` 二分查找；`path_to_string`/`string_to_path` 中文路径转换 |
| 引擎环境 | `src/tools/Engine_Env/` | `Engine_Env` 获取可执行文件路径/目录，拼接绝对路径 |
| 计时器 | `src/tools/Timer/` | `Timer` 多任务命名计时 |
| 随机数生成器 | `src/tools/Random/` | `Random_Generator` 基于 PCG32 核心，生成全范围或无偏区间随机数 |
| 数值分配器 | `src/tools/Number_Allocator/` | `Number_Allocator` 编号分配与回收（复用池） |

---

## 四、对外接口契约

层间交接由「对外面自描述 ＋ 接入函数」两部分组成。

**1. 本层对外面：`cmake/对外接口.cmake`**

本层是拓扑最底层，没有下层可并入，故该文件只描述本层自身，按约定导出五个变量（前缀 `ENGINE`）：

| 变量 | 值 | 含义 |
| --- | --- | --- |
| `BYJY_ENGINE_OUT_LIB` | `EngineCore` | 本层静态库文件名（不含扩展名） |
| `BYJY_ENGINE_OUT_INC` | 本层根目录、`external/Json`、`external/glfw`、`external/glm`、`external`、`external/bullet3/src` | 使用本层公共头所需的包含目录 |
| `BYJY_ENGINE_OUT_DEF` | `GLFW_STATIC` | 使用本层公共头所需的编译定义 |
| `BYJY_ENGINE_OUT_LINK` | `external/glfw/glfw3.lib` | 本层对外传递的第三方库 |
| `BYJY_ENGINE_OUT_SYS` | `opengl32`、`user32`、`gdi32`、`shell32` | 本层对外传递的系统库 |

**2. 接入函数：上层各层的 `cmake/接入下层.cmake`**

`接入下层.cmake` **不属于本层**（本层无下层可接入），它位于系统层 / 测试层 / 游戏层，三份逐字节相同。上层通过其中的 `byjy_jieru_xiaceng(接口文件, 前缀, 覆盖变量)` 读取本层对外面，按**三级策略**定位已构建的 `EngineCore.lib`：

1. 覆盖变量（如 `BYJY_ENGINE_LIB_PATH`）非空且文件存在 → 直接使用；非空但不存在 → 硬失败，不静默降级。
2. 覆盖变量为空 → 扫描 `out/build/*/lib/EngineCore.lib`，多个候选取时间戳最新的一份。
3. 仍找不到 → 硬失败并给出构建本层的命令。

定位成功后建立 `IMPORTED STATIC GLOBAL` 目标 `EngineCore`，并把包含目录 / 编译定义 / 链接库挂到 `INTERFACE` 属性上逐层向上传递。**任何情况下都不回退编译本层源码。**

**3. `cmake/ar_rsp_bom.ps1`（MSVC + Ninja 专用）**

Ninja 写出的归档响应文件（`*.rsp`）为 UTF-8 无 BOM，MSVC 的 `lib.exe` 会按系统 ANSI 代码页解读，导致含中文对象名（如 bullet3 编译产物）的文件报 `LNK1181`。该脚本在调用真实归档器前为响应文件补 UTF-8 BOM（已含 BOM 或全 ASCII 的文件不改动），`CMakeLists.txt` 在 `project()` 之后替换 `CMAKE_CXX_CREATE_STATIC_LIBRARY` 接入，只对 Ninja 生成器生效。

---

## 五、构建

本层无下层依赖，是分层构建链的第一步。工具链：CMake ≥ 3.20、Ninja 生成器、C++20（禁用扩展）、MSVC 已验证。

```bash
# 配置（在本层根目录执行）
cmake -S . -B out/build/x64-Debug -G Ninja

# 构建
cmake --build out/build/x64-Debug
```

- 源文件由 `GLOB_RECURSE CONFIGURE_DEPENDS` 自动收集 `src/`、`common/`、`external/` 下的 `.cpp`（源文件增删无需手动重配）。
- 产物：`out/build/x64-Debug/lib/EngineCore.lib`（静态库，外部符号约 16342 条）。
- 编译选项（MSVC）：`/MP`（多核）、`/utf-8`（源文件 UTF-8）、`/WX-`（不将警告视为错误）。
- 本层**不再产出任何 `.exe`**：宿主机制已取消，全项目唯一可执行文件 `EngineTests.exe` 只由测试层产出。

---

## 六、当前状态与遗留

**当前状态**：本层已可用。测试层全量 338 用例（22 套件）全绿；`EngineCore.lib` 可正常构建并被上层接入。

**已知遗留项**：

- `src/core/spatial/partition/Quadtree/core/区块信息检索.hpp` 与 `src/core/event/Event_Broker/core/事件中转器.cpp` 存在既有 `warning C4715`（非 void 函数存在未覆盖返回路径），尚未处理。
- 对外世界坐标仍受 `int`（±2^31）约束：`Quadtree_Manager` 的对外检索接口使用 `Point2i` / `Rect2i`（32 位整数），超大尺寸层仅在树内部以 64 位整数承载。