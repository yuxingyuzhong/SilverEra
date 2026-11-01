# 深研规划 · 方向二 阶段 D：粒子间接绘制（GPU 写实例数）

- 形成轮次：第 332 轮（文档轮，2026-11-01）
- 起点代码版次：Renderer `3d934d6`、Test `790eaa3`
- 目标：把常驻粒子的绘制从「主机回读位图 / 状态 → 构造实例矩阵」改为「GPU 紧凑＋GPU 写实例数 → 单条 `vkCmdDrawIndexedIndirect`」，
  解除常驻绘制 65 536 容量上限，使 1 048 576 容量可直接出帧。

---

## 一、现状与阻塞

| 项 | 现状 | 阻塞点 |
| --- | --- | --- |
| 常驻状态 | 双缓冲 + 控制缓冲（位图 / 计数 / 环形指针）全在 GPU | 主机无紧凑索引概念 |
| 常驻绘制 | 回读位图（偏移 8）＋整块状态 → CPU 构 billboard 矩阵 → 实例缓冲上传 | 1M 时回读 128 KB + 320 MB 实例上传不可接受，故上限 65 536 |
| 帧间接路径 | `Frame_Context` 已有主机写入的 `vkCmdDrawIndexedIndirect`（第二百五十二轮），仅用于「相邻等价项合并」 | 命令由主机写，实例数来自 `item.instance_count` |
| 管线布局 | 全部图形管线共享 set0 = 场景 UBO(binding 0) + 簇缓冲(1..4)、set1 = 采样器 | 无「存储缓冲进 set0」的粒子专用描述符集 |
| 索引类型 | 全层统一 `VK_INDEX_TYPE_UINT32`，索引偏移在绑定时给出 | 间接命令 `first_index` 相对绑定偏移，可恒为 0 |

## 二、设计要点

1. **紧凑索引缓冲**：`uint32 × 容量`（STORAGE），由 `Particle紧凑.comp` 对活跃位图逐位判活、
   `atomicAdd` 取位写入索引；写指针 `control[0]` 不可复用，另设 `紧凑计数`（独立 4 B 缓冲或控制缓冲新增字）。
2. **间接命令**：`VkDrawIndexedIndirectCommand`（20 B）= {index_count = 6（四边形两三角）, instance_count = 紧凑计数,
   first_index = 0, vertex_offset = 0, first_instance = 0}；缓冲用法 `STORAGE | INDIRECT`。
   命令由**单线程 dispatch 的 `粒子间接.comp`** 写（避免紧凑阶段与写命令的跨工作组同步）。
3. **GPU 侧 billboard**：`粒子GPU.vert` 用 `gl_InstanceIndex` → 紧凑索引 → `Particle_State`，
   在着色器中按「相机位置 → 粒子」构造右 / 上基向量（相机位置经推送常量给出），
   输出与 `粒子.frag` 兼容的 `out_uv` / `flat out_age`；尺寸随年龄收缩 35% 同步下沉到着色器。
4. **描述符接入**：`Pipeline_Context` 增 `particle_set_get(slot)`（按 set0 既有布局分配一条独立描述符集）＋
   `particle_scene_bind(slot, state_buffer, index_buffer)`（binding 1 = 状态、binding 2 = 紧凑索引、3/4 = 占位，
   binding 0 = 场景 UBO）；与场景簇缓冲同位但**集合独立**，不覆盖场景 pass 的绑定。
5. **帧侧 GPU 间接项**：`Frame_Render_Item` 增 `VkBuffer indirect_buffer` / `VkDeviceSize indirect_offset` / `bool gpu_indirect`；
   录制时在绑定管线、描述符集、推送常量、顶点与索引缓冲后执行 `vkCmdDrawIndexedIndirect(..., 1, 20)`；
   `batch_adjacent_items` 与「相邻等价项间接合并」**跳过** `gpu_indirect` 项；阴影 pass 不收录（`cast_shadow = false`）。
6. **门面**：`particle_indirect_draw(float size)`（常驻模式专用）：
   上传相机位置 / 尺寸 / 参数推送 → 调度「紧凑 ＋ 写命令」→ 登记一个 `gpu_indirect` 绘制项；
   容量上限取 `PARTICLE_CAPACITY_MAX`（1 048 576），不再受 65 536 限制。

## 三、分阶段实施

| 轮次 | 内容 | 验收 |
| --- | --- | --- |
| 332 | 本设计定稿（文档轮） | 契约与分阶段可评审 |
| 333 | `Particle紧凑.comp` ＋ 紧凑计数缓冲 ＋ `粒子间接.comp` 写命令 | 紧凑数与位图计数一致（回读计数 = 活跃数） |
| 334 | `Pipeline_Context::particle_set_get / particle_scene_bind` ＋ `粒子GPU.vert` | 着色器编译通过、管线创建成功 |
| 335 | 帧侧 `gpu_indirect` 项 ＋ `particle_indirect_draw` ＋ 用例 | 出帧可见；与常驻绘制像素等价（容差阈值） |
| 336 | 百万间接绘制基线 ＋ 阶段 D 收口 ＋ 导航新版次 | 1M 单条间接命令出帧、实例数来自 GPU |

## 四、待确认事项

1. **描述符池容量**：粒子专用描述符集需在 `Pipeline_Context` 的池中额外分配（每帧槽 1 条），需确认池剩余配额是否足够。
2. **实例缓冲空值**：录制层当前要求 `instance_buffer` 非空；`gpu_indirect` 项是否放宽需在 333 轮核对 `命令录制.cpp` 前置校验。
3. **推送常量布局**：粒子管线与场景管线共享布局，粒子需要「相机位置 ＋ size ＋ 年龄参数」；
   需确认现有 128 B 块内剩余偏移（当前场景项用 0，后处理用 80 / 96 / 112）。
4. **深度分桶与间接**：分桶（0..16）在 GPU 紧凑下需每桶一次紧凑或分桶索引表；阶段 D 首版**仅支持关闭分桶**（`particle_sort_buckets = 0`），分桶 + 间接留待后续。

## 五、风险

- 帧侧 `gpu_indirect` 项绕过主机实例数，**合批与剔除统计口径**需分别处理，以免 `drawn_instances` 统计失真；
- 多帧在途时间接缓冲需按帧槽复用（或在 `frames_wait` 后写），避免与上一帧 GPU 读取冲突；
- 校验层（validation）对 `STORAGE | INDIRECT` 组合缓冲的用法标注需完整（`VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | STORAGE_BUFFER_BIT`）。
