# 白银纪元

一个用 **C++20** 从零编写的游戏引擎工程。源码按依赖方向分成四层，**每一层都是一个独立的 git 仓库**，分别向本仓库推送自己的分支。

本文件是 `main` 分支的 README：`main` 是本仓库的**总览与代码快照线**，所以下面把**每个分支**都完整交代一遍。

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

- 上层**只链接下层已构建的 `.lib`**，绝不会通过 `add_subdirectory` 把下层源码拉进本层构建树重复编译，也没有任何源码回退路径。
- 层与层之间的交接由两份 CMake 脚本描述：`cmake/对外接口.cmake`（本层对外面自描述）＋ `cmake/接入下层.cmake`（`byjy_jieru_xiaceng()` 把下层已构建静态库接进本层）。
- **全项目唯一的可执行文件是 `EngineTests.exe`**，由测试层产出；各层不再各自维护运行入口（「宿主机制」已取消）。

每一层内部还有一份自己的 `README.md`，写该层的目录结构、模块清单、接口契约与构建方式。只想了解某一层时，直接进那个目录看它的 README 即可。

---

## 二、分支总览

| 分支 | 推送源 | 职责 | 产物 | 状态 |
| --- | --- | --- | --- | --- |
| `main` | 顶层仓库（本目录） | 顶层总览 + 四层源码快照 | 无 | 只作合并线 |
| `engine` | `引擎层/` | 事件、对象、空间分区、碰撞、工具集 | `EngineCore.lib` | 可用 |
| `system` | `系统层/` | 实体、属性、效应运行时系统 | `SystemCore.lib` | 可构建 |
| `test` | `测试层/` | 单元测试体系 + 测试模块选择器 | `EngineTests.exe` | 338 用例全绿 |
| `game` | `游戏层/` | 游戏内容与资源 | 无（当前无源码） | 骨架 |
| `doc` | 工作站（工作区之外） | 工作站文档：任务报告 + 项目导航 | 无 | 文档索引 |

---

## 三、各分支详解

### `engine` —— 引擎层

**职责**：拓扑最底层的基础能力层，**不含任何具体游戏逻辑**，也不感知任何上层。

**内容**：

| 模块 | 路径 |
| --- | --- |
| 事件系统 | `src/core/event/`（`Event_Terminal` 事件终端、`Event_Broker` 事件中转器） |
| 对象系统 | `src/core/object/`（`Object` 基类、`Object_Pool` 对象池） |
| 四叉树 | `src/core/spatial/partition/Quadtree/` |
| 四叉树管理器 | `src/core/spatial/partition/Quadtree_Manager/` |
| 碰撞体 | `src/core/spatial/collision/Collider/` |
| 碰撞空间 | `src/core/spatial/collision/Collision_Region/` |
| 碰撞代理器 | `src/core/spatial/collision/Collision_Proxy/` |
| 坐标类型与依赖封装 | `src/core/spatial/common/` |
| 工具模块群 | `src/tools/`（数据校验器、配置加载器、日志系统、网格加载器、辅助算法、引擎环境、计时器、随机数生成器、数值分配器） |

**构建**：

```
cmake -S 引擎层 -B 引擎层/out/build/x64-Debug -G Ninja
cmake --build 引擎层/out/build/x64-Debug
```

产物 `引擎层/out/build/x64-Debug/lib/EngineCore.lib`。本层是分层构建链的第一步，无下层依赖。

**当前状态**：可用。测试层全量用例全绿。遗留：两处既有 `warning C4715` 未处理；对外世界坐标仍受 `int`（±2^31）约束（超大尺寸只在四叉树内部以 64 位承载）。

> 详见 [引擎层/README.md](引擎层/README.md)

### `system` —— 系统层

**职责**：运行于引擎层之上的运行时系统层，向游戏层提供实体、属性、效应。

**内容**：

| 模块 | 路径 |
| --- | --- |
| 实体 / 实体管理器 | `src/entity/Entity/`、`src/entity/Entity_Manager/` |
| 属性 / 属性槽分发器 | `src/prop/Prop/`、`src/prop/Prop_Distributor/` |
| 效应 / 效应管理器 | `src/effect/Effect/`、`src/effect/Effect_Manager/` |
| 配置编辑器（GUI） | `src/gui/Config_Editor/`（含独立 `main()`，被 CMake 排除，未接入构建） |

**构建**：先构建引擎层，再

```
cmake -S 系统层 -B 系统层/out/build/x64-Debug -G Ninja
cmake --build 系统层/out/build/x64-Debug
```

产物 `系统层/out/build/x64-Debug/lib/SystemCore.lib`。配置期会自动定位引擎层已产出的 `EngineCore.lib`，找不到即硬失败。

**当前状态**：已接入层间静态库契约，可独立配置并构建。遗留：`实体管理器.h`、`属性槽分发器.h`、`效应.h`、`效应管理器.h` 四个头文件仍按引擎层旧路径 `src/tools/Config_Checker/配置检查器.h` 引用（引擎层已更名为 `Data_Validator`），待同步改名。

> 详见 [系统层/README.md](系统层/README.md)

### `test` —— 测试层

**职责**：**全项目唯一产出可执行文件的层**。承载单元测试体系与「测试模块选择器」，同时链接引擎层与系统层的静态库。

**内容**：

- `src/主调/`：主程序、测试选择模型、控制台选择菜单、图形选择窗口（Dear ImGui）、退出等待判定。
- `src/单元测试/`：用例源文件，按被测层分「主调 / 工具 / 核心」三棵子树。
- `external/`：googletest、Dear ImGui、glad（源码副本，本层自行编译）。
- `assets/`：指向 `游戏层/assets` 的**目录联接**，保证资产单一真源（不入库）。

**测试规模**：**22 个套件 / 338 个用例 / 0 失败 / 0 禁用**。

**构建与运行**：

```
# 先构建引擎层与系统层，再：
cmake -S 测试层 -B 测试层/out/build/x64-Debug -G Ninja
cmake --build 测试层/out/build/x64-Debug

# 产物：测试层/EngineTests.exe
EngineTests.exe                 # 默认 --selector=window：图形窗口勾选测试模块
EngineTests.exe --selector=console   # 控制台编号菜单（支持 1,3-5、all）
EngineTests.exe --selector=off       # 不开窗，全量运行（ctest 走这条）
EngineTests.exe --gtest_filter=...   # 已带 gtest 参数时直接透传，不弹窗
```

**当前状态**：全量 338 / 338 通过，`ctest` 1/1 通过。遗留：图形窗口的鼠标点选交互尚未自动化，回归只覆盖「窗口创建失败降级」路径。

> 详见 [测试层/README.md](测试层/README.md)

### `game` —— 游戏层

**职责**：依赖链顶端的游戏内容层。**当前为骨架**：`src/` 为空、不产出静态库，只保留层间契约骨架与 `assets/` 资产数据。

**内容**：

- `assets/config/`：实体、属性、格式定义与路由表（JSON）。
- `assets/scripts/`：实体初始化脚本与行为（决策树）脚本（Lua）。
- `cmake/对外接口.cmake`、`cmake/接入下层.cmake`：与其余三层一致的契约接入脚本。

**构建**：先构建系统层，再

```
cmake -S 游戏层 -B 游戏层/out/build/x64-Debug -G Ninja
```

当前无源码，配置可通过，但**不生成任何静态库**（`GameCore` 是条件目标：`src/` 下出现第一份 `.cpp/.c` 后才会建立）。本层不产出可执行文件。

**历史说明**：本层过去挂着一个「组合根可执行文件」，另有一个把测试层宿主并入本层构建树的开关。两者都已移除——组合根会把下层源码拉进本层重复编译，与「层间只链接已构建静态库」的契约冲突；现在可执行文件只由测试层产出。

> 详见 [游戏层/README.md](游戏层/README.md)

### `doc` —— 工作站文档

**职责**：存放本项目的工作站文档，与代码无关。由工作区之外的**工作站**（`D:\agent\working_station\silver_era`）推送。

**内容**：

- `任务报告合集/`：每项任务的执行计划与执行报告。
- `项目导航合集/`：每个代码版次的项目架构与模块架构文档（`项目架构.md` ＋ `模块架构/`）。

文档按「每分支一目录、每版次一目录」隔离存储，版次目录以**代码推送哈希值**命名（不含日期），因此目录列表本身看不出先后——要看版次顺序，请看该分支根目录的 `README.md`，它按时间倒序维护。

> 详见 doc 分支根目录的 `README.md`

---

## 四、`main` 分支是什么

`main` 是**顶层仓库**，本身不写代码，只做两件事：

1. **总览**：就是本文件。
2. **快照**：把四个层分支的最新源码各收一份，作为普通目录放在 `引擎层/`、`系统层/`、`测试层/`、`游戏层/` 下。

之所以要用「快照」而不是让顶层直接收录下层：同一个目录里嵌套 git 仓库时，上层仓库无法收录下层仓库的文件——git 会把整个子目录记成一个 gitlink，推到远程后那些目录是空的。让每层独立存档、各推各的分支，推送源互不干扰。

需要注意：

- **快照不会自动跟随各层更新。** 某一层有新提交后，需要重新做一次合并，`main` 才会跟上。所以 `main` 上的代码可能比 `engine` / `system` / `test` / `game` 落后，**要看最新代码请切到对应层分支**。
- `main` 的新提交只由「四分支合并」产生，不承载独立开发。
- 克隆 `main` 即可一次性拿到四层源码，无需切分支。

想跟踪某一层的开发过程，切到对应的分支即可。

---

## 五、构建

**工具链要求**：CMake ≥ 3.20、Ninja 生成器、支持 C++20 的编译器（当前只在 **MSVC / x64 / Windows** 上验证过）。命令需在已激活的 Visual Studio 开发人员环境中执行（需要 `cl.exe`）。

**必须逐层构建**，顺序不能颠倒——每一层在配置期就会去定位下层**已构建**的静态库，找不到会直接硬失败（这是刻意的：契约不允许回退编译下层源码）。

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
-DBYJY_ENGINE_LIB_PATH=<EngineCore.lib 绝对路径>
-DBYJY_SYSTEM_LIB_PATH=<SystemCore.lib 绝对路径>
```

**关于顶层 `CMakeLists.txt`**：顶层仍保留一份把 `引擎层` / `系统层` / `游戏层` `add_subdirectory` 进同一构建树的编排脚本，但它**不包含测试层、不产出任何可执行文件**，而且「同一构建树统一编排」与现行的「层间只链接已构建静态库」契约是相冲的（各层在配置阶段就会去定位下层的 `.lib`）。**请按上面的逐层顺序构建，不要依赖顶层一次性构建。**

**在 Visual Studio 中打开**：每一层都有自己的 `CMakeSettings.json`（Ninja / x64-Debug）。用 VS 的「打开本地文件夹」选中**层目录**即可配置构建；顶层也有一份 `CMakeSettings.json`，但同上，建议逐层打开。

---

## 六、许可

见 [LICENSE](LICENSE)。