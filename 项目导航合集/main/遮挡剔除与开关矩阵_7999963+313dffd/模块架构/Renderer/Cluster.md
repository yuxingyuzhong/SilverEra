# Cluster 模块（分簇光照）

> 本文件为第二百六十七轮新建模块的首版。相关门面接线见 [门面.md](门面.md) 第 16–17 条；
> 测试见 [单元测试-渲染.md](../Test/单元测试-渲染.md) 第二百六十六 / 二百六十七轮。

## 文件内目录

- §1 模块介绍
- §2 公开接口
- §3 私有实现
- §4 内部架构
- §5 设计细节
- §6 运行链路

## §1 模块介绍

`src/render/Cluster/` 是阶段 D（Cluster 光照）的划分与计数模块：
把裁剪空间按 `Cluster_Grid` 切簇，并为「光源记录 → 逐簇覆盖计数」提供 CPU 参考实现。
模块不依赖 Vulkan / 相机 / 门面，只依赖 Vulkan-free 的 `Scene/光源数据.h`。

本版次（267）只落地数据与 CPU 参考；GPU 逐簇计数（268）、前缀和（269–270）、列表写入（271）、
着色器采样（272 起）按规划顺延。

## §2 公开接口

| 名称 | 签名 | 说明 |
| --- | --- | --- |
| `Cluster_Grid` | `struct { uint32_t x = 16; y = 9; z = 24; }` | 网格轴数 |
| `cluster_grid_clusters` | `constexpr uint32_t cluster_grid_clusters(Cluster_Grid)` | 总簇数（任一轴 0 → 0） |
| `cluster_grid_clamp` | `constexpr Cluster_Grid cluster_grid_clamp(Cluster_Grid)` | 各轴 1..64；总数 > 4096 时收缩 z |
| `Cluster_Input` | `struct` | `view_proj[16]` / `camera_position[3]` / `fov` / `aspect` / `near_plane` / `far_plane` |
| `cluster_counts_reference` | `uint32_t cluster_counts_reference(uint32_t* counts, uint32_t capacity, Cluster_Grid, const Light_Record*, uint32_t, const Cluster_Input&)` | 写出逐簇计数，返回总簇数；容量不足 / 非法入参返回 0 |

常量：`CLUSTER_GRID_X/Y/Z_DEFAULT`（16 / 9 / 24）、`CLUSTER_GRID_AXIS_MAX = 64`、`CLUSTER_TOTAL_MAX = 4096`。

## §3 私有实现

- `length3`：三维向量长度（球心到相机距离）。
- `transform_point`：列主序 4×4 × 三维点，写出齐次坐标。
- `clamp_float` / `index_floor`：NaN 安全钳制与「floor 后钳到轴范围」的下标换算。

## §4 内部架构

```
Cluster/
├── 簇划分.h            ← 公开数据与参考计数声明（Vulkan-free）
├── core/簇划分.cpp     ← 计数实现
```

计数缓冲线性下标为 `(z * y + y) * x + x`（x 变化最快），与后续前缀和 / 列表写入共用同一排布。

## §5 设计细节

1. **只统计有效记录**：`Light_Record.flags` bit0 为真的记录参与计数（补光强度 0 → flags=0 → 不计）。
2. **方向光全簇**：`Light_Type::Directional` 直接给全部簇 +1。
3. **点 / 聚光保守覆盖**：球心经 `view_proj` 投影到屏幕（`u = 0.5 + 0.5 · cx/w`、`v = 0.5 − 0.5 · cy/w`，Y 翻转与引擎一致）；
   半径占屏幕比例 `dv = r / (2 · d · tan(fov/2))`、`du = dv / aspect`；深度区间 `[d − r, d + r]`
   线性映射到 z 轴。矩形上界取 `ceil − 1`，宁多不少。
4. **跨近平面 / 相机背后**：整屏且全部深度（w ≤ 0 或 `d − r ≤ near`）。
5. **不做精确球-簇相交**：保守覆盖是为 GPU 同口径（268）与后续列表正确性服务；簇内多算仅带来额外光源循环。

## §6 运行链路

`Renderer::cluster_counts_reference_ex` → 取当前相机（`view_proj_get` / `position_get` / `fov_get` /
`aspect_effective_get` / `clip_get`）＋ `light_records_get` 快照 → `cluster_counts_reference`
→ 线性逐簇计数写入调用方缓冲（供测试对数和 268 轮 GPU 回读比较）。

## 本版次内更新（第二百六十八轮：GPU 逐簇计数）

1. `assets/shaders/Cluster计数.comp`：local_size 64；binding 0 只读 `Light_Record` 数组、binding 1 `uint` 计数数组；
   推送常量 112 字节（`view_proj[4]` / `uvec4 grid`（x,y,z,记录数）/ 相机位置 / `(fov, aspect, near, far)`）。
2. 口径：方向光逐线程全簇 `atomicAdd`；点 / 聚光用与 §5.3 完全相同的屏幕矩形与深度区间公式，原子累加范围内簇。
3. 门面 `Renderer::cluster_counts_gpu`（非 const）：记录快照 → 光源/计数缓冲 → 懒加载管线 → 绑定 0/1 →
   `compute_dispatch(count, 64)` → `compute_buffer_read`；失败逐级回收。
4. 验证：`Renderer_Cluster_Test.GPU逐簇计数与CPU参考一致` 断言两组场景逐簇**完全相等**（非仅总数）。

## 本版次内更新（第二百六十九轮：块内前缀和）

1. 常量 `CLUSTER_SCAN_BLOCK = 256`（与着色器 `local_size` 一致）；块数 `ceil(count / 256)`。
2. CPU 参考 `cluster_prefix_block_scan`：逐块**包含式**前缀和并写出 `block_totals[block]`。
3. `Cluster块内扫描.comp`：binding 0 只读计数、binding 1 前缀输出、binding 2 块总和；越界线程填 0 参与屏障；
   共享内存 Hillis-Steele 8 轮（offset 1..128），块尾线程写块总和。
4. 门面 `cluster_prefix_block_scan_cpu` / `cluster_prefix_block_scan_gpu`（非 const）。
5. 验证：14 块 3456 元素逐元素前缀和与逐块总和与 CPU 完全相等。

## 本版次内更新（第二百七十轮：全量前缀和）

1. CPU 参考 `cluster_prefix_scan`：全局包含式前缀和。
2. `Cluster块间扫描.comp`：单线程串行扫描块总和写排他偏移（总簇 ≤ 4096 → 最多 16 块）。
3. `Cluster块偏移合成.comp`：`prefix[i] += offsets[i / 256]`。
4. 门面 `cluster_prefix_scan_cpu` / `cluster_prefix_scan_gpu`（四缓冲 ＋ 三步调度 ＋ 回读）。
5. 验证：4096 元素（16 块）逐元素与 CPU 相等；末项等于总计数。

## 本版次内更新（第二百七十一轮：簇光源列表）

1. CPU 参考 `cluster_list_offsets`（排他偏移 ＋ 总条目）与 `cluster_list_reference`（按光源顺序写索引）。
2. `Cluster列表写入.comp`：binding 0 光源记录 / 1 计数 / 2 游标 / 3 列表；`atomicAdd(cursors[c], 1)` 取槽位；
   方向光写全部簇；推送常量 116 字节（在前 112 字节后追加 `light_count`）。
3. 门面 `cluster_list_build_cpu` / `cluster_list_build_gpu`。
4. 验证：偏移逐簇一致、逐簇成员排序后与 CPU 一致（簇内顺序允许不同）。
