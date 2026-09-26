# 白银纪元 · 游戏层

游戏层是「白银纪元」四层仓库中依赖链顶端的**游戏内容层**，也是整个工程数据驱动的**终点消费层**。游戏中「有哪些怪物、每个怪物订阅哪些事件、属性怎么初始化、如何决策行动」这些问题，在本层都以可核查的资产数据（JSON 配置 + Lua 脚本）落盘，而驱动这些数据的通用机制则位于下层的引擎层与系统层。

本层是一个**独立的 git 仓库**（与引擎层、系统层、测试层各自独立），不与其余三层共享工作树；四层再以「快照」形式被顶层仓库收录。本层的代码推送目标是远端仓库的 `game` 分支。

当前本层处于**骨架状态**：`src/` 为空，不产出静态库、也不产出可执行文件。工程里已有的实质内容是 `assets/` 下的配置资产与脚本资产、以及一套与其余三层完全一致的层间契约构建骨架（`CMakeLists.txt` + `cmake/` 两份脚本）。本文档即按这一现状如实记录。

> 提示：本层不产出可执行文件。全项目**唯一**的可执行入口由测试层产出（`EngineTests.exe`）；本层将来若游戏本体需要独立入口，再单独追加一个可执行目标。

---

## 目录

- [一、分支与仓库信息](#一分支与仓库信息)
- [二、本层职责与核心特性](#二本层职责与核心特性)
- [三、技术栈](#三技术栈)
- [四、目录结构](#四目录结构)
- [五、核心架构](#五核心架构)
- [六、资产数据详解](#六资产数据详解)
- [七、对外接口契约](#七对外接口契约)
- [八、构建](#八构建)
- [九、实现状态与已知问题](#九实现状态与已知问题)
- [十、开发指南](#十开发指南)
- [十一、路线图](#十一路线图)
- [十二、历史沿革](#十二历史沿革)
- [十三、许可](#十三许可)

---

## 一、分支与仓库信息

「该分支的基本信息」集中列在本节，便于接手者快速确认「这个仓库是什么、往哪推、在依赖链的哪一环」。

### 1.1 仓库与分支

| 项目 | 取值 |
| --- | --- |
| 仓库地址 | `https://github.com/yuxingyuzhong/SilverEra.git` |
| 远端名 | `sliverera` |
| 本地分支名 | `main` |
| 远端目标分支 | `game` |
| 上游跟踪 | `sliverera/game`（本层 `main` 跟踪远端 `game`） |

要点：

- 本层的本地分支名（`main`）与远端分支名（`game`）**不一致**。由于 `push.default=simple` 会拦截「本地名 ≠ 远端名」的推送，向远端推送时必须显式写成：
  ```bash
  git push sliverera main:game
  ```
- 远端同时承载其余三层的分支：`engine`（引擎层）、`system`（系统层）、`test`（测试层），以及 `main`（顶层合并线）。`remotes/sliverera/HEAD` 当前指向 `sliverera/main`。

### 1.2 本层在依赖链中的位置

```
引擎层 ← 系统层 ← 游戏层        （依赖方向单向）
测试层           横跨各层，是全项目唯一产出可执行文件的层
```

- 依赖方向**单向**：游戏层依赖系统层，系统层依赖引擎层，不反向依赖。
- 本层只链接系统层**已经构建好的**静态库 `SystemCore`（`SystemCore.lib`），绝不把下层源码拉进本层构建树编译，也没有任何源码回退路径。
- 系统层的对外面已并入引擎层的对外面，因此链接 `SystemCore` 即同时取得系统层与引擎层两层的公共包含路径、编译定义与第三方链接库。

### 1.3 本层产物

| 目标 | 类型 | 当前状态 | 说明 |
| --- | --- | --- | --- |
| `GameCore` | STATIC | **条件目标，当前未产出** | 仅当 `src/` 下出现第一份 `.cpp`/`.c` 时由 CMake 建立；当前 `src/` 为空，故目标不存在、也无 `GameCore.lib` |
| （无） | EXECUTABLE | — | 本层**不产出**可执行文件 |

- `BYJY_GAME_OUT_LIB` 已声明为 `GameCore`，属于「声明先于产物」：边界先写清楚，等源码就位后无需再改接口文件。
- 由于没有源码，本层当前构建的产物是「无产物」——配置期通过、构建期无编译任务，属**预期行为**。

### 1.4 许可证

本层目录下**没有**独立的 `LICENSE` 文件；许可证信息见工程根目录的 `LICENSE`（MIT License，Copyright (c) 2026 雨行雨中）。详见[十三、许可](#十三许可)。

### 1.5 与其余三层的关系

| 层级 | 仓库分支 | 产物 | 与本层的关系 |
| --- | --- | --- | --- |
| 引擎层 | `main` → `engine` | `EngineCore.lib` | 经系统层对外面**间接**传递到本层（包含路径 / 链接库 / 系统库 / `GLFW_STATIC`） |
| 系统层 | `main` → `system` | `SystemCore.lib` | 本层**直接依赖**：链接其已构建静态库；本层的配置与脚本资产是系统层运行时的数据源 |
| 测试层 | `test` → `test` | `EngineTests.exe` | 未来可选择接入 `GameCore` 做游戏逻辑测试（当前未接） |
| 游戏层 | `main` → `game` | `GameCore.lib`（条件产出） | 本层自身 |

工程根目录下另有一个统一的临时构建脚本 `out/_verify/构建层.sh`，用法为 `bash 构建层.sh <层目录名>`（如 `bash 构建层.sh 游戏层`），手工布置 MSVC 环境后把指定层构建到该层的 `out/build/x64-Debug`。

---

## 二、本层职责与核心特性

### 2.1 职责

1. **游戏本体逻辑的归属地**：实体类型如何组合、战斗规则、玩法系统等「游戏玩法」代码，未来都落在本层 `src/` 下，与引擎层/系统层的通用能力分离。
2. **数据驱动的配置资产**：`assets/` 承载全部运行时数据——实体配置 JSON、属性槽配置 JSON、格式定义 JSON、路由表 JSON、初始化 Lua 脚本、行为 Lua 脚本。这些资产是下层（尤其系统层）消费的数据源。
3. **层间契约链路的末端**：作为四层链路的最上层，本层 `对外接口.cmake` 完整并入系统层（进而并入引擎层）对外面，验证「对外面逐层传递」契约的末端形态。

### 2.2 核心特性

| 特性 | 说明 |
| --- | --- |
| 数据驱动 | 实体类型、属性初始化路径、行为脚本路径全部外置为 JSON，新增内容无需重编译 |
| 路由装载 | `assets/config/route/` 下的路由表把「配置路径 → 目标模块」外置，配置加载器按路由逐条装载 |
| 两级脚本体系 | 初始化脚本（写入初始属性 + 登记行为脚本路径）与行为脚本（每帧决策）分离 |
| 层间契约一致 | 与其余三层共用分支一致的 `cmake/对外接口.cmake` + `cmake/接入下层.cmake` 接入方式 |
| 条件构建骨架 | `src/` 空视为预期状态，只打印提示不报错；放入源码后自动产出 `GameCore` |

---

## 三、技术栈

### 3.1 语言与标准

| 项 | 值 |
| --- | --- |
| 语言 | C++20（`CMAKE_CXX_STANDARD 20` + `CMAKE_CXX_STANDARD_REQUIRED ON`，`CMAKE_CXX_EXTENSIONS OFF`） |
| 资产脚本 | Lua（行为脚本 / 初始化脚本，由系统层运行时加载） |
| 数据格式 | JSON（`nlohmann::json` 解析，解析器位于引擎层） |
| 编码约定 | 源文件与资产统一 UTF-8（MSVC `/utf-8`）；中文路径经 `u8string` 转换处理 |

说明：本层当前没有 C++ 源码，上述 C++ 标准由其构建骨架声明，用于与下层保持一致；一旦 `src/` 下放入源码即按此标准编译。

### 3.2 本层消费的第三方依赖

本层自身**不引入**任何第三方库，也不维护 `external/` 目录内容（该目录当前为空）。所有第三方能力均由**下层对外面经 `SystemCore` 导入目标传递**进来：

| 依赖 | 来源 | 传递形式 |
| --- | --- | --- |
| `nlohmann::json` | 引擎层 `external/Json` | 包含路径（经引擎层对外面 → 系统层对外面 → 本层） |
| GLFW | 引擎层 `external/glfw` | 包含路径 + `glfw3.lib` + 编译定义 `GLFW_STATIC` |
| glm | 引擎层 `external/glm` | 包含路径 |
| bullet3 | 引擎层 `external/bullet3/src` | 包含路径 |
| opengl32 / user32 / gdi32 / shell32 | 引擎层对外面 | 系统库（`BYJY_SYSTEM_OUT_SYS`） |

因此本层的 `GameCore` 只要 `target_link_libraries(GameCore PUBLIC SystemCore)`，即自动继承上述全部依赖，无需在本层重复声明。

### 3.3 构建工具链

| 项 | 值 |
| --- | --- |
| 构建系统 | CMake ≥ 3.20（脚本使用 `CONFIGURE_DEPENDS` 自动检测源文件变更） |
| 生成器 | Ninja |
| 编译器 | MSVC（x64），`/MP /utf-8`、`_CRT_SECURE_NO_WARNINGS`；`/WX-`（不把警告当错误） |
| VS 集成配置 | `CMakeSettings.json`：配置名 `x64-Debug`，生成器 Ninja，继承 `msvc_x64_x64` 环境 |
| 输出目录 | 静态库 → `<构建目录>/lib/`（本层当前无产物） |

---

## 四、目录结构

```
游戏层/
├── CMakeLists.txt          # 构建脚本（接入 SystemCore + 条件产出 GameCore）
├── CMakeSettings.json      # VS 的 CMake 集成配置（含已无消费方的 BYJY_JOIN_TEST_HOST 遗留项）
├── .gitignore              # 忽略 out/、编译产物、IDE 目录
├── cmake/
│   ├── 对外接口.cmake       # 本层对外面 = 本层自有面 + 系统层对外面
│   └── 接入下层.cmake       # byjy_jieru_xiaceng()：把下层已构建静态库接进本层
├── assets/                 # 资产根（运行时数据；相对 assets/ 的路径即数据契约）
│   ├── config/             # JSON 配置
│   │   ├── entities/       #   实体配置（7 份）
│   │   │   ├── au.json
│   │   │   ├── 史莱姆 (Slime).json
│   │   │   ├── 史莱姆王 (Slime_King).json
│   │   │   ├── 哥布林 (Goblin).json
│   │   │   ├── 哥布林祭司 (Goblin_Priest).json
│   │   │   ├── 牛头人 (Minotaur).json
│   │   │   └── 牛头人战士 (Minotaur_Warrior).json
│   │   ├── property/       #   属性槽配置（7 份，与 entities/ 同名对应）
│   │   │   ├── au.json
│   │   │   └── （与 entities/ 同名的 6 个怪物属性配置）
│   │   ├── format/         #   格式定义（字段契约，供配置编辑器/校验使用）
│   │   │   ├── Entity_Manager.json
│   │   │   └── Property_Manager.json
│   │   └── route/          #   路由表（配置路径 → 目标模块）
│   │       ├── entity.json
│   │       └── property.json
│   └── scripts/            # Lua 脚本
│       ├── behavior/       #   行为（决策树）脚本
│       │   └── 哥布林 (Goblin)_Behavior.lua
│       └── initialize/     #   属性槽初始化脚本
│           └── 哥布林 (Goblin).lua
├── src/                    # 空目录（本层尚无源码，CMake 在此收集 .cpp/.c）
└── external/               # 空目录（本层不引入第三方，依赖由下层传递）
```

> 根目录另有未被 git 跟踪的构建产物（`TestEngine.exe`、`TestEngine.ilk`、`TestEngine.pdb`），属**旧宿主可执行文件的历史遗留产物**，已被本层 `.gitignore` 覆盖（`*.exe` / `*.ilk` / `*.pdb`）。它们**不是**本层现在的构建产物——本层当前不产出任何可执行文件。

---

## 五、核心架构

### 5.1 层间契约骨架

本层保留 `CMakeLists.txt` 与 `cmake/` 两份脚本，目的不是产出库，而是与其余三层保持**完全一致的层间契约接入方式**。将来在 `src/` 下放入第一份 `.cpp`/`.c` 时，`GLOB_RECURSE ... CONFIGURE_DEPENDS` 会自动纳入，无需改动 CMake 即可产出 `GameCore` 静态库。

```
游戏层（GameCore，条件产出）
   │  include 系统层对外接口（经 byjy_jieru_xiaceng 建立 IMPORTED 目标 SystemCore）
   ▼
系统层（SystemCore.lib）—— 对外面 = 自有面 + 并入的引擎层对外面
   ▼
引擎层（EngineCore.lib）—— 包含路径 / glfw3.lib / GLFW_STATIC / 四个系统库
```

### 5.2 条件目标 `GameCore`

`CMakeLists.txt` 按「源文件是否非空」分两条路径，二者都是预期状态：

```cmake
file(GLOB_RECURSE GAME_SOURCES CONFIGURE_DEPENDS
    "${PROJECT_ROOT_DIR}/src/*.cpp"
    "${PROJECT_ROOT_DIR}/src/*.c"
)

if(GAME_SOURCES)
    add_library(GameCore STATIC ${GAME_SOURCES})
    target_include_directories(GameCore PUBLIC "${PROJECT_ROOT_DIR}")
    target_link_libraries(GameCore PUBLIC SystemCore)
    # 编译选项 / 输出目录同下层，产物落 <构建目录>/lib/
else()
    message(STATUS "游戏层暂无源码，未生成 GameCore 目标。")
endif()
```

- **空源文件不是错误**：与引擎层、系统层「无源码即 `FATAL_ERROR`」不同，本层把空源码视为**预期状态**，只打印 STATUS 提示、不报错。
- `GameCore` 链接的是 `byjy_jieru_xiaceng()` 建立的**导入目标** `SystemCore`，其 `INTERFACE` 上已挂有系统层 + 引擎层的全部对外面。

### 5.3 接入方式与其余三层一致

本层在 `CMakeLists.txt` 中调用通用接入函数（系统层、测试层、本层三份 `接入下层.cmake` 逐字节相同）：

```cmake
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/接入下层.cmake")

set(BYJY_SYSTEM_LIB_PATH "" CACHE FILEPATH
    "系统层静态库路径；留空则自动探测 系统层/out/build/*/lib/SystemCore.lib")

byjy_jieru_xiaceng(
    "${PROJECT_ROOT_DIR}/../系统层/cmake/对外接口.cmake"
    "BYJY_SYSTEM"
    "BYJY_SYSTEM_LIB_PATH"
)
```

---

## 六、资产数据详解

本层 `assets/` 是四层中当前**唯一有实质内容**的部分，定义了「数据如何描述实体」的契约。相对 `assets/` 的路径即数据契约（如 `config/entities/...`、`scripts/initialize/...`）。

### 6.1 配置目录结构（`assets/config/` 四类）

| 目录 | 内容 | 作用 | 相互关系 |
| --- | --- | --- | --- |
| `entities/` | 实体类型定义：`type` / `acls` / `needed_events` 等 | 描述「有哪些实体类型、从属权限、订阅什么事件」 | 由路由表按 `config_path` 指向，交给实体管理器消费 |
| `property/` | 属性槽初始化路径：`type` / `initialize_path` | 描述「每个实体类型的属性怎么初始化」 | `type` 与 `entities/` 的 `type` 同名对应；`initialize_path` 指向 `scripts/initialize/` 下的脚本 |
| `format/` | 格式定义：`Entity_Manager.json`、`Property_Manager.json` | 声明上述两类配置的**字段契约**（字段名 / 类型 / 是否必填） | 供配置编辑器生成与校验配置；不参与运行时装载 |
| `route/` | 路由表：`entity.json`、`property.json` | 声明「配置路径 → 目标模块」的装载清单 | 配置加载器扫描此目录，按路由逐条读取 `config_path` 并分发 |

四者的配合方式：**配置加载器**先扫描 `route/` 得到装载清单，再逐条读取 `config_path` 指向的 `entities/`、`property/` 配置文件，包装为配置事件发送给路由中声明的目标模块。`format/` 是旁路的元数据，只服务编辑器/校验。

### 6.2 实体与属性配置

**实体配置真实清单（`assets/config/entities/`，共 7 份）**：

| 文件 | `type` | 内容要点 |
| --- | --- | --- |
| `au.json` | `au` | 字段最完整：`acls: ["au"]`、`decision_load_path`、`needed_events` |
| `史莱姆 (Slime).json` | `Slime` | 仅有 `type` + 空 `acls` |
| `史莱姆王 (Slime_King).json` | `Slime_King` | `acls: ["Slime"]` |
| `哥布林 (Goblin).json` | `Goblin` | `acls` 为对象（`master` / `minion_set`）+ `needed_events` 数组 |
| `哥布林祭司 (Goblin_Priest).json` | `Goblin_Priest` | `acls: ["Goblin"]` |
| `牛头人 (Minotaur).json` | `Minotaur` | 仅有 `type` + 空 `acls` |
| `牛头人战士 (Minotaur_Warrior).json` | `Minotaur_Warrior` | `acls: ["Minotaur"]` |

`哥布林 (Goblin).json`（对象形态的 `acls` + 对象数组形态的 `needed_events`）：

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

`au.json`（完整形态：`acls` 为字符串数组、`needed_events` 为 `[分类, 标签]` 对列表、并带 `decision_load_path`）：

```json
{
  "acls": [ "au" ],
  "decision_load_path": "scripts/behavior/au_Behavior.lua",
  "needed_events": [
    [ "Entity", "Request" ]
  ],
  "type": "au"
}
```

**属性配置真实清单（`assets/config/property/`，共 7 份，与实体配置同名对应）**：

| 文件 | `type` | `initialize_path` | 脚本是否存在 |
| --- | --- | --- | --- |
| `au.json` | `au` | `scripts/initialize/au.lua` | 否（悬空） |
| `哥布林 (Goblin).json` | `Goblin` | `scripts/initialize/哥布林 (Goblin).lua` | **是** |
| `史莱姆 (Slime).json` | `Slime` | `scripts/monster/slime.lua` | 否（悬空） |
| `史莱姆王 (Slime_King).json` | `Slime_King` | `scripts/monster/slime_king.lua` | 否（悬空） |
| `哥布林祭司 (Goblin_Priest).json` | `Goblin_Priest` | `scripts/monster/goblin_priest.lua` | 否（悬空） |
| `牛头人 (Minotaur).json` | `Minotaur` | `scripts/monster/minotaur.lua` | 否（悬空） |
| `牛头人战士 (Minotaur_Warrior).json` | `Minotaur_Warrior` | `scripts/monster/minotaur_warrior.lua` | 否（悬空） |

`哥布林 (Goblin).json`（唯一指向实际存在脚本的一份）：

```json
{
  "type": "Goblin",
  "initialize_path": "scripts/initialize/哥布林 (Goblin).lua"
}
```

**JSON 字段说明**（以 `format/Entity_Manager.json` 声明的契约为准）：

| 字段 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `type` | string | 是 | 实体类型标识，保存后配置文件会自动迁移；也是与属性配置对应的键 |
| `decision_load_path` | script | 是 | 决策树行为脚本路径（相对 `assets/`），实体管理器使用 |
| `acls` | string_list | 是 | 允许从属的实体类型列表（master 为实体类型自身） |
| `needed_events` | pair_list | 是 | 订阅事件列表，每项 = `[分类, 标签]`，如 `[Entity, Request]` |

`format/Property_Manager.json` 声明的属性配置契约：

| 字段 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `type` | string | 是 | 实体类型标识，与实体配置的 `type` 一致 |
| `initialize_path` | script | 是 | 属性槽初始化 Lua 脚本路径（相对 `assets/`） |

### 6.3 格式定义与路由表（`format/` 与 `route/`）

**`format/Entity_Manager.json`** 是实体配置的字段契约，除 `fields` 外还带元字段：`dir: "entities"`、`route: "entity.json"`、`builtin: true`、`module: "Entity_Manager"`。其 `fields` 逐条对应 §6.2 的字段表（含 `desc` 描述与 `display` 显示名，供配置编辑器使用）。

**`format/Property_Manager.json`** 是属性配置的字段契约，元字段为 `dir: "property"`、`route: "property.json"`、`module: "Property_Manager"`。它沿用了旧模块名 `Property_Manager`（实际对应模块此后改名为 `Prop_Distributor`，位于系统层）。

**路由表 `route/entity.json`** 是一个 JSON 数组，每项 `{config_path, module}` 描述一条装载指令，覆盖 6 个怪物实体（不含 `au`）：

```json
[
  { "config_path": "config/entities/哥布林 (Goblin).json", "module": "Entity_Mangaer" },
  { "config_path": "config/entities/史莱姆 (Slime).json",  "module": "Entity_Mangaer" }
]
```

**路由表 `route/property.json`** 结构相同，`config_path` 指向 `config/property/` 下 6 个怪物属性配置，`module` 为 `"Property_Manager"`。

与配置加载器的配合方式：加载器扫描 `assets/config/route/` 下的**全部普通文件**（不按扩展名过滤），把每个路由文件当作 JSON 数组逐条解析，对 `config_path` 做越界安全检查（路径前两段必须为 `assets/config`、全程不得含 `..`、拼接后必须存在且为普通文件）后读取配置，包装为 `category="Config"`、`tag="Load"` 的事件发送给 `module` 指定的目标模块。

### 6.4 行为脚本与初始化脚本（两级脚本体系）

`assets/scripts/` 下分两级，分别由不同链路调用：

| 级别 | 目录 | 调用方 | 职责 |
| --- | --- | --- | --- |
| 初始化脚本 | `initialize/` | 属性槽初始化链路（按 `property/*.json` 的 `initialize_path` 加载） | 实体创建时向通用属性槽写入初始数值，并登记行为脚本路径 |
| 行为脚本 | `behavior/` | `Entity::action_load` 加载后由 `act()` 每帧驱动 | 每帧构建并执行行动指令（索敌、追击、攻击、待机等决策） |

**当前脚本资产真实清单**：`behavior/` 下仅 `哥布林 (Goblin)_Behavior.lua` 一份；`initialize/` 下仅 `哥布林 (Goblin).lua` 一份。其余实体类型的配置尚无对应脚本。

初始化脚本片段（`scripts/initialize/哥布林 (Goblin).lua`）：

```lua
--函数功能：哥布林实体初始化脚本
--由实体管理器在构建哥布林实体时调用
--用途1：向通用属性槽(pros)写入初始属性
--用途2：记录行为决策脚本加载路径
function Goblin_Initialize(entity)

    --通用属性槽引用
    local pros = entity.pros

    --最大生命值：哥布林基础生命
    pros.max_hp = 100.0

    --攻击力：哥布林普通攻击伤害
    pros.attack_power = 12.0

    --行为决策脚本路径：Entity据此加载行为决策脚本
    entity.behavior_script_path = "scripts/behavior/哥布林 (Goblin)_Behavior.lua"

end
```

行为脚本片段（`scripts/behavior/哥布林 (Goblin)_Behavior.lua`）：

```lua
--函数功能：哥布林行为决策主函数
--由 Entity 每帧调用，接收注入的数据引用，完成行动指令构建与执行
function Goblin_Behavior(pros, minion_set, transfer_buffer, event_set,
                         effect_script_sign, wrap_script_sign, wrap_script_unload, wrap_script_call,
                         send)

    --待执行的行动指令表，由阶段一填充或从 Command 事件读取
    local action_commands = nil

    --遍历事件集合，检查是否存在定向命令事件 Entity/Command
    for _, evt in ipairs(event_set) do
        if evt.category == "Entity" and evt.tag == "Command" then
            local cmd = evt.config and evt.config.Action_Commands
            if cmd ~= nil then
                action_commands = cmd
            end
            break
        end
    end
end
```

行为脚本的两阶段结构：**阶段一**构建行动指令（优先读取事件集合中的 `Entity/Command` 定向命令，否则自主决策生成指令表）；**阶段二**逐条执行指令（`UseSkill` 走 `wrap_<技能名>` 打包脚本、`MoveTo` 走 `wrap_MoveTo`、`Idle` 忽略）。函数返回后事件集合由引擎端自动清空。

> 说明：C++ ↔ Lua 的**绑定层（Sol2）**属于系统层（`common/external/Sol2/`），负责把 C++ 类型注册给 Lua；本层只提供脚本资产，不承担绑定职责。

### 6.5 属性槽在 Lua 中的读写约定

- 属性名使用**小写下划线**命名（如 `max_hp`、`attack_power`、`move_speed`）。
- 所有属性值为 `double`（Lua 侧写作浮点数，如 `100.0`）。
- 状态类属性约定为 `state_*` 前缀（`state_giddy` 眩晕、`state_frozen` 冰冻），数值表示剩余持续时间，`0.0` 表示无该状态。
- 读写在初始化脚本中通过 `local pros = entity.pros` 取得属性槽引用后进行；行为脚本则以参数 `pros` 直接接收同一引用。
- 哥布林初始化脚本已写入的属性槽（可作命名参考）：`max_hp`、`now_hp`、`attack_power`、`defense`、`move_speed`、`attack_range`、`attack_cooldown`、`sight_range`、`state_giddy`、`state_frozen`。

---

## 七、对外接口契约

### 7.1 `cmake/对外接口.cmake`

声明「本层向上层提供什么」，供未来上层读取并建立 IMPORTED 目标（不使用 `add_subdirectory` 回退编源码）。首行 `include` 系统层的对外接口文件，把系统层对外面并入本层，因此本层对外面 = **本层自有面 + 系统层对外面**（系统层又已并入引擎层对外面）。并入是纯数据赋值，重复 include 无副作用。

| 变量 | 本层取值 |
| --- | --- |
| `BYJY_GAME_OUT_LIB` | `GameCore`（当前尚无源码，声明先于产物） |
| `BYJY_GAME_OUT_INC` | 本层根目录 + `BYJY_SYSTEM_OUT_INC` |
| `BYJY_GAME_OUT_DEF` | 继承 `BYJY_SYSTEM_OUT_DEF`（即引擎层的 `GLFW_STATIC`） |
| `BYJY_GAME_OUT_LINK` | 继承 `BYJY_SYSTEM_OUT_LINK`（`glfw3.lib` 等） |
| `BYJY_GAME_OUT_SYS` | 继承 `BYJY_SYSTEM_OUT_SYS`（`opengl32` / `user32` / `gdi32` / `shell32`） |

`BYJY_GAME_OUT_LIB` 声明为 `GameCore`，但当前 `src/` 为空、尚未产出库；此时若真有上层来链接它，自动探测会找不到库而硬失败——属源码注释明确标注的**预期行为**。因此**任何上层接入 `GameCore` 的动作，都必须发生在游戏层产出库之后**。

### 7.2 `cmake/接入下层.cmake`

提供通用接入函数 `byjy_jieru_xiaceng(<下层对外接口文件> <变量前缀> <覆盖库路径变量>)`，系统层、测试层、本层三份逐字节相同（改动必须同步三层）。它：

- 读入下层对外面并校验五个 `_OUT_*` 变量是否齐备（缺一即 `FATAL_ERROR`）；
- 建立 `IMPORTED STATIC`（GLOBAL）静态库目标，把包含目录、编译定义、链接库挂到其 `INTERFACE` 上，供本层向上继续传递；
- **三级库定位**：① 覆盖变量非空且文件存在 → 用它（非空但文件不存在则直接 `FATAL_ERROR`，不静默降级）；② 留空 → 自动探测 `<下层>/out/build/*/lib/<库名>.lib`，多候选取时间戳最新的一份；③ 仍无 → `FATAL_ERROR` 并给出构建下层的命令；
- **任何情况下都不回退编译下层源码**。

### 7.3 覆盖变量 `BYJY_SYSTEM_LIB_PATH`

| 变量 | 类型 | 默认 | 说明 |
| --- | --- | --- | --- |
| `BYJY_SYSTEM_LIB_PATH` | `FILEPATH` | `""` | 系统层静态库路径覆盖；留空则自动探测 `系统层/out/build/*/lib/SystemCore.lib` |

若留空且探测不到 `SystemCore.lib`，接入函数会硬失败并提示先构建系统层；此时可显式指定：`-DBYJY_SYSTEM_LIB_PATH=<SystemCore.lib 的绝对路径>`。

---

## 八、构建

### 8.1 前置条件

先构建**系统层**（系统层又依赖已构建的**引擎层**）。顺序为：引擎层 → 系统层 → 游戏层。若系统层尚未构建，本层配置期会在库定位阶段硬失败。

### 8.2 逐条命令

在本层目录内：

```bash
cmake -S . -B out/build/x64-Debug -G Ninja
cmake --build out/build/x64-Debug
```

在工程根目录内（等价的显式路径形式）：

```bash
cmake -S 游戏层 -B 游戏层/out/build/x64-Debug -G Ninja
cmake --build 游戏层/out/build/x64-Debug
```

也可使用统一入口脚本（脚本位于工程根的 `out/_verify/` 下，自动布置 MSVC 环境）：

```bash
bash out/_verify/构建层.sh 游戏层
```

### 8.3 当前无源码时的行为

- 由于本层当前无源码，**配置可以通过，但不生成任何静态库**；`cmake --build` 无编译任务即成功退出。
- 配置期完成「接入系统层 + 空源文件判定」两步；构建期无目标可构建。
- 若覆盖变量 `BYJY_SYSTEM_LIB_PATH` 留空，则自动探测 `系统层/out/build/*/lib/SystemCore.lib`；探测不到会硬失败并提示先构建系统层。

### 8.4 本层不产出可执行文件

宿主机制取消后，可执行文件只在测试层产生（`EngineTests.exe`）。本层根目录残留的 `TestEngine.exe` / `.ilk` / `.pdb` 属旧宿主时期的历史遗留产物，不是本层现在的构建产物。

---

## 九、实现状态与已知问题

### 9.1 已完成

- 层间契约骨架齐全：`CMakeLists.txt`、`cmake/对外接口.cmake`、`cmake/接入下层.cmake` 全部就位，接入方式与其余三层一致。
- `assets/` 配置资产落盘：`entities/`（7 份）、`property/`（7 份）、`format/`（2 份）、`route/`（2 份）。
- 两级脚本骨架落盘：哥布林的初始化脚本与行为脚本各一份。
- 接入系统层已构建静态库的链路可跑通（配置期校验通过）。

### 9.2 尚未完成

- **本层无源码**：`src/` 为空，`GameCore` 目标未生成、无 `GameCore.lib`。
- **脚本覆盖不全**：仅哥布林具备初始化脚本与行为脚本，其余实体类型（含 `au`）尚无对应 Lua。
- **运行时闭环缺失**：资产数据已就位，但缺少本层自己的运行时来驱动「配置 → 实体 → 战斗」的完整闭环；当前配置只能被下层的接口消费。
- **测试层尚未接入 `GameCore`**：需等本层产出库之后才能接入。

### 9.3 已知问题

| 位置 | 现象 |
| --- | --- |
| 宿主机制（历史决策） | 本层过去挂着「组合根可执行文件」与「把测试层宿主并入本构建树」的开关（`BYJY_JOIN_TEST_HOST`），两者均已移除 |
| `CMakeSettings.json` | 仍保留 `cacheVariables.BYJY_JOIN_TEST_HOST = true`，但 `CMakeLists.txt` 全文已无任何代码读取该变量——**已无消费方的遗留项**，CMake 会忽略未消费的缓存变量，不影响构建 |
| `route/entity.json` | 路由中的模块名存在拼写错误：`"Entity_Mangaer"`（应为 `Entity_Manager`），读取方若按正确名匹配将落空 |
| 资产与脚本目录不一致 | 除哥布林外，`property/*.json` 的 `initialize_path` 均指向 `scripts/monster/*.lua`，但 `assets/scripts/` 下**不存在** `monster/` 子目录，这些路径当前悬空；`au.json` 指向的 `scripts/initialize/au.lua` 同样不存在 |
| 配置文件名 | `entities/` 与 `property/` 的配置文件名带空格与括号（如 `史莱姆 (Slime).json`）——中文 + 空格路径在 MSVC/Ninja 与 googletest 下是已知风险区 |
| 字段命名不统一 | `format/Entity_Manager.json` 声明行为脚本字段为 `decision_load_path`，而实际初始化脚本写入的键为 `entity.behavior_script_path`；两者命名尚未统一 |

> 关于 **宿主机制取消的原因**：其一，本层的组合根与测试层宿主职责重叠；其二，组合根会把下层源码拉进本层构建树重复编译，与「层间一律链接已构建静态库」的契约冲突。现在各层分工为：引擎层产出 `EngineCore.lib`、系统层产出 `SystemCore.lib`、本层产出 `GameCore.lib`（条件）、测试层产出唯一可执行文件 `EngineTests.exe`。

---

## 十、开发指南

### 10.1 如何新增一个实体类型

1. **编写实体配置**：在 `assets/config/entities/` 新建 `<类型名>.json`，声明 `type` / `acls` / `needed_events`（如需行为脚本另加 `decision_load_path`）。
2. **编写属性配置**：在 `assets/config/property/` 新建同名 JSON，`initialize_path` 指向初始化脚本路径。
3. **编写初始化脚本**：在 `assets/scripts/initialize/` 新建 `<类型名>.lua`，函数名约定为 `<类型名>_Initialize(entity)`，写入初始属性并登记行为脚本路径。
4. **编写行为脚本**（可选）：在 `assets/scripts/behavior/` 新建行为脚本，由初始化脚本登记其路径。
5. **登记路由**：在 `assets/config/route/entity.json` 与 `route/property.json` 中新增对应条目。
6. 重新运行（由下层运行时驱动）`assets/config/` 的装载流程，配置加载器会自动读取新配置（无需重编译）。

### 10.2 如何新增一个属性槽配置

1. 在 `assets/config/property/` 新建 `<类型名>.json`，`type` 与实体配置一致；
2. `initialize_path` 指向 `assets/scripts/initialize/` 下真实存在的脚本（避免悬空路径）；
3. 在 `route/property.json` 中登记该配置。

### 10.3 如何新增一个行为脚本

1. 在 `assets/scripts/behavior/` 新建脚本，函数体遵循「阶段一构建指令 → 阶段二执行指令」两阶段结构；
2. 在初始化脚本中登记其路径（当前约定键为 `entity.behavior_script_path`）；
3. 属性读写遵循 §6.5 的命名约定（小写下划线、`state_*` 前缀）。

### 10.4 代码与数据风格约定

- **命名**：类名首字母大写（`Prop_Effect`）；struct/union/enum 首字母小写（`event`）；变量/函数全小写 + 下划线分隔（`acl_key`）；文件名默认中文。
- **注释**：中文注释；不足三行用 `//`（无空格），三行及以上用 `/**/`；换行注释。
- **编码**：源文件与资产统一 UTF-8（无 BOM）；中文路径经 `u8string` 处理。
- **数据**：JSON 字段名使用英文小写（`type` / `acls` / `needed_events`）；事件 `category`/`tag` 使用英文（如 `Entity` / `Request`）。
- **契约**：CMake 变量名与函数名全部 ASCII（`byjy_jieru_xiaceng`）；三份 `接入下层.cmake` 改动必须同步三层。

---

## 十一、路线图

- [ ] 游戏本体逻辑落地：在 `src/` 下放入第一份 `.cpp/.c`，自动产出 `GameCore` 静态库
- [ ] 脚本覆盖：为全部实体类型补齐初始化脚本与行为脚本
- [ ] 资产一致性修复：修正 `route/entity.json` 的 `module` 拼写、对齐 `initialize_path` 与脚本目录、补全 `entities/` 缺失字段
- [ ] 字段命名收敛：统一 `decision_load_path` 与 `behavior_script_path` 两套命名
- [ ] 测试层接入 `GameCore`，为游戏逻辑建立用例
- [ ] 运行时闭环：接通「配置 → 实体 → 战斗」的完整数据流
- [ ] 游戏本体入口：若需要独立可执行程序，在本层单独追加一个可执行目标

---

## 十二、历史沿革

本层是在「分层之前的一体化引擎」时期之后，从旧工程《游戏引擎》中按依赖方向切分出来的层。旧工程的部分模块与资产在本层保留，部分迁往引擎层/系统层。对照如下：

| 旧工程中的名称 | 现名 / 现归属 | 说明 |
| --- | --- | --- |
| 一体化工程《游戏引擎》 | 拆分为引擎层 / 系统层 / 游戏层 / 测试层四层 | 旧 README 题名《游戏引擎》，对应分层前的一体化时期 |
| `assets/config/`（entities / property / format / route） | **游戏层** `assets/config/` | 配置资产整体留在本层，作为数据驱动终点 |
| `assets/scripts/`（initialize / behavior） | **游戏层** `assets/scripts/` | 两级脚本资产留在本层 |
| `Property_Manager`（模块） | 系统层 `src/prop/Prop_Distributor/` | 模块改名为 `Prop_Distributor`；本层仅保留 `format/Property_Manager.json` 等旧命名的资产 |
| `Entity_Manager` / `Entity` / `Prop`（模块与类） | 系统层 `src/entity/`、`src/prop/` | 实体、属性相关模块迁入系统层 |
| `Config_Loader`（模块） | 引擎层 `src/tools/Config_Loader/` | 配置加载器迁入引擎层，消费本层的 `route/` 资产 |
| `Data_Validator` | 引擎层 `src/tools/Data_Validator/` | 旧名 `Config_Checker`（配置检查器），迁入引擎层 |
| 绑定层（Sol2） | 系统层 `common/external/Sol2/` | C++ ↔ Lua 绑定属于系统层，本层只提供脚本资产 |
| 组合根可执行文件 | **已移除** | 与宿主职责重叠，且会把下层源码拉入本层构建树 |
| `BYJY_JOIN_TEST_HOST` 开关 | **已取消**（残留于 `CMakeSettings.json`） | 把测试层宿主并入本构建树的开关，已无消费方 |
| `TestEngine.exe` 等旧宿主产物 | 历史遗留（未跟踪） | 已被 `.gitignore` 覆盖，非本层现产物 |

---

## 十三、许可

本层目录下无独立 `LICENSE` 文件，许可证见工程根目录的 `LICENSE`：

- **MIT License**
- Copyright (c) 2026 雨行雨中

---

*本文档依据本层实际文件系统、构建脚本与资产文件逐项核对后编写。资产文件清单、CMake 变量名与目标名均与磁盘/源码一致；接口与目录以源码为准，若发现不一致，请以源码为真。*