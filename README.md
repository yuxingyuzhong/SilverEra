# 白银纪元

一个用 **C++20** 从零编写的游戏引擎工程。源码按依赖方向分成四层，**每一层都是一个独立的 git 仓库**，分别向本仓库推送自己的分支。

本文件是 `main` 分支的 README。`main` 是本仓库的**总览与代码快照线**：它自己不含独立开发，只负责把四个层分支交代清楚，并把四层源码各收一份快照。因此下面把**每个分支**都完整写一遍——包括各分支的推送源、对应关系、模块清单、对外接口、构建方式与当前状态。

只想了解某一层时，直接进那个目录看它的 `README.md` 即可，那里有该层更细的模块实现说明。

---

## 一、整体结构

```
白银纪元/
├── 引擎层/      最底层基础能力（事件 / 对象 / 空间分区 / 碰撞 / 工具集）
├── 系统层/      运行时系统（实体 / 属性 / 效应）
├── 测试层/      单元测试与测试模块选择器（全项目唯一可执行文件产地）
└── 游戏层/      游戏内容与资源（当前为骨架）
```

依赖方向**单向**，上层只消费下层**已经构建好的静态库**：

```
引擎层 ──▶ 系统层 ──▶ 游戏层
   EngineCore.lib   SystemCore.lib
        ▲                ▲
        └──── 测试层 ─────┘   （横跨各层，产出 EngineTests.exe）
```

三条硬约定：

1. **上层只链接下层已构建的 `.lib`**，绝不会通过 `add_subdirectory` 把下层源码拉进本层构建树重复编译，也没有任何源码回退路径。
2. 层与层之间的交接由两份 CMake 脚本描述：`cmake/对外接口.cmake`（本层对外面自描述）＋ `cmake/接入下层.cmake`（`byjy_jieru_xiaceng()` 把下层已构建静态库接进本层）。
3. **全项目唯一的可执行文件是 `EngineTests.exe`**，由测试层产出；各层不再各自维护运行入口（旧的「宿主机制」已取消）。

## 二、分支总览

| 分支 | 推送源 | 远端分支 | 本地分支名 | 职责 | 产物 | 状态 |
| --- | --- | --- | --- | --- | --- | --- |
| `main` | 顶层仓库（本目录） | `main` | `main` | 顶层总览 ＋ 四层源码快照 | 无 | 只作合并线 |
| `engine` | `引擎层/` | `engine` | `main` | 事件、对象、空间分区、碰撞、工具集 | `EngineCore.lib` | 可用 |
| `system` | `系统层/` | `system` | `main` | 实体、属性、效应运行时系统 | `SystemCore.lib` | 可构建 |
| `test` | `测试层/` | `test` | `test` | 单元测试体系 ＋ 测试模块选择器 | `EngineTests.exe` | 22 套件 / 338 用例全绿 |
| `game` | `游戏层/` | `game` | `main` | 游戏内容与资源 | 无（当前无源码） | 骨架 |
| `doc` | 工作站（工作区之外） | `doc` | `main` | 工作站文档：任务报告 ＋ 项目导航 | 无 | 文档索引 |

两点说明：

- **四层的远端名统一为 `sliverera`，顶层为 `origin`**，指向的是同一个 GitHub 仓库，只是各自推不同分支。
- **引擎层 / 系统层 / 游戏层的本地分支名都是 `main`，与远端分支名（`engine` / `system` / `game`）不一致**，推送时必须写显式 refspec（例如 `git push sliverera main:engine`）。只有测试层的本地与远端同名（`test`）。

---

## 三、各分支详解

### 3.1 `engine` —— 引擎层

| 项目 | 内容 |
| --- | --- |
| 推送源 | `引擎层/`（独立 git 仓库） |
| 远端分支 | `engine`（本地分支名 `main`，推送需 `git push sliverera main:engine`） |
| 依赖链位置 | 拓扑最底层，**无下层依赖**，不感知任何上层 |
| 产物 | `引擎层/out/build/<配置>/lib/EngineCore.lib` |
| 对外面前缀 | `BYJY_ENGINE_*` |
| 许可证 | 本层独立 `LICENSE.txt`（版权年份与署名仍为占位符，发布前需补齐） |

**职责**：提供与具体游戏无关的基础能力——事件机制、对象与对象池、空间分区（四叉树）、碰撞检测、以及一批通用工具模块。本层不含任何游戏逻辑。

**模块清单**：

| 模块 | 路径 | 说明 |
| --- | --- | --- |
| 事件系统 | `src/core/event/` | `event` 结构体、事件终端 `Event_Terminal`、终端接口 `Terminal_Interface`、事件中转器 `Event_Broker` |
| 对象系统 | `src/core/object/` | 对象基类 `Object`、对象池 `Object_Pool` |
| 坐标类型与依赖封装 | `src/core/spatial/common/` | 模板坐标类型 `Point2i` / `Point2d` / `Point2l`、`Rect2i` / `Rect2d` / `Rect2l`；第三方依赖的封装头 |
| 四叉树 | `src/core/spatial/partition/Quadtree/` | 空间分区本体：建树、插入、区域检索、合并 |
| 四叉树管理器 | `src/core/spatial/partition/Quadtree_Manager/` | 多棵四叉树的组织、智能创建、范围计算与扩大回调 |
| 碰撞体 | `src/core/spatial/collision/Collider/` | 碰撞形状（含网格形状） |
| 碰撞空间 | `src/core/spatial/collision/Collision_Region/` | 碰撞空间容器 |
| 碰撞代理器 | `src/core/spatial/collision/Collision_Proxy/` | 碰撞检测调度入口 |
| 工具模块群 | `src/tools/` | 见下表，共 9 个模块 |

`src/tools/` 下的 9 个工具模块：

| 模块 | 头文件 | 职责 |
| --- | --- | --- |
| `Data_Validator`（数据校验器） | `src/tools/Data_Validator/数据校验器.h` | JSON 字段存在性与类型校验、路径有效性校验 |
| `Config_Loader`（配置加载器） | `src/tools/Config_Loader/配置加载器.h` | 扫描路由目录读取配置并广播 `Config/Load` 事件 |
| `Logging`（日志系统） | `src/tools/Logging/日志系统.h` | 分级日志格式化输出，活跃流可切换 |
| `Mesh_Loader`（网格加载器） | `src/tools/Mesh_Loader/网格加载器.h` | 解析 OBJ 为 `Mesh_Data`，供碰撞网格形状使用 |
| `Auxi_Algorithm`（辅助算法） | `src/tools/Auxi_Algorithm/二分查找.h`、`路径字符串转换.h` | 容器二分/区间查找；中文路径与字符串互转 |
| `Engine_Env`（引擎环境） | `src/tools/Engine_Env/引擎环境.h` | 获取可执行文件路径/目录，拼接绝对路径 |
| `Timer`（计时器） | `src/tools/Timer/计时器.h` | 多任务命名计时 |
| `Random`（随机数生成器） | `src/tools/Random/随机数生成器.h` | 基于 PCG32 的全范围/无偏区间随机数 |
| `Number_Allocator`（数值分配器） | `src/tools/Number_Allocator/数值分配器.h` | 编号分配与回收（复用池） |

**对外接口**（`引擎层/cmake/对外接口.cmake` 导出的五个变量，前缀 `ENGINE`）：

| 变量 | 值 |
| --- | --- |
| `BYJY_ENGINE_OUT_LIB` | `EngineCore` |
| `BYJY_ENGINE_OUT_INC` | 本层根目录、`external/Json`、`external/glfw`、`external/glm`、`external`、`external/bullet3/src`（共 6 条） |
| `BYJY_ENGINE_OUT_DEF` | `GLFW_STATIC` |
| `BYJY_ENGINE_OUT_LINK` | `external/glfw/glfw3.lib` |
| `BYJY_ENGINE_OUT_SYS` | `opengl32`、`user32`、`gdi32`、`shell32` |

**构建**（本层是分层构建链的第一步，无下层依赖）：

```
cmake -S 引擎层 -B 引擎层/out/build/x64-Debug -G Ninja
cmake --build 引擎层/out/build/x64-Debug
```

**当前状态**：可用。测试层对本层的全量用例全绿。遗留：两处既有 `warning C4715` 未处理；对外暴露的世界坐标仍受 `int`（±2^31）约束，超大尺寸只在四叉树内部以 64 位承载。

> 详见 [引擎层/README.md](引擎层/README.md)

### 3.2 `system` —— 系统层

| 项目 | 内容 |
| --- | --- |
| 推送源 | `系统层/`（独立 git 仓库） |
| 远端分支 | `system`（本地分支名 `main`，推送需 `git push sliverera main:system`） |
| 依赖链位置 | 引擎层之上、游戏层之下（`引擎层 ──▶ 系统层 ──▶ 游戏层`） |
| 产物 | `系统层/out/build/<配置>/lib/SystemCore.lib` |
| 对外面前缀 | `BYJY_SYSTEM_*` |
| 许可证 | 本层无独立 LICENSE 文件 |

**职责**：在引擎层提供的基础能力之上，构建一套运行时系统——实体（Entity）、属性槽（Prop）、效应（Effect）及其管理器。游戏层的配置与脚本最终由本层消费。

**模块清单**：

| 模块 | 路径 | 说明 |
| --- | --- | --- |
| 实体 | `src/entity/Entity/` | 实体本体 |
| 实体管理器 | `src/entity/Entity_Manager/` | 实体构建、ID 分配、事件驱动装配 |
| 属性槽 | `src/prop/Prop/` | 属性槽数据单元 |
| 属性槽分发器 | `src/prop/Prop_Distributor/` | 按配置把属性值分发到实体 |
| 效应 | `src/effect/Effect/` | 效应单元（可带优先级） |
| 效应管理器 | `src/effect/Effect_Manager/` | 效应登记、排序、分组查找与生效 |
| 配置编辑器（GUI） | `src/gui/Config_Editor/` | 图形化配置编辑工具，含独立 `main()`，**被 CMake 排除、未接入构建**（`core/` 下 18 个 `.cpp` ＋ 2 个 `.py`） |

**构建**（先构建引擎层，再构建本层）：

```
cmake -S 系统层 -B 系统层/out/build/x64-Debug -G Ninja
cmake --build 系统层/out/build/x64-Debug
```

配置期会自动定位引擎层已产出的 `EngineCore.lib`，找不到即**硬失败**（契约不允许回退编译引擎层源码）。

**当前状态**：已接入层间静态库契约，可独立配置并构建。遗留：`src/entity/Entity_Manager/实体管理器.h`、`src/prop/Prop_Distributor/属性槽分发器.h`、`src/effect/Effect/效应.h`、`src/effect/Effect_Manager/效应管理器.h` 四个头文件仍按引擎层**旧路径** `src/tools/Config_Checker/配置检查器.h` 引用（引擎层该模块已更名 `Data_Validator`），待同步改名。

> 详见 [系统层/README.md](系统层/README.md)

### 3.3 `test` —— 测试层

| 项目 | 内容 |
| --- | --- |
| 推送源 | `测试层/`（独立 git 仓库） |
| 远端分支 | `test`（本地分支名 `test`，与远端同名） |
| 依赖链位置 | 横跨各层：同时链接引擎层与系统层的静态库 |
| 产物 | `测试层/EngineTests.exe`（全项目唯一可执行文件） |
| 许可证 | 本层无独立 LICENSE 文件 |

**职责**：承载单元测试体系与「测试模块选择器」。它是唯一产出可执行文件的层，因此也是本项目功能验证的落脚点。

**目录内容**：

| 目录 | 内容 |
| --- | --- |
| `src/主调/` | 主程序、测试选择模型、控制台选择菜单、图形选择窗口（Dear ImGui）、退出等待判定 |
| `src/单元测试/` | 用例源文件，按被测层分「主调 / 工具 / 核心」三棵子树（共 20 个用例源文件） |
| `external/` | googletest、Dear ImGui、glad 的源码副本，本层自行编译 |
| `assets/` | 指向 `游戏层/assets` 的**目录联接**，保证资产单一真源（不入版本管理） |

**构建目标**：`TestCore`（STATIC）、`TestGui`（STATIC）、`TestLauncher`（STATIC）、`EngineTestObjects`（**OBJECT**）、`EngineTests`（EXECUTABLE）。`EngineTestObjects` 特意用 OBJECT 库，防止未被引用的 `.obj` 被丢弃而导致 `TEST_F` 的静态注册失效。

**测试规模**：**22 个套件 / 338 个用例 / 0 失败 / 0 禁用**（主调 2 套件 24 例、工具 9 套件 120 例、核心 11 套件 194 例）。

**构建与运行**（先构建引擎层与系统层）：

```
cmake -S 测试层 -B 测试层/out/build/x64-Debug -G Ninja
cmake --build 测试层/out/build/x64-Debug

# 产物：测试层/EngineTests.exe
EngineTests.exe                       # 默认 --selector=window：图形窗口勾选测试模块
EngineTests.exe --selector=console    # 控制台编号菜单（支持 1,3-5、all）
EngineTests.exe --selector=off        # 不开窗，全量运行（ctest 走这条）
EngineTests.exe --gtest_filter=...    # 已带 gtest 参数时直接透传，不弹窗
```

**当前状态**：全量 338 / 338 通过，`ctest` 1/1 通过。遗留：图形窗口的鼠标点选交互尚未自动化，回归只覆盖「窗口创建失败降级」路径。

> 详见 [测试层/README.md](测试层/README.md)

### 3.4 `game` —— 游戏层

| 项目 | 内容 |
| --- | --- |
| 推送源 | `游戏层/`（独立 git 仓库） |
| 远端分支 | `game`（本地分支名 `main`，推送需 `git push sliverera main:game`） |
| 依赖链位置 | 依赖链顶端（`引擎层 ──▶ 系统层 ──▶ 游戏层`） |
| 产物 | 无（`src/` 为空，不产出静态库） |
| 对外面前缀 | `BYJY_GAME_*` |
| 许可证 | 本层无独立 LICENSE 文件 |

**职责**：依赖链顶端的游戏内容层。**当前为骨架**：`src/` 与 `external/` 均为空，只保留层间契约骨架与 `assets/` 资产数据。

**资产内容**：

| 路径 | 内容 |
| --- | --- |
| `assets/config/entities/` | 实体配置 7 份（`au.json` 与 6 个怪物：史莱姆、史莱姆王、哥布林、哥布林祭司、牛头人、牛头人战士） |
| `assets/config/property/` | 属性配置 7 份（与实体同名） |
| `assets/config/format/` | 格式定义 2 份（`Entity_Manager.json`、`Property_Manager.json`） |
| `assets/config/route/` | 路由表 2 份（`entity.json`、`property.json`） |
| `assets/scripts/initialize/` | 实体初始化脚本（Lua） |
| `assets/scripts/behavior/` | 实体行为（决策树）脚本（Lua） |
| `cmake/` | `对外接口.cmake`、`接入下层.cmake`，与其余三层一致的契约脚本 |

**构建**（先构建系统层）：

```
cmake -S 游戏层 -B 游戏层/out/build/x64-Debug -G Ninja
```

当前无源码，配置可通过，但**不生成任何静态库**——`GameCore` 是条件目标，要等 `src/` 下出现第一份 `.cpp` / `.c` 才会建立。本层不产出可执行文件。

**历史说明**：本层过去挂着一个「组合根可执行文件」，另有一个把测试层宿主并入本层构建树的开关。两者都已移除——组合根会把下层源码拉进本层重复编译，与「层间只链接已构建静态库」的契约冲突；现在可执行文件只由测试层产出。

> 详见 [游戏层/README.md](游戏层/README.md)

### 3.5 `doc` —— 工作站文档

| 项目 | 内容 |
| --- | --- |
| 推送源 | 工作站（**位于工作区之外**，不随项目分发） |
| 远端分支 | `doc` |
| 内容 | 工作站文档，**不含项目源码** |
| 许可证 | 随顶层 |

**目录内容**：

- `任务报告合集/`：每项任务的执行计划与执行报告。
- `项目导航合集/`：每个代码版次的项目架构与模块架构文档（`项目架构.md` ＋ `模块架构/`）。

文档按「每分支一目录、每版次一目录」隔离存储，版次目录以**代码推送哈希值**命名（不含日期）。哈希是内容寻址的，与时间没有对应关系，而文件系统与 GitHub 网页都按目录名的 Unicode 码位序排列——所以**目录列表本身看不出先后，最新的一份往往夹在中间**。

要看版次顺序，请读该分支根目录的 `README.md`（以及各合集下的 `README.md`），它们都按时间倒序维护，最新的在最上面。

---

## 四、层间契约

这是本项目最容易被误解的一部分，单独说明。

每层各维护一份 `cmake/对外接口.cmake`，声明「本层对外面 = 本层自有面 ＋ 下层对外面」，按固定约定导出五个变量：

| 变量后缀 | 含义 |
| --- | --- |
| `OUT_LIB` | 本层静态库文件名（不含扩展名） |
| `OUT_INC` | 使用本层公共头所需的包含目录 |
| `OUT_DEF` | 使用本层公共头所需的编译定义 |
| `OUT_LINK` | 本层对外传递的第三方库 |
| `OUT_SYS` | 本层对外传递的系统库 |

上层通过 `cmake/接入下层.cmake` 中的 `byjy_jieru_xiaceng(接口文件, 前缀, 覆盖变量)` 读取下层的这五个变量，并以**三级策略**定位下层已构建的静态库：

1. **覆盖变量优先**：形如 `BYJY_ENGINE_LIB_PATH` 的缓存变量非空且文件存在 → 直接使用；**非空但文件不存在 → 硬失败**，不静默降级。
2. **自动探测**：覆盖变量为空 → 扫描 `<下层>/out/build/*/lib/<库名>.lib`，多个候选取时间戳最新的一份。
3. **硬失败**：仍找不到 → 报错并给出构建下层的命令。

定位成功后建立 `IMPORTED STATIC GLOBAL` 目标，把包含目录 / 编译定义 / 链接库与系统库挂到 `INTERFACE` 属性上逐层向上传递。**任何情况下都不回退编译下层源码。**

`cmake/接入下层.cmake` 位于系统层 / 测试层 / 游戏层，三份**逐字节相同**；引擎层无下层可接入，故不含该文件。

## 五、构建

**工具链要求**：CMake ≥ 3.20、Ninja 生成器、支持 C++20 的编译器（当前只在 **MSVC / x64 / Windows** 上验证过）。命令需在已激活的 Visual Studio 开发人员环境中执行（需要 `cl.exe`）。

**必须逐层构建**，顺序不能颠倒——每一层在配置期就会去定位下层**已构建**的静态库，找不到会直接硬失败（这是刻意的）。

```
# 1. 引擎层（无下层依赖）
cmake -S 引擎层 -B 引擎层/out/build/x64-Debug -G Ninja
cmake --build 引擎层/out/build/x64-Debug

# 2. 系统层（定位引擎层已产出的 EngineCore.lib）
cmake -S 系统层 -B 系统层/out/build/x64-Debug -G Ninja
cmake --build 系统层/out/build/x64-Debug

# 3. 测试层（定位 EngineCore.lib 与 SystemCore.lib，产出 EngineTests.exe）
cmake -S 测试层 -B 测试层/out/build/x64-Debug -G Ninja
cmake --build 测试层/out/build/x64-Debug

# 4. 跑测试
测试层/EngineTests.exe --selector=off
```

库路径可用覆盖变量显式指定，跳过自动探测：

```
-DBYJY_ENGINE_LIB_PATH=<EngineCore.lib 路径>
-DBYJY_SYSTEM_LIB_PATH=<SystemCore.lib 路径>
```

**关于顶层 `CMakeLists.txt`**：顶层仍保留一份把 `引擎层` / `系统层` / `游戏层` `add_subdirectory` 进同一构建树的编排脚本，但它**不包含测试层、不产出任何可执行文件**，而且「同一构建树统一编排」与现行的「层间只链接已构建静态库」契约是相冲的（各层在配置阶段就会去定位下层的 `.lib`）。**请按上面的逐层顺序构建，不要依赖顶层一次性构建。**

**在 Visual Studio 中打开**：每一层都有自己的 `CMakeSettings.json`（Ninja / x64-Debug）。用 VS 的「打开本地文件夹」选中**层目录**即可配置构建；顶层也有一份 `CMakeSettings.json`，但同上，建议逐层打开。

**关于中文源码文件名**：本层源码文件名含中文，而 Ninja 写出的归档响应文件（`*.rsp`）是无 BOM 的 UTF-8，MSVC 的 `lib.exe` 会按系统 ANSI 代码页解读导致乱码并报 `LNK1181`。引擎层的 `cmake/ar_rsp_bom.ps1` 在调用真实归档器前为响应文件补 UTF-8 BOM 解决该问题（注意 `CMAKE_CXX_CREATE_STATIC_LIBRARY` 必须在 `project()` 之后设置，否则会被平台默认值覆盖）。

## 六、各层文档索引

| 层 | 文档 | 覆盖内容 |
| --- | --- | --- |
| 引擎层 | [引擎层/README.md](引擎层/README.md) | 事件/对象/坐标/四叉树/碰撞/9 个工具模块的详解，对外接口契约，构建与已知问题 |
| 系统层 | [系统层/README.md](系统层/README.md) | 实体/属性槽/效应全链路，模块详解，Lua 集成与运行链路 |
| 测试层 | [测试层/README.md](测试层/README.md) | 主调与选择器详解，单元测试体系与规模，构建目标与运行方式 |
| 游戏层 | [游戏层/README.md](游戏层/README.md) | 资产数据详解（配置目录结构、实体与属性配置、格式定义与路由表、行为与初始化脚本） |
| doc | doc 分支根目录的 `README.md` | 工作站文档索引（按时间倒序） |

## 七、许可

见 [LICENSE](LICENSE)（MIT，Copyright 2026 雨行雨中）。各层目录下如需独立许可证，以其自身的 `LICENSE` / `LICENSE.txt` 为准（当前仅引擎层有 `LICENSE.txt`）。