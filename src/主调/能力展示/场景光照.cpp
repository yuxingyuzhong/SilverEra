//场景配置：相机 / 光照 / 雾 / 天空 / 阴影 / 簇灯墙 / 粒子预配置
#include "src/主调/能力展示/展区公共.h"

//渲染器门面（Vulkan-free 公共面）
#include "Application/Renderer/src/render/Renderer/渲染器.h"

namespace engine
{
	//引擎命名空间
	void 场景配置(展示资源& 资源, 展示状态& 状态, Renderer& 渲染器)
	{
		//—— 相机（朝向展廊纵深） ——
		Camera_Data 相机;
		相机.position[0] = 状态.位置[0];
		相机.position[1] = 状态.位置[1];
		相机.position[2] = 状态.位置[2];
		//注视点由 update 按偏航 / 俯仰实时计算，此处给一个朝向场景纵深的值
		相机.target[0] = 0.0f;
		相机.target[1] = 3.0f;
		相机.target[2] = -20.0f;
		相机.fov = 1.30f;
		相机.far_plane = 260.0f;
		相机.clear_color[0] = 0.03f;
		相机.clear_color[1] = 0.05f;
		相机.clear_color[2] = 0.09f;
		相机.clear_color[3] = 1.0f;

		//—— 光照 ——
		Light_Data 光照;
		/*
		主光方向
		加大水平分量、减小纵深分量：让投影向画面右侧铺开而不是藏在物体背后，
		使「阴影」在漫游视角下始终可见。
		*/
		光照.direction[0] = 0.62f;
		光照.direction[1] = -0.62f;
		光照.direction[2] = -0.24f;
		光照.color[0] = 1.0f;
		光照.color[1] = 0.94f;
		光照.color[2] = 0.86f;
		//主光强度（压低到不过曝）
		光照.intensity = 1.05f;
		光照.ambient = 0.22f;
		//补光（冷色）
		光照.fill_direction[0] = -0.5f;
		光照.fill_direction[1] = -0.25f;
		光照.fill_direction[2] = 0.55f;
		光照.fill_color[0] = 0.42f;
		光照.fill_color[1] = 0.62f;
		光照.fill_color[2] = 1.0f;
		光照.fill_intensity = 0.18f;
		//三盏彩色点光源（分布在前三排展区，制造冷暖层次）
		光照.point_count = 3;
		const float 点位置[3][3] = { { 0.0f, 6.0f, -10.0f }, { -14.0f, 6.0f, -30.0f }, { 14.0f, 6.0f, -30.0f } };
		const float 点颜色[3][3] = { { 1.0f, 0.42f, 0.30f }, { 0.34f, 0.78f, 1.0f }, { 1.0f, 0.86f, 0.52f } };
		for (uint32_t 序号 = 0; 序号 < 3; ++序号)
		{
			for (uint32_t 分量 = 0; 分量 < 3; ++分量)
			{
				光照.point_position[序号][分量] = 点位置[序号][分量];
				光照.point_color[序号][分量] = 点颜色[序号][分量];
			}
			光照.point_radius[序号] = 16.0f;
			//点光强度压低，避免近处表面被推到纯白
			光照.point_intensity[序号] = 0.55f;
			光照.point_shininess[序号] = 48.0f;
		}
		//一盏自上而下的聚光灯（正对光照 / 阴影展区）
		光照.spot_count = 1;
		光照.spot_position[0][0] = 14.0f;
		光照.spot_position[0][1] = 9.0f;
		光照.spot_position[0][2] = -10.0f;
		光照.spot_direction[0][0] = 0.0f;
		光照.spot_direction[0][1] = -1.0f;
		光照.spot_direction[0][2] = 0.0f;
		光照.spot_color[0][0] = 1.0f;
		光照.spot_color[0][1] = 0.96f;
		光照.spot_color[0][2] = 0.88f;
		光照.spot_radius[0] = 26.0f;
		光照.spot_intensity[0] = 1.3f;
		光照.spot_inner_deg[0] = 16.0f;
		光照.spot_outer_deg[0] = 38.0f;
		光照.specular_intensity = 0.0f;
		光照.specular_shininess = 96.0f;
		//半球环境光（天光偏冷、地面反光偏暖）
		光照.ambient_color[0] = 0.32f;
		光照.ambient_color[1] = 0.36f;
		光照.ambient_color[2] = 0.44f;
		光照.ambient_ground[0] = 0.28f;
		光照.ambient_ground[1] = 0.24f;
		光照.ambient_ground[2] = 0.20f;
		光照.ambient_hemisphere = 0.30f;
		//阴影（软边 ＋ 三级级联）
		光照.shadow_enabled = 1;
		光照.shadow_bias = 0.004f;
		光照.shadow_softness = 0.35f;
		光照.shadow_strength = 0.8f;

		//—— 雾（线性，远距离才起效，给远景补空间层次） ——
		Fog_Data 雾;
		雾.color[0] = 0.52f;
		雾.color[1] = 0.62f;
		雾.color[2] = 0.78f;
		雾.mode = 0;
		雾.density = 0.0f;
		雾.start = 60.0f;
		雾.end = 240.0f;
		//高度衰减：高处物体比贴地物体更通透
		雾.height_falloff = 0.012f;
		渲染器.scene_apply(相机, 光照, 雾);

		/*
		Cluster 灯墙灯光登记
		临时开启簇光照以突破 8 盏上限，登记 16 × 8 盏彩色点光（记录持久保留），随后恢复关闭；
		设备不支持计算路径时跳过（灯墙只保留墙体与 8 盏前向灯光）。
		*/
		if (渲染器.cluster_supported())
		{
			渲染器.cluster_enable_set(true);
			const float 灯墙色[6][3] = {
				{ 1.0f, 0.38f, 0.32f }, { 1.0f, 0.72f, 0.30f }, { 0.55f, 0.95f, 0.55f },
				{ 0.35f, 0.90f, 0.95f }, { 0.45f, 0.62f, 1.0f }, { 0.85f, 0.55f, 1.0f },
			};
			for (uint32_t 行 = 0; 行 < 8; ++行)
			{
				for (uint32_t 列 = 0; 列 < 16; ++列)
				{
					const uint32_t 序号 = 行 * 16 + 列;
					//灯位略在墙前（-48.8），半径 2.5 覆盖邻近墙面
					const float 灯位置[3] = { -19.0f + 0.64f * static_cast<float>(列),
						1.0f + 0.7f * static_cast<float>(行), -48.8f };
					(void)渲染器.light_add_point(灯位置, 灯墙色[序号 % 6], 2.5f, 0.16f, 0.0f);
				}
			}
			渲染器.cluster_enable_set(false);
		}

		//阴影范围与级联（覆盖整条展廊：三片 × 远处倍增）
		渲染器.shadow_range_set(40.0f, 1.0f, 220.0f);
		渲染器.shadow_cascade_set(3, 18.0f, 2.2f);
		渲染器.shadow_cascade_blend_set(0.18f);
		//天空盒 / 简易 IBL（渐变天空提供背景层次与环境反射）
		渲染器.sky_enable_set(true);
		渲染器.sky_texture_set(资源.天空);
		//天空 IBL 强度压低：否则整幅画面被蓝色天光冲成单色
		渲染器.sky_params_set(0.28f, 0.26f);
		//PBR-lite 全局开启（逐物体另设粗糙度 / 金属度）
		渲染器.pbr_enable_set(true);
		渲染器.pbr_roughness_set(1.0f);
		渲染器.pbr_metallic_set(0.0f);
		渲染器.static_batching_set(true);
		渲染器.cull_enabled_set(true);
		/*
		多线程渲染：默认 4 工作线程（实例级剔除与实例缓冲差异扫描并行；
		场景 pass 在「场景目标 → 同帧后处理链」路径下还会按段并行录制）。
		*/
		渲染器.render_threads_set(4);
		//多重采样抗锯齿（4×；SSAA 不叠加）
		渲染器.msaa_samples_set(4);
		/*
		后处理链默认关闭
		六段链会把整幅画面（含 UI 与文字）糊成亮部残影并吞掉叠加层；
		展示场景默认走直通路径，后处理类展区（⑫–⑰）进区时再按需启用。
		*/
		(void)渲染器.post_chain_set(nullptr, 0);
		(void)渲染器.render_target_present_chain_set(nullptr, 0);

		/*
		粒子喷泉预配置
		默认关闭；粒子展区（⑨）进入时由 update 开启并逐帧发射 / 步进，frame_draw 登记 billboard 绘制。
		*/
		{
			Particle_Emitter 发射器{};
			发射器.position[0] = 14.0f;
			发射器.position[1] = 0.25f;
			发射器.position[2] = -50.0f;
			发射器.direction[1] = 1.0f;
			发射器.speed_min = 2.2f;
			发射器.speed_max = 3.6f;
			发射器.lifetime_min = 1.4f;
			发射器.lifetime_max = 2.2f;
			发射器.shape = static_cast<uint32_t>(Emit_Shape::Sphere_Shell);
			发射器.extents[0] = 0.18f;
			//每次发射上限（配合大容量展示）
			发射器.count = 256;
			Particle_Params 粒子参数{};
			粒子参数.gravity[1] = -1.4f;
			粒子参数.damping = 0.985f;
			粒子参数.restitution = 0.35f;
			粒子参数.friction = 0.25f;
			粒子参数.ground_enabled = 1;
			粒子参数.floor_y = 0.02f;
			//配色与柔边：新生冷青 → 衰老暖橙；软边 0.35
			粒子参数.color_start[0] = 0.35f;
			粒子参数.color_start[1] = 0.85f;
			粒子参数.color_start[2] = 1.0f;
			粒子参数.color_end[0] = 1.0f;
			粒子参数.color_end[1] = 0.45f;
			粒子参数.color_end[2] = 0.12f;
			粒子参数.color_enabled = 1;
			粒子参数.edge_softness = 0.35f;
			//软粒子档位 0.01（贴近地面处按 HiZ 历史深度淡出）
			粒子参数.soft_depth_range = 0.01f;
			//请求 Hi-Z 深度历史（软粒子前置；不支持时为假，不影响其余展区）
			(void)渲染器.hiz_enable_set(true);
			//容量升到 65 536 → 常驻路径走 GPU 间接绘制（单条命令、实例数由 GPU 写）
			if (渲染器.particle_supported() && 渲染器.particle_capacity_set(65536)
				&& 渲染器.particle_enable_set(true))
			{
				状态.粒子已配置 = 渲染器.particle_emitter_set(发射器) && 渲染器.particle_params_set(粒子参数);
				渲染器.particle_enable_set(false);
			}
		}
	}
}
