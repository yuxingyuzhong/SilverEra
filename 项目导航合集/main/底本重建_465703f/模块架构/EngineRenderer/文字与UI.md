# 文字与UI模块

## 一、文件内目录

本模块覆盖 `Engine/EngineRenderer/src/render/` 下两个并列子目录，共 13 个文件。快照内 `Application/Game/星屑弹幕/external/silver_era/include/...` 下的同名头是外部使用方的随包拷贝，不属于本模块源码，不重复登记。

| 相对工作区根路径 | 一句话职责 |
| --- | --- |
| `Engine/EngineRenderer/src/render/Text/文字布局数据.h` | 定义行内对齐枚举 `Text_Align` 与布局参数 `Text_Layout`（换行宽度、行高、字距、首行缩进、行距倍数）。 |
| `Engine/EngineRenderer/src/render/Text/字体.h` | 定义字形度量 `Font_Glyph`、字体记录 `Font_Record`、字体上下文 `Font_Context`（TTF → 字形图集 → 度量/几何）。 |
| `Engine/EngineRenderer/src/render/Text/文字渲染.h` | 定义宿主回调结构 `Text_Host` 与文字绘制子系统 `Text_Renderer`（网格缓存、颜色、管线句柄）。 |
| `Engine/EngineRenderer/src/render/Text/局部命名空间使用.h` | 文字子目录的私有 `using` 头：引入 `字体.h`，并声明 `std::string` / `std::vector` 别名。 |
| `Engine/EngineRenderer/src/render/Text/core/字体加载.cpp` | `Font_Context` 的实现：stb_truetype 单编译单元、ASCII 预烘、行式打包、惰性补烘、图集扩容、纹理创建与归还。 |
| `Engine/EngineRenderer/src/render/Text/core/文字几何.cpp` | UTF-8 解码、码点收集、单行度量、按布局切行/对齐/产出四边形几何。 |
| `Engine/EngineRenderer/src/render/Text/core/文字渲染.cpp` | `Text_Renderer` 的实现：字形补烘、管线确保、按（字体, 文本, 布局）查表、LRU 淘汰、延迟归还、像素→NDC 矩阵与绘制项登记。 |
| `Engine/EngineRenderer/src/render/UI/用户界面.h` | 定义 UI 句柄/控件类型/矩形/颜色/控件节点 `Ui_Node` 与保留模式控件树 `Ui_Context`（Vulkan-free）。 |
| `Engine/EngineRenderer/src/render/UI/UI渲染器.h` | 定义 `Ui_Renderer`：每帧槽动态顶点/索引缓冲、两条 UI 管线、字形图集绑定与 UI 阶段录制入口（层内实现头，含 Vulkan 类型）。 |
| `Engine/EngineRenderer/src/render/UI/局部命名空间使用.h` | UI 子目录的私有 `using` 头：引入 `用户界面.h`，并声明 `std::string` / `std::vector` 别名。 |
| `Engine/EngineRenderer/src/render/UI/core/控件树.cpp` | `Ui_Context` 的创建/销毁/属性读写实现：槽位复用、句柄索引、后代级联释放。 |
| `Engine/EngineRenderer/src/render/UI/core/交互.cpp` | 每帧交互：命中测试、悬停/按下/点击、焦点切换、文本框 UTF-8 编辑（含 SDL 扫描码）。 |
| `Engine/EngineRenderer/src/render/UI/core/UI渲染器.cpp` | `Ui_Renderer` 的实现：控件树遍历、纯色四边形与文字几何追加、帧槽缓冲扩容与持久映射、管线切换、描述符与推送常量下发、索引绘制。 |

本文件内跳转索引：

- [§二 模块功能介绍](#二模块功能介绍)
- [§三 模块公开接口](#三模块公开接口)
- [§四 模块私有方法](#四模块私有方法)
- [§五 模块内部架构](#五模块内部架构)
- [§六 模块设计 / 实现细节](#六模块设计--实现细节)
- [§七 模块运行链路详例](#七模块运行链路详例)
- [附：待确认正文](#附待确认正文)

---

## 二、模块功能介绍

**定位**。本模块是 `EngineRenderer` 层内“文字”与“原生 UI”两个并列子域的集合。文字子域负责把 TTF 字体变成可采样的字形图集、把 UTF-8 字符串变成像素空间的四边形几何、并把几何缓存成可复用的网格句柄；UI 子域负责保留模式的控件树、每帧输入消费（命中/焦点/点击/文本编辑），以及把控件树转成每帧动态顶点/索引并在 UI 阶段录制到命令缓冲。两者共用同一份 `Font_Context` 字形数据，但绘制路径完全不同：文字走“登记绘制项、由场景阶段消费”的延迟路径，UI 走“UI 阶段即时录制”的直写路径。

**文字职责边界**。负责：字体文件字节读取（原样 `std::ifstream` 打开）、ASCII 可打印字符预烘、非 ASCII 码点惰性补烘、字形图集（RGBA8，RGB 恒白、A 为覆盖率）的创建/扩容/重建、字形度量（uv、像素矩形、笔位推进）、UTF-8 解码、单行度量、按布局切行/对齐/行距/缩进/字距的几何生成、按（字体句柄, 文本, 布局六项）缓存网格、LRU 淘汰与延迟归还、文字颜色钳制、文字管线句柄的记录与回填、像素坐标到 NDC 的仿射矩阵与绘制项登记。不负责：文字管线与着色器的创建（由门面惰性登记后回填句柄）、描述符集写装（由门面在纹理绑定路径完成）、场景合批与阴影（文字绘制项被显式排除在阴影投影与静态合批之外）。

**UI 职责边界**。负责：控件节点的创建/销毁/属主关系、句柄分配与槽位复用、可见/可用/悬停/按下/焦点/单击的每帧状态机、逆序命中测试、焦点唯一性、文本框的 UTF-8 光标移动与增删、控件树到顶点/索引的转换、每帧槽缓冲的扩容与持久映射、两条 UI 管线的切换、UI 专用采样描述符集的按需改写、推送常量（矩阵 + 颜色）下发与 `vkCmdDrawIndexed` 提交。不负责：窗口事件采集（由平台事件泵 → 输入聚合器喂入）、平台文本输入启停的具体实现（只通过 `text_focus_active()` 表达意愿）、外部 ImGui 类 UI（由门面 `ui_channel` 另行回调，且绘制在原生 UI 之后覆盖其上）、裁剪/滚动/拖拽/右键菜单/模态层（本模块没有这些能力）。

**依赖边界**。`文字布局数据.h`、`字体.h`、`用户界面.h` 三个对外头保持 Vulkan-free 与 SDL-free，只依赖前置头与 `Mesh_Data`、`Input_Frame`；`UI渲染器.h` 是渲染层“实现细节”头，直接包含 `vulkan/vulkan.h`、`vk_mem_alloc.h` 与内存/管线/纹理/字体模块头，只允许本层 `.cpp` 与层内模块头包含，不进公共头。`UI渲染器.cpp` 不直接用 SDK 之外的平台接口；`交互.cpp` 私有包含 `<SDL3/SDL.h>` 以取扫描码常量（渲染层允许层内私有包含）。

**关键约束**。

1. 句柄语义统一为“0 = 无效，下标 = 句柄 − 1”，字体句柄与 UI 句柄都靠槽位下标分配，槽位可复用，且没有代际计数（见 §六）。
2. 字形图集宽度固定 512 像素，高度按内容决定且只在扩容时翻倍；已烘焙字形的 `u` 分量因此恒定，`v` 分量只在扩容时变化。
3. 所有文字都按“一个（字体, 文本, 布局）组合一个网格、一个绘制项”处理，不按图集做跨文本合批。
4. 控件树的绘制顺序等于槽位顺序（即创建顺序），命中测试为槽位逆序；父节点不参与坐标变换，子节点矩形是绝对像素矩形。
5. 本模块所有状态都没有加锁，隐含“只在渲染线程访问”的约定；字体图集纹理、UI 缓冲的创建/销毁都发生在渲染线程。

**存在理由**。上层（应用层 / 游戏层）不应该为了写一行中文或摆一个按钮而接触 Vulkan、描述符集、帧槽与命令缓冲；本模块把“像素坐标 + UTF-8 文本”作为唯一输入语义，把 GPU 侧的管线、纹理、缓冲与录制时机全部收在门面与本模块内部，从而让文字与简单 UI 能以近乎“2D 绘图 API”的形式被调用。

**能力边界（明确不支持）**。排版层面：不识别单词边界（逐字符断行）、不支持 RTL/竖排/字间混排样式、不支持富文本（同一段落多字体/多颜色）、不支持省略号截断、不支持行内图片；UI 层面：没有布局约束系统（锚点/百分比/自动换行）、没有父子坐标继承、没有裁剪矩形与滚动视图、没有模态与输入拦截、文本框只支持单行且不处理输入法组合串（`preedit` 未被使用）。

---

## 三、模块公开接口

本节只列头文件中真实存在的声明。`core/*.cpp` 中发现的实现与声明/注释不一致处在条目内标【待确认：N】，正文集中在文末“附：待确认正文”。

### 3.1 字体常量与句柄

```cpp
// Engine/EngineRenderer/src/render/Text/字体.h
inline constexpr uint32_t FONT_CODEPOINT_FIRST = 32;                              // ASCII 可打印首码点（空格）
inline constexpr uint32_t FONT_CODEPOINT_LAST  = 126;                             // ASCII 可打印末码点（~）
inline constexpr uint32_t FONT_GLYPH_COUNT     = FONT_CODEPOINT_LAST - FONT_CODEPOINT_FIRST + 1; // 95
```

- 句柄类型：`uint64_t`（`Font_Context::font_load_file` 的出参），有效值从 1 起，`0` 表示无效；内部下标 = 句柄 − 1。
- 句柄只保证在对应记录 `valid == true` 期间有效；`font_destroy` 后槽位可被下一次加载复用，旧句柄不会自动失效标记，可能指向新字体（见 §六）。
- 文本网格缓存键里也使用该句柄，因此卸载字体时必须先调用 `Text_Renderer::font_meshes_destroy`（门面 `font_unload` 已按此顺序编排）。

### 3.2 `Font_Glyph`（字形度量）

```cpp
struct Font_Glyph
{
    float u0 = 0.0f;   // 图集 uv：左上
    float v0 = 0.0f;
    float u1 = 0.0f;   // 图集 uv：右下
    float v1 = 0.0f;
    float x0 = 0.0f;   // 像素矩形：左上 x（相对文本原点）
    float y0 = 0.0f;   // 像素矩形：左上 y（相对文本原点，Y 向下）
    float x1 = 0.0f;   // 像素矩形：右下 x
    float y1 = 0.0f;   // 像素矩形：右下 y
    float advance = 0.0f;              // 笔位推进（像素）
    uint32_t atlas_x = 0;              // 图集内原始矩形（像素）
    uint32_t atlas_y = 0;
    uint32_t atlas_w = 0;
    uint32_t atlas_h = 0;
    bool has_ink = false;              // 是否有可见像素（空白字符为假，但仍携带 advance）
};
```

语义与前置条件：

- `x0/y0/x1/y1` 以“文本左上角”为原点、Y 向下，已把 `baseline` 并入（`y0 = stb 位图 y 偏移 + baseline`）。
- 只有 `has_ink == true` 且 `x1 > x0`、`y1 > y0` 的字形才会产出四边形；空白字形只推进笔位。
- `atlas_*` 是补烘/扩容后重算 `v` 的唯一依据，`u` 不需要重算（图集宽度恒定）。
- 未支持码点会在字形表里登记一个“全零”的 `Font_Glyph`（`has_ink == false`、`advance == 0`），从而在几何阶段被静默跳过且不再重复尝试。

### 3.3 `Font_Record`（字体记录）

```cpp
struct Font_Record
{
    uint32_t pixel_height = 0;                          // 字号（字形像素高度）
    float baseline = 0.0f;                              // 基线相对文本原点的偏移（floor(ascent × scale + 0.5)）
    uint64_t atlas_texture = 0;                         // 字形图集纹理句柄（纹理模块持有）
    uint32_t atlas_width = 0;                           // 图集宽（恒 512）
    uint32_t atlas_height = 0;                          // 图集高（扩容时翻倍）
    std::unordered_map<uint32_t, Font_Glyph> glyphs;    // 码点 → 度量（惰性追加）
    std::vector<uint8_t> font_bytes;                    // 字体文件字节（惰性补烘需重新 InitFont）
    std::vector<uint8_t> atlas_pixels;                  // 图集像素（RGBA8 紧排）
    uint32_t cursor_x = 0;                              // 行式打包游标 x
    uint32_t cursor_y = 0;                              // 行式打包游标 y（当前行顶）
    uint32_t shelf_height = 0;                          // 当前行高度
    std::vector<uint64_t> retired_atlas_textures;       // 已退役的旧图集纹理（字体销毁时统一归还）
    uint64_t skipped_codepoints = 0;                    // 累计被跳过的码点数（按码点去重）
    bool valid = false;                                 // 记录是否有效
};
```

语义：

- `pixel_height` 同时是“单行文本高度”的定义（`text_measure` 的 `height_out` 恒等于它），也是 `line_height == 0` 时的行距。
- `skipped_codepoints` 只统计补烘阶段判定“无法呈现”的码点（字体不含、字形过宽、图集空间不足），UTF-8 解码失败的字节不计。
- `retired_atlas_textures` 的存在说明“图集纹理会被多次重建”，旧句柄不能立即销毁。
- 该结构是公开类型但不应由外部构造；外部只通过 `font_get` 读取。

### 3.4 `Font_Context`

```cpp
class Font_Context
{
public:
    Font_Context(void) = default;                                   // 不加载任何字体
    ~Font_Context(void);                                            // 内部调用 destroy()
    Font_Context(const Font_Context&) = delete;                     // 不可拷贝
    Font_Context& operator=(const Font_Context&) = delete;

    bool build(Texture_Context& texture);                           // 记录纹理模块
    void destroy(void);                                             // 幂等；归还全部图集纹理并清空记录
    bool valid(void) const;                                         // texture != nullptr
    uint32_t font_count(void) const;                                // 有效记录数

    bool font_load_file(uint64_t& handle_out, const std::string& path, uint32_t pixel_height);
    void font_destroy(uint64_t handle);
    const Font_Record* font_get(uint64_t handle) const;
    uint64_t font_skipped_codepoints_get(uint64_t handle) const;
    Font_Record* font_get_mutable(uint64_t handle);

    bool glyphs_ensure(Font_Record& record, const std::string& text);

    static bool text_codepoints(const std::string& text, std::vector<uint32_t>& codepoints_out);
    static const Font_Glyph* glyph_seek(const Font_Record& record, uint32_t codepoint);
    static bool text_measure(const Font_Record& record, const std::string& text,
        uint32_t& width_out, uint32_t& height_out);
    static bool text_geometry_build(Mesh_Data& data_out, const Font_Record& record, const std::string& text);
    static bool text_layout_build(Mesh_Data& data_out, const Font_Record& record, const std::string& text,
        const Text_Layout& layout);
};
```

逐条语义、前置条件与失败行为：

| 成员 | 语义 / 前置 | 失败与告警 |
| --- | --- | --- |
| `bool build(Texture_Context& texture)` | 记录字形图集上传/销毁所用的纹理模块；必须先于任何加载调用。 | 若 `texture.valid()` 为假：`logger.error("字体:: 构建时纹理模块不可用")`，返回假。重复调用会覆盖记录。 |
| `void destroy(void)` | 幂等清理：逐条归还 `atlas_texture` 与 `retired_atlas_textures`，把记录整体复位，清空 `records`，`texture = nullptr`。析构函数自动调用。 | 纹理模块为空时只复位记录不归还句柄（句柄由外部已销毁的模块负责，属正常路径）。 |
| `bool valid(void) const` | 是否已记录纹理模块。 | 无。 |
| `uint32_t font_count(void) const` | 统计 `valid == true` 的记录数（不区分槽位空洞）。 | 无。 |
| `bool font_load_file(uint64_t&, const std::string&, uint32_t)` | 从 TTF 读取字节、`stbtt_InitFont`、按 `pixel_height` 求缩放、烘焙 ASCII 32..126、行式打包进 512 宽图集、`texture_create_from_memory` 上传、分配槽位。成功写出 ≥1 的句柄。 | 出参先置 0；未构建 / `pixel_height == 0` / 文件打不开 / 文件为空 / 读取失败 / 字体解析失败 / 任一 ASCII 字形“宽 + 2 > 512” / 图集纹理创建失败 → 均 `logger.error` 并返回假；失败路径不留下记录（局部记录在栈上，位图逐个 `stbtt_FreeBitmap` 释放）。 |
| `void font_destroy(uint64_t)` | 归还该记录的当前图集与全部退役图集，整条记录复位（槽位变空）。 | `handle == 0` / 越界 / 已空 → 静默忽略。成功记 `logger.info`。 |
| `const Font_Record* font_get(uint64_t) const` | 只读取记录。 | 句柄无效或已空 → `nullptr`。 |
| `uint64_t font_skipped_codepoints_get(uint64_t) const` | 取诊断计数。 | 句柄无效或已空 → 0。 |
| `Font_Record* font_get_mutable(uint64_t)` | 可写取记录，供惰性补烘修改图集、游标与度量。 | 句柄无效或已空 → `nullptr`。 |
| `bool glyphs_ensure(Font_Record&, const std::string&)` | 把文本中尚未烘焙的码点光栅化并追加进该字体图集；确有像素写入时重建图集纹理并退役旧纹理。返回真表示“所需字形已全部可用或无需补烘”。 | `record.valid == false` 或纹理模块为空 → 静默返回假；`text_codepoints` 一个码点都没解出 → 返回真（视为无需补烘）；`font_bytes` 为空 → 报错返回假；`stbtt_InitFont` 失败 → 报错返回假；单码点“字体不含/过宽/扩容后仍放不下” → `logger.warn` + `skipped_codepoints` 自增并登记空度量，不中断其余码点；图集纹理重建失败 → 报错返回假。 |
| `static bool text_codepoints(const std::string&, std::vector<uint32_t>&)` | 解码并**去重**收集码点；解码失败只跳过 1 字节。 | 返回“是否至少解出一个码点”；出参先清空。 |
| `static const Font_Glyph* glyph_seek(const Font_Record&, uint32_t)` | 在已烘焙字形表中查度量。 | 未烘焙 → `nullptr`（几何/度量阶段据此“跳过且不推进笔位”）。 |
| `static bool text_measure(const Font_Record&, const std::string&, uint32_t&, uint32_t&)` | 单行度量：笔位按 `advance` 累加，宽度取“最后一个有墨迹字形推进后的笔位”四舍五入，高度恒为 `pixel_height`；不做换行，不含 `tracking`。 | 记录无效 / 没有任何可呈现字形 / 右缘 ≤ 0 → 两个出参置 0 并返回假。 |
| `static bool text_geometry_build(Mesh_Data&, const Font_Record&, const std::string&)` | 等价于以默认布局调用 `text_layout_build`（单行、左对齐、不换行）。 | 同下。 |
| `static bool text_layout_build(Mesh_Data&, const Font_Record&, const std::string&, const Text_Layout&)` | 两阶段：逐字符切行（显式 `'\n'`、`max_width` 超限）→ 逐行按对齐平移并在行顶偏移下产出四边形；顶点 `position` 为像素坐标（z = 0），`uv` 为图集 uv。 | 出参先清空；记录无效或 `any_ink == false` → 返回假且不留下几何。 |

【待确认：6】`text_measure` 的头注释写“宽度 = 末个有墨迹字形的右缘”，实现却把 `ink_right` 取成“推进 `advance` 之后的笔位”，与字形位图右缘 `x1` 不是同一量（详见文末）。

### 3.5 `Text_Align` 与 `Text_Layout`

```cpp
enum class Text_Align
{
    Left,     // 每行起始笔位落在文本原点
    Center,   // 每行按自身行宽居中于 [原点, 原点 + max_width)
    Right,    // 每行右端对齐到 原点 + max_width
};

struct Text_Layout
{
    float max_width = 0.0f;          // 换行宽度上限（像素；0 = 不换行，此时 align 无效果）
    float line_height = 0.0f;        // 行高（像素；0 = 取 Font_Record::pixel_height）
    Text_Align align = Text_Align::Left;
    float tracking = 0.0f;           // 字距（像素；计入行宽，故影响换行与对齐）
    float first_line_indent = 0.0f;  // 首行缩进（像素；不计入行宽与换行判据）
    float line_spacing = 1.0f;       // 行距倍数（(0, 8]，越界/非数按 1）
};
```

前置与口径：

- `max_width == 0` 时整段单行，`align` 与 `first_line_indent` 之外的对齐行为全部失效（`first_line_indent` 在实现里仍会叠加到首行笔位）。
- `line_spacing` 只改变纵向行间步进（`line_top = 行号 × line_height × line_spacing`），不影响换行判据、行宽与对齐。
- `tracking` 同时进入“换行判定”和“行宽（对齐基准）”；`text_measure` 明确不含 `tracking`，故“度量宽度”与“布局行宽”在非零字距下不相等。
- `first_line_indent` 不计入行宽也不参与换行，右对齐/居中时首行整体右移，可能超出 `max_width`。

### 3.6 `Text_Host`（宿主回调）

```cpp
struct Text_Host
{
    void* user = nullptr;                                                   // 回调上下文（门面自身）
    uint64_t (*pipeline_ensure)(void* user) = nullptr;                      // 确保文字管线就绪（0 = 失败）
    void     (*pipeline_bind)(void* user, uint64_t pipeline) = nullptr;     // 绑定（激活）管线
    void     (*texture_bind)(void* user, uint64_t texture) = nullptr;       // 绑定字形图集为当前纹理
    bool     (*mesh_create)(void* user, Mesh_Handle& mesh, const Mesh_Data& data) = nullptr;
    void     (*mesh_destroy)(void* user, Mesh_Handle mesh) = nullptr;
    bool     (*mesh_draw)(void* user, Mesh_Handle mesh, const float* transform, const float* color) = nullptr;
    void     (*extent_get)(void* user, float& width, float& height) = nullptr; // 当前绘制目标尺寸（像素）
};
```

- 七个函数指针的**实际语义**由门面注入：`pipeline_ensure` 惰性登记 `文字.vert.spv` / `文字.frag.spv`（`Blend_Mode::Alpha`，按配置开启 alpha-to-coverage）并把句柄回填 `Text_Renderer::pipeline_set`；`pipeline_bind` 激活管线；`texture_bind` 走门面纹理绑定（会等待全部在途帧后写满所有帧槽 set1）；`mesh_create/mesh_destroy` 转发网格模块；`mesh_draw` 登记一个延迟绘制项；`extent_get` 取交换链尺寸。
- `draw_layout` 在“全部七个回调非空且字体模块非空”时才继续，否则整体拒绝，避免半程失败（见 §六）。

### 3.7 `Text_Renderer`

```cpp
class Text_Renderer
{
public:
    void init(Font_Context& font, const Text_Host& host);
    bool draw_layout(uint64_t font, const std::string& text, float pixel_x, float pixel_y,
        const Text_Layout& layout);
    void color_set(const float color[4]);
    const float* color(void) const;
    uint64_t pipeline_get(void) const;
    void pipeline_set(uint64_t value);
    void font_meshes_destroy(uint64_t font);
    void retired_flush(void);
    void frame_begin(uint32_t overlap);
    void shutdown(void);
    uint64_t mesh_cache_hits(void) const;                                    // 内联：返回 cache_hits
    uint64_t mesh_cache_misses(void) const;                                  // 内联：返回 cache_misses
    void mesh_cache_capacity_set(uint32_t capacity);
    uint32_t mesh_cache_capacity(void) const;                                // 内联：返回 mesh_cache_max
    uint32_t mesh_cache_size(void) const;                                    // 内联：static_cast<uint32_t>(records.size())
};
```

| 成员 | 语义 / 前置 | 失败与告警 |
| --- | --- | --- |
| `init(Font_Context&, const Text_Host&)` | 绑定字体模块与宿主回调；**不持有**字体与任何 GPU 资源。重复调用以最后一次为准（旧缓存不清理）。 | 无校验、无日志。 |
| `draw_layout(uint64_t, const std::string&, float, float, const Text_Layout&)` | 像素坐标、原点在绘制目标左上、Y 向下；`pixel_x/pixel_y` 为**首行左上角**。内部顺序：宿主完整性检查 → 取可写字体记录 → 空文本直接成功 → `glyphs_ensure` → `pipeline_ensure` → 缓存查找/新建 → 管线与图集绑定 → 取目标尺寸 → 组装像素→NDC 矩阵 → `mesh_draw` 登记。 | 宿主回调不全 / 字体句柄无效 / 字形补烘失败 / 管线登记返回 0 / 几何生成失败（文本无可见字形）/ 网格创建失败 / 目标尺寸非法 / 绘制登记失败 → 各自 `logger.error` 并返回假；空文本返回真但不登记绘制项。 |
| `color_set(const float color[4])` | 逐分量钳制到 `[0,1]`，非数按 0 处理。 | 空指针静默忽略。 |
| `const float* color(void) const` | 指向内部 4 浮点 RGBA（默认白）。 | 无。 |
| `uint64_t pipeline_get(void) const` | 当前记录的管线句柄（0 = 未登记）。 | 无。 |
| `void pipeline_set(uint64_t)` | 由 `pipeline_ensure` 的回调回填。 | 无。 |
| `void font_meshes_destroy(uint64_t font)` | 把该字体的缓存条目移入待销毁列表并移除记录，重建分桶索引，然后**立即** `retired_flush`。 | 无日志；`mesh_destroy` 回调为空时记录被直接丢弃。 |
| `void retired_flush(void)` | 对 `retired` 中全部条目调用 `mesh_destroy` 并清空。 | 宿主未绑定销毁回调时直接丢弃记录。 |
| `void frame_begin(uint32_t overlap)` | 记录 `frame_overlap`（`<1` 按 1）、`++frame_clock`；逐项归还满足 `retired_tick + frame_overlap < frame_clock` 的待销毁网格。 | 宿主 `mesh_destroy` 为空时只推进时钟，不归还。 |
| `void shutdown(void)` | 把全部缓存条目移入 `retired`、清空缓存与分桶索引、统一归还，复位 `pipeline`、`use_clock`、`frame_clock` 与颜色为白。 | `hits/misses/mesh_cache_max/frame_overlap` 不复位（诊断量保留）。 |
| `uint64_t mesh_cache_hits/misses(void) const` | 自 `init` 起累计的命中/未命中计数。 | 无。 |
| `void mesh_cache_capacity_set(uint32_t)` | 钳到 `[1, 1024]`（0 按 1）；超出上限时循环淘汰最久未用项（移入 `retired`，`retired_tick = 当前帧`），每次淘汰后重建分桶索引。 | 无日志。 |
| `uint32_t mesh_cache_capacity(void) const` | 当前上限（默认 64）。 | 无。 |
| `uint32_t mesh_cache_size(void) const` | 当前条目数（`records.size()`）。 | 无。 |

### 3.8 `Ui_Handle` / `Ui_Node_Type` / `Ui_Rect` / `Ui_Color`

```cpp
using Ui_Handle = uint64_t;                        // 0 = 无效 / 已销毁

enum class Ui_Node_Type : uint8_t
{
    Root,       // 根节点（覆盖整个窗口，不绘制）
    Panel,      // 矩形背景，不参与交互
    Button,     // 矩形背景 + 居中文本
    Text_Box    // 矩形背景 + 左对齐文本 + 光标
};

struct Ui_Rect
{
    float x = 0.0f;   // 左上角（窗口像素坐标，Y 向下）
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;
    bool contains(float px, float py) const;    // 左闭右开、上闭下开
};

struct Ui_Color
{
    float r = 1.0f;
    float g = 1.0f;
    float b = 1.0f;
    float a = 1.0f;
};
```

`Ui_Rect::contains` 实现为 `px >= x && px < x + w && py >= y && py < y + h`：相邻矩形不会同时命中同一点。

### 3.9 `Ui_Node`（控件树节点）

```cpp
struct Ui_Node
{
    Ui_Handle handle = 0;                                   // 句柄（0 = 无效 / 已销毁）
    Ui_Handle parent = 0;                                   // 父句柄（0 = 根节点）
    Ui_Node_Type type = Ui_Node_Type::Root;
    Ui_Rect rect{};                                         // 绝对像素矩形
    Ui_Color color{};                                       // 背景色
    Ui_Color text_color{};                                  // 文本色
    std::string label{};                                    // 按钮标签
    std::string text{};                                     // 文本框内容（UTF-8）
    std::size_t caret = 0;                                  // 文本框光标（字节偏移）
    bool visible = true;                                    // 是否可见
    bool enabled = true;                                    // 是否可用（不可用不参与命中）
    bool hovered = false;                                   // 是否悬停
    bool pressed = false;                                   // 是否按下
    bool focused = false;                                   // 是否持有焦点
    bool clicked = false;                                   // 本帧是否被点击（可被 clicked_consume 取走）
    void (*on_click)(Ui_Handle handle, void* user) = nullptr;
    void* user = nullptr;
    bool alive = true;                                      // 是否存活（内部使用；0 = 已销毁）
};
```

语义与隐式约定：

- `enabled` 只影响命中（`hit_test` 跳过不可用节点），不影响绘制（禁用按钮仍以原色绘制）。
- `label` 仅对 `Button` 有绘制意义，`text` 仅对 `Text_Box` 有绘制意义；两者都可以在任何类型上写入，不做校验。
- `caret` 是 UTF-8 **字节**偏移，不是码点序号；插入/删除都按“字节区间”操作（见 §六）。
- 节点矩形是绝对窗口像素坐标：父节点移动不会带动子节点。
- `visible`/`enabled` 不沿父链继承：隐藏父节点不会隐藏其子节点（父子关系只用于 `destroy` 级联）。
- `alive == false` 的槽位对 `node_at` 不可见，但 `node_count()` 仍计入。

### 3.10 `Ui_Context`（控件树与交互）

```cpp
class Ui_Context
{
public:
    Ui_Context(void);                                     // 自动建立根节点（句柄 1）
    ~Ui_Context(void) = default;
    Ui_Context(const Ui_Context&) = delete;
    Ui_Context& operator=(const Ui_Context&) = delete;

    void build(void);                                     // 复位控件树并重建根节点
    void shutdown(void);                                  // 清空控件树与交互状态（根节点也清除）
    Ui_Handle root_get(void) const;                       // 空树返回 0，否则恒为 1

    Ui_Handle panel_create(Ui_Handle parent, const Ui_Rect& rect, const Ui_Color& color);
    Ui_Handle button_create(Ui_Handle parent, const Ui_Rect& rect, const std::string& label,
        void (*on_click)(Ui_Handle, void*) = nullptr, void* user = nullptr,
        const Ui_Color& color = Ui_Color{ 0.20f, 0.20f, 0.50f, 1.0f });
    Ui_Handle text_box_create(Ui_Handle parent, const Ui_Rect& rect, const std::string& text);
    bool destroy(Ui_Handle handle);

    bool rect_set(Ui_Handle handle, const Ui_Rect& rect);
    bool rect_get(Ui_Handle handle, Ui_Rect& rect_out) const;
    bool color_set(Ui_Handle handle, const Ui_Color& color);
    bool label_set(Ui_Handle handle, const std::string& label);
    bool text_set(Ui_Handle handle, const std::string& text);
    bool text_get(Ui_Handle handle, std::string& text_out) const;
    bool visible_set(Ui_Handle handle, bool visible);
    bool enabled_set(Ui_Handle handle, bool enabled);
    bool clicked_consume(Ui_Handle handle);
    bool hovered_get(Ui_Handle handle) const;
    bool focused_get(Ui_Handle handle) const;

    bool text_focus_active(void) const;
    void font_set(uint64_t font);
    uint64_t font_get(void) const;

    std::size_t node_count(void) const;
    const Ui_Node* node_at(std::size_t index) const;

    void frame_begin(const Input_Frame& input);
};
```

| 成员 | 语义 / 前置 | 失败行为 |
| --- | --- | --- |
| 构造 | 调用 `build()`，建立根节点（`handle = 1`、`parent = 0`、`type = Root`、背景色 `(0.05,0.05,0.10,1)`、文本色白）。 | 无。 |
| `build(void)` | 清空 `nodes` 与三个交互句柄，再压入根节点。可从任意状态调用。 | 无。 |
| `shutdown(void)` | 清空 `nodes`、三个交互句柄与 `font`；`root_get()` 随后返回 0。 | 无。 |
| `root_get(void) const` | 根句柄（空树为 0）。 | 无。 |
| `panel_create` | 建 `Panel` 节点并写入背景色。父句柄无效/已销毁 → 挂到根节点；树为空 → 返回 0。 | 无日志。 |
| `button_create` | 建 `Button` 节点并写入 `label`、`on_click`、`user`、背景色（默认深蓝 0.20/0.20/0.50/1）。 | 同上。 |
| `text_box_create` | 建 `Text_Box` 节点，写入 `text`、`caret = text.size()`、深色背景 `(0.02,0.02,0.05,0.95)`、白色文本。 | 同上。 |
| `destroy(Ui_Handle)` | 句柄 1（根）禁止销毁返回假；不存在返回假；否则按“每轮回收一层直接子节点、直到无子节点”的方式级联释放全部后代，最后释放自身。 | 返回假，无日志。 |
| `rect_set / rect_get / color_set / label_set / text_set / text_get` | 属性读写；`text_set` 同时把 `caret` 移到末尾。 | 句柄无效 → 返回假（不写任何内容）。 |
| `visible_set` | 写 `visible`；置假且该节点正持焦点时清焦点。 | 句柄无效 → 假。 |
| `enabled_set` | 写 `enabled`；置假且该节点正持焦点时清焦点。 | 句柄无效 → 假。 |
| `clicked_consume(Ui_Handle)` | 取走单击标记（读后清零）。 | 句柄无效 → 返回假（不清任何状态）。 |
| `hovered_get / focused_get` | 只读查询。 | 句柄无效 → 假。 |
| `text_focus_active(void) const` | 当前焦点节点是 `Text_Box` 且 `visible && enabled` 时为真；供门面驱动平台文本输入启停。 | 焦点无效 → 假。 |
| `font_set(uint64_t) / font_get(void) const` | UI 绘制所用字体句柄（0 = 不绘制文本）。 | 无。 |
| `node_count(void) const` | 槽位总数（含根、含已销毁空槽）。 | 无。 |
| `node_at(std::size_t) const` | 按槽位取只读节点；越界或已销毁 → `nullptr`。 | 无。 |
| `frame_begin(const Input_Frame&)` | 每帧一次：刷根矩形为窗口客户区、清点击标记、校验焦点、命中测试刷新悬停、处理左键按下/抬起沿、刷新按下态、编辑焦点文本框（详见 §七）。 | 树为空时除根处理外均为空操作；不产生日志。 |

命中测试的**可观测接口**是 `hovered_get` / `focused_get` / `clicked_consume` / `text_focus_active`；真正的命中函数 `hit_test` 是私有成员，见 §四。

### 3.11 `Ui_Renderer`

```cpp
class Ui_Renderer
{
public:
    Ui_Renderer(void) = default;                          // 不创建任何 GPU 资源
    ~Ui_Renderer(void);                                   // 内部调用 shutdown()
    Ui_Renderer(const Ui_Renderer&) = delete;
    Ui_Renderer& operator=(const Ui_Renderer&) = delete;

    bool build(VkDevice device, Memory_Context& memory, Pipeline_Context& pipeline,
        Texture_Context& texture, Font_Context& font,
        uint64_t solid_pipeline, uint64_t text_pipeline, uint32_t frame_overlap);
    void shutdown(void);
    bool valid(void) const;
    void draw(const Ui_Context& ui, VkCommandBuffer command_buffer, VkExtent2D extent, uint32_t slot);
};
```

| 成员 | 语义 / 前置 | 失败与告警 |
| --- | --- | --- |
| `build(...)` | 记录设备与四个模块指针、两条 UI 管线句柄，按 `frame_overlap` 预分配槽记录（不创建缓冲）。 | `device == VK_NULL_HANDLE` / `memory.valid()` 假 / `pipeline.valid()` 假 → `logger.error("UI 渲染器:: 构建前置不完整")` 返回假；`frame_overlap == 0` → 报错返回假。`texture` / `font` 的 `valid()` 不做校验（后续绘制时按空处理）。 |
| `shutdown(void)` | 逐帧槽归还顶点/索引缓冲（内存模块非空时），清空 CPU 数组、绘制记录、临时文字几何与依赖指针。幂等。 | 无日志。 |
| `valid(void) const` | `device != VK_NULL_HANDLE && memory && pipeline && !slots.empty()`。**不校验** `texture/font`。 | 无。 |
| `draw(const Ui_Context&, VkCommandBuffer, VkExtent2D, uint32_t)` | 每帧把控件树转成顶点/索引并录制到给定命令缓冲；`slot` 为当前帧槽；`extent` 为 UI 阶段颜色附件尺寸。无可见控件时不产生任何绘制命令。 | 未构建 / 命令缓冲空 / 槽越界 / 尺寸为 0 → 静默返回；无索引 → 返回；槽缓冲扩容失败 → 返回；两条管线都未登记 → 返回。 |

### 3.12 本模块消费的外部结构（`Input_Frame`）

UI 交互的唯一输入是 `Engine/EngineRenderer/src/render/Input/输入快照.h` 的 `Input_Frame`（属于 Input 模块，此处只列本模块实际读取的字段与其语义，避免重复定义）：

| 字段 | 类型 | 本模块用法 |
| --- | --- | --- |
| `frame` | `uint64_t` | 不使用（仅门面观测）。 |
| `dt` | `double` | 不使用（UI 无动画时间推进）。 |
| `window_width` / `window_height` | `uint32_t` | 每帧写入根节点矩形，作为命中与绘制的坐标基准。 |
| `dpi_scale` | `float` | 不使用（UI 全部按物理像素布局）。 |
| `window_focused` | `bool` | 为假时不做命中（`hovered = 0`），因此点击也不会生效。 |
| `pointer.x` / `pointer.y` | `float` | 命中测试输入（窗口客户区像素，左上原点、Y 向下）。 |
| `pointer.button_down[0]` | `bool` | 刷新控件的“按下”视觉态。 |
| `pointer.button_pressed[0]` | `bool` | 左键按下沿：确定 `pressed` 与焦点。 |
| `pointer.button_released[0]` | `bool` | 左键抬起沿：同一目标上抬起即判定单击。 |
| `text.committed` | `std::string` | 焦点文本框插入的本帧 UTF-8 文本。 |
| `text.preedit` | `std::string` | **未使用**（输入法组合串不显示）。 |
| `text.active` | `bool` | **未读取**（由 `text_focus_active()` 反向驱动启停）。 |
| `key_pressed_check(Input_Key_Code)` | `bool` | 文本框编辑键：退格、删除、左移、右移（编码与 SDL 扫描码数值一致）。 |

`Ui_Rect::contains` 与命中测试使用的是“窗口像素”坐标，`Ui_Renderer::draw` 使用的是 UI 阶段颜色附件的 `VkExtent2D`；两者一致的前提是“UI 阶段恒绘制在交换链最终图像上”。

---

## 四、模块私有方法

### 4.1 `Text/core/字体加载.cpp`

文件内定义（`namespace engine`，非类成员）：

```cpp
struct Pending_Glyph
{
    uint32_t codepoint = 0;
    int width = 0;              // 位图宽 / 高
    int height = 0;
    int x_offset = 0;           // 位图相对笔位的偏移
    int y_offset = 0;
    unsigned char* bitmap = nullptr;  // 1 通道位图（调用方释放）
    uint32_t atlas_x = 0;       // 打包后填充
    uint32_t atlas_y = 0;
};

static unsigned char* glyph_bitmap_get(const stbtt_fontinfo& info, float scale, uint32_t codepoint,
    float& advance_out, int& width_out, int& height_out, int& x_offset_out, int& y_offset_out);
static void glyph_blit(Font_Record& record, uint32_t atlas_x, uint32_t atlas_y,
    int width, int height, const unsigned char* bitmap);
static void glyph_metric_set(Font_Record& record, Font_Glyph& glyph,
    uint32_t atlas_x, uint32_t atlas_y, uint32_t width, uint32_t height,
    int x_offset, int y_offset, float baseline);
static bool atlas_grow(Font_Record& record, Texture_Context& texture);
```

| 方法 | 职责与实现口径 |
| --- | --- |
| `glyph_bitmap_get` | 用 `stbtt_GetCodepointHMetrics` 取 `advance` 与左侧留白（后者未使用），`advance_out = advance × scale`；再用 `stbtt_GetCodepointBitmap(info, 0.0f, scale, ...)` 光栅化（`scale_x` 传 0 表示沿用 `scale_y`，所以签名里没有独立横向缩放）。空白/无位图返回空指针，但 `advance` 已写出。调用方负责 `stbtt_FreeBitmap`。 |
| `glyph_blit` | 把 1 通道覆盖率逐像素写进图集的 A 通道（`index = ((y+row) × atlas_width + (x+col)) × 4 + 3`）；覆盖率为 0 的像素直接跳过（图集初始 A 即为 0）。RGB 不动（初始化时恒 255）。 |
| `glyph_metric_set` | 由图集矩形与图集尺寸推出 `u/v`；`x0 = x_offset`、`y0 = y_offset + baseline`、`x1 = x0 + width`、`y1 = y0 + height`；同时记录 `atlas_x/y/w/h` 并置 `has_ink = true`。`u/v` 用“矩形不含 1 像素 padding”的精确边界，故采样不会渗到邻字。 |
| `atlas_grow` | 图集高度翻倍（并对齐到 4 的倍数）；新建 RGBA8 缓冲（RGB 全 255、A 全 0）；逐行 `memcpy` 旧内容（宽度不变，行偏移全部保留）；调用 `texture_create_from_memory` 重建纹理，**失败则不改动记录的任何一个字段**并返回假；成功则旧纹理压入 `retired_atlas_textures`、替换 `atlas_texture`、交换像素缓冲、更新 `atlas_height`，再对全部 `has_ink` 字形重算 `v0/v1`（`u` 不变）。日志：失败 `error`，成功 `info`。 |

`Font_Context` 的成员实现（在 `字体加载.cpp` 中，按调用链排列）：

| 成员 | 落点与要点 |
| --- | --- |
| `font_load_file` | 二进制 + `std::ios::ate` 取长度 → 整块读入 → `stbtt_GetFontOffsetForIndex(bytes, 0)`（TTC 取第 0 号）→ `stbtt_InitFont` → `stbtt_ScaleForPixelHeight` → `stbtt_GetFontVMetrics` 求 `ascent/descent/line_gap`，`baseline = floor(ascent × scale + 0.5)`（`descent/line_gap` 未参与）→ 逐 ASCII 码点光栅化并登记 `advance`（空白直接登记）→ 行式打包（步进 `width + 2`，换行时 `cursor_y += shelf_height + 1`）→ `ascii_height = cursor_y + shelf_height + 1`，`atlas_height = max(512, ascii_height × 4)` 且 4 对齐 → 构造白底透明图集 → 写 A 通道并补齐度量 → `texture_create_from_memory` → 优先复用首个空槽，否则追加；句柄 = 下标 + 1。 |
| `glyphs_ensure` | 前置校验 → `text_codepoints` 去重 → 收集 `glyphs.find` 未命中的码点 → 若空则直接返回真 → `font_bytes` 必须非空 → 重新 `stbtt_InitFont`（每次补烘独立，不保存 `stbtt_fontinfo`）→ 逐码点：`stbtt_FindGlyphIndex == 0` 即“字体不含”，`warn` + 计数 + 登记空度量；否则光栅化 → 空白登记 advance；过宽（`width + 2 > atlas_width`）跳过并计数；行尾不足则换行；高度不足则 `while (放不下) atlas_grow(...)`，扩容失败或仍放不下则跳过并计数；成功则 `glyph_blit` + `glyph_metric_set` + 推进游标 + 刷新 `shelf_height` → 只要有字形写入就重建整张图集纹理并退役旧纹理。 |
| `destroy / font_destroy / font_get / font_get_mutable / font_skipped_codepoints_get / font_count / build / valid / glyph_seek` | 均为薄封装：句柄越界或 `!valid` 一律返回空/0；销毁路径归还当前与全部退役图集后把记录整体赋值为 `Font_Record{}`（`alive` 语义由 `valid` 承担）。 |

### 4.2 `Text/core/文字几何.cpp`

```cpp
static bool utf8_decode(const std::string& text, size_t& offset, uint32_t& codepoint_out);
```

| 方法 | 职责与实现口径 |
| --- | --- |
| `utf8_decode` | 从 `offset` 解一个码点：ASCII 单字节；`110xxxxx` / `1110xxxx` / `11110xxx` 对应 1/2/3 个续字节；非法首字节（含以 `10xxxxxx` 开头）只 `++offset` 并返回假；后续字节数不足（截断）或任一续字节不是 `10xxxxxx` 也只跳过 1 字节。不校验过长编码、不校验代理区与 > U+10FFFF。成功时 `offset` 前进整段长度。 |
| `text_codepoints` | 循环 `utf8_decode`，用 `std::find` 做去重后收集；返回是否非空。 |
| `text_measure` | 笔位累加 `advance`；`has_ink && x1 > x0` 时把 `ink_right` 更新为“推进后的 `pen_x`”；无字形或右缘 ≤ 0 返回假；宽 = `uint32_t(ink_right + 0.5)`，高 = `pixel_height`。 |
| `text_layout_build` | 细节见 §六 6.3；产出顶点顺序“左上 → 右上 → 右下 → 左下”，索引 `(0,2,1)(0,3,2)`（在“像素空间 Y 向下”的约定下与既有管线的正面判定一致），每个可见字形 4 顶点 / 6 索引。 |
| `text_geometry_build` | 以 `Text_Layout{}` 默认布局转发 `text_layout_build`（单行、左对齐、不换行、行高取字体高度、行距 1）。 |

### 4.3 `Text/core/文字渲染.cpp`

```cpp
namespace { uint64_t layout_fingerprint_of(const Text_Layout& layout); }
void Text_Renderer::font_index_rebuild(void);   // 私有成员
```

| 方法 | 职责与实现口径 |
| --- | --- |
| `layout_fingerprint_of` | 把 `max_width`、`line_height`、`align`、`tracking`、`first_line_indent`、`line_spacing` 六个标量按位混合成 64 位（浮点经 `std::memcpy` 转 `uint32_t` 再混合）。仅作缓存查找的**快速排除**，命中后仍逐字段比较六个值以排除碰撞。 |
| `font_index_rebuild` | `font_index.clear()` 后按 `records` 下标重建“字体句柄 → 下标列表”的桶索引。`records` 任何增删（含淘汰）后都必须调用；查找时桶内下标还会做 `slot >= records.size()` 的防御性跳过。 |

其余成员按调用链排列：`init`（仅赋值）→ `color_set`（逐分量钳制）→ `draw_layout`（见 §七 7.1）→ `font_meshes_destroy`（移入 `retired` + 重建索引 + 立即 flush）→ `retired_flush` → `frame_begin`（帧时钟与延迟归还）→ `mesh_cache_capacity_set`（钳制 + 循环淘汰）→ `shutdown`（全量移入 `retired` 后 flush）。

### 4.4 `UI/core/控件树.cpp`

```cpp
static Ui_Handle node_create_impl(std::vector<Ui_Node>& nodes, Ui_Handle parent,
    const Ui_Rect& rect, Ui_Node_Type type);
```

| 方法 | 职责与实现口径 |
| --- | --- |
| `node_create_impl` | 树为空 → 返回 0；`parent == 0` 或父不存在/已销毁 → 挂到根节点（1）；优先复用**第一个** `alive == false` 的空槽（整条赋值，`handle = 下标 + 1`），否则追加新槽（`handle = size + 1`）。初始背景色 `(0,0,0,0)`、文本色白、其余默认。 |
| `Ui_Context` 构造 / `build` / `shutdown` / `root_get` | 见 §三 3.10；根节点固定为槽位 0、句柄 1。 |
| `node_find`（两个重载） | 句柄 0、越界、`!alive` → `nullptr`；否则返回槽位地址。 |
| `node_release` | 置 `alive = false`，清 `hovered/pressed/focused/clicked`，并把三个全局交互句柄中等于该句柄者清 0。不回收内存、不清理父子引用（父句柄字段保留）。 |
| `destroy` | 根节点不可销毁；不存在的句柄返回假；否则按“多轮：每轮只回收 `parent == handle` 的节点，直到一轮无变化”方式级联（只按直接父子关系逐层传播），最后 `node_release(handle)`。 |
| `panel_create / button_create / text_box_create` | 调 `node_create_impl` 后按类型写入颜色与文本字段；创建失败（返回 0）时 `node_find(0)` 为 `nullptr`，静默返回 0。 |
| 属性读写族 | 统一“先 `node_find`，空则返回假”，不做类型校验。 |

### 4.5 `UI/core/交互.cpp`

```cpp
static std::size_t utf8_prev(const std::string& text, std::size_t pos);   // 前一码点起点
static std::size_t utf8_next(const std::string& text, std::size_t pos);   // 下一码点终点

// Ui_Context 私有成员
Ui_Handle hit_test(float px, float py) const;
void      focus_set(Ui_Handle handle);
void      text_edit(Ui_Node& node, const Input_Frame& input);
```

| 方法 | 职责与实现口径 |
| --- | --- |
| `hit_test` | 从 `nodes.size()` 到 1 逆序遍历：跳过 `!alive`、`!visible`、`!enabled`、`Root`；`rect.contains(px, py)` 命中即返回该句柄；无命中返回 0。**不检查父链可见性、不做裁剪、不区分控件类型**（因此 `Panel` 也会“吃掉”命中，但因为 `frame_begin` 只对 Button/Text_Box 处理按下，面板实际只起到“挡住下层控件”的作用）。 |
| `focus_set` | 与当前焦点相同则空操作；清旧节点 `focused`、写 `focused` 句柄、置新节点 `focused = true`。`handle == 0` 即清除焦点。 |
| `text_edit` | 光标先钳制到 `text.size()`；插入 `input.text.committed`（`insert(caret, committed)` 后 `caret += committed.size()`）；`SDL_SCANCODE_BACKSPACE` 用 `utf8_prev` 求起点并删除 `[begin, caret)`；`SDL_SCANCODE_DELETE` 用 `utf8_next` 求终点并删除 `[caret, end)`；`SDL_SCANCODE_LEFT/RIGHT` 按码点移动；末尾再钳制一次。全部用**按下沿**判定（长按重复不生效）。 |
| `utf8_prev` | `pos == 0` 返回 0；否则从 `pos − 1` 向前跳过 `10xxxxxx` 续字节，停在首字节位置。不校验首字节合法性。 |
| `utf8_next` | `pos >= size` 返回 `size`；否则从 `pos + 1` 向后跳过续字节。 |
| `frame_begin` | 见 §七 7.3；顺序固定：根矩形 → 清点击 → 校验焦点 → 命中刷新悬停 → 按下沿 → 抬起沿 → 按下态 → 文本编辑。 |

### 4.6 `UI/core/UI渲染器.cpp`

```cpp
struct Ui_Renderer::Slot_Buffers { VkBuffer vertex_buffer; VmaAllocation vertex_allocation;
    void* vertex_mapped; uint32_t vertex_capacity; VkBuffer index_buffer; VmaAllocation index_allocation;
    void* index_mapped; uint32_t index_capacity; uint64_t bound_atlas; };
struct Ui_Renderer::Draw_Record { uint32_t first_index; uint32_t index_count; Ui_Color color; bool text; };

bool Ui_Renderer::slot_ensure(Slot_Buffers& slot, uint32_t vertex_count, uint32_t index_count);
void Ui_Renderer::quad_append(const Ui_Rect& rect, const Ui_Color& color);
bool Ui_Renderer::text_append(uint64_t font_handle, const std::string& text, float pixel_x,
    float pixel_y, float center_area_width, const Ui_Color& color);
void Ui_Renderer::text_atlas_mark(uint64_t font_handle);   // 仅有声明，快照内无定义、无调用
```

| 方法 | 职责与实现口径 |
| --- | --- |
| `slot_ensure` | 容量足够（顶点与索引**两者**都够）则空操作；否则先销毁该槽现有的两个缓冲并整体复位（`bound_atlas` 归零），再按 `目标顶点 = 需要 × 2 + 256`、`目标索引 = 需要 × 2 + 512` 创建主机可见 + 一致内存的顶点/索引缓冲，并各自持久映射；任一步失败都回滚已建资源并返回假。容量只增不减。 |
| `quad_append` | 追加 4 个顶点（左上 → 右上 → 右下 → 左下，z = 0，uv 恒 0）与 6 个索引（`base+0,1,2` / `base+0,2,3`），登记一条 `Draw_Record{ first_index = base, index_count = 6, text = false }`。绕序与文字几何相反（文字用 `0,2,1 / 0,3,2`），因 UI 管线固定 `cull_none = true`，两者都可见。 |
| `text_append` | 字体句柄为 0 / 字体模块空 / 文本空 / 记录无效 → 返回假；否则 `glyphs_ensure`（返回值**未检查**）→ 清空复用 `text_scratch` → `text_geometry_build`（单行）→ `text_measure` 取宽高 → 若 `center_area_width > 0` 则按 `(center_area_width − width) × 0.5` 平移 → 顶点逐个加 `offset_x/pixel_y` 并置 z=0，索引整体加 `base` → 登记 `Draw_Record{ text = true, color = 控件文本色 }` → 记录 `text_atlas = record->atlas_texture`。 |
| `text_atlas_mark` | 头文件声明为“结束一笔文本绘制（记录当前字形图集句柄）”，但**快照内没有定义**，也没有任何调用点；实际“记录图集”是在 `text_append` 末尾直接赋值 `text_atlas`。【待确认：3】 |
| `Ui_Renderer::draw` | 见 §七 7.3 与 §六 6.8；要点：按槽位顺序遍历、按类型分发、纯色与文字分记录、槽缓冲一次上传、矩阵与颜色各推一次、按记录切换管线。 |

---

## 五、模块内部架构

### 5.1 子文件分工

```
render/Text/
├── 文字布局数据.h        ← 纯数据：Text_Align / Text_Layout（Vulkan-free）
├── 字体.h                ← Font_Glyph / Font_Record / Font_Context（Vulkan-free，引用 Texture_Context）
├── 局部命名空间使用.h     ← using std::string / std::vector（并引入字体.h）
└── core/
    ├── 字体加载.cpp       ← Font_Context 实现；**全层唯一** STB_TRUETYPE_IMPLEMENTATION
    ├── 文字几何.cpp       ← UTF-8 解码 / 度量 / 切行 / 几何
    └── 文字渲染.cpp       ← Text_Renderer 实现（缓存、延迟归还、绘制项登记）

render/UI/
├── 用户界面.h            ← Ui_Node / Ui_Context（Vulkan-free、SDL-free，引用 Input_Frame）
├── UI渲染器.h            ← Ui_Renderer（层内实现头，含 Vulkan/VMA 与四个模块头）
├── 局部命名空间使用.h     ← using std::string / std::vector（并引入用户界面.h）
└── core/
    ├── 控件树.cpp         ← Ui_Context：节点的增删查改、槽位复用、级联销毁
    ├── 交互.cpp           ← Ui_Context：命中 / 焦点 / 点击 / 文本框编辑（私有包含 SDL3）
    └── UI渲染器.cpp       ← Ui_Renderer：顶点索引生成、帧槽缓冲、管线与描述符、录制
```

分工的主线是“**数据与设备解耦**”：`字体.h`、`文字布局数据.h`、`用户界面.h` 不出现任何 Vulkan 类型，故度量/布局/控件树可以在没有设备的情况下单独使用（也便于测试）；真正接触 GPU 的只有 `字体加载.cpp`（经 `Texture_Context`）、`UI渲染器.cpp`（直接调用 Vulkan）与门面注入的回调。

### 5.2 所有权与引用关系

```
Renderer::Impl
├── Font_Context  font         ← 拥有字体记录与图集纹理句柄；纹理实际归 Texture_Context
├── Text_Renderer text         ← 仅持有 &font 与 Text_Host（不拥有字体、不拥有网格）
│     └── records[] ── Mesh_Handle ──→ Mesh_Context（门面持有）
│     └── retired[] ── 待归还网格
├── Ui_Context    ui           ← 拥有控件树（纯主机侧数据）
├── Ui_Renderer   ui_render    ← 持有每帧槽 VkBuffer/VmaAllocation（经 Memory_Context 创建）
│     ├── &memory / &pipeline / &texture / &font（均不拥有）
│     └── cpu_vertices / cpu_indices / draws / text_scratch / text_atlas（每帧临时）
└── ui_solid_pipeline / ui_text_pipeline  ← 句柄由门面惰性登记并记录
```

生命周期：`Font_Context` 随 `Impl` 构造、在 `Impl` 中止时 `destroy()`；`Text_Renderer` 与 `Ui_Renderer` 必须**先于**其依赖模块销毁（门面 shutdown 顺序为 `ui_render.shutdown()` → `ui.shutdown()` → `text.shutdown()` → `mesh.destroy()` → `font.destroy()` → `texture.destroy()`）。线程归属：全部在渲染线程（主线程）上串行访问，无锁、无原子量。

### 5.3 文字数据流

```
TTF 字节
  │  std::ifstream(binary, ate) 整块读入
  ▼
stbtt_InitFont（font_offset = GetFontOffsetForIndex(..., 0)）
  │  ScaleForPixelHeight(pixel_height) / GetFontVMetrics → baseline
  ▼
ASCII 32..126 光栅化（GetCodepointHMetrics → advance；GetCodepointBitmap → 1 通道位图）
  │  行式打包（宽 512，步进 width+2，行距 +1，shelf_height 取行内最高）
  ▼
atlas_pixels（RGBA8，RGB=255，A=覆盖率）  ──texture_create_from_memory──→  atlas_texture
  │                                                                    │
  │  惰性补烘（首次绘制遇到新码点）                                        │ 旧纹理 → retired_atlas_textures
  ▼                                                                    │
Font_Record.glyphs（码点 → Font_Glyph{uv, 像素矩形, advance, atlas 矩形, has_ink}）
  │
  │  glyphs_ensure（补烘 / 扩容 / 重建纹理）
  ▼
text_layout_build(text, layout)
  │  utf8_decode → glyph_seek → 切行 → 行宽 → 对齐 → 行顶 = 行号 × line_height × line_spacing
  ▼
Mesh_Data{vertices(TL,TR,BR,BL), indices(0,2,1)(0,3,2)}   ← 像素坐标（原点=文本左上，Y 向下）
  │  host.mesh_create
  ▼
Mesh_Handle（缓存在 Text_Renderer.records，键 = 字体句柄 + 文本 + 布局指纹/六项）
  │  host.mesh_draw(mesh, 像素→NDC 矩阵, 颜色)
  ▼
Renderer::pending_items（一个绘制项，pipeline = 文字管线）
  │  frame_render 场景 pass 逐项消费
  ▼
文字.frag 采样 set1 字形图集（A 通道作覆盖率，offset 64 颜色、offset 80 描边）
```

### 5.4 UI 数据流

```
Input_Aggregator.event_feed（平台事件泵桥接）
  │  input.frame_advance(0.0, 窗口宽, 窗口高, dpi)
  ▼
Input_Frame（本帧快照）
  │  ui.frame_begin(input)
  ├─ Root.rect = 窗口客户区
  ├─ hit_test（槽位逆序）→ hovered
  ├─ 按下/抬起沿 → pressed / focused / clicked / on_click
  └─ text_edit（焦点 Text_Box：committed 插入 + 退格/删除/左右）
  │
  ▼
Ui_Context.nodes[]（创建顺序 = 槽位顺序 = 绘制顺序）
  │  ui_render.draw(ui, cb, extent, slot)
  ├─ 遍历 node_at(0..count)（跳过空槽 / 不可见 / Root）
  ├─ Panel → quad_append
  ├─ Button → quad_append（悬停 +0.12、按下 ×0.72）+ text_append（垂直居中、水平居中）
  ├─ Text_Box → quad_append + text_append（左对齐 + padding 6）+ 光标 quad
  ├─ text_append：glyphs_ensure → text_geometry_build（单行）→ text_measure（居中）
  ▼
cpu_vertices / cpu_indices / draws（每条记录：索引区间 + 颜色 + 是否文字）
  │  slot_ensure（2× + 256/512，主机可见 + 一致内存，持久映射）
  ▼
slot.vertex_buffer / slot.index_buffer（memcpy 上传）
  │  vkCmdBindVertexBuffers / vkCmdBindIndexBuffer
  ▼
push constant{ mat4 像素→NDC, vec4 颜色 }（offset 0 / 64）
  │  逐记录：切纯色或文字管线（文字还需 set1 = UI 专用集）
  ▼
vkCmdDrawIndexed（UI 阶段：仅颜色附件、LOAD 保留、无深度）
```

### 5.5 模块间交互关系（只列与本模块直接接壤的）

| 对接方 | 接口形态 | 方向 |
| --- | --- | --- |
| `Renderer` 门面 | `Text_Host` 七个回调、`font_load_file/font_unload/text_draw/text_draw_layout/text_measure/text_color_set/text_outline_set/font_skipped_codepoints/text_cache_*`、`ui_context()` | 门面 → 本模块（转发/注入）；本模块 → 门面（回调） |
| `Texture_Context` | `texture_create_from_memory`、`texture_destroy`、`texture_bind_get` | 本模块调用，纹理归纹理模块 |
| `Mesh_Context` | `mesh_create`、`mesh_destroy`、`mesh_record_get`、`mesh_bounds_get` | 经门面回调，句柄由网格模块持有 |
| `Pipeline_Context` | `pipeline_by_handle_get`、`layout_get`、`ui_set_get`、`ui_texture_bind`、`pipeline_activate` | UI 渲染器直接调用；文字经门面回调 |
| `Memory_Context` | `buffer_create`、`buffer_map`、`buffer_destroy` | UI 每帧槽缓冲 |
| `Input` 模块 | `Input_Frame`（只读）、`Input_Aggregator.text_input_enable` | UI 消费；门面驱动文本输入启停 |
| `Frame_Context` | `ui_pass_record(...)` 内的 `Frame_Ui_Callback` | 门面在 UI 阶段回调 `Impl::ui_draw_entry` |
| 外部 UI（ImGui） | `Ui_Channel_Context::draw_callback_get` | 门面在原生 UI 之后调用，覆盖其上 |

---

## 六、模块设计 / 实现细节

### 6.1 中文字形与 UTF-8 文本处理

1. **ASCII 预烘 + 惰性补烘的两段式**。`font_load_file` 只烘焙 32..126；其余码点（含全部 CJK）在第一次被某段文本用到时由 `glyphs_ensure` 现场光栅化并追加。这避免了“加载一个中文字体就烘几千个字”的启动代价，但把代价转移到“首次出现该字的绘制帧”。头文件对此有明确约定（“惰性字形”），实现与之一致。
2. **图集余量的定量依据**。初始高度取 `max(512, ascii 占用 × 4)` 并 4 对齐；源码注释说明“× 4 是把首轮容量抬到约 500 行像素，常规展示场景一次即可容纳全部中文”，即余量是按“几十个中文标签”的经验量估的，不是按字体全部码点计算。余量耗尽时走 `atlas_grow` 翻倍，而不是直接丢弃。
3. **解码器的容错口径**。`utf8_decode` 对“非法首字节 / 续字节格式不符 / 尾部截断”一律“只跳过 1 字节”，不中断整串；这保证一段文本中偶发坏字节不会导致整段文字消失，但也意味着坏字节会被当作一个“不存在的码点”参与后续流程（`text_codepoints` 里它不会进入去重表，几何阶段 `glyph_seek` 找不到就跳过且不推进笔位）。解码器**不**校验过长编码（如 3 字节表示的 ASCII）、不校验代理区、不校验 > U+10FFFF，这些序列会得到一个码点后交给 stb 判“字体不含”。
4. **缺字处理**。`glyphs_ensure` 先 `stbtt_FindGlyphIndex == 0` 判定“字体真的不含该码点”，此时 `logger.warn`、`skipped_codepoints` 自增，并往 `glyphs` 写一个全零 `Font_Glyph`。好处是“同一缺字只尝试一次、也只计数一次”；代价是缺字在排版上宽度为 0（不会留出占位方块），一串缺字会直接塌陷。门面提供 `font_skipped_codepoints` 作为诊断出口。
5. **“空白字符”与“无墨迹”是两件事**。空格没有位图但 `advance` 有效，登记为 `has_ink == false`；几何阶段只推进笔位、不产四边形。`text_measure` 的宽度因此**不含尾随空白**（它用“最后一个有墨迹字形”的笔位）。这与“行宽 = advance 之和”（布局用）不是同一口径：布局的换行与对齐会把尾随空白算进去。
6. **字节偏移 vs 码点**。UI 的 `caret` 是字节偏移；插入 `input.text.committed` 后 `caret += committed.size()`（按字节推进，正确）；退格/删除/左右移都先经 `utf8_prev/utf8_next` 找码点边界再操作字节区间。文本框不做任何字符过滤：若 `committed` 中出现 `'\n'`，它会原样进入 `text`，而绘制是“单行几何 + 单行高度排布”，此时几何阶段会断行、控件只按第一行高度垂直居中，属未定义表现。
7. **输入法**。`Input_Frame::text.preedit` 完全未被读取，`text_edit` 只消费 `committed`，所以组合中的拼音串不会显示，光标也不会随组合串移动。这是明确的实现边界，不是遗漏说明。

### 6.2 字形图集管理

1. **单张图集、固定宽度 512**。宽度固定是刻意选择：`u` 分量永远不需要因扩容而重算，只有 `v` 需要；这让“扩容后旧几何仍然可采样”成为一个只与高度有关的问题。代价是“超宽字形无法进入图集”：ASCII 预烘阶段遇到 `width + 2 > 512` 会让**整次加载失败**，而惰性补烘阶段遇到同样情况只是**跳过该码点并计数**——两条路径策略不一致（见 §六 6.10）。
2. **行式（shelf）打包**。游标 `cursor_x` 向右推进 `width + 2`（右侧留 2 像素；源码注释写“左右各留 1 像素”，实现上首字无左留白、相邻字之间共 2 像素，效果等价于“防渗色”），行高 `shelf_height` 取本行最高字形（按位图高度，不含偏移），换行时 `cursor_y += shelf_height + 1`。竖直方向的 1 像素是行间间隔。所有 uv 都按“精确矩形边界”计算，不把 padding 纳入，因此采样不会读进邻字。
3. **扩容的原子性**。`atlas_grow` 先建新缓冲、拷贝旧内容、再创建纹理；**纹理创建失败时立即返回假且不修改记录的任何字段**（旧像素、旧高度、旧 uv 全部保持）。成功后旧纹理进 `retired_atlas_textures`（而不是立刻销毁），再重算全部 `has_ink` 字形的 `v0/v1`。`glyphs_ensure` 用 `while` 循环反复扩容直到放得下，避免“一次翻倍仍不够”时直接跳过。
4. **每次补烘都重建整张纹理**。`glyphs_ensure` 只要本次写入过任意字形像素（`atlas_changed`），就把**整张** `atlas_pixels` 重新 `texture_create_from_memory`（新建纹理对象，而不是写旧纹理）。这意味着“首次出现 20 个中文”可能产生 20 次全图上传、20 个旧纹理句柄压入退役列表。退役纹理只在 `font_destroy` / `destroy` 时统一归还，期间一直占用显存。这是本模块最主要的显存与带宽代价来源，也解释了源码里“旧纹理不可立即销毁，可能仍被在途帧引用”的注释。
5. **扩容与缓存的不一致**。【待确认：4】扩容会改 `Font_Glyph` 的 `v`，但 `Text_Renderer` 里已经缓存的网格顶点 uv 是“旧高度下的 uv”，缓存条目没有任何失效标记（缓存键里也不含 `atlas_height`）。因此一旦发生扩容，此前缓存的文字网格会在采样上错位（横向不受影响，纵向会取到错误的图集行），直到该条目被 LRU 淘汰或字体卸载。`Ui_Renderer` 同帧的情况更直接：先 `text_append` 的控件顶点用旧 uv、后面的控件触发扩容后 `text_atlas` 换成新纹理，同一帧内两段文字对新旧高度各说各话。
6. **图集纹理的“当前绑定”语义**。文字绘制走门面 `texture_bind_set`：它会把目标纹理写入**所有帧槽**的 set1，并在写入前 `frames_wait()` 等待全部在途帧完成。UI 走自己的 UI 专用集（`pipeline.ui_texture_bind(slot, sampler, view)`），只写当前槽、不等待在途帧。

### 6.3 换行 / 对齐 / 行距 / 缩进 / 字距的精确口径

`text_layout_build` 分两阶段，全部由 `文字几何.cpp` 实现：

**阶段一：切行**。

- 逐码点 `utf8_decode`；`'\n'` 立刻收束当前行（空行也保留，形成竖直间距），行宽记为当前 `line_advance`，再开新行。
- `glyph_seek` 找不到度量（未烘焙/不支持）→ 直接跳过，**不推进笔位**，也不计入本行。
- 换行判据：`max_width > 0 && !当前行.empty() && line_advance + advance + tracking > max_width`。三个细节：(1) `max_width == 0` 永不自动换行；(2) 当前行为空时不换行，因此“单个字形本身超过 `max_width`”会独占一行而不是无限空转；(3) `tracking` 计入判据。
- 每行的 `line_widths` 是 `advance + tracking` 的累加和（不是墨迹右缘）。
- 循环结束后再 `push_back` 最后一次 `line_advance`，保证 `lines.size() == line_widths.size()`。

**阶段二：逐行产出**。

- `line_height = layout.line_height > 0 ? layout.line_height : pixel_height`；`line_spacing` 只在 `(0, 8]` 内取用，否则按 1。
- `line_top = 行号 × line_height × line_spacing`：行距倍数只拉纵向步进。
- 起始笔位 `pen_x`：只有 `max_width > 0` 时对齐才有基准；`Center` 为 `(max_width − 行宽) × 0.5`，`Right` 为 `max_width − 行宽`，结果**负值钳到 0**（保证超宽行不向左溢出）。
- 首行缩进：`行号 == 0` 时 `pen_x += first_line_indent`；缩进不参与行宽，也不参与换行判据，故右对齐/居中时首行整体右移、可能超出 `max_width`。
- 逐字形：仅 `has_ink && x1 > x0 && y1 > y0` 才追加四边形；顶点基址 `base = vertices.size()`；四角位置为 `(pen_x + x0, line_top + y0)` 等，uv 取字形 `u/v`；随后 `pen_x += advance + tracking`（布局推进含字距，与阶段一的判据一致）。
- 顶点顺序 `TL → TR → BR → BL`，索引 `(0,2,1)` 与 `(0,3,2)`。源码注释明确这是为了在“像素空间 Y 向下”下与既有管线的正面判定一致。
- 最后若 `any_ink == false`（空串、全空白、全是不支持码点），清空顶点与索引并返回假——“失败不留半成品几何”。

**与度量口径的差异**：`text_measure` 不含 `tracking`、不切行；UI 的文字绘制使用的是“单行 + `text_geometry_build`（默认布局，tracking=0）”，所以 UI 内度量与几何自洽；而文字子系统若 `tracking != 0`，`text_measure` 与 `text_draw_layout` 的行宽会不同，调用方不应拿度量结果去反推布局盒。

### 6.4 文字网格缓存与延迟归还

1. **缓存键**：`(font 句柄, text, layout 六项)`。查找只在 `font_index[font]` 桶内进行（桶内保存 `records` 下标），避免每次绘制遍历全部字体；条目里额外保存 `layout_fingerprint`，查找时“先比指纹、再逐字段确认”，兼顾速度与碰撞安全。`text` 与 `layout` 都按值保存，因此缓存条目持有完整文本副本。
2. **LRU 与容量**。`use_clock` 单调递增，命中时刷新 `last_used`；插入前若 `records.size() >= mesh_cache_max`（默认 64，可配 `[1,1024]`），先淘汰 `last_used` 最小者；缩容时循环淘汰直到不超上限。淘汰只做“移入 `retired` + 删记录 + 重建桶索引”，不立即调 `mesh_destroy`。
3. **延迟归还的两级时钟**。`retired_tick` 记录“退役发生的帧号”，`frame_begin(overlap)` 每帧 `++frame_clock`，归还条件为 `retired_tick + overlap < frame_clock`。也就是说，淘汰发生在第 N 帧，则在第 N + overlap + 1 帧开始前才真正销毁，保证 overlap 个在途帧都不会再引用它。`overlap` 取配置的在途帧数（最小 1）。
4. **两处绕过延迟的路径**。【待确认：5】`font_meshes_destroy` 在把条目移入 `retired` 之后**立即** `retired_flush()`，注释理由是“该字体已随调用方销毁，待销毁项此刻可直接归还”；`Font_Context::font_destroy` / `destroy` 同样立即 `texture_destroy` 当前与退役图集。若在途帧仍引用这些网格或纹理（门面 shutdown 前通常已 `frame` 等待，但运行期调用 `font_unload` 时不一定），就构成“销毁在途资源”。是否安全取决于调用时机（门面 `font_unload` 不在帧内被调用即安全，源码未强制）。
5. **统计口径**。`cache_hits/misses` 自 `init` 起累计，`shutdown` 不复位；`mesh_cache_size` 只读 `records.size()`（不含 `retired`）。缓存命中不跳过后续的管线绑定与纹理绑定，命中只省掉“几何生成 + 网格创建”这两步。
6. **缓存的“文本内容”成本**。每条记录都保存完整 `std::string`（UI 侧的控件文本也另存一份），对长文本或大量短文本都会放大内存；`records` 与 `retired` 用同一 `Record` 结构，退役项也带着文本与布局副本，直到 flush。

### 6.5 UI 控件树的遍历顺序与槽位语义

1. **槽位即顺序**。`nodes` 是 `std::vector<Ui_Node>`，句柄恒为 `下标 + 1`。绘制按 `0 → count−1` 正序遍历，命中按 `count−1 → 0` 逆序遍历。因为根节点在槽 0、所有控件都在根之后创建，所以“父先于子”在**创建顺序**上自然成立；但这不是树序遍历——若先创建“面板 A → 按钮 A1”，再创建“面板 B → 按钮 B1”，顺序是 A、A1、B、B1，`B` 会盖住 `A1`（即使 `A1` 在层级上更深）。换言之：**绘制层级 = 创建顺序，不是父子深度**。
2. **槽位复用**。`node_create_impl` 优先复用第一个 `alive == false` 的空槽（整条覆盖），因此句柄会在“销毁后重建”时被复用；`node_count()` 不减少（数组永不收缩），`node_at()` 对空槽返回 `nullptr`。句柄没有代际计数，所以“旧句柄”可能指向新控件（ABA）。
3. **父子关系的唯一用途**。`parent` 只在 `destroy` 的级联回收里使用；它不参与坐标（`rect` 是绝对像素）、不参与可见性继承、不参与命中过滤、不参与绘制裁剪。父节点被隐藏时子节点照常绘制与命中；父节点被销毁时子节点随级联一起释放。
4. **可见性/可用性的实际语义**。`visible == false` → 不绘制、不命中；`enabled == false` → 仍然绘制（原色）、不命中、若正持焦点会被清焦点。三个标志（`hovered/pressed/focused`）每帧由 `frame_begin` 重算，但只在 Button/Text_Box 上置真（`pressed` 在释放沿之后自然归零）。
5. **根节点**。句柄固定 1，`frame_begin` 每帧把它铺满窗口客户区，`draw` 时跳过不绘制；它的背景色只在 `build` 时写入 `(0.05,0.05,0.10,1)`，没有任何绘制路径会用到，属于“预留但未使用”的字段。
6. **“层”概念缺失**。没有 z-order、没有兄弟置顶、没有“置前/置后”接口；要改变遮挡只能销毁重建（从而改变槽位）或干脆调整创建顺序（重建整棵 UI）。这是保留模式设计没有提供的表达能力。

### 6.6 输入命中与“模态”

1. **命中只做矩形相交**。`hit_test` 用逆序第一个矩形相交的节点；`Ui_Rect::contains` 是半开区间，相邻矩形不会重复命中。不做像素级命中（圆角/异形按钮按外接矩形算）。
2. **没有模态**。不存在“模态层 / 捕获组 / 输入拦截”结构：所有节点在同一个平面里，谁在逆序里更靠前谁先被命中；面板（Panel）虽然不参与按下逻辑，但它会挡住其下层控件（因为命中先返回面板句柄，而 `frame_begin` 对非 Button/Text_Box 的命中只做“清焦点”）。如果希望实现“弹窗遮罩”，现有机制只能靠“创建一整块大面板覆盖全屏”来间接实现，且它不会阻止键盘——键盘文本永远只进焦点文本框。
3. **焦点是单例且只属于 Text_Box**。`focused` 是唯一的 `Ui_Handle`；按下 Text_Box 即获焦，按下 Button 或空白即清焦，焦点节点被隐藏/禁用/销毁/类型变化时自动清焦。`focused_get` 对任何节点都返回其 `focused` 字段，但实际只有 Text_Box 会被置真。
4. **点击判定需要“按下与抬起在同一目标”**。按下沿记录 `pressed = hovered`（仅当命中 Button/Text_Box），抬起沿要求 `pressed == hovered` 且类型为 Button 才置 `clicked` 并调用 `on_click`。拖出按钮再松手不会触发；`clicked` 标记保留到本帧结束（下一帧 `frame_begin` 统一清零）或被 `clicked_consume` 取走。
5. **窗口失焦即不命中**。`window_focused == false` 时 `hovered = 0`，按下沿虽然仍会进入分支，但 `node_find(0)` 为空因而清焦点、不置 `pressed`。这避免了“Alt+Tab 回来后误触发点击”。
6. **回调时机与重入**。`on_click` 在 `frame_begin` 的“抬起沿”分支内同步调用，参数是 `node->handle` 与 `node->user`。回调里若创建/销毁控件，`nodes` 可能重新分配或槽位内容变化——实现里回调之后不再使用该节点引用，但“遍历所有节点刷新 pressed 状态”的循环在回调之后才执行，因此回调中删除节点是安全的；回调中把当前节点删除也不会崩（后续只写 `pressed = 0`）。回调中再次触发 `frame_begin` 则属于未定义行为（会重入状态机）。
7. **文本输入的启停**。门面在 `ui.frame_begin` 之后调用 `input.text_input_enable(ui.text_focus_active())`，所以“是否有文本框获得焦点”直接影响平台是否接收文本事件。这是本模块与输入模块之间唯一的“反向控制”点。

### 6.7 UI 文本的绘制口径

1. **不复用 `Text_Renderer`**。`Ui_Renderer::text_append` 直接调用 `Font_Context::glyphs_ensure` + `text_geometry_build` + `text_measure`，把几何并入 UI 的动态顶点/索引缓冲；既不查也不写 `Text_Renderer` 的网格缓存。原因：UI 每帧顶点本来就要整体重写，单独为 UI 文本维护网格句柄再逐条 `mesh_draw` 反而会引入每帧 N 个绘制项与 N 次管线/纹理切换。
2. **代价**。每个带文本的控件每帧都做一次“解码 + 度量 + 几何生成 + 顶点拷贝”，文本越长越贵；若文本首次出现新码点，还会触发一次整张图集纹理重建（见 6.2）。注释与实现都没有做“UI 文本几何缓存”，这是有意的取舍（UI 文本量通常很小）。
3. **只支持单行、两种水平排布**。按钮标签：`pixel_x = rect.x`、`center_area_width = rect.w` → 在矩形宽度内水平居中；文本框：`pixel_x = rect.x + 6`、`center_area_width = 0` → 左对齐。垂直方向统一 `rect.y + (rect.h − text_height) × 0.5`，其中 `text_height` 取字体 `pixel_height`（不是实测行高），因此字体度量与实际墨迹高度差异会体现为上下 1~2 像素的偏移。
4. **居中使用 `text_measure` 的宽度**。该宽度不含尾随空白、不含 tracking（UI 不设 tracking，故无影响），且对“全空白标签”返回假（宽度 0）。若标签以空格结尾，居中会略微偏右。
5. **光标**。仅焦点文本框绘制：`caret_width` 由 `text_measure(text.substr(0, caret))` 得到（按字节截断，若截断点落在多字节字符中间，会得到“半个字符”的文本；实际 `caret` 由编辑逻辑保证落在码点边界，但 `text_set` 之外的外部写入不保证）；光标矩形 `x = text_x + caret_width`、`y = rect.y + 4`、`w = 2`、`h = rect.h − 8`，颜色纯白。光标不闪烁（无时间驱动）。
6. **UI 文字着色**。`UI文字.frag` 用 `coverage = texture(...).a`，输出 `vec4(color.rgb, color.a × coverage)`；不支持描边（描边只在 `文字.frag` 的 offset 80 路径里）。文本框/按钮的文本色都来自 `Ui_Node::text_color`（默认白）。

### 6.8 与帧上下文的录制接入点

1. **交互接入点**：`Renderer::frame_render` 在事件泵之后、任何 GPU 工作之前，先 `input.frame_advance(0.0, window_size, dpi)` 生成 `Input_Frame`，再 `impl->ui.frame_begin(impl->input.frame_get())`，随后 `input.text_input_enable(ui.text_focus_active())`。也就是说，上层在本帧 `frame_render` 之前创建/修改的控件，参与**本帧**的命中；在 `frame_render` 之后修改的，等下一帧。
2. **文字帧时钟接入点**：`impl->text.frame_begin(impl->config.frame_overlap)` 在 `frame_render` 的场景数据组装阶段调用（在“推进帧时钟、归还已淘汰网格”的位置），早于场景 UBO 组装与绘制项消费。文字绘制项本身是**上层在 frame_render 之前**调用 `text_draw_layout` 登记的（延迟路径），所以“本帧登记、本帧渲染”。
3. **UI 录制接入点**：帧录制的后处理链在“末级后处理结束”之后，若存在 `ui_callback`，按交换链范围设置视口/裁剪并调用 `Frame_Context::ui_pass_record(...)`：图像屏障（同布局 COLOR_ATTACHMENT_OPTIMAL）→ 动态渲染（仅颜色附件、`LOAD_OP_LOAD`、`STORE_OP_STORE`、无深度附件）→ `vkCmdBeginRendering` → 设置视口/裁剪 → `ui_callback->fn(...)` → `vkCmdEndRendering`。门面传入的回调是 `Renderer::Impl::ui_draw_entry`。
4. **同一个回调里的两层 UI**：`ui_draw_entry` 先 `ui_render.draw(ui, cb, swapchain.extent, frame.slot_index_get())`（原生 UI），再取 `ui_channel.draw_callback_get()` 调外部 UI（ImGui 等）。因此**外部 UI 永远画在原生 UI 之上**；原生 UI 不参与任何后处理级，也不会被 bloom/色调影响（它在末级之后）。
5. **为什么 UI 必须在最后一级之后**。UI 阶段的目标是交换链图像本身（`extent` 取交换链尺寸），而且 `loadOp = LOAD` 保留了后处理结果；若把 UI 放进后处理链之前，UI 会被色调/泛光/SSAA 影响且坐标会与最终分辨率不匹配。
6. **帧槽并行保护**：`draw` 用 `slot = frame.slot_index_get()` 取当前帧槽，顶点/索引缓冲与 UI 描述符集都按槽隔离；`bound_atlas` 也是每槽缓存（避免逐帧重写描述符）。UI 缓冲的写入是“整块 memcpy 到持久映射内存”，不做显式 flush（依赖 `HOST_COHERENT`）。
7. **文字与 UI 的阶段差异**。文字绘制项登记进 `pending_items`，由**场景 pass** 消费：因此文字会进入深度测试（文字管线是否写/测深度由管线登记参数决定，门面用默认 `pipeline_load` 路径）、会参与实例矩阵、会被视锥剔除判断【待确认：7】、`cast_shadow` 被显式置假、`transparent` 为真（Alpha 混合档）从而不参与静态合批。UI 则在颜色附件 LOAD 保留的独立阶段录制，不受上述任何机制影响。两条路径的“坐标原点都是左上、Y 向下”，但矩阵约定不同（见 6.9）。

### 6.9 像素坐标到 NDC 的两种矩阵约定

**文字路径**（`文字渲染.cpp`）：

```
transform[0]  =  2 / 屏宽
transform[5]  =  2 / 屏高          // 不翻转
transform[10] =  1
transform[12] =  2 × pixel_x / 屏宽 − 1
transform[13] =  2 × pixel_y / 屏高 − 1
transform[15] =  1
```

源码注释给出的理由：Vulkan 视口把 NDC 的 y = −1 映射到帧缓冲第 0 行，所以“像素行号 = 帧缓冲行号”，不需要翻转。屏宽/屏高来自宿主 `extent_get`（门面转交换链尺寸）。矩阵经 `mesh_draw` 进入实例缓冲（模型矩阵），片元颜色经推送常量 offset 64。

**UI 路径**（`UI渲染器.cpp`）：

```
push.transform[0]  =  2 / width
push.transform[5]  = -2 / height       // 翻转
push.transform[10] =  1
push.transform[12] = -1
push.transform[13] =  1
push.transform[15] =  1
```

【待确认：1】两条路径对同一个“左上原点、Y 向下”的像素坐标给出了**相反**的纵向映射：文字是 `y_ndc = 2/h × py + (2 × pixel_y / h − 1)`，UI 是 `y_ndc = −2/h × py + 1`。在标准（正高度）视口下，前者的像素 0 落在顶部，后者落在底部。UI 阶段的视口高度确认为正（`命令录制.cpp` 中 `ui_viewport.height = swapchain_extent.height`），所以二者之一与“Y 向下”的约定不符，需实机确认（详见文末）。

`文字.vert` 的头注释写“该矩阵含文本位置偏移与 Y 轴翻转”，与实现（不翻转）不一致；按规范“以实现为准”，此处记录为注释陈旧，不单列待确认。

### 6.10 错误处理与降级策略

| 场景 | 实现行为 | 影响 |
| --- | --- | --- |
| 文字宿主回调不全 | `draw_layout` 整次拒绝并 `logger.error` | 不补烘字形、不建网格，避免半程副作用 |
| 文字管线登记失败 | `pipeline_ensure` 返回 0 → `draw_layout` 拒绝 | 整帧所有文字不绘制（不是“退化为纯色”） |
| 文字几何无可见字形 | `text_layout_build` 返回假 → `draw_layout` 报错返回假 | 空串走“返回真、不登记”的快速路径；全空白/全缺字则被当作失败 |
| 网格创建失败 | 报错返回假；缓存不登记 | 下一帧会重试（可能持续失败并持续报错） |
| 目标尺寸非法 | 报错返回假（避免除零） | 窗口最小化等 0 尺寸场景 |
| 字形过宽（加载阶段） | 整次 `font_load_file` 失败并释放已光栅化位图 | 一个字形的宽度就能否决整个字体；而补烘阶段同样情况只“跳过 + 计数” |
| 字体不含码点 / 图集不足 | `warn` + `skipped_codepoints` 自增 + 登记空度量 | 文本继续绘制，缺字宽度为 0 |
| 图集扩容纹理重建失败 | `atlas_grow` 返回假、记录不变；`glyphs_ensure` 最终返回假 | 本帧文字不绘制；已烘焙字形仍可用 |
| UI 构建前置不全 | `logger.error` 返回假 | 门面只 `warn`“原生 UI 不绘制”，不影响既有渲染 |
| UI 槽缓冲创建/映射失败 | 就地回滚已建资源并返回假；`draw` 静默返回 | 本帧 UI 缺失，下一帧重试 |
| UI 两条管线都未登记 | `draw` 在取管线后返回 | 原生 UI 不绘制，外部 UI 不受影响 |
| 节点句柄无效 | 所有属性/查询接口返回假或空 | 静默，无日志；UI 调试需自行判返回值 |

统一的“静默降级”原则：UI 侧的任何失败都不会阻断帧渲染（`draw` 是 `void`，只跳过本次绘制）；文字侧失败会向上返回 `bool`，但仍不影响其他绘制项。

### 6.11 性能取舍汇总

1. **文字几何按“文本串”缓存**：一次布局 + 一次网格上传换取后续每帧零 CPU 几何成本；代价是缓存条目持有完整文本副本、LRU 淘汰带来延迟销毁队列。
2. **字形惰性光栅化**：启动快、首次出现新字慢；补烘一次就整图重建是最大带宽开销，适合“文本种类稳定”的场景（HUD、菜单）。
3. **每帧一次 `texture_bind` 的隐性代价**：`draw_layout` 无论缓存是否命中都会 `host.texture_bind`，而门面实现会 `frames_wait()` 并写满所有帧槽 set1。多段文字逐次绘制时，等于“每段文字一次 GPU 等待”。（是否值得优化为“图集句柄未变则跳过”是门面的职责，本模块无法自行判断。）
4. **UI 顶点/索引整体重写**：每帧把所有控件三角化并 memcpy 上传，简单、无需脏标记；代价是 1000 个控件级别的 UI 会有可观的 CPU 拷贝。槽缓冲按“2× + 常量”增长且不收缩，峰值过后仍占显存。
5. **UI 文本无缓存**（见 6.7），文字子系统有缓存（见 6.4）：两套策略服务于两类负载，源码中未统一。
6. **描述符改写被缓存**：UI 侧用 `Slot_Buffers::bound_atlas` 避免逐帧重写；文字侧没有等价缓存（每次都写满全槽）。
7. **命中测试 O(n) 逆序**：控件数量线性；没有空间索引。对菜单级 UI 足够。
8. **无裁剪 / 无滚动**：省掉 scissor 与裁剪栈，代价是长列表只能靠重建节点。

---

## 七、模块运行链路详例

### 7.1 链路一：一段中文文本从布局到上屏

**调用方**：上层在 `frame_render` 之前调用 `Renderer::text_draw_layout(font, u8"设置", x, y, layout)`（或 `text_draw`，它等价于 `Text_Layout{}` 默认布局）。

1. `Renderer::text_draw_layout` 置 `impl->in_text_draw = true`（标记“本绘制项不参与阴影投影”），转发 `impl->text.draw_layout(...)`，返回后复位。
2. `Text_Renderer::draw_layout` 先做**宿主完整性检查**：`font != nullptr` 且 `pipeline_ensure / pipeline_bind / texture_bind / mesh_create / mesh_destroy / mesh_draw / extent_get` 七个回调全部非空；否则 `logger.error` 返回假。
3. `font->font_get_mutable(font_handle)` 取可写记录；句柄无效报错返回假；`text.empty()` 直接返回真（不登记绘制项）。
4. `font->glyphs_ensure(*record, text)`：`text_codepoints` 解码去重 → 收集未烘焙码点（首次的“设”与“置”在此列）→ 重新 `stbtt_InitFont` → 逐码点 `FindGlyphIndex`/`GetCodepointHMetrics`/`GetCodepointBitmap` → 行式打包 + 高度不足时 `atlas_grow` → `glyph_blit` + `glyph_metric_set` → 重建整张图集纹理并把旧纹理压入 `retired_atlas_textures`。返回真后，这两个码点在 `glyphs` 中已有 uv、像素矩形与 advance。
5. `host.pipeline_ensure(host.user)`：门面检查 `impl->text.pipeline_get()`，未登记则 `pipeline_load("文字.vert.spv", "文字.frag.spv", Blend_Mode::Alpha, config.text_alpha_to_coverage)`，成功后 `pipeline_set(handle)` 并返回；本模块把返回值写回 `pipeline`。登记失败 → 报错返回假。
6. **缓存查找**：从 `font_index[font_handle]` 取候选下标列表；逐个要求 `item.font == font_handle && item.text == text`，再要求 `layout_fingerprint` 与当前布局一致，最后逐字段核对 `max_width / line_height / align / tracking / first_line_indent / line_spacing`。命中 → 取 `item.mesh`、刷新 `last_used`、`++cache_hits`。
7. **未命中**：`++cache_misses` → `Font_Context::text_layout_build(data, *record, text, layout)`（见 §六 6.3：`'\n'` 与 `max_width` 切行、行宽 = advance + tracking、`line_top` 按 `line_spacing` 拉伸、对齐与首行缩进、产出 4 顶点/6 索引）→ `host.mesh_create`（门面 `Mesh_Context::mesh_create`：一次上传顶点+索引到设备本地缓冲并计算像素空间包围盒）→ 若 `records.size() >= mesh_cache_max`，选出 `last_used` 最小者移入 `retired`（`retired_tick = 当前帧`）、重建桶索引 → 压入新记录并把新下标追加到 `font_index[font_handle]`。
8. `host.pipeline_bind(host.user, text_pipeline)`：门面 `pipeline_activate`，后续 `mesh_draw` 的绘制项会记录该管线；`host.texture_bind(host.user, record->atlas_texture)`：门面 `texture_bind_set` 等待全部在途帧后把该图集写入所有帧槽 set1，并更新“当前绑定视图/采样器”快照。
9. `host.extent_get(host.user, w, h)`：门面取交换链宽高；若任一 ≤ 0，报错返回假。
10. 组装**像素→NDC** 矩阵（见 §六 6.9）：`2/w`、`2/h`、平移 `2 × pixel_x / w − 1`、`2 × pixel_y / h − 1`。
11. `host.mesh_draw(host.user, mesh, transform, text_color)`：门面 `mesh_draw` → 实例矩阵追加（transform 作为模型矩阵）→ 组装 `Frame_Render_Item{pipeline = 当前激活管线, instance_count = 1, cast_shadow = false, transparent = true}` → 推送常量前 64 字节占位单位矩阵、offset 64 写颜色、offset 80 写文字描边参数。返回真表示已登记（或被视锥剔除而合法跳过）。
12. `frame_render` 的场景 pass 逐项消费该绘制项：`vkCmdDrawIndexed` → `文字.vert` 用实例矩阵把像素坐标变到 NDC → `文字.frag` 采样 set1 的 `glyph_atlas`，`coverage = texture.a`，`out_color = vec4(color.rgb, color.a × coverage)`（描边开启时另加两圈八方向采样）。

**结果/副作用**：首次出现的中文字形被写进图集并新建/替换图集纹理；本帧多一个文字绘制项；`cache_misses` 自增；若发生扩容，所有已烘焙字形的 `v` 被重算，而既有缓存网格的 uv 不会同步（见 6.2 第 5 点）。

### 7.2 链路二：字体加载与字形缓存（含扩容）

**入口 A：加载字体**（上层 `Renderer::font_load_file`）。

1. 门面校验 `impl != nullptr && initialized`，转 `impl->font.font_load_file(handle_out, path, pixel_height)`（出参先置 0）。
2. `Font_Context::font_load_file`：`valid()`（纹理模块已记录）与 `pixel_height != 0` 校验 → `std::ifstream(path, binary | ate)` 取长度 → 为空/读取失败/打不开则报错 → `seekg(0)` 整块读入 `bytes`。
3. `stbtt_GetFontOffsetForIndex(bytes.data(), 0)`（TTC 多字体取第 0 号）→ `stbtt_InitFont`；失败报错。
4. `scale = stbtt_ScaleForPixelHeight(info, pixel_height)`；`stbtt_GetFontVMetrics` 取 `ascent/descent/line_gap`；`baseline = floor(ascent × scale + 0.5)`。
5. 对码点 32..126：`glyph_bitmap_get` 写 `advance` 并返回 1 通道位图；空白（无位图/零尺寸）直接登记 `Font_Glyph{advance}`；`width + 2 > 512` 立即释放全部位图并整体失败。
6. 行式打包：放不下则 `cursor_x = 0; cursor_y += shelf_height + 1; shelf_height = 0`；定位后 `cursor_x += width + 2`，刷新 `shelf_height`。
7. `ascii_height = cursor_y + shelf_height + 1`；`atlas_height = max(512, ascii_height × 4)` 且 4 对齐。构造 `pixels`（长度 `512 × H × 4`，RGB = 255、A = 0）。
8. 把 `pixel_height / baseline / atlas 尺寸 / pixels / 游标 / shelf_height / font_bytes` 写入局部 `record`；逐位图 `glyph_blit` + `glyph_metric_set`（`y0 = y_offset + baseline`）；释放全部 `stbtt` 位图。
9. `texture->texture_create_from_memory(atlas_texture, pixels, 512, H)`；失败报错（不留下记录）。
10. 分配句柄：优先第一个 `!records[i].valid` 的槽（`handle = i + 1`），否则追加（`handle = size`）；`record.valid = true`；`handle_out = handle`；`logger.info` 打印路径、字号、图集尺寸与句柄。

**入口 B：首次绘制新码点（惰性补烘 + 扩容）**。

1. `draw_layout` → `glyphs_ensure(record, text)`；`record.valid` 与纹理模块非空后 `text_codepoints` 去重。
2. 对每个码点查 `record.glyphs`；全部已存在 → 直接返回真（不改纹理）。
3. 缺 `font_bytes` → 报错；否则重新 `stbtt_InitFont` + `ScaleForPixelHeight(record.pixel_height)`。
4. 逐缺失码点：`FindGlyphIndex == 0` → `warn` + 计数 + 空度量；否则光栅化 → 空白登记 advance → 过宽跳过并计数 → 行尾不足换行 → `while (cursor_y + height + 1 > atlas_height)` 调 `atlas_grow`：
   - `atlas_grow`：新高度 = 旧 × 2 且 4 对齐；新缓冲 RGB 白/A 0；逐行拷贝旧内容；`texture_create_from_memory` 失败则**不改记录**返回假；成功则旧纹理入 `retired_atlas_textures`、替换句柄、交换像素、改 `atlas_height`、重算全部 `has_ink` 字形的 `v0/v1`。
   - 扩容失败或仍放不下 → `warn` + 计数 + 释放位图 + 登记空度量 + 继续下一个码点。
5. 定位（`atlas_x = cursor_x, atlas_y = cursor_y`）→ `glyph_blit` → `glyph_metric_set`（用 `record.baseline`）→ 登记 → `cursor_x += width + 2`、刷新 `shelf_height` → 释放位图 → `atlas_changed = true`。
6. 若 `atlas_changed`：`texture_create_from_memory` 重建整张纹理；成功则把旧句柄压入退役列表并替换；失败报错返回假。
7. `logger.info` 输出“现存字形数 + 图集尺寸”。

**入口 C：卸载字体**（`Renderer::font_unload`）：先 `impl->text.font_meshes_destroy(handle)`（该字体的全部文字网格移入 `retired`，重建桶索引，**立即** flush 调 `mesh_destroy`），再 `impl->font.font_destroy(handle)`（归还当前与全部退役图集纹理，记录整体复位）。顺序保证“网格先于字体度量/纹理失效”。

**入口 D：每帧回收**：`frame_render` → `impl->text.frame_begin(config.frame_overlap)`：`frame_overlap = max(1, overlap)`、`++frame_clock`，逐项判断 `retired_tick + frame_overlap < frame_clock` 则 `mesh_destroy` 并移除。

### 7.3 链路三：UI 控件树的构建、交互、渲染

**构建（上层，任意帧）**。`Renderer::ui_context()` 返回 `&impl->ui`（未初始化返回空）。上层调用：

- `panel_create(root, {x, y, w, h}, color)`：`node_create_impl` 选槽（首个空槽或追加），写入类型、矩形、背景色。
- `button_create(root, rect, label, on_click, user, color)`：同上并写入标签、回调、用户数据、背景色。
- `text_box_create(root, rect, u8"默认文本")`：写入文本、`caret = text.size()`、深色背景与白字。
- `font_set(font_handle)` 指定 UI 字体；`rect_set / label_set / text_set / visible_set / enabled_set` 等后续修改。

节点顺序 = 创建顺序，句柄 = 槽位 + 1；`destroy` 会级联回收全部后代（不支持销毁根）。

**交互（每帧 `frame_render` 开头）**。`Renderer::frame_render` → `platform.events_pump()`（事件桥接把 SDL 事件喂给 `input.event_feed` 并转交外部 UI 后端）→ `input.frame_advance(0.0, 窗口宽, 窗口高, dpi)` → `ui.frame_begin(input.frame_get())`：

1. 根节点 `rect = {0, 0, window_width, window_height}`。
2. 全体节点 `clicked = false`（上一帧的单击标记清零）。
3. 若 `focused != 0`：`node_find(focused)` 为空、类型不是 `Text_Box`、或不可见/不可用 → `focus_set(0)`。
4. `hovered = window_focused ? hit_test(pointer.x, pointer.y) : 0`；随后对每个节点置 `hovered = alive && handle == hovered && (Button || Text_Box)`。
5. 左键按下沿：`pressed = 0`；取 `hovered` 节点，若是 Button/Text_Box → `pressed = 该句柄`、`focus_set(Text_Box ? 该句柄 : 0)`；否则（空白/面板/无命中）→ `focus_set(0)`。
6. 左键抬起沿：取 `pressed` 节点，若存在且 `pressed == hovered` 且为 Button → `clicked = true`、若有 `on_click` 则同步调用 `on_click(handle, user)`；随后 `pressed = 0`。
7. 刷新按下态：`pressed = alive && handle == pressed && pointer.button_down[0]`。
8. 焦点文本框：`text_edit(*focus_node, input)` —— 先钳 `caret`，插入 `committed`（`caret += committed.size()`），退格删前一码点、删除删后一码点、左右移一个码点，末尾再钳制。
9. 返回门面后 `input.text_input_enable(ui.text_focus_active())`：焦点是可见可用的 Text_Box 时开启平台文本输入，否则关闭。

**渲染提交（帧录制末级后处理之后）**。`ui_pass_record` → `vkCmdBeginRendering`（仅颜色附件、`LOAD_OP_LOAD`、无深度）→ 设视口/裁剪为交换链范围 → `Impl::ui_draw_entry`：

1. `ui_render.draw(ui, command_buffer, swapchain.extent, frame.slot_index_get())`：
   - `!valid()` / 命令缓冲空 / 槽越界 / `extent` 任一为 0 → 返回。
   - 清空 `cpu_vertices / cpu_indices / draws / text_atlas`；取 `font_handle = ui.font_get()`、`font_record = font->font_get(font_handle)`（不取时 `text_height = 0`）。
   - 按 `0..node_count()` 取 `node_at(index)`；`nullptr` 或 `!visible` 跳过；`Root` 不绘制。
   - `Panel` → `quad_append(rect, color)`。
   - `Button` → 悬停时 RGB 各 `+0.12`（上限 1）、按下时 RGB `×0.72`（按下优先）→ `quad_append`；标签非空 → `text_append(font_handle, label, rect.x, rect.y + (rect.h − text_height) × 0.5, rect.w, text_color)`（水平居中）。
   - `Text_Box` → `quad_append`；`text_append(font_handle, text, rect.x + 6, rect.y + (rect.h − text_height) × 0.5, 0, text_color)`；若 `focused` 且有字体记录：`text_measure(substr(0, caret))` 取 `caret_width`，追加光标矩形 `{text_x + caret_width, rect.y + 4, 2, rect.h − 8}`（白）。
   - `text_append` 内部：`glyphs_ensure`（可能触发补烘/扩容）→ `text_geometry_build`（单行）→ `text_measure` 取宽 → 居中偏移或左对齐 → 顶点加偏移、索引加 `base` → 登记 `Draw_Record{text = true}` → `text_atlas = 当前图集句柄`。
   - `cpu_indices` 为空 → 返回（不产生任何命令）。
   - `slot_ensure(slots[slot], 顶点数, 索引数)`：容量足够则复用；否则销毁旧缓冲、按 `2× + 256 / 2× + 512` 重建并持久映射。失败 → 返回。
   - `memcpy` 顶点与索引到映射内存。
   - 取 `solid_vk / text_vk / layout`；两者都空 → 返回。
   - `vkCmdBindVertexBuffers` + `vkCmdBindIndexBuffer`（`VK_INDEX_TYPE_UINT32`）。
   - 组装推送常量：矩阵（`2/w`、`−2/h`、平移 `(−1, +1)`）+ 颜色（逐记录写入）。
   - 逐条 `Draw_Record`：文字记录 → 无文字管线/`text_atlas == 0`/纹理模块空则跳过；`buffers.bound_atlas != text_atlas` 时 `texture->texture_bind_get` 取视图/采样器 → `pipeline->ui_texture_bind(slot, sampler, view)` → 更新 `bound_atlas`；取 `pipeline->ui_set_get(slot)`，为空则跳过；首次切到文字时 `vkCmdBindPipeline(text_vk)` + `vkCmdBindDescriptorSets(set = 1)`。纯色记录 → 无纯色管线则跳过；首次切到纯色时绑定 `solid_vk`。最后 `vkCmdPushConstants(offset 0, 80 字节)` + `vkCmdDrawIndexed(index_count, 1, first_index, 0, 0)`【待确认：2】。
2. `vkCmdEndRendering`。
3. 回到 `ui_draw_entry`，取 `ui_channel.draw_callback_get()` 调外部 UI 回调（ImGui 在此绘制，覆盖在原生 UI 之上）。

**结果/副作用**：控件状态（hovered/pressed/focused/clicked）与文本框内容被本帧输入更新；原生 UI 的顶点/索引写入当前帧槽缓冲；命令缓冲中新增若干纯色/文字 `vkCmdDrawIndexed`；UI 不进入后处理，也不影响深度。

---

## 附：待确认正文

以下条目对应正文中的【待确认：N】。仅为核对线索，不代表已确认的缺陷；需结合实机或后续版本判定。

**【待确认：1】UI 与文字的纵向映射方向相反。**

- 文字：`Engine/EngineRenderer/src/render/Text/core/文字渲染.cpp` 中 `transform[5] = 2.0f / screen_height`、`transform[13] = 2.0f * pixel_y / screen_height - 1.0f`，注释明确“不做 Y 翻转即可让像素行号等于帧缓冲行号”。
- UI：`Engine/EngineRenderer/src/render/UI/core/UI渲染器.cpp` 中 `push.transform[5] = -2.0f / height`、`push.transform[13] = 1.0f`，同一套“左上原点、Y 向下”的矩形因此会被送到 NDC 的相反纵向位置。
- 视口确认：`Engine/EngineRenderer/src/render/Frame/core/命令录制.cpp` 的 `ui_pass_record` 之前，`ui_viewport.height = static_cast<float>(swapchain_extent.height)`（正值），未使用负高度翻转技巧。
- 结论待定：按标准 Vulkan 视口，文字应显示正确、UI 疑似上下镜像；若 UI 实际显示正常，则说明文字一侧的假设或某处（投影矩阵、`文字.vert` 的实例矩阵来源）另有翻转，需实机截图确认。附带：`Engine/EngineRenderer/assets/shaders/文字.vert` 注释称矩阵“含 Y 轴翻转”，与实现不一致（注释陈旧）。

**【待确认：2】UI 绘制记录的 `first_index` 口径疑似错位。**

- `ui_quad_append` / `text_append` 都用 `const uint32_t base = static_cast<uint32_t>(cpu_vertices.size())` 作为 `Draw_Record::first_index`，并把 `base + i` 写入 `cpu_indices`。
- `vkCmdDrawIndexed(command_buffer, record.index_count, 1, record.first_index, 0, 0)` 的第四个参数是“索引起始下标”，不是“顶点基址”。由于每个四边形是 4 顶点 / 6 索引，第二条记录起 `first_index = base` 与真实索引区间（`6 × 记录序号`）不等，可能读到上一条记录的索引。
- 首条记录（`base == 0`）恰好正确，因此单控件 UI 不会暴露；多控件（面板 + 按钮 + 文本框）时可能出现三角形错乱。需实机验证；若确认，应改为“追加前记录 `cpu_indices.size()` 作为起始索引下标”。

**【待确认：3】`Ui_Renderer::text_atlas_mark` 只有声明。**

- `Engine/EngineRenderer/src/render/UI/UI渲染器.h` 第 91 行声明 `void text_atlas_mark(uint64_t font_handle);`，`core/UI渲染器.cpp` 中没有定义，全仓库检索也没有调用点。
- 当前不影响运行（未被引用则不会产生未定义符号需求）；但该声明与“结束一笔文本绘制（记录当前字形图集句柄）”的注释，与实际在 `text_append` 末尾直接赋 `text_atlas` 的做法不符，属声明/注释陈旧。

**【待确认：4】图集扩容后已缓存几何的 uv 未失效。**

- `atlas_grow`（`core/字体加载.cpp`）在扩容成功后重算全部 `has_ink` 字形的 `v0/v1`；`Text_Renderer` 的缓存键只含“字体句柄 + 文本 + 布局六项”，不含 `atlas_height`，且没有“图集版本”字段。
- 已缓存的 `Mesh_Handle` 顶点里保存的是扩容前的 `v`，扩容后再绘制同一文本会命中缓存并直接复用旧几何 → 采样到错误的图集行。
- 同帧内 UI 侧也有类似时序：先 `text_append` 的控件（旧高度 uv）与后 `text_append` 触发扩容后统一使用的 `text_atlas`（新纹理）不一致。
- 触发条件：补烘过程中图集高度不足并成功扩容。修复方向应是“记录图集版本、扩容时清空/标记文字网格缓存”（需产品决策）。

**【待确认：5】字体卸载路径绕过延迟归还。**

- `Text_Renderer::font_meshes_destroy` 把条目移入 `retired` 后**立即** `retired_flush()`；`Font_Context::font_destroy` / `destroy` 立即 `texture_destroy` 当前与退役图集纹理。两处注释都承认“网格/旧纹理可能仍被在途帧引用”，但选择“此刻可直接归还”。
- 与 `frame_begin` 的延迟策略（`retired_tick + overlap < frame_clock`）不一致。运行期（非 shutdown）调用 `font_unload` 时若存在在途帧，可能销毁仍被命令缓冲引用的资源。需确认调用约束（是否只允许在帧间、且已等待帧完成时卸载）。

**【待确认：6】`text_measure` 的“右缘”口径与注释不一致。**

- `字体.h` 注释：宽度按字形 `advance` 累加，“不含尾随空白”，语义偏向“末个有墨迹字形的右缘”。
- `core/文字几何.cpp` 实现：`pen_x += glyph->advance; if (has_ink && x1 > x0) ink_right = pen_x;`，取的是“推进后的笔位”，不是字形墨迹右缘 `x1`。两者相差 `x0 + (advance − 位图宽)` 量级，通常为字形的左右留白。UI 的按钮标签居中与文本框光标位置都以此为依据，因此居中/光标位置会有 1~3 像素的系统性偏移（取决于字体）。属“注释与实现不一致”，需确认哪个是期望口径。

**【待确认：7】文字网格参与实例级视锥剔除的坐标系不匹配。**

- `Renderer::mesh_draw_range`（门面）在 `impl->cull_enabled`（默认 true）且网格有包围盒时执行 `frustum_cull_aabb(view_proj, model, bounds)`，其中 `mvp = view_proj × model`。
- 文字网格的顶点是“像素坐标”，传入的 `model` 已经是“像素→NDC”的仿射矩阵；再乘相机 `view_proj` 相当于把 NDC 坐标又当作世界坐标变换一次，包围盒极可能被判为完全在视锥外。
- `mesh_draw_range` 没有针对 `in_text_draw` 的剔除豁免；`cast_shadow` 有豁免，剔除没有。若实机文字正常显示，说明该测试恰好通过（或包围盒/矩阵使其未被判外）；若文字偶发消失，这是首要怀疑点。UI 不受影响（UI 不经过 `mesh_draw`）。
