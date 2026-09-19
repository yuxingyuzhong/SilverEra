# 游戏引擎

> 一个基于 **C++20 + Lua** 的轻量级游戏引擎，采用「事件驱动 + 对象池」架构，内置四叉树空间索引与碰撞处理骨架、数据驱动的实体配置与 Lua 行为脚本，并附带一个基于 Dear ImGui 的图形化配置编辑器。

**当前版次**：`0b8dbd27d36f4eef2f7f43b73a0ba52f87547cf2`（对象池与事件终端优化）

---

## 目录

- [项目简介](#项目简介)
- [核心特性](#核心特性)
- [技术栈](#技术栈)
- [目录结构](#目录结构)
- [核心架构](#核心架构)
- [核心模块详解](#核心模块详解)
- [构建与运行](#构建与运行)
- [配置系统](#配置系统)
- [Lua 脚本系统](#lua-脚本系统)
- [配置编辑器（GUI）](#配置编辑器gui)
- [实现状态与已知问题](#实现状态与已知问题)
- [开发指南](#开发指南)
- [路线图](#路线图)
- [许可证](#许可证)

---

## 项目简介

本项目是一个自研的轻量级游戏引擎，整体设计目标是**数据驱动 + 逻辑可热更新**：

- **对象（Object）**：引擎内一切可被对象池托管的事物的公共基类，提供编号绑定/获取与有效性标记（`ID_bind` / `ID` / `valid_set` / `valid`）。
- **实体（Entity）**：继承 `Object`，是可被 Lua 脚本驱动的活动对象，持有事件终端、属性槽指针、权限密钥与行为脚本运行时。
- **属性槽（Prop）**：继承 `Object`，是「一组具名数值」的容器（`unordered_map<std::string, double>`）。实体以指针引用属性槽，属性槽本体由实体管理器统一分配。
- **事件系统（Event）**：模块之间通过 `Event_Broker`（事件中转器）解耦通信；各模块持有 `Event_Terminal`（事件终端）完成订阅注册与事件收发。事件以「发起者 + 目标 + 大类 + 标签」描述，并可携带 JSON 配置包。
- **空间系统（Space）**：基于四叉树（Quadtree）的 2D 空间索引，由 `Quadtree_Manager` 管理，用于区域查询与碰撞候选筛选。
- **碰撞系统（Collision）**：`Collision_Agent`（碰撞代理器）负责碰撞体登记，`Collision_Processer`（碰撞处理器）负责碰撞判定与结果分发。
- **脚本系统（Lua）**：通过 Sol2 将 C++ 类型暴露给 Lua。实体初始化脚本负责写入初始属性，行为脚本负责每帧的决策逻辑。
- **配置驱动（JSON）**：实体类型、属性槽初始化路径、行为脚本路径等由 `assets/config/` 下的 JSON 描述，`Config_Loader` 在启动时加载并广播给各模块。

> 当前状态：**开发中（未完成）**。核心模块骨架已可编译运行；空间与碰撞模块具备头文件与算法骨架，尚未接入实体系统。

> 关于效应系统：`src/core/effect/` **已不在源码树内**。相关实现（`Effect/`、`Effect_Manager/`）存放于根目录的 `排除编译代码/effect/`，不参与编译。如需恢复，可从版次 `845558f`（引擎内核优化）取回。

---

## 核心特性

| 特性 | 说明 |
| --- | --- |
| 🧩 **事件驱动架构** | 模块间通过事件解耦，支持订阅登记、单/批量投递、目标定向 |
| 🗂️ **对象池托管** | `Object_Pool<T>` 统一分配与回收实体、属性槽，O(1) 取用 |
| 🧬 **对象继承体系** | `Object` → `Prop` / `Entity`，统一编号与有效性语义 |
| 📊 **属性槽机制** | 通用数值属性以 `string → double` 键值对存储，Lua 端可直接读写 |
| 🔐 **权限密钥（ACL）** | 事件终端的收发接口均以密钥校验调用者身份 |
| 🌳 **四叉树空间索引** | 2D 空间划分与区域查询，为索敌/碰撞提供候选集 |
| 💥 **碰撞处理骨架** | 碰撞代理器 + 碰撞处理器，算法已就位、待接线 |
| 📜 **Lua 脚本驱动** | Sol2 绑定，初始化脚本 + 行为脚本两级体系 |
| 📄 **JSON 数据驱动** | 实体类型、属性路径、行为脚本路径全部外置，无需重编译 |
| 🖥️ **图形化配置编辑器** | 基于 Dear ImGui + GLFW + OpenGL 的独立工具 `ConfigEditor.exe` |
| 🔧 **现代化 C++20** | 强制 C++20、禁用编译器扩展 |
| 🧪 **双可执行目标** | `TestEngine`（引擎测试入口）与 `ConfigEditor`（配置编辑器） |

---

## 技术栈

### 语言与标准

| 项 | 值 |
| --- | --- |
| 语言 | C++20（`CMAKE_CXX_STANDARD 20` + `CMAKE_CXX_STANDARD_REQUIRED ON`，`CMAKE_CXX_EXTENSIONS OFF`） |
| 命名空间 | `engine` |
| 源码风格 | 中文注释 + 中文类名/文件名（如 `实体.h`、`属性槽分发器.h`） |

### 第三方依赖（`游戏引擎/external/`）

| 库 | 用途 | 引入方式 |
| --- | --- | --- |
| [Lua](https://www.lua.org/) | 脚本语言运行时 | 源码直接编译（`.c` 文件编入引擎） |
| [Sol2](https://github.com/ThePhD/sol2) | C++ ↔ Lua 绑定层 | 纯头文件（`external/Sol2/include`） |
| [nlohmann/json](https://github.com/nlohmann/json) | JSON 解析 | 纯头文件（`external/Json`） |
| [GLFW](https://www.glfw.org/) | 窗口与输入 | 预编译静态库 `glfw3.lib` |
| [GLAD](https://glad.dav1d.de/) | OpenGL 函数加载 | 头文件 + 源码（`external/glad`） |
| [glm](https://github.com/g-truc/glm) | 数学库（供编辑器使用） | 纯头文件（`external/glm`） |
| [Dear ImGui](https://github.com/ocornut/imgui) | 即时模式 GUI | 源码（`external/Dear_ImGui`） |
| [stb](https://github.com/nothings/stb) | 单头文件图像/字体库 | 单头文件（`external/stb`） |

### 构建工具链

| 项 | 值 |
| --- | --- |
| 构建系统 | CMake ≥ 3.20（使用 `CONFIGURE_DEPENDS` 自动检测源文件变更） |
| 编译器 | MSVC（Visual Studio）、GCC / Clang |
| Windows 配置 | Visual Studio「CMake 配置」：`x64-Debug`，生成器 Ninja，继承 `msvc_x64_x64` 环境 |
| 输出目录 | 静态库 → `out/build/<config>/lib`；可执行文件 → `游戏引擎/` 目录 |

---

## 目录结构

> 注意：仓库根目录下存在一层**嵌套的 `游戏引擎/` 子目录**，所有源码与资源都在其中。根目录只放 CMake 工程文件、代码存档与文档。

```
D:\代码存储\代码仓库\游戏引擎\
├── CMakeLists.txt                # 顶层 CMake 构建脚本（253 行）
├── CMakeSettings.json            # Visual Studio CMake 配置（x64-Debug / Ninja）
├── LICENSE.txt                   # MIT 许可证
├── README.md                     # 本文档
├── 排除编译代码/                  # 不参与编译的代码存档
│   ├── effect/                   #   效应系统（已从源码树移出）
│   └── Tests/                    #   测试代码存档
├── out/                          # CMake 构建输出（build / install）
│
└── 游戏引擎/                      # ← 引擎本体（所有源码与资源）
    ├── TestEngine.exe            # 引擎测试可执行文件（构建产物）
    ├── ConfigEditor.exe          # 配置编辑器可执行文件（构建产物）
    ├── imgui.ini                 # Dear ImGui 布局配置
    │
    ├── common/                   # 公共头文件层
    │   ├── 前置头文件包含.h       #   预编译头（标准库 + 第三方库统一引入）
    │   ├── 引擎总头文件.h         #   引擎聚合头（一键包含常用模块）
    │   ├── external/             #   公共外部库封装
    │   │   └── Sol2/             #     sol类型别名.h / sol类型注册.h
    │   └── types/                #   全局类型定义
    │       ├── 对象类型.h         #     Object（对象基类）/ Prop（属性槽）
    │       ├── 事件类型.h         #     event + std::hash 特化
    │       ├── 坐标类型.h         #     坐标相关类型
    │       ├── 几何体类型.h       #     几何体相关类型
    │       └── 计时器类型.h       #     计时器相关类型
    │
    ├── src/                      # 引擎源码
    │   ├── core/                 # ★ 核心模块
    │   │   ├── collision/        #   碰撞系统
    │   │   │   ├── Collision_Agent/      #     碰撞代理器
    │   │   │   └── Collision_Processer/  #     碰撞处理器
    │   │   ├── entity/           #   实体系统
    │   │   │   ├── Entity/               #     实体（Lua 行为驱动）
    │   │   │   ├── Entity_Manager/       #     实体管理器（创建/卸载/行动）
    │   │   │   └── Prop_Distributor/     #     属性槽分发器
    │   │   ├── event/            #   事件系统
    │   │   │   ├── Event_Broker/         #     事件中转器（模块解耦中枢）
    │   │   │   └── Event_Terminal/       #     事件终端 + 终端接口
    │   │   └── space/            #   空间系统
    │   │       ├── Quadtree/             #     四叉树（空间划分）
    │   │       └── Quadtree_Manager/     #     四叉树管理器
    │   │
    │   └── tools/                # 工具模块
    │       ├── GUI/              #   图形界面工具
    │       │   └── Config_Editor/        #     配置编辑器（独立 exe）
    │       │       ├── 配置编辑器.h
    │       │       ├── 配置编辑器_内部工具.h
    │       │       ├── 配置编辑器主程序_外观.h
    │       │       ├── 实体配置模型.h
    │       │       ├── 实体配置模型_内部工具.h
    │       │       └── core/             #     编辑器核心实现
    │       └──           #   非图形工具
    │           ├── Auxi_Algorithm/       #     算法辅助（二分查找、路径字符串转换）
    │           ├── Config_Checker/       #     配置检查器
    │           ├── Config_Loader/        #     配置加载器
    │           ├── Engine_Env/           #     引擎环境
    │           ├── Logging/              #     日志系统
    │           ├── Number_Allocator/     #     数值分配器
    │           ├── Object_Pool/          #     对象池
    │           ├── Random/               #     随机数生成器
    │           └── Timer/                #     计时器
    │
    ├── 主调文件/                 # 引擎测试入口（main）
    │   └── 主调文件.cpp
    │
    ├── assets/                   # 资源与配置（数据驱动核心）
    │   ├── config/               #   JSON 配置
    │   │   ├── entities/         #     实体类型配置（6 个实体类型 + au.json）
    │   │   ├── property/         #     属性槽初始化配置（7 个）
    │   │   ├── format/           #     格式定义（Entity_Manager / Property_Manager）
    │   │   └── route/            #     路径/路由配置（entity / property）
    │   ├── scripts/              #   Lua 脚本
    │   │   ├── initialize/       #     实体初始化脚本
    │   │   └── behavior/         #     行为脚本
    │   └── UI/                   #   UI 资源（配置编辑器封面 / 立绘 / 帮助图）
    │
    └── external/                 # 第三方库（源码级引入）
        ├── Dear_ImGui/           #   Dear ImGui（imgui + backends）
        ├── glad/                 #   OpenGL 加载器
        ├── glfw/                 #   GLFW（含预编译 glfw3.lib）
        ├── glm/                  #   数学库（纯头文件）
        ├── Json/                 #   nlohmann/json
        ├── Lua/                  #   Lua 官方源码
        ├── Sol2/                 #   Sol2 头文件
        └── stb/                  #   stb 单头文件库
```

> 模块目录通行布局：`模块名/`（核心头文件 + `局部命名空间使用.h`）+ `模块名/core/`（实现细节）。文件命名遵循「中文文件名优先」约定。

---

## 核心架构

引擎采用「**事件中枢 + 依赖注入 + 对象池**」的结构。模块之间不直接互相持有引用，而是通过 `Event_Broker` 中转事件；需要跨模块取用的数据（如属性槽）则通过注入的 `std::function` 通道获取。

```
                     ┌──────────────────────────────────────────┐
                     │              Event_Broker                │
                     │            （事件中转器 / 中枢）           │
                     │  info_register(模块名, 关注事件, 入口)      │
                     │  receive(单事件 / 事件集) → 分发           │
                     └───────▲──────────────────────────▲───────┘
                             │ attach_entry             │ attach_entry
                  ┌──────────┴─────────┐      ┌─────────┴──────────┐
                  │  Prop_Distributor  │      │   Entity_Manager   │
                  │   （属性槽分发器）   │      │    （实体管理器）    │
                  └──────────┬─────────┘      └─────────┬──────────┘
                             │ prop_slots_bind          │ 持有
                             │  （注入取槽通道）          │
                             ▼                          ▼
                     Object_Pool<Prop>        Object_Pool<Entity>
                      （属性槽池）                （实体池）
                                                      │
                                                      ▼
                                             Object（对象基类）
                                              ├─ Prop（属性槽）
                                              └─ Entity（实体）

           ┌──────────────────┐
           │   Config_Loader  │  启动时读取 assets/config/ → 广播配置事件
           │  （配置加载器）    │  仅注入事件入口，不注入接入入口
           └──────────────────┘
```

### 启动流程（`主调文件/主调文件.cpp`）

1. 控制台切换 UTF-8 编码（`SetConsoleOutputCP(CP_UTF8)` / `SetConsoleCP(CP_UTF8)`）；
2. 构造**四个**核心对象：`Event_Broker`、`Config_Loader`、`Prop_Distributor`、`Entity_Manager`；
3. 由 `Event_Broker` 派生三个入口 lambda：
   - `attach_entry(name, events, event_entry)` → 转发给 `event_broker.info_register`，供模块登记订阅；
   - `event_entry(evt)` → 转发给 `event_broker.receive`，投递**单个**事件；
   - `event_set_entry(event_set)` → 转发给 `event_broker.receive`，投递**一批**事件；
4. **属性槽分发器**：注入接入入口 → `attach()` 接入事件中枢；
5. **实体管理器**：注入接入入口 + 单事件入口 + 多事件入口 → `attach()` 接入事件中枢；
6. 由 `Entity_Manager::prop_slot_get` 封装出属性槽绑定通道 `prop_bind_entry(distribute_key)`，交给 `Prop_Distributor::prop_slots_bind`；
7. **配置加载器**：注入单事件入口 + 多事件入口 → `act()` 加载 `assets/config/` 下全部配置并广播；
8. 进入 `for (;;)` 主循环（帧循环尚未接入）。

### 依赖注入模式

模块之间通过 **std::function 回调注入**解耦：

| 通道 | 类型 | 作用 |
| --- | --- | --- |
| `attach_entry` | `(name, vector<event>, function<void(shared_ptr<event>)>) → void` | 模块向事件中枢登记订阅（名称 + 关注的事件集 + 接收回调） |
| `event_entry` | `(shared_ptr<event>) → void` | 投递单个事件 |
| `event_set_entry` | `(vector<shared_ptr<event>>) → void` | 批量投递事件 |
| `prop_bind_entry` | `(const uint64_t&) → Object_Pool<Prop>*` | 以分发密钥换取属性槽池 |

各模块统一通过自身公开成员 `event_terminal` 发起接入与投递：`对象.event_terminal->attach_handler_register(入口)`、`对象.event_terminal->event_sender_register(入口)`。

---

## 核心模块详解

### 1. 实体系统（`src/core/entity/`）

#### 对象基类 `Object`（`common/types/对象类型.h`）

引擎内一切「有编号、有有效性」的事物的公共基类：

| 成员 | 类型 | 访问 | 说明 |
| --- | --- | --- | --- |
| `object_ID` | `uint64_t` | protected | 对象编号，初值 0 |
| `is_valid` | `bool` | protected | 有效性标记，初值 false |
| `ID_bind(ID)` | `void` | public | 绑定对象编号 |
| `ID()` | `uint64_t` | public | 获取对象编号 |
| `valid_set(bool)` | `void` | public | 设置有效性标记 |
| `valid()` | `bool` | public | 查询有效性标记 |

#### 属性槽 `Prop`（`common/types/对象类型.h`）

继承 `Object`，一组具名数值的容器：

| 成员 | 类型 | 访问 | 说明 |
| --- | --- | --- | --- |
| `property_slot` | `unordered_map<std::string, double>` | private | 属性槽本体 |
| `prop_get()` | `unordered_map<string,double>&` | public | 取属性槽引用 |

#### 实体 `Entity`（`Entity/实体.h`）

继承 `Object`，是可被 Lua 脚本驱动的活动对象：

| 成员 | 类型 | 访问 | 说明 |
| --- | --- | --- | --- |
| `entity_type` | `std::string` | private | 实体类型标签（如 `"Goblin"`） |
| `property_slot` | `unordered_map<string,double>*` | private | 通用属性槽指针（由实体管理器分配） |
| `event_terminal` | `Event_Terminal` | **public** | 该实体的事件终端 |
| `acl_key` | `int64_t` | private | 权限密钥（ACL 校验） |
| `action` | `LuaState` | private | 行为脚本运行时 |

公开接口：

| 接口 | 说明 |
| --- | --- |
| `Entity()` / `Entity(const int64_t& ID)` / `Entity(const int64_t& ID, const std::string& load_path)` | 三个构造函数 |
| `Entity(const Entity&)` / `operator=` | 拷贝已删除（禁用） |
| 移动构造 / 移动赋值 | `= default` |
| `~Entity()` | 析构函数 |
| `type()` | 获取实体类型标签 |
| `prop_slot_bind(ptr)` | 绑定属性槽指针 |
| `action_load(load_path)` | 加载行为脚本 |
| `act()` | **行为决策**（virtual），每帧由实体管理器调用 |

> 说明：行为脚本由 `action_load()` 加载，由 `act()` 驱动，两者均声明在 `Entity` 上。

#### 实体管理器 `Entity_Manager`（`Entity_Manager/实体管理器.h`）

引擎中所有实体与属性槽的「户籍管理处」：

| 数据 | 类型 | 说明 |
| --- | --- | --- |
| `event_map` | `unordered_set<event>` | 本模块订阅的事件集合 |
| `event_terminal` | `Event_Terminal` | 事件终端（public） |
| `acl_key` | `int64_t` | 事件发送权限密钥 |
| `distribute_key` | `optional<uint64_t>` | 属性槽分发密钥 |
| `prop_config_paths` | `unordered_map<string, LuaState>` | 类型 → 属性配置脚本运行时 |
| `action_load_path` | `unordered_map<string, string>` | 类型 → 行为脚本路径 |
| `props` | `Object_Pool<Prop>` | 属性槽池 |
| `entities` | `Object_Pool<Entity>` | 实体池 |

公开接口：

| 接口 | 说明 |
| --- | --- |
| `attach()` | 接入事件中转站 |
| `entity_build(type, counts)` | 按类型批量创建实体，返回新实体编号列表 |
| `entity_unload(IDs)` | 按编号批量卸载实体 |
| `entity_act(IDs)` | 指定实体执行行为决策 |
| `entity_act()` | 全部实体执行行为决策 |
| `distribute_key_gen()` | 生成属性槽分发密钥 |
| `prop_slot_get(distribute_key)` | 以密钥换取属性槽池 |
| `event_broadcast(evt)` | 事件广播 |
| `event_unicast(type, ID, evt)` | 事件定向发送（判返回是否送达） |

私有接口：`config_field_parse(config)`（配置字段检验）、`action_load_path_register(...)`、`prop_load_path_register(...)`、`event_process(evt)`（事件处理）。

#### 属性槽分发器 `Prop_Distributor`（`Prop_Distributor/属性槽分发器.h`）

属性槽的对外分发窗口。**这是原 `Property_Manager` 更名后的模块**，`assets/config/format/` 下仍保留旧的 `Property_Manager.json` 命名。

| 成员 / 接口 | 说明 |
| --- | --- |
| `props` | `Object_Pool<Prop>*`，指向实体管理器的属性槽池（private） |
| `event_terminal` | 事件终端（public） |
| `attach()` | 接入事件中转站 |
| `prop_slots_bind(bind_entry)` | 注入属性槽池取用通道 |
| `prop_slot_get(ID)` | 按对象编号取可写属性槽 |
| `const_prop_slot_get(ID)` | 按对象编号取只读属性槽 |
| `event_process(evt)` | 事件处理（private） |

### 2. 事件系统（`src/core/event/`）

#### 事件类型 `event`（`common/types/事件类型.h`）

```cpp
namespace engine {
    struct event {
        std::string sender_object{};   // 事件发起者
        std::string target_object{};   // 事件目标
        std::string category;          // 事件大类
        std::string tag;               // 类内标签
        nlohmann::json config;         // 配置包

        event() = default 形式;
        event(sender_object, target_object, category, tag, config);

        bool operator==(const event& other) const;   // 比较 category + tag + target_object + config
    };
}
```

- 事件为**单一结构体**：不设抽象基类，配置包 `config` 直接作为成员携带；
- `operator==` 比较 `category`、`tag`、`target_object`、`config` 四项，**不比较** `sender_object`；
- `std::hash<engine::event>` 已特化（组合 `category` / `tag` / `target_object` / `config.dump()`），因此事件可直接用作 `unordered_set` / `unordered_map` 的键；
- **相等范围与哈希范围严格一致**，这是事件能安全作为无序容器键的前提，改动其中一处务必同步另一处。

#### 事件终端 `Event_Terminal`（`Event_Terminal/事件终端.h`）

每个需要参与事件通信的模块/实体所持有的收发接口：

| 成员 / 接口 | 说明 |
| --- | --- |
| `terminal_interface` | `Terminal_Interface`，函数包装器与内存槽的集合（private） |
| `operator->()` | **终端接口快捷通道**，返回 `Terminal_Interface*`，使外部可写 `对象.event_terminal->xxx(...)` |
| `acl_key_gen()` | 生成权限密钥 |
| `attach(module_name, needed_events, acl_key)` | 接入事件中转站 |
| `check(module_name)` | 目标对象接入检查 |
| `call(module_name)` | 目标对象呼叫 |
| `build()` | 事件构造，返回 `shared_ptr<event>` |
| `send(evt, acl_key)` / `send(events, acl_key)` | 事件发送（单/多） |
| `receive(evt)` / `receive(events)` | 事件接收（单/多，存入本地事件集） |
| `query(acl_key)` | 查阅本地事件集 |
| `clear(acl_key)` | 清空本地事件集 |
| `operator()` 四个重载 | 转发到 `send` / `receive` |

#### 终端接口 `Terminal_Interface`（`Event_Terminal/终端接口.h`）

`Event_Terminal` 的私有成员，集中存放各类 `std::function` 包装器与内存槽；`Event_Terminal` 通过 `operator->` 把它开放给外部。所有 `*_register` 形式的入口注册方法都由它提供。

#### 事件中转器 `Event_Broker`（`Event_Broker/事件中转器.h`）

全局事件中枢，模块之间唯一的通信交汇点：

| 数据 | 类型 | 说明 |
| --- | --- | --- |
| `acl_set` | `unordered_map<string, vector<event_acl>>` | 模块名 → 订阅的事件标签及订阅者编号 |
| `mapping_set` | `unordered_map<string, int32_t>` | 模块名 → 订阅者编号 |
| `event_entries` | `unordered_map<int32_t, function<void(shared_ptr<event>)>>` | 订阅者编号 → 事件投递入口 |

公开接口：

| 接口 | 说明 |
| --- | --- |
| `info_register(module_name, needed_events, event_entry)` | 订阅者登记注册 |
| `target_object_check(module_name)` | 订阅者登记状态确认 |
| `receive(evt)` | 事件接收（单事件重载） |
| `receive(event_set)` | 事件接收（多事件重载） |

> 内部结构体 `event_acl { std::string tag; std::vector<int32_t> ID_set; }` 描述「某个事件标签被哪些订阅者关注」。

### 3. 空间系统（`src/core/space/`）

| 组件 | 文件 | 职责 |
| --- | --- | --- |
| `Quadtree` | `Quadtree/四叉树.h` | 四叉树节点结构与插入/查询算法；`四叉树通信结构体.h` 定义查询输入输出；`函数预声明.h` 声明算法接口 |
| `Quadtree_Manager` | `Quadtree_Manager/四叉树管理器.h` | 四叉树管理入口：整体划分、区域查询、对象管理；`四叉树管理器通信结构体.h` 定义管理通信协议 |

> 状态：算法实现约 2.8 千行，**当前尚无模块实例化四叉树**，属于已写好但未接线的模块。

### 4. 碰撞系统（`src/core/collision/`）

| 组件 | 文件 | 职责 |
| --- | --- | --- |
| `Collision_Agent` | `Collision_Agent/碰撞代理器.h` | 碰撞代理：为对象登记碰撞体，向处理器投递碰撞请求 |
| `Collision_Processer` | `Collision_Processer/` | 碰撞处理器：执行碰撞判定，产出碰撞结果 |

> 状态：骨架已就位，尚未接入实体系统与空间系统。

### 5. 非图形工具（`src/tools/`）

| 模块 | 职责 |
| --- | --- |
| `Auxi_Algorithm` | 算法辅助：`二分查找.h`、`路径字符串转换.h` |
| `Config_Checker` | 配置检查器：校验 JSON 配置字段合法性 |
| `Config_Loader` | 配置加载器：启动时加载 `assets/config/` 全部配置并广播事件 |
| `Engine_Env` | 引擎环境：路径、环境变量等运行环境信息 |
| `Logging` | 日志系统：引擎运行日志 |
| `Number_Allocator` | 数值分配器：为对象池等提供编号分配 |
| `Object_Pool` | 对象池：模板化的对象分配与回收容器 |
| `Random` | 随机数生成器：同时供事件终端的密钥生成使用 |
| `Timer` | 计时器：帧时间、倒计时等 |

---

## 构建与运行

### 环境要求

- **CMake** ≥ 3.20（顶层脚本使用了 `CONFIGURE_DEPENDS` 等特性）
- **C++20 编译器**：MSVC（Visual Studio 2022）或 GCC / Clang
- **OpenGL**（GLFW / GLAD 依赖）
- Windows 下需安装「使用 C++ 的桌面开发」工作负载（含 MSVC 工具集）

### 构建步骤

#### 方式一：Visual Studio（推荐，Windows）

1. 用 VS 打开仓库根目录（含 `CMakeLists.txt` 的目录）；
2. VS 会读取 `CMakeSettings.json`，自动生成 **x64-Debug（Ninja）** 配置；
3. 选择「生成全部」，产出：

| 目标 | 产物 | 位置 |
| --- | --- | --- |
| `EngineCore`（静态库） | `EngineCore.lib` | `out/build/x64-Debug/lib/` |
| `TestEngine`（可执行） | `TestEngine.exe` | `游戏引擎/` |
| `ConfigEditor`（可执行） | `ConfigEditor.exe` | `游戏引擎/` |

#### 方式二：命令行

```bash
cmake -S . -B out/build/x64-Debug -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build out/build/x64-Debug
```

#### 方式三：跨平台（GCC / Clang）

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

> 注：跨平台构建时需自行准备 GLFW 静态库（当前 `glfw3.lib` 为 Windows 预编译产物）。

### 运行

- **TestEngine**：引擎功能测试入口，启动后加载配置并进入主循环（当前为 `for(;;)` 空循环，等待帧循环接入）；
- **ConfigEditor**：独立配置编辑器（Dear ImGui 界面），用于可视化编辑实体 JSON 配置。

### 顶层 CMake 目标解析（`CMakeLists.txt`，253 行）

| 区块 | 内容 |
| --- | --- |
| §1 项目信息 | `project(游戏引擎 LANGUAGES C CXX)`；强制 C++20、`CMAKE_CXX_EXTENSIONS OFF` |
| §2 源文件收集 | `file(GLOB_RECURSE ... CONFIGURE_DEPENDS)` 收集 `游戏引擎/**/*.cpp` 与 `*.c`；排除构建目录、`主调文件/`、`src/tools/GUI/Config_Editor/` |
| §3 `EngineCore` | 静态库目标 |
| §4 头文件路径 | `PUBLIC` 传播 `游戏引擎/` 根路径 |
| §5 第三方库 | Sol2 / Lua / Json / GLFW / GLAD / external 根 / Dear ImGui(+backends) / stb 的 include 路径 |
| §6 链接 | `glfw3.lib` + `OpenGL::GL`；MSVC 追加 `opengl32 / user32 / gdi32 / shell32`；`GLFW_STATIC` 宏 |
| §7 编译选项 | MSVC：`/MP /utf-8`、`_CRT_SECURE_NO_WARNINGS`；GCC/Clang：`-Wall -Wextra -pedantic`、`_GNU_SOURCE` |
| §7 输出目录 | 库 → `out/build/<cfg>/lib`；可执行 → `游戏引擎/` |
| §8 安装规则 | 头文件（排除 `Private`）→ `include/EngineCore/`；库 → `lib/` |
| §9 `TestEngine` | 编译 `主调文件/*.cpp`，链接 `EngineCore`，设为 VS 默认启动项目 |
| §10 预留接口 | 注释保留 `add_subdirectory(Plugins/CombatProxy)` 插件扩展位 |
| §11 `ConfigEditor` | 编译 `src/tools/GUI/Config_Editor/core/*.cpp`，链接 `EngineCore` |

---

## 配置系统

### 配置目录（`assets/config/`）

| 目录 | 内容 |
| --- | --- |
| `entities/` | 实体类型定义（`type` / `acls` / `needed_events`），现含 6 个实体类型配置与 `au.json` |
| `property/` | 属性槽初始化路径（`type` / `initialize_path`），现含 7 个 |
| `format/` | 格式定义：`Entity_Manager.json`、`Property_Manager.json` |
| `route/` | 路径 / 路由配置：`entity.json`、`property.json` |

### 实体配置示例（`entities/哥布林 (Goblin).json`）

```json
{
  "type": "Goblin",
  "acls": {
    "master": "Goblin",
    "minion_set": []
  },
  "needed_events": [
    { "category": "Entity", "tag": "Request" },
    { "category": "config", "tag": "damage" }
  ]
}
```

| 字段 | 说明 |
| --- | --- |
| `type` | 实体类型标识（对应 Lua 初始化脚本命名） |
| `acls.master` | 从属权限：主实体类型 |
| `acls.minion_set` | 允许的从属实体类型集合 |
| `needed_events` | 该实体需要订阅的事件（`category` + `tag`） |

### 属性配置示例（`property/哥布林 (Goblin).json`）

```json
{
  "type": "Goblin",
  "initialize_path": "scripts/initialize/哥布林 (Goblin).lua"
}
```

配置加载器启动时读取全部 JSON → 构造 `event`（携带 JSON 配置包）→ 通过事件中转站广播 → 各模块按订阅响应。

> 命名提示：`format/Property_Manager.json` 使用的仍是旧模块名，实际模块为 `Prop_Distributor`。

---

## Lua 脚本系统

### 绑定层（`common/external/Sol2/`）

- `sol类型别名.h`：预定义 Sol2 库类型别名（如 `LuaState`）；
- `sol类型注册.h`：C++ 类型注册到 Lua 的方法封装。

### 两级脚本体系（`assets/scripts/`）

#### ① 初始化脚本（`initialize/`）

实体创建时由属性槽分发链路调用，向通用属性槽写入初始数值，并登记行为脚本路径。

```lua
-- scripts/initialize/哥布林 (Goblin).lua
function Goblin_Initialize(entity)
    local pros = entity.pros          -- 通用属性槽引用

    pros.max_hp = 100.0               -- 最大生命值
    pros.now_hp = 100.0               -- 当前生命值
    pros.attack_power = 12.0          -- 攻击力
    pros.defense = 3.0                -- 防御力
    pros.move_speed = 4.5             -- 移动速度
    pros.attack_range = 1.5           -- 攻击范围
    pros.attack_cooldown = 1.2        -- 攻击间隔（秒）
    pros.sight_range = 10.0           -- 索敌视野
    pros.state_giddy = 0.0            -- 眩晕状态计时
    pros.state_frozen = 0.0           -- 冰冻状态计时

    entity.behavior_script_path =
        "scripts/behavior/哥布林 (Goblin)_Behavior.lua"   -- 行为脚本登记
end
```

#### ② 行为脚本（`behavior/`）

由 `Entity::action_load()` 加载，作为实体的行为运行时，在 `act()` 每帧驱动下执行索敌、攻击、移动等决策逻辑。当前已有一份 `哥布林 (Goblin)_Behavior.lua`。

### 属性槽在 Lua 中的读写约定

- 属性名使用**小写下划线**命名（如 `max_hp`、`attack_power`）；
- 所有属性值为 `double`；
- 状态类属性约定为 `state_*` 前缀（`state_giddy`、`state_frozen`），数值表示剩余持续时间。

---

## 配置编辑器（GUI）

`ConfigEditor.exe` 是基于 **Dear ImGui + GLFW + OpenGL** 的独立图形化工具，用于可视化编辑实体配置：

| 文件 | 大小 | 说明 |
| --- | --- | --- |
| `配置编辑器.h` | 12.5 KB | 编辑器主界面与交互逻辑 |
| `实体配置模型.h` | 19.2 KB | 实体配置的数据模型 |
| `配置编辑器_内部工具.h` / `实体配置模型_内部工具.h` | 0.9 / 0.8 KB | 内部工具声明 |
| `配置编辑器主程序_外观.h` | 1.0 KB | 外观主题声明 |
| `core/` | 19 个 `.cpp` | 编辑器核心实现（由顶层 CMake 单独编译为独立 exe） |

`core/` 目录另存有 2 个 `.py` 脚本（`_拆分阶段2.py`、`_拆分阶段4外观.py`），为代码拆分过程中的辅助脚本，**不参与编译**。

ImGui 布局保存于 `游戏引擎/imgui.ini`；编辑器封面、立绘与帮助图取自 `assets/UI/`。

---

## 实现状态与已知问题

### 尚未完成

- **帧循环**：`main` 中为 `for (;;)` 空循环，未接入固定时间步与 `entity_act()` 驱动；
- **事件终端**：`check()` 与 `call()` 为目标对象接入检查/呼叫能力，当前实现尚不完整；
- **空间与碰撞**：算法已写好，但未与实体管理器接线；
- **脚本覆盖**：仅 `哥布林 (Goblin)` 具备初始化脚本与行为脚本，其余实体类型配置尚无对应 Lua。

### 已知问题

| 位置 | 现象 |
| --- | --- |
| `实体管理器.cpp` 的 `entity_build(type, counts)` | 返回语句位于循环体内，只创建首个实体；`counts` 为 0 时函数缺返回路径（C4715） |
| 实体创建事件分支 | 判断条件与实际语义相反，ID 集合非空时反而报「未定义创建数量」并驳回 |
| `Event_Terminal::attach` | 对未分配的 `shared_ptr` 解引用后赋值，属未定义行为 |
| `Event_Terminal::query` | 密钥不匹配时 `return {};`，函数签名返回引用，会产出悬垂引用（C4172） |
| `Object_Pool` 成员 | 实体管理器中的两个池缺少初始化点，调用其成员函数会解引用空指针 |
| include 路径大小写 | `引擎总头文件.h` 中的 `quadtree` / `quadtree_manager`、以及若干 `Non_Gui` 路径与实际目录名大小写不一致。**Windows 上无症状，Linux/macOS 上会直接编译失败** |
| `assets/config/route/*.json` | 路由中的模块名存在拼写错误（`Entity_Mangaer`） |

> 上表记录的是当前版次的真实状态，供接手者排障参考。

---

## 开发指南

### 如何新增一个实体类型

1. **编写实体配置**：在 `assets/config/entities/` 新建 `<类型名>.json`，声明 `type` / `acls` / `needed_events`；
2. **编写属性配置**：在 `assets/config/property/` 新建同名 JSON，指向初始化脚本路径；
3. **编写初始化脚本**：在 `assets/scripts/initialize/` 新建 `<类型名>.lua`，函数名约定为 `<类型名>_Initialize(entity)`，写入初始属性并登记行为脚本路径；
4. **编写行为脚本**（可选）：在 `assets/scripts/behavior/` 新建行为脚本，由初始化脚本登记路径；
5. 重新运行 `TestEngine`，`Config_Loader` 会自动加载新配置（无需重编译）。

### 如何新增一个引擎模块

1. 在 `src/core/` 或 `src/tools/` 下创建模块目录（含 `局部命名空间使用.h`、核心头文件、`core/` 实现目录）；
2. 若需要参与事件通信：持有公开成员 `Event_Terminal event_terminal`；
3. 在 `common/引擎总头文件.h` 中登记该模块的头文件；
4. 在 `主调文件.cpp` 中按「注册入口 → `attach()` 接入事件中枢 → 注入所需依赖通道」的顺序初始化；
5. 源码文件会被 CMake 的 `CONFIGURE_DEPENDS` 自动收集，直接构建即可。

### 代码风格约定

- 命名空间统一为 `engine`；
- 文件名 / 类名 / 注释使用中文（如 `实体.h`、`Prop_Distributor/` 目录）；
- 目录级命名约定：`模块名_Manager`（管理器）、`模块名_Broker`（中转器）、`模块名_Terminal`（终端）、`模块名_Distributor`（分发器）；
- 每个模块目录通常包含：`局部命名空间使用.h`（模块内 using 声明）、`core/`（实现细节）；
- 事件 `category` / `tag` 使用英文（如 `Entity` / `Request`）；
- **include 路径必须与实际目录大小写完全一致**（跨平台硬性要求）。

---

## 路线图

- [ ] **实体体系完善**：修正实体创建流程与事件分支判断，接通 `entity_act()` 驱动
- [ ] **对象池初始化**：为实体管理器中的属性槽池、实体池补齐分配点
- [ ] **事件终端补全**：实现 `check()` / `call()`，修复 `attach()` 的空指针解引用
- [ ] 主循环接入：将 `for(;;)` 空循环替换为帧循环（固定时间步 + 更新驱动）
- [ ] 渲染管线：接入 GLFW / GLAD / OpenGL 渲染循环
- [ ] 四叉树空间查询接入实体系统（索敌 / 碰撞候选）
- [ ] 碰撞处理器接线：`Collision_Agent` / `Collision_Processer` 与实体、空间系统联动
- [ ] 效应系统回归：从 `排除编译代码/effect/` 取回并改善后重新接入
- [ ] Lua 脚本覆盖：为全部实体类型补齐初始化脚本与行为脚本
- [ ] 配置编辑器增强：实体可视化编辑、属性预览、脚本关联
- [ ] 插件化模块接口（`add_subdirectory(Plugins/...)`）

---

## 许可证

[MIT License](LICENSE.txt)

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
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
```

---

*本文档依据仓库实际源码、构建脚本与资源目录逐一核对后编写，对应版次 `0b8dbd27`。接口与目录以源码为准，若发现不一致，请以源码为真。*
