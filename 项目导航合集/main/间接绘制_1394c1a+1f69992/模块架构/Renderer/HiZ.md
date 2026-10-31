# Hi-Z 模块（HiZ）

> 版次：`间接绘制_1394c1a+1f69992`（第 253 轮新增；窗口内增量）
> 源码：`Application/Renderer/src/render/HiZ/`（`HiZ.h` / `局部命名空间使用.h` / `core/HiZ.cpp`）
> 着色器：本轮无（生成 compute 在 254 轮落地）

## 目录

1. [模块功能介绍](#1-模块功能介绍)
2. [模块公开接口](#2-模块公开接口)
3. [模块私有方法](#3-模块私有方法)
4. [模块内部架构](#4-模块内部架构)
5. [模块设计 / 实现细节](#5-模块设计--实现细节)
6. [模块运行链路详例](#6-模块运行链路详例)
7. [待确认](#7-待确认)

## 1. 模块功能介绍

按**帧槽**持有 Hi-Z 遮挡剔除所需的两类资源：① 深度历史纹理（上帧场景深度的拷贝）；② 单通道浮点 mip 金字塔（`R32_SFLOAT`）及其逐级视图。并提供「场景深度 → 历史」的录制辅助（屏障链 ＋ `vkCmdCopyImage`）。本模块**不做**遮挡判定、不生成金字塔内容（254 轮由计算着色器写入），也不决定何时启用（门面 / 帧链路负责）。

深度来源核查结论（方向一规划 §2.4）：交换链深度按**交换链图像**轮转，离屏深度归渲染目标自身，均非「每在途帧一份」，故本模块自建每帧槽历史，语义为「同一帧槽上一次使用时的场景深度」。

## 2. 模块公开接口

| 接口 | 语义 |
| --- | --- |
| `constexpr HIZ_LEVEL_MAX = 12` / `HIZ_PYRAMID_FORMAT = VK_FORMAT_R32_SFLOAT` | 金字塔级数上限（4096 内完整）与金字塔格式 |
| `bool build(VkDevice, Memory_Context&, uint32_t frame_overlap, uint32_t width, uint32_t height, VkFormat depth_format)` | 按槽建立历史（深度同格式、`TRANSFER_DST | SAMPLED`）与金字塔（`STORAGE | SAMPLED`，完整 mip 链）＋ 逐级视图；失败整体回滚 |
| `void destroy()` / `bool valid()` | 幂等销毁 / 是否就绪 |
| `bool extent_set(width, height, depth_format)` | 尺寸或格式变化时重建全部槽；一致时为空操作 |
| `uint32_t width_get() / height_get() / level_count() / slot_count()` | 当前尺寸 / 级数 / 槽数（未构建为 0） |
| `bool level_extent_get(level, width_out, height_out)` | 第 level 级尺寸 = `(max(1, w >> level), max(1, h >> level))`；越界返回假且出参归零 |
| `VkImage depth_history_image(slot)` / `VkImageView depth_history_view(slot)` | 该槽历史图像（深度面视图；采样用途） |
| `VkImage pyramid_image(slot)` / `VkImageView pyramid_level_view(slot, level)` | 该槽金字塔与逐级视图（`baseMipLevel = level`、`levelCount = 1`） |
| `bool history_valid(slot)` / `void history_invalidate(slot)` | 历史是否可用（重建 / 尺寸变化后为假；供 254 / 256 首帧跳过遮挡） |
| `bool history_copy_record(command_buffer, slot, source_image, source_layout_before, source_layout_after)` | 录制「源深度 → 历史」：源转 `TRANSFER_SRC` → 历史转 `TRANSFER_DST` → 整幅拷贝（DEPTH 面）→ 历史转 `SHADER_READ_ONLY` → 源恢复；标记历史有效并累计次数 |
| `uint64_t history_copy_count()` | 累计拷贝次数（诊断） |

## 3. 模块私有方法

| 方法 | 职责 |
| --- | --- |
| `level_count_from(width, height)`（文件内静态） | 自 1 级起逐级折半（宽 / 高各自不小于 1），直至 1×1 或达到 `HIZ_LEVEL_MAX` |
| `slot_create(Slot&)` | 创建单个槽的历史图像 / 视图、金字塔图像与逐级视图；任一步失败回滚该槽 |
| `slot_destroy(Slot&)` | 归还单个槽的全部视图与图像（幂等） |
| `slots_destroy()` | 归还全部槽；内存模块已失效时只清句柄（避免访问已销毁的分配器） |

## 4. 模块内部架构

```
HiZ/
├── HiZ.h                 ← 对外头（类声明 / 常量 / Slot 结构）
├── 局部命名空间使用.h      ← 实现文件的类型引入
└── core/HiZ.cpp          ← 构建 / 重建 / 销毁 / 查询 / 历史拷贝录制
```

- `Slot`：`history_image / history_allocation / history_view`、`pyramid_image / pyramid_allocation`、`level_views`（长度 = 级数）、`history_valid`。
- 上下文字段：`device`、`memory*`、`depth_format`、`width / height / levels`、`slots`、`copy_count`。
- 依赖只有**内存模块**（图像 / 视图创建与销毁），不依赖计算上下文、帧模块或门面。

## 5. 模块设计 / 实现细节

- **惰性构建**：默认关闭时零分配；门面首次 `hiz_enable_set(true)` 才调用 `hiz_ensure()` 构建；关闭只置标志、保留资源（避免反复重建，1080p 单槽金字塔约 5.3 MB）。
- **级数口径**：`max(w,h)` 逐级折半至 1×1；64×36 → 7 级 `(64,36)…(1,1)`；上限 12 级，超过 4096 的尺寸按上限截断（遮挡查询需对末级做 clamp，见 255 轮）。
- **历史旧布局**：每次拷贝前统一取 `UNDEFINED`（内容被整幅覆盖，无需维护布局状态机），拷贝后固定 `SHADER_READ_ONLY_OPTIMAL` 供计算 / 片元采样。
- **格式一致性**：`vkCmdCopyImage` 不允许跨格式；历史格式 = 构建时场景深度格式，源不一致时由调用方先行跳过本辅助（本模块不持有源格式信息）。
- **同步**：全部使用 `VkMemoryBarrier2` / `VkImageMemoryBarrier2`（精确 stage / access）：源深度写 → 传输读；传输写 → 着色器读；源恢复深度附件读写。
- **尺寸跟随**：交换链重建成功后由门面调用 `extent_set`；失败则门面销毁上下文并把开关复位（不阻断出帧）。
- **能力约束**：仅当计算路径可用且 `R32_SFLOAT` 同时具备 `STORAGE_IMAGE` 与 `SAMPLED_IMAGE` 特性时门面允许构建（`hiz_supported`）。

## 6. 模块运行链路详例

1. `Renderer::init` 完成交换链 / 内存 / 计算上下文后，探测 `R32_SFLOAT` 格式特性并记录 `hiz_capable`。
2. 配置 `hiz_enable = true` → 初始化末尾 `hiz_ensure()` → `build(device, memory, frame_overlap, extent, depth_format)` → 每槽创建历史与金字塔。
3. 运行时 `hiz_enable_set(false/true)` 只切换标志；再次开启复用已有资源。
4. 交换链重建 → `swapchain_rebuild_deferred` 成功 → `hiz.extent_set(新范围)`（尺寸未变时空操作）。
5. 254 轮起：帧链路在场景 pass 末尾调用 `history_copy_record(命令缓冲, 当前槽, 当前单采样深度, DEPTH_ATTACHMENT, DEPTH_ATTACHMENT)`，随后计算着色器以历史纹理逐级写入金字塔。

## 7. 待确认

- 【待确认：253-1】`HIZ_LEVEL_MAX` 截断后末级仍大于 1×1（> 4096 窗口）；遮挡查询的 clamp 口径留待 255 轮定稿。
- 【待确认：253-2】历史拷贝的接入点（交换链路径 vs 离屏路径 vs 后处理链路径）尚未在录制中验证，254 轮接线后回写本节。
