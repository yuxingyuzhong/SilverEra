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

		/*
		地板网格像素（深色底 ＋ 浅色细网格线）
		大面积地面与展区地台共用：细线网格提供尺度参照，比高对比棋盘更耐看、不抢物体。
		*/
		std::vector<uint8_t> 地板像素(uint32_t 边长, uint32_t 格距)
		{
			std::vector<uint8_t> 像素(static_cast<std::size_t>(边长) * 边长 * 4);
			for (uint32_t y = 0; y < 边长; ++y)
			{
				for (uint32_t x = 0; x < 边长; ++x)
				{
					//底色 96 / 网格线 168（线宽 2 像素）
					const bool 线上 = (x % 格距) < 2 || (y % 格距) < 2;
					const uint8_t 值 = 线上 ? 168 : 96;
					const std::size_t 偏移 = (static_cast<std::size_t>(y) * 边长 + x) * 4;
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
		/*
		程序化正二十面体球（第一百九十二轮；卡通渲染区专用）
		纯三角面拓扑（细分 2），更接近二次元角色 / 道具的建模习惯，
		在色阶量化与硬边高光下能看出「低多边形 ＋ 卡通着色」的典型观感。
		*/
		if (!渲染器.mesh_icosphere_load(卡通球_, 1.0f, 2))
			return false;
		/*
		程序化 XZ 水平细分网格（第一百九十五轮；展区①的细分地形底板）
		8×8 分段的 9×9 单位水平网格，用于直观呈现「顶点密度可控」的地形类网格。
		*/
		if (!渲染器.mesh_ground_grid_load(地形_, 9.0f, 9.0f, 8, 8))
			return false;
		//—— 纹理 ——
		if (!渲染器.texture_load_checkerboard(棋盘_, 256, 256, 32))
			return false;
		//地板网格纹理（细线格，供地面与地台共用）
		const std::vector<uint8_t> 地板 = 地板像素(256, 64);
		if (!渲染器.texture_load(地板_, 地板.data(), 256, 256))
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
		/*
		地面材质（双面渲染）
		地面用大面积平面铺设：若正面绕序与本机剔除方向不一致，单面材质会让整块地面消失；
		双面材质关闭背面剔除，无论绕序如何都能看到，且把棋盘纹理与材质绑定合并为一次提交。
		*/
		Material_Data 地面描述;
		地面描述.texture = 地板_;
		地面描述.double_sided = true;
		地面描述.pbr_override = true;
		地面描述.pbr_roughness = 1.0f;
		地面描述.pbr_metallic = 0.0f;
		地面描述.uv_scale[0] = 6.0f;
		地面描述.uv_scale[1] = 6.0f;
		if (!渲染器.material_load(地面材质_, 地面描述))
			return false;
		/*
		法线 / 粗糙度贴图材质（一次登记，逐帧复用）
		此前每次绘制都登记并卸载材质，属于每帧重复分配；改为 init 登记、shutdown 卸载。
		*/
		Material_Data 贴图描述;
		贴图描述.pbr_override = true;
		贴图描述.pbr_roughness = 0.7f;
		贴图描述.pbr_metallic = 0.35f;
		贴图描述.normal_texture = 法线贴图_;
		贴图描述.roughness_texture = 粗糙贴图_;
		贴图描述.normal_strength = 1.5f;
		if (!渲染器.material_load(贴图材质_, 贴图描述))
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
		//注视点由 update 按偏航 / 俯仰实时计算，此处给一个朝向场景纵深的值
		相机.target[0] = 0.0f;
		相机.target[1] = 3.0f;
		相机.target[2] = -20.0f;
		相机.fov = 1.30f;
		相机.far_plane = 200.0f;
		相机.clear_color[0] = 0.03f;
		相机.clear_color[1] = 0.05f;
		相机.clear_color[2] = 0.09f;
		相机.clear_color[3] = 1.0f;
		Light_Data 光照;
		//主方向光（暖色）
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
		//主光强度（压低到不过曝；此前 0.85 ＋ 多盏补光把地面推到纯白）
		光照.intensity = 0.85f;
		光照.ambient = 0.06f;
		//补光（冷色）
		光照.fill_direction[0] = -0.5f;
		光照.fill_direction[1] = -0.25f;
		光照.fill_direction[2] = 0.55f;
		光照.fill_color[0] = 0.42f;
		光照.fill_color[1] = 0.62f;
		光照.fill_color[2] = 1.0f;
		光照.fill_intensity = 0.18f;
		//三盏彩色点光源（分布在材质区 / 拼接区 / 纹理区，制造冷暖层次）
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
			//点光强度压低，避免近处表面被推到纯白（纹理与色调全都看不见）
			光照.point_intensity[序号] = 0.55f;
			光照.point_shininess[序号] = 48.0f;
		}
		//一盏自上而下的聚光灯（正对光照 / 阴影展示区）
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
		//聚光强度压低，避免整片地面过曝
		光照.spot_intensity[0] = 1.3f;
		光照.spot_inner_deg[0] = 16.0f;
		光照.spot_outer_deg[0] = 38.0f;
		光照.specular_intensity = 0.0f;
		光照.specular_shininess = 96.0f;
		//半球环境光（天光偏冷、地面反光偏暖；强度压到不参与过曝）
		光照.ambient_color[0] = 0.32f;
		光照.ambient_color[1] = 0.36f;
		光照.ambient_color[2] = 0.44f;
		光照.ambient_ground[0] = 0.28f;
		光照.ambient_ground[1] = 0.24f;
		光照.ambient_ground[2] = 0.20f;
		光照.ambient_hemisphere = 0.15f;
		//阴影（软边 ＋ 两级级联）
		光照.shadow_enabled = 1;
		光照.shadow_bias = 0.004f;
		光照.shadow_softness = 0.35f;
		光照.shadow_strength = 1.0f;
		Fog_Data 雾;
		雾.color[0] = 0.52f;
		雾.color[1] = 0.62f;
		雾.color[2] = 0.78f;
		/*
		线性雾（改为远距离才起效）
		指数平方雾从 6 米就开始压暗，导致「越远越模糊、近远观感差别巨大」；
		改为线性雾并把起雾距离推到 90 米，只给远景补一点空间层次，近处保持锐利。
		*/
		雾.mode = 0;
		雾.density = 0.0f;
		雾.start = 60.0f;
		雾.end = 170.0f;
		//高度衰减（第一百四十三轮）：高处物体比贴地物体更通透
		雾.height_falloff = 0.012f;
		渲染器.scene_apply(相机, 光照, 雾);
		//阴影范围与级联（覆盖整条展廊：三片 × 远处倍增）
		渲染器.shadow_range_set(30.0f, 1.0f, 130.0f);
		渲染器.shadow_cascade_set(3, 18.0f, 2.2f);
		渲染器.shadow_cascade_blend_set(0.18f);
		//天空盒 / 简易 IBL（渐变天空提供背景层次与环境反射，不再是大块纯蓝）
		渲染器.sky_enable_set(true);
		渲染器.sky_texture_set(天空_);
		//天空 IBL 强度压低：否则整幅画面被蓝色天光冲成单色
		渲染器.sky_params_set(0.12f, 0.20f);
		//PBR-lite 全局开启（逐物体另设粗糙度 / 金属度）
		渲染器.pbr_enable_set(true);
		渲染器.pbr_roughness_set(1.0f);
		渲染器.pbr_metallic_set(0.0f);
		渲染器.static_batching_set(true);
		渲染器.cull_enabled_set(true);
		/*
		多线程渲染演示（第一百九十八轮）
		4 个工作线程：实例级剔除与实例缓冲差异扫描并行；若走到「场景目标 → 同帧后处理链」路径，
		场景 pass 还会按段并行录制（面板「最近一帧并行段」显示段数，直通路径恒 0）。
		*/
		渲染器.render_threads_set(4);
		//多重采样抗锯齿（4×；几何体边缘与棋盘格纹路的锯齿明显，SSAA 不再叠加）
		渲染器.msaa_samples_set(4);
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
					/*
					重建目标但保持直通路径（第二百一十八轮修正）
					init 已明确关闭后处理链（六段链会把整幅画面连叠加层一起糊成残影），
					而此处此前会重新启用 6 级链并切换呈现链，导致「窗口一改尺寸画面就被链吞掉」。
					故 resize 只重建目标（供后续显式启用），不改变链与呈现路径。
					*/
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
		/*
		卡通渲染区切换（第一百九十二轮）
		渲染层的风格化参数是全局的（Scene_Data 级），故不做局部样板；
		改为「进入第七展区即整幅画面切换为卡通风格、离开即恢复写实」，用空间位置做模式开关。
		仅在状态变化时调用门面，避免逐帧重复设置。
		*/
		const float 卡通区x = 0.0f;
		const float 卡通区z = -50.0f;
		//相机与卡通区中心的水平距离平方（判定半径 9 单位）
		const float 距x = 位置_[0] - 卡通区x;
		const float 距z = 位置_[2] - 卡通区z;
		const bool 进入卡通区 = (距x * 距x + 距z * 距z) < 81.0f;
		//若激活状态变化
		if (进入卡通区 != 卡通区激活_)
		{
			卡通区激活_ = 进入卡通区;
			//四阶段：色阶量化（4 档）＋ 菲涅尔边缘光 ＋ 暗部冷紫色调 ＋ 卡通高光阈值
			渲染器.toon_set(进入卡通区 ? 4u : 0u, 进入卡通区 ? 0.65f : 0.0f);
			渲染器.toon_shadow_tint_set(进入卡通区 ? 0.42f : 0.0f,
				进入卡通区 ? 0.46f : 0.0f, 进入卡通区 ? 0.72f : 0.0f);
			渲染器.toon_specular_set(进入卡通区 ? 0.35f : 0.0f);
		}
	}

	//登记本帧全部绘制项与字幕
	void 能力展示场景::frame_draw(Renderer& 渲染器)
	{
		/*
		展廊布局（两排三列的展区网格）
		每个展区拥有独立地台色块、四角立柱与三维投影标签，
		对应「展示区域有效分割」与「标明各区域展示的效果」两项要求。
		*/
		struct 展区
		{
			//中心 X
			float x;
			//中心 Z
			float z;
			//地台色
			float 地色[3];
			//区域标题
			const char* 标题;
		};
		/*
		展区网格（前两排各三列，第三排为卡通渲染区；第一百九十二轮增第七区）
		第一排距离相机约 22 单位，第二排约 42 单位，第三排约 62 单位；每区 12×12 地台。
		*/
		const 展区 展区表[7] = {
			{ -14.0f, -10.0f, { 0.46f, 0.52f, 0.70f }, "① 程序化网格（含细分地形）" },
			{ 0.0f,   -10.0f, { 0.74f, 0.52f, 0.40f }, "② PBR 材质" },
			{ 14.0f,  -10.0f, { 0.40f, 0.66f, 0.60f }, "③ 光照与阴影" },
			{ -14.0f, -30.0f, { 0.58f, 0.46f, 0.74f }, "④ 实例化合批" },
			{ 0.0f,   -30.0f, { 0.76f, 0.60f, 0.34f }, "⑤ 几何拼接" },
			{ 14.0f,  -30.0f, { 0.50f, 0.62f, 0.46f }, "⑥ 纹理与动态" },
			{ 0.0f,   -50.0f, { 0.44f, 0.50f, 0.68f }, "⑦ 卡通渲染（走近切换）" },
		};
		//圆周常数（环形排布与环绕动画用）
		const float 圆周率 = 3.14159265f;

		//—— 基础地面（双面材质：绕序无关，杜绝大面积平面被背面剔除） ——
		渲染器.material_bind_set(地面材质_);
		//地面基调（冷灰，压暗以免抢走展区色彩）
		渲染器.material_tint_set(0.24f, 0.26f, 0.31f, 1.0f);
		const std::array<float, 16> 地面矩阵 = 变换(0.0f, -0.02f, 0.0f, 8.0f);
		(void)渲染器.mesh_draw(地面_, 地面矩阵.data());
		渲染器.material_tint_clear();

		//—— 展区地台（每区一块彩色地砖，作为分区与地面参照） ——
		for (const 展区& 区 : 展区表)
		{
			渲染器.material_tint_set(区.地色[0], 区.地色[1], 区.地色[2], 1.0f);
			const std::array<float, 16> 地台 = 变换(区.x, 0.02f, 区.z, 0.30f);
			(void)渲染器.mesh_draw(地面_, 地台.data());
		}
		渲染器.material_tint_clear();
		渲染器.material_bind_set(0);

		//—— 展区角柱（每区四角一根，一次实例化提交，形成清晰边界） ——
		{
			std::vector<float> 柱矩阵;
			柱矩阵.reserve(28 * 16);
			for (const 展区& 区 : 展区表)
			{
				for (int 横 = -1; 横 <= 1; 横 += 2)
				{
					for (int 纵 = -1; 纵 <= 1; 纵 += 2)
					{
						const std::array<float, 16> 柱 = 非均匀变换(区.x + 5.7f * static_cast<float>(横), 0.36f,
							区.z + 5.7f * static_cast<float>(纵), 0.16f, 0.45f, 0.16f);
						柱矩阵.insert(柱矩阵.end(), 柱.begin(), 柱.end());
					}
				}
			}
			//石灰色角柱（与彩色地台形成对比）
			渲染器.material_tint_set(0.42f, 0.44f, 0.48f, 1.0f);
			(void)渲染器.mesh_draw_instanced(立方体_, 柱矩阵.data(), 24);
			渲染器.material_tint_clear();
		}

		//—— 展区①：八种程序化网格（圆台 ＋ 环形排布）＋ 细分地形底板 ——
		{
			//细分地形底板（第一百九十五轮；9×9 单位、8×8 段，略高于地台避免 z-fighting）
			if (地形_ != 0)
			{
				渲染器.material_tint_set(0.34f, 0.40f, 0.52f, 1.0f);
				const std::array<float, 16> 地形矩阵 = 变换(-14.0f, 0.06f, -10.0f, 1.0f);
				(void)渲染器.mesh_draw(地形_, 地形矩阵.data());
				渲染器.material_tint_clear();
			}
			//圆台底座
			渲染器.material_tint_set(0.30f, 0.34f, 0.44f, 1.0f);
			const std::array<float, 16> 圆台 = 非均匀变换(-14.0f, 0.25f, -10.0f, 2.4f, 0.25f, 2.4f);
			(void)渲染器.mesh_draw(圆柱_, 圆台.data());
			渲染器.material_tint_clear();
			//八种图元环形排布（逐物体色调）
			const engine::Mesh_Handle 网格排[8] = { 球_, 圆柱_, 圆锥_, 胶囊_, 圆环_, 平面_, 圆盘_, 立方体_ };
			const float 色调表[8][3] = {
				{ 1.0f, 0.45f, 0.40f }, { 1.0f, 0.65f, 0.30f }, { 1.0f, 0.92f, 0.45f }, { 0.45f, 0.95f, 0.55f },
				{ 0.40f, 0.90f, 0.95f }, { 0.45f, 0.62f, 1.0f }, { 0.72f, 0.50f, 1.0f }, { 1.0f, 0.55f, 0.85f },
			};
			for (uint32_t 序号 = 0; 序号 < 8; ++序号)
			{
				const float 角度 = 圆周率 * 2.0f * static_cast<float>(序号) / 8.0f;
				const float x = -14.0f + std::cos(角度) * 1.7f;
				const float z = -10.0f + std::sin(角度) * 1.7f;
				渲染器.material_tint_set(色调表[序号][0], 色调表[序号][1], 色调表[序号][2], 1.0f);
				//竖立图元统一放在圆台上方
				const std::array<float, 16> 矩阵 = 变换(x, 0.95f, z, 0.55f);
				(void)渲染器.mesh_draw(网格排[序号], 矩阵.data());
			}
			渲染器.material_tint_clear();
		}

		//—— 展区②：PBR 材质（粗糙度 / 金属度递增 ＋ 法线与粗糙度贴图） ——
		{
			渲染器.texture_bind_set(0);
			const float 粗糙度[4] = { 0.10f, 0.35f, 0.65f, 0.92f };
			for (uint32_t 序号 = 0; 序号 < 4; ++序号)
			{
				(void)渲染器.material_pbr_set(粗糙度[序号], 0.0f);
				const std::array<float, 16> 矩阵 = 变换(-4.5f + 2.2f * static_cast<float>(序号), 0.75f, -8.0f, 0.7f);
				(void)渲染器.mesh_draw(球_, 矩阵.data());
			}
			//金属度递增（粗糙度固定 0.3）
			for (uint32_t 序号 = 0; 序号 < 4; ++序号)
			{
				(void)渲染器.material_pbr_set(0.3f, 0.15f + 0.25f * static_cast<float>(序号));
				const std::array<float, 16> 矩阵 = 变换(-4.5f + 2.2f * static_cast<float>(序号), 0.75f, -11.5f, 0.7f);
				(void)渲染器.mesh_draw(球_, 矩阵.data());
			}
			(void)渲染器.material_pbr_clear();
			//法线 / 粗糙度贴图球（一次登记的材质）
			if (贴图材质_ != 0)
			{
				渲染器.material_bind_set(贴图材质_);
				const std::array<float, 16> 贴图球 = 变换(4.4f, 0.9f, -9.8f, 0.85f);
				(void)渲染器.mesh_draw(球_, 贴图球.data());
				渲染器.material_bind_set(0);
			}
			//自发光标记（示意材质的 emissive 通道）
			渲染器.material_emissive_set(3.0f);
			渲染器.material_tint_set(0.45f, 0.85f, 1.0f, 1.0f);
			const std::array<float, 16> 自发光 = 变换(4.6f, 2.0f, -11.8f, 0.5f);
			(void)渲染器.mesh_draw(圆环_, 自发光.data());
			渲染器.material_emissive_clear();
			渲染器.material_tint_clear();
		}

		//—— 展区③：光照与阴影（四根立柱 ＋ 顶部聚光） ——
		{
			for (uint32_t 序号 = 0; 序号 < 4; ++序号)
			{
				const float x = 11.4f + 1.7f * static_cast<float>(序号);
				//柱身（圆柱）
				const std::array<float, 16> 柱身 = 非均匀变换(x, 1.0f, -10.0f, 0.4f, 1.2f, 0.4f);
				(void)渲染器.mesh_draw(圆柱_, 柱身.data());
				//柱头（立方体）
				const std::array<float, 16> 柱头 = 变换(x, 2.2f, -10.0f, 0.4f);
				(void)渲染器.mesh_draw(立方体_, 柱头.data());
			}
			/*
			聚光灯自身在展区地面上形成光池，不再额外绘制自发光圆盘
			（自发光圆盘贴近地面时会变成一块刺眼白斑，干扰「阴影」的观察）。
			*/
			//三盏点光源的可见光源球（与场景点光位置一一对应）
			const float 光源位置[3][3] = { { 0.0f, 6.0f, -10.0f }, { -14.0f, 6.0f, -30.0f }, { 14.0f, 6.0f, -30.0f } };
			const float 光源颜色[3][3] = { { 1.0f, 0.42f, 0.30f }, { 0.34f, 0.78f, 1.0f }, { 1.0f, 0.86f, 0.52f } };
			for (uint32_t 序号 = 0; 序号 < 3; ++序号)
			{
				渲染器.material_emissive_set(4.0f);
				渲染器.material_tint_set(光源颜色[序号][0], 光源颜色[序号][1], 光源颜色[序号][2], 1.0f);
				const std::array<float, 16> 光源球 = 变换(光源位置[序号][0], 光源位置[序号][1], 光源位置[序号][2], 0.22f);
				(void)渲染器.mesh_draw(球_, 光源球.data());
			}
			渲染器.material_emissive_clear();
			渲染器.material_tint_clear();
		}

		//—— 展区④：实例化与合批（阶梯金字塔 ＋ 环形小球阵，两次实例化提交） ——
		{
			std::vector<float> 阶梯矩阵;
			阶梯矩阵.reserve(55 * 16);
			const uint32_t 层数表[5] = { 5, 4, 3, 2, 1 };
			for (uint32_t 层 = 0; 层 < 5; ++层)
			{
				const uint32_t 边长 = 层数表[层];
				const float 半 = (static_cast<float>(边长) - 1.0f) * 0.5f;
				for (uint32_t 行 = 0; 行 < 边长; ++行)
				{
					for (uint32_t 列 = 0; 列 < 边长; ++列)
					{
						const float x = -14.0f + (static_cast<float>(列) - 半) * 1.15f;
						const float z = -30.0f + (static_cast<float>(行) - 半) * 1.15f;
						const float y = 0.36f + static_cast<float>(层) * 0.72f;
						const std::array<float, 16> 块 = 变换(x, y, z, 0.45f);
						阶梯矩阵.insert(阶梯矩阵.end(), 块.begin(), 块.end());
					}
				}
			}
			//阶梯色调（紫蓝）
			渲染器.material_tint_set(0.58f, 0.52f, 0.92f, 1.0f);
			(void)渲染器.mesh_draw_instanced(立方体_, 阶梯矩阵.data(), static_cast<uint32_t>(阶梯矩阵.size() / 16));
			渲染器.material_tint_clear();
			//环绕小球阵（第二类实例化）
			std::vector<float> 球阵;
			球阵.reserve(12 * 16);
			for (uint32_t 序号 = 0; 序号 < 12; ++序号)
			{
				const float 角度 = 圆周率 * 2.0f * static_cast<float>(序号) / 12.0f;
				const float x = -14.0f + std::cos(角度) * 4.6f;
				const float z = -30.0f + std::sin(角度) * 4.6f;
				const std::array<float, 16> 球 = 变换(x, 0.35f, z, 0.3f);
				球阵.insert(球阵.end(), 球.begin(), 球.end());
			}
			渲染器.material_tint_set(1.0f, 0.78f, 0.36f, 1.0f);
			(void)渲染器.mesh_draw_instanced(球_, 球阵.data(), 12);
			渲染器.material_tint_clear();
		}

		//—— 展区⑤：复杂几何拼接（拱门 ＋ 晶塔 ＋ 环阵） ——
		{
			渲染器.material_tint_set(0.72f, 0.66f, 0.52f, 1.0f);
			//拱门立柱
			const std::array<float, 16> 左柱 = 非均匀变换(-4.4f, 2.2f, -30.0f, 0.28f, 2.4f, 0.28f);
			const std::array<float, 16> 右柱 = 非均匀变换(-1.6f, 2.2f, -30.0f, 0.28f, 2.4f, 0.28f);
			(void)渲染器.mesh_draw(立方体_, 左柱.data());
			(void)渲染器.mesh_draw(立方体_, 右柱.data());
			//拱门横梁
			const std::array<float, 16> 横梁 = 非均匀变换(-3.0f, 4.5f, -30.0f, 1.9f, 0.2f, 0.28f);
			(void)渲染器.mesh_draw(立方体_, 横梁.data());
			//拱门顶环
			const std::array<float, 16> 顶环 = 绕Y变换(-3.0f, 5.1f, -30.0f, 0.0f, 0.75f);
			(void)渲染器.mesh_draw(圆环_, 顶环.data());
			//晶塔基座 / 塔身 / 锥顶
			const std::array<float, 16> 塔基 = 非均匀变换(2.4f, 0.55f, -30.0f, 1.6f, 0.42f, 1.6f);
			const std::array<float, 16> 塔身 = 非均匀变换(2.4f, 1.5f, -30.0f, 1.1f, 0.6f, 1.1f);
			const std::array<float, 16> 塔顶 = 非均匀变换(2.4f, 2.9f, -30.0f, 0.8f, 0.8f, 0.8f);
			(void)渲染器.mesh_draw(圆柱_, 塔基.data());
			(void)渲染器.mesh_draw(圆柱_, 塔身.data());
			(void)渲染器.mesh_draw(圆锥_, 塔顶.data());
			//晶塔能量球（自发光）
			渲染器.material_emissive_set(2.5f);
			渲染器.material_tint_set(0.55f, 0.85f, 1.0f, 1.0f);
			const std::array<float, 16> 能量球 = 变换(2.4f, 3.75f, -30.0f, 0.3f);
			(void)渲染器.mesh_draw(球_, 能量球.data());
			//环绕晶塔的两道旋转环
			渲染器.material_tint_set(0.72f, 0.66f, 0.52f, 1.0f);
			const std::array<float, 16> 晶环甲 = 绕Y变换(2.4f, 2.2f, -30.0f, 累计秒_ * 0.7f, 1.2f);
			const std::array<float, 16> 晶环乙 = 绕Y变换(2.4f, 1.3f, -30.0f, -累计秒_ * 1.1f, 1.0f);
			(void)渲染器.mesh_draw(圆环_, 晶环甲.data());
			(void)渲染器.mesh_draw(圆环_, 晶环乙.data());
			//环阵（大环 ＋ 中心球 ＋ 环绕小球）
			const std::array<float, 16> 大环 = 绕Y变换(4.6f, 1.8f, -33.0f, 累计秒_ * 0.35f, 1.5f);
			(void)渲染器.mesh_draw(圆环_, 大环.data());
			const std::array<float, 16> 中心球 = 变换(4.6f, 1.8f, -33.0f, 0.45f);
			(void)渲染器.mesh_draw(球_, 中心球.data());
			//绕环小球（实例化）
			std::vector<float> 环珠;
			环珠.reserve(8 * 16);
			for (uint32_t 序号 = 0; 序号 < 8; ++序号)
			{
				const float 角度 = 圆周率 * 2.0f * static_cast<float>(序号) / 8.0f + 累计秒_ * 0.4f;
				const float 珠x = 4.6f + std::cos(角度) * 1.9f;
				const float 珠y = 1.8f + std::sin(角度) * 1.9f;
				const std::array<float, 16> 珠 = 变换(珠x, 珠y, -33.0f, 0.18f);
				环珠.insert(环珠.end(), 珠.begin(), 珠.end());
			}
			(void)渲染器.mesh_draw_instanced(球_, 环珠.data(), 8);
			渲染器.material_emissive_clear();
			渲染器.material_tint_clear();
		}

		//—— 展区⑥：纹理（棋盘面板 / 动态贴图面板 / UV 立方 / 贴图球） ——
		{
			//大棋盘面板（竖直）
			渲染器.texture_bind_set(棋盘_);
			const std::array<float, 16> 棋盘面 = 非均匀变换(10.4f, 2.0f, -34.4f, 3.0f, 2.0f, 1.0f);
			(void)渲染器.mesh_draw(平面_, 棋盘面.data());
			//逐帧动态更新面板（每 0.1 秒写入一个移动色块）
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
				const std::array<float, 16> 动态面 = 非均匀变换(14.0f, 1.8f, -34.4f, 1.8f, 1.8f, 1.0f);
				(void)渲染器.mesh_draw(平面_, 动态面.data());
			}
			//棋盘立方（展示立方体六面 UV）
			渲染器.texture_bind_set(棋盘_);
			const std::array<float, 16> 纹理立方 = 绕Y变换(10.8f, 0.95f, -29.4f, 累计秒_ * 0.3f, 1.1f);
			(void)渲染器.mesh_draw(立方体_, 纹理立方.data());
			//法线 / 粗糙度贴图球
			if (贴图材质_ != 0)
			{
				渲染器.material_bind_set(贴图材质_);
				const std::array<float, 16> 贴图球 = 变换(17.2f, 0.9f, -30.6f, 0.9f);
				(void)渲染器.mesh_draw(球_, 贴图球.data());
				渲染器.material_bind_set(0);
			}
		}

		/*
		—— 展区⑦：卡通渲染（第一百九十二轮） ——
		三个正二十面体球环绕排布，作为风格化观感的主体；
		风格参数本身是全局的，切换由 update 内的「相机进入本区」驱动（见该函数注释）。
		*/
		if (卡通球_ != 0)
		{
			const float 卡通色[3][3] = {
				{ 1.0f, 0.62f, 0.45f }, { 0.55f, 0.85f, 1.0f }, { 0.95f, 0.85f, 0.45f },
			};
			for (uint32_t 序号 = 0; 序号 < 3; ++序号)
			{
				const float 角度 = 圆周率 * 2.0f * static_cast<float>(序号) / 3.0f;
				const float x = std::cos(角度) * 2.2f;
				const float z = -50.0f + std::sin(角度) * 2.2f;
				渲染器.material_tint_set(卡通色[序号][0], 卡通色[序号][1], 卡通色[序号][2], 1.0f);
				//球心抬高到地台上方，避免穿地
				const std::array<float, 16> 矩阵 = 变换(x, 1.35f, z, 1.3f);
				(void)渲染器.mesh_draw(卡通球_, 矩阵.data());
			}
			渲染器.material_tint_clear();
		}

		//—— 区域标签（三维投影：把世界坐标投到屏幕像素，再绘制文字） ——
		if (字体_ != 0)
		{
			const float 屏宽 = static_cast<float>(渲染器.extent_width());
			const float 屏高 = static_cast<float>(渲染器.extent_height());
			//标签描边（深色底描边，保证在亮色地台上仍清晰）
			const float 描边色[3] = { 0.03f, 0.04f, 0.07f };
			渲染器.text_outline_set(0.0025f, 描边色);
			for (uint32_t 序号 = 0; 序号 < 7; ++序号)
			{
				/*
				标签世界锚点（展区地台前缘，像展牌一样贴地）
				贴地可避开左上角图例面板的遮挡，也让标签与所属展区在透视上更紧密。
				*/
				const float 锚点[3] = { 展区表[序号].x, 0.45f, 展区表[序号].z + 5.6f };
				float u = 0.0f;
				float v = 0.0f;
				float 深度 = 0.0f;
				//若锚点在相机前方
				if (!渲染器.camera_project(锚点, &u, &v, &深度))
					continue;
				//屏幕像素坐标（左上角原点；越界标签直接跳过）
				float 像素x = u * 屏宽;
				const float 像素y = v * 屏高;
				//按文本宽度居中
				float 文本宽 = 0.0f;
				float 文本高 = 0.0f;
				if (渲染器.text_measure(字体_, 展区表[序号].标题, &文本宽, &文本高))
					像素x -= 文本宽 * 0.5f;
				if (像素x < -300.0f || 像素x > 屏宽 || 像素y < 0.0f || 像素y > 屏高)
					continue;
				(void)渲染器.text_draw(字体_, 展区表[序号].标题, 像素x, 像素y);
			}
			渲染器.text_outline_set(0.0f, 描边色);
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
		//材质
		if (贴图材质_ != 0)
			渲染器.material_unload(贴图材质_);
		if (地面材质_ != 0)
			渲染器.material_unload(地面材质_);
		贴图材质_ = 0;
		地面材质_ = 0;
		//字体与纹理
		if (字体_ != 0)
			渲染器.font_unload(字体_);
		const uint64_t 纹理[7] = { 棋盘_, 地板_, 法线贴图_, 粗糙贴图_, 天空_, 查找表_, 动态贴图_ };
		for (uint64_t 句柄 : 纹理)
		{
			if (句柄 != 0)
				渲染器.texture_unload(句柄);
		}
		字体_ = 0;
		棋盘_ = 0;
		地板_ = 0;
		法线贴图_ = 0;
		粗糙贴图_ = 0;
		天空_ = 0;
		查找表_ = 0;
		动态贴图_ = 0;
		//网格（含程序化立方体、卡通球与细分地形；第一百九十二 / 一百九十五轮补齐）
		const engine::Mesh_Handle 网格[12] = { 球_, 圆柱_, 圆锥_, 胶囊_, 圆环_, 平面_, 地面_, 地面反向_, 圆盘_,
			立方体_, 卡通球_, 地形_ };
		for (uint64_t 句柄 : 网格)
		{
			if (句柄 != 0)
				渲染器.mesh_unload(句柄);
		}
		球_ = 圆柱_ = 圆锥_ = 胶囊_ = 圆环_ = 平面_ = 地面_ = 圆盘_ = 立方体_ = 卡通球_ = 地形_ = 0;
		//复位展示用开关
		渲染器.render_threads_set(0);
		渲染器.toon_set(0, 0.0f);
		渲染器.toon_shadow_tint_set(0.0f, 0.0f, 0.0f);
		渲染器.toon_specular_set(0.0f);
		卡通区激活_ = false;
		渲染器.sky_enable_set(false);
		/*
		清除天空贴图与 LUT 绑定（第二百一十八轮）
		二者绑定的是展示场景自建纹理，纹理卸载后绑定仍指向旧句柄；
		虽然关闭开关后不采样，但下次启用前必须先解除绑定，避免引用已销毁纹理。
		*/
		渲染器.sky_texture_set(0);
		渲染器.post_lut_bind(0);
		渲染器.shadow_enable_set(false);
		渲染器.shadow_cascade_set(1, 3.0f, 12.0f);
		渲染器.pbr_enable_set(false);
		渲染器.fog_clear();
		渲染器.texture_bind_set(0);
		渲染器.material_pbr_clear();
		渲染器.material_tint_clear();
		渲染器.material_emissive_clear();
		//多重采样恢复默认（展示期曾设为 4×）
		渲染器.msaa_samples_set(1);
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
