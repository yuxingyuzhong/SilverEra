# Hi-Z 模块（HiZ）

> 版次：`间接绘制_1394c1a+1f69992`（第 253 轮新增、第 254 轮按生成实现修订）
> 源码：`Application/Renderer/src/render/HiZ/`（`HiZ.h` / `局部命名空间使用.h` / `core/HiZ.cpp`）
> 着色器：`Application/Renderer/assets/shaders/HiZ深度拷贝.comp`、`HiZ降采样.comp`

## 目录

1. [模块功能介绍](#1-模块功能介绍)
2. [模块公开接口](#2-模块公开接口)
3. [模块私有方法](#3-模块私有方法)
4. [模块内部架构](#4-模块内部架构)
5. [模块设计 / 实现细节](#5-模块设计--实现细节)
6. [模块运行链路详例](#6-模块运行链路详例)
7. [待确认](#7-待确认)

## 1. 模块功能介绍

按**帧槽**持有 Hi-Z 遮挡剔除所需资源，并录制其内容生成：

- **深度历史**（`R32_SFLOAT` 颜色面）：上帧场景单采样深度经 compute 采样写入；
- **mip 金字塔**（`R32_SFLOAT`，完整 mip 链 ＋ 逐级视图）：逐级取 2×2 最大深度（最远遮挡面，保守）。

生成由两条计算着色器完成：`HiZ深度拷贝.comp`（深度 → 历史）、`HiZ降采样.comp`（历史 → 第 0 级、级 i-1 → 级 i）。模块不做遮挡判定（255 轮）与开关决策（门面负责）。

深度来源核查（方向一规划 §2.4）：交换链深度按交换链图像轮转、离屏深度归渲染目标，均非每在途帧一份，故自建每帧槽历史。

## 2. 模块公开接口

| 接口 | 语义 |
| --- | --- |
| `constexpr HIZ_LEVEL_MAX = 12` / `HIZ_PYRAMID_FORMAT = VK_FORMAT_R32_SFLOAT` | 级数上限与统一浮点格式 |
| `bool build(VkDevice, Memory_Context&, frame_overlap, width, height, VkFormat depth_format)` | 建立采样器 ＋ 每槽历史（`STORAGE | SAMPLED | TRANSFER_DST`）＋ 金字塔（`STORAGE | SAMPLED`）＋ 逐级视图 |
| `void destroy()` / `bool valid()` | 幂等销毁（含采样器）/ 是否就绪 |
| `bool extent_set(width, height, depth_format)` | 尺寸 / 格式变化时重建全部槽；一致时空操作 |
| `void compute_set(Compute_Context*)` | 关联计算上下文（生成时写描述符集与调度）；传空解除 |
| `bool pyramid_generate(cb, slot, depth_image, depth_view, depth_copy_pipeline, downsample_pipeline)` | 录制「深度 → 历史 → 逐级金字塔」并恢复深度布局；失败返回假且不录制 |
| `uint32_t width_get() / height_get() / level_count() / slot_count()` | 尺寸 / 级数 / 槽数 |
| `bool level_extent_get(level, w, h)` | 第 level 级尺寸 = `(max(1, w >> level), max(1, h >> level))` |
| `VkImage depth_history_image(slot)` / `VkImageView depth_history_view(slot)` | 该槽历史（颜色面视图） |
| `VkImage pyramid_image(slot)` / `VkImageView pyramid_level_view(slot, level)` | 金字塔与逐级视图（`baseMipLevel = level`） |
| `bool history_valid(slot)` / `void history_invalidate(slot)` | 该槽金字塔是否已生成（重建 / 尺寸变化后为假） |
| `uint64_t generation_count()` | 累计生成次数 |

## 3. 模块私有方法

| 方法 | 职责 |
| --- | --- |
| `level_count_from(width, height)`（文件内静态） | 逐级折半至 1×1，上限 `HIZ_LEVEL_MAX` |
| `image_barrier_record(...)`（文件内静态） | 单图像布局转换（同步 2 口径；可指定 mip 级范围） |
| `group_count(elements)`（文件内静态） | 按本地尺寸 8×8 向上取整 |
| `slot_create / slot_destroy / slots_destroy` | 单槽 / 全部槽的资源创建与归还（内存模块已失效时只清句柄） |

## 4. 模块内部架构

```
HiZ/
├── HiZ.h                 ← 对外头（类声明 / 常量 / Slot）
├── 局部命名空间使用.h      ← 实现文件的类型引入
└── core/HiZ.cpp          ← 构建 / 重建 / 销毁 / 生成录制
```

- `Slot`：`history_image / allocation / view`、`pyramid_image / allocation`、`level_views`（长度 = 级数）、`history_valid`。
- 上下文：`device`、`memory*`、`compute*`、`depth_sampler`、`depth_format`、`width / height / levels`、`slots`、`generation_total`。
- 依赖：内存模块（图像 / 视图 / 采样器生命周期）与计算模块（帧槽描述符集 ＋ 调度）；不依赖帧 / 门面。

## 5. 模块设计 / 实现细节

- **历史为颜色面**：深度格式不能作存储图像，且 `vkCmdCopyImage` 要求同格式；改为 compute「采样深度 → 写 R32」一步解决（第 253 轮的 `history_copy_record` 未被调用即被取代）。
- **保守性**：每级取 2×2 **最大**深度（最远遮挡面）；深度采样器最近邻 ＋ 钳边 ＋ 点 mip，禁止线性过滤把边界深度拉近。
- **第 0 级**：仍为全分辨率；用「同尺寸降采样」调用（2×2 邻域全部钳到同纹素）实现逐纹素拷贝，无需额外着色器。
- **布局**：历史 / 金字塔全程 `GENERAL`（存储读写与采样都合法）；场景深度仅在生成期间往返 `SHADER_READ_ONLY ↔ DEPTH_ATTACHMENT`。
- **绑定**：计算 set0 的 binding 4 = 深度采样器、5 / 6 = 存储图像（源 / 目标级）；生成期间按级重写（仅在开启时发生）。
- **屏障**：源深度写 → 计算读；历史 / 各级 `UNDEFINED → GENERAL`；每级 dispatch 之间 `ComputeToCompute`；结束时深度转回深度附件。
- **降级**：生成管线句柄缺失、尺寸与资源不一致、槽越界、计算关联缺失时返回假（门面仅告警，不影响出帧）；MSAA 与离屏 / 链路径尚未挂接（260 / 256 轮）。

## 6. 模块运行链路详例

1. `hiz_enable_set(true)` → `hiz_ensure()`（构建资源 ＋ 关联计算）→ 懒加载 `HiZ深度拷贝.comp` / `HiZ降采样.comp` 计算管线。
2. 帧渲染：场景动态渲染通道结束 → 帧模块调用 `Frame_Post_Scene_Callback`（仅单采样）。
3. `Renderer::Impl::hiz_post_scene_entry` → `hiz.pyramid_generate(命令缓冲, 当前槽, 深度, 深度视图, 两条管线)`：
   深度转可读 → 历史 `GENERAL` → 深度拷贝 dispatch（采样 → 存储写）→ 屏障 →
   逐级循环（目标级 `UNDEFINED → GENERAL`、绑源 / 目标存储图像、降采样 dispatch、屏障）→ 深度复习回。
4. 生成完成：`history_valid[slot] = true`、`generation_count++`；金字塔保持 `GENERAL` 供 255 / 256 轮读取。

## 7. 待确认

- 【待确认：254-1】`HIZ_LEVEL_MAX` 截断后末级仍大于 1×1（> 4096 窗口），遮挡查询的 clamp 口径待 255 轮定稿。
- 【待确认：254-2】每级重写描述符集的成本未计时；若成为热点，272 轮改为预建逐级描述符集。
- 【待确认：254-3】MSAA 与离屏 / 后处理链路径的生成接入（260 / 256 轮）尚未验证。

## 增补（第二百五十五轮：遮挡测试 CPU 参考）

新增 Vulkan-free 头 `src/render/HiZ/遮挡测试.h` 与实现 `core/遮挡测试.cpp`（不依赖 Vulkan / 内存 / 计算模块）：

| 接口 | 语义 |
| --- | --- |
| `HIZ_OCCLUSION_LEVEL_MAX = 12` / `HIZ_OCCLUSION_BIAS = 0.002f` | 级数上限与判定容差 |
| `struct HiZ_Pyramid_View { data; width; height; levels }` | 逐级紧排的金字塔视图 |
| `struct HiZ_Screen_Rect { min_x/min_y/max_x/max_y; nearest_depth; cross_near }` | 屏幕矩形、对象最近深度、跨近平面标志 |
| `enum class HiZ_Visibility { Visible, Occluded }` | 判定结果 |
| `hiz_screen_rect_build(rect, view_proj, aabb_min, aabb_max, width, height)` | 8 角点投影；任一角点 `w ≤ 0` 置 `cross_near`；非法入参返回假 |
| `hiz_level_for_rect(rect_w, rect_h, levels)` | `ceil(log2(max))` 截到 `[0, levels-1]` |
| `hiz_pyramid_level_get(pyramid, level, data, w, h)` | 逐级紧排偏移换算 |
| `hiz_occlusion_test(pyramid, rect, bias)` | 跨近平面 / 视图外 → 可见；覆盖纹素取最大，最近深度 > 最大 ＋ bias → 剔除 |
| `hiz_pyramid_downsample(src, sw, sh, dst, dw, dh)` | 2×2 取最大、越界钳到末行 / 列（与 `HiZ降采样.comp` 同口径） |

口径：深度 0..1（越小越近），金字塔存最远面；同尺寸降采样并非逐纹素恒等（邻域为 `(2x,2x+1)×(2y,2y+1)`，偏保守）。256 轮着色器按同一口径实现，并以「CPU 与 GPU 遮挡结果一致」用例验收。

## 增补（第二百五十六轮：遮挡接入剔除）

- 宽高覆盖口径：两个生成着色器改用 `imageSize()` 取范围，不再依赖推送常量（避免推送 / 绑定错位导致静默截断）。
- 新增 `pyramid_full_view(slot)`（完整 mip 链视图）与 `depth_sampler_get()`：供剔除着色器绑定为 `sampler2D`，按级 `texelFetch`。
- 关键前提：交换链深度图像必须带 `VK_IMAGE_USAGE_SAMPLED_BIT`（此前缺失，采样恒为 0，已于本轮回补）。
- 接入方式：`实例压缩.comp` 视锥判定后做遮挡测试（与 `hiz_occlusion_test` 同口径）；门面仅在 `hiz_enable` ＋ 资源 ＋ 最近生成槽有效时启用，首帧自动跳过。

## 增补（第二百五十七轮：保守性初版）

- `HIZ_RECT_DILATE_PIXELS = 1.0f`：屏幕矩形每边外扩 1 像素后选级与映射纹素（CPU 参考与 `实例压缩.comp` 同步）。
- 采样级上限由 `levels-1` 改为 `levels-2`：末两级（1×1 / 2×1）不采样，规避本机实测「末级内容不可用 → 粗级采样为 0 → 整批误剔」的风险。
- 新增用例：CPU `矩形保守外扩`；GPU `部分遮挡场景不误剔`（左右两墙留缝、缝内实例全可见）。

## 增补（第二百六十至二百六十二轮：MSAA / 观测 / 性能）

- MSAA：场景深度附件以 `SAMPLE_ZERO` 解析到单采样深度（`msaa_depth_image_get`），退场回调在 MSAA 下也可生成。
- 观测：`Counter_Block.occluded_count`（仅遮挡分支累加）；CPU 双计数回读（`uint32_t[2]`，修复曾用 16 字节读取 8 字节缓冲的缺陷）；`Frame_Stats.gpu_occluded`。
- 性能（1080p ＋ MSAA 4× ＋ 1 万实例 ＋ 全屏墙）：录制 +300.5%、GPU 帧 +48.8%、遮挡剔除 2690；未达规划阈值，默认关闭；调优列入 272 轮。
