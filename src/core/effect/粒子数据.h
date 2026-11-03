#pragma once
/*
粒子数据（第三百零七轮迁入引擎核心层）
定位：图形 API-free 纯数据与常量；CPU 模拟（系统层）、GPU 缓冲上传与绘制（渲染层）共用同一布局。
约束：结构字段一律英文、下划线分隔；整体大小须与 GPU std430 布局一致（由静态断言守住）。
本头位于 EngineCore，由系统层与渲染层共同引用 —— 模拟语义与绘制职责因此可分离，
而两侧仍共享同一份数据类型。
*/
#include <cstddef>
#include <cstdint>

//引擎命名空间
namespace engine
{
	/*
	单个粒子状态（32 字节：位置 12 ＋ 速度 12 ＋ 年龄 4 ＋ 寿命 4）
	age 达到 lifetime 即视为死亡；position / velocity 为世界坐标与每秒速度。
	*/
	struct Particle_State
	{
		float position[3];
		float velocity[3];
		float age;
		float lifetime;
	};

	//发射形状（0 = 点、1 = 盒、2 = 球壳；越界值一律拒绝）
	enum class Emit_Shape : uint32_t
	{
		Point = 0,
		Box = 1,
		Sphere_Shell = 2,
	};

	/*
	力场类型（第三百一十八轮；越界值一律拒绝）
	Attractor = 引力 / 斥力点（按半径线性衰减）；Vortex = 绕轴涡流；Wind = 恒定风向。
	*/
	enum class Force_Type : uint32_t
	{
		Attractor = 0,
		Vortex = 1,
		Wind = 2,
	};

	/*
	单个力场（40 字节；与 GPU std430 float 数组布局逐字段一致）
	position = 力场中心；radius = 作用半径（Wind 忽略）；strength = 强度（可负，斥力）；
	direction = 风方向 / 涡流轴（长度须非零，内部归一化）。
	*/
	struct Particle_Force_Field
	{
		float position[3];
		float radius;
		float strength;
		float direction[3];
		uint32_t type;
		float padding;
	};

	/*
	发射器（一次配置、逐帧按 count 发射）
	position = 发射中心；extents = 盒半尺寸 / 球壳半径（点发射忽略）；
	direction = 主发射方向（长度须非零，内部归一化），与随机锥混合；
	speed / lifetime 为 [min, max] 区间（min ≤ max）。
	*/
	struct Particle_Emitter
	{
		float position[3];
		float extents[3];
		float direction[3];
		float speed_min;
		float speed_max;
		float lifetime_min;
		float lifetime_max;
		uint32_t shape;
		uint32_t count;
	};

	/*
	全局模拟参数
	gravity = 每秒平方加速度；damping = **每步**速度衰减系数（0..1，1 = 不衰减；固定步长口径）；
	floor_y = 地面高度；restitution / friction = 反弹系数与切向摩擦（0..1）。
	force_count / forces = 力场数量与数组（按顺序逐场施加，最多 PARTICLE_FORCE_MAX 个）。
	color_start / color_end / color_enabled = 颜色覆盖（默认关闭）。
	edge_softness = 软边宽度（0 = 内置 0.65 内半径）。
	soft_depth_range = 软粒子深度档位（0 = 关闭）。
	*/
	struct Particle_Params
	{
		float gravity[3];
		float damping;
		float floor_y;
		float restitution;
		float friction;
		uint32_t force_count;
		Particle_Force_Field forces[8];
		//碰撞球（半径 ≤ 0 表示关闭）
		float sphere_center[3];
		float sphere_radius;
		//地面碰撞开关（1 = 启用位置钳位 ＋ 法向反弹 ＋ 切向摩擦）
		uint32_t ground_enabled;
		//颜色覆盖（新生 / 衰老端颜色 ＋ 开关）
		float color_start[3];
		float color_end[3];
		uint32_t color_enabled;
		//软边宽度（0..1）
		float edge_softness;
		//软粒子深度档位（0 = 关闭）
		float soft_depth_range;
	};

	//单个粒子状态字节数（与 `Particle_State` 大小一致；GPU 缓冲步长）
	constexpr std::size_t PARTICLE_STRIDE = 32;
	//力场数量上限
	constexpr uint32_t PARTICLE_FORCE_MAX = 8;
	//单个力场字节数（与 GPU 力场缓冲步长一致）
	constexpr std::size_t PARTICLE_FORCE_STRIDE = 40;
	//默认粒子容量（未显式配置时）
	constexpr uint32_t PARTICLE_CAPACITY_DEFAULT = 8192;
	//单系统粒子容量上限（1 048 576；双缓冲 × 32B ≈ 64 MB）
	constexpr uint32_t PARTICLE_CAPACITY_MAX = 1048576;
	//单帧单发射器发射数量上限
	constexpr uint32_t PARTICLE_EMIT_COUNT_MAX = 65536;

	//发射器参数是否合法（形状 / 速度 / 寿命 / 数量 / 半尺寸）
	bool particle_emitter_valid(const Particle_Emitter& emitter);
	//全局模拟参数是否合法（阻尼 / 反弹 / 摩擦）
	bool particle_params_valid(const Particle_Params& params);
}
