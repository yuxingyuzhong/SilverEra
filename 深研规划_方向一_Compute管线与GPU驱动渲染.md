# 深研规划 · 方向一：Compute Shader 管线 ＋ GPU 驱动渲染

> 状态：**待用户评审**（2026-10-30 制定）
> 口径（用户 2026-10-30 裁定）：三个深研方向按 **Compute → 粒子 → toon/多层** 串行；GPU 驱动范围 **全都要（剔除 ＋ Hi-Z ＋ Cluster）**；新能力**默认关闭、默认路径逐字节不变**；轮粒度 **30–40 分钟/轮**；任务报告**每轮新建**，项目导航**每窗口建立一次**。
> 计划提前量：本方向排布 **32 轮**（≥20 轮要求，按「全都要」扩容）。

---

## 一、目标与验收总纲

- **目标**：在现有 Vulkan 1.3 渲染层内建成一套**可复用、数据驱动、默认关闭**的 GPU 驱动渲染链路：
  1. 通用 **Compute 管线**（着色器模块 / 描述符 / 推送常量 / 屏障 / 调度封装）；
  2. **GPU 端视锥与距离剔除** ＋ **间接绘制**（Indirect Draw）；
  3. **Hi-Z 遮挡剔除**（深度金字塔 ＋ 上一帧复用）；
  4. **Cluster 光照**（3D 分簇 ＋ 逐簇光源列表 ＋ 着色器采样）。
- **验收总纲**（每项都须同时满足）：
  - 功能：三类剔除与 Cluster 光照在展廊/用例中**可见生效**，计数与 CPU 基线一致或更优；
  - 兼容：全部新路径**默认关闭**；关闭时**画面逐字节不变**（像素级回归证明）；
  - 完整：不是「最小可用」——包含资源生命周期、降级路径、观测字段、门面与数据化、用例、展廊展示；
  - 质量：三树零错误、全量回归全过、源码统计与文档按轮落盘。

## 二、现状与差距（2026-10-30 核查）

| 能力 | 现状 | 差距 |
| --- | --- | --- |
| `.comp` 编译 | CMake 已 glob `assets/shaders/*.comp` 并用 glslc 编译（第 399 行） | **尚未有任何 .comp 源文件** |
| Compute 管线 | 无 | 需新增 `vkCreateComputePipelines`、compute 描述符布局、compute 绑定/调度 |
| 存储缓冲（SSBO） | 内存模块只覆盖 UBO / 顶点 / 索引 / 实例 | 需通用 storage buffer 创建、写入、读回 |
| GPU 数据回读 | 仅颜色/深度/纹理读回 | 需 compute 结果回读通道（staging） |
| 间接绘制 | 无（全部 `vkCmdDrawIndexed`） | 需 indirect 参数/计数缓冲与 `vkCmdDrawIndexedIndirect` |
| Hi-Z | 无（仅有级联阴影深度图） | 需深度金字塔生成与采样 |
| Cluster 光照 | 逐片元循环 8 盏点光/聚光（`场景.frag`） | 需光源 SSBO、分簇构建（前缀和）、着色器分支 |

## 三、架构设计（先定骨架，避免架构债）

### 3.1 模块划分

```
Application/Renderer/src/render/
├── Compute/                     # 新增：通用 compute 基础设施
│   ├── core/                    #   compute 管线缓存 / 描述符 / 调度 / 屏障
│   ├── 计算数据.h               #   Vulkan-free 公共语义头（开关、线程组、统计）
│   └── Compute.h                #   对外头（供 Renderer 门面消费）
├── GPU驱动/                     # 新增：剔除与间接绘制
│   ├── core/                    #   视锥/距离/遮挡测试 compute、indirect 参数
│   └── 驱动数据.h               #   开关、阈值、统计
├── HiZ/                         # 新增：深度金字塔
│   ├── core/                    #   生成 compute（逐级降采样取最大深度）
│   └── HiZ数据.h
└── Cluster/                     # 新增：分簇光照
    ├── core/                    #   分簇构建、前缀和、光源列表
    └── 分簇数据.h
```

- 新模块一律遵守既有「core/ ＋ 对外头 ＋ 局部命名空间使用.h」结构；门面 `渲染器.h` 只保留转发与查询，能力域 API 放扩展头。
- `Frame` 模块负责把 compute pass 与图形 pass 串联（复用现有并行录制与命令池），**不把 compute 逻辑塞进 Frame**。

### 3.2 数据布局（关键决策，实施前冻结）

- **实例剔除输入**：`Instanced_Record { mat4 model; vec4 material; vec4 aabb_min; vec4 aabb_max; }`（AABB 由网格模块提供局部包围盒 × 模型矩阵求得，或在登记时预变换）。
- **剔除输出（间接参数）**：`Draw_Indexed_Indirect { uint32 index_count; uint32 instance_count; uint32 first_index; int32 vertex_offset; uint32 first_instance; }` ＋ 独立 `visible_count` 缓冲（供统计与回读）。
- **Hi-Z**：与场景深度同宽高的 `R32_SFLOAT` 金字塔，级数 `floor(log2(max(w,h))) + 1`；`HiZ[i] = max(HiZ[i+1] 四邻域)`。
- **Cluster**：视图空间 3D 网格（默认 `16×9×24`，可配）；`cluster_light_count[i]`、`cluster_light_offset[i]`（前缀和）、`cluster_light_indices[]`。
- **光源 SSBO**：`Light_Record { vec4 pos_radius; vec4 color_intensity; vec4 dir_cone; vec4 params; }`，上限可配（默认 1024），旧 8 盏门面 API **保留并映射**。

### 3.3 同步与生命周期

- compute 与图形 pass 之间用 **同步 2 屏障**（`VkMemoryBarrier2` 的 stage/access 掩码按用途精确指定），禁止 `vkDeviceWaitIdle`。
- 资源全部走既有**延迟销毁队列**（在途帧数 + 1），compute 缓冲同规则。
- Hi-Z 使用**上一帧**深度（延迟一帧，避免同帧读写冲突）；首帧无历史时整体跳过遮挡，只做视锥剔除。
- 不支持 compute 的设备：配置项直接报 false 并回落到 CPU 剔除路径（现有路径）。

### 3.4 门面与数据化（默认关闭）

- `Renderer_Config` 增：`compute_enable`（默认 false）、`gpu_cull_enable`、`hiz_enable`、`cluster_enable`、`cluster_grid[3]`、`light_capacity`。
- 门面增：`compute_supported()`、`gpu_cull_set/get`、`hiz_set/get`、`cluster_set/get`、`cluster_grid_set`、`gpu_stats()`（剔除/间接/集群计数）。
- **所有开关关闭时**：走原逐项 `vkCmdDrawIndexed` 与逐片元光源循环，像素逐字节一致。

## 四、32 轮分解（每轮 30–40 分钟）

### 阶段 A · Compute 基础设施（1–6 轮）

| 轮 | 主题 | 主要改动 | 验收 |
| --- | --- | --- | --- |
| 241 | Compute 管线创建与绑定 | `Pipeline` 增 compute 管线分支（layout / descriptor / push constant），`pipeline_compute_register/activate` | 空 compute 着色器（写入常量）回读正确 |
| 242 | Compute 调度封装 | 新模块 `Compute/core`：`dispatch(group_x,...)`、组尺寸查询、错误检查、屏障辅助 | 调度封装单测（1D 拷贝用例） |
| 243 | 门面 `compute_*` ＋ SSBO 创建 | `buffer_create/storage_bind`、`compute_load/bind/dispatch`；默认关闭 | 1 万 float「加一」用例 CPU 回读一致 |
| 244 | GPU 数据回读通道 | staging 缓冲 ＋ 拷贝 ＋ 回读封装；门面 `compute_readback` | 回读与写入值一致（含大缓冲） |
| 245 | Compute 统计与计时 | `Frame_Stats` 增 `compute_dispatches` / `gpu_compute_ms`（时间戳） | 关闭时为 0；开启可读 |
| 246 | Compute 资源生命周期与降级 | 延迟销毁接入、设备不支持时 `compute_supported = false` 且初始化不失败 | 强制关闭仍正常出帧 |

### 阶段 B · GPU 剔除 ＋ 间接绘制（7–12 轮）

| 轮 | 主题 | 主要改动 | 验收 |
| --- | --- | --- | --- |
| 247 | 实例 AABB 缓冲 | 网格局部 AABB × 模型矩阵构建 compute 输入（登记期生成） | AABB 与 CPU 计算一致 |
| 248 | 视锥剔除 compute | 6 平面测试写可见索引与 count | 可见集合与 CPU 剔除一致 |
| 249 | 间接绘制路径 | `vkCmdDrawIndexedIndirect` ＋ count buffer；门面开关 | 开关打开时画面与逐项路径逐字节一致 |
| 250 | 距离 / 背面剔除 | 距离阈值与背面朝向剔除并入 compute | 用例覆盖边界（相切、背向） |
| 251 | 剔除观测与统计 | `gpu_culled_instances` / `indirect_draws`；面板与展廊同步 | 计数与 CPU 基线一致 |
| 252 | 多批实例合批与缓冲复用 | 多网格批次的 indirect 参数合并、每帧缓冲复用 | 大实例场景（≥1 万）帧率不降 |

### 阶段 C · Hi-Z 遮挡剔除（13–20 轮）

| 轮 | 主题 | 主要改动 | 验收 |
| --- | --- | --- | --- |
| 253 | 深度金字塔资源 | `HiZ` 模块：级数计算、逐级视图、布局与销毁 | 资源创建/销毁无校验错误 |
| 254 | Hi-Z 生成 compute | 逐级 2×2 取最大深度；边界 clamp | 每级值 = 下一级对应区域最大值 |
| 255 | 遮挡测试 compute | AABB 投影矩形 ＋ 逐级 `max` 比较 | 被完全遮挡实例被剔除 |
| 256 | 接入剔除管线（默认关闭） | `hiz_enable` 开关；首帧跳过 | 开关关闭画面逐字节不变 |
| 257 | 保守性收紧 | 投影矩形扩张与深度 bias；避免误剔除 | 用例：可见实例 0 误剔除（多场景） |
| 258 | 上一帧 Hi-Z 复用 | 延迟一帧读取历史深度 | 画面稳定、无闪烁（逐帧像素一致） |
| 259 | MSAA / 离屏兼容 | MSAA 下用解析深度生成 Hi-Z；离屏路径同口径 | 4× MSAA 路径可用 |
| 260 | Hi-Z 观测与展廊 | `hiz_culled_instances`、金字塔可视化开关 | 面板与展廊可见 |

### 阶段 D · Cluster 光照（21–29 轮）

| 轮 | 主题 | 主要改动 | 验收 |
| --- | --- | --- | --- |
| 261 | 光源 SSBO 化 | `Light_Record` 数组、容量可配（默认 1024）；旧 8 盏 API 映射 | 旧用例全过（≤8 盏逐字节一致） |
| 262 | 分簇网格与计数 | 视图空间 3D 网格、逐簇光源计数 compute | 计数与 CPU 参考一致 |
| 263 | 前缀和与列表写入 | 并行前缀和 ＋ 索引写入（两段 dispatch） | 偏移单调、无越界 |
| 264 | 着色器 Cluster 采样 | `场景.frag` 增 cluster 分支（默认关闭） | 小规模光源与旧路径逐像素一致 |
| 265 | Cluster 数据化与门面 | `cluster_set`、`cluster_grid_set`、`Cluster_Data` | 配置非法值钳制 |
| 266 | 大于 8 盏光源支持 | 点光/聚光上限扩到 `light_capacity`；门面 API 兼容 | 512 盏点光可用且不崩 |
| 267 | 与阴影 / 聚光锥 / 衰减兼容 | Cluster 分支覆盖三类光源参数 | 阴影/锥形/衰减用例全过 |
| 268 | Cluster 展廊展示 | 数百点光的「灯墙」展区 ＋ 图例 | 目视确认 |
| 269 | 调优与耗时观测 | local size / dispatch 合并；`gpu_cluster_ms` | 构建耗时下降，画面不变 |

### 阶段 E · 整合收口（30–32 轮）

| 轮 | 主题 | 主要改动 | 验收 |
| --- | --- | --- | --- |
| 270 | 总开关与降级矩阵 | 四开关组合矩阵（纯 CPU / 仅 compute / 剔除 / Hi-Z / Cluster） | 每种组合可出帧 |
| 271 | 全路径回归与稳定性 | 三树构建、全量回归、展示冒烟、双跑复核 | 全量全过 |
| 272 | 性能基线记录与方向收口 | 工作站记录性能对比、能力汇总、导航、按轮报告 | 文档齐备 |

## 五、测试与回归策略

- **用例分层**：L1 数据正确性（SSBO/回读/AABB/前缀和）→ L2 行为一致性（开关开/关像素逐字节）→ L3 场景级（大实例、512 光源、MSAA、离屏）。
- **新套件**：`Renderer_Compute_Test`、`Renderer_Gpu_Cull_Test`、`Renderer_HiZ_Test`、`Renderer_Cluster_Test`，各 ≤ 约 400 行（遵守单文件上限）。
- **回归口径**：每轮全量回归；默认关闭路径必须逐字节一致；性能类改动须给出前后计数（绘制项、剔除数、dispatch 数）。

## 六、风险与回退

| 风险 | 处置 |
| --- | --- |
| Compute 队列族缺失（仅图形队列） | 优先复用图形队列族（Vulkan 1.3 常见），失败即降级并报 `compute_supported=false` |
| Hi-Z 误剔除造成闪烁 | 投影矩形扩张 ＋ 深度 bias ＋ 上一帧复用；用例「零误剔除」把关 |
| Cluster 前缀和在 30–40 分钟轮内偏重 | 允许第 263 轮拆成「扫描上行/下行」两次提交，仍算一轮 |
| 着色器分支导致旧路径像素变化 | 所有分支以 push constant/UBO 开关驱动，关闭时值恒为 0，编译器仍可能重排 —— 用像素回归强制把关 |
| 设备能力差异（本机 NVIDIA） | 关键能力（compute、timestamp、storage buffer）启动时探测并记录 |

## 七、文档与流程义务

- **每轮**新建任务报告目录（执行计划 ＋ 执行报告），并更新 `报告提交历史`。
- **每窗口**新建项目导航版次（窗口内就地增量），窗口收口更新 README / 分支提交历史。
- 两个汇总文件（执行计划汇总 / 执行报告汇总）、能力汇总、下轮方向逐轮追加；外部库变更逐轮登记（无则写「无」）。
- 源码统计每轮运行 `检查/源码行数统计.ps1`。

## 八、与方向二 / 三的接口预留

- **粒子**复用：Compute 调度封装、SSBO、间接绘制、GPU 回读 —— 方向二不再重复搭基础设施。
- **toon/多层材质**只依赖材质与着色器域，**不修改** compute/剔除结构；多层材质的材质参数化预留 SSBO 扩展位（图层栈最多 8 层，参数走材质 UBO/SSBO）。
