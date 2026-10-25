//渲染能力展示场景实现：一个静态场景集中铺开渲染层全部能力，相机可自由漫游
#include "src/主调/能力展示场景.h"

//渲染器门面（Vulkan-free 公共面）
#include "Application/Renderer/src/render/Renderer/渲染器.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

//引擎命名空间
namespace engine
{
	//文件内部工具
	namespace
	{
		//字体候选（按顺序取第一个可用者）
		const char* const 字体候选[] = {
			"C:\\Windows\\Fonts\\msyh.ttc",
			"C:\\Windows\\Fonts\\simhei.ttf",
			"C:\\Windows\\Fonts\\simsun.ttc",
		};

		//平移 ＋ 等比缩放（列主序）
		std::array<float, 16> 变换(float x, float y, float z, float 缩放)
		{
			return {
				缩放, 0.0f, 0.0f, 0.0f,
				0.0f, 缩放, 0.0f, 0.0f,
				0.0f, 0.0f, 缩放, 0.0f,
				x, y, z, 1.0f,
			};
		}

		//绕 Y 轴旋转 ＋ 平移
		std::array<float, 16> 绕Y变换(float x, float y, float z, float 弧度, float 缩放)
		{
			const float c = std::cos(弧度) * 缩放;
			const float s = std::sin(弧度) * 缩放;
			return {
				c, 0.0f, -s, 0.0f,
				0.0f, 缩放, 0.0f, 0.0f,
				s, 0.0f, c, 0.0f,
				x, y, z, 1.0f,
			};
		}

		//绕 X 轴旋转 ＋ 平移（把 XY 平面网格放平）
		std::array<float, 16> 绕X变换(float x, float y, float z, float 弧度, float 缩放)
		{
			const float c = std::cos(弧度) * 缩放;
			const float s = std::sin(弧度) * 缩放;
			return {
				缩放, 0.0f, 0.0f, 0.0f,
				0.0f, c, s, 0.0f,
				0.0f, -s, c, 0.0f,
				x, y, z, 1.0f,
			};
		}

		//漫反射法线贴图（全朝 +Z）
		std::vector<uint8_t> 法线像素(uint32_t 边长)
		{
			std::vector<uint8_t> 像素(static_cast<std::size_t>(边长) * 边长 * 4);
			for (std::size_t 偏移 = 0; 偏移 < 像素.size(); 偏移 += 4)
			{
				像素[偏移] = 128;
				像素[偏移 + 1] = 128;
				像素[偏移 + 2] = 255;
				像素[偏移 + 3] = 255;
			}
			return 像素;
		}

		//粗糙度贴图（左粗右滑的横向渐变）
		std::vector<uint8_t> 粗糙像素(uint32_t 边长)
		{
			std::vector<uint8_t> 像素(static_cast<std::size_t>(边长) * 边长 * 4);
			for (uint32_t y = 0; y < 边长; ++y)
			{
				for (uint32_t x = 0; x < 边长; ++x)
				{
					uint8_t 值 = static_cast<uint8_t>(255u - (255u * x / (边长 > 1 ? 边长 - 1 : 1)));
					std::size_t 偏移 = (static_cast<std::size_t>(y) * 边长 + x) * 4;
					像素[偏移] = 值;
					像素[偏移 + 1] = 值;
					像素[偏移 + 2] = 值;
					像素[偏移 + 3] = 255;
				}
			}
			return 像素;
		}

		//天空盒六面像素（自上而下的深蓝天光渐变）
		std::vector<uint8_t> 天空像素(uint32_t 边长)
		{
			std::vector<uint8_t> 像素(static_cast<std::size_t>(边长) * 边长 * 4 * 6);
			std::size_t 面字节 = static_cast<std::size_t>(边长) * 边长 * 4;
			for (uint32_t 面 = 0; 面 < 6; ++面)
			{
				for (uint32_t y = 0; y < 边长; ++y)
				{
					float t = 边长 > 1 ? static_cast<float>(y) / static_cast<float>(边长 - 1) : 0.0f;
					uint8_t 红 = static_cast<uint8_t>(22.0f + (1.0f - t) * 46.0f);
					uint8_t 绿 = static_cast<uint8_t>(46.0f + (1.0f - t) * 96.0f);
					uint8_t 蓝 = static_cast<uint8_t>(100.0f + (1.0f - t) * 130.0f);
					for (uint32_t x = 0; x < 边长; ++x)
					{
						std::size_t 偏移 = 面 * 面字节 + (static_cast<std::size_t>(y) * 边长 + x) * 4;
						像素[偏移] = 红;
						像素[偏移 + 1] = 绿;
						像素[偏移 + 2] = 蓝;
						像素[偏移 + 3] = 255;
					}
				}
			}
			return 像素;
		}

		//加载可用字体（失败返回 0，字幕退化为无）
		uint64_t 加载字体(Renderer& 渲染器)
		{
			for (const char* 路径 : 字体候选)
			{
				uint64_t 句柄 = 0;
				if (渲染器.font_load_file(句柄, 路径, 22))
					return 句柄;
			}
			return 0;
		}
	}

	//搭建场景与全部资源
	bool 能力展示场景::init(Renderer& 渲染器)
	{
		//—— 网格（八种程序化图元） ——
		if (!渲染器.mesh_sphere_load(球_, 1.0f, 32, 16))
			return false;
		if (!渲染器.mesh_cylinder_load(圆柱_, 0.8f, 1.8f, 24))
			return false;
		if (!渲染器.mesh_cone_load(圆锥_, 0.9f, 1.8f, 24))
			return false;
		if (!渲染器.mesh_capsule_load(胶囊_, 0.6f, 1.0f, 24, 8))
			return false;
		if (!渲染器.mesh_torus_load(圆环_, 0.8f, 0.3f, 32, 12))
			return false;
		if (!渲染器.mesh_plane_load(平面_, 1.8f, 1.8f))
			return false;
		if (!渲染器.mesh_grid_load(地面_, 40.0f, 40.0f, 1, 1))
			return false;
		if (!渲染器.mesh_disc_load(圆盘_, 0.9f, 32))
			return false;
		//—— 纹理 ——
		if (!渲染器.texture_load_checkerboard(棋盘_, 256, 256, 32))
			return false;
		const std::vector<uint8_t> 法线 = 法线像素(64);
		if (!渲染器.texture_load(法线贴图_, 法线.data(), 64, 64))
			return false;
		const std::vector<uint8_t> 粗糙 = 粗糙像素(64);
		if (!渲染器.texture_load(粗糙贴图_, 粗糙.data(), 64, 64))
			return false;
		const std::vector<uint8_t> 天空 = 天空像素(64);
		if (!渲染器.texture_cube_load(天空_, 天空.data(), 64, 64))
			return false;
		//—— 字体与后处理管线 ——
		字体_ = 加载字体(渲染器);
		if (!渲染器.pipeline_load(亮部提取_, "后处理.vert.spv", "亮部提取.frag.spv"))
			return false;
		if (!渲染器.pipeline_load(模糊_, "后处理.vert.spv", "模糊.frag.spv"))
			return false;
		if (!渲染器.pipeline_load(暗角_, "后处理.vert.spv", "暗角.frag.spv"))
			return false;
		//—— 中间目标（与交换链同尺寸，后处理 1:1 取源） ——
		目标宽_ = 渲染器.extent_width();
		目标高_ = 渲染器.extent_height();
		if (!渲染器.render_target_create(目标甲_, 目标宽_, 目标高_))
			return false;
		if (!渲染器.render_target_create(目标乙_, 目标宽_, 目标高_))
			return false;

		//—— 场景与光照 ——
		Camera_Data 相机;
		相机.position[0] = 位置_[0];
		相机.position[1] = 位置_[1];
		相机.position[2] = 位置_[2];
		相机.target[0] = 0.0f;
		相机.target[1] = 1.0f;
		相机.target[2] = 2.0f;
		相机.fov = 1.0472f;
		相机.far_plane = 200.0f;
		相机.clear_color[0] = 0.04f;
		相机.clear_color[1] = 0.06f;
		相机.clear_color[2] = 0.12f;
		相机.clear_color[3] = 1.0f;
		Light_Data 光照;
		//主方向光（暖色）
		光照.direction[0] = 0.35f;
		光照.direction[1] = -0.75f;
		光照.direction[2] = -0.5f;
		光照.color[0] = 1.0f;
		光照.color[1] = 0.94f;
		光照.color[2] = 0.86f;
		光照.intensity = 0.8f;
		光照.ambient = 0.12f;
		//补光（冷色）
		光照.fill_direction[0] = -0.5f;
		光照.fill_direction[1] = -0.25f;
		光照.fill_direction[2] = 0.55f;
		光照.fill_color[0] = 0.42f;
		光照.fill_color[1] = 0.62f;
		光照.fill_color[2] = 1.0f;
		光照.fill_intensity = 0.4f;
		//三盏彩色点光源
		光照.point_count = 3;
		const float 点位置[3][3] = { { -6.5f, 3.0f, -3.0f }, { 6.5f, 3.0f, -3.0f }, { 0.0f, 4.0f, -9.0f } };
		const float 点颜色[3][3] = { { 1.0f, 0.35f, 0.28f }, { 0.32f, 1.0f, 0.45f }, { 0.38f, 0.5f, 1.0f } };
		for (uint32_t 序号 = 0; 序号 < 3; ++序号)
		{
			for (uint32_t 分量 = 0; 分量 < 3; ++分量)
			{
				光照.point_position[序号][分量] = 点位置[序号][分量];
				光照.point_color[序号][分量] = 点颜色[序号][分量];
			}
			光照.point_radius[序号] = 12.0f;
			光照.point_intensity[序号] = 1.4f;
			光照.point_shininess[序号] = 48.0f;
		}
		//一盏自上而下的聚光灯
		光照.spot_count = 1;
		光照.spot_position[0][0] = 0.0f;
		光照.spot_position[0][1] = 7.0f;
		光照.spot_position[0][2] = -6.0f;
		光照.spot_direction[0][0] = 0.0f;
		光照.spot_direction[0][1] = -1.0f;
		光照.spot_direction[0][2] = 0.0f;
		光照.spot_color[0][0] = 1.0f;
		光照.spot_color[0][1] = 0.96f;
		光照.spot_color[0][2] = 0.88f;
		光照.spot_radius[0] = 18.0f;
		光照.spot_intensity[0] = 2.2f;
		光照.spot_inner_deg[0] = 10.0f;
		光照.spot_outer_deg[0] = 26.0f;
		光照.specular_intensity = 0.45f;
		光照.specular_shininess = 48.0f;
		//半球环境光
		光照.ambient_color[0] = 0.42f;
		光照.ambient_color[1] = 0.52f;
		光照.ambient_color[2] = 0.72f;
		光照.ambient_ground[0] = 0.5f;
		光照.ambient_ground[1] = 0.38f;
		光照.ambient_ground[2] = 0.24f;
		光照.ambient_hemisphere = 0.6f;
		//阴影（软边 ＋ 两级级联）
		光照.shadow_enabled = 1;
		光照.shadow_bias = 0.004f;
		光照.shadow_softness = 0.65f;
		光照.shadow_strength = 0.9f;
		Fog_Data 雾;
		雾.color[0] = 0.52f;
		雾.color[1] = 0.62f;
		雾.color[2] = 0.78f;
		雾.density = 0.5f;
		雾.start = 16.0f;
		雾.end = 48.0f;
		渲染器.scene_apply(相机, 光照, 雾);
		//阴影范围与级联
		渲染器.shadow_range_set(20.0f, 1.0f, 70.0f);
		渲染器.shadow_cascade_set(2, 3.0f, 16.0f);
		//天空盒 / 简易 IBL
		渲染器.sky_enable_set(true);
		渲染器.sky_texture_set(天空_);
		渲染器.sky_params_set(0.5f, 0.6f);
		//PBR-lite 全局开启（逐物体另设粗糙度 / 金属度）
		渲染器.pbr_enable_set(true);
		渲染器.pbr_roughness_set(1.0f);
		渲染器.pbr_metallic_set(0.0f);
		渲染器.static_batching_set(true);
		渲染器.cull_enabled_set(true);
		//后处理链：亮部提取 → 模糊 → 暗角（两台目标轮转）
		渲染器.bloom_param_set(0.5f, 0.15f, 1.2f);
		uint64_t 链[3] = { 亮部提取_, 模糊_, 暗角_ };
		uint64_t 目标组[2] = { 目标甲_, 目标乙_ };
		if (!渲染器.post_chain_set(链, 3))
			return false;
		if (!渲染器.render_target_present_chain_set(目标组, 2))
			return false;
		return true;
	}

	//按输入推进相机，并在窗口尺寸变化时重建后处理目标
	void 能力展示场景::update(Renderer& 渲染器, const 展示输入& 输入, float 秒)
	{
		//累计时间（驱动轻量动画）
		累计秒_ += 秒;
		//窗口尺寸变化：后处理目标必须与交换链同尺寸
		const uint32_t 宽 = 渲染器.extent_width();
		const uint32_t 高 = 渲染器.extent_height();
		if (宽 != 目标宽_ || 高 != 目标高_)
		{
			if (宽 != 0 && 高 != 0)
			{
				//先建新目标、再销毁旧目标，避免中途回落单级路径
				uint64_t 新甲 = 0;
				uint64_t 新乙 = 0;
				if (渲染器.render_target_create(新甲, 宽, 高) && 渲染器.render_target_create(新乙, 宽, 高))
				{
					if (目标甲_ != 0)
						渲染器.render_target_destroy(目标甲_);
					if (目标乙_ != 0)
						渲染器.render_target_destroy(目标乙_);
					目标甲_ = 新甲;
					目标乙_ = 新乙;
					目标宽_ = 宽;
					目标高_ = 高;
					uint64_t 链[3] = { 亮部提取_, 模糊_, 暗角_ };
					uint64_t 目标组[2] = { 目标甲_, 目标乙_ };
					(void)渲染器.post_chain_set(链, 3);
					(void)渲染器.render_target_present_chain_set(目标组, 2);
				}
			}
		}
		//鼠标拖拽改变朝向
		偏航_ += 输入.偏航增量;
		俯仰_ += 输入.俯仰增量;
		if (俯仰_ > 1.35f)
			俯仰_ = 1.35f;
		if (俯仰_ < -1.35f)
			俯仰_ = -1.35f;
		//速度（默认 8 单位 / 秒，滚轮倍率 0.25..4）
		float 速度 = 8.0f * (输入.速度倍率 > 0.0f ? 输入.速度倍率 : 1.0f);
		//前向（水平）与右向
		const float 前x = std::sin(偏航_);
		const float 前z = -std::cos(偏航_);
		const float 右x = std::cos(偏航_);
		const float 右z = std::sin(偏航_);
		//位移（WASD ＋ Q/E 升降）
		位置_[0] += (前x * 输入.前后 + 右x * 输入.左右) * 速度 * 秒;
		位置_[1] += 输入.上下 * 速度 * 秒;
		位置_[2] += (前z * 输入.前后 + 右z * 输入.左右) * 速度 * 秒;
		//不离地过低
		if (位置_[1] < 0.6f)
			位置_[1] = 0.6f;
		//注视点 = 位置 ＋ 朝向（含俯仰）
		const float cos俯仰 = std::cos(俯仰_);
		const float 注视x = 位置_[0] + 前x * cos俯仰;
		const float 注视y = 位置_[1] + std::sin(俯仰_);
		const float 注视z = 位置_[2] + 前z * cos俯仰;
		渲染器.camera_position_set(位置_[0], 位置_[1], 位置_[2]);
		渲染器.camera_target_set(注视x, 注视y, 注视z);
	}

	//登记本帧全部绘制项与字幕
	void 能力展示场景::frame_draw(Renderer& 渲染器)
	{
		//—— 地面（棋盘纹理；细分平面放平） ——
		渲染器.texture_bind_set(棋盘_);
		const std::array<float, 16> 地面矩阵 = 绕X变换(0.0f, 0.0f, -4.0f, -1.5707963f, 1.0f);
		(void)渲染器.mesh_draw(地面_, 地面矩阵.data());

		//—— 第一排：八种程序化网格（棋盘纹理） ——
		const engine::Mesh_Handle 网格排[8] = { 球_, 圆柱_, 圆锥_, 胶囊_, 圆环_, 平面_, 圆盘_, 球_ };
		for (uint32_t 序号 = 0; 序号 < 8; ++序号)
		{
			const std::array<float, 16> 矩阵 = 变换(-7.0f + 2.0f * static_cast<float>(序号), 1.1f, -3.0f, 0.85f);
			(void)渲染器.mesh_draw(网格排[序号], 矩阵.data());
		}

		//—— 第二排：PBR 材质（粗糙度 / 金属度递增 ＋ 法线与粗糙度贴图） ——
		渲染器.texture_bind_set(0);
		const float 粗糙度[4] = { 0.15f, 0.4f, 0.7f, 0.95f };
		for (uint32_t 序号 = 0; 序号 < 4; ++序号)
		{
			(void)渲染器.material_pbr_set(粗糙度[序号], 0.0f);
			const std::array<float, 16> 矩阵 = 变换(-6.0f + 2.2f * static_cast<float>(序号), 1.2f, -8.0f, 0.95f);
			(void)渲染器.mesh_draw(球_, 矩阵.data());
		}
		for (uint32_t 序号 = 0; 序号 < 4; ++序号)
		{
			(void)渲染器.material_pbr_set(0.25f, 0.2f + 0.26f * static_cast<float>(序号));
			const std::array<float, 16> 矩阵 = 变换(-6.0f + 2.2f * static_cast<float>(序号), 1.2f, -10.0f, 0.95f);
			(void)渲染器.mesh_draw(球_, 矩阵.data());
		}
		(void)渲染器.material_pbr_clear();
		//带法线 / 粗糙度贴图的球
		Material_Data 贴图材质;
		贴图材质.pbr_override = true;
		贴图材质.pbr_roughness = 0.7f;
		贴图材质.pbr_metallic = 0.35f;
		贴图材质.normal_texture = 法线贴图_;
		贴图材质.roughness_texture = 粗糙贴图_;
		贴图材质.normal_strength = 1.5f;
		Material_Handle 贴图句柄 = 0;
		if (渲染器.material_load(贴图句柄, 贴图材质))
		{
			渲染器.material_bind_set(贴图句柄);
			const std::array<float, 16> 贴图球 = 变换(5.6f, 1.2f, -9.0f, 0.95f);
			(void)渲染器.mesh_draw(球_, 贴图球.data());
			渲染器.material_bind_set(0);
			渲染器.material_unload(贴图句柄);
		}
		//逐物体色调与自发光
		渲染器.material_tint_set(1.0f, 0.5f, 0.25f, 1.0f);
		{
			const std::array<float, 16> 矩阵 = 变换(-2.0f, 1.4f, -8.0f, 0.6f);
			(void)渲染器.mesh_draw(圆环_, 矩阵.data());
		}
		渲染器.material_tint_clear();
		渲染器.material_emissive_set(2.5f);
		渲染器.material_tint_set(0.45f, 0.85f, 1.0f, 1.0f);
		{
			const std::array<float, 16> 矩阵 = 变换(2.0f, 1.4f, -8.0f, 0.6f);
			(void)渲染器.mesh_draw(胶囊_, 矩阵.data());
		}
		渲染器.material_emissive_clear();
		渲染器.material_tint_clear();

		//—— 第三排：阴影展示（圆锥 / 圆柱 / 胶囊立在阴影范围中心附近） ——
		渲染器.texture_bind_set(棋盘_);
		const engine::Mesh_Handle 影排[3] = { 圆锥_, 圆柱_, 胶囊_ };
		for (uint32_t 序号 = 0; 序号 < 3; ++序号)
		{
			const std::array<float, 16> 矩阵 = 变换(-2.2f + 2.2f * static_cast<float>(序号), 1.6f, -15.0f, 1.2f);
			(void)渲染器.mesh_draw(影排[序号], 矩阵.data());
		}

		//—— 实例化与合批：右侧 8×8 球阵（一次实例化调用） ——
		std::vector<float> 实例矩阵;
		实例矩阵.reserve(64 * 16);
		for (uint32_t 行号 = 0; 行号 < 8; ++行号)
		{
			for (uint32_t 列号 = 0; 列号 < 8; ++列号)
			{
				const float x = 8.0f + (static_cast<float>(列号) - 3.5f) * 1.15f;
				const float z = -9.0f + (static_cast<float>(行号) - 3.5f) * 1.15f;
				const std::array<float, 16> 矩阵 = 变换(x, 0.45f, z, 0.32f);
				实例矩阵.insert(实例矩阵.end(), 矩阵.begin(), 矩阵.end());
			}
		}
		(void)渲染器.mesh_draw_instanced(球_, 实例矩阵.data(), 64);

		//—— 自转圆环（动画；证明逐帧重建绘制项） ——
		const std::array<float, 16> 转环 = 绕Y变换(0.0f, 2.6f, -6.0f, 累计秒_ * 0.8f, 1.4f);
		(void)渲染器.mesh_draw(圆环_, 转环.data());

		//—— 文字：标题与区域标签（最后画，避免字体图集被后续网格覆盖） ——
		if (字体_ != 0)
		{
			(void)渲染器.text_draw(字体_, "白银纪元 · 渲染能力展示", 24.0f, 24.0f);
			(void)渲染器.text_draw(字体_, "程序化网格 / 纹理", 60.0f, 300.0f);
			(void)渲染器.text_draw(字体_, "光照 / 阴影 / 级联", 60.0f, 340.0f);
			(void)渲染器.text_draw(字体_, "PBR 材质 / 贴图 / 自发光", 60.0f, 380.0f);
			(void)渲染器.text_draw(字体_, "实例化 / 合批 / 天空盒 / 雾 / 后处理", 60.0f, 420.0f);
		}
	}

	//释放全部资源（幂等）
	void 能力展示场景::shutdown(Renderer& 渲染器)
	{
		//先退后处理链，避免目标被采样时销毁
		(void)渲染器.post_chain_set(nullptr, 0);
		(void)渲染器.render_target_present_chain_set(nullptr, 0);
		if (目标乙_ != 0)
			渲染器.render_target_destroy(目标乙_);
		if (目标甲_ != 0)
			渲染器.render_target_destroy(目标甲_);
		目标乙_ = 0;
		目标甲_ = 0;
		//后处理管线
		const uint64_t 管线[3] = { 亮部提取_, 模糊_, 暗角_ };
		for (uint64_t 句柄 : 管线)
		{
			if (句柄 != 0)
				渲染器.pipeline_unload(句柄);
		}
		亮部提取_ = 0;
		模糊_ = 0;
		暗角_ = 0;
		//字体与纹理
		if (字体_ != 0)
			渲染器.font_unload(字体_);
		const uint64_t 纹理[4] = { 棋盘_, 法线贴图_, 粗糙贴图_, 天空_ };
		for (uint64_t 句柄 : 纹理)
		{
			if (句柄 != 0)
				渲染器.texture_unload(句柄);
		}
		字体_ = 0;
		棋盘_ = 0;
		法线贴图_ = 0;
		粗糙贴图_ = 0;
		天空_ = 0;
		//网格
		const engine::Mesh_Handle 网格[8] = { 球_, 圆柱_, 圆锥_, 胶囊_, 圆环_, 平面_, 地面_, 圆盘_ };
		for (engine::Mesh_Handle 句柄 : 网格)
		{
			if (句柄 != 0)
				渲染器.mesh_unload(句柄);
		}
		球_ = 圆柱_ = 圆锥_ = 胶囊_ = 圆环_ = 平面_ = 地面_ = 圆盘_ = 0;
		//复位展示用开关
		渲染器.sky_enable_set(false);
		渲染器.shadow_enable_set(false);
		渲染器.shadow_cascade_set(1, 3.0f, 12.0f);
		渲染器.pbr_enable_set(false);
		渲染器.fog_clear();
		渲染器.texture_bind_set(0);
		渲染器.material_pbr_clear();
		渲染器.material_tint_clear();
		渲染器.material_emissive_clear();
	}

	//相机世界位置
	void 能力展示场景::相机位置(float 出[3]) const
	{
		出[0] = 位置_[0];
		出[1] = 位置_[1];
		出[2] = 位置_[2];
	}

	//当前偏航
	float 能力展示场景::偏航(void) const
	{
		return 偏航_;
	}

	//当前俯仰
	float 能力展示场景::俯仰(void) const
	{
		return 俯仰_;
	}
}
