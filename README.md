# 白银纪元

一个用 **C++20** 从零编写的游戏引擎工程。源码按依赖方向分成若干层，**每一层都是一个独立的 git 仓库**，分别向本仓库推送自己的分支。

本文件是 `main` 分支的 README。`main` 是本仓库的**总览线**：它自己不含独立开发，只负责把各层分支交代清楚。

四层各自是**独立 git 仓库**（层目录内含自身 `.git`）。git 不会把「含自身 `.git` 的目录」内的文件收录进顶层仓库，因此**顶层 `main` 不收录各层源码**——各层源码由 `engine` / `system` / `test` / `game` 四个分支承载；顶层只保留总览、许可、忽略规则，以及 Engine 聚合层（`Engine/`，无嵌套 `.git`）的构建脚本。因此下面把**每个分支**都完整写一遍——包括各分支的推送源、对应关系、模块清单、对外接口、构建方式与当前状态。

只想了解某一层时，直接进那个目录看它的 `README.md` 即可。

---

## 一、整体结构

```
白银纪元/
├── Engine/                    引擎聚合层（不编源码，只聚合下面两个子层对外面）
│   ├── EngineCore/            引擎核心层：事件 / 对象 / 空间分区 / 碰撞 / 工具集
│   └── EngineSystem/          引擎系统层：实体 / 属性 / 效应
└── Application/               应用层目录
    ├── Test/                  测试层与测试模块选择器（全项目唯一可执行文件产地）
    └── Game/                  游戏内容与资源（当前为骨架）
```

依赖方向**单向**，上层只消费下层**已经构建好的静态库**：

```
EngineCore ──▶ EngineSystem ──▶ Engine（聚合） ──▶ Application/Game
  EngineCore.lib  EngineSystem.lib   （无产物）        GameCore.lib（条件产出）
                                    └────────────▶ Application/Test
                                                    EngineTests.exe
```

四条硬约定：

1. **上层只链接下层已构建的 `.lib`**，绝不通过 `add_subdirectory` 把下层源码拉进本层构建树重复编译，也没有任何源码回退路径。
2. 层与层之间的交接由两份 CMake 脚本描述：`cmake/对外接口.cmake`（本层对外面自描述）＋ `cmake/接入下层.cmake`（`byjy_jieru_xiaceng()` 把下层已构建静态库接进本层）。工程内部**一律写「项目根相对全路径」包含**（`Engine/EngineCore/...`、`Engine/EngineSystem/...`、`Application/Test/...`），消除「直接 `src/...` 包含」带来的路径归属不明与文件重名。
3. **全项目唯一的可执行文件是 `EngineTests.exe`**，由 `Application/Test` 产出；各层不再各自维护运行入口。
4. **顶层没有构建脚本**：过去的顶层 `CMakeLists.txt`（把各层 `add_subdirectory` 进同一构建树）已删除——它与「层间只链接已构建静态库」根本冲突。构建一律**逐层独立进行**。

## 二、分支总览

| 分支 | 推送源 | 远端分支 | 本地分支名 | 职责 | 产物 | 状态 |
| --- | --- | --- | --- | --- | --- | --- |
| `main` | 顶层仓库（本目录） | `main` | `main` | 顶层总览（**不收录各层源码**，见开头说明） | 无 | 只作总览线 |
| `engine` | `Engine/EngineCore/` | `engine` | `main` | 事件、对象、空间分区、碰撞、工具集 | `EngineCore.lib` | 可用 |
| `system` | `Engine/EngineSystem/` | `system` | `main` | 实体、属性、效应运行时系统 | `EngineSystem.lib` | 可构建 |
| `test` | `Application/Test/` | `test` | `test` | 单元测试体系 ＋ 测试模块选择器 | `EngineTests.exe` | 27 套件 / 625 用例 |
| `game` | `Application/Game/` | `game` | `main` | 游戏内容与资源 | 无（当前无源码） | 骨架 |
| `doc` | 工作站（工作区之外） | `doc` | `main` | 工作站文档：任务报告 ＋ 项目导航 | 无 | 文档索引 |

两点说明：

- **各层的远端名统一为 `sliverera`，顶层为 `origin`**，指向的是同一个 GitHub 仓库，只是各自推不同分支。
- **`EngineCore` / `EngineSystem` / `Game` 的本地分支名都是 `main`，与远端分支名（`engine` / `system` / `game`）不一致**，推送时必须写显式 refspec（例如 `git push sliverera main:engine`）。只有测试层的本地与远端同名（`test`）。

> `Engine` 聚合层自身没有独立远端分支：它只提供 `cmake/对外接口.cmake` 与构建脚本，源码随顶层快照入库。

---

## 三、各层详解

### 3.1 `engine` —— EngineCore（引擎核心层）

| 项目 | 内容 |
| --- | --- |
| 目录 | `Engine/EngineCore/`（独立 git 仓库） |
| 依赖链位置 | 拓扑最底层，**无下层依赖**，不感知任何上层 |
| 产物 | `Engine/EngineCore/out/build/<配置>/lib/EngineCore.lib` |
| 对外面前缀 | `BYJY_ENGINE_CORE_*` |
| 许可证 | 本层独立 `LICENSE.txt`（版权年份与署名仍为占位符，发布前需补齐） |

**职责**：提供与具体游戏无关的基础能力——事件机制、对象与对象池、空间分区（四叉树）、碰撞检测、以及一批通用工具模块。

**模块清单**：

| 模块 | 路径（相对项目根） | 说明 |
| --- | --- | --- |
| 事件系统 | `Engine/EngineCore/src/core/event/` | `Event` 结构体、事件终端 `Event_Terminal`、终端接口 `Terminal_Interface`、事件中转器 `Event_Broker` |
| 对象系统 | `Engine/EngineCore/src/core/object/` | 对象基类 `Object`、对象池 `Object_Pool` |
| 坐标类型与依赖封装 | `Engine/EngineCore/src/core/spatial/common/` | 模板坐标类型 `Point2i`/`Point2d`/`Point2l`、`Rect2i`/`Rect2d`/`Rect2l`；第三方依赖封装头 |
| 四叉树 | `Engine/EngineCore/src/core/spatial/partition/Quadtree/` | 空间分区本体：建树、插入、区域检索、合并 |
| 四叉树管理器 | `Engine/EngineCore/src/core/spatial/partition/Quadtree_Manager/` | 多棵四叉树的组织、智能创建、范围计算与扩大回调 |
| 碰撞体 / 碰撞空间 / 碰撞代理器 | `Engine/EngineCore/src/core/spatial/collision/` | 碰撞形状、碰撞空间容器、碰撞检测调度入口 |
| 配置加载 | `Engine/EngineCore/src/core/config/Config_Loader/` | 配置加载器：可信根/路由目录扫描、脏标记缓存回写、`Config/Load` 事件广播 |
| 工具模块群 | `Engine/EngineCore/src/tools/` | 见下表 |

`src/tools/` 下的工具模块：

| 模块 | 头文件 | 职责 |
| --- | --- | --- |
| `Detail` | `src/tools/Detail/*.h`、`Detail/package/*.h` | 容器二分/区间查找；中文路径与字符串互转；哈希混合；JSON 字段与文件路径可用性校验；`format` 特化 |
| `Logging` | `src/tools/Logging/日志系统.h` | 日志系统拆 `Log` / `Stream_Tree` / `Stream_Sink` / `Node_Cache` 四类 ＋ 运行包，按 `logger.` 实例语义调用 |
| `Mesh_Loader` | `src/tools/Mesh_Loader/网格加载器.h` | 解析 OBJ 为 `Mesh_Data`，供碰撞网格形状使用 |
| `Engine_Env` | `src/tools/Engine_Env/引擎环境.h` | 获取可执行文件路径/目录，拼接绝对路径；逻辑帧计数 |
| `Timer` | `src/tools/Timer/计时器.h` | 多任务命名计时 |
| `Random_Generator` | `src/tools/Random_Generator/随机数生成器.h` | 基于 PCG32 的全范围/无偏区间随机数（模板类） |
| `Number_Allocator` | `src/tools/Number_Allocator/数值分配器.h` | 编号分配与回收（复用池） |

**构建**（分层构建链的第一步，无下层依赖）：

```
cmake -S Engine/EngineCore -B Engine/EngineCore/out/build/x64-Debug -G Ninja
cmake --build Engine/EngineCore/out/build/x64-Debug
```

### 3.2 `system` —— EngineSystem（引擎系统层）

| 项目 | 内容 |
| --- | --- |
| 目录 | `Engine/EngineSystem/`（独立 git 仓库） |
| 依赖链位置 | EngineCore 之上 |
| 产物 | `Engine/EngineSystem/out/build/<配置>/lib/EngineSystem.lib` |
| 对外面前缀 | `BYJY_ENGINE_SYSTEM_*` |
| 许可证 | 本层无独立 LICENSE 文件 |

**职责**：在 EngineCore 提供的基础能力之上，构建一套运行时系统——实体（Entity）、属性槽（Prop）、效应（Effect）及其管理器。游戏层的配置与脚本最终由本层消费。

**模块清单**：

| 模块 | 路径（相对项目根） | 说明 |
| --- | --- | --- |
| 实体 / 实体管理器 | `Engine/EngineSystem/src/entity/` | 实体本体；实体构建、ID 分配、事件驱动装配 |
| 属性槽 / 属性槽分发器 | `Engine/EngineSystem/src/prop/` | 属性槽数据单元；按配置把属性值分发到实体 |
| 效应 / 效应管理器 | `Engine/EngineSystem/src/effect/` | 效应单元（可带优先级）；效应登记、排序、分组查找与生效 |
| 配置编辑器（GUI） | `Engine/EngineSystem/src/gui/Config_Editor/` | 图形化配置编辑工具，含独立 `main()`，**被 CMake 排除、未接入构建** |

**构建**（先构建 EngineCore）：

```
cmake -S Engine/EngineSystem -B Engine/EngineSystem/out/build/x64-Debug -G Ninja
cmake --build Engine/EngineSystem/out/build/x64-Debug
```

配置期自动定位 EngineCore 已产出的 `EngineCore.lib`，找不到即**硬失败**。

### 3.3 `Engine` —— 引擎聚合层

| 项目 | 内容 |
| --- | --- |
| 目录 | `Engine/`（随顶层入库；无独立远端分支） |
| 依赖链位置 | EngineSystem 之上，是 Application 各层唯一的下层 |
| 产物 | **无**（不编源码、不产出静态库，也不合并出 `Engine.lib`） |
| 对外面前缀 | `BYJY_ENGINE_*` |

**职责**：把 `EngineSystem` 与 `EngineCore` 两层的能力合并成「一个 Engine 面」，让 `Application` 各层只接一层。其 `cmake/对外接口.cmake` 转发两个库：

| 变量 | 值 |
| --- | --- |
| `BYJY_ENGINE_OUT_LIB` | `EngineSystem;EngineCore`（分号列表，聚合转发） |
| `BYJY_ENGINE_OUT_LIBDIR` | 与上表一一对应的两个子层目录（供自动探测 `.lib`） |
| `BYJY_ENGINE_OUT_INC` | 项目根 ＋ Sol2 / Lua / Json / glfw / bullet3 等第三方目录 |
| `BYJY_ENGINE_OUT_DEF` / `_LINK` / `_SYS` | 继承两层 |

**构建**（自检式：校验两个子层库齐备）：

```
cmake -S Engine -B Engine/out/build/x64-Debug -G Ninja
cmake --build Engine/out/build/x64-Debug
```

### 3.4 `test` —— 测试层

| 项目 | 内容 |
| --- | --- |
| 目录 | `Application/Test/`（独立 git 仓库） |
| 依赖链位置 | 顶端：只接 `Engine` 聚合层 |
| 产物 | `Application/Test/EngineTests.exe`（全项目唯一可执行文件） |
| 许可证 | 本层无独立 LICENSE 文件 |

**目录内容**：

| 目录 | 内容 |
| --- | --- |
| `src/主调/` | 主程序、测试选择模型、控制台选择菜单、图形选择窗口（Dear ImGui）、退出等待判定、引擎日志屏蔽 |
| `src/单元测试/` | 用例源文件，按被测层分「主调 / 工具 / 核心」三棵子树 |
| `external/` | googletest、Dear ImGui、glad 的源码副本，本层自行编译 |
| `assets/` | 指向 `Application/Game/assets` 的目录联接，保证资产单一真源（不入版本管理） |

**构建目标**：`TestCore`（STATIC）、`TestGui`（STATIC）、`TestLauncher`（STATIC）、`EngineTestObjects`（**OBJECT**）、`EngineTests`（EXECUTABLE）。`EngineTestObjects` 特意用 OBJECT 库，防止未被引用的 `.obj` 被丢弃而导致 `TEST_F` 的静态注册失效。

**当前规模**：**27 个套件 / 625 个用例 / 619 通过 / 2 跳过 / 4 失败**；4 项失败集中在 `Config_Loader_Test`（可信根事件链路，属在建功能）。

**构建与运行**（先构建 EngineCore → EngineSystem）：

```
cmake -S Application/Test -B Application/Test/out/build/x64-Debug -G Ninja
cmake --build Application/Test/out/build/x64-Debug

# 产物：Application/Test/EngineTests.exe
EngineTests.exe                       # 默认 --selector=window：图形窗口勾选测试模块
EngineTests.exe --selector=console    # 控制台编号菜单（支持 1,3-5、all）
EngineTests.exe --selector=off        # 不开窗，全量运行（ctest 走这条）
EngineTests.exe --gtest_filter=...    # 已带 gtest 参数时直接透传，不弹窗
```

### 3.5 `game` —— 游戏层

| 项目 | 内容 |
| --- | --- |
| 目录 | `Application/Game/`（独立 git 仓库） |
| 依赖链位置 | 依赖链顶端 |
| 产物 | 无（`src/` 为空，不产出静态库） |
| 对外面前缀 | `BYJY_GAME_*` |

**职责**：游戏内容层，**当前为骨架**。`assets/` 内有实体/属性/格式/路由配置与 Lua 脚本（初始化、行为决策树）。

**构建**（先构建 EngineCore → EngineSystem）：

```
cmake -S Application/Game -B Application/Game/out/build/x64-Debug -G Ninja
```

无源码时不生成 `GameCore`，配置仍可通过。

### 3.6 `doc` —— 工作站文档

| 项目 | 内容 |
| --- | --- |
| 推送源 | 工作站（**位于工作区之外**，不随项目分发） |
| 远端分支 | `doc` |
| 内容 | 工作站文档，**不含项目源码** |

- `任务报告合集/`：每项任务的执行计划与执行报告。
- `项目导航合集/`：每个代码版次的项目架构与模块架构文档（`项目架构.md` ＋ `模块架构/`）。

版次目录以**代码推送哈希值**命名（不含日期）。哈希是内容寻址的，与时间没有对应关系，而文件系统与 GitHub 网页都按目录名的 Unicode 码位序排列——所以**目录列表本身看不出先后**。要看版次顺序，请读各合集下的 `README.md`（按时间倒序维护）。

---

## 四、层间契约

这是本项目最容易被误解的一部分，单独说明。

每层各维护一份 `cmake/对外接口.cmake`，声明「本层对外面 = 本层自有面 ＋ 下层对外面」，按固定约定导出六个变量：

| 变量后缀 | 含义 |
| --- | --- |
| `OUT_LIB` | 本层对外传递的静态库名列表（分号分隔）；不产出库时留空 |
| `OUT_LIBDIR` | 与 `OUT_LIB` **一一对应**的「该库所在层目录」；可选，缺省为本层目录 |
| `OUT_INC` | 使用本层公共头所需的包含目录 |
| `OUT_DEF` | 使用本层公共头所需的编译定义 |
| `OUT_LINK` | 本层对外传递的第三方库 |
| `OUT_SYS` | 本层对外传递的系统库 |

`OUT_LIB` 支持列表是为了聚合层：`Engine` 不产出自己的库，而是把 `EngineSystem;EngineCore` 转出去，`OUT_LIBDIR` 则告诉上层这两个库分别在哪个子层目录里。

上层通过 `cmake/接入下层.cmake` 中的 `byjy_jieru_xiaceng(接口文件, 前缀, 覆盖变量)` 读取下层的这些变量，并按 `OUT_LIB` **逐库**以三级策略定位已构建的静态库：

1. **覆盖变量优先**：形如 `BYJY_ENGINE_CORE_LIB_PATH` 的缓存变量非空 → 按元素下标与 `OUT_LIB` 逐项对应（元素个数须等长）；**指向的文件不存在 → 硬失败**。
2. **自动探测**：覆盖变量为空 → 扫描 `<该库所在层目录>/out/build/*/lib/<库名>.lib`，多个候选取时间戳最新的一份。
3. **硬失败**：仍找不到 → 报错并给出构建产出该库的下层命令。

定位成功后建立 `IMPORTED STATIC GLOBAL` 目标，把包含目录 / 编译定义 / 链接库与系统库挂到 `INTERFACE` 属性上逐层向上传递。**任何情况下都不回退编译下层源码。**

`cmake/接入下层.cmake` 位于 `EngineSystem` / `Engine` / `Test` / `Game`，四份**逐字节相同**；`EngineCore` 无下层可接入，故不含该文件。

### 4.1 包含路径与「前缀解析根」

工程内部一律写**项目根相对全路径**：

| 用途 | 写法 |
| --- | --- |
| 引擎核心头 | `#include "Engine/EngineCore/src/tools/Detail/二分查找.h"` |
| 引擎系统头 | `#include "Engine/EngineSystem/src/entity/Entity/实体.h"` |
| 应用层自身头 | `#include "Application/Test/src/主调/测试选择模型.h"` |
| 第三方头 | 保持原样（`<nlohmann/json.hpp>`、`<GLFW/glfw3.h>`、`<sol/sol.hpp>`） |

对外接口的 `OUT_INC` 首项就是**项目根**，它是 `Engine/...`、`Application/...` 的「前缀解析根」。

### 4.2 头快照（同版次编译）

源码树里的项目根同时能解析到 `Engine/...` 的**源码副本**。若它与下层静态库不同版次（头已改、库未重建），就会「新头配旧库」地编译链接。为此每个产出层在构建时把**本层自有头**（`common`、`src`）镜像到自己的构建目录：

```
<层构建目录>/include/Engine/EngineCore/**      ← EngineCore 的快照
<层构建目录>/include/Engine/EngineSystem/**    ← EngineSystem 的快照
```

消费层把**下层快照根作为「自己的包含目录」列出，并排在项目根之前**：

```cmake
byjy_tou_kuaizhao_gen(BYJY_CORE_SNAPSHOT   EngineCore)     # 解析并校验快照根，缺失即硬失败
byjy_tou_kuaizhao_gen(BYJY_SYSTEM_SNAPSHOT EngineSystem)
set(BYJY_APP_INC "${BYJY_CORE_SNAPSHOT}" "${BYJY_SYSTEM_SNAPSHOT}" "${BYJY_PROJECT_ROOT}")
```

**为什么必须这样写**：MSVC 下目标自身的包含目录（`-I`）恒在导入目标传递来的 `/external:I` 之前。快照根作为自身包含目录列出，就一定先于「项目根」命中；若反过来去改写导入目标的对外面（把项目根换成快照根），项目根只剩自身包含目录一个来源、反而排到了快照之前，快照会被旁路。可用 `compile_commands.json` 复核次序：应为 **子层快照根 → 项目根 → 第三方**。

---

## 五、构建

**工具链要求**：CMake ≥ 3.20、Ninja 生成器、支持 C++20 的编译器（当前只在 **MSVC / x64 / Windows** 上验证过）。命令需在已激活的 Visual Studio 开发人员环境中执行（需要 `cl.exe`）。

**必须逐层构建**，顺序不能颠倒——每一层在配置期就会去定位下层**已构建**的静态库，找不到会直接硬失败（这是刻意的）。

```
# 1. EngineCore（无下层依赖）
cmake -S Engine/EngineCore -B Engine/EngineCore/out/build/x64-Debug -G Ninja
cmake --build Engine/EngineCore/out/build/x64-Debug

# 2. EngineSystem（定位 EngineCore.lib）
cmake -S Engine/EngineSystem -B Engine/EngineSystem/out/build/x64-Debug -G Ninja
cmake --build Engine/EngineSystem/out/build/x64-Debug

# 3. Engine 聚合层（自检两个子层库齐备）
cmake -S Engine -B Engine/out/build/x64-Debug -G Ninja
cmake --build Engine/out/build/x64-Debug

# 4. 应用层（定位 EngineSystem.lib 与 EngineCore.lib）
cmake -S Application/Test -B Application/Test/out/build/x64-Debug -G Ninja
cmake --build Application/Test/out/build/x64-Debug

# 5. 跑测试
Application/Test/EngineTests.exe --selector=off
```

库路径可用覆盖变量显式指定，跳过自动探测（聚合层按 `EngineSystem;EngineCore` 顺序给出分号列表）：

```
-DBYJY_ENGINE_CORE_LIB_PATH=<EngineCore.lib 路径>
-DBYJY_ENGINE_LIB_PATH=<EngineSystem.lib;EngineCore.lib 路径列表>
```

### 5.1 本机环境的三条硬性注意事项

1. **配置期必须处于 UTF-8 代码页**（例如先执行 `chcp 65001`，且必须在进入 VS 开发人员环境**之后**执行）。
   原因：MSVC 的 `/showIncludes` 前缀在「CMake 编译器探测」与「工程编译」两侧编码不同——探测时不带 `/utf-8`，按系统本地码页输出；工程编译带 `/utf-8`，输出 UTF-8 字节。两者逐字节不等时，ninja 匹配不到 `msvc_deps_prefix`，后果是**一条头文件依赖都不记录**（`ninja -t deps <obj>` 显示 `#deps 0`），改头文件永不触发重编。
2. **`TMP` / `TEMP` 必须指向纯 ASCII 路径**。
   本机默认临时目录位于含非 ASCII 用户名的路径下（`C:\Users\<非ASCII>\AppData\Local\Temp`），会导致 cl.exe 只要请求调试信息（`/Zi` 或 `/Z7`）就报
   `D8050 :无法执行 c1.dll: 未能将命令行放入调试记录中`，编译器探测阶段直接失败。构建前把 `TMP`/`TEMP` 指到纯 ASCII 目录即可。
3. **`chcp` 与 VS 开发环境的先后顺序**：先 `Enter-VsDevShell` / 开发人员命令行，**再** `chcp 65001`；反过来会让 cl.exe 报同样的 `D8050`。

### 5.2 关于中文源码文件名

源码文件名含中文，而 Ninja 写出的归档响应文件（`*.rsp`）是无 BOM 的 UTF-8，MSVC 的 `lib.exe` 会按系统 ANSI 代码页解读导致乱码并报 `LNK1181`。`Engine/EngineCore/cmake/ar_rsp_bom.ps1` 在调用真实归档器前为响应文件补 UTF-8 BOM 解决该问题（注意 `CMAKE_CXX_CREATE_STATIC_LIBRARY` 必须在 `project()` 之后设置，否则会被平台默认值覆盖）。

### 5.3 文件增删与 IDE 同步

各层的源文件用 `file(GLOB_RECURSE ... CONFIGURE_DEPENDS)` 收集，并把 **GLOB 基点目录登记进 `CMAKE_CONFIGURE_DEPENDS`**：

```cmake
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${PROJECT_ROOT_DIR}/src"
    ...)
```

于是**新增/删除文件后直接构建即可**：ninja 先输出 `Re-checking globbed directories...`，CMake 自动重跑配置并重扫 GLOB，IDE/VS 的目录视图随之更新，不需要手动「重新生成缓存」。

### 5.4 在 Visual Studio 中打开

每一层都有自己的 `CMakeSettings.json`（Ninja / x64-Debug）。用 VS 的「打开本地文件夹」选中**层目录**即可配置构建。顶层不再有 `CMakeLists.txt`，因此也没有顶层构建配置——请逐层打开。

---

## 六、各层文档索引

| 层 | 目录 | 覆盖内容 |
| --- | --- | --- |
| EngineCore | [Engine/EngineCore/README.md](Engine/EngineCore/README.md) | 事件/对象/坐标/四叉树/碰撞/工具模块详解，对外接口契约，构建与已知问题 |
| EngineSystem | [Engine/EngineSystem/README.md](Engine/EngineSystem/README.md) | 实体/属性槽/效应全链路，模块详解，Lua 集成与运行链路 |
| Engine（聚合） | 本文件 §3.3 | 聚合面与转发契约 |
| Test | [Application/Test/README.md](Application/Test/README.md) | 主调与选择器详解，单元测试体系与规模，构建目标与运行方式 |
| Game | [Application/Game/README.md](Application/Game/README.md) | 资产数据详解（配置目录结构、实体与属性配置、格式定义与路由表、行为与初始化脚本） |
| doc | doc 分支根目录的 `README.md` | 工作站文档索引（按时间倒序） |

## 七、许可

见 [LICENSE](LICENSE)（MIT，Copyright 2026 雨行雨中）。各层目录下如需独立许可证，以其自身的 `LICENSE` / `LICENSE.txt` 为准（当前仅 EngineCore 有 `LICENSE.txt`）。