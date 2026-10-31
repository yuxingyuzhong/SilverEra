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
