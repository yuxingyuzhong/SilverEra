#pragma once
//预编译头
#include "Engine/EngineSystem/common/前置头文件包含.h"

/*
场景语义类型（Vulkan-free；2026-11-07 自渲染层迁入系统层）
相机 / 光照 / 雾 / 光源快照 / 场景聚合五组纯数据一律归本层：场景描述是「系统层产出的中立数据契约」，
渲染层消费它们（scene_apply 等）而不反向被本层依赖。
本头不含任何 Vulkan / VMA / SDL3 / glm 类型；渲染层在
`Engine/EngineRenderer/src/render/Scene/场景数据.h` 留转发头承接本头，故既有包含路径仍然有效。
*/
namespace engine
{
	/*
	相机组（第四十七轮拆分）
	聚合「相机位置 / 注视点 / 投影 / 清屏色」；默认值与既有默认完全一致。
	*/
	struct Camera_Data
	{
		//相机位置（世界坐标）
		float position[3] = { 0.0f, 0.0f, 3.0f };
		//相机注视点（世界坐标）
		float target[3] = { 0.0f, 0.0f, 0.0f };
		//垂直视场角（弧度；默认 60°）
		float fov = 1.0472f;
		//近裁剪平面距离
		float near_plane = 0.1f;
		//远裁剪平面距离
		float far_plane = 100.0f;
		//正交投影开关（默认关闭 = 透视）
		bool ortho = false;
		//正交投影的垂直可见高度（世界单位；仅 ortho 为真时生效）
		float ortho_height = 2.0f;
		/*
		上方向与宽高比（第一百八十六轮）
		up：相机上方向（世界坐标；默认 +Y，长度须非零，view 矩阵内部按 glm 语义处理）。
		aspect：宽高比覆盖（0 = 跟随窗口，默认；> 0 时固定该比例，供离屏 / 非窗口比例输出）。
		*/
		float up[3] = { 0.0f, 1.0f, 0.0f };
		float aspect = 0.0f;
		//清屏色（RGBA，分量取值 0..1）
		float clear_color[4] = { 0.05f, 0.05f, 0.10f, 1.0f };
	};

	/*
	光照组（第四十七轮拆分）
	聚合方向光 / 环境光 / 点光源组 / 聚光灯组 / 高光 / 逐盏衰减 / 环境光颜色；
	默认值即「满照」：方向朝 -Z、白色、强度 1、环境光 0、0 盏点 / 聚光灯。
	*/
	struct Light_Data
	{
		//光传播方向（单位向量；长度须非零，渲染层内部归一化）
		float direction[3] = { 0.0f, 0.0f, -1.0f };
		//光颜色（RGB，分量 0..1）
		float color[3] = { 1.0f, 1.0f, 1.0f };
		//光强度（≥ 0）
		float intensity = 1.0f;
		//环境光强度（0..1，直接叠加到照度）
		float ambient = 0.0f;
		//点光源数量（0..32）
		uint32_t point_count = 0;
		//点光源位置（世界坐标）
		float point_position[32][3] = {};
		//点光源颜色（RGB，0..1）
		float point_color[32][3] = {
			{ 1.0f, 1.0f, 1.0f }, { 1.0f, 1.0f, 1.0f }, { 1.0f, 1.0f, 1.0f }, { 1.0f, 1.0f, 1.0f },
		};
		//点光源线性衰减半径（> 0）
		float point_radius[32] = {
			5.0f, 5.0f, 5.0f, 5.0f, 5.0f, 5.0f, 5.0f, 5.0f,
			5.0f, 5.0f, 5.0f, 5.0f, 5.0f, 5.0f, 5.0f, 5.0f,
			5.0f, 5.0f, 5.0f, 5.0f, 5.0f, 5.0f, 5.0f, 5.0f,
			5.0f, 5.0f, 5.0f, 5.0f, 5.0f, 5.0f, 5.0f, 5.0f,
		};
		//点光源强度（≥ 0）
		float point_intensity[32] = {
			1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
			1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
			1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
			1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
		};
		//点光源逐盏衰减模式（0 跟随全局 / 1 线性 / 2 平方反比；>2 按 2）
		uint32_t point_attenuation[32] = {};
		//点光源逐盏高光缩放（第四十八轮；1 = 跟随全局，0 = 该灯不产生高光；0..100）
		float point_specular[32] = {
			1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
			1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
			1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
			1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
		};
		//点光源逐盏高光光泽度（第五十一轮；0 = 跟随全局，>0 为该灯自身光泽度 1..1000）
		float point_shininess[32] = {};
		//镜面高光强度（≥ 0；0 = 关闭）
		float specular_intensity = 0.0f;
		//镜面高光光泽度（≥ 1）
		float specular_shininess = 16.0f;
		//聚光灯数量（0..32；与点光源共用 32 盏上限）
		uint32_t spot_count = 0;
		//聚光灯位置（世界坐标）
		float spot_position[32][3] = {};
		//聚光灯传播方向（从光源指向被照区域；长度须非零）
		float spot_direction[32][3] = {
			{ 0.0f, 0.0f, -1.0f }, { 0.0f, 0.0f, -1.0f }, { 0.0f, 0.0f, -1.0f }, { 0.0f, 0.0f, -1.0f },
		};
		//聚光灯颜色（RGB，0..1）
		float spot_color[32][3] = {
			{ 1.0f, 1.0f, 1.0f }, { 1.0f, 1.0f, 1.0f }, { 1.0f, 1.0f, 1.0f }, { 1.0f, 1.0f, 1.0f },
		};
		//聚光灯线性衰减半径（> 0）
		float spot_radius[32] = {
			5.0f, 5.0f, 5.0f, 5.0f, 5.0f, 5.0f, 5.0f, 5.0f,
			5.0f, 5.0f, 5.0f, 5.0f, 5.0f, 5.0f, 5.0f, 5.0f,
			5.0f, 5.0f, 5.0f, 5.0f, 5.0f, 5.0f, 5.0f, 5.0f,
			5.0f, 5.0f, 5.0f, 5.0f, 5.0f, 5.0f, 5.0f, 5.0f,
		};
		//聚光灯强度（≥ 0）
		float spot_intensity[32] = {
			1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
			1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
			1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
			1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
		};
		//内锥角（度；须满足 0 < 内角 < 外角）
		float spot_inner_deg[32] = {
			15.0f, 15.0f, 15.0f, 15.0f, 15.0f, 15.0f, 15.0f, 15.0f,
			15.0f, 15.0f, 15.0f, 15.0f, 15.0f, 15.0f, 15.0f, 15.0f,
			15.0f, 15.0f, 15.0f, 15.0f, 15.0f, 15.0f, 15.0f, 15.0f,
			15.0f, 15.0f, 15.0f, 15.0f, 15.0f, 15.0f, 15.0f, 15.0f,
		};
		//外锥角（度；须 ≤ 90）
		float spot_outer_deg[32] = {
			30.0f, 30.0f, 30.0f, 30.0f, 30.0f, 30.0f, 30.0f, 30.0f,
			30.0f, 30.0f, 30.0f, 30.0f, 30.0f, 30.0f, 30.0f, 30.0f,
			30.0f, 30.0f, 30.0f, 30.0f, 30.0f, 30.0f, 30.0f, 30.0f,
			30.0f, 30.0f, 30.0f, 30.0f, 30.0f, 30.0f, 30.0f, 30.0f,
		};
		//聚光灯逐盏衰减模式（语义同 point_attenuation）
		uint32_t spot_attenuation[32] = {};
		//聚光灯逐盏高光缩放（第四十八轮；语义同 point_specular）
		float spot_specular[32] = {
			1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
			1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
			1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
			1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
		};
		//聚光灯逐盏高光光泽度（第五十一轮；语义同 point_shininess）
		float spot_shininess[32] = {};
		//全局距离衰减模式（0 = 线性、1 = 平方反比）
		uint32_t attenuation_mode = 0;
		//环境光颜色（RGB，0..1；默认白 = 与标量环境光等价）
		float ambient_color[3] = { 1.0f, 1.0f, 1.0f };
		/*
		半球环境光（第四十七轮）
		ambient_ground 为「朝下方向的环境光颜色」（朝上方向仍取 ambient_color）；
		ambient_hemisphere 为半球混合强度（0 = 关闭，只用 ambient_color；1 = 按法线 y 分量在两组颜色间线性插值）。
		默认 0 使既有画面逐字节不变。
		*/
		//环境光地面颜色（RGB，0..1）
		float ambient_ground[3] = { 1.0f, 1.0f, 1.0f };
		//半球混合强度（0..1；0 = 关闭）
		float ambient_hemisphere = 0.0f;
		/*
		补光（第四十七轮）
		第二盏方向光，用于主光背面补光；默认强度 0 = 关闭，画面与引入前逐字节一致。
		*/
		//补光传播方向（单位向量；长度须非零）
		float fill_direction[3] = { 0.0f, 0.0f, -1.0f };
		//补光颜色（RGB，0..1）
		float fill_color[3] = { 1.0f, 1.0f, 1.0f };
		//补光强度（≥ 0；0 = 关闭）
		float fill_intensity = 0.0f;
		//方向光逐光高光缩放（第五十轮；1 = 跟随全局，0 = 不产生高光；0..100）
		float directional_specular = 1.0f;
		//补光逐光高光缩放（第五十轮；语义同 directional_specular）
		float fill_specular = 1.0f;
		//方向光阴影开关（第五十二轮；0 = 关闭、1 = 开启）与深度偏置（0..0.05）
		int shadow_enabled = 0;
		float shadow_bias = 0.003f;
		//阴影软度（第五十三轮；0 = 单点硬边、≥0.5 = 3×3 PCF；0..1）与强度（0 = 无效、1 = 全遮蔽）
		float shadow_softness = 0.0f;
		float shadow_strength = 1.0f;
		//阴影图分辨率（第五十四轮；仅 512 / 1024 / 2048，默认 1024）
		int shadow_resolution = 1024;
		/*
		聚光灯阴影开关（第四百七十四轮）
		非 0 时复用方向光阴影图渲染「第 1 盏聚光灯」的透视阴影（单级）；
		开启期间方向光阴影让位（着色器按 cascade_params.x = 0 判聚光模式），属 v1 互斥限制。
		*/
		int spot_shadow_enabled = 0;
		/*
		聚光灯阴影质量参数（第四百七十六轮）
		与方向光阴影的 shadow_bias / shadow_softness / shadow_strength 相互独立：
		透视深度精度沿距离变化剧烈，默认偏置取 0.010（大于方向光的 0.003）；
		软度 0 为硬边、强度 1 为全遮蔽。仅在 spot_shadow_enabled 非 0 时参与着色。
		*/
		float spot_shadow_bias = 0.010f;
		float spot_shadow_softness = 0.0f;
		float spot_shadow_strength = 1.0f;
		/*
		聚光灯阴影序号（第四百七十九轮）
		指向已登记的聚光灯槽位（0..7）；阴影图只由该盏聚光投影，且只遮蔽该盏聚光灯
		（修复多盏聚光时全部被第 1 盏阴影误遮蔽的缺陷）。越界时按「无聚光阴影」处理。
		*/
		int spot_shadow_index = 0;
		/*
		多盏聚光灯阴影（第五百零七轮）
		非 0 时前 4 盏聚光灯按声明顺序各自投影（瓦片 0..3，各自独立阴影图）；
		为 0 时沿用 spot_shadow_index 的单盏语义，画面与引入前逐字节一致。
		第五百一十三轮起开启时前 8 盏聚光灯各自投影（瓦片 0..7）。
		*/
		int spot_shadow_all = 0;
		/*
		聚光灯阴影偏置自校准（第五百二十九轮）
		非 0 时逐瓦片有效偏置按「参考跨度 9 / 该瓦片跨度」缩放，使不同半径的阴影偏移接近一致；
		为 0（默认）时逐瓦片偏置恒等于 spot_shadow_bias，画面与引入前逐字节一致。
		*/
		int spot_shadow_bias_auto = 0;
		/*
		聚光阴影图集（第六百七十轮；C1）
		非 0 时 8 张聚光阴影瓦片合并为单张 4×2 图集（每瓦片 shadow_resolution 见方），
		聚光阴影 pass 写入图集子视口，着色器按瓦片号换算纹素原点；
		为 0（默认）时维持 8 张独立阴影图，画面与引入前逐字节一致。
		*/
		int spot_shadow_atlas = 0;
		//PBR-lite 镜面开关与粗糙度缩放（第六十四轮；0 = 关闭走 Blinn-Phong，缩放 0.05..4）
		int pbr_enabled = 0;
		float pbr_roughness = 1.0f;
		//PBR 金属度（第六十五轮；0 = 介电、1 = 纯金属）
		float pbr_metallic = 0.0f;
		/*
		PBR 掠射高光能量修正（第六百七十二轮；D1 第一步）
		非 0 且 PBR 开启时，镜面项乘入射余弦 NoL（等价 D·G·F/(4·NoV)），修正掠射角过亮；
		为 0（默认）时乘子恒 1，画面与引入前逐字节一致。
		*/
		int pbr_energy = 0;
		/*
		PBR 粗糙度绝对口径（第七百轮；D1 收尾）
		非 0 且 PBR 开启时，材质 / 全局粗糙度值直接作为 GGX 粗糙度（不再由镜面光泽度换算），
		使金属度-粗糙度与 Blinn 光泽度解耦；为 0（默认）时沿用旧换算，画面逐字节不变。
		*/
		int pbr_roughness_absolute = 0;
		/*
		二次元风格化（第一百七十四轮）
		toon_steps：光照色阶数（0 = 关闭，原样光照；1..8 = 把各光源 N·L 量化到 N 档）；
		toon_rim：菲涅尔边缘光强度（0 = 关闭；0..1）。
		默认 0 使画面与引入前逐字节不变。
		*/
		uint32_t toon_steps = 0;
		float toon_rim = 0.0f;
		/*
		二次元风格化 · 暗部色调化（第一百七十八轮）
		toon_shadow_tint：暗部（主方向光量化受光系数低于 0.5）乘用的色调（RGB，0..1）；
		默认 (1,1,1) 时乘性恒等，画面与引入前逐字节不变。
		*/
		float toon_shadow_tint[3] = { 1.0f, 1.0f, 1.0f };
		/*
		二次元风格化 · 卡通高光（第一百七十九轮）
		toon_specular：高光二值化阈值（0 = 关闭；0..1）；
		开启后高光亮度低于阈值处整块归零、高于阈值处保留原值，形成硬边高光块。
		*/
		float toon_specular = 0.0f;
		/*
		二次元风格化 · 反向外扩描边（第二百二十八轮）
		outline_width：沿顶点法线的世界单位外扩宽度（0 = 关闭；0..1）；
		outline_color：描边色（RGB，0..1）。默认宽度 0 使画面与引入前逐字节不变。
		*/
		float outline_width = 0.0f;
		float outline_color[3] = { 0.0f, 0.0f, 0.0f };
		/*
		光源选择模式与预算（第六百四十五轮；A1）
		light_select_mode：0 = 声明顺序（默认，与引入前逐字节一致）；1 = 重要性选择
		（候选数超过预算时按「强度 × 半径 /(半径 ＋ 相机距离) × 锥角可见性」择优，同分同序）。
		light_budget：参与前向光照的最大光源数（0 = 跟随路径上限（旧前向 8 / 簇路径全量））。
		命中集合确定后仍按声明顺序登记，故聚光瓦片按槽位映射的口径不变。
		*/
		int light_select_mode = 0;
		uint32_t light_budget = 0;
		/*
		逐盏聚光灯阴影开关（第六百四十八轮；按前向槽位 0..7）
		1（默认）= 该盏聚光灯参与阴影；0 = 该盏不投影（不影响其余盏与方向光阴影）。
		*/
		int spot_shadow_on[8] = { 1, 1, 1, 1, 1, 1, 1, 1 };
		/*
		级联接缝淡化比例（第六百五十一轮；0..0.5）
		0（默认）= 硬切换，与引入前逐字节一致；>0 时在切换带内与上一级混合。
		*/
		float shadow_cascade_blend = 0.0f;
		/*
		级联阴影配置（第六百五十四轮；与门面 shadow_cascade_set 同义）
		默认（1 级 / 远区倍数 3 / 切换距离 12）与引入该字段前一致；仅在数值变化时才重设（避免逐帧重建阴影图）。
		*/
		int shadow_cascade_count = 1;
		float shadow_cascade_far_scale = 3.0f;
		float shadow_cascade_split = 12.0f;
		/*
		PCSS 软阴影（第六百五十七轮；默认关闭）
		shadow_pcss：0 = 关闭（旧硬边 / 3×3 软度口径，逐字节一致）、1 = 开启；
		shadow_pcss_search：阻塞物搜索尺度（纹素，钳到 1..32）；
		shadow_pcss_max：PCF 半径上限（纹素，钳到 1..16）。
		*/
		int shadow_pcss = 0;
		float shadow_pcss_search = 24.0f;
		float shadow_pcss_max = 8.0f;
		/*
		逐盏聚光灯阴影偏置缩放（第六百五十八轮；按前向槽位 0..7）
		1（默认）= 沿用既有偏置口径；>1 加大偏置（抑制自阴影），<1 减小偏置（收紧接触阴影）；
		有效偏置 = 既有逐瓦片偏置 × 该缩放（钳到 [0.1, 10]）。
		*/
		float spot_shadow_bias_scale[8] = { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f };
		/*
		逐盏聚光灯阴影质量缩放（第七百零八轮；按前向槽位 0..7）
		spot_shadow_softness_scale：软度缩放（0.25..4，默认 1 = 跟随全局软度）；
		spot_shadow_strength_scale：强度缩放（0..1，默认 1 = 跟随全局强度）。
		有效软度 = 全局软度 × 该缩放；有效强度 = 全局强度 × 该缩放（钳到 0..1）。
		分辨率仍由全局 shadow_resolution / shadow_budget_bytes 决定（图集各瓦片尺寸一致，不可逐盏分档）。
		*/
		float spot_shadow_softness_scale[8] = { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f };
		float spot_shadow_strength_scale[8] = { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f };
		/*
		逐盏聚光灯阴影优先级（第七百零九轮；按光源槽位 0..31）
		仅在「多盏聚光阴影开启且可投影聚光灯超过 8 盏」时生效：按优先级降序、同分按槽位升序取前 8 盏分配阴影瓦片。
		默认 0（全部同分）→ 仍取登记顺序最前的 8 盏，与引入该字段前逐字节一致。
		*/
		float spot_shadow_priority[32] = {};
		/*
		阴影分辨率预算（第六百五十九轮；0 = 关闭（默认，不干预既有分辨率））
		总字节按 RGBA8（4 字节/像素）折算：边长 = floor(√(预算 /(单位数 × 4)))，向下取 256 倍数并钳 256..4096，
		再取不超过该值的最大受支持档（512 / 1024 / 2048）；结果与请求分辨率取较小值。
		单位数 = 方向光级联数 ＋ 聚光瓦片数。
		*/
		uint32_t shadow_budget_bytes = 0;
		/*
		曝光补偿（第六百六十轮；EV 值，默认 0 = 关闭）
		最终颜色按 2^EV 缩放；EV = 0 时乘性恒等（画面逐字节不变）；钳到 [-4, 4]。
		*/
		float exposure = 0.0f;
		/*
		阴影纹素尺寸上限（第六百六十一轮；世界单位 / 纹素，0 = 关闭（默认））
		>0 时每片级联半径按「上限 × 分片边长 / 2」收紧：近处纹素不再过粗，
		代价是覆盖面缩小（半径外的片元按现有口径视为受光）；关闭时矩阵与引入前逐字节一致。
		*/
		float shadow_texel_max = 0.0f;
		/*
		级联切换带自动淡化（第六百六十一轮；0 = 关闭（默认））
		开启且显式淡化比例为 0、级联数 ≥ 2 时，按「4 / 分片边长」（切换带半宽 ≥ 2 纹素）
		自动给出建议比例；显式比例 > 0 时以显式值为准；关闭时行为与引入前逐字节一致。
		*/
		int shadow_cascade_blend_auto = 0;
		/*
		阴影批量提交（第六百六十九轮；0 = 关闭（默认））
		开启且本帧阴影段数 > 1（级联片 ＋ 聚光瓦片）时，全部段在**一次队列提交**内完成；
		关闭时逐段提交，行为与引入前逐字节一致。
		*/
		int shadow_batch_submit = 0;
	};

	/*
	点光源状态快照（第九十二轮；Vulkan-free，只读查询用）
	与门面 `point_light_state_get` 配套：由内部逐盏记录导出，供上层诊断 / 回显。
	*/
	struct Point_Light_State
	{
		//位置（世界坐标）
		float position[3] = { 0.0f, 0.0f, 0.0f };
		//颜色（RGB，0..1）
		float color[3] = { 1.0f, 1.0f, 1.0f };
		//衰减半径（> 0）
		float radius = 5.0f;
		//强度（≥ 0）
		float intensity = 1.0f;
		//逐光源衰减模式（0 = 跟随全局、1 = 线性、2 = 平方反比）
		uint32_t attenuation = 0;
		//逐盏高光缩放（1 = 跟随全局，0 = 不产生高光）
		float specular_scale = 1.0f;
		//逐盏光泽度（0 = 跟随全局）
		float shininess = 0.0f;
		//是否聚光灯
		bool is_spot = false;
		//光传播方向（单位向量；仅聚光灯有效）
		float spot_direction[3] = { 0.0f, 0.0f, -1.0f };
		//内 / 外锥角余弦（仅聚光灯有效）
		float spot_inner_cos = 1.0f;
		float spot_outer_cos = 0.0f;
	};

	/*
	雾组（第四十七轮拆分）
	默认密度 0（关闭），画面与引入雾前一致。
	*/
	struct Fog_Data
	{
		//雾色（RGB，0..1）
		float color[3] = { 0.05f, 0.05f, 0.10f };
		//雾强度（0..1；0 = 关闭）
		float density = 0.0f;
		//雾起始距离（世界单位；≥ 0）
		float start = 1.0f;
		//雾结束距离（世界单位；须大于起始；仅线性模式使用）
		float end = 10.0f;
		/*
		雾模式（第一百四十三轮）
		0 = 线性区间（默认，按 [start, end] 归一化后取 t² × density）；
		1 = 指数平方（factor = 1 − exp(−(density × (距离 − start))²)）。
		*/
		uint32_t mode = 0;
		/*
		高度衰减系数（第一百四十三轮；0 = 关闭）
		雾因子按 `exp(−height_falloff × max(世界 y, 0))` 再缩放：越高越稀薄，地面附近不受影响。
		*/
		float height_falloff = 0.0f;
	};

	/*
	场景聚合（Vulkan-free；兼容入口）
	保留原先的扁平字段布局，供既有调用方继续使用；新代码建议直接使用
	Camera_Data / Light_Data / Fog_Data 三组，经 scene_apply(camera, light, fog) 提交
	（与扁平 Scene_Data 版本等价）。
	*/
	struct Scene_Data
	{
		//相机位置（世界坐标）
		float position[3] = { 0.0f, 0.0f, 3.0f };
		//相机注视点（世界坐标）
		float target[3] = { 0.0f, 0.0f, 0.0f };
		//垂直视场角（弧度；默认 60°）
		float fov = 1.0472f;
		//近裁剪平面距离
		float near_plane = 0.1f;
		//远裁剪平面距离
		float far_plane = 100.0f;
		/*
		正交投影开关（默认关闭 = 透视，与既有默认一致）
		开启后「垂直可见高度」由 ortho_height 决定，与相机距离无关（近大远小消失）。
		*/
		bool ortho = false;
		//正交投影的垂直可见高度（世界单位；仅 ortho 为真时生效；须为正）
		float ortho_height = 2.0f;
		//上方向与宽高比覆盖（第一百八十六轮；语义见 Camera_Data::up / aspect）
		float up[3] = { 0.0f, 1.0f, 0.0f };
		float aspect = 0.0f;
		//清屏色（RGBA，分量取值 0..1）
		float clear_color[4] = { 0.05f, 0.05f, 0.10f, 1.0f };
		/*
		方向光与环境光（第二十九轮新增）
		默认值即「满照」：方向朝 -Z、白色、强度 1、环境光 0 —— 未提供法线的网格照度为 1，
		故默认场景画面与引入光照前逐通道一致。
		*/
		//光传播方向（单位向量；长度须非零，渲染层内部归一化）
		float light_direction[3] = { 0.0f, 0.0f, -1.0f };
		//光颜色（RGB，分量 0..1）
		float light_color[3] = { 1.0f, 1.0f, 1.0f };
		//光强度（≥ 0）
		float light_intensity = 1.0f;
		//环境光（0..1，直接叠加到照度）
		float light_ambient = 0.0f;
		/*
		点光源组与镜面高光（第三十二 / 三十三轮）
		默认 0 盏点光源、高光关闭 —— 与引入点光源前画面一致。
		*/
		//点光源数量（0..32；超出 32 的部分由 scene_apply 忽略并告警）
		uint32_t light_count = 0;
		//点光源位置（世界坐标；仅前 light_count 项有效）
		float point_light_position[32][3] = {};
		//点光源颜色（RGB，0..1）
		float point_light_color[32][3] = {
			{ 1.0f, 1.0f, 1.0f }, { 1.0f, 1.0f, 1.0f }, { 1.0f, 1.0f, 1.0f }, { 1.0f, 1.0f, 1.0f },
		};
		//点光源线性衰减半径（> 0）
		float point_light_radius[32] = {
			5.0f, 5.0f, 5.0f, 5.0f, 5.0f, 5.0f, 5.0f, 5.0f,
			5.0f, 5.0f, 5.0f, 5.0f, 5.0f, 5.0f, 5.0f, 5.0f,
			5.0f, 5.0f, 5.0f, 5.0f, 5.0f, 5.0f, 5.0f, 5.0f,
			5.0f, 5.0f, 5.0f, 5.0f, 5.0f, 5.0f, 5.0f, 5.0f,
		};
		//点光源强度（≥ 0）
		float point_light_intensity[32] = {
			1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
			1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
			1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
			1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
		};
		/*
		点光源逐盏衰减模式（第四十六轮）
		0 = 跟随全局（attenuation_mode）、1 = 线性、2 = 平方反比；其它取值按 2 处理。
		*/
		uint32_t point_light_attenuation[32] = {};
		//点光源逐盏高光缩放（第四十八轮；语义见 Light_Data::point_specular）
		float point_light_specular[32] = {
			1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
			1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
			1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
			1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
		};
		//点光源逐盏高光光泽度（第五十一轮；语义见 Light_Data::point_shininess）
		float point_light_shininess[32] = {};
		//镜面高光强度（≥ 0；0 = 关闭）
		float specular_intensity = 0.0f;
		//镜面高光光泽度（≥ 1）
		float specular_shininess = 16.0f;
		/*
		聚光灯组（第三十五轮）
		与点光源组共用 32 盏上限（第七百零一轮由 16 扩为 32）：`scene_apply` 先登记点光源、再登记聚光灯，超出部分忽略；
		默认 0 盏 —— 与引入聚光灯前画面一致。
		*/
		//聚光灯数量（0..32）
		uint32_t spot_light_count = 0;
		//聚光灯位置（世界坐标；仅前 spot_light_count 项有效）
		float spot_light_position[32][3] = {};
		//聚光灯传播方向（从光源指向被照区域；长度须非零）
		float spot_light_direction[32][3] = {
			{ 0.0f, 0.0f, -1.0f }, { 0.0f, 0.0f, -1.0f }, { 0.0f, 0.0f, -1.0f }, { 0.0f, 0.0f, -1.0f },
		};
		//聚光灯颜色（RGB，0..1）
		float spot_light_color[32][3] = {
			{ 1.0f, 1.0f, 1.0f }, { 1.0f, 1.0f, 1.0f }, { 1.0f, 1.0f, 1.0f }, { 1.0f, 1.0f, 1.0f },
		};
		//聚光灯线性衰减半径（> 0）
		float spot_light_radius[32] = {
			5.0f, 5.0f, 5.0f, 5.0f, 5.0f, 5.0f, 5.0f, 5.0f,
			5.0f, 5.0f, 5.0f, 5.0f, 5.0f, 5.0f, 5.0f, 5.0f,
			5.0f, 5.0f, 5.0f, 5.0f, 5.0f, 5.0f, 5.0f, 5.0f,
			5.0f, 5.0f, 5.0f, 5.0f, 5.0f, 5.0f, 5.0f, 5.0f,
		};
		//聚光灯强度（≥ 0）
		float spot_light_intensity[32] = {
			1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
			1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
			1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
			1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
		};
		//内锥角（度；须满足 0 < 内角 < 外角）
		float spot_light_inner_deg[32] = {
			15.0f, 15.0f, 15.0f, 15.0f, 15.0f, 15.0f, 15.0f, 15.0f,
			15.0f, 15.0f, 15.0f, 15.0f, 15.0f, 15.0f, 15.0f, 15.0f,
			15.0f, 15.0f, 15.0f, 15.0f, 15.0f, 15.0f, 15.0f, 15.0f,
			15.0f, 15.0f, 15.0f, 15.0f, 15.0f, 15.0f, 15.0f, 15.0f,
		};
		//外锥角（度；须 ≤ 90）
		float spot_light_outer_deg[32] = {
			30.0f, 30.0f, 30.0f, 30.0f, 30.0f, 30.0f, 30.0f, 30.0f,
			30.0f, 30.0f, 30.0f, 30.0f, 30.0f, 30.0f, 30.0f, 30.0f,
			30.0f, 30.0f, 30.0f, 30.0f, 30.0f, 30.0f, 30.0f, 30.0f,
			30.0f, 30.0f, 30.0f, 30.0f, 30.0f, 30.0f, 30.0f, 30.0f,
		};
		//聚光灯逐盏衰减模式（第四十六轮；语义同 point_light_attenuation）
		uint32_t spot_light_attenuation[32] = {};
		//聚光灯逐盏高光缩放（第四十八轮；语义同 point_light_specular）
		float spot_light_specular[32] = {
			1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
			1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
			1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
			1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
		};
		//聚光灯逐盏高光光泽度（第五十一轮；语义同 point_light_shininess）
		float spot_light_shininess[32] = {};
		/*
		距离衰减模式（第三十七轮）
		0 = 线性（默认）、1 = 平方反比；其它取值由 scene_apply 忽略并告警（保持门面当前模式）。
		*/
		uint32_t attenuation_mode = 0;
		/*
		距离雾（第三十九轮）
		默认密度 0（关闭），画面与引入雾前一致；开启后按「相机 → 片元」距离在 [起始, 结束] 间混入雾色。
		*/
		//雾色（RGB，0..1）
		float fog_color[3] = { 0.05f, 0.05f, 0.10f };
		//雾强度（0..1；0 = 关闭）
		float fog_density = 0.0f;
		//雾起始距离（世界单位；≥ 0）
		float fog_start = 1.0f;
		//雾结束距离（世界单位；须大于起始）
		float fog_end = 10.0f;
		//雾模式（第一百四十三轮；0 = 线性区间、1 = 指数平方；语义见 Fog_Data::mode）
		uint32_t fog_mode = 0;
		//雾高度衰减系数（第一百四十三轮；语义见 Fog_Data::height_falloff）
		float fog_height_falloff = 0.0f;
		//环境光颜色（RGB，0..1；第四十一轮；默认白 = 与标量环境光等价）
		float ambient_color[3] = { 1.0f, 1.0f, 1.0f };
		//环境光地面颜色与半球混合强度（第四十七轮；语义见 Light_Data::ambient_ground / ambient_hemisphere）
		float ambient_ground[3] = { 1.0f, 1.0f, 1.0f };
		float ambient_hemisphere = 0.0f;
		//补光（第四十七轮；语义见 Light_Data::fill_direction / fill_color / fill_intensity）
		float fill_direction[3] = { 0.0f, 0.0f, -1.0f };
		float fill_color[3] = { 1.0f, 1.0f, 1.0f };
		float fill_intensity = 0.0f;
		//方向光 / 补光的逐光高光缩放（第五十轮；语义见 Light_Data::directional_specular / fill_specular）
		float directional_specular = 1.0f;
		float fill_specular = 1.0f;
		//方向光阴影开关与深度偏置（第五十二轮；语义见 Light_Data::shadow_enabled / shadow_bias）
		int shadow_enabled = 0;
		float shadow_bias = 0.003f;
		//阴影软度与强度（第五十三轮；语义见 Light_Data::shadow_softness / shadow_strength）
		float shadow_softness = 0.0f;
		float shadow_strength = 1.0f;
		//阴影图分辨率（第五十四轮；语义见 Light_Data::shadow_resolution）
		int shadow_resolution = 1024;
		//PBR-lite 镜面开关与粗糙度缩放（第六十四轮；语义见 Light_Data::pbr_enabled / pbr_roughness）
		int pbr_enabled = 0;
		float pbr_roughness = 1.0f;
		//PBR 金属度（第六十五轮；语义见 Light_Data::pbr_metallic）
		float pbr_metallic = 0.0f;
		//二次元风格化（第一百七十四轮；语义见 Light_Data::toon_steps / toon_rim）
		uint32_t toon_steps = 0;
		float toon_rim = 0.0f;
		//二次元暗部色调（第一百七十八轮；语义见 Light_Data::toon_shadow_tint）
		float toon_shadow_tint[3] = { 1.0f, 1.0f, 1.0f };
		//二次元卡通高光阈值（第一百七十九轮；语义见 Light_Data::toon_specular）
		float toon_specular = 0.0f;
		//二次元反向外扩描边宽度与描边色（第二百二十八轮；语义见 Light_Data::outline_width）
		float outline_width = 0.0f;
		float outline_color[3] = { 0.0f, 0.0f, 0.0f };
		//光源选择模式与预算（第六百四十五轮；语义见 Light_Data::light_select_mode / light_budget）
		int light_select_mode = 0;
		uint32_t light_budget = 0;
		//逐盏聚光灯阴影开关（第六百四十八轮；语义见 Light_Data::spot_shadow_on）
		int spot_shadow_on[8] = { 1, 1, 1, 1, 1, 1, 1, 1 };
		//级联接缝淡化比例（第六百五十一轮；语义见 Light_Data::shadow_cascade_blend）
		float shadow_cascade_blend = 0.0f;
		//级联阴影配置（第六百五十四轮；语义见 Light_Data::shadow_cascade_count / far_scale / split）
		int shadow_cascade_count = 1;
		float shadow_cascade_far_scale = 3.0f;
		float shadow_cascade_split = 12.0f;
		//PCSS 软阴影（第六百五十七轮；语义见 Light_Data::shadow_pcss / search / max）
		int shadow_pcss = 0;
		float shadow_pcss_search = 24.0f;
		float shadow_pcss_max = 8.0f;
		//逐盏聚光灯阴影偏置缩放（第六百五十八轮；语义见 Light_Data::spot_shadow_bias_scale）
		float spot_shadow_bias_scale[8] = { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f };
		//逐盏聚光灯阴影质量缩放（第七百零八轮；语义见 Light_Data::spot_shadow_softness_scale / strength_scale）
		float spot_shadow_softness_scale[8] = { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f };
		float spot_shadow_strength_scale[8] = { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f };
		//逐盏聚光灯阴影优先级（第七百零九轮；语义见 Light_Data::spot_shadow_priority）
		float spot_shadow_priority[32] = {};
		uint32_t shadow_budget_bytes = 0;
		//曝光补偿（第六百六十轮；语义见 Light_Data::exposure）
		float exposure = 0.0f;
		//阴影纹素尺寸上限与级联自动淡化（第六百六十一轮；语义见 Light_Data 同名成员）
		float shadow_texel_max = 0.0f;
		int shadow_cascade_blend_auto = 0;
		//阴影批量提交（第六百六十九轮；语义见 Light_Data::shadow_batch_submit）
		int shadow_batch_submit = 0;
		//聚光阴影图集（第六百七十轮；语义见 Light_Data::spot_shadow_atlas）
		int spot_shadow_atlas = 0;
		//PBR 掠射高光能量修正（第六百七十二轮；语义见 Light_Data::pbr_energy）
		int pbr_energy = 0;
		//PBR 粗糙度绝对口径（第七百轮；语义见 Light_Data::pbr_roughness_absolute）
		int pbr_roughness_absolute = 0;
	};
}
