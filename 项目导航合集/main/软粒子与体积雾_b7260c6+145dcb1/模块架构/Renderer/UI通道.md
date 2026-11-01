# UI 通道 · 模块架构

> 收录范围：`Application/Renderer/src/render/Renderer/` 下的两个文件——对外**扩展头** `渲染器_UI通道.h`（结构 `Ui_Channel_Info` / `Ui_Event_Callback`、类 `Ui_Channel_Context` 声明）与 `core/UI通道.cpp`（`Ui_Channel_Context` 全部成员实现）。本模块无自有 `局部命名空间使用.h`、无独立 `core/` 辅助头。
> 本文为「白银纪元」渲染层（`Application/Renderer`）中「UI 通道」子模块的导航文档。本模块物理上归属「渲染层-渲染器」门面，是该门面面向上层 UI 后端的**公共扩展面**；帧链路侧衔接见「渲染层-帧」§6.14，平台侧事件外送见「渲染层-平台与表面」§3.8／§6.16。
> 记录依据为当前工作区源码，不含旧版次导航信息。文中路径均为相对源码根（`工作区`）的相对路径。UI 通道为本轮（Renderer `0f985ec..b2ffe50`）**新增**模块，源码无历史设计编号引用。

---

## 一、文件内目录

- [二、模块功能介绍](#二模块功能介绍)：2.1 定位 ／ 2.2 职责边界 ／ 2.3 架构位置 ／ 2.4 相邻模块分界 ／ 2.5 生命周期与调用约定
- [三、模块公开接口介绍](#三模块公开接口介绍)：3.1 类总览 ／ 3.2 `Ui_Channel_Info` ／ 3.3 `Ui_Event_Callback` ／ 3.4 逐条成员说明 ／ 3.5 门面取用接口
- [四、模块私有方法介绍](#四模块私有方法介绍)：4.1「无私有函数」的说明与实现分布
- [五、模块内部架构介绍](#五模块内部架构介绍)：5.1 目录树 ／ 5.2 编译单元职责 ／ 5.3 数据成员 ／ 5.4 构成关系 ／ 5.5 与私有别名头的关系
- [六、模块设计/实现细节](#六模块设计实现细节)：6.1 公共扩展面与包含根 ／ 6.2 与 `ImGui_ImplVulkan_InitInfo` 对齐 ／ 6.3 自引用指针 ／ 6.4 生命周期与所有权 ／ 6.5 帧内绘制时机 ／ 6.6 事件转发链路 ／ 6.7 线程约定 ／ 6.8 易错点
- [七、模块运行链路详例](#七模块运行链路详例)：7.1 初始化 ／ 7.2 事件转发 ／ 7.3 帧内绘制 ／ 7.4 关闭 ／ 7.5 边界分支表
- [附录、待确认条目](#附录待确认条目)：附录 A 依赖与引用 ／ 附录 B 待确认条目索引

---

## 二、模块功能介绍

### 2.1 模块定位

「UI 通道」（`Ui_Channel_Context`）位于渲染层门面目录 `Application/Renderer/src/render/Renderer/`，是渲染层对上层（`Application/Test` 层）提供的**唯一 UI 接入口**，把上层接入一个 Vulkan UI 后端（本项目为 Dear ImGui）所需的三样东西交出：

1. **通道信息**（`Ui_Channel_Info`）：渲染层已建的 `VkInstance` / `VkPhysicalDevice` / `VkDevice` / 图形队列与族索引 / 自建 `SDL_Window*` / 交换链图像数与格式 / 动态渲染口径；
2. **每帧绘制时机**（`Frame_Ui_Callback`）：帧链路在「场景动态渲染通道内、`vkCmdEndRendering` 之前」把本帧命令缓冲交还上层；
3. **输入事件出送口**：平台事件泵把每个原始 `SDL_Event` 逐个转发给上层平台后端。

一句话：**本模块是渲染层「把 Vulkan/窗口/时机/事件交给上层 UI 后端」的极薄转接层——只提供通道，不代管 UI；不 include imgui、不持 ImGui 上下文、不建渲染通道。**

### 2.2 职责边界与解决的问题

1. **让上层复用渲染层已建的窗口与 Vulkan 设备**，避免上层自建窗口/设备与渲染层争用同一 SDL 视频子系统与 GPU 资源；上层直接据此初始化第三方后端（`ImGui_ImplVulkan_Init` / `ImGui_ImplSDL3_InitForVulkan`）。
2. **以「字段对齐但不 include imgui」的结构传递后端所需信息**：`Ui_Channel_Info` 字段与 `ImGui_ImplVulkan_InitInfo` 一一对齐，但渲染层不包含 imgui 头——第三方 UI 库改版不牵连渲染层源码，也不把 imgui 类型拖进层内编译单元。
3. **固化「UI 与场景共用同一渲染通道」的绘制时机**：帧链路走动态渲染，本模块提供 `Frame_Ui_Callback`，由帧链路在场景绘制项之后、`vkCmdEndRendering` 之前调用；上层在此调用 `ImGui_ImplVulkan_RenderDrawData`，**直接画在本帧已开启的动态渲染通道内，不自开 RenderPass/通道**。代价是上层 ImGui 管线须与本通道附件格式相容，故本模块把 `color_format`/`depth_format` 与装填好的 `pipeline_rendering` 一并交出（§6.2／§6.3）。
4. **桥接输入事件且不破坏「事件队列单点轮询」**：SDL 事件队列由平台单点 `SDL_PollEvent` 取出后经事件转发出口（`event_forward_set`）交本模块再转上层（`imgui_impl_sdl3`），避免多处轮询。

### 2.3 在整体架构中的位置

UI 通道处于门面与上层之间，横向被平台、帧两个子模块「接出」：门面在 `init` ⑰ 末尾 `build` 配置它，`frame_render` 每帧取 `draw_callback_get()` 交帧的 `command_record`（场景 pass 内调用），平台 `events_pump` 每帧经 `event_forward_set` 桥接送入 `event_dispatch`；上层经 `Renderer::ui_channel()` 取用。

- **向上**：调用方为上层 `Application/Test`（`src/主调/图形操作界面.cpp`）。
- **向下（层内）**：不依赖任何渲染子模块——信息由门面从设备/交换链/平台取出后经 `build` 灌入；绘制回调由帧链路调用；事件由平台转发。
- **横向依赖**：本头只包含预编译头、`<vulkan/vulkan.h>`、`<SDL3/SDL.h>` 与 `帧上下文.h`，与设备/交换链/平台/帧**无直接包含依赖**，跨模块数据由门面撮合。

### 2.4 与相邻模块的职责分界

| 相邻对象 | 分界 |
| --- | --- |
| 门面（`渲染器.h`／`core/渲染器.cpp`／`core/渲染器实现.h`） | 以值成员 `ui_channel` 持有本模块；`init` 末尾调 `build` 并挂平台事件桥接；`frame_render` 取 `draw_callback_get()` 交帧；`shutdown` 解桥接后 `destroy`；对外提供唯一入口 `Renderer::ui_channel()`（未初始化返回空）。 |
| 帧模块（`render/Frame/`） | 只定义 `Frame_Ui_Callback` 类型（`帧上下文.h`）并在 `command_record` 的场景 pass 内调用，**不持有、不代管 UI**；回调由本模块提供（见「渲染层-帧」§6.14）。 |
| 平台模块（`render/Platform/`） | 只做「事件单点轮询 + 事件外送」，不做输入解释；其外送回调由门面桥接到本模块 `event_dispatch`。 |
| 上层 `Application/Test` | 拥有 ImGui 上下文与两条后端；负责填装 `ImGui_ImplVulkan_InitInfo`、注册绘制与事件回调、结束前解接线。 |

### 2.5 生命周期与调用约定

```
构造（不创建 Vulkan/SDL 对象） → build(...)（门面 init ⑰） → [使用期] → destroy()（幂等） → 析构（兜底再调 destroy）
```

- 状态机两态：**未配置**与**已配置**；`valid()` 判据 = `built && info.device != VK_NULL_HANDLE`。
- `destroy()` 幂等：仅复位 `info` / 两个回调 / `built`，**不拥有也不释放**任何 Vulkan / SDL 资源（资源属设备/平台/交换链），可重复调用；析构兜底再调一次。
- 类**不可拷贝、不可移动**（`= delete`）。
- **非线程安全**：无锁无原子，要求所有调用来自同一线程（渲染主线程）。上层在 `Renderer::init` 成功之后、`shutdown` 之前取用。

---

## 三、模块公开接口介绍

可见性两层：**对外层**（上层可见）为 `engine::Ui_Channel_Info`、`engine::Ui_Event_Callback`、`engine::Ui_Channel_Context`，声明于**公共扩展头** `渲染器_UI通道.h`（含 Vulkan/SDL 类型，**不进**公共主头 `渲染器.h`，后者仅前置声明该类）；**模块内部层**无额外头与自由函数。

### 3.1 `Ui_Channel_Context` 类总览

**声明**：`Application/Renderer/src/render/Renderer/渲染器_UI通道.h`；**实现**：`core/UI通道.cpp`。

| 类别 | 成员 | 签名 | 可见性 |
| --- | --- | --- | --- |
| 构造 | `Ui_Channel_Context` | `Ui_Channel_Context(void) = default` | public |
| 析构 | `~Ui_Channel_Context` | `~Ui_Channel_Context(void)` | public |
| 删除 | 拷贝/赋值 | `Ui_Channel_Context(const Ui_Channel_Context&) = delete` / `operator=` 删除 | public |
| 配置 | `build` | `void build(VkInstance, VkPhysicalDevice, VkDevice, uint32_t graphics_family, VkQueue graphics_queue, SDL_Window* window, uint32_t min_image_count, uint32_t image_count, VkFormat color_format, VkFormat depth_format)` | public |
| 销毁 | `destroy` | `void destroy(void)` | public |
| 查询 | `valid` | `bool valid(void) const` | public |
| 取值 | `info_get` | `const Ui_Channel_Info& info_get(void) const` | public |
| 绘制回调 | `draw_callback_set` | `void draw_callback_set(void (*callback)(VkCommandBuffer, void*), void* user_data)` | public |
| 绘制回调 | `draw_callback_clear` | `void draw_callback_clear(void)` | public |
| 绘制回调 | `draw_callback_active` | `bool draw_callback_active(void) const` | public |
| 绘制回调 | `draw_callback_get` | `const Frame_Ui_Callback* draw_callback_get(void) const` | public |
| 事件回调 | `event_callback_set` | `void event_callback_set(void (*callback)(const SDL_Event*, void*), void* user_data)` | public |
| 事件回调 | `event_callback_clear` | `void event_callback_clear(void)` | public |
| 事件回调 | `event_callback_active` | `bool event_callback_active(void) const` | public |
| 事件回调 | `event_dispatch` | `void event_dispatch(const SDL_Event* event)` | public |
| 数据 | `info` / `callback` / `event_callback` / `built` | 见 §5.3 | private |

> 本模块**无 private 成员函数**（§四）；`private` 数据成员说明见 §5.3。

### 3.2 结构 `Ui_Channel_Info`（通道信息）

**语义**：与 Dear ImGui 的 `ImGui_ImplVulkan_InitInfo` 字段对齐（但本头不 include imgui）。由 `build` 填充后经 `info_get` 以 const 引用交出；生命周期 = 渲染器 `init` 之后至 `shutdown` 之前。

| 字段 | 类型 | 默认值 | 含义 | 对应 `ImGui_ImplVulkan_InitInfo` 字段 |
| --- | --- | --- | --- | --- |
| `api_version` | `uint32_t` | `VK_API_VERSION_1_3` | 应用 API 版本 | `ApiVersion` |
| `instance` | `VkInstance` | `VK_NULL_HANDLE` | 实例 | `Instance` |
| `physical_device` | `VkPhysicalDevice` | `VK_NULL_HANDLE` | 物理设备 | `PhysicalDevice` |
| `device` | `VkDevice` | `VK_NULL_HANDLE` | 逻辑设备 | `Device` |
| `graphics_family` | `uint32_t` | `0` | 图形队列族索引 | `QueueFamily` |
| `graphics_queue` | `VkQueue` | `VK_NULL_HANDLE` | 图形队列 | `Queue` |
| `window` | `SDL_Window*` | `nullptr` | 渲染层自建窗口（供平台后端绑定输入） | ——（供 `imgui_impl_sdl3`） |
| `min_image_count` | `uint32_t` | `2` | 最小图像数（= 在途帧数；imgui 要求 ≥ 2） | `MinImageCount` |
| `image_count` | `uint32_t` | `0` | 交换链图像数 | `ImageCount` |
| `color_format` | `VkFormat` | `VK_FORMAT_UNDEFINED` | 颜色附件格式（交换链格式） | 经 `pipeline_rendering` 间接给出 |
| `depth_format` | `VkFormat` | `VK_FORMAT_UNDEFINED` | 深度附件格式（交换链深度图；UI 与场景共用同一通道，ImGui 管线须与本格式相容） | 经 `pipeline_rendering` 间接给出 |
| `use_dynamic_rendering` | `bool` | `true` | 使用动态渲染（后端须同步开启） | `UseDynamicRendering` |
| `pipeline_rendering` | `VkPipelineRenderingCreateInfo` | `{}` | 动态渲染信息（`pColorAttachmentFormats` 指向本结构 `color_format`） | `PipelineInfoMain.PipelineRenderingCreateInfo` |

- `build` 只回填上表中除 `api_version` 外的字段（`api_version` 保持初值）。
- 「对应字段」取自 `ImGui_ImplVulkan_InitInfo`（落点 `Application/Test/external/Dear_ImGui/backends/imgui_impl_vulkan.h`）与上层填装点 `图形操作界面.cpp`（§6.2）。

### 3.3 结构 `Ui_Event_Callback`（事件转发回调）

| 字段 | 类型 | 默认值 | 含义 |
| --- | --- | --- | --- |
| `fn` | `void (*)(const SDL_Event* event, void* user_data)` | `nullptr` | 事件转发回调（空表示未设置） |
| `user_data` | `void*` | `nullptr` | 用户数据 |

本结构为内部回调载体（`Ui_Channel_Context::event_callback` 的类型），不由上层直接构造，上层经 `event_callback_set` 传函数指针与用户数据。

### 3.4 逐条成员说明

- **构造/析构**：`= default` 构造不创建任何 Vulkan/SDL 对象（`info` 取类内初值、回调为空、`built = false`，故 `valid()` 假）；析构仅调 `destroy()`，即便从未 `build` 也安全。
- **`build(...)`**：逐字段写入 `info`；`use_dynamic_rendering` **恒置 `true`**；装填 `info.pipeline_rendering`（`sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO`、`colorAttachmentCount = 1`、`pColorAttachmentFormats = &info.color_format`、`depthAttachmentFormat = depth_format`、`stencilAttachmentFormat = VK_FORMAT_UNDEFINED`）；若 `min_image_count < 2` 记 `logger.warn("渲染器:: UI 通道最小图像数 {} 低于 imgui 要求的 2，建议把在途帧数设为 2 或 3", min_image_count)`（仅告警不阻断）；末置 `built = true`。**注意**：头文件对该函数的说明段只列 `window`/`min_image_count`/`image_count`/`color_format`，**未提 `depth_format`**，以签名与实现为准。
- **`destroy()`**：`info = Ui_Channel_Info{};`、`callback`/`event_callback` 置空结构、`built = false;`；幂等，不释放任何资源。
- **`valid()`**：`built && info.device != VK_NULL_HANDLE`（「已配置且逻辑设备非空」的最小可用判据）。
- **`info_get()`**：返回 `info` 的 **const 引用**（不拷贝）；未 `build` 时即返回默认值结构；调用方须在本对象存活期内使用。
- **绘制回调四成员**：`draw_callback_set(callback_fn, user_data)` → `callback.fn = callback_fn; callback.user_data = user_data;`（形参名 `callback_fn` 以避开成员 `callback`）；`draw_callback_clear()` 置空；`draw_callback_active()` 判 `fn != nullptr`；`draw_callback_get()` 于 `fn` 空时返回 `nullptr`，否则 `&callback`。时机语义：帧链路在「场景渲染 pass 结束后、呈现转换前」调用（精确落点为场景通道内、`vkCmdEndRendering` 之前，§6.5）。
- **事件回调四成员**：`event_callback_set(callback_fn, user_data)` 记录；`event_callback_clear()` 置空；`event_callback_active()` 判 `fn != nullptr`；`event_dispatch(event)` 于 `fn != nullptr` 时调 `fn(event, user_data)`，否则**空操作**（由门面挂到平台事件外送桥接、由事件泵逐事件调用，§6.6）。

### 3.5 门面转出的取用接口

| 来源 | 签名 | 说明 |
| --- | --- | --- |
| `engine::Renderer::ui_channel`（`渲染器.h` 声明／`core/渲染器.cpp` 实现） | `Ui_Channel_Context* ui_channel(void)` | 返回门面持有的 UI 通道指针；`impl == nullptr` 时返回 `nullptr`；上层需自行包含扩展头取得完整类型。 |

**上层实测取用**：`Ui_Channel_Context* 通道 = 渲染器.ui_channel();` → 判 `通道 != nullptr && 通道->valid()` → `通道->info_get()` / `draw_callback_set` / `event_callback_set`。

---

## 四、模块私有方法介绍

### 4.1 「无私有函数」的说明与实现分布

本模块**没有任何类内 private/protected 成员函数，也没有 `core/UI通道.cpp` 内的匿名命名空间辅助函数或 `static` 文件局部函数**。`Ui_Channel_Context` 的全部逻辑都是上述 15 个 public 成员在 `core/UI通道.cpp` 中的直接实现：

| 实现函数（`core/UI通道.cpp`） | 职责 |
| --- | --- |
| `~Ui_Channel_Context` | 调 `destroy()` |
| `build` | 回填 `info` + 装填 `pipeline_rendering` + 最小图像数告警 + 置 `built` |
| `destroy` | 复位 `info` / `callback` / `event_callback` / `built` |
| `valid` | `built && info.device != VK_NULL_HANDLE` |
| `info_get` | 返回 `info` 引用 |
| `draw_callback_set/clear/active/get` | 绘制回调的记录 / 置空 / 判定 / 取出 |
| `event_callback_set/clear/active/dispatch` | 事件回调的记录 / 置空 / 判定 / 转发 |

故本节无逐条私有函数可列；数据成员说明见 §5.3。这是本模块「源码单薄」（仅一头一 `.cpp`，无内部分解）的直接体现。

---

## 五、模块内部架构介绍

### 5.1 目录/文件结构树

```
Application/Renderer/src/render/Renderer/
├── 渲染器.h（公共主头，Vulkan-free：前置声明 class Ui_Channel_Context；声明 Renderer::ui_channel）
├── 渲染器_UI通道.h（公共扩展头，含 Vulkan/SDL3：Ui_Channel_Info / Ui_Event_Callback / Ui_Channel_Context）
├── 局部命名空间使用.h（门面模块私有头，本模块 .cpp 借用）
└── core/
    ├── 渲染器.cpp（init ⑰ 调 build + 挂事件桥接；frame_render 取绘制回调；shutdown 解桥接后 destroy）
    ├── 渲染器实现.h（实现体 Renderer::Impl：值成员 Ui_Channel_Context ui_channel）
    ├── 结果检查.h（本层唯一 VkResult 检查出口；本模块经它间接引入 logger）
    └── UI通道.cpp（本模块唯一实现：Ui_Channel_Context 全部 public 成员）
```

本模块**没有**自有 `局部命名空间使用.h`，其 `.cpp` 直接包含门面模块的 `src/render/Renderer/局部命名空间使用.h`，复用其别名环境与预编译头（符合渲染层「实现文件跨目录包含写本层根相对路径」口径，§6.1）。

### 5.2 各编译单元职责

| 编译单元 | 职责 | 关键内容 |
| --- | --- | --- |
| `渲染器_UI通道.h` | 对外契约（公共扩展头） | 预编译头、`<vulkan/vulkan.h>`、`<SDL3/SDL.h>`、`帧上下文.h`；三个对外类型 |
| `core/UI通道.cpp` | 唯一实现单元 | `#include "src/render/Renderer/局部命名空间使用.h"`、`".../core/结果检查.h"`（仅为引入 `logger`）、`"src/render/Renderer/渲染器_UI通道.h"`；实现全部成员 |

**包含要点**：`UI通道.cpp` 三个包含依次为——门面私有头（本层根相对，间接含 `渲染器.h` → 预编译头）、`core/结果检查.h`（注释 `//获取结果检查出口（含日志系统）`，借其引入全局 `logger` 供 `build` 告警）、本模块扩展头（注释 `//UI 通道声明`）。本模块**只**用 `logger`，**未**调用 `vk_result_check`（不发起 Vulkan 调用）。

### 5.3 `Ui_Channel_Context` 数据成员逐字段说明

| 字段 | 类型 | 初值 | 含义 | 生命周期/约束 |
| --- | --- | --- | --- | --- |
| `info` | `Ui_Channel_Info` | 类内各字段初值 | 通道信息 | `build` 回填；`destroy` 复位为 `Ui_Channel_Info{}`；经 `info_get` 以 const 引用交出 |
| `callback` | `Frame_Ui_Callback` | `{ nullptr, nullptr }` | 每帧 UI 绘制回调 | `draw_callback_set` 写；`draw_callback_clear`/`destroy` 置空；`draw_callback_get` 返回其地址（`fn` 空则返回空指针） |
| `event_callback` | `Ui_Event_Callback` | `{ nullptr, nullptr }` | SDL 事件转发回调 | `event_callback_set` 写；`event_callback_clear`/`destroy` 置空；`event_dispatch` 消费 |
| `built` | `bool` | `false` | 是否已配置 | `build` 置真；`destroy` 置假；`valid()` 判据之一 |

**不变量**：`info.pipeline_rendering.pColorAttachmentFormats` 恒指向本对象 `info.color_format`（§6.3）；`built == true` 不保证 `info.device != VK_NULL_HANDLE`（`valid()` 二者兼校）。

### 5.4 内部构成关系

```
Ui_Channel_Context（无基类、无成员子对象）
 ├─ has-a info           : Ui_Channel_Info（对齐 imgui 后端字段集）
 ├─ has-a callback       : Frame_Ui_Callback（帧链路调用）
 └─ has-a event_callback : Ui_Event_Callback（事件泵桥接调用）
         └─ pipeline_rendering.pColorAttachmentFormats ──▶ info.color_format（自引用）
```

### 5.5 与 `局部命名空间使用.h` 的关系

本模块自身无别名头；`core/UI通道.cpp` 复用门面模块的 `局部命名空间使用.h`（全文 `#include "渲染器.h"` + `using std::string; using std::vector;`），主要作用是按渲染层统一口径引入预编译头与门面声明环境；因本模块只做字段赋值与指针转发，**实际未用到 `string`/`vector` 别名**。实现统一位于 `namespace engine { ... }`。

---

## 六、模块设计/实现细节

### 6.1 公共扩展面与包含根

`渲染器_UI通道.h` 是渲染层**公共扩展头**：属对外面，但**含 Vulkan/SDL3 类型**，故**不并入**公共主头 `渲染器.h`（主头只前置声明 `class Ui_Channel_Context;` 并声明 `Renderer::ui_channel()`，保持 Vulkan-free 收口，见「渲染层-门面」§4.1）。层外（Test 层）要包含本头并编译 ImGui 两条后端，须先拿到 Vulkan SDK 与 SDL3 公共头——`cmake/对外接口.cmake`（`BYJY_RENDERER_OUT_INC`）为此把 `${BYJY_VULKAN_INCLUDE_DIR}`、`external/SDL3/include`（+`build_config`）、`external/VulkanMemoryAllocator/include`（VMA：本头经 `帧上下文.h` → `交换链.h` 传递到 `vk_mem_alloc.h`）提升为**对外面（PUBLIC）**；这是「把 Vulkan SDK/SDL3 包含根提升为 PUBLIC」预留路线的落地，私有包含根（`SDL3/src`、VMA、`external/` 顶层等）仍只对本层可见。

### 6.2 与 `ImGui_ImplVulkan_InitInfo` 的字段对齐，及「不 include imgui」的用意

**对齐**：`Ui_Channel_Info` 字段与 `ImGui_ImplVulkan_InitInfo` 中「由应用提供的 Vulkan 面」一一对齐（表见 §3.2）。上层填装实测（`图形操作界面.cpp`）：`ApiVersion/Instance/PhysicalDevice/Device/QueueFamily/Queue` ← 通道信息 `api_version/instance/physical_device/device/graphics_family/graphics_queue`；`MinImageCount/ImageCount/UseDynamicRendering` ← `min_image_count/image_count/use_dynamic_rendering`；`PipelineInfoMain.PipelineRenderingCreateInfo` ← `pipeline_rendering`；而 `DescriptorPoolSize` 等后端专属量由上层决定（后端自建描述符池），`window` 字段专供 `ImGui_ImplSDL3_InitForVulkan(通道信息.window)` 平台后端（不属 `ImGui_ImplVulkan_InitInfo`）。

**不 include imgui 的用意**：渲染层只暴露「与 imgui 结构同构」的**自有结构**。收益：① 渲染层不依赖第三方 UI 库版本——近版 imgui 把 `ColorFormat`/`DepthFormat`/`RenderPass` 等收进 `PipelineInfoMain`，渲染层侧只装填 `pipeline_rendering` 一个成员即自适应，改版不牵连渲染层源码；② 层内编译单元不因包含本头而吃进 imgui 类型；③ 对齐关系以注释 + 上层填装点固化，改版时只需上层同步。

### 6.3 动态渲染信息装填与自引用指针

`build` 装填 `info.pipeline_rendering`：`sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO`、`colorAttachmentCount = 1`、`pColorAttachmentFormats = &info.color_format`（**指向本结构自身的 `color_format` 成员**）、`depthAttachmentFormat = depth_format`、`stencilAttachmentFormat = VK_FORMAT_UNDEFINED`（模板不参与）。

**自引用指针的含义与约束**：`pColorAttachmentFormats` 不是独立存储，而指向同一 `Ui_Channel_Info` 的 `color_format`。故：① 装填后的 `Ui_Channel_Info` **不可按值拷贝后再使用**（拷贝会使指针留指原对象）；本模块据此只经 `info_get()` 交出 **const 引用**。② 上层填装 ImGui 时把 `pipeline_rendering` 整个结构**按值赋给** `后端信息.PipelineInfoMain.PipelineRenderingCreateInfo`，拷贝后 `pColorAttachmentFormats` 仍指向渲染层 `info.color_format`——只要渲染层 UI 通道（及宿主 `Renderer`）在该后端使用期内存活即有效；结合生命周期编排（渲染器 `shutdown` 在 UI 后端 `Shutdown` 之后），该指针始终有效。③ `depthAttachmentFormat` 直传 `depth_format`，UI 管线据此与场景通道深度附件相容，这是 §6.5「UI 与场景共用同一通道」的配套条件。

### 6.4 生命周期、幂等与所有权

- **三段式**：`build`（门面 `init` ⑰）→ 使用 → `destroy`（门面 `shutdown` 末尾）；`destroy` 幂等、析构兜底。
- **幂等与外部资源无关**：`destroy` 只复位本对象数据，不触碰任何 Vulkan/SDL 对象，可重复调用、可在半初始化状态安全调用。
- **所有权**：本模块**不拥有**任何被引用句柄——`info` 里 `instance`/`device`/`queue`/`window` 属设备/平台（由门面借入仅作记录），图像数与格式属交换链；故**不承担释放责任**，真正释放由各持有模块在 `shutdown` 内完成。
- **销毁顺序协同**：门面 `shutdown` 先 `platform.event_forward_clear()`（解除平台→本模块桥接，避免平台销毁后仍有回调指向本模块），随后 `platform.destroy()`/`device.destroy()`，最后才 `ui_channel.destroy()`。
- **交换链重建的刷新缺口**：门面 `frame_render` 的两条重建路径（事件路径与呈现路径）只调 `swapchain.rebuild` + `pipeline.pipeline_rebuild`，**未**重新 `build` 本模块、也**未**调用 ImGui 的 `ImGui_ImplVulkan_SetMinImageCount` 通知后端刷新图像数；若某次重建改变交换链图像数，`info.image_count` 与上层 ImGui `ImageCount` 会失配【待确认：R-UICH-01】。

### 6.5 帧内 UI 绘制时机（与帧模块的衔接）

`Frame_Ui_Callback` 类型定义于帧模块 `Frame/帧上下文.h`，注释规定其调用时机：**每帧在「场景渲染 pass 内、`vkCmdEndRendering` 之前」由帧链路调用**，把本帧命令缓冲交上层。上层在其上调用 `ImGui_ImplVulkan_RenderDrawData`，该方法**直接画在本帧已开启的动态渲染通道内**（与场景共用同一通道），**不自开渲染通道**。**设计结果**：UI 与场景共用同一颜色/深度附件，故 ImGui 管线须与 `color_format`/`depth_format` 相容（本模块经 `pipeline_rendering` 下发此约束）；渲染层**无需**为 UI 另建 RenderPass 或通道。门面与帧模块都只按类型调用该回调，不持有、不代管 UI。门面 → 帧 → `command_record` 的具体调用链见 §7.3（衔接另见「渲染层-帧」§6.14）。

### 6.6 事件转发链路（平台 → 桥接 → 本模块 → 上层）

渲染层不解释输入，只做事件原文转发（逐事件时序见 §7.2）：**平台出口** `Platform_Context::events_pump`（`Platform/core/事件泵.cpp`）先在 `SDL_PollEvent` 循环内调事件转发回调、再执行自身 `switch` 置平台标志（退出/尺寸/最小化；事件队列仍单点轮询）；**门面桥接** `init` 在 `build` 后调 `platform.event_forward_set(<转投 event_dispatch 的 lambda>, &ui_channel)`——桥接 lambda **不持资源**；**本模块转发** `event_dispatch` 于已设 `fn` 时调之（即 `ImGui_ImplSDL3_ProcessEvent`），否则空操作；**解除** `shutdown` 中先 `platform.event_forward_clear()`。**注册时序非对称**：桥接由门面在 `init` 内**无条件挂上**，上层回调由上层在 `init` **之后自行注册**；故在「桥接已挂、回调未注册」的窗口期内事件会送进 `event_dispatch`，此时 `fn == nullptr`，转发为空操作——这正是 `event_dispatch` 判空的意义。

### 6.7 线程/并发约定

本模块无互斥量、无原子量，成员均为普通标量与句柄。约定与门面一致：**所有调用来自同一线程（渲染主线程）且串行**——`build`/`destroy` 在门面 `init`/`shutdown` 内；`info_get`/`*_set`/`*_get` 由上层与门面在主线程调用；绘制回调在命令录制阶段（主线程）被调用；事件转发在 `events_pump`（主线程）内被调用。故无需加锁；源码未对跨线程读取作承诺。

### 6.8 易错点

| 易错点 | 说明与规避 |
| --- | --- |
| `Ui_Channel_Info` 不可按值转存再用 | `pipeline_rendering.pColorAttachmentFormats` 自引用本结构 `color_format`，按值拷贝会悬指原对象；只经 `info_get()` const 引用使用（§6.3） |
| 扩展头不进公共主头 | 本头含 Vulkan/SDL 类型，**不得**并入 `渲染器.h`（后者保持 Vulkan-free）；层外 include 前须先由 PUBLIC 包含根拿到 Vulkan SDK/SDL3/VMA 头（§6.1） |
| `valid()` 双重判据 | 为 `built && info.device != VK_NULL_HANDLE`，单看 `built` 不够 |
| `build` 的 `min_image_count` 实为在途帧数 | 门面传入 `frame_overlap`；低于 2 只告警不阻断，上层若照此填 ImGui `MinImageCount` 可能触发后端断言 |
| 回调须在动态渲染通道内调用 | 帧链路在 `vkCmdBeginRendering` 后、`vkCmdEndRendering` 前调用；上层回调内**不得**再 `vkCmdBeginRendering`（会与场景通道嵌套） |
| `destroy` 不释放资源 | 仅复位数据；句柄属设备/平台/交换链 |

---

## 七、模块运行链路详例

调用方为上层（Test 层）与渲染器门面。

### 7.1 初始化链路

1. **门面 `init` 阶段 ⑰**：`impl->ui_channel.build(device.instance_get(), device.physical_device_get(), device.device_get(), device.graphics_family_get(), device.graphics_queue_get(), platform.window_get(), effective.frame_overlap, swapchain.image_count_get(), swapchain.format_get(), swapchain.depth_format_get())`；本模块回填 `info`、装填 `pipeline_rendering`、若 `frame_overlap < 2` 告警、置 `built = true`。
2. **门面挂事件桥接**：`platform.event_forward_set(<转投 event_dispatch 的 lambda>, &impl->ui_channel)`；随后 `initialized = true;`。
3. **上层首次取用**：`通道 = 渲染器.ui_channel();` 判 `通道 != nullptr && 通道->valid()`，再 `const Ui_Channel_Info& 通道信息 = 通道->info_get();`。
4. **上层初始化 ImGui**：`ImGui::CreateContext()` → `ImGui_ImplSDL3_InitForVulkan(通道信息.window)` → 填 `ImGui_ImplVulkan_InitInfo`（映射 §6.2）→ `ImGui_ImplVulkan_Init(&后端信息)` → 加载中文字体。
5. **上层接线**：`通道->draw_callback_set(ui_绘制回调, &上下文);`、`通道->event_callback_set(ui_事件回调, nullptr);`。
   **边界**：`通道 == nullptr` 或 `!通道->valid()` 时上层判失败并 `渲染器.shutdown()`（Test 层实测即如此）。

### 7.2 每帧事件转发链路

`渲染器.frame_render()` ① 调 `platform.events_pump()`；其 `SDL_PollEvent` 循环内对每个事件**先**调 `event_callback(&event, event_callback_data)`（已由门面设为桥接 lambda）→ 桥接 lambda 调 `ui_channel.event_dispatch(event)`：若上层已注册则调 `event_callback.fn(event, user_data)`（即 `ImGui_ImplSDL3_ProcessEvent(事件)`），否则空操作；随后事件送回 `events_pump` 的 `switch` 置平台标志（与 UI 无关）。**结果**：键鼠/文本输入由上层后端消费，渲染层只转发不解释，事件队列单点轮询得以保持。

### 7.3 帧内绘制链路

1. 门面 `frame_render` 在完成泵事件、重建决策、UBO 写入与描述符集填装后调 `frame.render(..., ui_channel.draw_callback_get())`（未设置则为空指针）。
2. `提交呈现.cpp::render` 取图像、等栅栏、重置命令池，调 `command_record(..., ui_callback)`。
3. `命令录制.cpp::command_record`：布局转换 → `vkCmdBeginRendering`（颜色附件 = 交换链图像、深度附件 = 交换链深度图）→ 设视口/裁剪 → 逐渲染项 `vkCmdDrawIndexed`（场景）→ **UI 阶段**：`if (ui_callback != nullptr && ui_callback->fn != nullptr) ui_callback->fn(command_buffer, ui_callback->user_data);`。
4. 上层回调（Test 层 `ui_绘制回调`）：`ImGui_ImplVulkan_NewFrame()` → `ImGui_ImplSDL3_NewFrame()` → `ImGui::NewFrame()` → 构建界面 → `ImGui::Render()` → `ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), 命令缓冲)`（画在**当前已开启**的动态渲染通道内）。
5. 回调返回后 `command_record` 继续：`vkCmdEndRendering` → 颜色布局转 `PRESENT_SRC_KHR` → `vkEndCommandBuffer`；随后提交与呈现。
   **结果**：UI 与场景共用同一动态渲染通道、UI 绘制在场景之后。**说明**：本链路不含「交换链重建后刷新 UI 通道信息/通知后端」的步骤（§6.4）【待确认：R-UICH-01】。

### 7.4 关闭链路

1. **上层先解接线**（Test 层实测）：`通道->event_callback_clear(); 通道->draw_callback_clear();` → `ImGui_ImplVulkan_Shutdown(); ImGui_ImplSDL3_Shutdown(); ImGui::DestroyContext();`
2. **上层调 `渲染器.shutdown()`**：门面先 `platform.event_forward_clear()`（断开平台→本模块桥接），再逆序销毁各子模块（`frame → mesh → texture → 场景 UBO → pipeline → 着色器 → shader → swapchain → memory → platform → device`），末尾 `ui_channel.destroy()`（复位数据，不释放外部资源）并复位 `frame_count`/`initialized`。
3. **析构兜底**：`~Renderer` 再调一次 `shutdown`，此时 `ui_channel.destroy()` 为无操作（幂等）。
   **注意**：UI 后端（ImGui）的 `Shutdown` 必须早于 `渲染器.shutdown()`，否则后端可能在渲染层资源（设备/交换链）销毁后仍持有其句柄——Test 层清理顺序即满足此约束。

### 7.5 关键边界分支汇总表

| 场景 | 触发点 | 处理 | 结果 |
| --- | --- | --- | --- |
| 未初始化即取通道 | `Renderer::ui_channel()`（`impl == nullptr`） | 返回 `nullptr` | 上层判失败 |
| 通道未 `valid` | 上层判 `!通道->valid()` | 停止初始化并 `shutdown` | 不进入 UI 流程 |
| 未设绘制回调 | `draw_callback_get()` / 帧链路 | 帧链路读到空指针 | 本帧只画场景，不画 UI |
| 未设事件回调 | `event_dispatch` 判 `fn == nullptr` | 空操作 | 事件不转上层（不报错） |
| `min_image_count < 2` | `build` | `logger.warn` | 仍置 `built`；上层应避免据此填 ImGui `MinImageCount` |
| 回调内再次开通道 | 上层误调 `vkCmdBeginRendering` | 与场景通道嵌套（校验层报错） | 规避：回调内只提交绘制数据 |
| 交换链重建改变图像数 | 门面重建路径 | 未刷新 UI 通道/未通知后端 | `image_count` 可能过期【待确认：R-UICH-01】 |
| `destroy` 重复调用 / 析构 | `destroy` 数据复位 | 幂等 | 无副作用 |

---

## 附录、待确认条目

### 附录 A. 依赖与引用一览

| 依赖项 | 路径 | 用途 |
| --- | --- | --- |
| Vulkan 头 | `<vulkan/vulkan.h>` | `VkInstance`/`VkDevice`/`VkQueue`/`VkFormat`/`VkPipelineRenderingCreateInfo` 等 |
| SDL3 头 | `<SDL3/SDL.h>` | `SDL_Window` / `SDL_Event` |
| 帧模块回调类型 | `Application/Renderer/src/render/Frame/帧上下文.h` | `Frame_Ui_Callback` |
| 结果检查出口（借入 logger） | `Application/Renderer/src/render/Renderer/core/结果检查.h` | 间接引入全局 `logger`（仅用于 `build` 告警） |
| 门面取用入口 | `Application/Renderer/src/render/Renderer/渲染器.h` / `core/渲染器.cpp` | `Renderer::ui_channel()`；`init` ⑰ `build`；`frame_render` 取回调；`shutdown` 解桥接 + `destroy` |
| 平台事件外送 | `Application/Renderer/src/render/Platform/平台上下文.h` / `core/事件泵.cpp` | `event_forward_set` / `events_pump` 逐事件转发 |
| 上层实现 | `Application/Test/src/主调/图形操作界面.cpp` | ImGui 初始化、信息填装、回调注册与清理 |
| Dear ImGui | `Application/Test/external/Dear_ImGui/backends/imgui_impl_vulkan.h` / `imgui_impl_sdl3.*` | `ImGui_ImplVulkan_InitInfo`（对齐参照；渲染层不 include） |

### 附录 B. 本模块待确认条目索引

| 编号 | 一句话 | 正文标记位置 |
| --- | --- | --- |
| `R-UICH-01` | 交换链重建后未刷新 UI 通道信息、亦未通知 ImGui 后端（`MinImageCount`/`ImageCount` 可能失配） | §6.4、§7.3、§7.5 |

**R-UICH-01 详情**：交换链重建（门面 `frame_render` 的事件路径与呈现路径）只调 `swapchain.rebuild` + `pipeline.pipeline_rebuild`，**未**重新 `ui_channel.build`、也**未**调用 Dear ImGui 提供的 `ImGui_ImplVulkan_SetMinImageCount`（全仓无该调用点）。故若某次重建使交换链图像数变化，则 `Ui_Channel_Info::image_count` 会过期、上层 `ImGui_ImplVulkan_InitInfo::ImageCount` 与之失配。源码未表达「是否应同步刷新」的意图——是「刻意接受（本项目呈现模式下图像数不随重建变化）」还是「未覆盖的边界」，无法从源码确证，故标 `【待确认：R-UICH-01】`，正文只作事实陈述、不作定论。

*（本文档为「白银纪元」渲染层-UI 通道模块导航，正文不含任何绝对路径；所有路径均为相对源码根 `工作区` 的相对路径。）*
