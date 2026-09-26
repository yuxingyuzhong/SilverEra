# 白银纪元 · 系统层

系统层运行于引擎层之上，向游戏层提供实体、属性、效应等运行时系统，产出静态库 `SystemCore.lib`。

## 本层在依赖链中的位置

「白银纪元」由四层嵌套的独立 git 仓库组成，依赖方向单向：

```
引擎层  ←──  系统层  ←──  游戏层
```

- 系统层**只依赖引擎层**，并被游戏层依赖；不存在反向依赖。
- 层与层之间**只链接下层已构建的静态库**：本层链接引擎层产出的 `EngineCore.lib`，**绝不回退编译引擎层源码**。
- 本层独立仓库，向远端 `system` 分支推送。
- 本层**只产出静态库 `SystemCore.lib`，不产出任何 `.exe`**；可执行文件由测试层统一产出。

## 目录结构

以下为到模块一级的真实目录树（省略 `out/`、`.vs/`、`.git/` 等构建与工具产物）：

```
系统层/
├── CMakeLists.txt              # 构建脚本：产出 SystemCore 静态库
├── CMakeSettings.json          # VS 配置（x64-Debug / Ninja）
├── .gitignore
├── cmake/
│   ├── 对外接口.cmake           # 本层对外面自描述（并入引擎层对外面）
│   └── 接入下层.cmake           # 通用接入函数 byjy_jieru_xiaceng()
├── common/
│   ├── 前置头文件包含.h          # 层内公共预编译头
│   └── external/Sol2/
│       ├── sol类型别名.h         # LuaTable / LuaScript / LuaState 别名
│       └── sol类型注册.h         # C++ 类型向 Lua 注册
├── external/                   # Dear_ImGui / glad / glfw / glm / Lua / Sol2 / stb 副本
├── assets/UI/                  # 配置编辑器用图片资源（png / jpg）
└── src/
    ├── entity/
    │   ├── Entity/             # 实体类 + 实现
    │   └── Entity_Manager/     # 实体管理器 + 按职责拆分的实现
    ├── prop/
    │   ├── Prop/               # 属性槽
    │   └── Prop_Distributor/   # 属性槽分发器
    ├── effect/
    │   ├── Effect/             # 效应 + 实现
    │   └── Effect_Manager/     # 效应管理器 + 实现
    └── gui/
        └── Config_Editor/      # 配置编辑器（含独立 main，未接入静态库构建）
```

## 模块清单

| 模块 | 路径 | 功能 |
| --- | --- | --- |
| 实体 | `src/entity/Entity/` | `Entity` 类：继承引擎层 `Object`，持有事件终端、属性槽指针与 Lua 行为脚本。 |
| 实体管理器 | `src/entity/Entity_Manager/` | `Entity_Manager`：实体池 + 属性槽池，配置驱动创建 / 卸载 / 行动，事件接入与分发（实现拆为 `core/配置处理.cpp`、`实体处理.cpp`、`事件处理.cpp`、`属性槽处理.cpp`）。 |
| 属性 | `src/prop/Prop/` | `Prop` 类：通用属性槽（`std::unordered_map<std::string, double>`，纯头文件实现）。 |
| 属性槽分发器 | `src/prop/Prop_Distributor/` | `Prop_Distributor`：以分发密钥向外部提供属性槽池的读写通道。 |
| 效应 | `src/effect/Effect/` | `Prop_Effect` 类：Lua 驱动的效应（配置读取、数据注入、触发）。 |
| 效应管理器 | `src/effect/Effect_Manager/` | `Effect_Manager`：按归属分组管理效应记录，按执行阶段 / 优先级触发。 |
| 配置编辑器（GUI） | `src/gui/Config_Editor/` | 基于 ImGui 的图形配置编辑器，含独立 `int main(void)`（`core/配置编辑器主程序.cpp`），被 CMake 排除在 `SystemCore` 之外。 |

## 对外接口契约

本层通过两份 CMake 脚本接入「层间静态库契约」，二者是层间的公开约定：

- `cmake/对外接口.cmake`——**本层对外面自描述**，声明「本层向上层提供什么」。本层对外面 = **本层自有面 + 下层（引擎层）对外面**，通过一句 `include("${CMAKE_CURRENT_LIST_DIR}/../../引擎层/cmake/对外接口.cmake")` 并入引擎层的 `BYJY_ENGINE_OUT_*`。契约变量（改名必须同步上层）：
  - `BYJY_SYSTEM_OUT_LIB` = `SystemCore`（本层静态库文件名，不含扩展名）；
  - `BYJY_SYSTEM_OUT_INC` = 本层根目录、`src/`、`external/Sol2/include`、`external/Lua`，再接引擎层的包含目录；
  - `BYJY_SYSTEM_OUT_DEF` / `_OUT_LINK` / `_OUT_SYS` 直接承自引擎层对外面。
  - 因公共头文件（`src/entity/Entity/实体.h`、`src/effect/Effect/效应.h`）对外暴露 `LuaState`（`sol::state`），Sol2 / Lua 路径必须列入对外包含目录。
- `cmake/接入下层.cmake`——提供通用接入函数 `byjy_jieru_xiaceng(<下层对外接口文件> <变量前缀> <覆盖库路径变量名>)`，把**下层已构建好的静态库**接进当前层：
  - 库定位三级顺序：① 覆盖变量非空且文件存在则采用（非空但不存在则 `FATAL_ERROR`，不静默降级）；② 覆盖变量为空则扫 `引擎层/out/build/*/lib/<库名>.lib`，取时间戳最新的一份；③ 仍无则 `FATAL_ERROR` 并提示先构建下层。
  - 建立 `IMPORTED STATIC GLOBAL` 目标（目标名即下层库名），挂载包含目录 / 编译定义 / 链接库到其 `INTERFACE` 属性，供本层向上继续传递。
  - **任何情况下都不回退编译下层源码**。
  - 本文件在本层 / 测试层 / 游戏层各一份，逐字节相同，改动需同步三层。
  - 本层 `CMakeLists.txt` 的调用：`byjy_jieru_xiaceng("${PROJECT_ROOT_DIR}/../引擎层/cmake/对外接口.cmake" "BYJY_ENGINE" "BYJY_ENGINE_LIB_PATH")`。

## 构建

前置条件：**先构建引擎层**（产出 `EngineCore.lib`），本层链接的是已构建的引擎层静态库。

```bash
cmake -S . -B out/build/x64-Debug -G Ninja
cmake --build out/build/x64-Debug
```

- 工具链：CMake ≥ 3.20、Ninja 生成器、C++20（禁用编译器扩展）、MSVC（Windows 已验证）。
- 源文件由 `GLOB_RECURSE ... CONFIGURE_DEPENDS` 自动收集 `src/entity/`、`src/prop/`、`src/effect/` 下的 `.cpp` / `.c`，并排除构建目录、`Tests/` 与 `src/gui/Config_Editor/`。
- 产物：`out/build/x64-Debug/lib/SystemCore.lib`（静态库）。本层不产出任何可执行文件。

## 当前状态与遗留

- 已接入层间静态库契约，可独立配置并构建出 `SystemCore.lib`；实体、属性、效应三模块的源码已纳入 `SystemCore` 编译。
- `src/gui/Config_Editor/` 因含独立 `main()` 被 CMake 整体排除，未建立可执行目标；接入时需追加 `add_executable` 并补全 ImGui / stb 等包含路径。
- 遗留引用：`src/entity/Entity_Manager/实体管理器.h`、`src/prop/Prop_Distributor/属性槽分发器.h`、`src/effect/Effect/效应.h`、`src/effect/Effect_Manager/效应管理器.h` 仍按引擎层旧路径 `#include "src/tools/Config_Checker/配置检查器.h"`（引擎层已更名为 `Data_Validator`），待同步改名。
- 测试：本层当前无独立用例；单元测试由测试层承载（链接本层已构建的静态库）。