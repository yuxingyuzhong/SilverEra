#pragma once
//预编译头
#include "Engine/EngineSystem/common/前置头文件包含.h"
/*
相机 / 光照 / 雾三组：使用本层自有的纯数据类型
`Camera_Data / Light_Data / Fog_Data` 已随 `场景数据.h` 一并归入本层（2026-11-07），
关卡数据直接持有这三组，不经任何渲染层头文件；渲染层消费这批类型而本层不反向依赖它。
*/
#include "场景数据.h"

/*
关卡数据（第四百二十九轮迁入系统层；方向六阶段 A）
本模块把「关卡文本 ↔ 关卡数据」的读写职责从渲染层下放到系统层：
渲染层只保留 `scene_apply` 这套纯数据消费接口，关卡数据本身在此聚合
相机 / 光照 / 雾三组 ＋ 资产声明 ＋ 材质实例声明。
本模块全部结构为纯数据（图形 API-free），不持有任何 GPU 句柄。
*/
namespace engine
{
	/*
	材质实例声明（第四百六十六轮；方向七阶段 B）
	关卡文本的 `material` 行：实例名 ＋ 模板材质名 ＋ 可选 tint / emissive / toon / pbr / clearcoat 覆盖。
	只保存声明（图形 API-free），不持有句柄；装配阶段由场景装配模块按实例名查表。
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
		//相机组（直接复用渲染层纯数据）
		Camera_Data camera;
		//光照组（直接复用渲染层纯数据）
		Light_Data light;
		//雾组（直接复用渲染层纯数据）
		Fog_Data fog;
		//资产声明行（`mesh` / `mtl` / `place`，已去首尾空白的原文）
		std::vector<std::string> asset_lines;
		//材质实例声明（按文本出现顺序，旧文件为空）
		std::vector<Level_Material> materials;
	};
}
