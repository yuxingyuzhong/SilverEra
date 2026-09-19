# 白银纪元

一个用 C++20 从零编写的游戏引擎工程，按依赖方向分层组织。

## 层次结构

```
白银纪元
├── 引擎层/    最底层，与具体游戏逻辑无关
├── 系统层/    实体、属性、效应等运行时系统
├── 游戏层/    具体游戏内容与资源
└── 测试层/    试验与验证代码
```

依赖方向单向：`引擎层 ← 系统层 ← 游戏层`。上层可以引用下层，下层不感知上层。

## 各层产物

| 层 | 构建产物 |
| --- | --- |
| 引擎层 | `EngineCore.lib`、`ConfigEditor.exe`（配置编辑器） |
| 系统层 | `SystemCore.lib` |
| 游戏层 | `TestEngine.exe` |
| 测试层 | 试验代码，尚未接入顶层构建 |

## 构建

要求 CMake 3.20 以上，以及支持 C++20 的编译器（当前在 MSVC 上验证）。

### 全量构建

```
cmake -S . -B out/build/x64-Debug -G Ninja
cmake --build out/build/x64-Debug
```

### 单层构建

每一层都保留独立的 `CMakeLists.txt`，可以脱离顶层单独配置编译：

```
cmake -S 引擎层 -B out/build/engine-layer -G Ninja
cmake --build out/build/engine-layer
```

### 在 Visual Studio 中打开

顶层已附带 `CMakeSettings.json`（Ninja / x64-Debug），用 VS 的「打开本地文件夹」选中本目录即可。

## 仓库结构

本工程采用「多分支存档」：三个层各自是独立的 Git 仓库，分别向同一个远程仓库推送自己的分支。

| 远程分支 | 推送源 | 内容 |
| --- | --- | --- |
| `main` | 本目录（顶层仓库） | 顶层 CMakeLists、CMakeSettings、README，以及测试层 |
| `engine` | `引擎层/` | 引擎层全部源码 |
| `system` | `系统层/` | 系统层全部源码 |
| `game` | `游戏层/` | 游戏层全部源码 |

之所以这样安排，是因为同一个目录里嵌套 Git 仓库时，上层仓库无法收录下层仓库的文件——Git 会把整个子目录记成一个 gitlink，推到远程后那些目录是空的。让每一层独立存档、各推各的分支，四个推送源互不干扰。

想看某一层的完整源码，clone 之后切到对应分支即可。

## 许可

见 [LICENSE](LICENSE)。
