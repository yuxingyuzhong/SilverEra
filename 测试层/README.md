# 白银纪元 · 测试层

本层是全项目**唯一产出可执行文件的层**，承载单元测试体系与「测试模块选择器」（图形窗口 / 控制台菜单）。

## 一、本层在依赖链中的位置

测试层横跨各层：向上不产出被链接的库，向下**只链接各层已构建的静态库**，绝不回退编译下层源码。

```
引擎层 EngineCore.lib ──▶ 系统层 SystemCore.lib ──▶ 测试层 EngineTests.exe
        （已构建）              （已构建）                  （唯一可执行产物）
```

- 通过 `cmake/接入下层.cmake` 的 `byjy_jieru_xiaceng()` 接入引擎层与系统层：先用覆盖变量 `BYJY_ENGINE_LIB_PATH` / `BYJY_SYSTEM_LIB_PATH`，留空则自动探测 `<下层>/out/build/*/lib/*.lib` 取最新，都没有则硬失败。
- 引擎层对外面已并入系统层对外面，故引擎层的公共头文件（`common/` 预编译头、`src/` 头文件、`external/Json`、`external/glfw` 等）随导入目标一并到位。
- **宿主机制已取消**：本层不再产出分层宿主；可执行文件只由本层产出，`EngineTests.exe` 既是单元测试入口，也是项目启动项。

## 二、目录结构

```
测试层/
├── CMakeLists.txt            # 构建脚本：TestCore / TestGui / TestLauncher / EngineTestObjects / EngineTests
├── CMakeSettings.json
├── .gitignore
├── cmake/
│   ├── 对外接口.cmake        # 本层对外面自描述（BYJY_TEST_OUT_*）
│   └── 接入下层.cmake        # byjy_jieru_xiaceng() 通用接入函数
├── external/                 # 第三方源码副本（本层自行编译）
│   ├── googletest/           # gtest 框架源码（EXCLUDE_FROM_ALL 接入）
│   ├── Dear_ImGui/           # imgui 核心（imgui/）+ glfw/opengl3 后端（backends/）
│   └── glad/                 # OpenGL 函数加载器（src/gl.c、include/glad/gl.h）
├── src/
│   ├── 主调/                 # 测试模块选择器与主程序
│   └── 单元测试/             # 用例源文件（主调/ 工具/ 核心/ 三棵子树）
├── assets/                   # 目录联接 → 游戏层/assets（见下方说明，不入库）
└── out/build/                # 构建产物目录（不入库）
```

说明：

- `assets/` 是一个**指向 `../游戏层/assets` 的目录联接**，不是本层的真实目录。这样做的目的是让资产保持**单一真源**（实体/属性配置与 Lua 脚本只在游戏层维护一份），同时让本层运行时的相对路径与游戏层一致。联接本身已列入 `.gitignore`（否则 git 会把联接当真实目录递归追踪，产生大量重复条目），因此**克隆本层不会得到 `assets/`**，需要自行建立联接或从游戏层复制。
- 可执行产物 `EngineTests.exe` 落在**层根目录**（连同 `.ilk` / `.pdb`），属构建产物，已列入 `.gitignore`。

## 三、模块清单

### 3.1 主调（`src/主调/`）

| 模块 | 路径 | 功能 |
| --- | --- | --- |
| 主程序 | `src/主调/主程序.cpp` | `main()` 入口：参数摘取、googletest 初始化、四步分流、过滤串落地、收尾等待 |
| 测试选择模型 | `src/主调/测试选择模型.h` / `.cpp` | 反射建立「套件→用例」两层选择树，维护勾选态，生成 `--gtest_filter` 过滤串 |
| 控制台选择菜单 | `src/主调/控制台选择菜单.h` / `.cpp` | 无图形环境时的编号菜单：打印列表、读入编号、应用勾选 |
| 图形选择窗口 | `src/主调/图形选择窗口.h` / `.cpp` | GLFW + OpenGL3 + Dear ImGui 树形勾选窗口，失败返回 false 触发降级 |
| 退出等待判定 | `src/主调/退出等待判定.h` | 声明 `engine::需要等待退出()`（实现位于 `主程序.cpp`，供用例直接链接验证） |

### 3.2 单元测试体系（`src/单元测试/`）

20 个用例源文件，按被测层分三棵子树；套件名 = 夹具类名。

| 子树 | 路径 | 覆盖内容 |
| --- | --- | --- |
| 主调 | `src/单元测试/主调/` | `测试选择模型测试`（16 例）、`退出暂停判定测试`（8 例） |
| 工具 | `src/单元测试/工具/` | 二分查找、引擎环境、数值分配器、数据校验器、日志系统、网格加载器、计时器、路径字符串转换、随机数生成器，共 9 文件 |
| 核心 | `src/单元测试/核心/` | 事件、事件中转器、事件终端、四叉树、四叉树管理器、坐标类型、对象、对象池、碰撞（`Collider_Test` / `Collision_Region_Test` / `Collision_Proxy_Test` 三套件），共 9 文件 |

### 3.3 构建目标

| 目标 | 类型 | 说明 |
| --- | --- | --- |
| `TestCore` | STATIC | `src/` 下功能测试代码（排除 `单元测试/`、`主调/`）；当前为空，跳过产出 |
| `TestGui` | STATIC | Dear ImGui 核心 4 文件 + glfw/opengl3 后端 + glad，仅供图形窗口使用 |
| `TestLauncher` | STATIC | 纯逻辑的测试选择模型 + 控制台菜单，供主程序与用例共用 |
| `EngineTestObjects` | OBJECT | 全部用例 `.obj`；用 OBJECT 库而非 STATIC，避免未引用 `.obj` 被丢弃导致用例静默不跑 |
| `EngineTests` | 可执行文件 | 用例 `.obj` + 主程序 + 图形窗口，链接下层静态库与 gtest，输出到层根目录 |

## 四、测试规模与运行方式

### 4.1 规模（已核实）

- 用例源文件 20 个；测试套件 **22 个**；测试用例 **338 个**；失败 0；DISABLED **0**（无遗留禁用用例）。
- 分布：主调 24 例（2 套件）、工具 120 例（9 套件）、核心 194 例（11 套件）。

### 4.2 选择器参数

| 命令 | 行为 |
| --- | --- |
| `EngineTests.exe` | 默认 `window`：图形窗口树形勾选；窗口不可用时自动降级为控制台菜单 |
| `EngineTests.exe --selector=window` | 显式指定图形窗口（同样支持失败降级） |
| `EngineTests.exe --selector=console` | 控制台编号菜单，支持 `1,3-5` 区间与 `all` 全选，直接回车表示全跑 |
| `EngineTests.exe --selector=off` | 不开窗、不建模型，直接全量运行（ctest 使用） |
| `EngineTests.exe --gtest_filter=...` | 命令行已带 `--gtest_*` 且未显式指定选择器时直接透传，不弹窗 |

- 交互运行（window/console）结束后会打印提示并停住等待按键；自动化路径（透传 / off）不等按键，退出码不受影响。
- 显式 `--selector=` 优先于「带 gtest 参数即透传」的约定。

### 4.3 ctest

在构建目录执行 `ctest`。工程路径为纯 ASCII 时用 `gtest_discover_tests` 逐用例注册；路径含非 ASCII（本仓库即含中文）时退化为整体注册一个用例，命令等价于 `EngineTests --selector=off`。

## 五、第三方依赖

全部以**源码副本**形式入库在 `external/` 下，由本层自行编译：

- `external/googletest/`：仅测试框架本体，沿用 googletest 惯例——`INSTALL_GTEST OFF`、`BUILD_GMOCK OFF`、`gtest_force_shared_crt ON`（强制共享运行时库避免 CRT 冲突），并以 `EXCLUDE_FROM_ALL` 接入；不使用 `GTest::gtest_main`，`main` 由本层提供。
- `external/Dear_ImGui/`：imgui 核心（不编 `imgui_demo.cpp`）+ glfw/opengl3 后端。
- `external/glad/`：OpenGL 函数加载器，供图形窗口自身 GL 调用使用。

引擎层/系统层的第三方库（GLFW、nlohmann::json 等）不在此重复，随下层导入目标传递。

## 六、构建

前置条件：先构建引擎层与系统层（各自产出 `EngineCore.lib` / `SystemCore.lib`）。

```bash
# 配置（CMake ≥ 3.20，Ninja 生成器，C++20，MSVC 已验证）
cmake -S . -B out/build/x64-Debug -G Ninja

# 构建（产物：层根目录 EngineTests.exe）
cmake --build out/build/x64-Debug
```

层间契约：层与层之间一律链接已构建静态库，本层不拉入任何下层源码，也没有源码回退路径。

## 七、当前状态与遗留

- **状态**：`EngineTests.exe --selector=off` 全量 338 / 338 通过、0 失败、0 DISABLED；`ctest` 1/1 Passed。
- **遗留**：
  - 图形窗口的鼠标点选交互尚未自动化，回归覆盖限于「窗口创建失败降级」路径；
  - 四叉树超大树上范围查询尚未专项验证；
  - `src/` 下功能测试代码（`TestCore`）当前为空，测试能力集中于单元测试与选择器。