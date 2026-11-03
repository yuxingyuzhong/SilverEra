#pragma once
//预编译头
#include "Application/Renderer/common/前置头文件包含.h"
//渲染器门面（Vulkan-free 公共面：资源创建 / 纯数据场景提交 / 绘制登记）
#include "Application/Renderer/src/render/Renderer/渲染器.h"
//系统层关卡数据（关卡文本读写已下沉到 EngineSystem）
#include "Engine/EngineSystem/src/scene/Level/关卡文本.h"
//系统层场景清单解析（清单语法职责已下沉到 EngineSystem）
#include "Engine/EngineSystem/src/scene/Manifest/场景清单.h"

/*
关卡装配桥（应用层；渲染层职责下沉第一批 · 轮 2）
渲染层只保留「纯数据消费接口」（Camera_Data / Light_Data / Fog_Data ＋ scene_apply）
与资源门面（mesh_load_obj / material_load_mtl / mesh_draw_sections）；
「关卡文本 → 资产装配 → 摆放表 → 绘制登记」的组合职责归应用层，由本桥承担。
本桥的装配顺序与失败回退口径与旧 `Renderer::level_load` / `scene_load` / `scene_draw` 完全一致，
故两条链路对同一关卡出帧逐字节相同。
*/
namespace engine
{
	//关卡相机组 → 渲染层相机组（逐字段拷贝，不做语义换算）
	void level_camera_map(const Level_Camera& source, Camera_Data& out);
	//关卡光照组 → 渲染层光照组（逐字段拷贝）
	void level_light_map(const Level_Light& source, Light_Data& out);
	//关卡雾组 → 渲染层雾组（逐字段拷贝）
	void level_fog_map(const Level_Fog& source, Fog_Data& out);

	/*
	装载关卡（应用层）
	顺序：读关卡文本 → 校验相机 → 装配资产（失败整体回退）→ 应用相机 / 光照 / 雾。
	先装配后应用，保证「装载失败时既有场景状态不被改动」。
	apply_scene 为假时只装配资产（供展示层切换关卡时不动全局相机 / 光照 / 雾）。
	失败时 `level_out` 与 `scene_out` 均被清空。
	*/
	bool level_bridge_load(Renderer& renderer, Level_Data& level_out, Asset_Scene& scene_out,
		const std::string& path, bool apply_scene = true);

	//释放装配创建的全部资源（幂等）
	void level_bridge_unload(Renderer& renderer, Asset_Scene& scene);

	/*
	装载场景清单文件（应用层）
	解析清单文本 → 按清单路径解析资产目录 → 创建资源并装配摆放表。
	任一资源创建失败整体回退；失败时 `scene_out` 清空。
	*/
	bool scene_bundle_load(Renderer& renderer, Asset_Scene& scene_out, const std::string& path);

	//逐对象构造变换矩阵并逐段绑材质（返回成功登记的对象数）
	uint32_t level_bridge_draw(Renderer& renderer, const Asset_Scene& scene);
}
