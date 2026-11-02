# 深研规划 · 方向四（逐物体 toon 化 ＋ 物体多材质）

> 用户 2026-11-02 09:05 指定：方向三做完后转本方向｜起点：方向三收口（Renderer `93e1dda`、Test `392f23e`）
> 相关：[方向三规划](深研规划_方向三_屏幕空间效果.md)、[方向三收口复盘](深研规划_方向三_收口复盘.md)

## 一、方向定位

现有二次元风格化参数（色阶数 / 边缘光 / 暗部色调 / 卡通高光阈值）全部是**场景级全局**（`Light_Data` → Scene UBO），
同一帧内所有物体共用一套「卡通档位」。方向四要做两件事：

1. **逐物体 toon 化**：每个物体（精确说是每次带材质的绘制）可拥有独立的卡通档位，
   使同一帧里「角色 3 档硬阴影 ＋ 背景 5 档柔和 ＋ UI 全平涂」并存；
2. **物体多材质**：一个网格可携带多个材质槽（分段索引区），一次绘制请求内按段绑定不同材质。

## 二、现状事实（复开发必读）

| 事实 | 位置 |
| --- | --- |
| 实例属性 = 模型矩阵 16 浮点 ＋ 材质 4 浮点（`INSTANCE_FLOAT_COUNT = 20`，步长 80） | `Application/Renderer/src/render/Pipeline/管线.h` |
| 实例属性顶点声明 location 3..7（3–6 模型矩阵、7 材质），由 `图形管线.cpp` 的 `add_attribute` 按 SPIR-V 位集声明 | `.../Pipeline/core/图形管线.cpp` |
| 逐物体 PBR 的范式：`Material_Data` 字段 → `Material_Record` → `impl->material_pbr_*` → `instance_matrices_append` 写入实例属性 | `.../Renderer/core/渲染器.cpp` |
| 全局卡通参数：色阶 `scene.pbr_params.w`、边缘光 `scene.sky_params.w`、高光阈值 `scene.toon_specular_params.x`、暗部色调 `camera_range.zw ＋ cascade_params2.w` | `assets/shaders/场景.frag` |
| 材质绑定即吸附：`material_bind_set` 设置 impl 状态，只影响之后登记的绘制项 | `.../Renderer/core/渲染器.cpp` |
| 网格记录只有「整段索引区」（`mesh_record_get` 返回 index_offset / index_count） | `.../Mesh/core/网格加载器.cpp` |

## 三、阶段划分

| 阶段 | 内容 | 关键改动 | 状态 |
| --- | --- | --- | --- |
| A. 逐物体卡通参数 | 实例步长 80 → 96，新增 location 8 vec4（x = 色阶数、y = 边缘光、z = 卡通高光阈值、w = 覆盖标志）；材质字段 `toon_override` ＋ 三项参数；片元按 `in_toon.w` 覆盖全局值 | `管线.h` / `图形管线.cpp` / `场景.vert` / `场景.frag` / `材质数据.h` / `渲染器实现.h` / `渲染器.cpp` | **已交付（第 408 轮）** |
| B. 逐物体暗部色调 | 实例再扩 16 字节（步长 112）＋ `location 9`；`toon_shadow_tint` 改读逐物体 | 同 A 的链路 | **已交付（第 409 轮）** |
| C. 物体多材质 | 网格分段（`Mesh_Section`）＋ `mesh_load_sections` ＋ `Material_Handle` 数组的绘制入口；按段顺序提交多个绘制项 | `网格数据.h` / 网格加载器 / 门面 / 渲染器 | **已交付（第 410 轮；门面 `mesh_draw_range` ＋ `mesh_draw_sections`，索引区按 3 对齐均分）** |
| D. 展廊接入 | 展区⑦「卡通渲染」改为逐物体档位演示（同屏 3 个不同档位物体）；新增「多材质物体」展区 | `Application/Test/src/主调/能力展示/` | 待做 |

> 阶段 A 实测（第 408 轮，Renderer `4a414a6` / Test `4d7b770`）：全关等价差 0、色阶 1 差异 100 像素、边缘光中心外通道 0 → 60、非法色阶 9 差 0；`Renderer_*` 582 / 582。

## 四、阶段 A 设计要点（已落地）

1. **通道**：实例属性第 80..95 字节（location 8 vec4），与材质 vec4 并列；`INSTANCE_FLOAT_COUNT` 20 → 24。
2. **覆盖语义**：`w > 0.5` 时三项整体覆盖（不做「单项 0 表示不覆盖」，因为色阶 0 与边缘光 0 本身就是合法值，需要与「不覆盖」区分）。
3. **兼容性**：覆盖标志默认 0 → 片元沿用全局值；实例步长变化由宏推导（合批相邻判定、阴影签名、实例缓冲容量、上传字节数全部自动跟随）。
4. **参数校验**：色阶 `[0, 8]`、边缘光 `[0, 1]`、高光阈值 `[0, 1]`；任一项非有限或越界 → 整组按不覆盖登记（告警留痕）。
5. **复位**：`init` 重置块与 `material_bind_set(0)` / 无效句柄回落块都要清 `material_toon_*`，保证默认画面不变。

## 五、验收口径

- 全关等价：`toon_override = true` 且三项为 0 的材质与不绑定材质画面逐通道差 ≤ 2（实测 0）；
- 逐物体生效：色阶 1 vs 不覆盖，显著差异像素 ≥ 30（实测 100）；
- 逐物体边缘光：倾斜 45° 四边形无光照时，中心外通道 0 → ≥ 40（实测 0 → 60）；
- 非法参数：色阶 9 越界按不覆盖，画面差 ≤ 2（实测 0）；
- 全量回归：`Renderer_*` 582 / 582。

## 六、阶段 C 实现方案（物体多材质，待做）

1. **门面新增两个入口**（`渲染器.h` ＋ `渲染器.cpp`）：
   - `bool mesh_draw_range(Mesh_Handle, const float* transform, const float* color, uint32_t first_index, uint32_t index_count)`：按索引区间登记一个绘制项；
   - `bool mesh_draw_sections(Mesh_Handle, const float* transform, const Material_Handle* materials, uint32_t count)`：
     把网格总索引数按 `count` 均分（余数归前段），逐段 `material_bind_set(materials[i])` ＋ `mesh_draw_range`，
     全部段成功后恢复 `material_bind_set(0)`；任一段失败返回假（已登记的段不回滚，与既有绘制语义一致）。
2. **内部参数化**：`mesh_draw` 的现有实现改为内部 `mesh_draw_impl(handle, transform, color, first_index, index_count)`
   （`first_index = UINT32_MAX` 表示整段），公开 `mesh_draw` 一行转发——避免复制约 200 行登记逻辑。
3. **索引偏移换算**：`index_offset = 网格索引区基址 ＋ first_index × 4`（`uint32` 索引），`index_count = 区间长度`；
   区间越界（`first_index + count > 总索引数`）整次拒绝并告警。
4. **剔除与合批**：区间绘制沿用网格包围盒剔除（保守口径，可能保留不可见段）；多材质段是独立绘制项，
   仅当「同管线 ＋ 推送块相同 ＋ 实例区首尾相接」才合批，多材质通常不合批——第一版接受。
5. **测试口径**：立方体（36 索引）分 2 段（18 / 18，恰好两个三角面组）；两材质反照率色调不同（红 / 蓝）；
   断言两帧（单材质 vs 双材质）差异像素 ≥ 50、且双材质画面中红蓝各占明显区域（按外通道判定）。

## 七、风险与边界

- **实例步长增长**：每实例多 16 字节（阶段 B 后 112 字节 / 实例）；`instance_capacity` 个实例的内存占用随步长线性上升。
- **多材质与合批**：同一物体的多个材质段是独立的绘制项，静态合批只能合并「同管线同实例区且相邻」的项，多材质会降低合批率；第一版不做自动合批优化。
- **暗部色调的通道带宽**：`toon_shadow_tint` 需 3 个浮点，阶段 B 若继续扩实例步长，需重新核算 `INSTANCE_FLOAT_COUNT` 与顶点属性上限（当前数组 9 条，最多可扩到 16）。
