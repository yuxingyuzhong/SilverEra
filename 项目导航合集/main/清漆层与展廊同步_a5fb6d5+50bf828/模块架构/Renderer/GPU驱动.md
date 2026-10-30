# GPU 驱动模块（GPU驱动）

> 版次：`清漆层与展廊同步_a5fb6d5+50bf828`（第 247–248 轮新增）
> 源码：`Application/Renderer/src/render/GPU驱动/`（`包围体.h` / `剔除数据.h` / `局部命名空间使用.h` / `core/包围体.cpp` / `core/剔除.cpp`）
> 着色器：`Application/Renderer/assets/shaders/视锥剔除.comp`

## 目录

1. [模块功能介绍](#1-模块功能介绍)
2. [模块公开接口](#2-模块公开接口)
3. [模块私有方法](#3-模块私有方法)
4. [模块内部架构](#4-模块内部架构)
5. [模块设计 / 实现细节](#5-模块设计--实现细节)
6. [模块运行链路详例](#6-模块运行链路详例)
7. [待确认](#7-待确认)

## 1. 模块功能介绍

承载 GPU 驱动渲染的**CPU 侧公共数据准备**：把「网格局部 AABB ＋ 实例模型矩阵」换算成世界空间包围球与世界空间 AABB，供后续视锥剔除（248）与 Hi-Z 遮挡（255）使用。本模块**不含任何 Vulkan 类型**，也不做剔除判定本身（判定在 compute 着色器里）。

## 2. 模块公开接口

| 接口 | 语义 |
| --- | --- |
| `struct World_Bounds` | `center[3]` ＋ `radius`（包围球）＋ `aabb_min[3]` / `aabb_max[3]`（世界 AABB）；两项均已含保守扩张 |
| `constexpr WORLD_BOUNDS_AABB_EXPAND = 0.002` | 世界 AABB 每轴外扩量 |
| `constexpr WORLD_BOUNDS_SPHERE_SCALE = 1.001` / `WORLD_BOUNDS_SPHERE_EXPAND = 0.002` | 包围球半径倍率与附加量 |
| `bool world_bounds_build(World_Bounds& out, const float local_min[3], const float local_max[3], const float model[16])` | 由局部 AABB 与列主序模型矩阵构造世界包围体；入参非法返回假 |

### 2.1 第二百四十八轮增量（视锥剔除）

| 接口 | 语义 |
| --- | --- |
| `struct Frustum_Planes{planes[6][4]}` / `FRUSTUM_CULL_EPSILON = 1e-4`（`剔除数据.h`） | 六平面（xyz 法线 ＋ w 常数，已归一化）与判定容差 |
| `bool frustum_planes_extract(Frustum_Planes&, const float view_proj[16])` | 列主序矩阵按行组合（row3 ± row0/1/2；Vulkan z∈[0,1]），逐平面归一化；退化平面返回假 |
| `bool frustum_sphere_visible(planes, center, radius)` | 球判可见（`dot(n,c)+d+r ≥ −ε`，与 `视锥剔除.comp` 同公式） |
| `bool frustum_aabb_visible(planes, aabb_min, aabb_max)` | AABB 正顶点判定（更紧，用于验证 GPU 不多剔） |
| 门面 `bool gpu_cull_mask(transforms, count, local_min, local_max, mask_out)` | CPU 求球 → SSBO → 懒加载 `视锥剔除.comp` 管线 → 调度 → 回读掩码（1 = 可见） |

### 2.2 第二百四十九轮增量（GPU 实例压缩）

| 接口 / 配置 | 语义 |
| --- | --- |
| `Renderer_Config::gpu_cull_enable`（默认关） | 为真且计算可用时，`mesh_draw_instanced` 改走 GPU 压缩；失败自动回落 CPU 剔除 |
| `void gpu_cull_enable_set(bool)` / `bool gpu_cull_enable_get()` | 运行时开关与查询 |
| `bool gpu_instance_compact(transforms, count, local_min, local_max, matrices_out, visible_out)` | 三缓冲（源矩阵 / 压缩输出 / 原子计数）＋ 懒加载 `实例压缩.comp` → 调度 → 回读可见数与压缩矩阵 |
| `Frame_Stats::gpu_cull_ms` | 走 GPU 路径时为计算时间戳量测的压缩耗时；未走时为 0 |

着色器 `实例压缩.comp` 与渲染器 `frustum_cull_aabb` **逐式同构**（mvp × 8 角点 × 六面「全部角点在外」），故开/关两路径像素逐字节一致；原子槽位顺序不保证与输入顺序一致，渲染正确性不依赖顺序。

## 3. 模块私有方法

无（单一自由函数，无内部状态）。

## 4. 模块内部架构

```
GPU驱动/
├── 包围体.h          # Vulkan-free：World_Bounds ＋ 保守常量 ＋ world_bounds_build
├── 剔除数据.h        # Vulkan-free：Frustum_Planes ＋ 平面提取 ＋ 球 / 盒判可见（第 248 轮）
├── 局部命名空间使用.h  # cpp 侧 using 引入
└── core/
    ├── 包围体.cpp    # 实现：中心 / 半对角 / 列长 / 8 角点重求 / 盒角半径修正
    └── 剔除.cpp      # 实现：六平面提取与球 / 盒判定（第 248 轮）
```

- 与 `Compute/` 的关系：计算模块只管 GPU 侧管线与缓冲；包围体的 CPU 侧换算在本模块，二者不互相包含。
- 与 `Renderer` 的关系：门面第 248 轮新增 `gpu_cull_mask`（剔除原语），仍未接入默认帧路径；第 249 轮接入间接绘制后由帧路径调用。

## 5. 模块设计 / 实现细节

1. **数学口径（规划 §2.3）**：局部 AABB 经非轴对齐变换后不再是 AABB，**严禁**直接把局部 min/max 乘矩阵；实现逐 8 角点变换后重求 min/max，结果偏大属正确的保守包围。
2. **球半径**：先按「半对角长 × 三列最大列长 × 1.001 ＋ 0.002」估下界，**再按扩张后世界 AABB 的 8 个角点距离修正并取较大值**（`corner_max × 1.001 ＋ 0.002`）。原因：旋转后世界 AABB 的角点不在变换后局部角点的凸包上，只按局部半对角估半径会出现「AABB 判可见、球判不可见」，破坏 `C ⊆ G`（第二百四十八轮实测：半径 0.0854 对盒角 0.1012）。
3. **球心**：变换局部 AABB 中心（而非 8 角点重心），与半径口径配套。
4. **扩张**：AABB 每轴 ±0.002（世界单位）；球 ×1.001 ＋ 0.002；扩张在构造内完成，调用方无需自行加容差。
5. **非法入参**：空指针 / 局部下界大于上界 / 矩阵含非有限值（`NaN` / `Inf`）一律返回假且不写出参。
6. **样式**：Windows 头带 `min` / `max` 宏，故实现里一律写 `(std::min)` / `(std::max)` 形式避免宏展开冲突。

## 6. 模块运行链路详例

`world_bounds_build(out, [-1,-0.5,-2], [1,0.5,2], 单位矩阵)`：
1. 局部中心 `(0,0,0)`、半对角长 `√(1²+0.5²+2²)`；
2. 三列列长均为 1 → `column_max = 1`；
3. 球心 = 原点；半径 = `half_diagonal × 1.001 ＋ 0.002`；
4. 8 角点变换（单位矩阵即原值）→ AABB = 局部盒；
5. 每轴 ±0.002 后写出。

## 7. 待确认

- 【待确认：247-1】门面尚无「实例包围体缓存」：248 轮接入 GPU 剔除时需要把每实例的 `World_Bounds` 写入实例 SSBO，届时决定是「登记期算一次缓存」还是「每帧重算」（前者省 CPU，后者省内存）。
