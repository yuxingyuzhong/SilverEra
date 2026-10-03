# 白银纪元 · EngineSystem 层

EngineSystem 是「白银纪元」分层结构中的**玩法逻辑层**，位于 `Engine/EngineCore` 之上、`Application/Game` 之下。它把 EngineCore 提供的
「对象、事件、数据校验、资源路径」等基础能力，组合成三类游戏要素：**实体（Entity）**、**属性（Prop）**、
**效应（Effect）**。本层对外产出静态库 `EngineSystem.lib`，经 **Engine 聚合层**（摊平 EngineSystem 与 EngineCore 两面）供 `Application/Game` 与 `Application/Test` 链接。

本层是一个**独立的 git 仓库**，与 `Engine/EngineCore`、`Application/Game`、`Application/Test` 在同一目录树下并存（四个实体层各自独立仓库，顶层仓库只记录目录指针，**不再做源码快照**）。本层只依赖 EngineCore，并被 `Application/Game` 依赖，依赖方向单向；层与层之间**只链接
下层已构建好的静态库，绝不回退编译下层源码**。本层**不产出任何可执行文件**：全项目唯一的可执行入口
由 `Application/Test` 产出。

本层的三大子系统全部由**事件驱动**：模块之间不直接互相持有指针，而是各自持有事件终端（`Event_Terminal`），
通过 EngineCore 的事件中转器（`Event_Broker`）订阅与发布事件进行协作；实体的行为逻辑与效应的触发逻辑由
**Lua 脚本**承载，C++ 侧负责生命周期、事件路由与属性槽绑定。

---

## 一、分支与仓库信息

### 1.1 仓库与分支

| 项 | 值 |
| --- | --- |
| 远程仓库地址 | `https://github.com/yuxingyuzhong/SilverEra.git` |
| 远端名 | `sliverera` |
| 目标（上游）分支 | `system` |
| 本层本地当前分支 | `main` |
| 本地分支与上游分支的关系 | 本地 `main` 跟踪远端 `sliverera/system`（二者名字不同） |
| 推送命令 | 分支名不一致会被 `push.default=simple` 拦下，须显式推送 `git push sliverera main:system` |
| 仓库形态 | 本层为独立 git 仓库，`.git` 位于本层目录下 |
| 最近提交 | `52d3704 构建系统适配 Engine 聚合层：目录迁至 Engine/EngineSystem/、产物与目标改名 EngineSystem、接入 EngineCore 接口路径调整、include 全量改写为项目根相对全路径`（前序：`a3a85d4` README、`0e27d7b` 接入层间静态库契约、`0980ab4` 目录迁移落地、`257daf4` 仓库初始提交） |

### 1.2 本层在依赖链中的位置

「白银纪元」由四个实体层（各自独立仓库）＋ 一个聚合层组成，依赖方向单向：

```
EngineCore（EngineCore.lib） ──▶ EngineSystem（EngineSystem.lib） ──▶ ┬─▶ Application/Game（GameCore.lib，条件产出）
                                    ▲                                 └─▶ Application/Test（EngineTests.exe，全项目唯一可执行文件）
                                    │
                              Engine（聚合层：摊平 EngineSystem + EngineCore 两面，不产出库）
```

- EngineSystem **只依赖 EngineCore**；EngineCore 不反向依赖 EngineSystem。`Application/Game` 依赖本层；本层不反向依赖 Game。
- `Application/Test` **横跨**引擎两层，经 **Engine 聚合层一次接入**，是全项目唯一产出可执行文件的层。
- 层与层的交接一律通过「层间静态库契约」完成（见第七节），不通过 `add_subdirectory` 拉入下层源码。

### 1.3 本层产物

| 目标名 | 类型 | 产物 | 落点 |
| --- | --- | --- | --- |
| `EngineSystem` | STATIC | `EngineSystem.lib` | `Engine/EngineSystem/out/build/x64-Debug/lib/EngineSystem.lib` |
| `EngineSystemIncludeSnapshot` | CUSTOM（ALL） | 对外头快照 | `Engine/EngineSystem/out/build/x64-Debug/include/Engine/EngineSystem/**` |

- 本层**不产出任何 `.exe`**；配置编辑器虽带独立 `main()`，但当前未被 CMake 建立为可执行目标（见第十节）。
- 静态库落点由 `ARCHIVE_OUTPUT_DIRECTORY` 设为构建目录下的 `lib/`；这也是上层 `byjy_jieru_xiaceng()`
  自动探测时扫描的路径（`<该库所在层目录>/out/build/*/lib/<库名>.lib`）。

### 1.4 与其余各层的关系

| 层 | 方向 | 关系 |
| --- | --- | --- |
| EngineCore | 下游（本层依赖它） | 链接其已构建的 `EngineCore.lib`，使用 `Object`、`Object_Pool<T>`、`Event_Terminal`、`Event_Broker`、`detail`（`field_check` / `path_check` / `binary_search` 等）、`Engine_Env`、`logger` 等基础件 |
| Engine（聚合层） | 上游（消费本层对外面） | 本层 `cmake/对外接口.cmake` 被聚合层 `include`，其 `BYJY_ENGINE_SYSTEM_OUT_*` 被摊平为 `BYJY_ENGINE_*` |
| Application/Game | 上游（依赖本层） | 经 Engine 聚合层接入，取得 `EngineCore` + `EngineSystem` 两个库与全部包含路径（当前 `src/` 为空壳） |
| Application/Test | 横跨 | 经 Engine 聚合层接入，按两个子层快照编译，承载单元测试；当前用例集中于 EngineCore，本层用例尚未落位 |

### 1.5 许可证

本层目录下**没有独立的 LICENSE 文件**；许可随工程根，详见工程根 `LICENSE`。

---

## 二、本层职责与核心特性

### 2.1 三大子系统

| 子系统 | 目录 | 核心类型 | 解决的问题 |
| --- | --- | --- | --- |
| 实体子系统 | `src/entity/` | `Entity`、`Entity_Manager` | 「游戏对象」的创建、卸载、行为（决策树）驱动；实体是事件参与者与 Lua 行为脚本的宿主 |
| 属性子系统 | `src/prop/` | `Prop`、`Prop_Distributor` | 实体的数值面板（`unordered_map<string,double>` 属性槽）；把「属性槽集合」按分发密钥安全地借给效应系统 |
| 效应子系统 | `src/effect/` | `Prop_Effect`、`Effect_Manager` | 「技能/效果」的构建、分组、按执行阶段触发；效应以 Lua 脚本实现，修改实体的属性槽 |

### 2.2 核心特性

| 特性 | 说明 |
| --- | --- |
| 事件驱动架构 | 三个管理器（`Entity_Manager` / `Prop_Distributor` / `Effect_Manager`）均通过 `Event_Terminal` 接入EngineCore 层 `Event_Broker`，以 `Event` 为唯一协作媒介 |
| 数据与行为解耦 | 实体持有行为脚本（Lua 决策树）与属性槽**指针**；属性槽本体由 `Prop` 承载、由 `Entity_Manager` 统一管理 |
| 对象池托管 | `Object_Pool<Entity>`（实体池）与 `Object_Pool<Prop>`（属性槽池）统一分配与回收，两池同 ID 对齐 |
| Lua 脚本化 | 实体行为、效应逻辑全部由 Lua 脚本承载；Sol2 负责 C++/Lua 桥接，脚本可读写 `pros`、查阅 `event_set`、调用 `send()` |
| 权限控制 | 事件发送与查阅需 `acl_key`（事件终端生成的权限密钥）；属性槽池借用需 `distribute_key`（64 位随机分发密钥） |
| 配置驱动 | 实体类型与效应的行为/属性/订阅事件均从 JSON 配置加载，配置字段经EngineCore 层数据校验器校验后注册为加载路径 |
| 效应分组与优先级 | 效应按归属分组（`Effect_Group`），按执行阶段（`act_phase` 哈希）触发，按优先级（`priority` 降序）排序 |

---

## 三、技术栈

### 3.1 语言与标准

| 项 | 值 |
| --- | --- |
| 语言 | C++20（`CMAKE_CXX_STANDARD 20` + `CMAKE_CXX_STANDARD_REQUIRED ON`，`CMAKE_CXX_EXTENSIONS OFF`） |
| CMake 工程语言 | `project(EngineSystem LANGUAGES C CXX)`（比 EngineCore 多声明了 C —— Lua 源码为 C） |
| 命名空间 | `engine` |
| 源码风格 | 中文注释 + 中文类名/文件名（如 `实体.h`、`属性槽分发器.h` 所在的 `Prop_Distributor/` 目录） |

### 3.2 第三方依赖

本层 `external/` 下存有多份第三方库副本；其中**只有 Sol2 / Lua 进入对外面**，其余为配置编辑器预留。

| 库 | 用途 | 在本层的位置 | 是否进 `EngineSystem` |
| --- | --- | --- | --- |
| [Sol2](https://github.com/ThePhD/sol2) | C++ ↔ Lua 绑定层 | `external/Sol2/include` | 是（公共头暴露 `LuaState`，必须进对外包含目录） |
| [Lua](https://www.lua.org/) | 脚本语言运行时 | `external/Lua` | 是（Sol2 依赖的 Lua 解释器头） |
| [Dear ImGui](https://github.com/ocornut/imgui) | 即时模式 GUI | `external/Dear_ImGui`（imgui + backends） | 否（配置编辑器专用，当前被 CMake 排除） |
| [glad](https://glad.dav1d.de/) | OpenGL 函数加载 | `external/glad` | 否（同上） |
| [GLFW](https://www.glfw.org/) | 窗口与输入 | `external/glfw`（含 `glfw3.h`、`glfw3.lib`） | 否（由EngineCore 层对外面提供同一份能力） |
| [glm](https://github.com/g-truc/glm) | 数学库 | `external/glm` | 否（由EngineCore 层对外面提供） |
| [stb](https://github.com/nothings/stb) | 单头文件图像库 | `external/stb`（`stb_image.h`） | 否（仅配置编辑器直接引用） |

> 本层 `common/前置头文件包含.h` 是EngineSystem 层专有的预编译头：先整体包含EngineCore 层的 `common/前置头文件包含.h`
> （标准库 + `nlohmann/json` + GLFW + Windows 头），再补上 `<sol/sol.hpp>`。因为 Sol2 已由EngineCore 层搬到EngineSystem 层，
> EngineCore 层那份预编译头里没有它。

### 3.3 构建工具链

| 项 | 值 |
| --- | --- |
| 构建系统 | CMake ≥ 3.20（使用 `CONFIGURE_DEPENDS` 自动检测源文件增删） |
| 生成器 | Ninja |
| 编译器 | MSVC（Windows 已验证）；GCC / Clang 分支在 `CMakeLists.txt` 中保留 |
| Windows 配置 | Visual Studio「CMake 配置」：`x64-Debug`，生成器 Ninja，继承 `msvc_x64_x64` 环境 |
| 编译选项 | MSVC：`/MP /utf-8`，定义 `_CRT_SECURE_NO_WARNINGS`，全局 `/WX-`（不把警告当错误）；GCC/Clang：`-Wall -Wextra -pedantic`，定义 `_GNU_SOURCE` |
| 库输出目录 | `out/build/<config>/lib`（`ARCHIVE_OUTPUT_DIRECTORY` / `LIBRARY_OUTPUT_DIRECTORY`） |

---

## 四、目录结构

以下为到模块一级的真实目录树（省略 `out/`、`.vs/`、`.git/` 等构建与工具产物）：

```
Engine/EngineSystem/
├── CMakeLists.txt                    # 构建脚本：产出 EngineSystem 静态库
├── CMakeSettings.json                # VS 配置（x64-Debug / Ninja）
├── README.md                         # 本文档
├── .gitignore                        # 忽略 out/、构建产物、.vs/、.cyrene/ 等
├── cmake/
│   ├── 对外接口.cmake                # 本层对外面自描述（并入 EngineCore 对外面）
│   └── 接入下层.cmake                # 通用接入函数 byjy_jieru_xiaceng() + 头快照函数 byjy_tou_kuaizhao_gen()（四份逐字节相同之一）
├── common/
│   ├── 前置头文件包含.h              # EngineSystem 层预编译头（EngineCore 层预编译头 + sol/sol.hpp）
│   └── external/Sol2/
│       ├── sol类型别名.h             # LuaTable / LuaScript / LuaState 别名
│       └── sol类型注册.h             # JSON ↔ Lua 表互转（json_to_table / table_to_json）
├── external/                         # Dear_ImGui / glad / glfw / glm / Lua / Sol2 / stb 副本
├── assets/UI/                        # 配置编辑器图片资源（cyrene_cover.jpg / cyrene_help.png / cyrene_portrait.jpg）
└── src/
    ├── entity/
    │   ├── Entity/                   # 实体.h + 局部命名空间使用.h + core/实体.cpp
    │   └── Entity_Manager/           # 实体管理器.h + 局部命名空间使用.h + core/{配置处理,实体处理,事件处理,属性槽处理}.cpp
    ├── prop/
    │   ├── Prop/                     # 属性.h（纯头文件内联实现）
    │   └── Prop_Distributor/         # 属性槽分发器.h + 局部命名空间使用.h + core/属性槽分发器.cpp
    ├── effect/
    │   ├── Effect/                   # 效应.h + 局部命名空间使用.h + core/效应.cpp
    │   └── Effect_Manager/           # 效应管理器.h + 局部命名空间使用.h + core/效应管理器.cpp
    └── gui/
        └── Config_Editor/            # 配置编辑器（含独立 main，未接入静态库构建）
            ├── 配置编辑器.h / 实体配置模型.h / 配置编辑器_内部工具.h
            ├── 配置编辑器主程序_外观.h / 实体配置模型_内部工具.h
            └── core/                 # 18 个 .cpp + 2 个 .py 拆分脚本
```

**模块目录通行布局**：每个类一个 `<类名>.h` + 一个 `core/<类名>.cpp`（`Prop` 例外，纯头文件内联；
`Entity_Manager` 按职责拆成 4 个 `.cpp`）。`.cpp` 首行 `#include "../局部命名空间使用.h"`，
第二行 `#include "Engine/EngineCore/src/tools/Logging/日志系统运行包.h"`。

---

## 五、核心架构与运行链路

### 5.1 事件接入模式（attach → 订阅 → 分派）

```
模块.attach()
  │  ① event_terminal->event_receiver_register(λ(evt){ event_process(evt); })
  │  ② event_terminal.attach("<模块名>", needed_events, acl_key)
  ▼
Event_Broker（EngineCore 层中转站）——按订阅集合把事件路由给各模块的 event_process
  ▼
event_process(evt) —— 按 category/tag 分派到具体业务函数
```

| 模块 | 模块名（attach 第一参数） | 订阅集合 | 分派逻辑 |
| --- | --- | --- | --- |
| `Entity_Manager` | `"Entity_Manager"` | 动态（`event_map`，由配置事件填充） | `category=="Config"` → 配置解析；否则按 tag：Build / Unload / Act / 其他（unicast） |
| `Prop_Distributor` | `"Prop_Distributor"` | 空 `{}` | `category=="Key"` → 取分发密钥 |
| `Effect_Manager` | `"Effect_Manager"` | 4 类固定事件 | `category=="Effect"` 按 tag：Build / Unload / Act / 其他（定向） |

### 5.2 EngineSystem 层启动时序（准备期 → 配置期 → 运行期）

```
准备期（C++ 侧装配）
  em.attach() → ef.attach() → pd.attach() → ef.bind_entry_register(属性槽通道)
  → em.distribute_key_gen()（生成密钥并发送 Key 事件）→ pd 收到密钥
  → pd.prop_slots_bind(em.prop_slot_get 的适配)（密钥匹配时 props 指向 em 的属性槽池）
配置期（事件驱动）
  发送 Config 事件 → em：校验字段 → 注册行为/属性槽路径 → 填充 event_map → attach() 重新订阅
  发送 Config/Load 事件 → ef：配置效应脚本
运行期（事件驱动 + 直接调用）
  发送 Build 事件（target_type, counts）→ em.entity_build → 实体就绪
  em.entity_act(IDs) / em.entity_act()  → Entity::act → Lua decision() → 读写 pros / send 事件
  发送 Effect/Build → 效应构建 + 分组 + 排序；Effect/Act（act_phase）→ 按阶段触发
  发送 Effect/Unload（target_ID）→ 卸载效应；发送 Unload（ID_set）→ 实体与属性槽一起卸载
```

### 5.3 实体生命周期

```
entity_build(type, counts)：路径查表 → entities.build() × counts + props.build() × counts
  → 逐个初始化：清槽 → 注册事件发送入口 → prop_slot_bind(&prop) → action_load(path) → 返回 IDs
entity_act(IDs)    → entities.find(ID)->act() → Lua decision()
entity_unload(IDs) → entities.unload(IDs) + props.unload(IDs)
```

### 5.4 效应生命周期

```
Effect/Build  → effect_build：effect_set.build() → config_read（Lua 重置 + 加载 + 开库）
              → act_phase = hash(字符串) → priority 解析 → effect_object_bind(bind_entry(inclusion))
              → 分组二分（找不到则建组）→ 通知组内 → 入组 → sort_order_set(true, priority)
Effect/Act    → effect_act(hash(阶段)) → 阶段匹配 → pro_effect.effect_act() → Lua action()
Effect/Unload → 组内唯一则删组，否则通知组内并摘出 → effect_set.unload(target_ID)
```

---

## 六、模块详解

### 6.1 实体 `Entity`（`src/entity/Entity/实体.h`）

**功能**：可被 Lua 脚本驱动的活动对象。它继承EngineCore 层 `Object`（提供 `object_ID`、`is_valid`、
`ID()` / `ID_set()` / `valid()` / `valid_set()`），持有事件终端、属性槽指针与行为脚本运行时。

> 注意：基类 `Object` **属于EngineCore 层**（`src/core/object/Object/对象.h`），不在EngineSystem 层；EngineSystem 层只是继承它。

**对外接口**（关键签名）：

```cpp
class Entity : public Object
{
public:
    Entity(void);                                              //构造函数
    Entity(const int64_t& ID);                                 //构造函数
    Entity(const int64_t& ID, const std::string& load_path);   //构造函数（加载行为）
    Entity(const Entity&) = delete;                            //禁用拷贝
    Entity& operator=(const Entity&) = delete;
    Entity(Entity&&) = default;                                //允许移动
    Entity& operator=(Entity&&) = default;
    ~Entity();

    std::string type(void);                                    //类型信息获取
    void prop_slot_bind(std::unordered_map<std::string, double>* ptr);   //属性槽绑定
    void action_load(const std::string& load_path);            //行为加载
    virtual void act(void);                                    //行为决策
};
```

**内部实现要点**：

- 成员：`entity_type`（实体类型标签）、`property_slot`（属性槽**指针**，不拥有）、`event_terminal`（公开）、
  `acl_key`（权限密钥）、`action`（行为脚本 `LuaState`）。
- 拷贝被**禁用**（含不可拷贝的 `LuaState` 与外部指针）；移动为 `default`，使 `Object_Pool<Entity>` 内
  `std::vector<Entity>` 的扩容可行。
- 构造：`Entity(void)` 从 `event_terminal.acl_key_gen()` 生成密钥；`Entity(ID)` 委托默认构造后设 `object_ID`；
  `Entity(ID, load_path)` 再委托 ID 构造后 `action_load()`。
- `act()`：运行 `action["decision"]()`，随后 `event_terminal.clear(acl_key)` 清空本帧事件集合。
- `action_load()`：打开全部标准库 → `load_file` → 注册 `pros` / `event_set` / `send` 到 Lua（详见第九节）。

**涉及文件**：`实体.h`、`core/实体.cpp`、`局部命名空间使用.h`。

### 6.2 实体管理器 `Entity_Manager`（`src/entity/Entity_Manager/实体管理器.h`）

**功能**：引擎中所有实体与属性槽的「户籍管理处」——实体池 + 属性槽池，配置驱动创建/卸载/行动，
事件接入与分发。

**对外接口**（关键签名）：

```cpp
class Entity_Manager
{
public:
    Entity_Manager();                                    //构造函数
    ~Entity_Manager() = default;                         //析构函数
    void attach(void);                                   //事件中转站接入

    std::vector<uint64_t> entity_build(const std::string& type, const int& counts);  //实体创建
    void entity_unload(std::vector<uint64_t>& ID);       //实体卸载
    void entity_act(std::vector<uint64_t>& IDs);         //实体行动（指定）
    void entity_act(void);                               //实体行动（全量）
    bool distribute_key_gen(void);                       //属性槽分发密钥生成
    Object_Pool<Prop>* prop_slot_get(const uint64_t& distribute_key);  //属性槽获取（按密钥）

    void event_broadcast(std::shared_ptr<Event> evt);    //事件广播（向所有实体）
    bool event_unicast(const std::string& type, const uint64_t& ID,
        std::shared_ptr<Event> evt);                     //事件定向发送
};
```

**内部实现要点**：

- 数据：`event_map`（`unordered_set<Event>` 订阅集合）、`event_terminal`、`acl_key`、`distribute_key`、
  `prop_config_paths`（类型 → 属性槽配置脚本 `LuaState`，**存的是状态机而非路径**）、`action_load_path`
  （类型 → 决策树脚本路径）、`props`（`Object_Pool<Prop>`）、`entities`（`Object_Pool<Entity>`）。
- 实现按职责拆为 4 个编译单元：`core/配置处理.cpp`（构造 / attach / config_field_parse / 路径注册）、
  `core/实体处理.cpp`（entity_build / unload / act）、`core/事件处理.cpp`（broadcast / unicast / process）、
  `core/属性槽处理.cpp`（distribute_key_gen / prop_slot_get）。
- 配置事件：`config_field_parse` 校验 `type` / `prop_load_path` / `action_load_path` / `needed_events` 四字段；
  但注册时 `action_load_path_register` 实际读取 **`decision_load_path`**，`prop_load_path_register` 读取
  `prop_load_path` 并直接 `load_file`。
- 属性槽获取：`prop_slot_get(密钥)` 密钥未生成或不匹配返回 `nullptr`，匹配则返回 `&props`。

**涉及文件**：`实体管理器.h`、`core/配置处理.cpp`、`core/实体处理.cpp`、`core/事件处理.cpp`、`core/属性槽处理.cpp`。

### 6.3 属性槽 `Prop`（`src/prop/Prop/属性.h`）

**功能**：一组具名数值的容器（`std::unordered_map<std::string, double>`）；实体以**指针**引用属性槽，
属性槽本体由 `Entity_Manager` 统一分配。

**对外接口**：

```cpp
class Prop : public Object
{
public:
    std::unordered_map<std::string, double>& prop_get(void);   //属性槽获取（返回引用，可读写）
};
```

**内部实现要点**：`Prop` 是最轻量的容器类，无构造/析构自定义、无拷贝限制（内含可拷贝的 `unordered_map`），
`prop_get()` 为头文件内联实现。它继承 `Object` 才能进入 `Object_Pool<Prop>`（对象池有
`requires std::is_base_of_v<Object, T>` 约束）。

**涉及文件**：`属性.h`（纯头文件，无 `.cpp`）。

### 6.4 属性槽分发器 `Prop_Distributor`（`src/prop/Prop_Distributor/属性槽分发器.h`）

**功能**：属性槽的对外分发窗口——以分发密钥向外部提供属性槽池的读写通道。它**不继承 `Object`**
（无需进对象池），通过事件通道获取密钥、通过回调 `bind_entry` 拿到属性槽池，与 `Entity_Manager`
以「密钥 + 回调」模式协作，不直接持有对方实例。

**对外接口**（关键签名）：

```cpp
class Prop_Distributor
{
public:
    Prop_Distributor();                                  //构造函数
    ~Prop_Distributor();                                 //析构函数
    void attach(void);                                   //事件中转站接入
    void prop_slots_bind(std::function<Object_Pool<Prop>*
        (const uint64_t& distribute_key)> bind_entry);   //属性槽集合绑定（注入取池通道）
    std::unordered_map<std::string, double>* prop_slot_get(const uint64_t& ID);   //属性槽获取（可写）
    const std::unordered_map<std::string, double>* const_prop_slot_get(const uint64_t& ID) const;  //只读获取
};
```

**内部实现要点**：

- 数据：`props`（`Object_Pool<Prop>*`，借来的属性槽池指针）、`event_terminal`、`acl_key`、`distribute_key`。
- `attach()`：注册事件接收入口，并以空订阅集合 `attach("Prop_Distributor", {}, acl_key)` 接入中转站。
- `prop_slots_bind()`：若 `distribute_key` 未生成则警告并直接返回；否则 `props = bind_entry(distribute_key.value())`。
- `prop_slot_get()`：内部 `const_cast` 复用只读版本；`const_prop_slot_get()` 走 `props->find(ID)`，
  找到返回 `&prop->prop_get()`，否则 `nullptr`。
- `event_process()`：仅处理 `category=="Key"` 的密钥事件，取出 `config["key"]` 写入 `distribute_key`。

**涉及文件**：`属性槽分发器.h`、`core/属性槽分发器.cpp`、`局部命名空间使用.h`。

### 6.5 效应 `Prop_Effect`（`src/effect/Effect/效应.h`）

**功能**：Lua 驱动的效应——配置读取、数据注入、触发。效应以 Lua 脚本实现，通过绑定的属性槽指针
修改作用对象的数值。

**对外接口**（关键签名）：

```cpp
class Prop_Effect
{
public:
    Prop_Effect();                                       //构造函数
    ~Prop_Effect();                                      //析构函数
    Prop_Effect(Prop_Effect&&) = default;                //允许移动（含 LuaState，拷贝被隐式删除）
    Prop_Effect& operator=(Prop_Effect&&) = default;

    bool config_read(const nlohmann::json& config);      //配置读取
    void data_injection(void);                           //数据注入
    void ID_bind(const uint64_t& ID);                    //编号绑定（幂等）
    void effect_object_bind(std::unordered_map<std::string, double>* object);   //作用对象绑定
    const std::string& effect_name_get(void);            //效应名称获取
    void effect_act(void);                               //效应触发
};
```

**内部实现要点**：

- 成员：`inclusion`（效应归属，所属实体 ID）、`ID`（`std::optional<uint64_t>` 效应编号）、`name`、
  `script`（Lua 脚本实例）、`effect_object`（作用对象属性槽指针）、`event_terminal`、`acl_key`。
- `config_read()`：校验 `path` / `inclusion` / `name` 三个字段 → `Engine_Env::absolute_path_get` 绝对路径化
  → `path_check` → 重置 `script = LuaState{}` → `load_file` → 打开 base/math/string/table 四个标准库；
  失败返回 `false`。
- `data_injection()`：将 `inclusion` / `ID` / `name` / `effect_object`（`sol::as_table`）注入 Lua，
  注册 `event_set` 与 `send()`；ID 未绑定时 `Log::error` 后 `return`。
- `effect_act()`：执行 `script["action"]()`（要求 Lua 脚本定义 `action` 函数）。

**涉及文件**：`效应.h`、`core/效应.cpp`、`局部命名空间使用.h`。

### 6.6 效应管理器 `Effect_Manager`（`src/effect/Effect_Manager/效应管理器.h`）

**功能**：按归属（`inclusion`）分组管理效应记录，按执行阶段（`act_phase`）触发，按优先级（`priority` 降序）排序。

**对外接口**（关键签名）：

```cpp
class Effect_Manager
{
public:
    Effect_Manager();                                    //构造函数
    ~Effect_Manager() = default;                         //析构函数
    void bind_entry_register(std::function<std::unordered_map<std::string, double>*
        (const uint64_t& ID)> bind_entry);               //注册属性槽绑定通道
    void attach(void);                                   //事件中转站接入
};
```

**内部实现要点**：

- 内部结构体：`Effect_Record : public Object`（含 `inclusion` / `act_phase` / `priority` / `pro_effect`，
  可进对象池）；`Effect_Group`（含 `inclusion` 与 `vector<Effect_Record*> effects`，不进对象池）。
- 数据：`effect_groups`（按 `inclusion` 升序，供二分）、`effect_set`（`Object_Pool<Effect_Record>`，按 `priority`
  降序）、`bind_entry`（属性槽绑定通道）、`event_terminal`、`acl_key`。
- `attach()`：订阅 4 类事件——`Config/Load`、`Effect/Build`、`Effect/Unload`、`Effect/Act`。
- `effect_build()`：校验 `inclusion` / `act_phase` / `priority` → 池内建记录 → `config_read`（失败回滚）
  → 记录归属、绑定 ID → `act_phase = hash<string>{}(字符串)` → 解析 `priority`（`"max"` → `uint64` 最大值，
  其余字符串告警回滚）→ 经 `bind_entry` 绑定作用对象 → 注册事件发送入口 → 二分分组（找不到则建组）
  → 把带 ID 的修饰事件通知组内已有效应后入组 → `effect_set.sort_order_set(true, &Effect_Record::priority)`。
- `effect_unload()`：组内唯一 → 删整组；否则通知组内其它效应（携 `inclusion` / `name` 的卸载事件）后摘出，
  最后 `effect_set.unload(target_ID)`。
- `effect_act(phase)`：遍历 `effect_set.data()`，对 `valid()` 且 `act_phase == phase` 的记录调用 `pro_effect.effect_act()`。
- `effect_group_seek()`：以 `binary_search(effect_groups, inclusion, less(), &Effect_Group::inclusion)` 查分组索引。

**涉及文件**：`效应管理器.h`、`core/效应管理器.cpp`、`局部命名空间使用.h`。

### 6.7 配置编辑器 `Config_Editor`（`src/gui/Config_Editor/`）

基于 Dear ImGui + GLFW + OpenGL 的图形化配置编辑器，含独立 `int main(void)`（见第十节）。模块清单与文件规模：

| 文件 | 大小 | 说明 |
| --- | --- | --- |
| `配置编辑器.h` | 12.5 KB | 编辑器主界面与交互逻辑（`配置编辑器` 类，方法 `加载()` / `渲染()` / `请求关闭窗口()` / `应关闭窗口()`） |
| `实体配置模型.h` | 19.2 KB | 实体配置的数据模型 |
| `配置编辑器_内部工具.h` | 0.9 KB | 内部工具声明 |
| `实体配置模型_内部工具.h` | 0.7 KB | 内部工具声明 |
| `配置编辑器主程序_外观.h` | 0.9 KB | 外观主题声明（`应用粉色主题()` / `加载中文字体()` / `渲染梦幻背景()`） |
| `core/` | 18 个 `.cpp` + 2 个 `.py` | 编辑器核心实现与拆分遗留脚本 |

---

## 七、对外接口契约

本层通过两份 CMake 脚本接入「层间静态库契约」。

### 7.1 `cmake/对外接口.cmake`——本层对外面自描述

本层对外面 = **本层自有面 + 下层（EngineCore）对外面**，通过首行 `include` 并入 EngineCore 的 `BYJY_ENGINE_CORE_OUT_*`：

```cmake
include("${CMAKE_CURRENT_LIST_DIR}/../../EngineCore/cmake/对外接口.cmake")
get_filename_component(BYJY_ENGINE_SYSTEM_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)

set(BYJY_ENGINE_SYSTEM_OUT_LIB "EngineSystem")
set(BYJY_ENGINE_SYSTEM_OUT_LIBDIR "${BYJY_ENGINE_SYSTEM_ROOT}")
set(BYJY_ENGINE_SYSTEM_OUT_INC
    "${BYJY_ENGINE_SYSTEM_ROOT}/external/Sol2/include"
    "${BYJY_ENGINE_SYSTEM_ROOT}/external/Lua"
    ${BYJY_ENGINE_CORE_OUT_INC}
)
set(BYJY_ENGINE_SYSTEM_OUT_DEF  ${BYJY_ENGINE_CORE_OUT_DEF})
set(BYJY_ENGINE_SYSTEM_OUT_LINK ${BYJY_ENGINE_CORE_OUT_LINK})
set(BYJY_ENGINE_SYSTEM_OUT_SYS  ${BYJY_ENGINE_CORE_OUT_SYS})
```

六个契约变量取值与含义（变量名与含义是层间契约的一部分，改名必须同步上层）：

| 变量 | 取值 | 含义 |
| --- | --- | --- |
| `BYJY_ENGINE_SYSTEM_OUT_LIB` | `EngineSystem` | 本层静态库名（`OUT_LIB` 自本版次起为**分号列表**形态，本层仍单元素） |
| `BYJY_ENGINE_SYSTEM_OUT_LIBDIR` | `Engine/EngineSystem`（本层根） | 与 `OUT_LIB` 一一对应的「该库所在层目录」，供上层自动探测（**本版次新增**） |
| `BYJY_ENGINE_SYSTEM_OUT_INC` | `external/Sol2/include`、`external/Lua`，再接 EngineCore 对外面（其首项为**项目根**） | 使用本层公共头文件所需的包含目录 |
| `BYJY_ENGINE_SYSTEM_OUT_DEF` | 承自 EngineCore（`GLFW_STATIC`） | 使用本层公共头文件所需的编译定义 |
| `BYJY_ENGINE_SYSTEM_OUT_LINK` | 承自 EngineCore（`.../glfw/glfw3.lib`） | 本层对外传递的第三方库 |
| `BYJY_ENGINE_SYSTEM_OUT_SYS` | 承自 EngineCore（`opengl32;user32;gdi32;shell32`） | 本层对外传递的系统库 |

> **本层不再声明自身层根与 `src/` 两个包含目录**（本版次起）：项目内部一律写「项目根相对全路径」（`Engine/EngineSystem/src/...`），由 EngineCore 对外面首项「项目根」统一解析。

**为什么 Sol2 / Lua 路径必须进对外包含目录**：本层公共头文件 `Engine/EngineSystem/src/entity/Entity/实体.h` 与
`Engine/EngineSystem/src/effect/Effect/效应.h` 对外暴露 `LuaState`（= `sol::state`），`Engine/EngineSystem/common/external/Sol2/` 下的包装头也直接
使用 `sol::` 类型；「使用本层公共头文件」必然要解析 `LuaState`，故这两条路径是硬性要求。`EngineSystem`
自身编译用的是同一份声明。

**未列入的第三方目录**：本层 `external/` 下的 Dear_ImGui、glad、glfw、glm、stb 目前不被 `EngineSystem`
源文件直接引用（唯一直接引用 `stb_image.h` 的 `src/gui/Config_Editor/` 含独立 `main()`，已排除在静态库之外；
glfw / glm 由EngineCore 层对外面提供同一份能力）。将来把配置编辑器接进来时需要一起补上其包含路径。

### 7.2 `cmake/接入下层.cmake`——通用接入函数

提供两个函数：`byjy_jieru_xiaceng(<下层对外接口文件> <变量前缀> <覆盖库路径变量名>)` 把**下层已构建好的静态库**接进当前层；`byjy_tou_kuaizhao_gen(<结果变量> <导入目标>)` 由导入目标反推下层的**头快照根**。本层调用：

```cmake
byjy_jieru_xiaceng(
    "${PROJECT_ROOT_DIR}/../EngineCore/cmake/对外接口.cmake"
    "BYJY_ENGINE_CORE"
    "BYJY_ENGINE_CORE_LIB_PATH"
)
```

**库定位三级策略**（任何情况下都不回退编译下层源码）：

1. **覆盖变量优先**：`BYJY_ENGINE_CORE_LIB_PATH` 非空且文件存在 → 用它；非空但不存在 → `FATAL_ERROR`，不静默降级。
2. **自动探测**：覆盖变量为空 → 扫 `Engine/EngineCore/out/build/*/lib/EngineCore.lib`，多个候选取时间戳最新的一份。
3. **硬失败**：仍无 → `FATAL_ERROR`，并给出先构建下层的命令。

随后以下层库名为目标名建立 `IMPORTED STATIC GLOBAL` 目标（目标名即 `EngineCore`），把包含目录 / 编译定义 /
链接库挂到其 `INTERFACE` 属性上，供本层向上继续传递。本文件在本层 / **Engine 聚合层** / Test / Game 各一份，
**四份逐字节相同**，改动需同步四处（EngineCore 无下层，故无此文件）。

### 7.3 覆盖变量

| 变量 | 归属 | 类型 | 默认 | 说明 |
| --- | --- | --- | --- | --- |
| `BYJY_ENGINE_CORE_LIB_PATH` | 本层（消费方，定位 EngineCore 库） | `FILEPATH` | `""` | EngineCore 静态库路径覆盖；留空则自动探测 `Engine/EngineCore/out/build/*/lib/EngineCore.lib` |
| `BYJY_ENGINE_LIB_PATH` | 上层（Engine 聚合层 / Test / Game，消费方，定位引擎两个库） | `STRING` | `""` | **分号列表**，顺序同 `OUT_LIB`（`EngineSystem;EngineCore`）；一经非空须与库数**等长**，不再自动探测 |

---

## 八、构建

### 8.1 前置条件

**必须先构建 EngineCore**（产出 `EngineCore.lib` 与其对外头快照），本层链接的是已构建的 EngineCore 静态库：

```bash
cmake -S Engine/EngineCore -B Engine/EngineCore/out/build/x64-Debug -G Ninja
cmake --build Engine/EngineCore/out/build/x64-Debug
```

### 8.2 构建本层

```bash
cmake -S Engine/EngineSystem -B Engine/EngineSystem/out/build/x64-Debug -G Ninja
cmake --build Engine/EngineSystem/out/build/x64-Debug
```

- **配置期须处于 UTF-8 代码页**（先进 VS 开发人员环境，再 `chcp 65001`），否则头文件依赖不会被记录，改头文件永不触发重编；`TMP`/`TEMP` 须为**纯 ASCII** 路径（详见顶层 README 的构建说明）。
- 本层**按 EngineCore 的对外头快照编译**，不再实时读取 EngineCore 源码树：配置期由 `byjy_tou_kuaizhao_gen(BYJY_ENGINE_CORE_SNAPSHOT EngineCore)` 反推 `<EngineCore 构建目录>/include`，并把它作为**本层自身包含目录**、排在项目根**之前**（否则 `Engine/...` 会命中源码树、快照被旁路）。因此「改了 EngineCore 源码头但没重建 EngineCore」时本层不会看到新头（正是为了避免新头配旧库）；要让新头生效，先构建 EngineCore。EngineCore 快照缺失会在配置期直接 `FATAL_ERROR`。
- 本层同时向 `<本层构建目录>/include` **导出自身的对外头快照**（`EngineSystemIncludeSnapshot` 常驻目标，镜像 `Engine/EngineSystem/common` 与 `Engine/EngineSystem/src`；第三方头不经快照），供上层按「与 `EngineSystem.lib` 同版次」的头编译。

- 源文件由 `GLOB_RECURSE ... CONFIGURE_DEPENDS` 自动收集 `src/entity/`、`src/prop/`、`src/effect/` 下的
  `.cpp` / `.c`，并依次排除：① 构建目录下的文件；② `Tests/` 目录；③ `src/gui/Config_Editor/`（含独立 `main()`）。
- **增删文件自动重配（本版次）**：`src/entity`、`src/prop`、`src/effect` 三个 GLOB 基点目录均登记进 `CMAKE_CONFIGURE_DEPENDS`；增删文件后**直接构建**即自动重配并重扫，不必手动「重新生成缓存」。
- 若收集结果为空，`CMakeLists.txt` 以 `FATAL_ERROR` 报错终止。
- `EngineSystem` 的 **PUBLIC 面直接取自 `cmake/对外接口.cmake`**（唯一真源），使包含目录/编译定义/链接库
  漏写或写错在构建期立刻暴露。
- 链接面：`target_link_libraries(EngineSystem PUBLIC EngineCore ${BYJY_ENGINE_SYSTEM_OUT_LINK} ${BYJY_ENGINE_SYSTEM_OUT_SYS})`。
- 若 EngineCore 库缺失，配置期即报 `接入下层：找不到 EngineCore 的静态库 EngineCore.lib`，并按提示给出构建命令。
  可用 `-DBYJY_ENGINE_CORE_LIB_PATH=<绝对路径>` 显式指定；一旦非空即不再自动探测。

### 8.3 产物

| 产物 | 落点 |
| --- | --- |
| `EngineSystem.lib` | `Engine/EngineSystem/out/build/x64-Debug/lib/EngineSystem.lib` |

---

## 九、Lua 集成

### 9.1 绑定层 `common/external/Sol2/`

| 文件 | 内容 |
| --- | --- |
| `sol类型别名.h` | 在 `engine` 命名空间内定义别名：`LuaTable = sol::table`、`LuaScript = sol::function`、`LuaState = sol::state` |
| `sol类型注册.h` | JSON ↔ Lua 表互转：`json_to_table(lua, json)` 把 `nlohmann::json` 递归转成 `sol::table`；`table_to_json(lua, table, empty_as_array)` 反向转换（识别整数连续键为数组） |

- 两个头文件均只 `#include "Engine/EngineCore/common/前置头文件包含.h"` 后直接使用 `sol::` 类型，因此「谁提供 Sol2」由
  EngineSystem 层预编译头兜住。
- 原 `register_event`（把 C++ 的 `Event` 类型注册到 Lua）已从 `sol类型注册.h` 移除，`实体.cpp` 与
  `效应.cpp` 中留有 TODO 注释——脚本当前无法直接构造 `Event` 对象，只能通过 `send` 接收已构造的
  `shared_ptr<Event>`。

### 9.2 实体行为脚本的调用方式（`Entity::action_load()`）

| Lua 符号 | C++ 来源 | 类型 | 用途 |
| --- | --- | --- | --- |
| `pros` | `property_slot` | `sol::as_table`（属性槽表引用） | 脚本读写实体属性 |
| `event_set` | `event_terminal.query(acl_key)` | 事件集合引用 | 脚本查阅本实体收到的事件 |
| `send` | `event_terminal.send({evt}, acl_key)` | Lua 函数 | 脚本发送事件（转发到外部） |
| `decision` | 脚本自身定义 | Lua 函数 | `act()` 调用的决策入口（脚本必须定义，否则 Sol2 抛异常） |

`Entity::act()` 每帧执行路径：`action["decision"]()` → `event_terminal.clear(acl_key)`。

> 边界：`action.set("pros", sol::as_table(property_slot))` 要求 `property_slot` **已非空**；若构造后未
> `prop_slot_bind` 就 `action_load`，`as_table(nullptr)` 行为未定义。`Entity_Manager` 路径先绑后加载，是安全的。

### 9.3 效应脚本的调用方式（`Prop_Effect::data_injection()`）

| Lua 符号 | 来源 | 用途 |
| --- | --- | --- |
| `inclusion` | `inclusion` | 效应归属（所属实体 ID） |
| `ID` | `ID.value()` | 效应编号 |
| `name` | `name` | 效应名称 |
| `effect_object` | `sol::as_table(*effect_object)` | 效应作用对象（可写属性槽表） |
| `event_set` | `event_terminal.query(acl_key)` | 事件集合引用 |
| `send` | `event_terminal.send(evt, acl_key)` | 事件发送函数 |

`Prop_Effect::effect_act()` 执行 `script["action"]()`（要求 Lua 脚本定义 `action` 函数）。

### 9.4 属性槽在 Lua 中的读写约定

- 属性名使用**小写下划线**命名（如 `max_hp`、`attack_power`）；所有属性值为 `double`。
- 状态类属性约定为 `state_*` 前缀（如 `state_giddy`、`state_frozen`），数值表示剩余持续时间。

> 说明：具体的实体类型配置 JSON、属性槽初始化脚本与行为脚本属于**上层（Game 层）的资源**，不在本层目录内；
> 本层只提供「配置驱动的加载路径注册」与「脚本运行时符号暴露」这套机制。

---

## 十、配置编辑器（GUI）

### 10.1 现状

`src/gui/Config_Editor/` 是一套完整的 ImGui 配置编辑器（由EngineCore 层迁入本层），当前**含独立 `main()`、未接入构建**。

- 入口：`core/配置编辑器主程序.cpp` 第 49 行 `int main(void)`——初始化 GLFW 窗口 + OpenGL 上下文 →
  初始化 Dear ImGui（GLFW + OpenGL3 后端）→ 载入中文字体与粉色主题 → 运行主循环并驱动编辑器界面。
  含独立 `main` 是其与静态库/Test 层入口互斥的原因。
- 模块：`配置编辑器.cpp`（主窗口 / 主界面 / 实体面板 / 模块面板 / 格式管理 / 保存 / 图片 / 内部工具 / 视觉）、
  `实体配置模型_*.cpp`（实体 / 属性槽 / 格式 / 通用配置 / 图片 / 内部工具）。
- 依赖：Dear_ImGui、glad、stb_image（本层 `external/` 下副本）。
- 拆分遗留：`core/_拆分阶段2.py`、`core/_拆分阶段4外观.py`（代码拆分过程的 Python 辅助脚本，非源码，不参与编译）。
- 资源：编辑器封面 / 立绘 / 帮助图取自本层 `assets/UI/`。

### 10.2 构建状态与接入缺口

`CMakeLists.txt` 的过滤规则 `list(FILTER SYSTEM_SOURCES EXCLUDE REGEX "/src/gui/Config_Editor/")` 将其整体
排除在 `EngineSystem` 之外；**没有为其建立独立 `add_executable`**，因此本层当前不产出 `ConfigEditor.exe`。
接入时需要补齐：

1. 在 `Engine/EngineSystem/CMakeLists.txt` 追加 `add_executable` 并链接 `EngineSystem`；
2. 把 `Engine/EngineSystem/external/` 下 Dear_ImGui、glad、glfw、glm、stb 对应的包含路径补进 `cmake/对外接口.cmake`；
3. 处理 `main()` 与其它入口的互斥关系。

---

## 十一、实现状态与已知问题

### 11.1 已完成

- 已接入层间静态库契约（`对外接口.cmake` + `接入下层.cmake`），可独立配置并构建出 `EngineSystem.lib`。
- 实体、属性、效应三模块的全部源码已纳入 `EngineSystem` 编译（`src/entity/`、`src/prop/`、`src/effect/`）。
- 三个管理器的事件接入与分派骨架完整：`Entity_Manager`（动态订阅）、`Prop_Distributor`（Key 事件）、
  `Effect_Manager`（4 类固定事件）。
- Lua 绑定层（Sol2 别名与 JSON 互转）与实体/效应两级的脚本运行时符号暴露已就位。

### 11.2 尚未完成

- 配置编辑器未接入构建（见第十节）。
- 本层当前无独立用例；单元测试由Test 层承载（链接本层已构建的静态库），EngineSystem 层用例尚未落位。
- `register_event`（C++ 事件类型注册到 Lua）已移除，脚本尚不能直接构造事件对象。

### 11.3 已知问题

| # | 位置 | 现象 |
| --- | --- | --- |
| 1 | `core/实体处理.cpp` `entity_build` | `return IDs;` 位于初始化循环**体内**，`counts > 1` 时只有第一个实体完成初始化（绑槽、加载决策树），其余实体处于半成品状态 |
| 2 | `core/实体处理.cpp` `entity_build` | 实体池与属性槽池各自维护独立分配器，**没有 ID 对齐校验**，若单独回收过 `props` 可能错位 |
| 3 | `core/事件处理.cpp` `event_process`(Build) | `if (!ID_set.empty())` 判定疑似写反：构建成功（非空）反而报「未定义创建数量」并驳回，失败（空）却继续广播 |
| 4 | `core/事件处理.cpp` `event_process`(Act) | Act 分支只做 `field_check<vector<int64_t>>(config, "ID_set")` 校验，**没有调用 `entity_act`**，实体行动事件不驱动任何实体 |
| 5 | `实体.cpp` / `core/事件处理.cpp` | `entity_type` 无写入路径（恒为空串），`event_unicast` 的 `it->type() == type` 过滤因此失效 |
| 6 | `core/属性槽处理.cpp` `distribute_key_gen` | 返回类型 `bool` 但函数体无 `return` 语句（C4715 告警，行为未定义） |
| 7 | `core/效应管理器.cpp` `event_process`(Build) | `optional<uint64_t> effect_ID = effect_build(evt)` 返回值被丢弃，调用方拿不到新效应 ID |
| 8 | `效应管理器.h` `Effect_Group.effects` | 存的是**裸指针**；`effect_set.build()` 触发 `std::vector` 扩容会使旧 `Effect_Record` 地址失效，分组内指针可能悬垂 |
| 9 | `core/属性槽分发器.cpp` vs `core/属性槽处理.cpp` | 发送端 `Entity_Manager` 构造的事件 tag 为 `"Distribute"`，接收端 `Prop_Distributor` 判断的是 `tag == "Distributor"`，**两者不一致**，密钥事件可能无法被接收 |
| 10 | 4 个头文件的 include | **已关闭**：`实体管理器.h`、`属性槽分发器.h`、`效应.h`、`效应管理器.h` 的 include 已随EngineCore 层工具重组改为 `src/tools/Detail/package/数据校验工具.h` 与 `src/tools/Detail/二分查找.h`，源码内 `field_check<T>` / `path_check` / `binary_search` 调用点已全部改为 `engine::detail::` 命名空间自由函数（其中 `binary_search` 语义由「返回 `-1`」改为返回 `std::optional`，未命中为 `std::nullopt`） |

> 提示：上表第 1～5 项是**代码行为事实**（逐行核对源码得出），不代表模块设计意图；重开发时应以「设计意图」为准修复。

---

## 十二、开发指南

### 12.1 如何新增一个实体类型

1. 提供实体类型配置 JSON，声明 `type`、`prop_load_path`、`action_load_path`、`needed_events` 字段
   （注意：`config_field_parse` 校验的是 `action_load_path`，而 `action_load_path_register` 实际读取的是
   `decision_load_path`；`event_process` 读取目标类型用的是 `target_type` 而非 `type`——配置需同时提供这些字段）。
2. 提供属性槽配置脚本（由 `prop_load_path` 指向），`prop_load_path_register` 会直接 `load_file`。
3. 提供行为脚本（由 `decision_load_path` 指向），脚本内必须定义 `decision` 函数。
4. 发送 `Config` 事件（`category=="Config"`），`Entity_Manager` 会自动注册路径并填充订阅集合（无需重编译）。
5. 发送 `Build` 事件（`target_type` + `counts`）创建实体。

> 上述配置与脚本文件属于上层资源，不在本层目录内；本层只负责按路径加载。

### 12.2 如何新增一个效应

1. 提供效应 Lua 脚本（定义 `action` 函数），供 `path` 字段指向。
2. 发送 `Effect/Build` 事件，`config` 需含：`path`（脚本相对路径）、`inclusion`（所属实体 ID）、
   `name`、`act_phase`、`priority`（`"max"` 或整数）。
3. 触发：发送 `Effect/Act` 事件（`act_phase` 按**字符串**校验并哈希）；卸载：发送 `Effect/Unload` 事件（`target_ID`）。
4. 需保证 `bind_entry` 已通过 `bind_entry_register` 注入，否则 `effect_object_bind` 借不到属性槽。

> 注意：`effect_build` 用 `field_check<uint64_t>` 校验 `act_phase`，随后却按 `get<string>()` 取值哈希——
> 构建端要求整数、触发端要求字符串，实际配置应使用字符串（如 `"1"`），否则构建期可能抛异常。

### 12.3 代码风格约定

- 命名空间统一为 `engine`；类名首字母大写（`Entity_Manager`、`Prop_Distributor`）；struct/union/enum
  同样单词首字母大写（`Event`、`Effect_Group`）；变量/函数全小写 + 下划线分隔（`entity_build`、`acl_key`）；文件名中文。
- 目录级命名约定：`模块名_Manager`（管理器）、`模块名_Broker`（中转器）、`模块名_Distributor`（分发器）。
- 注释：中文；少于三行用 `//`（无空格，如 `//绑定属性槽`），大于等于三行用 `/**/`；采用换行注释。
- 事件 `category` / `tag` 使用英文（如 `Entity` / `Build` / `Config`）。
- **include 路径必须与实际目录大小写完全一致**（跨平台硬性要求）。

---

## 十三、路线图

- [ ] 修复实体构建流程：将 `entity_build` 的 `return IDs` 移出循环、补齐两池 ID 对齐校验
- [ ] 修复 Build / Act 事件分支：修正 `ID_set.empty()` 判定、补 `entity_act(ID_set)` 调用
- [ ] 为实体补齐 `entity_type` 写入路径，恢复 `event_unicast` 的类型过滤
- [ ] 统一 `distribute_key_gen` 的返回值与 Key 事件的 tag 口径，打通属性槽分发链路
- [ ] 治理效应分组裸指针悬垂：`Effect_Group.effects` 改存 ID / 索引
- [ ] 按 TODO 补回 `register_event`（C++ 事件类型注册到 Lua）
- [ ] 接入配置编辑器：在 `CMakeLists.txt` 追加 `add_executable`，并在对外面补 ImGui / stb 包含路径
- [ ] 在Test 层补齐EngineSystem 层单元测试用例

---

## 十四、历史沿革

本层由「分层之前的一体化引擎」时期的单仓库源码迁移而来（旧仓库题名《游戏引擎》），下表为新旧名称与归属对照：

| 旧名 / 旧位置 | 现名 / 现归属 | 说明 |
| --- | --- | --- |
| `src/core/entity/Entity/` | `Engine/EngineSystem/src/entity/Entity/` | 实体迁入EngineSystem 层 |
| `src/core/entity/Entity_Manager/` | `Engine/EngineSystem/src/entity/Entity_Manager/` | 实体管理器迁入EngineSystem 层 |
| `common/types/对象类型.h` 中的 `Prop` | `Engine/EngineSystem/src/prop/Prop/属性.h` | 属性槽从类型定义独立为模块 |
| `Property_Manager` | `Prop_Distributor`（`Engine/EngineSystem/src/prop/Prop_Distributor/`） | 属性槽分发器更名；上层资源中仍保留旧名 `Property_Manager` 命名的格式文件 |
| `src/core/effect/`（后移出为 `排除编译代码/effect/`） | `Engine/EngineSystem/src/effect/` | 效应系统曾一度移出源码树不参与编译，本次已回归本层 |
| `src/tools/GUI/Config_Editor/` | `Engine/EngineSystem/src/gui/Config_Editor/` | 配置编辑器迁入EngineSystem 层（仍未接入构建） |
| `common/external/Sol2/`、`external/Sol2`、`external/Lua` | `Engine/EngineSystem/common/external/Sol2/`、`Engine/EngineSystem/external/Sol2`、`Engine/EngineSystem/external/Lua` | Sol2 / Lua 由EngineCore 层搬到EngineSystem 层（因公共头暴露 `LuaState`） |
| `src/tools/Config_Checker/配置检查器.h`（类 `Config_Checker`） | EngineCore 层 `src/tools/Detail/package/数据校验工具.h`（命名空间 `engine::detail` 自由函数 `field_check` / `path_check`） | EngineCore 层先更名 `Data_Validator`，后于工具重组中改为 `Detail` 命名空间自由函数（目录 `Config_Checker`、`Data_Validator` 均已不存在）；本层 4 个头文件与调用点已同步 |
| `src/core/event/`、`src/core/object/`、`src/core/space/`、`src/core/collision/`、其余工具 | EngineCore 层 | 事件、对象、空间、碰撞与其余工具均属EngineCore 层，不在本层 |
| 实体/属性配置 JSON 与初始化、行为 Lua 脚本 | Game 层 `assets/` | 属上层运行时资源 |
| 旧仓库的 `TestEngine.exe` / `ConfigEditor.exe` 双可执行目标 | 可执行文件统一由Test 层产出 | 层级契约改为「各层只产出静态库，只有Test 层产出可执行文件」 |

---

## 十五、许可

本层目录下没有独立的 LICENSE 文件，许可随工程根，详见工程根 `LICENSE`。

---

*本文档依据本层工作区实际源码、CMake 脚本与资源目录逐一核对后编写；接口与目录以源码为准，若发现不一致，请以源码为真。*