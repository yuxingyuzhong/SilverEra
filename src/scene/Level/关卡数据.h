#pragma once
//预编译头
#include "Engine/EngineSystem/common/前置头文件包含.h"

/*
关卡数据（第四百二十九轮迁入系统层；方向六阶段 A）
本模块把「关卡文本 ↔ 关卡数据」的读写职责从渲染层下放到系统层：
渲染层只保留 Camera_Data / Light_Data / Fog_Data ＋ scene_apply 这套纯数据消费接口，
关卡数据本身在此以中立字段表达（不引用渲染层任何类型），由应用层逐字段映射后提交渲染层。

字段与默认值逐字段对齐渲染层 Camera_Data / Light_Data / Fog_Data，
故「文本 → 关卡数据 → 渲染层数据」三段之间不做任何语义换算，往返与既有画面逐位一致。
本模块全部结构为纯数据（图形 API-free），不持有任何 GPU 句柄。
*/
namespace engine
{
	/*
	关卡相机组
	聚合「相机位置 / 注视点 / 投影 / 清屏色」；默认值与渲染层 Camera_Data 完全一致。
	*/
	struct Level_Camera
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
		//上方向（世界坐标；默认 +Y，长度须非零）
		float up[3] = { 0.0f, 1.0f, 0.0f };
		//宽高比覆盖（0 = 跟随窗口，默认；> 0 时固定该比例）
		float aspect = 0.0f;
		//清屏色（RGBA，分量取值 0..1）
		float clear_color[4] = { 0.05f, 0.05f, 0.10f, 1.0f };
	};

	/*
	关卡光照组
	聚合方向光 / 环境光 / 点光源组 / 聚光灯组 / 高光 / 逐盏衰减 / 环境光颜色 / 风格化参数；
	默认值即「满照」：方向朝 -Z、白色、强度 1、环境光 0、0 盏点 / 聚光灯。
	字段顺序与渲染层 Light_Data 一致（写出前校验按连续区间取样，顺序不可打乱）。
	*/
	struct Level_Light
	{
		//光传播方向（单位向量；长度须非零，渲染层内部归一化）
		float direction[3] = { 0.0f, 0.0f, -1.0f };
		//光颜色（RGB，分量 0..1）
		float color[3] = { 1.0f, 1.0f, 1.0f };
		//光强度（≥ 0）
		float intensity = 1.0f;
		//环境光强度（0..1，直接叠加到照度）
		float ambient = 0.0f;
		//点光源数量（0..8）
		uint32_t point_count = 0;
		//点光源位置（世界坐标）
		float point_position[8][3] = {};
		//点光源颜色（RGB，0..1）
		float point_color[8][3] = {
			{ 1.0f, 1.0f, 1.0f }, { 1.0f, 1.0f, 1.0f }, { 1.0f, 1.0f, 1.0f }, { 1.0f, 1.0f, 1.0f },
		};
		//点光源线性衰减半径（> 0）
		float point_radius[8] = { 5.0f, 5.0f, 5.0f, 5.0f, 5.0f, 5.0f, 5.0f, 5.0f };
		//点光源强度（≥ 0）
		float point_intensity[8] = { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f };
		//点光源逐盏衰减模式（0 跟随全局 / 1 线性 / 2 平方反比；>2 按 2）
		uint32_t point_attenuation[8] = {};
		//点光源逐盏高光缩放（1 = 跟随全局，0 = 该灯不产生高光；0..100）
		float point_specular[8] = { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f };
		//点光源逐盏高光光泽度（0 = 跟随全局，>0 为该灯自身光泽度 1..1000）
		float point_shininess[8] = {};
		//镜面高光强度（≥ 0；0 = 关闭）
		float specular_intensity = 0.0f;
		//镜面高光光泽度（≥ 1）
		float specular_shininess = 16.0f;
		//聚光灯数量（0..8；与点光源共用 8 盏上限）
		uint32_t spot_count = 0;
		//聚光灯位置（世界坐标）
		float spot_position[8][3] = {};
		//聚光灯传播方向（从光源指向被照区域；长度须非零）
		float spot_direction[8][3] = {
			{ 0.0f, 0.0f, -1.0f }, { 0.0f, 0.0f, -1.0f }, { 0.0f, 0.0f, -1.0f }, { 0.0f, 0.0f, -1.0f },
		};
		//聚光灯颜色（RGB，0..1）
		float spot_color[8][3] = {
			{ 1.0f, 1.0f, 1.0f }, { 1.0f, 1.0f, 1.0f }, { 1.0f, 1.0f, 1.0f }, { 1.0f, 1.0f, 1.0f },
		};
		//聚光灯线性衰减半径（> 0）
		float spot_radius[8] = { 5.0f, 5.0f, 5.0f, 5.0f, 5.0f, 5.0f, 5.0f, 5.0f };
		//聚光灯强度（≥ 0）
		float spot_intensity[8] = { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f };
		//内锥角（度；须满足 0 < 内角 < 外角）
		float spot_inner_deg[8] = { 15.0f, 15.0f, 15.0f, 15.0f, 15.0f, 15.0f, 15.0f, 15.0f };
		//外锥角（度；须 ≤ 90）
		float spot_outer_deg[8] = { 30.0f, 30.0f, 30.0f, 30.0f, 30.0f, 30.0f, 30.0f, 30.0f };
		//聚光灯逐盏衰减模式（语义同 point_attenuation）
		uint32_t spot_attenuation[8] = {};
		//聚光灯逐盏高光缩放（语义同 point_specular）
		float spot_specular[8] = { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f };
		//聚光灯逐盏高光光泽度（语义同 point_shininess）
		float spot_shininess[8] = {};
		//全局距离衰减模式（0 = 线性、1 = 平方反比）
		uint32_t attenuation_mode = 0;
		//环境光颜色（RGB，0..1；默认白 = 与标量环境光等价）
		float ambient_color[3] = { 1.0f, 1.0f, 1.0f };
		//环境光地面颜色（RGB，0..1）
		float ambient_ground[3] = { 1.0f, 1.0f, 1.0f };
		//半球混合强度（0..1；0 = 关闭）
		float ambient_hemisphere = 0.0f;
		//补光传播方向（单位向量；长度须非零）
		float fill_direction[3] = { 0.0f, 0.0f, -1.0f };
		//补光颜色（RGB，0..1）
		float fill_color[3] = { 1.0f, 1.0f, 1.0f };
		//补光强度（≥ 0；0 = 关闭）
		float fill_intensity = 0.0f;
		//方向光逐光高光缩放（1 = 跟随全局，0 = 不产生高光；0..100）
		float directional_specular = 1.0f;
		//补光逐光高光缩放（语义同 directional_specular）
		float fill_specular = 1.0f;
		//方向光阴影开关（0 = 关闭、1 = 开启）
		int shadow_enabled = 0;
		//方向光阴影深度偏置（0..0.05）
		float shadow_bias = 0.003f;
		//阴影软度（0 = 单点硬边、≥0.5 = 3×3 PCF；0..1）
		float shadow_softness = 0.0f;
		//阴影强度（0 = 无效、1 = 全遮蔽）
		float shadow_strength = 1.0f;
		//阴影图分辨率（仅 512 / 1024 / 2048，默认 1024）
		int shadow_resolution = 1024;
		//聚光灯阴影开关（非 0 时投射聚光灯阴影）
		int spot_shadow_enabled = 0;
		//聚光灯阴影深度偏置（0..0.05）
		float spot_shadow_bias = 0.010f;
		//聚光灯阴影软度（0..1）
		float spot_shadow_softness = 0.0f;
		//聚光灯阴影强度（0..1）
		float spot_shadow_strength = 1.0f;
		//聚光灯阴影序号（指向已登记聚光灯槽位 0..7）
		int spot_shadow_index = 0;
		//多盏聚光灯阴影开关（非 0 时前 8 盏聚光灯各自投影）
		int spot_shadow_all = 0;
		//聚光灯阴影偏置自校准开关（非 0 时逐瓦片按跨度归一化）
		int spot_shadow_bias_auto = 0;
		//PBR-lite 镜面开关（0 = 关闭走 Blinn-Phong）
		int pbr_enabled = 0;
		//PBR 粗糙度缩放（0.05..4）
		float pbr_roughness = 1.0f;
		//PBR 金属度（0 = 介电、1 = 纯金属）
		float pbr_metallic = 0.0f;
		//光照色阶数（0 = 关闭，1..8 = 量化档数）
		uint32_t toon_steps = 0;
		//菲涅尔边缘光强度（0 = 关闭；0..1）
		float toon_rim = 0.0f;
		//暗部色调（RGB，0..1；默认 (1,1,1) 乘性恒等）
		float toon_shadow_tint[3] = { 1.0f, 1.0f, 1.0f };
		//卡通高光二值化阈值（0 = 关闭；0..1）
		float toon_specular = 0.0f;
		//反向外扩描边宽度（0 = 关闭；0..1）
		float outline_width = 0.0f;
		//描边色（RGB，0..1）
		float outline_color[3] = { 0.0f, 0.0f, 0.0f };
	};

	/*
	关卡雾组
	默认密度 0（关闭），画面与引入雾前一致。
	*/
	struct Level_Fog
	{
		//雾色（RGB，0..1）
		float color[3] = { 0.05f, 0.05f, 0.10f };
		//雾强度（0..1；0 = 关闭）
		float density = 0.0f;
		//雾起始距离（世界单位；≥ 0）
		float start = 1.0f;
		//雾结束距离（世界单位；须大于起始；仅线性模式使用）
		float end = 10.0f;
		//雾模式（0 = 线性区间、1 = 指数平方）
		uint32_t mode = 0;
		//高度衰减系数（0 = 关闭）
		float height_falloff = 0.0f;
	};

	/*
	材质实例声明（第四百六十六轮；方向七阶段 B）
	关卡文本的 `material` 行：实例名 ＋ 模板材质名 ＋ 可选 tint / emissive / toon / pbr / clearcoat 覆盖。
	只保存声明（图形 API-free），不持有句柄；装配阶段由应用层按实例名查表。
	*/
	struct Level_Material
	{
		//实例名（place 行材质列引用；同一关卡内唯一）
		std::string 实例名;
		//模板材质名（引用 `mtl` 行里的材质名）
		std::string 模板;
		//是否覆盖底色
		bool use_tint = false;
		//覆盖底色（RGBA；use_tint 为假时无意义）
		float tint[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
		//是否覆盖自发光强度
		bool use_emissive = false;
		//覆盖自发光强度（use_emissive 为假时无意义）
		float emissive = 0.0f;
		//是否覆盖卡通参数
		bool use_toon = false;
		//色阶数（0..8）
		float toon_steps = 0.0f;
		//边缘光强度（0..1）
		float toon_rim = 0.0f;
		//卡通高光阈值（0..1）
		float toon_specular = 0.0f;
		//是否覆盖 PBR 参数（覆盖即启用 PBR）
		bool use_pbr = false;
		//粗糙度（0.05, 4]
		float pbr_roughness = 1.0f;
		//金属度（0..1）
		float pbr_metallic = 0.0f;
		//是否覆盖清漆参数（覆盖即启用清漆层）
		bool use_clearcoat = false;
		//清漆强度（0, 1]
		float clear_coat = 0.0f;
		//清漆粗糙度（0.03..1）
		float clear_coat_roughness = 0.15f;
	};

	/*
	关卡数据（第四百二十九轮；方向六阶段 A）
	聚合「关卡的相机 / 光照 / 雾 / 资产摆放」；资产段落以清单原文保存（`mesh` / `mtl` / `place` 行），
	装载时交给场景清单链路。本结构为纯数据，不持有任何 GPU 句柄。
	*/
	struct Level_Data
	{
		//关卡名（可为空；`level` 记录取行尾全部文本）
		std::string name;
		//格式版本（当前只接受 1，缺省行按 1）
		int version = 1;
		//相机组
		Level_Camera camera;
		//光照组
		Level_Light light;
		//雾组
		Level_Fog fog;
		//资产声明行（`mesh` / `mtl` / `place`，已去首尾空白的原文）
		std::vector<std::string> asset_lines;
		//材质实例声明（按文本出现顺序，旧文件为空）
		std::vector<Level_Material> materials;
	};
}
