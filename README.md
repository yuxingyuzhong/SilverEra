# 白银纪元 · 引擎层

引擎层是「白银纪元」四层仓库中**拓扑最底层的基础能力层**：向上提供事件、对象、空间分区、碰撞与通用工具集，自身**不包含任何具体游戏逻辑**，不感知任何上层存在。本层是独立 git 仓库，源码通过远端 `engine` 分支发布。

本层的历史名字是「游戏引擎」——那是一个「源码 / 资源 / 可执行文件 / 配置编辑器」一体的单体工程。分层改造后，本层只保留与具体游戏无关的基础能力，原先属于本层的实体体系、属性槽、效应、配置编辑器、资源与脚本目录均已迁出（详见[第十二节·历史沿革](#十二历史沿革旧名--现名现归属)）。

---

## 一、分支与仓库信息

### 1.1 仓库基本信息

| 项目 | 内容 |
| --- | --- |
| 层名称 | 引擎层（源码命名空间为 `engine`） |
| 本层根目录 | `引擎层/`（相对工程根） |
| 名义角色 | 基础能力层，拓扑最底层，**无下层依赖** |
| 本地分支 | `main` |
| 远端名 | `sliverera` |
| 远端地址 | `https://github.com/yuxingyuzhong/SilverEra.git` |
| 远端目标分支 | `engine`（推送命令形如 `git push sliverera main:engine`） |
| 构建产物 | `out/build/x64-Debug/lib/EngineCore.lib`（静态库） |
| 对外库名 | `EngineCore` |
| 对外面自描述 | `cmake/对外接口.cmake` |
| 接入函数 | **本层没有**（本层无下层可并入；`cmake/接入下层.cmake` 位于系统层 / 测试层 / 游戏层，三份逐字节相同） |

### 1.2 分层位置

```
引擎层  ←  系统层  ←  游戏层          测试层（横跨各层，唯一可执行文件产地）
```

- 依赖方向**单向**：`引擎层 ← 系统层 ← 游戏层`。系统层只消费引擎层，游戏层只消费系统层，反向依赖不存在。
- **测试层横跨各层**：测试层同时接入引擎层与系统层，承载单元测试与全项目唯一的可执行入口。
- 层与层之间**只链接下层已构建的静态库**，绝不通过 `add_subdirectory` 回退编译下层源码。
- 本层**不感知任何上层**：`src/` 与 `common/` 下不存在对 `系统层` / `游戏层` / `测试层` 的任何引用；上层消费本层的方式是「源码 include 本层公共头 + 链接 `EngineCore.lib`」。

### 1.3 版次与提交历史

本层每次代码推送在远端 `engine` 分支上汇聚成一个版次；本表按时间倒序，最新版次在最上。

| 代码提交哈希值 | 主题 | 要点 |
| --- | --- | --- |
| `65fc74f` | 重写 README：改为本层自述 | 目录结构 / 模块清单 / 对外接口契约 / 构建；当前 README 版次 |
| `089b070` | 碰撞模块拆分：碰撞代理器 / 碰撞空间 改为多编译单元 | 单文件拆为 5 + 4 个编译单元；对外符号约 16342 条 |
| `fa8459a` | 四叉树链路尺寸层 64 位化 | 修复超大边长上限整数除零（2^33 截断导致的主崩点） |
| `16d74df` | 修复区块检索在零尺寸下的整数除零 | `block_size == 0` 时提前返回 |
| `371a4c5` | 修复事件终端、对象池、四叉树、数据校验器与日志系统缺陷 | 多模块缺陷修复 |

---

## 二、本层职责与核心特性

### 2.1 职责边界

**本层负责**：

- 模块间通信基础设施（事件系统）；
- 通用对象编号与容器设施（对象系统）；
- 二维坐标基元与空间划分（坐标类型、四叉树、四叉树管理器）；
- 碰撞检测的完整封装（碰撞体、碰撞空间、碰撞代理器）；
- 与业务无关的通用工具（日志、配置加载、数据校验、网格解析、容器的二分检索、路径转换、可执行路径获取、计时、随机数、编号分配）。

**本层不负责**：

- 具体游戏逻辑（实体、属性、效应、行为）——归系统层；
- 资源配置与脚本（`assets/config/`、`assets/scripts/`）——归游戏层；
- 进程入口与帧循环——归测试层（全项目唯一可执行文件由测试层产出）；
- 渲染管线与窗口主循环——本层只把 GLFW 作为第三方依赖随库传递，不建立渲染体系。

### 2.2 核心特性

1. **事件驱动、模块解耦**：模块之间不互相持有指针，统一通过 `Event_Terminal`（事件终端）接入 `Event_Broker`（事件中转器），以 `event` 为统一载荷完成订阅 / 发布。每个终端持有一枚 ACL 权限密钥，越权发送会被拒绝。
2. **层间契约化**：本层对外暴露的内容全部写在 `cmake/对外接口.cmake` 中（包含目录、编译定义、第三方库、系统库），上层据此建立 `IMPORTED STATIC GLOBAL` 目标，无需任何 `add_subdirectory`。
3. **零上层感知**：本层是唯一「无下层」的层，构建顺序上处于分层构建链的第一步，可独立构建、独立测试。
4. **内存与生命周期自持**：对象池的编号分配器同时提供「稳定索引」与「排序检索」两种内存布局；四叉树支持原地加倍扩大；碰撞后端的四件套装配采用 `new(nothrow)` 并带失败回滚。
5. **数值稳健**：浮点坐标比较使用 ULP（最小精度单位）容差（≤ 4 ULP），避免物理模拟中的舍入抖动；四叉树内部几何运算走 64 位整数，规避超大边长下的溢出与除零。
6. **中文友好的工程实践**：源码、目录、注释均使用中文；文件名中文化；针对 Windows 平台 `lib.exe` 对无 BOM 归档响应文件的误读，专门提供了 `cmake/ar_rsp_bom.ps1` 包装脚本。

---

## 三、技术栈

### 3.1 语言与标准

| 项目 | 取值 |
| --- | --- |
| 语言 | C++ |
| 标准 | C++20（`CMAKE_CXX_STANDARD 20`，`CMAKE_CXX_STANDARD_REQUIRED ON`） |
| 编译器扩展 | **禁用**（`CMAKE_CXX_EXTENSIONS OFF`） |
| 源码编码 | UTF-8（MSVC 显式加 `/utf-8`） |
| 命名空间 | 统一 `engine`（含嵌套 `engine::detail` 存放内部工具） |

C++20 特性在源码中的使用：`concepts`（`Object_Pool` 的 `requires std::is_base_of_v<Object, T>`）、`<format>` / `std::format`（日志与坐标输出）、`std::bit_cast`（ULP 距离计算）、`std::numbers`、`std::ranges`、`<source_location>` 等均由预编译头统一引入。

### 3.2 第三方依赖

本层第三方库以**源码副本**形式存放在 `external/` 下，由 `file(GLOB_RECURSE ...)` 一并纳入 `EngineCore`。

| 库 | 目录 | 用途 | 引入方式 |
| --- | --- | --- | --- |
| nlohmann/json | `external/Json` | 事件载荷、配置数据的统一 JSON 容器（`nlohmann::json`） | 仅头文件，`PRIVATE` 包含；对外包含目录中同样声明 |
| GLFW | `external/glfw` | 窗口 / 输入基础设施；本层预编译头直接 `#include <GLFW/glfw3.h>` | 头文件 + 预编译静态库 `external/glfw/glfw3.lib`，编译定义 `GLFW_STATIC` |
| GLM | `external/glm` | 数学库；包含路径已在 `CMakeLists.txt` 与本层对外包含目录中声明 | 仅头文件（当前 `src/` 下未检索到对其头文件的直接引用） |
| Bullet Physics | `external/bullet3` | 碰撞检测后端；其类型被收敛为 `engine` 命名空间别名 | 源码级引入，头文件路径 `external/bullet3/src`，随 `EngineCore` 一起编译 |

补充说明：

- `EngineCore` 的第三方包含路径在本层构建中是 `PRIVATE` 的；`cmake/对外接口.cmake` 是按「上层实际需要什么」重新声明的**对外面**，两者若出现差异以 `cmake/对外接口.cmake` 为准。
- googletest 不属于本层，它由测试层持有。

### 3.3 构建工具链

| 项目 | 取值 / 说明 |
| --- | --- |
| 构建系统 | CMake，最低版本 `3.20`（`cmake_minimum_required(VERSION 3.20)`，因使用了 `CONFIGURE_DEPENDS`） |
| 生成器 | Ninja（`CMakeSettings.json` 中配置名 `x64-Debug`，生成器 `Ninja`，环境 `msvc_x64_x64`） |
| 主编译器 | MSVC（已验证）；`CMakeLists.txt` 同时为 GCC / Clang 准备了选项分支 |
| 构建目录 | `CMakeSettings.json` 中 `buildRoot` 为 `out/build/x64-Debug` |
| 版本控制 | git（本层为独立仓库） |

---

## 四、目录结构

以本层根为基准（`out/` 为构建输出，不纳入版本控制）：

```
引擎层/
├── CMakeLists.txt              # 构建 EngineCore 静态库（CMake ≥ 3.20，C++20）
├── CMakeSettings.json          # Visual Studio CMake 配置（x64-Debug / Ninja）
├── LICENSE.txt                 # MIT 许可证
├── README.md                   # 本文档
├── .gitignore
├── cmake/
│   ├── 对外接口.cmake          # 本层对外面自描述（BYJY_ENGINE_OUT_*）
│   └── ar_rsp_bom.ps1          # 归档响应文件 BOM 包装脚本（MSVC + Ninja 专用）
├── common/
│   ├── 前置头文件包含.h        # 统一预编译头：集中引入标准库与第三方库
│   └── 引擎.h                  # 引擎总入口：聚合事件系统运行包
├── external/                   # 第三方库副本：Json / bullet3 / glfw / glm
├── src/
│   ├── core/                   # 核心能力
│   │   ├── event/              #   事件系统
│   │   │   ├── 事件系统运行包.h
│   │   │   ├── Event/事件.h
│   │   │   ├── Event_Broker/{事件中转器.h, 局部命名空间使用.h, core/事件中转器.cpp}
│   │   │   └── Event_Terminal/{事件终端.h, 终端接口.h, 局部命名空间使用.h,
│   │   │                        core/{事件终端.cpp, 终端接口.cpp}}
│   │   ├── object/             #   对象系统
│   │   │   ├── 对象系统运行包.h
│   │   │   ├── Object/对象.h
│   │   │   └── Object_Pool/{对象池.h, core/对象池实例化.cpp}
│   │   └── spatial/            #   空间系统
│   │       ├── common/         #     坐标类型与依赖库封装
│   │       │   ├── 碰撞系统运行包.h
│   │       │   └── core/{坐标类型.h, 依赖库封装.h}
│   │       ├── collision/      #     碰撞系统
│   │       │   ├── Collider/碰撞体.h
│   │       │   ├── Collision_Region/{碰撞空间.h, 局部命名空间使用.h,
│   │       │   │                    core/{空间与边界.cpp, 碰撞体管理.cpp,
│   │       │   │                          形状构建.cpp, 检测执行.cpp}}
│   │       │   └── Collision_Proxy/{碰撞代理器.h, 局部命名空间使用.h,
│   │       │                        core/{空间边界与配置.cpp, 空间管理.cpp,
│   │       │                              碰撞体管理.cpp, 碰撞体配置.cpp,
│   │       │                              事件交互.cpp}}
│   │       └── partition/      #     空间分区
│   │           ├── Quadtree/{四叉树.h, 函数预声明.h, 数据结构.h,
│   │           │              core/{导航与范围计算.hpp, 节点创建与树扩大.hpp,
│   │           │                    区块信息检索.hpp, 设置与交互.hpp,
│   │           │                    四叉树实例化.cpp}}
│   │           └── Quadtree_Manager/{四叉树管理器.h, 函数预声明.h, 数据结构.h,
│   │                                  core/{基础操作.hpp, 区块信息检索.hpp,
│   │                                        设置与交互.hpp, 四叉树合并.hpp,
│   │                                        四叉树扩大回调管理.hpp,
│   │                                        四叉树智能创建.hpp,
│   │                                        相邻四叉树查找.hpp,
│   │                                        四叉树管理器实例化.cpp}}
│   └── tools/                  # 工具模块群
│       ├── Auxi_Algorithm/{二分查找.h, 路径字符串转换.h}
│       ├── Config_Loader/{配置加载器.h, 局部命名空间使用.h, core/配置加载器.cpp}
│       ├── Data_Validator/{数据校验器.h, 局部命名空间使用.h}
│       ├── Engine_Env/引擎环境.h
│       ├── Logging/日志系统.h
│       ├── Mesh_Loader/{网格加载器.h, 局部命名空间使用.h, core/网格加载器.cpp}
│       ├── Number_Allocator/数值分配器.h
│       ├── Random/{随机数生成器.h, core/随机数生成器.cpp}
│       └── Timer/{计时器.h, 局部命名空间使用.h, core/计时器.cpp}
└── out/                        # CMake 构建输出（build / install，不纳入版本控制）
```

目录组织约定：

- **模块目录**通常由「公共头文件 + `局部命名空间使用.h` + `core/` 实现目录」三部分组成。`core/` 下可以是一个或多个 `.cpp`（编译单元），也可以是一个或多个 `.hpp`（模板实现分片）。
- `局部命名空间使用.h` 不是业务头，它只做两件事：`#include` 本模块的公共头文件，并把本模块实现里频繁使用的 `std::` 名字（如 `string`、`vector`、`shared_ptr`、`nothrow`、`json`）提为文件级 `using` 声明。
- **模板类**（`Object_Pool<T, Key>`、`Quadtree<T>`、`Quadtree_Manager<T>`）实现拆到 `.hpp` 分片中，并由一个形如 `xxx实例化.cpp` 的编译单元显式实例化，从而既保留模板灵活性，又把实现编译进 `EngineCore`。
- **运行包头**（`事件系统运行包.h`、`对象系统运行包.h`、`碰撞系统运行包.h`）是纯粹的聚合头（facade），不含逻辑，用于「一行包含即引入某子系统全部公共定义」。

---

## 五、核心架构与运行链路

### 5.1 事件驱动架构

事件系统是本层的骨架，其余模块通过它解耦。三个角色：

```
  业务模块（持有 Event_Terminal）
        │  ① attach(module_name, needed_events, acl_key)
        │  ② build(sender, target, category, tag) → send(evt, acl_key)
        ▼
  ┌─────────────────────────┐
  │  Event_Broker（事件中转器）│  ③ 按订阅表把事件投递给命中模块的接收入口
  └─────────────────────────┘
        │  ④ 投递 receive(evt)
        ▼
  订阅方模块（持有 Event_Terminal，注册过 event_receiver）
```

- **订阅**：模块在 `attach()` 中声明自己关心的事件清单（`vector<event>`，只有 `category` 与 `tag` 参与匹配），事件中转器为其分配订阅者编号并登记。
- **发布**：模块用 `build()` 构造事件，用 `send()` 投出；`send()` 需要携带自己的 ACL 密钥。
- **投递**：中转器在 `process()` 中遍历订阅表，把事件逐个送入订阅方注册的 `std::function<void(std::shared_ptr<event>)>` 入口；订阅方通常在入口里再按 `tag` 分派到各 `event_process` 私有方法。
- **权限**：`Event_Terminal::attach()` 会校验终端接口中「事件发送入口」是否注册；只有持有合法密钥的终端才能发送，未分配的密钥会被拒绝。密钥由 `acl_key_gen()` 基于 PCG32 随机数生成器产生。
- **定向与广播**：事件的 `target_object` 为空表示广播，非空表示定向；接收方（如 `Collision_Proxy`）在 `event_process()` 开头用 `evt->target_object != module_name` 过滤掉不属于自己的定向事件。

### 5.2 层内模块依赖关系

```
                         ┌───────────────┐
                         │   事件系统      │  Event / Event_Terminal / Event_Broker
                         └──────┬────────┘
       ┌────────────────────────┼────────────────────────┐
       │                        │                        │
┌──────▼──────┐          ┌──────▼───────┐         ┌──────▼──────────┐
│  对象系统    │          │   空间系统     │         │   碰撞系统        │
│  Object     │          │  坐标类型       │────────▶│ Collider         │
│  Object_Pool│          │  Quadtree      │         │ Collision_Region │
└──────┬──────┘          │  Quadtree_     │         │ Collision_Proxy  │
       │                 │  Manager       │         └──────┬──────────┘
       │                 └──────┬────────┘                 │
       └──────────┬─────────────┴──────────────┬───────────┘
                  ▼                            ▼
           ┌──────────────┐            ┌────────────────────┐
           │ 通用工具模块群 │            │ 第三方依赖（外部）    │
           │ Logging       │            │ Json / GLFW / GLM   │
           │ Number_Alloc. │            │ bullet3             │
           │ Random / Timer│            └────────────────────┘
           │ Auxi_Algo ... │
           └──────────────┘
```

关键依赖事实：

- 事件系统被对象系统、碰撞系统、配置加载器共同复用；`Config_Loader` 与 `Collision_Proxy` 都持有公开成员 `Event_Terminal event_terminal`。
- `Collision_Proxy` 是本层**唯一对外的碰撞门面**：它持有多个 `Collision_Region`，维护「碰撞体编号 → 归属空间」的多重映射，并订阅 / 发布事件。
- `Object_Pool` 依赖 `Number_Allocator` 提供编号；`Event_Terminal` 依赖 `Random_Generator` 生成 ACL 密钥。
- 四叉树内部依赖 `坐标类型.h` 的 `Point2i/Point2l/Point2d` 与 `Rect2i/Rect2l/Rect2d`；碰撞模块依赖 `依赖库封装.h` 收敛 bullet3 类型。

### 5.3 典型运行链路

**链路一：配置驱动的碰撞空间构建**

```
1. Config_Loader::act()
     └ 扫描 assets/config/route/ 下路由文件，读取目标配置
        └ 发事件 Config/Load（target_object = 目标模块名，config = 配置 JSON）
2. Event_Broker 命中 Collision_Proxy 的订阅（Config/Load）
3. Collision_Proxy::event_process()
     └ 过滤 target_object → config_apply(config)
        ├ 读边界配置 → region_build / region_boundary_set
        └ 读碰撞体配置 → collider_build + collider_config_apply
                            └ 形状构建（六种形状之一）挂入 Collision_Region 的子弹世界
4. 后续由 Collision/RegionDetect 等指令驱动检测，结果以 Collision/<tag> 事件发布
```

**链路二：单点 / 范围空间检索**

```
调用方
  └ Quadtree_Manager::seek(单点检索) 或 seek(范围检索, stable)
       ├ 从树缓存中定位覆盖目标的四叉树（无则 qurdtree_build_smart 新建）
       ├ Quadtree::block_seek / range_seek
       │    ├ point_seekable_analyse / range_seekable_analyse 判断是否需要扩大
       │    ├ 需要扩大 → callback_register 回调交给管理器裁决 → tree_expand 原地加倍
       │    └ 沿递归路径下钻，产出 tree_chunk_data<T>
       └ 多树命中时用布尔表去重后返回
```

**链路三：网格加载到碰撞形状**

```
Collider::geometry（JSON）→ Mesh_Loader::load_obj(path, Mesh_Data)
     └ 解析 OBJ 顶点与面索引（含面索引四种写法、扇形三角化）
        └ 顶点数据喂给 bullet3 的 Triangle_Mesh → 挂入 Collision_Shape
           └ Collider 通过 setUserPointer 反向登记自身，供检测结果 recover 回查
```

---

## 六、模块详解

> 本节各模块的路径均以本层根为基准；行数为该版次源码的实际规模，用于衡量模块复杂度。

### 6.1 事件系统（`src/core/event/`）

本层模块间通信的枢纽。三个子件 + 一个聚合头。

#### 6.1.1 `event` 结构体

**涉及文件**：`src/core/event/Event/事件.h`（95 行）

**功能**：定义模块间传递的统一事件载荷。

**对外接口**：

```cpp
namespace engine
{
    //事件
    struct event
    {
        std::string sender_object;   //发起事件的对象名
        std::string target_object;   //事件的目标对象名（为空表示广播）
        std::string category;        //事件大类（英文，如 Config / Collision）
        std::string tag;             //事件标签（英文，如 Load / RegionBuild）
        nlohmann::json config;       //事件载荷

        event() = default;                                                  //默认构造
        event(const std::string& category, const std::string& tag);         //构造大类与标签
        event(const std::string& sender_object, const std::string& target_object,
              const std::string& category, const std::string& tag);         //对象标签构造
        event(const std::string& sender_object, const std::string& target_object,
              const std::string& category, const std::string& tag,
              const nlohmann::json& config);                                //全量构造

        bool operator==(const event& other) const;                          //判等
    };
}
```

**内部实现要点**：

- 三类含参构造逐级补全信息：只给大类与标签 → 再给发送者与目标 → 再给 `config` 载荷，便于订阅清单写得简洁（订阅清单里通常写 `event("", "", "Collision", "RegionBuild")`）。
- `operator==` 比较 `category`、`tag`、`target_object`、`config` 四项，**不比较 `sender_object`**——这样同一个事件无论由谁发出都可被同一订阅规则命中。
- 文件内提供 `std::hash<engine::event>` 特化：对 `category`、`tag`、`target_object` 做 `hash_combine`，再叠加 `config.dump()` 的哈希，使事件可作为 `unordered_*` 的键。
- `namespace detail` 中的 `hash_combine` 为内部工具，供上述特化使用。

#### 6.1.2 事件终端 `Event_Terminal` 与终端接口 `Terminal_Interface`

**涉及文件**：`src/core/event/Event_Terminal/事件终端.h`（68 行）、`终端接口.h`（80 行）、`core/事件终端.cpp`（239 行）、`core/终端接口.cpp`（88 行）、`局部命名空间使用.h`

**功能**：`Event_Terminal` 是模块持有的事件收发把手，负责密钥管理、事件构造、发送、接收与查阅；`Terminal_Interface` 是终端的「入口注册表」，把各类回调以 `std::function` 形式登记在案，供 `Event_Terminal` 转发调用。

**对外接口（`Event_Terminal`）**：

```cpp
namespace engine
{
    //事件终端
    class Event_Terminal
    {
    public:
        Event_Terminal() = default;
        ~Event_Terminal() = default;
        Event_Terminal(Event_Terminal&&) = default;
        Event_Terminal& operator=(Event_Terminal&&) = default;

        Terminal_Interface* operator->(void);                                     //终端接口快捷通道

        int64_t acl_key_gen(void);                                                //权限密钥生成

        bool attach(const std::string& module_name,
                    const std::vector<event>& needed_events,
                    const int64_t& acl_key);                                      //中转站接入
        bool interact(std::shared_ptr<event> evt, const int64_t& acl_key);        //中转站交互（单事件）
        bool interact(std::vector<std::shared_ptr<event>> events,
                      const int64_t& acl_key);                                    //中转站交互（多事件）

        std::shared_ptr<event> build(void);                                       //事件构造（空）
        std::shared_ptr<event> build(const std::string& category,
                                     const std::string& tag);                     //事件构造（大类标签）
        std::shared_ptr<event> build(const std::string& sender_object,
                                     const std::string& target_object,
                                     const std::string& category,
                                     const std::string& tag);                     //事件构造（全量）

        bool send(std::shared_ptr<event> evt, const int64_t& acl_key);            //事件发送（单事件）
        bool send(std::vector<std::shared_ptr<event>> events,
                  const int64_t& acl_key);                                        //事件发送（多事件）
        void receive(std::shared_ptr<event> evt);                                 //事件接收（单事件）
        void receive(std::vector<std::shared_ptr<event>> events);                 //事件接收（多事件）
        const std::vector<std::shared_ptr<event>>* query(const int64_t& acl_key); //事件查阅
        bool clear(const int64_t& acl_key);                                       //事件清空
    };
}
```

**对外接口（`Terminal_Interface`）**：

```cpp
namespace engine
{
    //终端接口列表
    enum class interface_ID
    {
        NONE, ATTACH_HANDLER, EVENT_INTERACTOR, EVENTS_INTERACTOR,
        EVENT_SENDOR, EVENTS_SENDOR, EVENT_RECEIVER, EVENTS_RECEIVER
    };

    //终端接口
    class Terminal_Interface
    {
        //（私有）类型别名
        using needed_events  = const std::vector<event>&;
        using event_handler  = std::function<void(std::shared_ptr<event> evt)>;
        using events_handler = std::function<void(std::vector<std::shared_ptr<event>>)>;
        using attch_handler  = std::function<void(const std::string& name,
                                   needed_events events, event_handler receiver)>;
        friend class Event_Terminal;
    public:
        bool attach_handler_register(attch_handler callback);           //中转站接入入口注册
        bool event_interactor_register(event_handler callback);         //中转站交互入口注册（单事件）
        bool events_interactor_register(events_handler callback);       //中转站交互入口注册（多事件）

        bool event_sender_register(event_handler callback);             //事件发送入口注册（单事件）
        bool event_sender_register(events_handler callback);            //事件发送入口注册（多事件）

        bool event_receiver_register(event_handler callback);           //事件接收入口注册（单事件）
        bool event_receiver_register(events_handler callback);          //事件接收入口注册（多事件）

        bool interface_check(const interface_ID& ID);                   //入口是否已注册
    };
}
```

**内部实现要点**：

- `Event_Terminal` 私有成员：`std::optional<int64_t> acl_key`（本终端密钥）、`Random_Generator key_generator`（密钥生成器）、`std::vector<std::shared_ptr<event>> event_set`（待发 / 已收事件集合）、`Terminal_Interface terminal_interface`（接口表）。
- `operator->()` 返回 `Terminal_Interface*`，于是模块可以写 `event_terminal->event_receiver_register(...)`，把注册工作直接穿透到接口表，而不必先取 `terminal_interface` 成员。
- `Terminal_Interface` 用 `std::vector<interface_ID> map` 记录各入口的注册顺序，配以 7 个 `std::unique_ptr<std::function<...>>` 成员（`attach_handler`、`event_interactor`、`events_interactor`、`event_sender`、`events_sender`、`event_receiver`、`events_receiver`）保存回调；公开的注册入口是 5 个名字、共 7 个重载（发送与接收各有单事件 / 多事件两个重载）。`interface_check(ID)` 供调用方在转发前确认入口已就位（`Collision_Proxy::attach()` 就是先检查 `ATTACH_HANDLER` 再注册接收入口）。
- `memory_malloc` / `function_register` 是 `Terminal_Interface` 的内部模板工具，负责在 `new(nothrow)` 失败时安全退出。
- **持有者责任**：`attach()` 用的「中转站接入入口」由事件中转站的持有者（宿主组合根）注册；终端本身只负责调用。

#### 6.1.3 事件中转器 `Event_Broker`

**涉及文件**：`src/core/event/Event_Broker/事件中转器.h`（40 行）、`core/事件中转器.cpp`（209 行）、`局部命名空间使用.h`

**功能**：订阅表 + 投递中枢。记录「谁订阅了哪些事件」，并在事件到来时逐个投递。

**对外接口**：

```cpp
namespace engine
{
    //事件中转器
    class Event_Broker
    {
    public:
        //订阅者登记注册
        void info_register(const std::string& module_name,
                           const std::vector<event>& needed_events,
                           std::function<void(std::shared_ptr<event>)> event_entry);
        void receive(std::shared_ptr<event> evt);                                  //单事件接收
        void receive(std::vector<std::shared_ptr<event>> event_set);               //多事件接收
        std::shared_ptr<event> process(std::shared_ptr<event> evt);                //单事件处理
        std::vector<std::shared_ptr<event>> process(
            std::vector<std::shared_ptr<event>> event_set);                        //多事件处理
    };
}
```

**内部实现要点**：

- 嵌套私有结构 `event_acl { std::string tag; std::vector<int32_t> ID_set; }`：用一个「事件标签 + 订阅者编号集合」表示一条订阅记录（匹配粒度是 `tag`）。
- 三张表：`acl_set`（`unordered_map<string, vector<event_acl>>`，大类 → 订阅记录集）、`mapping_set`（`unordered_map<string, int32_t>`，模块名 → 订阅者编号）、`event_entries`（`unordered_map<int32_t, function<void(shared_ptr<event>)>>`，编号 → 投递入口）。
- `info_register()` 分配 / 复用模块编号，登记投递入口，并把 `needed_events` 逐条写入 `acl_set`。
- `process()` 按事件的 `category` 取出候选订阅记录，再用 `tag` 精确比对，最后按 `ID_set` 逐项调用投递入口。

#### 6.1.4 事件系统运行包

**涉及文件**：`src/core/event/事件系统运行包.h`

一行聚合头：包含 `Event/事件.h` 与 `Event_Terminal/事件终端.h`。`common/引擎.h` 目前就只聚合了这一个运行包。

---

### 6.2 对象系统（`src/core/object/`）

#### 6.2.1 对象基类 `Object`

**涉及文件**：`src/core/object/Object/对象.h`（37 行）

**功能**：提供对象的最小共同契约——编号与合法性。

**对外接口**：

```cpp
namespace engine
{
    //对象
    class Object
    {
    protected:
        uint64_t object_ID = 0;      //对象编号
        bool is_valid = false;       //对象是否合法
    public:
        void ID_set(const uint64_t& ID);      //设置编号
        void valid_set(bool valid);           //设置合法性
        uint64_t ID(void) const;              //读取编号
        bool valid(void);                     //读取合法性
    };
}
```

**内部实现要点**：受保护成员只暴露读写函数，编号与合法性由派生者（以及对象池）负责维护；编号 0 与 `is_valid == false` 共同构成「空对象」标记。**注意方法名是 `ID_set`**，不是历史文档中的旧名。

#### 6.2.2 对象池 `Object_Pool`

**涉及文件**：`src/core/object/Object_Pool/对象池.h`（318 行）、`core/对象池实例化.cpp`（6 行）

**功能**：受约束的模板化对象容器，提供「稳定索引」与「排序检索」两套内存布局，配以编号分配与回收。

**对外接口（要点）**：

```cpp
namespace engine
{
    //对象池
    template <typename T, typename Key = uint64_t>
        requires std::is_base_of_v<Object, T>
    class Object_Pool
    {
    public:
        template <typename Projection>
        void sort_order_set(bool greater, Projection proj);                   //切换到排序模式
        void sort_order_reset(void);                                          //切回稳定模式

        typename std::vector<T>::iterator find(const Key& key);               //按键查找
        uint64_t build(void);                                                 //新增对象，返回其池下标

        void unload(const Key& key);                                          //卸载单个
        void unload(const std::vector<Key>& keys);                            //卸载多个
        void clear(void);                                                     //清空
        std::vector<T>& data(void);                                           //数据容器
        typename std::vector<T>::iterator end(void);                          //数据末尾
    };
}
```

**内部实现要点**：

- 类约束 `requires std::is_base_of_v<Object, T>`：只有继承 `Object` 的类型才能入池，从类型层面保证每个元素都有编号。
- 双分配器：`Number_Allocator ID_allocator`（分配对象编号）与 `Number_Allocator index_allocator`（分配容器下标）。
- **union 双模式**：容器内部用一个匿名 `union` 复用同一块内存——稳定模式下是 `unordered_map`（键 → 下标），排序模式下是「投影器 + 比较器 + `min_valid_index`」三元组，配合排序后的 `std::vector<T> objects` 做二分查找。`is_sorted` 标记当前模式。
- `build()` 是唯一的「新增对象」入口：先用 `index_allocator.get()` 取得池下标（越界则扩容），把该槽位重置为 `T{}` 以清除残留，再用 `ID_allocator.get()` 分配对象编号并 `valid_set(true)`，最后按当前模式维护索引映射或重排序；返回值为新对象所在的池下标。
- `min_valid_index` 记录当前有效元素的最小下标，`find()` 在排序模式下从该下标起二分，避免在已卸载的前缀上白费比较；`find()` 返回容器的迭代器，未命中时返回 `end()`。
- 卸载会做三重清理（释放编号、标记非法、归零编号），编号可被回收复用；`Object_Pool<Object>` 由 `core/对象池实例化.cpp` 显式实例化。
- 该模板实现直接写在头文件内（非 `.hpp` 分片），因此包含即得实现。

#### 6.2.3 对象系统运行包

**涉及文件**：`src/core/object/对象系统运行包.h`

聚合 `Object/对象.h` 与 `Object_Pool/对象池.h`。

---

### 6.3 坐标类型与依赖封装（`src/core/spatial/common/`）

**涉及文件**：`core/坐标类型.h`（146 行）、`core/依赖库封装.h`（117 行）、`碰撞系统运行包.h`（9 行）

#### 6.3.1 坐标类型

**功能**：引擎自有的二维点 / 矩形基元，全引擎唯一权威定义。

**对外接口**：

```cpp
namespace engine
{
    //二维点
    template <typename T>
    struct Point2
    {
        T X = T{};   //横坐标
        T Y = T{};   //纵坐标
        Point2() = default;
        Point2(T x, T y);
        bool operator==(const Point2& other) const noexcept;   //浮点走 ULP 容差
        bool operator!=(const Point2& other) const noexcept;
    };

    //二维矩形范围
    template <typename T>
    struct Rect2
    {
        T left = T{};    //左边界
        T right = T{};   //右边界
        T up = T{};      //上边界
        T down = T{};    //下边界
        Rect2() = default;
        Rect2(T l, T r, T u, T d);
        bool operator==(const Rect2& other) const noexcept;    //四边界精确比较
        bool operator!=(const Rect2& other) const noexcept;
    };

    using Point2i = Point2<int>;          //整数精度点
    using Point2l = Point2<int64_t>;      //64 位整数精度点
    using Point2d = Point2<double>;       //双精度点
    using Rect2i  = Rect2<int>;           //整数精度矩形
    using Rect2l  = Rect2<int64_t>;       //64 位整数精度矩形
    using Rect2d  = Rect2<double>;        //双精度矩形

    inline Point2i point_to_int(const Point2d& p) noexcept;       //浮点 → 整数（四舍五入）
    inline Point2d point_to_double(const Point2i& p) noexcept;    //整数 → 浮点
    inline Point2l point_to_l(const Point2d& p) noexcept;         //浮点 → 64 位整数
    inline Point2d point_to_double(const Point2l& p) noexcept;    //64 位整数 → 浮点
}
```

**内部实现要点**：

- `Point2::operator==` 对浮点类型启用 **ULP 容差**：先精确比较（含 `+0.0` / `-0.0`），再处理无穷与 NaN（NaN 恒不等），最后比较位模式距离，**≤ 4 ULP 视为相等**。整数类型走精确比较。
- `Rect2::operator==` 是四边界**逐项精确比较**，不复用 ULP 逻辑——矩形比较语义从严。
- `engine::detail::ulp_distance(double, double)` 是 ULP 距离的内部实现：按位取模式、符号不同直接返回 `UINT64_MAX`、swap 后相减得到距离。
- 三种精度之间**没有隐式转换**，跨精度必须显式调用上面四个转换自由函数；转换统一采用四舍五入（`std::lround` / `std::llround`）。
- 必须注意的有损场景：`int64_t` 超过 2^53 转 `double` 会丢精度；`Point2d` 超出 `int` 范围转 `Point2i` 会被收窄。

#### 6.3.2 依赖库封装

**功能**：把 bullet3 的核心类型收敛为 `engine` 命名空间下的语义别名，并提供碰撞后端聚合体。

**对外接口（要点）**：

```cpp
namespace engine
{
    //碰撞架构四件套
    using Collision_Config     = btDefaultCollisionConfiguration;   //碰撞配置
    using Collision_Dispatcher = btCollisionDispatcher;             //窄阶段调度器
    using Collision_Interface  = btBroadphaseInterface;             //宽阶段接口
    using Collision_World      = btCollisionWorld;                  //碰撞世界
    //宽阶段加速结构
    using Bvh_Tree = btDbvtBroadphase;    //动态包围体层次树
    using SAP      = btAxisSweep3;        //扫描线剪枝
    //其余别名涵盖：世界对象、三角网格、向量 / 变换 / 四元数 / 标量、
    //六种形状（盒体 / 球体 / 胶囊 / 圆柱 / 圆锥 / 凸包 / 三角网格）、持久流形、
    //扫掠回调与扫掠结果等。

    inline constexpr int Static_Object_Flag = ...;   //静态碰撞对象标记

    //碰撞后端
    struct Collision_Backend
    {
        std::shared_ptr<Collision_Config>     config;      //配置
        std::unique_ptr<Collision_Dispatcher> dispatcher;  //窄阶段调度器
        std::unique_ptr<Collision_Interface>  broadphase;  //宽阶段接口
        std::unique_ptr<Collision_World>      world;       //碰撞世界
        bool valid(void) const;                            //四件是否齐备
    };
}
```

**内部实现要点**：

- `Collision_Backend` 的装配集中在含参构造（`explicit Collision_Backend(std::shared_ptr<Collision_Config>)`）中：`dispatcher`、`broadphase`（实际实例是 `Bvh_Tree`）、`world` 用 `new(std::nothrow)` 分配，任一为空即**整体回滚**（四个成员全部 `reset()`），不留下半装配状态；默认构造委托该含参构造并以 `std::make_shared<Collision_Config>()` 提供配置。`valid()` 检查四者是否齐备；禁拷贝、可移动，避免 world 内部指针被浅拷贝破坏。
- 类型别名的意义在于「更换物理库时只需改这一个头文件」，上层代码只依赖 `engine::Collision_World` 之类的名字。
- `碰撞系统运行包.h` 把「依赖库封装 + 碰撞体 + 碰撞空间」聚合为单点包含，`Collision_Proxy` 只 include 它即可。

---

### 6.4 四叉树（`src/core/spatial/partition/Quadtree/`）

**涉及文件**：`四叉树.h`、`函数预声明.h`（131 行）、`数据结构.h`（48 行）、`core/导航与范围计算.hpp`（181 行）、`core/节点创建与树扩大.hpp`（64 行）、`core/区块信息检索.hpp`（364 行）、`core/设置与交互.hpp`（98 行）、`core/四叉树实例化.cpp`（6 行）

**功能**：以区块（chunk）为单位管理二维空间数据的四叉树容器——单棵方形区域的区块寻址、范围检索、自身原地扩大与销毁。它**不负责多棵树之间的调度、合并、去重**，那是 `Quadtree_Manager` 的职责。

**对外接口（要点）**：

```cpp
namespace engine
{
    //四叉树
    template <typename T>
    class Quadtree
    {
    public:
        Quadtree(const uint64_t& size = 256, const Point2d& root = { 0.5,0.5 });
        ~Quadtree(void);

        void set_block_size(const uint64_t& size);                          //最小区块单元边长
        void set_max_size(const uint64_t& size);                            //边长上限
        void set_callback_manage(const std::function<bool(Point2d root,
                                     Point2l target)>& cb);                 //扩大权限回调

        void block_seek(tree_chunk_data<T>*& receiver,
                        const Point2l& target, bool stable);                 //单点区块查询
        void range_seek(std::vector<tree_chunk_data<T>*>& receiver,
                        const Rect2l& target_range, bool stable);            //范围区块查询

        const tree_state& tree_state_get(void);                             //读取树状态
        bool tree_expand(void);                                             //原地扩大（边长翻倍）

        //底层计算工具（供 Quadtree_Manager 复用）
        void manage_range_calcu(Rect2l& receiver, const Point2d& root,
                                const uint64_t tree_size);
        void target_range_format(Rect2l& target_range, const Point2d& root,
                                 const uint64_t block_size);
        Point2d seekable_range_calcu(const Rect2l& target_range,
                                     Rect2l& seekable_range);
    };
}
```

配套数据结构（`数据结构.h`）：

```cpp
namespace engine
{
    //树状态
    struct tree_state
    {
        Point2d  root       { 0.5, 0.5 };               //根坐标
        uint64_t size       = 256;                      //当前边长
        uint64_t max_size   = 9223372036854775808ULL;   //边长上限（2^63）
        uint64_t block_size = 16;                       //最小区块单元边长
    };

    //区块数据（查询结果）
    template <typename T>
    struct tree_chunk_data
    {
        Point2d node;      //区块中心坐标
        T*      ptr_data;  //区块叶子数据指针
    };
}
```

**内部实现要点**：

- **64 位尺寸层契约**（由前序版次 `fa8459a` 引入）：区块节点范围用 `Rect2l`、回调目标坐标用 `Point2l`，内部几何运算全走 64 位整数，从而使边长可以配置到 `INT_MAX` 以上而不溢出；对外暴露的区块中心仍回落为 `Point2d`，保持与上层浮点接口兼容。
- `range_seek` 采用**显式栈迭代而非递归**：以 `std::vector<recur_record>` 模拟栈、`std::vector<int> recur_path` 记录当前路径，规避深递归导致的栈溢出。
- **原地扩大** `tree_expand()`：分配 4 个新的中间节点，把原根 4 个槽位的子树按反方向（`NW→SE、NE→SW、SW→NE、SE→NW`）下沉挂到新节点，再把新节点挂回根，最后 `size *= 2`；O(1) 完成「向上加一层」。
- **扩大权限受控**：四叉树不擅自扩大，`set_callback_manage()` 注册的 `callback` 向 `Quadtree_Manager` 申请权限，批准后才执行 `tree_expand()`；未注册回调时若 `size < max_size` 则自行扩大。
- **单点查询三态分析**：`point_seekable_analyse` 返回 0（不可行，终止）/ 1（可能可行，继续尝试扩大）/ 2（可行，直接寻址）。
- 节点 `Node` 是联合体：中间节点用 `ptr_child[4]`，叶子节点用 `leaf`（类型 `T`），按 `Node_type` 用 placement new 激活对应成员。
- **范围查询可能返回重复区块**：同一坐标的区块在相邻多次查询边界重叠时会被重复创建，去重由 `Quadtree_Manager` 用布尔表负责。
- 调用方注意：`block_seek` 的 `stable == false` 不分配缺失节点；`stable == true` 会即时 `new` 补齐路径，保证结果必定存在。
- 工程内以 `Quadtree<int>` 实例化（见 `core/四叉树实例化.cpp`），并在头文件末尾以 `using engine::Quadtree;` 引出命名空间。

---

### 6.5 四叉树管理器（`src/core/spatial/partition/Quadtree_Manager/`）

**涉及文件**：`四叉树管理器.h`、`函数预声明.h`（142 行）、`数据结构.h`（46 行）、`core/基础操作.hpp`（244 行）、`core/区块信息检索.hpp`（220 行）、`core/设置与交互.hpp`（118 行）、`core/四叉树合并.hpp`（307 行）、`core/四叉树扩大回调管理.hpp`（215 行）、`core/四叉树智能创建.hpp`（291 行）、`core/相邻四叉树查找.hpp`（182 行）、`core/四叉树管理器实例化.cpp`（6 行）

**功能**：管理若干棵四叉树的调度层——建树 / 卸载、单点与范围检索、相邻树查找、四叉树合并、以及四叉树扩大申请的裁决。是碰撞模块与空间查询的对外入口。

**对外接口（要点）**：

```cpp
namespace engine
{
    //四叉树管理器
    template <typename T>
    class Quadtree_Manager
    {
    public:
        Quadtree_Manager(void);
        ~Quadtree_Manager(void);

        //数据迁移方法注册
        void callback_register(const std::function<void(tree_chunk_data<T>& receiver,
            tree_chunk_data<T>& transmiter)>& cb_1);
        void set_block_size(const uint64_t& block_size);                 //最小区块单元边长
        void set_max_size(const uint64_t& max_size);                     //单树边长上限
        void set_min_size(const uint64_t& min_size);                     //单树边长下限
        void set_cache_state(bool enabled);                              //树缓存开关
        void set_cache_active_threshold(const uint64_t& threshold);      //缓存启用阈值
        void set_max_cach_records(const uint64_t max_entries);           //缓存记录上限

        void qurdtree_build_smart(const std::vector<Point2i>& coord_set); //按点集智能建树

        void seek(tree_chunk_data<T>*& reciver, const Point2i& target,
                  bool stable);                                          //单点检索
        void seek(std::vector<tree_chunk_data<T>*>& receiver,
                  const Rect2i& target_range, bool stable);              //范围检索

        const tree_manager_settings& settings_get(void);                 //读取设置
        const std::vector<tree_record<T>*>& records_get(void);           //读取树记录
        const uint64_t& largest_size_get(void);                          //最大单树边长

        void qurdtree_merge(void);                                       //四叉树合并
        void cache_clear(void);                                          //清空树缓存
        void clear(void);                                                //清空全部树
        void quadtree_unload(const std::vector<Point2d>& root_set);      //按根集卸载
    };
}
```

配套数据结构（`数据结构.h`）：

```cpp
namespace engine
{
    //单棵树记录
    template <typename T>
    struct tree_record
    {
        Quadtree<T>* tree;              //树指针
        Point2d      root { 0.5, 0.5 }; //树根坐标
        uint64_t     size = 256;        //树边长
    };

    //管理器设置
    struct tree_manager_settings
    {
        uint64_t block_size            = 16;     //最小区块单元边长
        uint64_t max_tree_size         = 65536;  //单树边长上限
        uint64_t min_tree_size         = 256;    //单树边长下限
        bool     is_cache_enabled      = true;   //是否启用树缓存
        uint64_t cache_active_threshold = 32;    //缓存启用阈值
        uint64_t max_cache_records     = 16;     //缓存记录上限
    };
}
```

**内部实现要点**：

- **64 位契约延续**：管理器内部以 64 位承载树尺寸；修复了超大边长（2^33）在尺寸截断后导致的整数除零主崩点。
- **智能建树** `qurdtree_build_smart(std::vector<Point2i>)`：依据待覆盖点集自动决定建树位置与边长，而非要求调用方指定根与尺寸。
- **相邻树三级筛选查找**：`相邻四叉树查找.hpp` 以由粗到细的三级筛选定位与目标范围相邻的树，降低查找开销。
- **四叉树合并**：`四叉树合并.hpp` 把可以合并的相邻同尺寸树归并为一棵，控制树的数量膨胀。
- **扩大裁决**：四叉树把自己的扩大申请通过回调上报给管理器；管理器由 `四叉树扩大回调管理.hpp` 判断该次扩大会否与已在册的其它树发生管辖范围重叠，再决定批准与否——这是多树并存时避免相互重叠的关键。
- **数据迁移回调**：`callback_register()` 注册的是 `void(tree_chunk_data<T>&, tree_chunk_data<T>&)` 形式的迁移方法，供合并 / 扩大时把源区块数据搬运到目标区块，业务层借此决定数据如何随空间重组而迁移。
- **缓存**：管理器维护 `tree_cache { records, ranges }`，缓存近期使用过的树；缓存启用阈值与记录上限可配置，`cache_clear()` 可手动清空。
- **对外检索的坐标仍受 32 位约束**：`seek` 使用 `Point2i` / `Rect2i`（详见第九节已知问题）。
- 工程内以 `Quadtree_Manager<int>` 实例化（见 `core/四叉树管理器实例化.cpp`）。

---

### 6.6 碰撞体（`src/core/spatial/collision/Collider/`）

**涉及文件**：`碰撞体.h`（100 行）

**功能**：描述一个碰撞体的全部属性：编号、子弹世界对象、可选的三角网格、形状、位移向量、检测方式、豁免标记、原始几何 JSON。

**对外接口（要点）**：

```cpp
namespace engine
{
    //检测方式
    struct Detection_Mode
    {
        bool     is_swept_volume = false;   //是否使用扫掠体检测
        uint32_t step_length     = 0;       //扫掠步长
    };

    //碰撞体
    struct Collider
    {
        uint64_t                      ID = 0;              //碰撞体编号
        Collision_Object              object;              //子弹世界对象
        std::unique_ptr<Triangle_Mesh> mesh;               //三角网格（先于形状声明）
        std::unique_ptr<Collision_Shape> shape;            //碰撞形状
        Vector3                       displacement_vector; //位移向量
        Detection_Mode                detection_mode;      //检测方式
        uint64_t                      exemption_flag = 0;  //豁免标记
        nlohmann::json                geometry;            //原始几何配置

        static Collider* recover(const Collision_Object* object);  //由世界对象反查碰撞体
    };
}
```

**内部实现要点**：

- **声明顺序即生命周期契约**：`mesh` 必须先于 `shape` 声明。原因是有形状（如三角网格形状）引用 mesh 的内存，析构顺序与声明顺序相反，把 `mesh` 放在前面可以保证形状先释放、mesh 后释放，避免悬挂引用。
- **自指针重挂**：`Collider` 移动（容器扩容等）后内部指针仍需指向新的自身地址，故移动路径上会重新挂接自指针。
- **反查机制**：构建时对子弹世界对象调用 `setUserPointer(this)`，检测回调里用 `Collider::recover(const Collision_Object*)` 把世界对象还原成 `Collider*`，从而拿到编号与豁免标记。
- **六种形状**：盒体、球体、胶囊、圆柱、圆锥、三角网格（OBJ），由 `Collision_Region` 的 `形状构建.cpp` 按 `geometry` JSON 装配。
- `exemption_flag` 供业务层标记「本碰撞体不参与某些配对检测」。

---

### 6.7 碰撞空间 `Collision_Region`（`src/core/spatial/collision/Collision_Region/`）

**涉及文件**：`碰撞空间.h`（113 行）、`局部命名空间使用.h`、`core/空间与边界.cpp`（95 行）、`core/碰撞体管理.cpp`（250 行）、`core/形状构建.cpp`（288 行）、`core/检测执行.cpp`（155 行）

**功能**：一个独立的碰撞世界。持有一份 `Collision_Backend`（子弹后端），管理空间边界与其中的全部碰撞体，执行扫掠 + 离散两阶段检测。

**对外接口（要点）**：

```cpp
namespace engine
{
    //碰撞结果
    struct Collision_Result
    {
        uint64_t collider_A;   //参与碰撞的碰撞体编号 A
        uint64_t collider_B;   //参与碰撞的碰撞体编号 B
    };

    //碰撞空间
    struct Collision_Region
    {
        //默认构造 / 含参构造（指定碰撞空间名称）
        Collision_Region();
        explicit Collision_Region(const std::string& region_name);
        Collision_Region(const Collision_Region&) = delete;            //禁止拷贝（后端含 unique_ptr）
        Collision_Region(Collision_Region&&) = default;                //可移动
        ~Collision_Region();

        bool valid(void) const;                                        //空间是否有效
        void state_set(bool active);                                   //设置活跃性
        bool state_check() const;                                      //读取活跃性

        bool boundary_build(const std::string& mesh_path);              //按网格路径构建边界
        void boundary_unload();                                        //卸载边界

        uint64_t collider_build(void);                                 //构建碰撞体（分配编号）
        bool collider_unload(uint64_t collider_ID);                    //卸载碰撞体
        bool collider_set(const uint64_t collider_ID, const Vector3& vector);        //设置位移向量
        bool collider_set(const uint64_t collider_ID, const Detection_Mode& vector); //设置检测方式
        bool collider_set(const uint64_t collider_ID, const uint64_t& vector);       //设置豁免标记
        bool collider_set(nlohmann::json geometry_config);             //追加几何体
        bool collider_find(uint64_t collider_id) const;                //编号是否存在于本空间
        std::vector<uint64_t> colliders(void);                         //列出全部编号
        bool collider_adopt(uint64_t collider_ID);                     //按编号接管（供空间间转移）
        nlohmann::json collider_geometry(uint64_t collider_ID) const;  //读取几何配置

        std::optional<std::vector<Collision_Result>> detect(void);     //执行碰撞检测
        bool contains(uint64_t collider_ID) const;                     //编号是否归属本空间
    };
}
```

**内部实现要点**：

- 私有成员：`name`（空间名）、`is_active`（活跃标记）、`Number_Allocator ID_allocator`（碰撞体编号分配）、`Collider region_boundary`（空间边界本身也是一个碰撞体）、`Collision_Backend backend`（子弹后端四件套）、`std::unordered_map<uint64_t, Collider> mapping`（编号 → 碰撞体）。
- **边界即碰撞体**：空间边界复用了 `Collider` 的表示（`region_boundary`），由 `boundary_build(mesh_path)` 按 OBJ 网格路径构建，因此边界与普通碰撞体走同一套检测机制。
- **析构顺序的硬约束**：碰撞体与空间边界都直接持有子弹对象，而碰撞世界内部记录的是这些对象的地址。`~Collision_Region()` 先把全部碰撞对象移出碰撞世界，再让映射与后端析构，否则碰撞世界析构时会解引用已释放的对象。同理，按值搬移（默认移动构造）之后须重新执行边界构建，否则边界会留下悬空指针；`Collision_Proxy` 用 `unique_ptr` 持有空间则不受此影响。
- **两阶段检测**：`检测执行.cpp` 先做扫掠（对配置了 `is_swept_volume` 的碰撞体按其位移向量与步长推进），再做离散检测；豁免标记在配对阶段生效。
- **形状构建**：`形状构建.cpp` 按 `geometry` JSON 构建六种形状；三角网格形状会调用 `Mesh_Loader` 读 OBJ，并把顶点数据转成子弹的三角网格。
- 私有 `collider_seek` / `scalar_read` / `vector_read` / `quaternion_read` / `mesh_shape_build` / `shape_build` 是配置解析与形状装配的内部工具。
- 编号由空间**自行分配**，因此不同空间可以持有相同编号（这正是 `Collision_Proxy` 用多重映射登记归属的原因）。

---

### 6.8 碰撞代理器 `Collision_Proxy`（`src/core/spatial/collision/Collision_Proxy/`）

**涉及文件**：`碰撞代理器.h`（91 行）、`局部命名空间使用.h`、`core/空间边界与配置.cpp`（159 行）、`core/空间管理.cpp`（151 行）、`core/碰撞体管理.cpp`（176 行）、`core/碰撞体配置.cpp`（141 行）、`core/事件交互.cpp`（250 行）

**功能**：本层对外唯一的碰撞门面。管理多个碰撞空间，维护「碰撞体编号 → 归属空间」映射，订阅配置与碰撞指令事件，发布检测结果事件。

**对外接口（要点）**：

```cpp
namespace engine
{
    //碰撞代理器
    class Collision_Proxy
    {
    public:
        Event_Terminal event_terminal;                                   //事件终端（公开）

        Collision_Proxy();
        ~Collision_Proxy() = default;

        bool region_build(const std::string& region);                    //构建碰撞空间
        bool region_unload(const std::string& region);                   //卸载碰撞空间
        bool region_detect(const std::string& region);                   //执行检测
        bool region_boundary_set(const std::string& path);               //按路径设置边界
        bool region_state_set(const std::string& region, bool active);   //设置活跃性

        std::optional<uint64_t> collider_build(const std::string& region);          //构建碰撞体
        bool collider_unload(const uint64_t collider_ID, const std::string& region);//卸载
        bool collider_transfer(const uint64_t collider_ID, const std::string& region);//转移
        bool collider_mirror(const uint64_t collider_ID, const std::string& region);  //镜像
        bool collider_set(const uint64_t collider_ID, const Vector3& vector);        //设置位移
        bool collider_set(const uint64_t collider_ID, const Detection_Mode& mode);   //设置检测方式
        bool collider_set(const uint64_t collider_ID, const uint64_t& flag);         //设置豁免标记

        void attach(void);                                               //接入事件中转站
    };
}
```

**订阅事件清单**（`attach()` 中登记，共 11 条）：

| category | tag | 含义 |
| --- | --- | --- |
| `Config` | `Load` | 配置加载（定向：`target_object == "Collision_Proxy"`） |
| `Collision` | `RegionBuild` | 构建碰撞空间 |
| `Collision` | `RegionUnload` | 卸载碰撞空间 |
| `Collision` | `RegionState` | 设置空间活跃性 |
| `Collision` | `RegionBoundary` | 设置空间边界 |
| `Collision` | `RegionDetect` | 执行空间检测 |
| `Collision` | `ColliderBuild` | 构建碰撞体 |
| `Collision` | `ColliderUnload` | 卸载碰撞体 |
| `Collision` | `ColliderTransfer` | 转移碰撞体归属 |
| `Collision` | `ColliderMirror` | 镜像碰撞体 |
| `Collision` | `ColliderSet` | 设置碰撞体参数 |

**内部实现要点**：

- `inline static const std::string module_name = "Collision_Proxy"`：既是事件中转站的登记名，也是事件 `sender_object` 标识、以及 `Config/Load` 定向过滤的比对基准。
- 私有成员：`std::unordered_map<std::string, std::unique_ptr<Collision_Region>> regions`（空间集合）、`std::unordered_multimap<uint64_t, std::string> collider_mapping`（碰撞体编号 → 归属空间名，**多重映射**）、`int64_t acl_key`（事件发送密钥）。
- **多重映射的必要性**：编号由各空间自行分配，同一编号可能被不同空间分别持有，故用 `unordered_multimap`；`collider_set_dispatch()` 会把一次参数设置分发到所有持有该编号的空间。
- **转移与镜像**：转移把碰撞体从一个空间移交给另一个空间（同时改动归属映射）；镜像在同一空间内按位位移向量生成对称的碰撞体，用于「成对出现」的几何体。
- `attach()` 的顺序是：先 `interface_check(ATTACH_HANDLER)` 确认接入入口已注册 → `event_receiver_register()` 注册接收入口 → 组装 `needed_events` → `event_terminal.attach(module_name, needed_events, acl_key)`。
- 发布侧统一走 `event_publish(tag, payload)`，内部用 `build(module_name, "", "Collision", tag)` 构造带有发送者标识的广播事件。

---

### 6.9 工具模块群（`src/tools/`）

| 模块 | 涉及文件 | 职责 |
| --- | --- | --- |
| `Data_Validator`（数据校验器） | `Data_Validator/数据校验器.h`（149 行）、`局部命名空间使用.h` | JSON 字段存在性与类型校验、路径有效性校验 |
| `Config_Loader`（配置加载器） | `Config_Loader/配置加载器.h`（44 行）、`core/配置加载器.cpp`（252 行）、`局部命名空间使用.h` | 扫描路由目录读取配置并广播 `Config/Load` 事件 |
| `Logging`（日志系统） | `Logging/日志系统.h`（151 行） | 分级日志格式化输出 |
| `Mesh_Loader`（网格加载器） | `Mesh_Loader/网格加载器.h`（31 行）、`core/网格加载器.cpp`（194 行）、`局部命名空间使用.h` | 解析 OBJ 为 `Mesh_Data`，供碰撞网格形状使用 |
| `Auxi_Algorithm`（辅助算法） | `Auxi_Algorithm/二分查找.h`（96 行）、`路径字符串转换.h`（28 行） | 容器的二分查找 / 区间查找；中文路径与字符串互转 |
| `Engine_Env`（引擎环境） | `Engine_Env/引擎环境.h`（137 行） | 获取可执行文件路径 / 目录，拼接绝对路径 |
| `Timer`（计时器） | `Timer/计时器.h`（39 行）、`core/计时器.cpp`（74 行）、`局部命名空间使用.h` | 多任务命名计时 |
| `Random`（随机数生成器） | `Random/随机数生成器.h`（31 行）、`core/随机数生成器.cpp`（72 行） | 基于 PCG32 的全范围 / 无偏区间随机数 |
| `Number_Allocator`（数值分配器） | `Number_Allocator/数值分配器.h`（78 行） | 编号分配与回收（复用池） |

**要点摘录**：

- `Data_Validator`：`template <typename T> static bool field_check(const nlohmann::json&, const std::string&)` 检查字段是否存在且类型匹配；`static bool path_check(path/string)` 两个重载校验路径有效性。这是历史文档中「配置检查器」的现名。
- `Config_Loader`：私有成员 `std::u8string scan_content = u8"assets/config/route/"` 与 `allowed_root = u8"assets/config/"`，即**它只扫描路由目录，并限制在配置根目录之内**（防止越权跳转，内部有 `skip_safety_inspect` 恶意跳转检查）；读取成功后构造事件，填写 `category = "Config"`、`tag = "Load"`，并设置 `target_object` 为路由里声明的目标模块名；`act()` 是入口。
- `Logging`：`Log` 类提供静态模板 `info / warn / error / debug`，签名接受 `std::format_string<Args...>` 支持 `{}` 占位格式化；`stream_set()` 可切换输出流（用于重定向到文件）；内部另有 `std::formatter<std::error_code>` 特化以便直接打印错误码。
- `Mesh_Loader`：`struct Mesh_Data { std::vector<float> vertices; std::vector<uint32_t> indices; }`；`static bool load_obj(const std::string&, Mesh_Data&)` 解析 OBJ；带两个硬上限——`max_file_size = 64MiB`（文件大小）与 `max_vertex_count = 1000000`（顶点数量）；面索引支持四种常见写法，非三角面用扇形三角化，末尾做完整性终检；路径采取双源回退。
- `Auxi_Algorithm`：`binary_search(first, last, target, comp, proj)` 返回相对 `first` 的全局下标（未找到返回 `-1`），另有容器重载；`range_binary_search` 返回闭区间 `std::pair<int,int>`（未找到返回 `{-1,-1}`）。`path_to_string()` / `string_to_path()` 经 `std::filesystem::path::u8string()` 往返，用于处理包含中文的文件路径。
- `Engine_Env`：`exe_path_get()`、`exe_dir_get()`、`absolute_path_get(path/string)`；可执行路径的获取按平台分派（Windows `GetModuleFileNameW` / Linux `/proc/self/exe` / macOS `_NSGetExecutablePath`），失败时回退到当前工作目录。
- `Timer`：`using Clock = std::chrono::steady_clock`，`task_build()` 建任务、`elapsed(task, restart = false)` 读耗时，静态 `units()` / `Milli_units()` / `Micro_units()` / `Nano_units()` 提供单位换算。
- `Random`：内核是结构 `pcg32 { uint64_t state, inc; }`，`operator()()` 生成全范围值，`operator()(min, max)` 生成无偏区间值；构造函数可传种子；`acl_key_gen()` 就是它的使用者。
- `Number_Allocator`：`set(min)` 设下限、`get()` 取号、`recycle(单/多)` 回收、`reset()` 复位；回收时用二分查找查重，重复回收会打 `Log::warn("Number_Pool::待回收数值已被回收!!!")`。

---

## 七、对外接口契约

层间交接由「对外面自描述 ＋ 接入函数」两部分组成。

### 7.1 本层对外面：`cmake/对外接口.cmake`

本层是拓扑最底层，没有下层可并入，故该文件只描述本层自身，按约定导出五个变量（前缀 `ENGINE`）：

| 变量 | 值 | 含义 |
| --- | --- | --- |
| `BYJY_ENGINE_OUT_LIB` | `EngineCore` | 本层静态库文件名（不含扩展名） |
| `BYJY_ENGINE_OUT_INC` | 本层根目录、`external/Json`、`external/glfw`、`external/glm`、`external`、`external/bullet3/src` | 使用本层公共头所需的包含目录（共 6 条） |
| `BYJY_ENGINE_OUT_DEF` | `GLFW_STATIC` | 使用本层公共头所需的编译定义 |
| `BYJY_ENGINE_OUT_LINK` | `external/glfw/glfw3.lib` | 本层对外传递的第三方库 |
| `BYJY_ENGINE_OUT_SYS` | `opengl32`、`user32`、`gdi32`、`shell32` | 本层对外传递的系统库 |

包含目录为什么这么多：本层的预编译头 `common/前置头文件包含.h` 内部 `#include` 了 `<nlohmann/json.hpp>` 与 `<GLFW/glfw3.h>`，上层只要包含这个预编译头就会用到 `external/Json` 与 `external/glfw`；bullet3 的头文件由 `src/core/spatial/collision/` 下的公共头引用，故 `external/bullet3/src` 也一并声明。

`BYJY_ENGINE_ROOT` 由 `get_filename_component(... ABSOLUTE)` 在本文件内推导，**不是硬编码路径**。

### 7.2 接入函数 `byjy_jieru_xiaceng()`

`cmake/接入下层.cmake` **不属于本层**（本层无下层可接入），它位于系统层 / 测试层 / 游戏层，三份逐字节相同。上层通过其中的 `byjy_jieru_xiaceng(接口文件, 前缀, 覆盖变量)` 读取本层对外面，按**三级策略**定位已构建的 `EngineCore.lib`：

1. **覆盖变量优先**：形如 `BYJY_ENGINE_LIB_PATH` 的缓存变量非空且文件存在 → 直接使用；非空但文件不存在 → **硬失败**，不静默降级。
2. **自动探测**：覆盖变量为空 → 扫描 `out/build/*/lib/EngineCore.lib`，多个候选取时间戳最新的一份。
3. **硬失败**：仍找不到 → 报错并给出构建本层的命令。

定位成功后建立 `IMPORTED STATIC GLOBAL` 目标 `EngineCore`，并把包含目录 / 编译定义 / 链接库与系统库挂到 `INTERFACE` 属性上，逐层向上传递。**任何情况下都不回退编译本层源码。**

### 7.3 归档响应文件 BOM 包装：`cmake/ar_rsp_bom.ps1`

Ninja 写出的归档响应文件（`*.rsp`）是 UTF-8 无 BOM，而 MSVC 的 `lib.exe` 在没有 BOM 时按系统 ANSI 代码页解读文件内容，中文对象名会被解成乱码，归档阶段报 `LNK1181`。该脚本在调用真实归档器前为响应文件补上 UTF-8 BOM（已含 BOM 或全 ASCII 的文件不改动），使源文件名保持中文、bullet3 继续留在 `EngineCore` 内。

接线要点（源码注释中明确强调）：`set(CMAKE_CXX_CREATE_STATIC_LIBRARY ...)` **必须在 `project()` 之后设置**，否则会被 `Platform/Windows-MSVC.cmake` 的默认值覆盖回去；该改写**只对 Ninja 生成器生效**，避免影响 Visual Studio 生成器（其归档由 MSBuild 的 `Lib` 任务完成）。当包装脚本或 PowerShell 任一缺失时，配置阶段会给出 `WARNING` 而不是静默继续。

---

## 八、构建与运行

### 8.1 环境要求

| 项目 | 要求 |
| --- | --- |
| CMake | ≥ 3.20 |
| 生成器 | Ninja |
| 编译器 | MSVC（已验证）；亦支持 GCC / Clang |
| C++ 标准 | C++20，禁用编译器扩展 |
| 平台 | Windows 为主要开发平台（源码内含 `windows.h` 的按需引入） |

### 8.2 构建步骤

本层无下层依赖，是分层构建链的第一步。

```bash
# 配置（在本层根目录执行）
cmake -S . -B out/build/x64-Debug -G Ninja

# 构建
cmake --build out/build/x64-Debug
```

若使用 Visual Studio，直接以 `CMakeSettings.json` 中已配置好的 `x64-Debug`（Ninja + `msvc_x64_x64`）打开本层根目录即可。

### 8.3 产物与关键构建行为

- **产物**：`out/build/x64-Debug/lib/EngineCore.lib`（静态库；输出目录由 `ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"` 指定）。该库的外部符号约 16342 条。
- **源文件收集**：`file(GLOB_RECURSE ENGINE_SOURCES CONFIGURE_DEPENDS src/*.cpp common/*.cpp external/*.cpp)`——增删源文件无需手动重跑 CMake；构建目录内的文件会被正则过滤掉，防止误编译临时产物；若过滤后无源文件则 `FATAL_ERROR` 终止。
- **包含路径**：本层构建时 `target_include_directories(EngineCore PRIVATE "${PROJECT_ROOT_DIR}")` 使源码可用 `src/...` 形式的自根包含；第三方路径（Json / glfw / glm / bullet3）同样为 `PRIVATE`。
- **链接**：`find_package(OpenGL REQUIRED)`；链接 `external/glfw/glfw3.lib` 与 `OpenGL::GL`；`target_compile_definitions(EngineCore PUBLIC GLFW_STATIC)`；MSVC 追加 `opengl32.lib user32.lib gdi32.lib shell32.lib`。
- **编译选项（MSVC）**：`/MP`（多核）、`/utf-8`（源文件 UTF-8）、`/WX-`（不将警告视为错误，全局设置）、`_CRT_SECURE_NO_WARNINGS`。
- **编译选项（GCC / Clang）**：`-Wall -Wextra -pedantic`，并定义 `_GNU_SOURCE`。
- **安装规则**：头文件（`*.h` / `*.hpp`）安装到 `include/EngineCore` 下（排除 `Private` 目录），静态库安装到 `lib` 下。
- **本层不产出任何 `.exe`**：全项目唯一的可执行文件由测试层产出，本层只交出一份静态库。

### 8.4 与其他层的协作

- 其他层要使用本层，只需在自己的 `CMakeLists.txt` 中 `include` 三份相同的 `cmake/接入下层.cmake`，调用 `byjy_jieru_xiaceng(...)` 指向本层的 `cmake/对外接口.cmake`，即可获得 `EngineCore` 目标（或通过 `BYJY_ENGINE_LIB_PATH` 显式指定库路径）。
- 本层构建产物不在版本控制中，`out/` 目录应被忽略。

---

## 九、实现状态与已知问题

### 9.1 已完成

- 事件系统：`event`、`Event_Terminal`、`Terminal_Interface`、`Event_Broker` 全部落地，含 ACL 权限密钥模型与订阅 / 发布投递。
- 对象系统：`Object` 基类与 `Object_Pool<T, Key>`（双模式内存布局、编号分配回收）。
- 空间系统：`Point2` / `Rect2` 坐标基元（含 ULP 容差）、四叉树与四叉树管理器（含 64 位尺寸层、原地扩大、智能建树、相邻查找、合并、缓存）。
- 碰撞系统：`Collider`（六种形状）、`Collision_Region`（后端四件套 + 两阶段检测）、`Collision_Proxy`（多空间门面 + 事件订阅发布 + 编号多重映射）。
- 工具模块群：上表九个模块全部可用。
- 层间契约：`cmake/对外接口.cmake` 五个变量齐备；本层可被上层以 `IMPORTED` 目标方式接入。
- 构建工程化：MSVC + Ninja 下中文对象名归档问题已有 `ar_rsp_bom.ps1` 方案。
- 测试现状（测试层视角）：22 个测试套件、338 个用例全部通过。

### 9.2 尚未完成

- **渲染管线**：本层只把 GLFW / OpenGL 作为依赖随库传递，尚未建立任何渲染循环或绘制体系。
- **空间系统与碰撞系统的联动**：碰撞模块当前不依赖 `Quadtree_Manager` 做候选筛选，区域检测仍是空间内全量两阶段检测；空间分区用于碰撞加速的接线尚未完成。
- **`common/引擎.h` 的聚合范围**：目前只聚合了事件系统运行包；`引擎.h` 末尾留有占位注释，对象系统运行包等尚未登记。

### 9.3 已知问题

| 位置 | 现象 |
| --- | --- |
| `src/core/spatial/partition/Quadtree/core/区块信息检索.hpp` | 既有 `warning C4715`：非 void 函数存在未覆盖的返回路径（历史遗留，尚未消除） |
| `src/core/event/Event_Broker/core/事件中转器.cpp` | 同上，存在既有 `warning C4715` |
| `Quadtree_Manager::seek` / 对外检索接口 | 使用 `Point2i` / `Rect2i`（32 位整数），对外世界坐标仍受 `int`（± 2^31）约束；超大尺寸层仅在树内部以 64 位整数承载 |
| 编译告警面 | 第三方头（尤其 bullet3 / Windows 头）会引入 `C4005`（宏重定义）一类的既有告警；`/WX-` 保证其不阻断构建 |
| 浮点比较语义 | `Point2::operator==` 使用 ≤ 4 ULP 容差，而 `Rect2::operator==` 为精确比较，两者语义不同，调用方须明确区分 |
| 大坐标精度 | `int64_t` 超过 2^53 转 `double` 会丢精度（`point_to_double` 的已知有损场景） |

---

## 十、开发指南

### 10.1 编码与目录规范

- 命名空间统一为 `engine`，内部工具放 `engine::detail`。
- 文件命名以中文优先；英文文件名单词间以下划线分隔、首字母小写。
- 类名单词首字母大写（如 `Event_Terminal`）；结构体 / 联合体 / 枚举首字母小写（如 `tree_state`、`event_acl`）。
- 变量 / 函数名英文、单词间下划线分隔、首字母小写（如 `region_boundary_set`）。
- 目录级命名约定：`模块名_Manager`（管理器）、`模块名_Broker`（中转器）、`模块名_Terminal`（终端）、`模块名_Proxy`（代理器）、`模块名_Region`（区域）、`模块名_Pool`（池）、`模块名_Loader`（加载器）、`模块名_Validator`（校验器）、`模块名_Allocator`（分配器）、`模块名_Generator`（生成器）。
- 注释使用中文，采用换行注释；预计少于三行用 `//`，三行及以上用 `/* */`；`//` 后不留空格。
- include 路径必须与实际目录大小写完全一致（跨平台硬性要求）。
- 文件编码统一 UTF-8（MSVC 已加 `/utf-8`）。
- 为避免中文路径在 Windows 下的编码冲突，路径与字符串互转走 `Auxi_Algorithm/路径字符串转换.h`。

### 10.2 如何新增一个引擎模块

1. 在 `src/core/` 或 `src/tools/` 下创建模块目录，目录内建立公共头文件（如需内部 `using` 别名则加 `局部命名空间使用.h`），实现放 `core/` 子目录；模板实现拆到 `.hpp` 分片并配一个显式实例化的 `.cpp`。
2. 若模块要参与事件通信：加入公开成员 `Event_Terminal event_terminal`，提供 `attach()`，在其中依次做「确认接入入口已注册 → 注册接收入口 → 组装 `needed_events` → `event_terminal.attach(...)`」。
3. 若模块需要被上层使用：其头文件应在 `cmake/对外接口.cmake` 的包含路径覆盖之内（本层根目录已在列表中），或把必要入口并入某个运行包头。
4. 若模块需要新的第三方库：新增内容应放入 `external/`（本层的公共外部库存储目录），并在 `CMakeLists.txt` 与 `cmake/对外接口.cmake` 两侧同步包含路径。
5. 源码文件会被 `CONFIGURE_DEPENDS` 自动收集，直接构建即可，无需手动重跑 CMake。

### 10.3 常见坑位

- **`CMAKE_CXX_CREATE_STATIC_LIBRARY` 的赋值位置**：必须在 `project()` 之后、且仅在 Ninja 生成器下改写，否则会被平台模块默认值覆盖。
- **`Collider` 的成员声明顺序**：`mesh` 必须先于 `shape`，破坏此顺序会导致形状先于网格释放的顺序错乱。
- **模板实例化**：新增模板类型若要进入 `EngineCore`，需在对应 `xxx实例化.cpp` 中补 `template class ...;`。
- **事件 `target_object` 过滤**：定向事件（非空 `target_object`）必须由接收方自行过滤，中转器不会代劳。
- **`Event_Broker` 的匹配粒度**：订阅匹配看 `category` + `tag`，`config` 不参与匹配；因此同一 `category` + `tag` 只能有一条投递规则。
- **`Number_Allocator` 重复回收**：会打印告警而非崩溃，排查编号错乱时可据此定位。
- **路径扫描越权防护**：`Config_Loader` 只扫描 `assets/config/route/` 且限制在 `assets/config/` 之内，新增路由目录不在此范围会被跳过。

---

## 十一、路线图

- [ ] **空间 - 碰撞联动**：让 `Collision_Proxy` / `Collision_Region` 借助 `Quadtree_Manager` 做碰撞候选筛选，替代当前的空间内全量检测。
- [ ] **对外检索尺寸层 64 位化**：把 `Quadtree_Manager::seek` 的对外接口由 `Point2i` / `Rect2i` 提升为 64 位（或将 32 位明确固化为对外契约并写清边界）。
- [ ] **清理既有告警**：消除两处 `C4715`，梳理第三方头引入的宏重定义告警。
- [ ] **`common/引擎.h` 补全**：在聚合事件系统运行包之外，登记对象系统运行包与其余常用入口。
- [ ] **渲染管线**：基于现成的 GLFW / OpenGL 依赖建立最小渲染循环。
- [ ] **`Object_Pool` 使用面扩展**：为编号复用补充更明确的文档与防护，降低编号错配风险。
- [ ] **插件化模块接口**：为可选模块预留可插拔的接入方式。

---

## 十二、历史沿革（旧名 → 现名／现归属）

本层的前身是单体工程「游戏引擎」。分层改造后，原工程中与具体游戏相关的内容已分配到其他层或取消。**下面这些旧内容在引擎层已不存在**，列在此处只为追溯来源；它们的现归属请查阅对应层的 README。

| 旧（单体「游戏引擎」时期） | 现名 / 现归属 | 说明 |
| --- | --- | --- |
| 仓库 / 工程名「游戏引擎」 | 「白银纪元 · 引擎层」 | 一体工程拆分为四层，本层只是基础能力层 |
| 根目录下嵌套 `游戏引擎/` 子目录 | 本层根即 `引擎层/` | 源码不再嵌套一层 |
| `common/types/对象类型.h`（`Object` + `Prop`） | `Object` → `src/core/object/Object/对象.h`（本层）；`Prop` 属性槽 → 系统层 | 对象基类留在本层，属性槽随实体体系迁出 |
| `common/types/事件类型.h` | `src/core/event/Event/事件.h` | 事件类型定义位置调整 |
| `common/types/坐标类型.h`、`几何体类型.h`、`计时器类型.h` | `src/core/spatial/common/core/坐标类型.h`（本层）；`几何体类型.h` 已不存在 | 计时器类型并入 `src/tools/Timer/计时器.h` 内部；几何体相关定义已不保留 |
| `common/引擎总头文件.h` | `common/引擎.h` | 聚合范围收窄为事件系统运行包 |
| `common/external/Sol2/`（sol 类型别名 / 注册） | Lua 绑定层 → 游戏层 | 脚本绑定迁出本层 |
| `src/core/entity/`（`Entity`、`Entity_Manager`、`Prop_Distributor`） | 实体系统、属性、效应（旧 `Prop_Effect`）、实体管理器、属性槽分发器 → 系统层 | 整体迁出本层 |
| `src/core/space/` | `src/core/spatial/` | 目录改名并细分为 `common` / `collision` / `partition` |
| `src/core/collision/Collision_Agent/` | `Collision_Proxy`（`src/core/spatial/collision/Collision_Proxy/`） | 旧「碰撞代理器」命名被 `Collision_Proxy` 取代 |
| `src/core/collision/Collision_Processer/` | `Collision_Region`（`src/core/spatial/collision/Collision_Region/`） | 旧「碰撞处理器」命名被 `Collision_Region` 取代 |
| `src/tools/Object_Pool/` | `src/core/object/Object_Pool/` | 对象池升入对象系统 |
| `Config_Checker` / 「配置检查器」 | `Data_Validator`（`src/tools/Data_Validator/数据校验器.h`） | 更名，职责不变 |
| `src/tools/Non_GUI/` 中间层 | 已取消 | 工具模块直接挂在 `src/tools/` 下 |
| `src/tools/GUI/Config_Editor/`（配置编辑器 GUI 与 `ConfigEditor.exe`、`imgui.ini`） | 配置编辑器 → 系统层 | 图形化编辑器迁出本层 |
| `主调文件/主调文件.cpp`（宿主组合根、依赖注入装配、`for(;;)` 帧循环） | 已取消 | 宿主机制取消；全项目唯一可执行文件由测试层产出 |
| `游戏引擎/TestEngine.exe` | 测试层产出的可执行文件 | 测试入口改由测试层承载 |
| `assets/config/`、`assets/scripts/`、`assets/UI/` | 游戏层 | 资源、JSON 配置、Lua 脚本、UI 资源整体迁出 |
| `排除编译代码/effect/`（旧效应系统） | 效应系统 → 系统层 | 效应系统从本层移出 |
| `external/` 下的 `Dear_ImGui`、`glad`、`Lua`、`Sol2`、`stb` | 已移出本层 | 本层现仅保留 `Json`、`glfw`、`glm`、`bullet3` |
| 旧坐标类型命名（如 `coord2D_int`） | `Point2i` / `Point2d` / `Point2l`、`Rect2i` / `Rect2d` / `Rect2l`（均在 `namespace engine`） | 统一收敛到模板 + 精度别名 |
| `四叉树通信结构体.h` / `四叉树管理器通信结构体.h` | `Quadtree/数据结构.h` / `Quadtree_Manager/数据结构.h` | 通信结构体更名为数据结构，并统一为 `tree_chunk_data` / `tree_state` / `tree_record` / `tree_manager_settings` |
| `Object::ID_bind` | `Object::ID_set` | 方法更名 |
| 旧版次 `0b8dbd27` 所描述的模块布局 | 已被 `089b070`、`65fc74f` 取代 | 旧版次的目录 / 命名不再适用 |

关于「旧版 README 中的失效条目」：单体时期文档描述的多处缺陷与未完成项（如实体创建的循环内返回、`Event_Terminal::attach` 的空指针解引用、`query` 的悬垂引用、路由中的模块名拼写错误、include 路径大小写不一致等）均属于**当时的实体体系与资源配置**，随相应代码一并迁出本层，因此不在本层第九节中重复记录。

---

## 十三、许可

本层采用 **MIT License**，许可证正文见本层根目录下的 `LICENSE.txt`。

```
MIT License

Copyright (c) [year] [fullname]

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

`LICENSE.txt` 中的版权年份与署名仍为占位符（`[year]` / `[fullname]`），尚未填写，需要发布前补齐。