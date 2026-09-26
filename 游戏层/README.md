# 白银纪元 · 游戏层

游戏层是「白银纪元」四层仓库中依赖链顶端的**游戏内容层**。当前为**骨架状态**：没有源码、不产出静态库，只保留接入层间契约所需的构建骨架与 `assets/` 资产数据。

## 一、本层在依赖链中的位置

```
引擎层 ← 系统层 ← 游戏层
```

- 依赖方向单向：游戏层依赖系统层，系统层依赖引擎层，不反向依赖。
- 本层只链接系统层**已经构建好的**静态库 `SystemCore`（`SystemCore.lib`），绝不把下层源码拉进本层构建树编译，也没有任何源码回退路径。
- 系统层的对外面已并入引擎层的对外面，因此链接 `SystemCore` 即同时取得系统层与引擎层两层的公共包含路径、编译定义与第三方链接库。

## 二、目录结构

```
游戏层/
├── CMakeLists.txt          # 构建脚本（接入 SystemCore + 条件产出 GameCore）
├── CMakeSettings.json      # VS 的 CMake 集成配置（含已无消费方的 BYJY_JOIN_TEST_HOST 遗留项）
├── .gitignore              # 忽略 out/、编译产物、IDE 目录
├── cmake/
│   ├── 对外接口.cmake       # 本层对外面 = 本层自有面 + 系统层对外面
│   └── 接入下层.cmake       # byjy_jieru_xiaceng()：把下层已构建静态库接进本层
├── assets/                 # 资产根（运行时数据；相对 assets/ 的路径即数据契约）
│   ├── config/
│   │   ├── entities/       # 实体配置：au.json 与 6 个怪物实体配置
│   │   ├── property/       # 属性槽配置：au.json 与 6 个怪物属性配置
│   │   ├── format/         # 格式定义：Entity_Manager.json、Property_Manager.json
│   │   └── route/          # 路由表：entity.json、property.json
│   └── scripts/
│       ├── behavior/       # 行为（决策树）脚本：哥布林 (Goblin)_Behavior.lua
│       └── initialize/     # 属性槽初始化脚本：哥布林 (Goblin).lua
├── src/                    # 空目录（本层尚无源码，CMake 在此收集 .cpp/.c）
└── external/               # 空目录
```

> 根目录另有未被 git 跟踪的构建产物（`TestEngine.exe`、`.ilk`、`.pdb`），属旧宿主可执行文件的遗留，已被 `.gitignore` 覆盖。

## 三、现状说明

- **本层当前无源码**：`src/` 为空，构建脚本收集到空源文件列表，因此**不生成 `GameCore` 目标**，也不产出 `GameCore.lib`。
- **宿主机制已取消**：本层过去挂着一个「组合根可执行文件」（把引擎层、系统层接在一起并从本层启动），另有一个「把测试层宿主并入本构建树」的开关。两者都已移除，原因有二：其一，组合根与宿主职责重叠；其二，组合根会把下层源码拉进本层构建树重复编译，与「层间一律链接已构建静态库」的契约冲突。现在**可执行文件只由测试层产出**（`EngineTests.exe`），各层不再各自维护一个可运行入口。
- **保留构建骨架的原因**：本层保留 `CMakeLists.txt` 与 `cmake/` 两份脚本，是为了与其余三层保持**完全一致的层间契约接入方式**。将来在 `src/` 下放入第一份 `.cpp/.c` 时，`GLOB_RECURSE ... CONFIGURE_DEPENDS` 会自动纳入，无需改动 CMake 即可产出 `GameCore` 静态库。
- 与引擎层、系统层「无源码即报错」不同，本层把空源码视为**预期状态**，只打印提示信息、不报错。

## 四、对外接口契约

### 4.1 `cmake/对外接口.cmake`

声明「本层向上层提供什么」，供未来上层读取并建立 IMPORTED 目标（不使用 `add_subdirectory` 回退编源码）。契约变量：

| 变量 | 本层取值 |
|---|---|
| `BYJY_GAME_OUT_LIB` | `GameCore`（当前尚无源码，声明先于产物） |
| `BYJY_GAME_OUT_INC` | 本层根目录 + `BYJY_SYSTEM_OUT_INC` |
| `BYJY_GAME_OUT_DEF` | 继承 `BYJY_SYSTEM_OUT_DEF` |
| `BYJY_GAME_OUT_LINK` | 继承 `BYJY_SYSTEM_OUT_LINK` |
| `BYJY_GAME_OUT_SYS` | 继承 `BYJY_SYSTEM_OUT_SYS` |

首行 `include` 系统层的对外接口文件，把系统层对外面并入本层，因此本层对外面 = **本层自有面 + 系统层对外面**（系统层又已并入引擎层对外面）。并入是纯数据赋值，重复 include 无副作用。

### 4.2 `cmake/接入下层.cmake`

提供通用接入函数 `byjy_jieru_xiaceng(<下层对外接口文件> <变量前缀> <覆盖库路径变量>)`，系统层、测试层、本层三份逐字节相同。它：

- 读入下层对外面并校验五个 `_OUT_*` 变量是否齐备；
- 建立 `IMPORTED STATIC`（GLOBAL）静态库目标，把包含目录、编译定义、链接库挂到其 `INTERFACE` 上，供本层向上继续传递；
- **三级库定位**：① 覆盖变量非空且文件存在 → 用它（非空但文件不存在则直接 `FATAL_ERROR`，不静默降级）；② 留空 → 自动探测 `<下层>/out/build/*/lib/<库名>.lib`，多候选取时间戳最新的一份；③ 仍无 → `FATAL_ERROR` 并给出构建下层的命令；
- **任何情况下都不回退编译下层源码**。

本层在 `CMakeLists.txt` 中的调用：以 `BYJY_SYSTEM` 为变量前缀、`BYJY_SYSTEM_LIB_PATH` 为覆盖变量，接入 `../系统层/cmake/对外接口.cmake`。

## 五、构建

前置：先构建系统层（系统层又依赖已构建的引擎层）。

```
cmake -S . -B out/build/x64-Debug -G Ninja
cmake --build out/build/x64-Debug
```

- 要求 CMake ≥ 3.20，生成器 Ninja，C++20；已在 MSVC（x64）上验证。
- 由于本层当前无源码，**配置可以通过，但不生成任何静态库**，`cmake --build` 无编译任务即成功退出。
- 若覆盖变量 `BYJY_SYSTEM_LIB_PATH` 留空，则自动探测 `../系统层/out/build/*/lib/SystemCore.lib`；探测不到会硬失败并提示先构建系统层。

## 六、后续方向

本层将承载具体的**游戏内容与游戏资源**：游戏玩法代码落到 `src/`（自动产出 `GameCore`），`assets/` 下的实体/属性/格式/路由 JSON 与 Lua 脚本已就位，作为数据驱动内容的终点消费层。本层目前不产出可执行文件；若将来游戏本体需要独立入口，再在本层单独追加一个可执行目标。