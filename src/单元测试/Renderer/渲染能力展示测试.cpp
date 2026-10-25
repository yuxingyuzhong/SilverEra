/*
渲染能力全景展示（特殊用例；随渲染层迭代同步更新）
目的：把渲染层当前可提供的全部能力按「阶段」串成一条可视巡览，供人直观感受渲染层进步；
      同时提供一条无人值守冒烟用例，保证展示代码与渲染层接口同步、不会因迭代而失效。

维护约定（新增渲染能力时必须遵守）：
1. 在「阶段表」中新增一项（名称 / 说明 / 应用函数），并在应用函数内把该能力配置到画面上；
2. 若新能力需要新资源（网格 / 纹理 / 管线），在「资源准备」中一并创建并随资源结构释放；
3. 阶段函数必须**自包含**：先在「默认状态恢复」之后显式设置本阶段所需的全部开关，
   不依赖上一阶段的残留状态（巡览会循环，顺序可能变化）。

运行方式：
- 交互巡览：设置环境变量 BYJY_SHOWCASE=1 后运行本套件（每个阶段约 2.5 秒，自动轮播三轮后结束；
  中途关闭窗口即退出）。用途：人眼验收 / 向客户展示。
- 冒烟：默认随全量回归运行（隐藏窗口 64×36，逐阶段渲染三帧并断言绘制量），保证接口不漂移。
*/
#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

//获取渲染器门面（场景 / 材质 / 后处理 / 文字 / 天空盒 / 抗锯齿）
#include "Application/Renderer/src/render/Renderer/渲染器.h"

namespace
{
	//交互巡览窗口尺寸
	constexpr uint32_t 展示窗口宽 = 1280;
	constexpr uint32_t 展示窗口高 = 720;
	//冒烟用例窗口尺寸（与既有隐藏用例一致，便于快速跑完）
	constexpr uint32_t 冒烟窗口宽 = 64;
	constexpr uint32_t 冒烟窗口高 = 36;
	//每个阶段停留的帧数（垂直同步 60 fps 下约 2.5 秒）
	constexpr uint32_t 每阶段帧数 = 150;
	//自动轮播的轮数（防止无人值守时无限循环）
	constexpr uint32_t 轮播轮数 = 3;
	//中文字体候选（按顺序取第一个可用者）
	const char* const 字体候选[] = {
		"C:\\Windows\\Fonts\\simhei.ttf",
		"C:\\Windows\\Fonts\\msyh.ttc",
		"C:\\Windows\\Fonts\\simsun.ttc",
		"C:\\Windows\\Fonts\\consola.ttf",
	};

	/*
	展示资源（一次创建、全程复用）
	网格 / 纹理 / 后处理管线 / 字体 / 中间目标都集中在此，阶段函数只引用不持有。
	*/
	struct 展示资源
	{
		//渲染器门面
		engine::Renderer& 渲染器;
		//程序化网格
		engine::Mesh_Handle 球 = 0;
		engine::Mesh_Handle 圆柱 = 0;
		engine::Mesh_Handle 圆锥 = 0;
		engine::Mesh_Handle 胶囊 = 0;
		engine::Mesh_Handle 圆环 = 0;
		engine::Mesh_Handle 平面 = 0;
		engine::Mesh_Handle 地面 = 0;
		engine::Mesh_Handle 圆盘 = 0;
		//纹理
		uint64_t 棋盘 = 0;
		uint64_t 中灰 = 0;
		uint64_t 法线贴图 = 0;
		uint64_t 粗糙贴图 = 0;
		uint64_t 天空 = 0;
		//字体（0 = 本机无可用字体，此时跳过字幕）
		uint64_t 字体 = 0;
		//后处理管线
		uint64_t 亮部提取 = 0;
		uint64_t 模糊 = 0;
		uint64_t 分级调色 = 0;
		uint64_t 暗角 = 0;
		uint64_t 胶片颗粒 = 0;
		uint64_t 锐化 = 0;
		uint64_t 景深 = 0;
		//中间目标（同帧后处理链轮转用）
		uint64_t 目标甲 = 0;
		uint64_t 目标乙 = 0;
		//当前交换链尺寸
		uint32_t 宽 = 0;
		uint32_t 高 = 0;
		//是否交互模式（决定字幕排版详略）
		bool 交互 = false;
	};

	//—— 矩阵构造（列主序，与渲染层模型矩阵口径一致）——

	//平移 ＋ 等比缩放
	std::array<float, 16> 变换(float x, float y, float z, float 缩放 = 1.0f)
	{
		return {
			缩放, 0.0f, 0.0f, 0.0f,
			0.0f, 缩放, 0.0f, 0.0f,
			0.0f, 0.0f, 缩放, 0.0f,
			x, y, z, 1.0f,
		};
	}

	//绕 X 轴旋转 ＋ 平移（把 XY 平面网格放平）
	std::array<float, 16> 绕X变换(float x, float y, float z, float 弧度, float 缩放 = 1.0f)
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

	//—— 数据构造 ——

	//相机数据
	engine::Camera_Data 相机数据(float px, float py, float pz, float tx, float ty, float tz, float 视场角 = 1.0472f)
	{
		engine::Camera_Data 相机;
		相机.position[0] = px;
		相机.position[1] = py;
		相机.position[2] = pz;
		相机.target[0] = tx;
		相机.target[1] = ty;
		相机.target[2] = tz;
		相机.fov = 视场角;
		return 相机;
	}

	//运费色纹理（RGBA8；四分量由参数给定）
	std::vector<uint8_t> 纯色像素(uint8_t 红, uint8_t 绿, uint8_t 蓝)
	{
		return std::vector<uint8_t>{ 红, 绿, 蓝, 255, 红, 绿, 蓝, 255, 红, 绿, 蓝, 255, 红, 绿, 蓝, 255 };
	}

	//漫反射法线贴图（切线空间：所有像素朝 +Z）
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

	//粗糙度贴图（左粗右滑的横向渐变；RGB 相同、A 为 255）
	std::vector<uint8_t> 粗糙像素(uint32_t 边长)
	{
		std::vector<uint8_t> 像素(static_cast<std::size_t>(边长) * 边长 * 4);
		for (uint32_t y = 0; y < 边长; ++y)
		{
			for (uint32_t x = 0; x < 边长; ++x)
			{
				//从左到右由粗（白）到滑（黑）
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

	//天空盒六面像素（自上而下的深蓝天光渐变；六面同内容，IBL 采样方向仍有明暗差）
	std::vector<uint8_t> 天空像素(uint32_t 边长)
	{
		std::vector<uint8_t> 像素(static_cast<std::size_t>(边长) * 边长 * 4 * 6);
		std::size_t 面字节 = static_cast<std::size_t>(边长) * 边长 * 4;
		for (uint32_t 面 = 0; 面 < 6; ++面)
		{
			for (uint32_t y = 0; y < 边长; ++y)
			{
				//顶部亮蓝、底部深蓝
				float t = 边长 > 1 ? static_cast<float>(y) / static_cast<float>(边长 - 1) : 0.0f;
				uint8_t 红 = static_cast<uint8_t>(20.0f + (1.0f - t) * 40.0f);
				uint8_t 绿 = static_cast<uint8_t>(40.0f + (1.0f - t) * 90.0f);
				uint8_t 蓝 = static_cast<uint8_t>(90.0f + (1.0f - t) * 130.0f);
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

	//登记一条后处理管线（顶点着色器统一复用 后处理.vert.spv）
	uint64_t 登记后处理(engine::Renderer& 渲染器, const char* 片元)
	{
		uint64_t 句柄 = 0;
		if (!渲染器.pipeline_load(句柄, "后处理.vert.spv", 片元))
			return 0;
		return 句柄;
	}

	//加载一个可用字体（找不到可用字体时返回 0，展示退化为无字幕）
	uint64_t 加载字体(engine::Renderer& 渲染器)
	{
		for (const char* 路径 : 字体候选)
		{
			uint64_t 句柄 = 0;
			if (渲染器.font_load_file(句柄, 路径, 22))
				return 句柄;
		}
		return 0;
	}

	//—— 绘制注册（每帧调用；只登记待绘制项，真正绘制在 frame_render 内发生）——

	//绘制一个网格（可指定缩放）
	void 画(展示资源& 资源, engine::Mesh_Handle 网格, float x, float y, float z, float 缩放 = 1.0f)
	{
		const std::array<float, 16> 矩阵 = 变换(x, y, z, 缩放);
		(void)资源.渲染器.mesh_draw(网格, 矩阵.data());
	}

	//绘制指定区间内的一排网格（等距摆开）
	void 画一排(展示资源& 资源, engine::Mesh_Handle 网格, uint32_t 数量, float 起点x, float 间距,
		float y, float z, float 缩放)
	{
		for (uint32_t 序号 = 0; 序号 < 数量; ++序号)
			画(资源, 网格, 起点x + 间距 * static_cast<float>(序号), y, z, 缩放);
	}

	//构造 8×8 实例矩阵（每实例一个平移缩放矩阵，共 64 个）
	std::vector<float> 网格实例(uint32_t 列, uint32_t 行, float 间距, float 中心x, float 中心y, float z, float 缩放)
	{
		std::vector<float> 输出;
		输出.reserve(static_cast<std::size_t>(列) * 行 * 16);
		for (uint32_t 行号 = 0; 行号 < 行; ++行号)
		{
			for (uint32_t 列号 = 0; 列号 < 列; ++列号)
			{
				float x = 中心x + (static_cast<float>(列号) - (列 - 1) * 0.5f) * 间距;
				float y = 中心y + (static_cast<float>(行号) - (行 - 1) * 0.5f) * 间距;
				const std::array<float, 16> 矩阵 = 变换(x, y, z, 缩放);
				输出.insert(输出.end(), 矩阵.begin(), 矩阵.end());
			}
		}
		return 输出;
	}

	/*
	默认状态恢复（每个阶段开始前调用）
	把「阶段之间会互相影响」的开关全部复位，保证阶段自包含；不改变交换链尺寸与资源。
	*/
	void 默认恢复(engine::Renderer& 渲染器)
	{
		//抗锯齿回到单采样
		渲染器.msaa_samples_set(1);
		渲染器.ssaa_factor_set(1);
		//特效开关复位
		渲染器.sky_enable_set(false);
		渲染器.shadow_enable_set(false);
		渲染器.shadow_cascade_set(1, 3.0f, 12.0f);
		渲染器.pbr_enable_set(false);
		渲染器.pbr_metallic_set(0.0f);
		渲染器.pbr_roughness_set(1.0f);
		渲染器.fog_clear();
		渲染器.blend_mode_set(engine::Blend_Mode::Opaque);
		渲染器.static_batching_set(true);
		渲染器.cull_enabled_set(true);
		//逐物体状态复位
		渲染器.material_pbr_clear();
		渲染器.material_tint_clear();
		渲染器.material_emissive_clear();
		//纹理与光源复位（回到内置白纹理 ＋ 单方向光）
		渲染器.texture_bind_set(0);
		渲染器.lights_clear();
		渲染器.specular_set(0.0f, 16.0f);
		float 白[3] = { 1.0f, 1.0f, 1.0f };
		渲染器.ambient_color_set(白);
		渲染器.ambient_ground_set(白, 0.0f);
		渲染器.directional_specular_set(1.0f);
		//后处理链复位（回到交换链直通）
		渲染器.bloom_param_set(0.6f, 0.1f, 1.0f);
		渲染器.post_chain_set(nullptr, 0);
		渲染器.render_target_present_chain_set(nullptr, 0);
		渲染器.render_target_present_set(0);
		//场景回到默认相机 / 满照 / 无雾
		渲染器.scene_apply(相机数据(0.0f, 0.0f, 3.0f, 0.0f, 0.0f, 0.0f),
			engine::Light_Data{}, engine::Fog_Data{});
	}

	//—— 阶段 1：程序化网格与纹理 ——
	void 阶段_网格纹理(展示资源& 资源)
	{
		engine::Camera_Data 相机 = 相机数据(0.0f, 0.0f, 14.0f, 0.0f, 0.0f, 0.0f);
		engine::Light_Data 光照;
		光照.intensity = 1.0f;
		光照.ambient = 0.35f;
		光照.specular_intensity = 0.35f;
		光照.specular_shininess = 24.0f;
		资源.渲染器.scene_apply(相机, 光照, engine::Fog_Data{});
		//棋盘纹理展示 UV 与采样
		资源.渲染器.texture_bind_set(资源.棋盘);
		//四列两行：八种程序化网格
		画(资源, 资源.球, -4.8f, 1.8f, 0.0f, 0.9f);
		画(资源, 资源.圆柱, -1.6f, 1.8f, 0.0f, 0.9f);
		画(资源, 资源.圆锥, 1.6f, 1.8f, 0.0f, 0.9f);
		画(资源, 资源.胶囊, 4.8f, 1.8f, 0.0f, 0.9f);
		画(资源, 资源.圆环, -4.8f, -1.6f, 0.0f, 0.9f);
		画(资源, 资源.平面, -1.6f, -1.6f, 0.0f, 0.9f);
		画(资源, 资源.圆盘, 1.6f, -1.6f, 0.0f, 0.9f);
		画(资源, 资源.地面, 4.8f, -1.6f, 0.0f, 0.45f);
	}

	//—— 阶段 2：光照系统（方向光 / 补光 / 点光 / 聚光 / 半球环境光）——
	void 阶段_光照(展示资源& 资源)
	{
		engine::Light_Data 光照;
		//主方向光偏暖、强度中等
		光照.direction[0] = 0.35f;
		光照.direction[1] = -0.75f;
		光照.direction[2] = -0.55f;
		光照.color[0] = 1.0f;
		光照.color[1] = 0.92f;
		光照.color[2] = 0.80f;
		光照.intensity = 0.75f;
		光照.ambient = 0.18f;
		//补光（第二盏方向光，冷色）
		光照.fill_direction[0] = -0.5f;
		光照.fill_direction[1] = -0.2f;
		光照.fill_direction[2] = 0.6f;
		光照.fill_color[0] = 0.45f;
		光照.fill_color[1] = 0.62f;
		光照.fill_color[2] = 1.0f;
		光照.fill_intensity = 0.45f;
		光照.fill_specular = 0.6f;
		//三盏彩色点光源
		光照.point_count = 3;
		const float 点光位置[3][3] = { { -4.0f, 2.2f, 4.0f }, { 4.0f, 2.2f, 4.0f }, { 0.0f, -2.6f, 4.0f } };
		const float 点光颜色[3][3] = { { 1.0f, 0.30f, 0.25f }, { 0.30f, 1.0f, 0.42f }, { 0.35f, 0.45f, 1.0f } };
		for (uint32_t 序号 = 0; 序号 < 3; ++序号)
		{
			for (uint32_t 分量 = 0; 分量 < 3; ++分量)
			{
				光照.point_position[序号][分量] = 点光位置[序号][分量];
				光照.point_color[序号][分量] = 点光颜色[序号][分量];
			}
			光照.point_radius[序号] = 9.0f;
			光照.point_intensity[序号] = 1.6f;
			光照.point_shininess[序号] = 48.0f;
		}
		//一盏自上而下的聚光灯
		光照.spot_count = 1;
		光照.spot_position[0][0] = 0.0f;
		光照.spot_position[0][1] = 4.5f;
		光照.spot_position[0][2] = 5.0f;
		光照.spot_direction[0][0] = 0.0f;
		光照.spot_direction[0][1] = -1.0f;
		光照.spot_direction[0][2] = -0.25f;
		光照.spot_color[0][0] = 1.0f;
		光照.spot_color[0][1] = 0.95f;
		光照.spot_color[0][2] = 0.85f;
		光照.spot_radius[0] = 14.0f;
		光照.spot_intensity[0] = 2.0f;
		光照.spot_inner_deg[0] = 10.0f;
		光照.spot_outer_deg[0] = 24.0f;
		//全局高光 ＋ 半球环境光（上冷下暖）
		光照.specular_intensity = 0.55f;
		光照.specular_shininess = 40.0f;
		光照.ambient_color[0] = 0.40f;
		光照.ambient_color[1] = 0.50f;
		光照.ambient_color[2] = 0.70f;
		光照.ambient_ground[0] = 0.55f;
		光照.ambient_ground[1] = 0.40f;
		光照.ambient_ground[2] = 0.25f;
		光照.ambient_hemisphere = 0.65f;
		资源.渲染器.scene_apply(相机数据(0.0f, 0.0f, 14.0f, 0.0f, 0.0f, 0.0f), 光照, engine::Fog_Data{});
		//白纹理下的球阵，便于同时观察漫反射与高光
		资源.渲染器.texture_bind_set(0);
		画一排(资源, 资源.球, 5, -4.8f, 2.4f, 1.4f, 0.0f, 1.0f);
		画一排(资源, 资源.球, 5, -4.8f, 2.4f, -1.6f, 0.0f, 1.0f);
	}

	//—— 阶段 3：PBR-lite 材质与逐物体属性 ——
	void 阶段_材质(展示资源& 资源)
	{
		engine::Light_Data 光照;
		光照.direction[0] = 0.45f;
		光照.direction[1] = -0.45f;
		光照.direction[2] = -0.75f;
		光照.intensity = 1.0f;
		光照.ambient = 0.12f;
		光照.specular_intensity = 0.6f;
		光照.specular_shininess = 64.0f;
		资源.渲染器.scene_apply(相机数据(0.0f, 0.0f, 13.0f, 0.0f, 0.0f, 0.0f), 光照, engine::Fog_Data{});
		//开启 PBR-lite（GGX ＋ 能量守恒）
		资源.渲染器.pbr_enable_set(true);
		资源.渲染器.texture_bind_set(0);
		//第一排：粗糙度递增的介电球
		const float 粗糙度[4] = { 0.15f, 0.35f, 0.65f, 0.95f };
		for (uint32_t 序号 = 0; 序号 < 4; ++序号)
		{
			float 位置x = -4.5f + 3.0f * static_cast<float>(序号);
			资源.渲染器.material_pbr_set(粗糙度[序号], 0.0f);
			画(资源, 资源.球, 位置x, 1.5f, 0.0f, 0.95f);
		}
		//第二排：金属度递增
		const float 金属度[4] = { 0.2f, 0.5f, 0.8f, 1.0f };
		for (uint32_t 序号 = 0; 序号 < 4; ++序号)
		{
			float 位置x = -4.5f + 3.0f * static_cast<float>(序号);
			资源.渲染器.material_pbr_set(0.25f, 金属度[序号]);
			画(资源, 资源.球, 位置x, -1.5f, 0.0f, 0.95f);
		}
		资源.渲染器.material_pbr_clear();
		//第三排中性色带：色调与自发光
		资源.渲染器.material_tint_set(1.0f, 0.45f, 0.2f, 1.0f);
		画(资源, 资源.圆环, -5.0f, 0.0f, 1.5f, 0.6f);
		资源.渲染器.material_tint_clear();
		资源.渲染器.material_emissive_set(2.2f);
		资源.渲染器.material_tint_set(0.4f, 0.8f, 1.0f, 1.0f);
		画(资源, 资源.胶囊, 0.0f, 0.0f, 1.5f, 0.55f);
		资源.渲染器.material_emissive_clear();
		资源.渲染器.material_tint_clear();
		//带法线 / 粗糙度贴图的材质（PBR 贴图化）
		engine::Material_Data 材质数据;
		材质数据.pbr_override = true;
		材质数据.pbr_roughness = 0.8f;
		材质数据.normal_texture = 资源.法线贴图;
		材质数据.roughness_texture = 资源.粗糙贴图;
		材质数据.normal_strength = 1.4f;
		材质数据.tint_override = true;
		材质数据.tint[0] = 0.85f;
		材质数据.tint[1] = 0.9f;
		材质数据.tint[2] = 1.0f;
		engine::Material_Handle 材质 = 0;
		if (资源.渲染器.material_load(材质, 材质数据))
		{
			资源.渲染器.material_bind_set(材质);
			画(资源, 资源.球, 5.0f, 0.0f, 1.5f, 0.75f);
			资源.渲染器.material_bind_set(0);
			资源.渲染器.material_unload(材质);
		}
	}

	//—— 阶段 4：方向光阴影与两级级联 ——
	void 阶段_阴影(展示资源& 资源)
	{
		engine::Light_Data 光照;
		光照.direction[0] = 0.45f;
		光照.direction[1] = -0.85f;
		光照.direction[2] = -0.35f;
		光照.intensity = 1.0f;
		光照.ambient = 0.22f;
		光照.specular_intensity = 0.3f;
		//阴影：软边 ＋ 略微减弱 ＋ 两级级联
		光照.shadow_enabled = 1;
		光照.shadow_bias = 0.004f;
		光照.shadow_softness = 0.7f;
		光照.shadow_strength = 0.85f;
		光照.shadow_resolution = 1024;
		资源.渲染器.scene_apply(相机数据(0.0f, 3.6f, 13.0f, 0.0f, 0.4f, 0.0f), 光照, engine::Fog_Data{});
		资源.渲染器.shadow_range_set(11.0f, 1.0f, 42.0f);
		资源.渲染器.shadow_cascade_set(2, 3.0f, 10.0f);
		资源.渲染器.texture_bind_set(0);
		//地面（细分平面放平）
		const std::array<float, 16> 地面矩阵 = 绕X变换(0.0f, -1.6f, 0.0f, -1.5707963f, 1.0f);
		(void)资源.渲染器.mesh_draw(资源.地面, 地面矩阵.data());
		//地面上方错落摆放不同网格，观察阴影形状与级联衔接
		画(资源, 资源.球, -4.2f, -0.3f, 0.0f, 1.1f);
		画(资源, 资源.圆锥, -1.6f, -0.1f, 0.0f, 1.2f);
		画(资源, 资源.圆柱, 1.2f, -0.5f, 0.0f, 1.0f);
		画(资源, 资源.胶囊, 4.2f, 0.0f, 0.0f, 0.9f);
	}

	//—— 阶段 5：实例化绘制与静态合批（同时展示视锥剔除）——
	void 阶段_实例化(展示资源& 资源)
	{
		engine::Light_Data 光照;
		光照.direction[0] = 0.2f;
		光照.direction[1] = -0.6f;
		光照.direction[2] = -0.75f;
		光照.intensity = 1.0f;
		光照.ambient = 0.3f;
		光照.specular_intensity = 0.45f;
		资源.渲染器.scene_apply(相机数据(0.0f, 0.0f, 15.0f, 0.0f, 0.0f, 0.0f), 光照, engine::Fog_Data{});
		资源.渲染器.texture_bind_set(资源.棋盘);
		//左半屏：64 个普通绘制项（相邻等价 → 由静态合批合并为一次实例化绘制）
		画一排(资源, 资源.球, 8, -8.5f, 1.0f, 1.4f, 0.0f, 0.35f);
		画一排(资源, 资源.球, 8, -8.5f, 1.0f, 0.6f, 0.0f, 0.35f);
		画一排(资源, 资源.球, 8, -8.5f, 1.0f, -0.2f, 0.0f, 0.35f);
		画一排(资源, 资源.球, 8, -8.5f, 1.0f, -1.0f, 0.0f, 0.35f);
		画一排(资源, 资源.球, 8, -8.5f, 1.0f, -1.8f, 0.0f, 0.35f);
		画一排(资源, 资源.球, 8, -8.5f, 1.0f, -2.6f, 0.0f, 0.35f);
		画一排(资源, 资源.球, 8, -8.5f, 1.0f, -3.4f, 0.0f, 0.35f);
		画一排(资源, 资源.球, 8, -8.5f, 1.0f, -4.2f, 0.0f, 0.35f);
		//右半屏：一次实例化调用提交 8×8 = 64 个实例
		const std::vector<float> 实例矩阵 = 网格实例(8, 8, 1.0f, 3.6f, 0.0f, 0.0f, 0.35f);
		(void)资源.渲染器.mesh_draw_instanced(资源.球, 实例矩阵.data(), 64);
		//视锥外一项（相机后方）用于展示剔除计数
		画(资源, 资源.圆环, 0.0f, 0.0f, 20.0f, 0.4f);
	}

	//—— 阶段 6：天空盒 / 简易 IBL 与距离雾 ——
	void 阶段_天空雾(展示资源& 资源)
	{
		engine::Light_Data 光照;
		光照.direction[0] = 0.2f;
		光照.direction[1] = -0.35f;
		光照.direction[2] = -0.9f;
		光照.intensity = 0.55f;
		光照.ambient = 0.10f;
		光照.specular_intensity = 0.7f;
		光照.specular_shininess = 72.0f;
		//距离雾：远处网格逐渐融入雾色
		engine::Fog_Data 雾;
		雾.color[0] = 0.55f;
		雾.color[1] = 0.64f;
		雾.color[2] = 0.78f;
		雾.density = 0.65f;
		雾.start = 4.0f;
		雾.end = 22.0f;
		资源.渲染器.scene_apply(相机数据(0.0f, 0.0f, 10.0f, 0.0f, 0.0f, 0.0f), 光照, 雾);
		//天空盒 ＋ IBL（漫反射 / 镜面补光）
		资源.渲染器.sky_enable_set(true);
		资源.渲染器.sky_texture_set(资源.天空);
		资源.渲染器.sky_params_set(0.55f, 0.65f);
		资源.渲染器.texture_bind_set(0);
		//由近及远摆一排球，观察雾的深度渐变与天光反射
		for (uint32_t 序号 = 0; 序号 < 6; ++序号)
		{
			float z = -2.0f - 3.0f * static_cast<float>(序号);
			画(资源, 资源.球, -3.0f + 1.2f * static_cast<float>(序号), 0.0f, z, 0.9f);
		}
	}

	//—— 阶段 7：后处理链 · 泛光扩散（亮部提取 → 模糊 → 模糊）——
	void 阶段_泛光(展示资源& 资源)
	{
		engine::Light_Data 光照;
		光照.direction[0] = 0.0f;
		光照.direction[1] = -0.3f;
		光照.direction[2] = -1.0f;
		光照.intensity = 0.45f;
		光照.ambient = 0.06f;
		资源.渲染器.scene_apply(相机数据(0.0f, 0.0f, 13.0f, 0.0f, 0.0f, 0.0f), 光照, engine::Fog_Data{});
		//泛光参数：低阈值、软膝、较强强度
		资源.渲染器.bloom_param_set(0.28f, 0.10f, 1.8f);
		//三级链（两台目标轮转）
		uint64_t 链[3] = { 资源.亮部提取, 资源.模糊, 资源.模糊 };
		uint64_t 目标组[2] = { 资源.目标甲, 资源.目标乙 };
		if (资源.渲染器.post_chain_set(链, 3))
			(void)资源.渲染器.render_target_present_chain_set(目标组, 2);
		//暗背景上的自发光物体，泛光扩散最直观
		资源.渲染器.texture_bind_set(0);
		资源.渲染器.material_emissive_set(3.0f);
		资源.渲染器.material_tint_set(1.0f, 0.75f, 0.35f, 1.0f);
		画(资源, 资源.球, -4.5f, 0.8f, 0.0f, 0.9f);
		资源.渲染器.material_tint_set(0.4f, 0.95f, 0.75f, 1.0f);
		画(资源, 资源.球, 0.0f, 1.2f, 0.0f, 0.9f);
		资源.渲染器.material_tint_set(0.45f, 0.6f, 1.0f, 1.0f);
		画(资源, 资源.球, 4.5f, 0.8f, 0.0f, 0.9f);
		资源.渲染器.material_emissive_clear();
		资源.渲染器.material_tint_clear();
		画(资源, 资源.圆环, 0.0f, -2.0f, 0.0f, 1.2f);
	}

	//—— 阶段 8：后处理链 · 分级调色 / 暗角 / 胶片颗粒 / 锐化 ——
	void 阶段_调色(展示资源& 资源)
	{
		engine::Light_Data 光照;
		光照.direction[0] = 0.3f;
		光照.direction[1] = -0.5f;
		光照.direction[2] = -0.8f;
		光照.intensity = 1.0f;
		光照.ambient = 0.25f;
		光照.specular_intensity = 0.4f;
		资源.渲染器.scene_apply(相机数据(0.0f, 0.0f, 13.0f, 0.0f, 0.0f, 0.0f), 光照, engine::Fog_Data{});
		uint64_t 链[4] = { 资源.分级调色, 资源.暗角, 资源.胶片颗粒, 资源.锐化 };
		uint64_t 目标组[2] = { 资源.目标甲, 资源.目标乙 };
		if (资源.渲染器.post_chain_set(链, 4))
			(void)资源.渲染器.render_target_present_chain_set(目标组, 2);
		//分级调色：轻微提亮 ＋ 伽马 ＋ 增益 ＋ 提高饱和度
		资源.渲染器.post_stage_param4_set(0, 0.03f, 1.12f, 1.10f, 1.6f);
		资源.渲染器.bloom_param_set(0.6f, 0.1f, 1.0f);
		//彩色网格铺满画面，便于观察调色与边缘锐化
		资源.渲染器.texture_bind_set(资源.棋盘);
		const engine::Mesh_Handle 网格[4] = { 资源.球, 资源.圆锥, 资源.圆环, 资源.胶囊 };
		for (uint32_t 序号 = 0; 序号 < 4; ++序号)
		{
			float 位置x = -4.5f + 3.0f * static_cast<float>(序号);
			画(资源, 网格[序号], 位置x, 1.3f, 0.0f, 1.0f);
			画(资源, 网格[序号], 位置x, -1.3f, 0.0f, 0.8f);
		}
	}

	//—— 阶段 9：后处理链 · 景深 ——
	void 阶段_景深(展示资源& 资源)
	{
		engine::Light_Data 光照;
		光照.direction[0] = 0.25f;
		光照.direction[1] = -0.45f;
		光照.direction[2] = -0.85f;
		光照.intensity = 1.0f;
		光照.ambient = 0.2f;
		光照.specular_intensity = 0.4f;
		资源.渲染器.scene_apply(相机数据(0.0f, 0.0f, 12.0f, 0.0f, 0.0f, 0.0f), 光照, engine::Fog_Data{});
		//对焦面 6、光圈 2、最大模糊 10
		if (资源.渲染器.post_chain_set(&资源.景深, 1))
		{
			资源.渲染器.post_stage_param_set(0, 6.0f, 2.0f, 10.0f);
			(void)资源.渲染器.render_target_present_set(资源.目标甲);
		}
		//由近及远摆放网格：近处与远景模糊、中央对焦面清晰
		资源.渲染器.texture_bind_set(资源.棋盘);
		画(资源, 资源.球, -3.6f, 0.0f, 5.0f, 0.8f);
		画(资源, 资源.球, 0.0f, 0.0f, 0.0f, 1.0f);
		画(资源, 资源.球, 3.6f, 0.0f, -6.0f, 0.9f);
		画(资源, 资源.圆环, 0.0f, -2.0f, 3.0f, 0.8f);
	}

	//—— 阶段 10：抗锯齿（MSAA ＋ SSAA 叠加）——
	void 阶段_抗锯齿(展示资源& 资源)
	{
		//MSAA 取设备支持上限（不超过 4），再叠加 2× SSAA
		uint32_t 采样 = 资源.渲染器.msaa_max_samples();
		if (采样 > 4)
			采样 = 4;
		资源.渲染器.msaa_samples_set(采样);
		资源.渲染器.ssaa_factor_set(2);
		engine::Light_Data 光照;
		光照.direction[0] = 0.3f;
		光照.direction[1] = -0.5f;
		光照.direction[2] = -0.8f;
		光照.intensity = 1.0f;
		光照.ambient = 0.22f;
		光照.specular_intensity = 0.5f;
		资源.渲染器.scene_apply(相机数据(0.0f, 0.0f, 13.0f, 0.0f, 0.0f, 0.0f), 光照, engine::Fog_Data{});
		资源.渲染器.texture_bind_set(资源.棋盘);
		//细长几何与斜边最能体现抗锯齿效果
		const std::array<float, 16> 斜置圆环 = 绕X变换(-3.0f, 0.5f, 0.0f, 0.55f, 1.0f);
		(void)资源.渲染器.mesh_draw(资源.圆环, 斜置圆环.data());
		画(资源, 资源.圆锥, 0.6f, 0.0f, 0.0f, 1.25f);
		画(资源, 资源.胶囊, 3.8f, -0.6f, 0.0f, 0.75f);
		//细密条纹网格（棋盘纹理自带的斜向高频）
		const std::array<float, 16> 地面矩阵 = 绕X变换(0.0f, -2.0f, 0.0f, -1.5707963f, 1.0f);
		(void)资源.渲染器.mesh_draw(资源.地面, 地面矩阵.data());
	}

	//—— 阶段 11：文字渲染与实时观测 ——
	void 阶段_文字(展示资源& 资源)
	{
		engine::Light_Data 光照;
		光照.intensity = 0.9f;
		光照.ambient = 0.4f;
		资源.渲染器.scene_apply(相机数据(0.0f, 0.0f, 12.0f, 0.0f, 0.0f, 0.0f), 光照, engine::Fog_Data{});
		资源.渲染器.texture_bind_set(资源.棋盘);
		//背景网格做衬底
		画一排(资源, 资源.球, 5, -4.0f, 2.0f, 2.6f, -2.0f, 0.5f);
		if (资源.字体 == 0)
			return;
		//中文与英文混排、显式换行 ＋ 自动换行
		engine::Text_Layout 布局;
		布局.max_width = 620.0f;
		布局.line_height = 30.0f;
		布局.align = engine::Text_Align::Left;
		(void)资源.渲染器.text_draw_layout(资源.字体,
			"文字渲染：中文字形 / 英文混排 / 自动换行 / 行距与字距均可配。\n"
			"Text rendering: CJK glyphs, wrapping, line spacing and tracking.",
			28.0f, 300.0f, 布局);
	}

	/*
	展示阶段（数据驱动；新增能力时在此追加一项）
	名称：简短标签；说明：一句能力描述；应用：配置场景并登记本帧绘制项。
	*/
	struct 展示阶段
	{
		const char* 名称;
		const char* 说明;
		void (*应用)(展示资源&);
	};

	const 展示阶段 阶段表[] = {
		{ "程序化网格与纹理", "八种程序化网格 ＋ 棋盘纹理 ＋ 默认光照", 阶段_网格纹理 },
		{ "光照系统", "方向光 / 补光 / 三盏彩色点光 / 聚光灯 / 半球环境光", 阶段_光照 },
		{ "PBR 材质", "GGX 镜面 / 金属度 / 粗糙度 / 逐物体色调与自发光 / 法线贴图", 阶段_材质 },
		{ "阴影与级联", "方向光阴影 ＋ 软边 ＋ 两级级联", 阶段_阴影 },
		{ "实例化与合批", "64 个普通绘制项自动合并 ＋ 64 实例一次绘制 ＋ 视锥剔除", 阶段_实例化 },
		{ "天空盒与雾", "立方体贴图天空盒 / 简易 IBL ＋ 距离雾", 阶段_天空雾 },
		{ "后处理 · 泛光", "亮部提取 → 模糊 → 模糊（三级链）", 阶段_泛光 },
		{ "后处理 · 调色", "分级调色 → 暗角 → 胶片颗粒 → 锐化（四级链）", 阶段_调色 },
		{ "后处理 · 景深", "对焦距离 / 光圈 / 最大模糊可配", 阶段_景深 },
		{ "抗锯齿", "MSAA 多重采样 ＋ SSAA 超采样叠加", 阶段_抗锯齿 },
		{ "文字渲染", "中文字形 / 英文混排 / 自动换行", 阶段_文字 },
	};

	constexpr uint32_t 阶段数量 = sizeof(阶段表) / sizeof(阶段表[0]);

	//—— 资源生命周期 ——

	//创建全部展示资源（任一失败只影响对应阶段，不影响其余阶段）
	void 资源准备(展示资源& 资源)
	{
		engine::Renderer& 渲染器 = 资源.渲染器;
		资源.宽 = 渲染器.extent_width();
		资源.高 = 渲染器.extent_height();
		//程序化网格
		(void)渲染器.mesh_sphere_load(资源.球, 1.0f, 32, 16);
		(void)渲染器.mesh_cylinder_load(资源.圆柱, 0.8f, 1.8f, 24);
		(void)渲染器.mesh_cone_load(资源.圆锥, 0.9f, 1.8f, 24);
		(void)渲染器.mesh_capsule_load(资源.胶囊, 0.6f, 1.0f, 24, 8);
		(void)渲染器.mesh_torus_load(资源.圆环, 0.8f, 0.3f, 32, 12);
		(void)渲染器.mesh_plane_load(资源.平面, 1.8f, 1.8f);
		(void)渲染器.mesh_grid_load(资源.地面, 20.0f, 14.0f, 1, 1);
		(void)渲染器.mesh_disc_load(资源.圆盘, 0.9f, 32);
		//纹理
		(void)渲染器.texture_load_checkerboard(资源.棋盘, 256, 256, 32);
		const std::vector<uint8_t> 灰像素 = 纯色像素(128, 128, 128);
		(void)渲染器.texture_load(资源.中灰, 灰像素.data(), 2, 2);
		const std::vector<uint8_t> 法线 = 法线像素(64);
		(void)渲染器.texture_load(资源.法线贴图, 法线.data(), 64, 64);
		const std::vector<uint8_t> 粗糙 = 粗糙像素(64);
		(void)渲染器.texture_load(资源.粗糙贴图, 粗糙.data(), 64, 64);
		const std::vector<uint8_t> 天空 = 天空像素(64);
		(void)渲染器.texture_cube_load(资源.天空, 天空.data(), 64, 64);
		//字体（找不到可用字体时字幕退化，不影响场景）
		资源.字体 = 加载字体(渲染器);
		//后处理管线
		资源.亮部提取 = 登记后处理(渲染器, "亮部提取.frag.spv");
		资源.模糊 = 登记后处理(渲染器, "模糊.frag.spv");
		资源.分级调色 = 登记后处理(渲染器, "分级调色.frag.spv");
		资源.暗角 = 登记后处理(渲染器, "暗角.frag.spv");
		资源.胶片颗粒 = 登记后处理(渲染器, "胶片颗粒.frag.spv");
		资源.锐化 = 登记后处理(渲染器, "锐化.frag.spv");
		资源.景深 = 登记后处理(渲染器, "景深.frag.spv");
		//中间目标（与交换链同尺寸，满足同帧后处理 1:1 取源）
		(void)渲染器.render_target_create(资源.目标甲, 资源.宽, 资源.高);
		(void)渲染器.render_target_create(资源.目标乙, 资源.宽, 资源.高);
		//资源创建结果自检（任一失败会直接反映为对应阶段无绘制项）
		EXPECT_GT(资源.球, 0u);
		EXPECT_GT(资源.地面, 0u);
		EXPECT_GT(资源.棋盘, 0u);
		EXPECT_GT(资源.天空, 0u);
		EXPECT_GT(资源.目标甲, 0u);
	}

	//释放展示资源
	void 资源释放(展示资源& 资源)
	{
		engine::Renderer& 渲染器 = 资源.渲染器;
		if (资源.目标乙 != 0)
			渲染器.render_target_destroy(资源.目标乙);
		if (资源.目标甲 != 0)
			渲染器.render_target_destroy(资源.目标甲);
		const uint64_t 管线[7] = { 资源.亮部提取, 资源.模糊, 资源.分级调色, 资源.暗角, 资源.胶片颗粒, 资源.锐化, 资源.景深 };
		for (uint64_t 句柄 : 管线)
		{
			if (句柄 != 0)
				渲染器.pipeline_unload(句柄);
		}
		if (资源.字体 != 0)
			渲染器.font_unload(资源.字体);
		const uint64_t 纹理[5] = { 资源.棋盘, 资源.中灰, 资源.法线贴图, 资源.粗糙贴图, 资源.天空 };
		for (uint64_t 句柄 : 纹理)
		{
			if (句柄 != 0)
				渲染器.texture_unload(句柄);
		}
		const engine::Mesh_Handle 网格[8] = { 资源.球, 资源.圆柱, 资源.圆锥, 资源.胶囊,
			资源.圆环, 资源.平面, 资源.地面, 资源.圆盘 };
		for (engine::Mesh_Handle 句柄 : 网格)
		{
			if (句柄 != 0)
				渲染器.mesh_unload(句柄);
		}
	}

	//—— 字幕 ——

	//在画面左上角绘制阶段标签、说明与实时观测（字体不可用时跳过）
	void 绘制字幕(展示资源& 资源, const 展示阶段& 阶段, uint64_t 帧序号)
	{
		if (资源.字体 == 0)
			return;
		engine::Renderer& 渲染器 = 资源.渲染器;
		//阶段标签
		char 标签[96] = {};
		std::snprintf(标签, sizeof(标签), "● %s", 阶段.名称);
		(void)渲染器.text_draw(资源.字体, 标签, 24.0f, 18.0f);
		//说明（自动换行，宽度按窗口收窄）
		engine::Text_Layout 布局;
		布局.max_width = static_cast<float>(资源.宽 > 48 ? 资源.宽 - 48 : 32);
		布局.line_height = 0.0f;
		(void)渲染器.text_draw_layout(资源.字体, 阶段.说明, 24.0f, 44.0f, 布局);
		//实时观测（每 20 帧刷新一次，避免文本网格缓存反复淘汰）
		if (帧序号 % 20 == 0 || 帧序号 < 2)
		{
			const engine::Frame_Stats 统计 = 渲染器.frame_stats_get();
			char 观测[192] = {};
			std::snprintf(观测, sizeof(观测),
				"frames %llu · drawn %llu · merged %llu · culled %llu · %.2f ms · GPU %s",
				static_cast<unsigned long long>(统计.frames),
				static_cast<unsigned long long>(统计.drawn_items),
				static_cast<unsigned long long>(统计.merged_items),
				static_cast<unsigned long long>(统计.culled_items),
				统计.frame_ms,
				统计.gpu_samples > 0 ? "on" : "off");
			(void)渲染器.text_draw(资源.字体, 观测, 24.0f, static_cast<float>(资源.高) - 40.0f);
		}
	}

	//交互 / 冒烟用窗口配置
	engine::Renderer_Config 展示配置(bool 交互)
	{
		engine::Renderer_Config 配置;
		配置.width = 交互 ? 展示窗口宽 : 冒烟窗口宽;
		配置.height = 交互 ? 展示窗口高 : 冒烟窗口高;
		配置.hidden = !交互;
		配置.title = "白银纪元 · 渲染能力全景展示";
		配置.vsync = true;
		//管线缓存持久化（交互巡览加速二次启动）
		if (交互)
			配置.pipeline_cache_path = "out/渲染展示管线缓存.bin";
		return 配置;
	}
}

//展示用例夹具
class Renderer_Showcase_Test : public ::testing::Test
{
};

/*
交互巡览（人眼验收 / 客户展示）
环境变量 BYJY_SHOWCASE=1 时运行；每阶段约 2.5 秒，自动轮播三轮后结束，中途关闭窗口即退出。
*/
TEST_F(Renderer_Showcase_Test, 交互巡览)
{
	if (std::getenv("BYJY_SHOWCASE") == nullptr)
		GTEST_SKIP() << "未设置 BYJY_SHOWCASE=1，跳过交互巡览（人眼验收用）";

	engine::Renderer 渲染器;
	ASSERT_TRUE(渲染器.init(展示配置(true)));
	展示资源 资源{ 渲染器 };
	资源.交互 = true;
	资源准备(资源);

	uint32_t 阶段序号 = 0;
	uint32_t 本阶段已用帧 = 0;
	uint64_t 总帧数 = 0;
	const uint64_t 总帧上限 = static_cast<uint64_t>(阶段数量) * 每阶段帧数 * 轮播轮数;
	while (!渲染器.close_requested() && 总帧数 < 总帧上限)
	{
		const 展示阶段& 阶段 = 阶段表[阶段序号];
		//阶段切换时复位全部开关，保证自包含
		if (本阶段已用帧 == 0)
			默认恢复(渲染器);
		//配置场景并登记本帧绘制项
		阶段.应用(资源);
		//左上角字幕与右下角实时观测
		绘制字幕(资源, 阶段, 总帧数);
		ASSERT_TRUE(渲染器.frame_render());
		++总帧数;
		++本阶段已用帧;
		//到点切换到下一阶段（轮播）
		if (本阶段已用帧 >= 每阶段帧数)
		{
			本阶段已用帧 = 0;
			阶段序号 = (阶段序号 + 1) % 阶段数量;
		}
	}
	EXPECT_GE(总帧数, static_cast<uint64_t>(阶段数量));
	资源释放(资源);
}

/*
阶段表逐项冒烟（无人值守回归用）
隐藏窗口逐阶段渲染三帧：断言配置成功、产生绘制项、帧计数递增，保证展示代码与接口同步。
*/
TEST_F(Renderer_Showcase_Test, 阶段表逐项冒烟)
{
	engine::Renderer 渲染器;
	ASSERT_TRUE(渲染器.init(展示配置(false)));
	展示资源 资源{ 渲染器 };
	资源准备(资源);

	for (uint32_t 阶段序号 = 0; 阶段序号 < 阶段数量; ++阶段序号)
	{
		const 展示阶段& 阶段 = 阶段表[阶段序号];
		默认恢复(渲染器);
		//登记前的累计登记量（用于区分「未登记」与「登记后被跳过」）
		const uint64_t 登记前 = 渲染器.frame_stats_get().draw_items;
		//每阶段渲染三帧，每帧都重新登记绘制项（绘制项按帧消费，与交互巡览同口径）
		for (uint32_t 帧 = 0; 帧 < 3; ++帧)
		{
			阶段.应用(资源);
			ASSERT_TRUE(渲染器.frame_render()) << "阶段「" << 阶段.名称 << "」第 " << 帧 << " 帧失败";
		}
		const engine::Frame_Stats 统计 = 渲染器.frame_stats_get();
		//本阶段至少登记一个绘制项
		EXPECT_GT(统计.draw_items - 登记前, 0u) << "阶段「" << 阶段.名称 << "」未登记绘制项";
		//单帧至少交给录制一个绘制项（文字阶段只有字幕）
		EXPECT_GT(统计.last_draw_items, 0u) << "阶段「" << 阶段.名称 << "」未产生绘制项";
	}
	EXPECT_EQ(渲染器.frame_stats_get().frames, static_cast<uint64_t>(阶段数量) * 3);
	资源释放(资源);
}
