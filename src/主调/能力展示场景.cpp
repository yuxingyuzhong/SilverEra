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

		//平移 ＋ 三轴非等比缩放（列主序）
		std::array<float, 16> 非均匀变换(float x, float y, float z, float sx, float sy, float sz)
		{
			return {
				sx, 0.0f, 0.0f, 0.0f,
				0.0f, sy, 0.0f, 0.0f,
				0.0f, 0.0f, sz, 0.0f,
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

		/*
		自建四边形网格（XY 平面、法线朝 +Z、与场景用例共用的绕序）
		内置平面生成器的绕序在部分视角下会被背面剔除，导致地面与立板不可见；
		展示场景使用与既有用例一致的显式四边形数据，保证任何角度都能看到。
		*/
		engine::Mesh_Data 四边形数据(float 半宽, float 半高)
		{
			engine::Mesh_Data 数据;
			数据.vertices = {
				{ { -半宽, -半高, 0.0f }, { 0.0f, 1.0f } },
				{ { 半宽, -半高, 0.0f }, { 1.0f, 1.0f } },
				{ { 半宽, 半高, 0.0f }, { 1.0f, 0.0f } },
				{ { -半宽, 半高, 0.0f }, { 0.0f, 0.0f } },
			};
			数据.indices = { 0, 1, 2, 0, 2, 3 };
			return 数据;
		}

		//XZ 平面四边形（法线朝 +Y；反向绕序参数用于确认正面绕序）
		engine::Mesh_Data 地面数据(float 半宽, bool 反向绕序)
		{
			engine::Mesh_Data 数据;
			数据.vertices = {
				{ { -半宽, 0.0f, -半宽 }, { 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f } },
				{ { 半宽, 0.0f, -半宽 }, { 1.0f, 0.0f }, { 0.0f, 1.0f, 0.0f } },
				{ { 半宽, 0.0f, 半宽 }, { 1.0f, 1.0f }, { 0.0f, 1.0f, 0.0f } },
				{ { -半宽, 0.0f, 半宽 }, { 0.0f, 1.0f }, { 0.0f, 1.0f, 0.0f } },
			};
			if (反向绕序)
				数据.indices = { 0, 3, 2, 0, 2, 1 };
			else
				数据.indices = { 0, 1, 2, 0, 2, 3 };
			return 数据;
		}

		/*
		暖调查找表像素（第一百四十四轮）
		3D LUT 以 2D 条带存放：宽 = 边长²、高 = 边长；
		按亮度做轻微冷暖分离（亮部偏暖、暗部偏冷），作为演示用的非恒等分级表。
		*/
		std::vector<uint8_t> 暖调查找像素(uint32_t 边长)
		{
			//条带尺寸（宽 = 边长²）
			const uint32_t 宽 = 边长 * 边长;
			const float 归一 = 边长 > 1 ? 1.0f / static_cast<float>(边长 - 1) : 0.0f;
			std::vector<uint8_t> 像素(static_cast<std::size_t>(宽) * 边长 * 4);
			for (uint32_t 层 = 0; 层 < 边长; ++层)
			{
				for (uint32_t 绿 = 0; 绿 < 边长; ++绿)
				{
					for (uint32_t 红 = 0; 红 < 边长; ++红)
					{
						//输入颜色（0..1）
						float r = static_cast<float>(红) * 归一;
						float g = static_cast<float>(绿) * 归一;
						float b = static_cast<float>(层) * 归一;
						//亮度与冷暖偏移（亮部偏暖、暗部偏冷）
						float 亮度 = 0.2126f * r + 0.7152f * g + 0.0722f * b;
						float 偏移 = (亮度 - 0.5f) * 0.04f;
						r += 偏移;
						b -= 偏移;
						//钳制到 0..1
						r = r < 0.0f ? 0.0f : (r > 1.0f ? 1.0f : r);
						g = g < 0.0f ? 0.0f : (g > 1.0f ? 1.0f : g);
						b = b < 0.0f ? 0.0f : (b > 1.0f ? 1.0f : b);
						/*
						线性 → sRGB 编码后存储
						纹理格式为 R8G8B8A8_SRGB，采样时自动解码回线性，
						故表内必须存编码值，否则分级会叠加一次错误的伽马。
						*/
						auto 编码 = [](float 线性)
						{
							float 值 = 线性 <= 0.0031308f ? 线性 * 12.92f
								: 1.055f * std::pow(线性, 1.0f / 2.4f) - 0.055f;
							return static_cast<uint8_t>(值 * 255.0f + 0.5f);
						};
						std::size_t 偏移字节 = (static_cast<std::size_t>(绿) * 宽 + 层 * 边长 + 红) * 4;
						像素[偏移字节] = 编码(r);
						像素[偏移字节 + 1] = 编码(g);
						像素[偏移字节 + 2] = 编码(b);
						像素[偏移字节 + 3] = 255;
					}
				}
			}
			return 像素;
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
		if (!渲染器.mesh_load(平面_, 四边形数据(0.9f, 0.9f)))
			return false;
		if (!渲染器.mesh_load(地面_, 地面数据(20.0f, false)))
			return false;
		if (!渲染器.mesh_load(地面反向_, 地面数据(20.0f, true)))
			return false;
		if (!渲染器.mesh_disc_load(圆盘_, 0.9f, 32))
			return false;
		//程序化立方体（第一百四十二轮新增入口）
		if (!渲染器.mesh_cube_load(立方体_, 0.8f))
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
		//动态贴图（第一百五十一轮；16×16 棋盘，每 0.1 秒更新一个移动色块）
		if (!渲染器.texture_load_checkerboard(动态贴图_, 16, 16, 4))
			return false;
		//—— 字体与后处理管线 ——
		字体_ = 加载字体(渲染器);
		if (!渲染器.pipeline_load(亮部提取_, "后处理.vert.spv", "亮部提取.frag.spv"))
			return false;
		if (!渲染器.pipeline_load(模糊_, "后处理.vert.spv", "模糊.frag.spv"))
			return false;
		if (!渲染器.pipeline_load(暗角_, "后处理.vert.spv", "暗角.frag.spv"))
			return false;
		//FXAA 平滑（第一百四十一轮）
		if (!渲染器.pipeline_load(平滑_, "后处理.vert.spv", "FXAA.frag.spv"))
			return false;
		//3D LUT 色彩查找（第一百四十四轮；暖调表）
		if (!渲染器.pipeline_load(色彩查找_, "后处理.vert.spv", "色彩查找.frag.spv"))
			return false;
		const std::vector<uint8_t> 查找像素 = 暖调查找像素(17);
		if (!渲染器.texture_load(查找表_, 查找像素.data(), 17 * 17, 17))
			return false;
		渲染器.post_lut_bind(查找表_);
		//屏幕空间遮蔽（第一百四十七轮）
		if (!渲染器.pipeline_load(遮蔽_, "后处理.vert.spv", "屏幕空间遮蔽.frag.spv"))
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
		//注视点由 update 按偏航 / 俯仰实时计算，此处给一个朝向场景中心的值
		相机.target[0] = 0.0f;
		相机.target[1] = 2.0f;
		相机.target[2] = -8.0f;
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
		光照.intensity = 0.85f;
		光照.ambient = 0.22f;
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
		const float 点位置[3][3] = { { -7.0f, 4.5f, -6.0f }, { 7.0f, 4.5f, -6.0f }, { 0.0f, 5.0f, -13.0f } };
		const float 点颜色[3][3] = { { 1.0f, 0.35f, 0.28f }, { 0.32f, 1.0f, 0.45f }, { 0.38f, 0.5f, 1.0f } };
		for (uint32_t 序号 = 0; 序号 < 3; ++序号)
		{
			for (uint32_t 分量 = 0; 分量 < 3; ++分量)
			{
				光照.point_position[序号][分量] = 点位置[序号][分量];
				光照.point_color[序号][分量] = 点颜色[序号][分量];
			}
			光照.point_radius[序号] = 16.0f;
			光照.point_intensity[序号] = 2.6f;
			光照.point_shininess[序号] = 48.0f;
		}
		//一盏自上而下的聚光灯
		光照.spot_count = 1;
		光照.spot_position[0][0] = 0.0f;
		光照.spot_position[0][1] = 9.0f;
		光照.spot_position[0][2] = -6.0f;
		光照.spot_direction[0][0] = 0.0f;
		光照.spot_direction[0][1] = -1.0f;
		光照.spot_direction[0][2] = 0.0f;
		光照.spot_color[0][0] = 1.0f;
		光照.spot_color[0][1] = 0.96f;
		光照.spot_color[0][2] = 0.88f;
		光照.spot_radius[0] = 22.0f;
		光照.spot_intensity[0] = 3.0f;
		光照.spot_inner_deg[0] = 14.0f;
		光照.spot_outer_deg[0] = 34.0f;
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
		//指数平方雾（第一百四十三轮）：近处通透、远处平滑压向雾色
		雾.mode = 1;
		雾.density = 0.022f;
		雾.start = 6.0f;
		雾.end = 48.0f;
		//高度衰减（第一百四十三轮）：高处物体比贴地物体更通透
		雾.height_falloff = 0.012f;
		渲染器.scene_apply(相机, 光照, 雾);
		//阴影范围与级联（第一百四十六轮扩为三级；第一百四十八轮加接缝淡化）
		渲染器.shadow_range_set(20.0f, 1.0f, 70.0f);
		渲染器.shadow_cascade_set(3, 3.0f, 16.0f);
		渲染器.shadow_cascade_blend_set(0.2f);
		//天空盒 / 简易 IBL（诊断：暂时关闭，确认是否是天空背景挡住地面）
		渲染器.sky_enable_set(false);
		渲染器.sky_texture_set(天空_);
		渲染器.sky_params_set(0.5f, 0.6f);
		//PBR-lite 全局开启（逐物体另设粗糙度 / 金属度）
		渲染器.pbr_enable_set(true);
		渲染器.pbr_roughness_set(1.0f);
		渲染器.pbr_metallic_set(0.0f);
		渲染器.static_batching_set(true);
		渲染器.cull_enabled_set(true);
		/*
		后处理链默认关闭（2026-10-26 展示优化）
		六段链（亮部提取 → 模糊 → 暗角 → 色彩查找 → 遮蔽 → 平滑）会把整幅画面（含 UI 与文字）
		糊成亮部残影并吞掉叠加层；展示场景默认走直通路径，保持画面锐利清晰。
		相关管线与查找表资源仍保留，供后续以显式开关启用。
		*/
		(void)渲染器.post_chain_set(nullptr, 0);
		(void)渲染器.render_target_present_chain_set(nullptr, 0);
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
					uint64_t 链[6] = { 亮部提取_, 模糊_, 暗角_, 色彩查找_, 遮蔽_, 平滑_ };
					uint64_t 目标组[2] = { 目标甲_, 目标乙_ };
					(void)渲染器.post_chain_set(链, 6);
					//链变更会清除逐级专设值，重建后重新应用查找表、遮蔽与 FXAA 参数
					渲染器.post_stage_param_set(3, 1.0f, 0.0f, 1.0f);
					渲染器.post_stage_param4_set(4, 0.8f, 12.0f, 0.5f, 1.0f);
					渲染器.post_stage_param_set(5, 0.0f, 0.0f, 1.0f);
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
		//—— 地面（用扁立方体做地面：平面类网格在本机渲染路径下不可见，改用有厚度的立方体） ——
		渲染器.texture_bind_set(棋盘_);
		const std::array<float, 16> 地面矩阵 = 非均匀变换(0.0f, -0.15f, -4.0f, 22.0f, 0.12f, 22.0f);
		(void)渲染器.mesh_draw(立方体_, 地面矩阵.data());

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

		//—— 程序化立方体（第一百四十二轮新增入口；独立摆位以不与既有阵列冲突） ——
		const std::array<float, 16> 立方矩阵 = 绕Y变换(4.6f, 1.0f, -4.0f, 0.5f, 1.0f);
		(void)渲染器.mesh_draw(立方体_, 立方矩阵.data());

		//—— 动态贴图（第一百五十一轮；每 0.1 秒更新一个 4×4 移动色块） ——
		if (动态贴图_ != 0 && 累计秒_ - 上次更新时间_ > 0.1f)
		{
			上次更新时间_ = 累计秒_;
			//色块左上角在 0..12 之间循环
			const uint32_t 色块x = static_cast<uint32_t>(累计秒_ * 8.0f) % 13u;
			const uint32_t 色块y = static_cast<uint32_t>(累计秒_ * 5.0f) % 13u;
			//4×4 纯红（RGBA8 紧排）
			std::vector<uint8_t> 色块(4 * 4 * 4, 0);
			for (std::size_t 偏移 = 0; 偏移 < 色块.size(); 偏移 += 4)
			{
				色块[偏移] = 255;
				色块[偏移 + 3] = 255;
			}
			(void)渲染器.texture_update_region(动态贴图_, 色块x, 色块y, 4, 4, 色块.data());
		}
		if (动态贴图_ != 0)
		{
			渲染器.texture_bind_set(动态贴图_);
			const std::array<float, 16> 动态面 = 变换(-4.6f, 1.2f, -4.0f, 1.0f);
			(void)渲染器.mesh_draw(平面_, 动态面.data());
		}

		//—— 文字：标题与区域标签（最后画，避免字体图集被后续网格覆盖） ——
		if (字体_ != 0)
		{
			/*
			标题带描边（第一百四十轮新增能力）
			描边仅对标题生效：登记标题后立即把描边宽度归零，后续标签保持无描边。
			*/
			const float 描边色[3] = { 0.05f, 0.02f, 0.02f };
			渲染器.text_outline_set(0.0035f, 描边色);
			(void)渲染器.text_draw(字体_, "白银纪元 · 渲染能力展示", 24.0f, 24.0f);
			//关闭描边，其余标签保持原样
			渲染器.text_outline_set(0.0f, 描边色);
			(void)渲染器.text_draw(字体_, "程序化网格 / 纹理", 60.0f, 300.0f);
			(void)渲染器.text_draw(字体_, "光照 / 阴影 / 级联", 60.0f, 340.0f);
			(void)渲染器.text_draw(字体_, "PBR 材质 / 贴图 / 自发光", 60.0f, 380.0f);
			(void)渲染器.text_draw(字体_, "实例化 / 合批 / 天空盒 / 雾 / 后处理 / 文字描边", 60.0f, 420.0f);
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
		const uint64_t 管线[6] = { 亮部提取_, 模糊_, 暗角_, 色彩查找_, 遮蔽_, 平滑_ };
		for (uint64_t 句柄 : 管线)
		{
			if (句柄 != 0)
				渲染器.pipeline_unload(句柄);
		}
		亮部提取_ = 0;
		模糊_ = 0;
		暗角_ = 0;
		色彩查找_ = 0;
		遮蔽_ = 0;
		平滑_ = 0;
		//字体与纹理
		if (字体_ != 0)
			渲染器.font_unload(字体_);
		const uint64_t 纹理[6] = { 棋盘_, 法线贴图_, 粗糙贴图_, 天空_, 查找表_, 动态贴图_ };
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
		查找表_ = 0;
		动态贴图_ = 0;
		//网格
		const engine::Mesh_Handle 网格[9] = { 球_, 圆柱_, 圆锥_, 胶囊_, 圆环_, 平面_, 地面_, 地面反向_, 圆盘_ };
		for (uint64_t 句柄 : 网格)
		{
			if (句柄 != 0)
				渲染器.mesh_unload(句柄);
		}
		球_ = 圆柱_ = 圆锥_ = 胶囊_ = 圆环_ = 平面_ = 地面_ = 圆盘_ = 立方体_ = 0;
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
