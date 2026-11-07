# 白银纪元 · Test 层

Test 是「白银纪元」分层工程中的**验证与启动层**。工程按依赖方向分层：`Engine/EngineFoundation` → `Engine/EngineSystem` → `Engine/EngineRenderer` → `Engine`（聚合层）→ `Application/{Game,Test}`；每一层都是一个独立的 git 仓库，各推各的分支；本层对应的分支是 `test`，向远端 `sliverera/test` 推送。顶层 `main` 仓库只记录各层目录指针（不再做源码快照），作总览与合并线。

本层在依赖链中处于**最上层**：它**接 Engine 聚合层一次**——链接两个**已经构建好的静态库**（`EngineFoundation.lib`、`EngineSystem.lib`，由聚合面一并转交）；另单独接入渲染层 `EngineRenderer.lib` 供图形操作界面使用。绝不通过 `add_subdirectory` 把下层源码拉进本构建树重复编译，也没有任何源码回退路径。这条「层间一律链接已构建静态库」的契约由每层的 `cmake/对外接口.cmake` 与 `cmake/接入下层.cmake` 共同保证。

本层是**全项目唯一产出可执行文件的层**：`EngineTests.exe` 既是单元测试入口，也是整套工程的启动项。「宿主机制」（把下层组件按由浅到深的范围接在一起、各自产出启动项）已经取消，各层不再各自维护运行入口，可执行文件只由本层产生。

本层承载两件事：一是 **googletest 单元测试体系**（78 个套件 / 1415 个用例，按被测层拆到 `tests/Foundation`、`tests/System`、`tests/Renderer` 三个独立子工程，另有本层主调逻辑自测）；二是**测试模块选择器**（图形操作界面 / 控制台菜单 / 命令行开关三种交互形态），让同一份主入口同时服务「开发期按模块精准复跑」与「CI 一键全量回归」两类场景。

---

## 一、分支与仓库信息

### 1.1 仓库与分支

| 项 | 值 |
| --- | --- |
| 远端仓库地址 | `https://github.com/yuxingyuzhong/SilverEra.git` |
| 远端名（remote） | `sliverera` |
| 本地分支名 | `test` |
| 目标推送分支 | `test`（本地与远端同名） |
| 上游跟踪关系 | `test` → `sliverera/test` |
| 推送命令 | `git push sliverera test:test` |

说明：

- 各层共用同一个远端仓库，各自推自己的分支（EngineFoundation `engine`、EngineSystem `system`、Test `test`、Game `game`）。本层本地分支就叫 `test`，因此推送时本地名与远端名一致，无需像 EngineFoundation 那样做 `main:engine` 的改名推送。
- 顶层 `main` 分支仓库是独立的一层仓库，与本层的 git 历史互不包含；它只记录各层目录指针（顶层不再收录含自身 `.git` 的子目录源码），不会自动跟随本层更新。

### 1.2 本层在依赖链中的位置

```
EngineFoundation（EngineFoundation.lib，已构建） ──▶ EngineSystem（EngineSystem.lib，已构建）
                     ▲                                    ▲
                     └──────── Engine（聚合层，无产物）────┘
                                        │
                     Engine/EngineRenderer（EngineRenderer.lib，已构建）
                                        │
                                        ▼
                    Application/Test ──▶ EngineTests.exe（全项目唯一可执行产物）
```

- 本层不产出供上层链接的库（`BYJY_TEST_OUT_LIB` 为空）；`EngineTests.exe` 与三个薄入口都是运行产物，不写进对外面。
- 本层对下层只做「消费」：经 Engine 聚合层拿到两个子层的静态库与公共头文件路径；另单独接入渲染层 `EngineRenderer.lib`（图形操作界面经其 UI 通道绘制），编译用例并链接。

### 1.3 仓库内布局与产物

| 项 | 值 |
| --- | --- |
| 本层根目录 | 工程根下的 `Application/Test/` |
| 主可执行产物 | `EngineTests.exe`，落在**本层根目录**（与 `assets/` 同级） |
| 分层薄入口 | `TestFoundation.exe` / `TestSystem.exe` / `TestRenderer.exe`，同样落在本层根目录，各自只跑一层用例 |
| 分层测试库 | `tests/<层>/out/build/x64-<配置>/lib/Test<层>.lib`（用例 + 注册锚） |
| 伴随调试产物 | `EngineTests.ilk`、`EngineTests.pdb`，同样落在层根目录 |
| 中间产物与静态库 | `out/build/<配置>/`（`lib/` 下为 `TestLauncher.lib` / `TestGui.lib` / `gtest.lib`） |

- 产物落在层根目录是**硬性要求**：引擎环境的 `Engine_Env` 以「可执行文件所在目录」为基准解析资源路径，用例里拼接临时网格文件也依赖这一点；产物若改到 `out/` 下，资源路径会全盘错位。
- `EngineTests.exe` / `.ilk` / `.pdb` 属构建产物，已列入本层 `.gitignore`，不入库。

### 1.4 许可证

本层**不单独维护许可证文件**，许可见工程根目录的 `LICENSE`。

### 1.5 与其余各层的关系

| 层 | 与 Test 的关系 |
| --- | --- |
| EngineFoundation | 本层链接其已构建的 `EngineFoundation.lib`；用例直接 include 其头文件（事件、对象、空间分区、碰撞、工具集）验证真实现 |
| EngineSystem | 本层链接其已构建的 `EngineSystem.lib`；两个库均由 Engine 聚合面一并转交 |
| Engine（聚合层） | 本层**只接这一次**：`byjy_jieru_xiaceng(Engine/cmake/对外接口.cmake, ...)`，两个导入目标随即可用 |
| Engine/EngineRenderer | 本层另接一次 `byjy_jieru_xiaceng(Engine/EngineRenderer/cmake/对外接口.cmake, ...)`，取得 `EngineRenderer.lib`、渲染层头快照、Vulkan SDK / SDL3 公共面；图形操作界面经其 UI 通道绘制 |
| Application/Game | 当前无源码、不参与本层构建；本层通过 `assets/`（指向 `Application/Game/assets` 的目录联接）共用资产 |
| 顶层 `main` | 只记录各层目录指针与总览，不参与本层构建 |

---

## 二、本层职责与核心特性

| 特性 | 说明 |
| --- | --- |
| 唯一主入口 | `EngineTests.exe` 是全工程的启动项；main 由本层主调提供，不链接 googletest 自带的 `gtest_main`。另有三个薄入口只跑单层用例 |
| 全量单元测试 | 78 个套件 / 1415 个用例，按被测层拆到 `tests/Foundation`、`tests/System`、`tests/Renderer` 三个独立子工程，外加本层主调逻辑自测 |
| 按层独立验证 | 改哪一层就只构建哪个测试子工程：子工程只读 `Engine/版本锁.cmake`、只链同版次静态库，不被其它层未升锁的修改拖垮 |
| 反射驱动的选择树 | 通过 googletest 反射枚举已注册的套件与用例，运行时建立「套件 → 用例」两层选择树，新增用例自动可见，无需维护名单 |
| 三种交互形态 | 图形操作界面（SDL3 + Vulkan + Dear ImGui，常驻窗口、同进程多轮跑测）、控制台编号菜单、命令行开关全跑；无图形环境时自动降级 |
| 所见即所测 | 勾选结果压缩成 googletest 原生 `--gtest_filter` 过滤串执行；执行前打印过滤串与复跑命令 |
| 自动化友好 | 已带 `--gtest_*` 参数即透传、不弹窗、不等按键；`--selector=off` 不开窗直接全量回归（ctest 走这条） |
| 中文可读 | 套件名与用例名全中文；控制台强制切 UTF-8 代码页；图形窗口加载中文字体候选链 |
| 资产单一真源 | `assets/` 是指向Game 层资产的目录联接，本层不重复维护一份资产 |

---

## 三、技术栈

### 3.1 语言与标准

| 项 | 值 |
| --- | --- |
| 语言 | C++20（`CMAKE_CXX_STANDARD 20` + `REQUIRED ON`，`CMAKE_CXX_EXTENSIONS OFF`） |
| 命名空间 | `engine` |
| 源码风格 | 中文注释 + 中文用例名/文件名；MSVC 编译选项含 `/utf-8` 与 `/MP` |

### 3.2 第三方依赖

全部以**源码副本**形式入库在本层 `external/` 下，由本层自行编译（不改动下层构建脚本与对外接口）：

| 库 | 用途 | 引入方式 |
| --- | --- | --- |
| googletest | 单元测试框架 | `external/googletest/` 源码副本；`EXCLUDE_FROM_ALL` 接入，`INSTALL_GTEST OFF`、`BUILD_GMOCK OFF`、`gtest_force_shared_crt ON`（强制共享运行时库，避免与下层 CRT 设置冲突产生 LNK2038）；不使用 `GTest::gtest_main` |
| Dear ImGui | 图形操作界面的即时模式界面 | `external/Dear_ImGui/` 源码副本；编 imgui 核心 4 文件（`imgui.cpp`/`imgui_draw.cpp`/`imgui_tables.cpp`/`imgui_widgets.cpp`）+ 后端（`imgui_impl_sdl3.cpp`/`imgui_impl_vulkan.cpp`）；**不编 `imgui_demo.cpp`**（示例窗口用不到） |
| glad | 改造前遗留（当前不参与编译） | `external/glad/` 源码副本仍留在树内；图形后端改用 SDL3 + Vulkan 后本层不再引用 glad，构建脚本也不再收编 `src/gl.c` |

EngineFoundation / EngineSystem 层的第三方库（GLFW、nlohmann::json、bullet3、glm 等）**不在本层重复**，随下层的导入目标与对外面一并传递。

### 3.3 构建工具链

| 项 | 值 |
| --- | --- |
| 构建系统 | CMake ≥ 3.20（使用 `file(GLOB_RECURSE ... CONFIGURE_DEPENDS)` 自动收集源文件） |
| 生成器 | Ninja |
| 编译器 | MSVC（当前只在 Windows / x64 / MSVC 上验证；脚本对 GCC/Clang 亦有分支） |
| 配置 | Visual Studio「CMake 配置」：`x64-Debug`，继承 `msvc_x64_x64` 环境 |
| 库输出目录 | `out/build/<配置>/lib/` |
| 用例门控 | 可见窗口用例（`Renderer_Visible_Test`）默认运行，会弹出真实窗口；无人值守环境设环境变量 `BYJY_RUN_L3=0` 跳过。当前只有 L3 这一级门控，L1/L2 未实现（有意取舍） |

---

## 四、目录结构

```
Application/Test/
├── CMakeLists.txt                 # 主层：运行器 + 图形界面；链三个测试库 + gtest
├── CMakeSettings.json             # Visual Studio CMake 配置（x64-Debug / Ninja）
├── .gitignore                     # 忽略构建产物、.vs、assets/ 联接等
├── README.md
├── EngineTests.exe                # 主可执行产物（落层根目录，已忽略）
├── TestFoundation.exe             # 分层薄入口：只跑 EngineFoundation 用例
├── TestSystem.exe                 # 分层薄入口：只跑 EngineSystem 用例
├── TestRenderer.exe               # 分层薄入口：只跑渲染层用例
├── cmake/
│   ├── 对外接口.cmake             # 本层对外面自描述（BYJY_TEST_OUT_*）
│   ├── 接入下层.cmake             # byjy_jieru_xiaceng() 通用接入函数（与其余两层逐字节相同）
│   └── 测试子工程.cmake           # 三个测试子工程共用的版本解析 / 接入 / 校验逻辑
├── external/                      # 第三方源码副本（本层与子工程各自编译）
│   ├── googletest/                # gtest 框架源码
│   ├── Dear_ImGui/                # ImGui 核心 4 文件 + SDL3 / Vulkan 后端
│   └── glad/                      # 改造前遗留（不参与编译）
├── src/
│   ├── 主调/                      # 主程序与测试模块选择器（含引擎日志屏蔽）
│   └── 单元测试/
│       └── Test/                  # 2 文件：本层主调逻辑自测（其余用例已迁到 tests/）
├── tests/                         # 按被测层拆分的独立测试子工程（各自 CMakePresets）
│   ├── Foundation/
│   │   ├── CMakeLists.txt
│   │   ├── CMakePresets.json
│   │   ├── cmake/对外接口.cmake   # OUT_LIB = TestFoundation
│   │   ├── entry/薄入口.cpp       # TestFoundation.exe 的 main
│   │   └── src/                   # 工具/（12 文件）、核心/（8 文件）、注册锚.cpp
│   ├── System/
│   │   ├── entry/薄入口.cpp
│   │   └── src/                   # 场景/、碰撞/（6 文件）、注册锚.cpp
│   └── Renderer/
│       ├── entry/薄入口.cpp
│       └── src/                   # 50 个渲染用例文件、注册锚.cpp
├── assets/                        # 目录联接 → Application/Game/assets（不入库）
└── out/build/                     # 构建输出目录（不入库）
```

说明：

- `assets/` 是一个**指向 `Application/Game/assets` 的目录联接**，不是本层的真实目录。这样做的目的是让资产保持**单一真源**（实体/属性配置与 Lua 脚本只在Game 层维护一份），同时让本层运行时的相对路径与Game 层一致。联接本身已列入 `.gitignore`（否则 git 会把联接当真实目录递归追踪，产生大量重复条目），因此**克隆本层不会得到 `assets/`**，需要自行建立联接或从Game 层复制。
- `src/主调/` 与 `src/单元测试/Test/` 由本层主工程收编；`tests/<层>/src/` 由各子工程用 `file(GLOB_RECURSE ... CONFIGURE_DEPENDS)` 各自收编，新增文件后重新构建即可，无需手改构建脚本。
- `tests/<层>/` **不是独立 git 仓库**，共用本层仓库；但各自是独立 CMake 工程，有各自的 `out/build/x64-<配置>` 与 `CMakePresets.json`。

---

## 五、核心架构与运行链路

### 5.1 分层与依赖方向

```
         ┌───────────────── Test 层（本层） ─────────────────┐
         │  EngineTests.exe                                 │
         │   ├── main()            分流 / 过滤串 / 收尾等待  │
         │   ├── 测试选择模型        反射 gtest 注册表建树    │
         │   ├── 控制台选择菜单      编号菜单（回退入口）      │
         │   ├── 图形操作界面        SDL3 + Vulkan 常驻操作界面 │
         │   └── RUN_ALL_TESTS()    执行选中的用例           │
         └───────┬──────────────────────────────┬───────────┘
                 │ WHOLE_ARCHIVE 收编            │ 反射 / 过滤
     ┌───────────▼───────────┐          ┌───────▼────────┐
     │ tests/Foundation      │          │  googletest    │
     │ TestFoundation.lib    │          │ (GTest::gtest) │
     ├───────────────────────┤          └───────┬────────┘
     │ tests/System          │                  │ 注册 TEST
     │ TestSystem.lib        │          ┌───────▼────────┐
     ├───────────────────────┤          │ src/单元测试/   │
     │ tests/Renderer        │          │ Test/（自测）   │
     │ TestRenderer.lib      │          └────────────────┘
     └───────────┬───────────┘
                 │ 只链同版次静态库（版本锁解析）
     ┌───────────▼───────────────────────────────┐
     │ EngineFoundation.lib / EngineSystem.lib   │
     │ EngineRenderer.lib                        │
     └───────────────────────────────────────────┘
```

每个测试子工程同时产出薄入口（`Test<层>.exe`），只跑自己那层；主入口把三个静态库整体收编后跑全量。

### 5.2 启动流程（`src/主调/主程序.cpp`）

主程序 `main()` 是全工程的进程入口，流程分六步：

1. 控制台切换 UTF-8 代码页（保证中文用例名可读）；
2. 摘出自定义开关 `--selector=`，其余参数原样保留，重建 `argv` 交给 googletest；
3. 调用 `InitGoogleTest()` 解析 googletest 参数；随后调用三个分层测试库的注册锚，漏链测试库时在此链接期失败；
4. 四步分流——透传 / `off` / `console` / `window`（详见第六节）；
5. 选择结果落地为 `--gtest_filter` 过滤串，调用 `RUN_ALL_TESTS()`；
6. 交互运行结束后停住等待按键，自动化路径直接返回退出码。

一体化引擎时期，这份入口还兼管「核心对象构造 + 依赖注入装配」（即旧「启动流程」，文件为 `主调文件/主调文件.cpp`）；分层重构后该职责已移出本层，本层入口只保留**进程启动、参数分流、测试调度**，来龙去脉见第十三节。

### 5.3 一次完整运行的链路

```
[构建] EngineFoundation.lib → EngineSystem.lib → EngineRenderer.lib
       → tests/Foundation、tests/System、tests/Renderer（各自 Test<层>.lib + Test<层>.exe）
       → 本层 EngineTests.exe（整体收编三个测试库）
[启动] EngineTests.exe（可带 --selector=... / --gtest_*）
   ├─ 交互路径（默认 window / console）：
   │     建立选择模型 → 反射 78 套件 / 1415 用例 → 用户勾选 → 生成过滤串
   └─ 自动化路径（--gtest_* 透传 / --selector=off）：不建模型、不开窗
[执行] GTEST_FLAG(filter) = 过滤串 → RUN_ALL_TESTS() → 逐用例 SetUp / 用例体 / TearDown
[收尾] 交互运行打印提示并等待按键；自动化运行立即返回退出码（0 全过 / 非 0 有失败）
```

---

## 六、主调与测试模块选择器详解

主调模块位于 `src/主调/`，由入口、选择逻辑、引擎日志屏蔽与图形操作界面四部分共 8 个实现文件组成。

### 6.1 主程序（`src/主调/主程序.cpp`）

- **功能**：提供全工程唯一可执行入口 `main()`，完成参数摘取、googletest 初始化、四步分流、过滤串落地与收尾等待。
- **对外接口**：`int main(int argc, char** argv);`
- **实现要点**：
  - 参数摘取循环：凡以 `--selector=` 开头的参数被摘出为选择模式并标记「显式选择器」，不进入保留参数（否则 gtest 会报未知 flag）；凡以 `--gtest_` 开头的参数只做「命令行带过滤」标记，**原样保留**给 gtest；其余参数原样保留。`argv[0]` 做空指针兜底。
  - 四步分流：① 未显式指定选择器且带 `--gtest_*` → 直接透传执行；② `--selector=off` → 不开窗全量运行；③ `--selector=console` → 控制台菜单；④ 默认或 `--selector=window` → 图形操作界面（多轮跑测，界面退出即会话结束），窗口创建失败则打印提示并降级为控制台菜单。
  - 过滤串落地：执行前打印实际过滤串与 `--gtest_filter=` 复跑命令，再写入 `GTEST_FLAG(filter)`，只调用一次 `RUN_ALL_TESTS()`。
  - 收尾：`return 测试结果` 直接透传 `RUN_ALL_TESTS()` 的退出码供 CI 判级。
- **涉及文件**：`主程序.cpp`；依赖 `测试选择模型.h`、`控制台选择菜单.h`、`图形操作界面.h`、`退出等待判定.h`、`引擎日志屏蔽.h`。

### 6.2 测试选择模型（`src/主调/测试选择模型.h` / `.cpp`）

- **功能**：纯逻辑层，不依赖任何图形库。通过 googletest 反射枚举当前已注册的套件与用例，建立两层选择树；维护勾选态（全选 / 半选 / 单条）；生成 `--gtest_filter` 过滤串；解析控制台编号输入。图形操作界面、控制台菜单与单元测试共用同一模型。
- **对外接口**：

```cpp
namespace engine
{
    // 选择树的两级条目
    struct Case_Entry { std::string name; bool checked = true; };
    struct Suite_Entry { std::string name; std::vector<Case_Entry> case_list; bool expanded = false; };

    class Test_Selection_Model
    {
    public:
        void suite_tree_build();                        // 反射建树（默认全部勾选）
        const std::vector<Suite_Entry>& suite_table_get() const;
        std::size_t suite_count() const;                // 另有 case_count / checked_case_count
        bool suite_all_checked(std::size_t 套件序号) const;   // 另有 suite_half_checked / suite_expanded
        void suite_check_set(std::size_t 套件序号, bool 勾选); // 另有 case_check_set / suite_expand_set
        void all_check();                               // 另有 all_clear / check_invert
        std::string filter_string_build() const;         // 三态压缩（见实现要点）
    };

    bool 解析控制台选择(const std::string& 输入, std::size_t 套件数, std::vector<bool>& 勾选表);
}
```

- **实现要点**：
  - 反射建树：取 `::testing::UnitTest::GetInstance()` 单例，遍历 `total_test_suite_count()` 个套件、每个套件 `total_test_count()` 个用例，登记条目并默认勾选。
  - 过滤串三态压缩：**全不选**退化为 `"*"`（与 gtest 通配语义一致，即全跑）；**整套件全选**压缩为 `套件.*`；**部分勾选**逐条列 `套件.用例`；多条片段以 `':'` 连接。
  - 控制台输入解析：编号从 1 开始；支持 `1,3-5` 区间（左闭右闭且左值不大于右值）；支持 `all`/`ALL`/`a`/`*` 全选；空段、非法字符、越界编号一律整体判非法；数字解析设上限防御溢出。
- **涉及文件**：`测试选择模型.h` / `.cpp`。

### 6.3 控制台选择菜单（`src/主调/控制台选择菜单.h` / `.cpp`）

- **功能**：无图形环境（或用户显式指定）时的文本交互入口：打印套件编号列表，读取用户输入，把选择结果应用到模型并输出过滤串。
- **对外接口**：`void engine::控制台选择菜单_运行(Test_Selection_Model& 模型, std::string& 过滤串);`
- **实现要点**：
  - 打印标题与逐条编号列表（`N) 套件名（M 个用例）`）。
  - 最多接受三次非法输入，其后兜底全跑；输入流结束（管道/重定向）与空输入（直接回车）均按全选处理。
  - 控制台菜单按**套件粒度**勾选；用例粒度是图形操作界面的专属能力（有意取舍，非缺陷）。
- **涉及文件**：`控制台选择菜单.h` / `.cpp`。

### 6.4 图形操作界面（`src/主调/图形操作界面.h` / `.cpp`）

- **功能**：默认交互入口。经渲染层 UI 通道（SDL3 窗口 + Vulkan 交换链 + Dear ImGui）常驻一个操作界面：套件/用例勾选、按轮运行、实时进度、结果表、失败详情、gtest 输出镜像与引擎日志面板；同进程内可连续多轮跑测，退出界面即结束会话。
- **对外接口**：`bool engine::图形操作界面_运行(Test_Selection_Model& 模型);`（窗口创建失败返回 `false`，调用方退控制台菜单）
- **实现要点**：
  - 初始化时序：`Renderer::init`（渲染层自建 SDL3 窗口与交换链）→ 取 `ui_channel()` → `ImGui::CreateContext` → `ImGui_ImplSDL3_InitForVulkan` → `ImGui_ImplVulkan_Init` → 加载中文字体 → 安装测试运行器 → 注册 `draw_callback` 与 `event_callback`；任一步失败即清理并返回 `false`。
  - 绘制时序：`ImGui_ImplSDL3_NewFrame` → `ImGui_ImplVulkan_NewFrame` → `ImGui::NewFrame` → **在铺满视口的 `ImGui::Begin` 面板内**绘制界面 → `ImGui::Render` → `ImGui_ImplVulkan_RenderDrawData`（命令缓冲由渲染层帧链路给出）。
  - 输入：渲染层事件泵逐事件转发到 `ui_事件回调`，再交 `ImGui_ImplSDL3_ProcessEvent`；界面因此可交互。
  - 多轮：测试在**工作线程**执行（`Test_Runner`），界面线程只读加锁快照，跑测期间界面不冻结；停止语义为「跑完当前轮后不再排队」。
  - 不写 `imgui.ini`；中文字体按候选链（微软雅黑 → 等线 → 黑体 → 宋体 → 楷体；非 Windows 另有一组）逐个探测，全部失败时界面仍可运行（中文显示为方块）。
- **配套模块**：
  - `测试界面.h/.cpp`：界面绘制（选择树、运行控制、进度、结果表/失败详情、输出镜像、轮次历史、引擎日志尾部）。
  - `测试运行器.h/.cpp`：运行状态机 + 工作线程 + 只读快照；界面模式安装输出镜像并释放默认打印器，退出时摘除监听器。
  - `gtest输出镜像.h/.cpp`：自实现 `TestEventListener`，把 gtest 事件采成用例结果并按控制台风格格式化为输出行。
  - `测试运行记录.h`：`Case_Result` / `Round_Summary` / `Output_Line` 数据结构。
- **涉及文件**：`图形操作界面.h/.cpp`、`测试界面.h/.cpp`、`测试运行器.h/.cpp`、`gtest输出镜像.h/.cpp`、`测试运行记录.h`；第三方 `external/Dear_ImGui`（SDL3 + Vulkan 后端）；Vulkan SDK / SDL3 头与链接库由渲染层对外面传递。

### 6.5 退出等待判定（`src/主调/退出等待判定.h`）

- **功能**：判断本次运行结束后是否要停住等待用户按键。交互运行（默认 / `window` / `console`）需要停住，避免控制台一闪而过；自动化运行（透传 / `off`）必须跑完即走，否则 CI 会被挂住。
- **对外接口**：`bool engine::需要等待退出(const std::string& 选择模式, bool 命令行带过滤, bool 显式选择器);`
- **实现要点**：
  - 判定表（实现为两个早退 `return false` + 一个兜底 `return true`）：

    | 选择模式 | 命令行带过滤 | 显式选择器 | 结果 | 场景 |
    | --- | --- | --- | --- | --- |
    | window | 否 | 否 | 停 | 默认双击运行 |
    | window / console | 否 | 是 | 停 | 显式指定选择器 |
    | window / console | 是 | 否 | 不停 | gtest 参数透传（自动化） |
    | window / console | 是 | 是 | 停 | 显式选择器优先于透传约定 |
    | off | 任意 | 任意 | 不停 | ctest / 一键全量回归 |
    | 未知模式 | 任意 | 是 | 停 | 兜底：宁可多等一次 |

  - 头文件只放声明，**实现位于 `主程序.cpp`**：用例直接链接可执行文件里的同名符号，保证「用例验证的判定」与「主程序实际使用的判定」是同一份逻辑。
  - 停住动作（文件内部工具 `等待用户关闭窗口()`）：无控制台窗口时直接跳过；否则打印提示、清空控制台残留按键（避免确认用的回车被立刻吃掉）、阻塞等待任意键。
- **涉及文件**：`退出等待判定.h`（声明）+ `主程序.cpp`（实现）。

---

## 七、单元测试体系

用例按被测层拆到 **独立测试子工程** `tests/<层>/`（与项目导航的层划分一致），每个子工程都是独立 CMake 工程，只读全链版本锁、只链同版次静态库；本层主调自测留在 `src/单元测试/Test/`。共 **78 个用例源文件**（.cpp）、**78 个套件**、**1415 个用例**。套件名 = googletest 夹具类名。

### 7.1 规模统计

| 层级 | 路径 | 源文件 | 套件数 | 用例数 |
| --- | --- | --- | --- | --- |
| EngineFoundation · 工具 | `tests/Foundation/src/工具/` | 12 | 12 | 275 |
| EngineFoundation · 核心 | `tests/Foundation/src/核心/` | 8 | 9 | 264 |
| EngineSystem · 碰撞 | `tests/System/src/碰撞/` | 5 | 4 | 58 |
| EngineSystem · 场景 | `tests/System/src/场景/` | 1 | 1 | 13 |
| Renderer | `tests/Renderer/src/` | 50 | 50 | 767 |
| 测试层（主调自测） | `src/单元测试/Test/` | 2 | 2 | 38 |
| **合计** | — | **78** | **78** | **1415** |

最近一次全量回归（`EngineTests.exe --selector=off`，设 `BYJY_RUN_L3=0` 跳过可见窗口用例）为 **1415 例 / 1413 通过 / 2 跳过 / 0 失败**；两个跳过均为四叉树用例对引擎在建缺陷的 `GTEST_SKIP`（见 10.3）。

各子工程也可单独跑：`TestFoundation.exe` 跑 21 套件 / 539 例，`TestSystem.exe` 跑 5 套件 / 71 例，`TestRenderer.exe` 跑 50 套件 / 767 例。

### 7.2 覆盖内容

| 层级 | 套件（夹具类名） | 被测对象 |
| --- | --- | --- |
| 测试层（主调） | `测试选择模型测试`、`退出暂停判定测试` | 本层 `Test_Selection_Model`、`需要等待退出` |
| EngineFoundation · 工具 | `Binary_Search_Test`、`Engine_Env_Test`、`Number_Allocator_Test`、`Data_Validator_Test`、`Field_Check_Test`、`Hash_Combine_Test`、`Log_Test`、`Mesh_Loader_Test`、`Timer_Test`、`Path_String_Test`、`Path_Check_Test`、`Random_Generator_Test` | EngineFoundation 工具模块组：二分查找、引擎环境、数值分配器、数据校验器、字段校验、哈希组合、日志系统、网格加载器、计时器、路径字符串转换、路径检查、随机数生成器 |
| EngineFoundation · 核心 | `Event_Test`、`Event_Broker_Test`、`Event_Terminal_Test`、`Quadtree_Test`、`Quadtree_Manager_Test`、`Coord_Type_Test`、`Object_Pool_Test`、`Object_Test`、`Config_Loader_Test` | EngineFoundation 核心模块：事件结构体、事件中转器、事件终端、四叉树、四叉树管理器、坐标类型、对象池、对象基类、配置加载器 |
| EngineSystem · 碰撞 | `Collider_Test`、`Collider_Move_Test`、`Collision_Region_Test`、`Collision_Proxy_Test` | 系统层碰撞：碰撞体、位移链路、空间边界、碰撞代理 |
| EngineSystem · 场景 | `EngineSystem_LevelText_Test` | 系统层关卡文本解析与往返 |
| Renderer | `Renderer_*` 等 50 个套件（初始化、交换链重建、帧、GPU 统计、网格、纹理、采样器、着色器、管线、场景、相机、文字、后处理链、可见窗口等） | 渲染层公共面：设备/平台/交换链/帧/内存/管线/网格/纹理/场景/文字/后处理 |

> 碰撞用例虽然原先放在 EngineFoundation 目录下，但直接 include 系统层头（`Engine/EngineSystem/src/collision/...`），本轮已归入 `tests/System`，避免 Foundation 子工程被迫依赖系统层。

### 7.3 构建目标

| 目标 | 所在工程 | 类型 | 说明 |
| --- | --- | --- | --- |
| `TestFoundation` | `tests/Foundation` | STATIC | Foundation 用例 + 注册锚 + 引擎日志屏蔽 |
| `TestFoundation.exe` | `tests/Foundation` | 可执行文件 | 薄入口，只跑 Foundation 用例 |
| `TestSystem` / `.exe` | `tests/System` | STATIC / 可执行文件 | 同上，只跑系统层用例 |
| `TestRenderer` / `.exe` | `tests/Renderer` | STATIC / 可执行文件 | 同上，只跑渲染层用例 |
| `TestGui` | 本层 | STATIC | Dear ImGui 核心 4 文件 + SDL3/Vulkan 后端，仅供图形操作界面使用 |
| `TestLauncher` | 本层 | STATIC | 测试选择模型 + 控制台菜单（引擎日志屏蔽已移入 TestFoundation） |
| `EngineTestObjects` | 本层 | OBJECT | 本层主调自测的 `.obj`（2 个用例文件） |
| `EngineTests` | 本层 | 可执行文件 | 主调自测 `.obj` + 主程序 + 图形界面 + WHOLE_ARCHIVE 收编三个测试库 |

设计要点：

- 三个测试库是**静态库**，跨构建树无法用 OBJECT 库；为防「未引用 `.obj` 被链接器丢弃、`TEST` 注册静默消失」，链接方一律用 `$<LINK_LIBRARY:WHOLE_ARCHIVE,Test<层>>` 整体收编。这是本体系的关键决策，改回普通链接会静默少跑用例。
- 每个测试库额外提供 `extern "C" byjy_test_anchor_<层>()`；薄入口与主入口都调用它 —— 漏链测试库时链接期直接失败，而不是安静地少跑一批。
- `TestLauncher` 与 `TestGui` 按**依赖方向**切开：图形操作界面依赖 ImGui，单独编进 `EngineTests`；纯逻辑部分供主程序使用。
- 不链接 `GTest::gtest_main`：`main` 由本层主程序与各薄入口提供。
- 每个子工程各自编一份 googletest（构建期重复编译可接受），但每个可执行文件只链一份；测试库不内嵌 gtest 目标。

### 7.4 按层独立验证与版本锁

每个子工程在配置期做的事（共用逻辑见 `cmake/测试子工程.cmake`）：

1. 读 `Engine/版本锁.cmake`，按 `Debug` / `Release` 分别解析依赖层版本：锁空 → 跟随该层最新构建树；锁非空 → 取 `Engine/版本缓存/<层>/x64-<配置>/<哈希>/` 冻结副本。子工程只读锁，不改写。
2. 用 `byjy_jieru_xiaceng()` 建立 IMPORTED 静态库目标，并逐配置注入版次锚点（头与库不同版次时链接期 LNK2019）。
3. 逐配置做全链一致性校验：被接入层构建时用的下层版本必须与本次解析一致，否则 `FATAL_ERROR`（锁必须成套）。
4. 逐配置做头完整性校验：子工程源码引用的下层头必须都在快照里，缺失即配置期报错。
5. 产出用例静态库 + 薄入口。

因此：Foundation 改动者只需构建 `tests/Foundation`；System / Renderer 的测试库不会因为 Foundation 未升锁而整体挂掉 —— 它们锁在各自已验证的版本上。升级锁定用工作站脚本 `升级测试锁.ps1` 一次性写入六条锁；锁值落后于当前构建树时配置期会打印「版本锁滞后」WARNING。

---

## 八、对外接口契约

### 8.1 本层对外面（`cmake/对外接口.cmake`）

本层对外面遵循统一契约，导出六个变量（前缀 `BYJY_TEST`）：

| 变量 | 值 / 含义 |
| --- | --- |
| `BYJY_TEST_OUT_LIB` | 空（本层不产出供上层链接的静态库；用例库在各子工程里） |
| `BYJY_TEST_OUT_LIBDIR` | 空 |
| `BYJY_TEST_OUT_INC` | 接 Engine 聚合面（`${BYJY_ENGINE_OUT_INC}`，其首项为项目根） |
| `BYJY_TEST_OUT_DEF` | 接 Engine 聚合面编译定义 |
| `BYJY_TEST_OUT_LINK` | 接 Engine 聚合面第三方库 |
| `BYJY_TEST_OUT_SYS` | 空（窗口 / 图形系统库由 `EngineRenderer` 对外面提供） |

- 三个测试子工程各自维护 `tests/<层>/cmake/对外接口.cmake`（`OUT_LIB = Test<层>`），主层按同一契约定位它们的静态库。
- 「本层对外面 = 本层自有面 + 下层对外面」的并入动作，由本层对外接口文件**直接 include `Engine/cmake/对外接口.cmake`** 完成（纯数据赋值，重复 include 无副作用）；聚合面又并入了 EngineSystem 与 EngineFoundation 两面，因此接入本层即取得引擎全部公共路径。
- 本层自有包含目录由**项目根**统一承担（`Application/Test/...` 全路径），该条目已在聚合面首项里，故此处只承接。
- 备注：本层是拓扑最上层，当前没有使用方，本文件仅作自描述；`EngineTests.exe` 与三个薄入口都不写进对外面。

### 8.2 接入下层（`cmake/接入下层.cmake`）

本层通过 `cmake/接入下层.cmake` 提供的两个函数接入下层：`byjy_jieru_xiaceng(<下层对外接口文件> <对外接口变量前缀> <覆盖库路径变量名>)` 与 `byjy_tou_kuaizhao_gen(<结果变量> <导入目标>)`。该文件在 **EngineSystem / Engine / Test / Game 四份逐字节相同**，改动须同步四处（EngineFoundation 无下层，无此文件）。

调用方式（本层**只接 Engine 聚合层一次**）：

```cmake
byjy_jieru_xiaceng("${PROJECT_ROOT_DIR}/../../Engine/cmake/对外接口.cmake" "BYJY_ENGINE" "BYJY_ENGINE_LIB_PATH")
```

聚合面 `OUT_LIB = "EngineSystem;EngineFoundation"`，函数会**逐库**定位。

**三级库定位顺序**（任何情况下都不回退编译下层源码）：

1. **覆盖变量优先**：`BYJY_ENGINE_LIB_PATH` 非空（**分号列表**，顺序同 `OUT_LIB`，须与库数等长）且指向已存在的文件 → 直接使用；非空但文件不存在 → 直接 `FATAL_ERROR`，不静默降级。
2. **自动探测**：覆盖变量留空 → 只认 `<该库所在层目录>/out/build/x64-<当前配置>/lib/<库名>.lib`（多配置生成器配置期无 `CMAKE_BUILD_TYPE` 时默认 Debug，实际分配的库由 `IMPORTED_LOCATION_<CONFIG>` 覆盖）；不扫描其它配置的构建树，避免跨配置混链（`/MD` 配 `/MDd`、版次锚点失配）。
3. **硬失败**：仍找不到 → `FATAL_ERROR`，并给出构建相应下层的命令。

定位成功后，以下层声明库名为目标名建立 `IMPORTED STATIC GLOBAL` 静态库（`EngineFoundation`、`EngineSystem` 两个目标），把包含目录 / 编译定义 / 链接库挂到其 `INTERFACE` 上，供本层继续传递。

### 8.3 分层契约的硬约束

- 上层**只链接下层已构建的 `.lib`**，绝不通过 `add_subdirectory` 把下层源码拉进本层构建树重复编译。
- 包含路径沿依赖方向叠加：本层接入聚合面后自动获得引擎两层全部包含路径（项目根、`external/Json`、`external/bullet3/src`、`external/Sol2/include`、`external/Lua` 等），渲染层再补 Vulkan / SDL3 / VMA 公共面，无需重复声明。
- 本层编译包含根次序为 `[EngineFoundation 快照根, EngineSystem 快照根, EngineRenderer 快照根, 项目根]`：
  三者由版本解析（`cmake/版本工具.cmake`）给出并**排在项目根之前**，防止 `Engine/...` 命中源码树、绕过快照。
  版本锁为空时取各层构建树快照；非空时取 `Engine/版本缓存/<层>/x64-<配置>/<版本哈希>/include`。
- 下层构建树支持两种布局：Ninja 双预设（`out/build/x64-<配置>/{lib,include}`）与 Visual Studio（`out/build/<树>/lib/<配置>`、`include/<配置>`）。锁空时**先找 Ninja 布局，缺失才回退 VS 布局**；同一层两棵树并存时优先 Ninja。
- 每层构建期还会做一次**头完整性兜底**：本层源码引用的下层头必须都在快照里，缺失即配置期报错（典型原因：下层新增头但没重建）。

---

## 九、构建与运行

### 9.1 前置条件

本层在配置阶段就会定位下层的**已构建静态库**，找不到即硬失败。因此必须**逐层构建**，顺序不能颠倒：

1. 构建 EngineFoundation，产出 `Engine/EngineFoundation/out/build/<配置>/lib/EngineFoundation.lib` 与其对外头快照；
2. 构建 EngineSystem，产出 `Engine/EngineSystem/out/build/<配置>/lib/EngineSystem.lib` 与其对外头快照；
3. 构建 EngineRenderer，产出 `Engine/EngineRenderer/out/build/<配置>/lib/EngineRenderer.lib` 与其对外头快照；
4. （可选自检）构建 Engine 聚合层，确认两个子层库齐备；
5. 构建三个测试子工程：`tests/Foundation`、`tests/System`、`tests/Renderer`（彼此独立，可只建要验证的那一个）；
6. 再配置并构建本层（需要三个 `Test<层>.lib` 已存在）。
   各层构建成功后会同时把「头快照 + 库」按版本哈希发布到 `Engine/版本缓存/`；若 `Engine/版本锁.cmake` 里锁定了某个版本，本层与子工程都会改从该缓存取用。

### 9.2 构建命令

```bash
# 配置（CMake ≥ 3.24，Ninja 生成器，C++20）
cmake -S . -B out/build/x64-Debug -G Ninja -DCMAKE_BUILD_TYPE=Debug

# 构建（产物：层根目录 EngineTests.exe）
cmake --build out/build/x64-Debug
```

分层测试子工程各自独立配置与构建（以 Foundation 为例，System / Renderer 同理）：

```bash
cmake -S tests/Foundation -B tests/Foundation/out/build/x64-Debug -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build tests/Foundation/out/build/x64-Debug
./TestFoundation.exe --gtest_filter=...
```

> 已提供 `CMakePresets.json` 时也可用 `cmake --preset x64-Debug` / `cmake --build --preset build-Debug`；`-DCMAKE_BUILD_TYPE` 不能省，否则单配置生成器会按空配置编译（无 `/O2`，且版本解析会按 Debug 处理）。

- **配置期须处于 UTF-8 代码页**（先进 VS 开发人员环境，再 `chcp 65001`），否则头文件依赖不会被记录；`TMP`/`TEMP` 须为**纯 ASCII** 路径（详见顶层 README 的构建说明）。
- 本层与各子工程**按下层对外头快照编译**，不再实时读取下层源码树：配置期由版本解析给出快照根，并把它们列为**自身包含目录、排在项目根之前**。因此「改了某个下层的公共头但没重建该层」时不会看到新头 —— 正是为了避免新头配旧库；要让新头生效，先构建对应下层。快照目录缺失会在配置期直接报错。
- **增删文件自动重配**：各工程的 GLOB 基点目录均登记进 `CMAKE_CONFIGURE_DEPENDS`；新增用例文件后**直接构建**即被收编，不必手动重生成缓存。
- **源文件经显式清单提供**：主层的 `源文件列表_TestLauncher.cmake` / `源文件列表_EngineTestObjects.cmake` / `源文件列表_EngineTests.cmake`，以及各子工程的 `源文件列表_Test<层>.cmake`（均为生成物，由共享脚本 `Engine/EngineFoundation/cmake/生成源文件列表.cmake` 维护）。其用途与「在 VS 文件视图里改名 / 迁移」的完整说明见 `Engine/EngineFoundation/README.md` 第 8.5 节。
- 运行期引擎日志被**屏蔽到文件** `out/引擎运行日志.txt`（由 `src/主调/引擎日志屏蔽.cpp` 把日志根节点
  绑到该文件，该实现编入 TestFoundation.lib），从而不与 googletest 的输出混在同一位置；日志系统用例本身要验证「未绑定即走控制台」，
  其夹具会临时关闭屏蔽、用例结束再恢复。

如需跳过自动探测、显式指定下层库路径（**分号列表，顺序同 `EngineSystem;EngineFoundation`**）：

```bash
cmake -S . -B out/build/x64-Debug -G Ninja \
      -DBYJY_ENGINE_LIB_PATH="<EngineSystem.lib 路径>;<EngineFoundation.lib 路径>"
```

在 Visual Studio 中，用「打开本地文件夹」选中层目录，会读取本层 `CMakeSettings.json` 自动生成 `x64-Debug（Ninja）` 配置。

### 9.3 命令行参数

| 命令 | 行为 | 是否开窗 | 结束后是否停住 |
| --- | --- | --- | --- |
| `EngineTests.exe` | 默认 `window`：图形窗口树形勾选；窗口不可用时自动降级为控制台菜单 | 是 | 是 |
| `EngineTests.exe --selector=window` | 显式指定图形窗口（同样支持失败降级） | 是 | 是 |
| `EngineTests.exe --selector=console` | 控制台编号菜单，支持 `1,3-5` 区间与 `all` 全选，直接回车表示全跑 | 否 | 是 |
| `EngineTests.exe --selector=off` | 不开窗、不建模型，直接全量运行（ctest 使用） | 否 | 否 |
| `EngineTests.exe --gtest_filter=套件.用例` | 命令行已带 `--gtest_*` 且未显式指定选择器时直接透传，不弹窗 | 否 | 否 |
| `EngineTests.exe --gtest_list_tests` | 列出全部套件与用例（走透传分支） | 否 | 否 |

- 交互运行（window / console）跑完会打印提示并停住等待按键；自动化路径（透传 / off）不等按键，退出码不受影响。
- **显式 `--selector=` 优先于「带 gtest 参数即透传」的约定**：`--selector=console --gtest_filter=...` 会走控制台菜单，且菜单结果会覆盖命令行过滤串。
- 未知选择模式（如 `--selector=xxx`）会落到图形窗口分支，窗口失败再退控制台菜单；退出等待判定对未知模式兜底为「停住」。

### 9.4 ctest

在构建目录执行 `ctest`。注册策略分两路：

- 工程路径为**纯 ASCII** 时，用 `gtest_discover_tests` 逐用例注册；
- 工程路径含**非 ASCII**（本仓库路径含中文）时，退化为整体注册一个用例，命令等价于 `EngineTests --selector=off`。

退化的原因：`gtest_discover_tests` 会在构建期以 `--gtest_output=json:<文件>` 枚举用例，而 googletest 写该报告文件时按 ANSI 代码页解析路径，中文路径必然失败，构建会被判失败。退化分支用 `--selector=off` 不开窗直接全跑，无参数的 `ctest` 不会阻塞在图形窗口上。

---

## 十、实现状态与已知问题

### 10.1 已完成

- 分流固化：透传 / `off` / `console` / `window` 四条路径与失败降级均已实现并有回归。
- 过滤串三态压缩与退出等待三参数判定均有专门用例覆盖。
- 单元测试体系 78 套件 / 1415 用例（Debug 与 Release 各跑一遍，均为 1413 通过 / 2 跳过 / 0 失败），`ctest` 1/1 通过。
- 产物落层根目录，资源相对路径解析与Game 层一致。
- 层间契约就位：只链接两层已构建静态库，无源码回退路径。

### 10.2 尚未完成

- `src/` 下功能测试代码（`TestCore` 目标）当前为空，尚未产出 `TestCore.lib`；测试能力集中于单元测试与选择器。
- Game 层的模块尚未被本层用例覆盖（当前用例集中在EngineFoundation 层核心与EngineSystem 层工具模块）。

### 10.3 已知问题

| 位置 | 现象 |
| --- | --- |
| 图形操作界面 | 鼠标点选交互尚未自动化，回归只覆盖「窗口创建失败降级」与「窗口存活」路径，点选本身依赖人工验证；字体候选链全部失败时界面仍可运行，但中文显示为方块（仅影响观感） |
| 四叉树 | 超大树上（`1ull<<33` 量级边长上限）的范围查询尚未专项验证；`退化树单点查找` 因引擎对中间节点取区块数据抛 `std::bad_variant_access` 而 `GTEST_SKIP`；`退化树创建被拒绝` 因引擎已移除退化解拦截而 `GTEST_SKIP`。两处跳过均为引擎侧在建缺陷，待引擎修复后取消跳过 |
| 过滤串生成 | 用例名/套件名若含 `:`、`*`、`?` 等 gtest 过滤串特殊字符，直接拼接会导致过滤串断裂；当前依赖命名规范规避，属已知边界 |
| 控制台输出捕获 | 日志系统用例不能用 gtest 自带的 `CaptureStdout`（其临时文件路径含中文时会 FATAL），改用自研 `Console_Capture`；新增需要捕获输出的用例须沿用该做法 |

### 10.4 设计取舍（答疑结论）

以下为历次「待答疑」中属于**有意取舍**的条目，按当前实现定案，不再作为缺陷处理：

| 主题 | 结论 |
| --- | --- |
| 控制台菜单粒度 | 仅支持套件粒度；用例粒度是图形操作界面的能力（有意取舍） |
| 选择树是否纳入被禁用/跳过用例 | 纳入。建树登记全部用例，实际执行与跳过由 gtest 决定 |
| 空白输入 | 控制台菜单拦截为全选；模型解析层把空串判为非法输入 |
| 过滤串阈值 | `1000000` 为输入编号上限，防止超长输入；边界按当前实现固定 |
| 分级门控 L1/L2/L3 | 只实现 L3（可跳过可见窗口用例）；L1/L2 未实现 |
| `Test_Object` 同名类型 | `对象测试.cpp` 与 `对象池测试.cpp` 各自定义、成员完全相同，属**有意隔离**（两棵树互不共享夹具） |
| `Collider_Test` / `Collider_Move_Test` | 覆盖角度不同（基础行为 vs 位移链路），保留两套 |
| 夹具分散、无共享头 | 有意取舍：用例文件自包含，避免用例间耦合 |
| 中文夹具名/用例名 | 遵循工程中文命名约定 |
| `Mesh_Loader` 不做顶点去重 | 有意简化，加载器只负责解析与索引重建 |
| 对象池 `capacity/size` | 以实现为准：`size` 为已分配记录数，`capacity` 为记录容器容量 |
| 数值分配器回绕/哨兵 | 以实现为准：到达最大值后回绕，`0` 为非法哨兵 |
| `Timer::elapsed(name, true)` | 以实现为准：读取后重置该计时项 |
| 随机数生成器 | 缺省构造以随机设备为种子；无参取值区间以实现为准 |
| 二分查找子区间索引 | 以实现为准：子区间索引相对整段起点 |
| 定向投递判定主体 | 由事件中转器的 ACL 键登记决定；用例按当前实现断言 |
| 位移持续/作废时序 | 按引擎当前时序语义断言 |
| `quadtree_merge` | 合并条件与未配对树的处理按当前实现断言 |
| 渲染用例的像素判定 | 一律采用**结构性/相对断言 + 诊断记录**，不硬编码换机敏感的绝对值；容差与几何前提写在用例文件头 |

---

## 十一、开发指南

### 11.1 如何新增一条用例

1. 在对应子工程的 `tests/<层>/src/<工具|核心|场景|碰撞|渲染...>/` 下打开或新建 `被测模块测试.cpp`（本层主调自测放 `src/单元测试/Test/`）；
2. 用例文件通用骨架：文件头一句话说明覆盖范围 → `#include <gtest/gtest.h>` → include 被测头文件（直接 include 真实源码头验证真实现）→ 夹具类（`class Xxx_Test : public ::testing::Test`）→ `TEST_F(套件名, 中文用例名)`；
3. 夹具三形态：空夹具（纯函数）、`protected` 成员被测对象（每用例独立构造）、静态辅助函数夹具（复用建树/写临时文件等公共逻辑）；
4. 重新构建对应子工程，`file(GLOB_RECURSE ... CONFIGURE_DEPENDS)` 会自动收编新文件，**无需手改构建脚本**（除非新用例需要额外链接库）；
5. 运行验证：`Test<层>.exe --gtest_filter=新套件.*` 单层验证，或重建主层后用 `EngineTests.exe --gtest_filter=...` / `--selector=off` 跑全量。

### 11.2 如何新增一个测试套件

- 若只是新增一个夹具类，直接在新建的 `*测试.cpp` 里 `class 新套件名_Test : public ::testing::Test` 并写 `TEST_F` 即可，套件名即夹具类名。
- 同一个 `.cpp` 文件可以注册多个套件（如 `碰撞测试.cpp` 含三个套件）。
- 新增后**反射驱动自动可见**：图形窗口与控制台菜单会直接出现新套件，无需改主调代码。

### 11.3 代码风格约定

- 命名空间统一为 `engine`；文件名 / 类名 / 注释 / 用例名用中文；英文标识符单词间用 `_` 分隔且首字母不大写（类名首字母大写）；
- 注释：汉字注释优先；不足三行用 `//` 且 `//` 后不留空格，三行及以上用 `/* */`；采用换行注释；函数头注释翻译函数名，函数体内为变量声明、`if` 条件、函数调用加注释；
- 源码统一 UTF-8，MSVC 编译选项必须带 `/utf-8`（中文源码、中文 include 路径、中文用例名都依赖它）；
- 临时文件（如网格加载用例写出的 `.obj`）必须在用例末尾删除，并用 `std::error_code` 容错；日志系统用例的落盘探针已纳入 `.gitignore`。

---

## 十二、路线图

- [ ] 补足 `src/` 下的功能测试代码，让 `TestCore` 目标真正产出静态库
- [ ] 图形操作界面的鼠标点选交互自动化，把「选择 → 过滤串 → 执行」全链路纳入回归
- [ ] 扩大用例覆盖面至EngineSystem 层的实体/属性/效应模块
- [ ] 四叉树超大树上范围查询的专项验证
- [ ] 过滤串对特殊字符的转义或校验，消除命名相关边界
- [ ] 为Game 层预留用例接入位（本层已同时接入EngineFoundation 层与EngineSystem 层，将来出现 Game 层用例不必再改接入代码）

---

## 十三、历史沿革

本工程曾是一体化引擎（不分层），当时工程根有一份题名《游戏引擎》的长篇 README（733 行，对应版次 `0b8dbd27`）。分层重构后，那份 README 被删去，内容各归其位。与本层相关的对应关系如下：

| 旧（一体化引擎时期） | 现（分层后） | 说明 |
| --- | --- | --- |
| `主调文件/主调文件.cpp`（唯一入口，负责构造核心对象 + 依赖注入装配 + 事件中枢接入） | `Application/Test/src/主调/主程序.cpp` 等六个文件 | 入口职责被拆到Test 层主调；旧文件承担的对象装配职责已移出本层，本层入口只保留进程启动与测试调度 |
| 旧「启动流程」：构造四个核心对象并注入 `attach_entry` / `event_entry` / `event_set_entry` / `prop_bind_entry` 通道 | 已取消，不再属于本层 | 「依赖注入装配」随分层重构移出Test 层 |
| 旧「宿主组合根」与分层宿主（引擎宿主 / 系统宿主 / 全栈宿主） | 已取消 | 宿主把下层源码拉进本层构建树重复编译，与「层间一律链接已构建静态库」的契约冲突；且与测试入口职责重叠，故只保留一个入口 |
| `TestEngine.exe`（旧引擎测试入口） | `EngineTests.exe` | 改名并升级为全工程唯一可执行文件 |
| EngineFoundation 层 `EngineFoundation.lib` 与旧测试入口同树构建 | 各自产出静态库，本层只消费 | 层间契约由此确立 |

构建脚本中留有明确的旁证注释：本层「过去是宿主产地」，宿主机制取消后，可执行文件只在Test 层产生。

---

## 十四、许可

本层不单独维护许可证文件，许可见工程根目录的 `LICENSE`。

---

*本文档依据本层实际源码、构建脚本、`.gitignore` 与工程约定逐条核对后编写。接口、目标名与目录以源码为准，若发现不一致，请以源码为真。*