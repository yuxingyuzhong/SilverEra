# 深研预研 · 阴影 pass 单次提交合并（C2）

> 目的：为方向八阶段 C2 提供可执行的设计与验收口径；本轮为预研轮（文档），下一窗口直接照此落地。
> 现状基线：Renderer `c7085ed` / Test `17b327d`；全量 **1393 例 / 78 套件 / 0 失败**。

---

## 一、现状（代码口径）

阴影重绘在 `渲染器.cpp` 的 ⑤a 段（约 2600–2724 行）：

1. 方向光级联：逐片构造 `Frame_Render_Target`（多级时收缩 `viewport_rect` 到方形小格），把该片矩阵写入 `shadow_items` 每个绘制项的 `push_constants`，调用 `frame.render_to_target(...)`，`shadow_pass = true`；第 0 片清屏，其余片保留。
2. 聚光瓦片：逐瓦片 `target_get` 独立阴影图，写入该瓦片矩阵后再次 `render_to_target(...)`；每张都清屏。
3. 全部 pass 结束后调用一次 `frame.frames_wait()`。

`Frame_Context::render_to_target`（`Frame/core/提交呈现.cpp`，约 292 行）每次调用都会：
`slot_fence_wait` → `vkResetFences` → `frame_slot_pools_reset`（主池 ＋ 次级池）→ 录制（含并行分片）→ `vkQueueSubmit2` → 可选回读。

因此当前口径为「**一片一次提交**」：方向光 1–3 次 ＋ 聚光最多 8 次，合计最多 11 次提交／帧；
每次提交都要等帧槽栅栏并重置整个命令池，CPU 侧重复开销明显。

## 二、目标与可达口径

| 指标 | 现状 | 目标 |
| --- | --- | --- |
| 阴影提交次数 | 级联数 ＋ 瓦片数（≤ 11） | **1** |
| 阴影 pass 数（诊断） | 级联 ＋ 瓦片 | 不变（语义仍是「片数」） |
| 画面 | — | 逐字节不变 |
| 主机侧耗时 | 每次 pass 重复栅栏 / 池重置 | 仅 1 次；新增 `Frame_Stats.shadow_submits` 记录实际提交数 |
| GPU 耗时 | 单一 `shadow_gpu_ms` | 口径不变（整段阴影提交） |

## 三、设计（Frame 模块新增批量入口）

新增（命名暂定，落地时按模块风格收敛）：

```
bool Frame_Context::render_to_targets_batch(
    const Frame_Render_Target* targets, const float* const* pass_matrices,
    uint32_t pass_count, const Frame_Render_Item* items, uint32_t item_count,
    const bool* clear_flags, bool shadow_pass);
```

要点：

1. **一次帧槽准备**：`slot_fence_wait` / `vkResetFences` / `frame_slot_pools_reset` 只做一次（与 `render_to_target` 前置段相同）。
2. **单命令缓冲顺序录制 N 段**：
   - 每段按 `targets[i]` 做布局转换 / 清屏（`clear_flags[i]`）/ 视口 / 绘制；
   - `pass_matrices[i]` 写入该段每个绘制项的 `push_constants`（录制前复制，不改共享数组的归属）；
   - 段间只做必要的图像布局或依赖（各自独立图无需全局屏障；同一张图的分片沿用现有 `viewport_rect` 语义）。
3. **一次提交**：`vkQueueSubmit2` 一次；阴影路径不请求回读、无 UI / 后处理回调，可裁剪相应分支。
4. **诊断**：`shadow_submits` 记 1；`last_shadow_parallel_segments` 取最后一段（与现有口径一致）；GPU 时间戳仍由 Frame 模块在段首／段尾各写一次，语义为「整段阴影」。
5. **默认关闭**：`Light_Data.shadow_batch_submit`（0 = 关闭）＋ 门面 `shadow_batch_submit_set`；关闭时完全走现有逐 pass 路径，逐字节与 662 一致。

## 四、验收与用例

| 用例 | 断言 |
| --- | --- |
| 关闭不变 | 单级 ＋ 双级 ＋ 多瓦片场景，开／关两帧像素逐字节一致 |
| 提交数 | 8 瓦片 ＋ 双级时 `Frame_Stats.shadow_submits` 由 10 降为 1（关闭时不动） |
| 诊断不变 | `shadow_passes / spot_shadow_passes / shadow_resolution` 与关闭时相同 |
| 阴影图不变 | `shadow_readback` 整图逐字节一致（复用 646 的稳定性口径） |
| 性能 | 相对 `性能基线_方向八.md`：主机侧录制 / 提交段 ±%；GPU 段在噪声内（±5%） |

## 五、风险与回退

1. Frame 模块是全部 pass 的公共入口——批量入口必须**只增不改**，现有 `render_to_target` 路径保持原样；
2. 命令池重置与次级池复用须与现有并行分片逻辑兼容（每段仍可用次级池，但必须在该段录制前完成池状态隔离）；
3. 清屏标志与视口：级联片第 0 片清屏、后续片保留；聚光瓦片各自清屏——批量入口逐段处理，不做「首段清屏」的全局假设；
4. 任何失败一律保留旧路径为默认：`shadow_batch_submit` 关闭即完全回退。

## 六、下一步

下一窗口第 663 轮按本预研实施（Frame 批量入口 → 渲染器开关 → 用例 → 性能对比），随后再做 C1 聚光 atlas（binding 9 数组收敛）。
