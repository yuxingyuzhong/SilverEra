//展区公共设施实现：变换 / 像素工具、程序化网格数据、字体、展区表、基础场地与标签
#include "src/主调/能力展示/展区公共.h"

//渲染器门面（Vulkan-free 公共面）
#include "Application/Renderer/src/render/Renderer/渲染器.h"

#include <cmath>
#include <cstdint>
#include <vector>

namespace engine
{
	//引擎命名空间
	namespace 展示
	{
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

		//地板网格像素（深色底 ＋ 浅色细网格线）
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

		//暖调查找表像素（3D LUT 的 2D 条带存放；亮部偏暖、暗部偏冷）
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

		//XY 平面四边形（手写绕序，保证任何角度可见）
		Mesh_Data 四边形数据(float 半宽, float 半高)
		{
			Mesh_Data 数据;
			数据.vertices = {
				{ { -半宽, -半高, 0.0f }, { 0.0f, 1.0f } },
				{ { 半宽, -半高, 0.0f }, { 1.0f, 1.0f } },
				{ { 半宽, 半高, 0.0f }, { 1.0f, 0.0f } },
				{ { -半宽, 半高, 0.0f }, { 0.0f, 0.0f } },
			};
			数据.indices = { 0, 1, 2, 0, 2, 3 };
			return 数据;
		}

		//XZ 平面四边形（反向绕序参数用于确认正面绕序）
		Mesh_Data 地面数据(float 半宽, bool 反向绕序)
		{
			Mesh_Data 数据;
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

		//加载可用字体（失败返回 0，字幕退化为无）
		uint64_t 加载字体(Renderer& 渲染器)
		{
			//字体候选（按顺序取第一个可用者；优先 TrueType，再回退 TTC）
			const char* const 候选[] = {
				"C:\\Windows\\Fonts\\simhei.ttf",
				"C:\\Windows\\Fonts\\msyh.ttc",
				"C:\\Windows\\Fonts\\msyhbd.ttc",
				"C:\\Windows\\Fonts\\simsun.ttc",
			};
			for (const char* 路径 : 候选)
			{
				uint64_t 句柄 = 0;
				//命中即用（失败仅记日志，继续下一个候选）
				if (渲染器.font_load_file(句柄, 路径, 22))
					return 句柄;
			}
			return 0;
		}

		/*
		展区表（23 区）
		前 9 区沿用原三列布局（x = −14 / 0 / 14，z = −10 / −30 / −50）；
		第 10 区起向纵深延伸（z = −70 / −90 / −110 / −130 / −150），同行三列。
		*/
		const 展区项* 展区表(uint32_t& 数量)
		{
			static const 展区项 表[] = {
				{ 1, -14.0f, -10.0f, { 0.46f, 0.52f, 0.70f }, "01 程序化网格" },
				{ 2, 0.0f, -10.0f, { 0.74f, 0.52f, 0.40f }, "02 PBR 材质" },
				{ 3, 14.0f, -10.0f, { 0.40f, 0.66f, 0.60f }, "03 光照阴影" },
				{ 4, -14.0f, -30.0f, { 0.58f, 0.46f, 0.74f }, "04 实例化合批" },
				{ 5, 0.0f, -30.0f, { 0.76f, 0.60f, 0.34f }, "05 几何拼接" },
				{ 6, 14.0f, -30.0f, { 0.50f, 0.62f, 0.46f }, "06 纹理动态" },
				{ 7, 0.0f, -50.0f, { 0.44f, 0.50f, 0.68f }, "07 逐物体卡通" },
				{ 8, -14.0f, -50.0f, { 0.66f, 0.44f, 0.52f }, "08 簇灯墙" },
				{ 9, 14.0f, -50.0f, { 0.68f, 0.60f, 0.38f }, "09 粒子喷泉" },
				{ 10, -14.0f, -70.0f, { 0.42f, 0.56f, 0.64f }, "10 贴图扩展" },
				{ 11, 0.0f, -70.0f, { 0.62f, 0.46f, 0.60f }, "11 文字布局" },
				{ 12, 14.0f, -70.0f, { 0.66f, 0.56f, 0.40f }, "12 泛光链" },
				{ 13, -14.0f, -90.0f, { 0.50f, 0.44f, 0.70f }, "13 LUT 调色" },
				{ 14, 0.0f, -90.0f, { 0.42f, 0.62f, 0.56f }, "14 SSAO + FXAA" },
				{ 15, 14.0f, -90.0f, { 0.70f, 0.50f, 0.46f }, "15 描边 + 体积雾" },
				{ 16, -14.0f, -110.0f, { 0.60f, 0.58f, 0.36f }, "16 体积光" },
				{ 17, 0.0f, -110.0f, { 0.46f, 0.60f, 0.72f }, "17 SSR 反射" },
				{ 18, 14.0f, -110.0f, { 0.64f, 0.46f, 0.56f }, "18 采样抗锯齿" },
				{ 19, -14.0f, -130.0f, { 0.52f, 0.60f, 0.52f }, "19 雾三模式" },
				{ 20, 0.0f, -130.0f, { 0.56f, 0.52f, 0.66f }, "20 计算管线" },
				{ 21, 14.0f, -130.0f, { 0.66f, 0.54f, 0.44f }, "21 多线程" },
				{ 22, -14.0f, -150.0f, { 0.48f, 0.56f, 0.60f }, "22 正交投影" },
				{ 23, 0.0f, -150.0f, { 0.60f, 0.60f, 0.48f }, "23 读回统计" },
			};
			数量 = static_cast<uint32_t>(sizeof(表) / sizeof(表[0]));
			return 表;
		}

		//基础场地：地面 ＋ 各展区彩色地台 ＋ 展区四角立柱
		void 绘制基础场地(展示资源& 资源, 展示状态& 状态, Renderer& 渲染器)
		{
			uint32_t 数量 = 0;
			const 展区项* 表 = 展区表(数量);
			//—— 基础地面（双面材质：绕序无关，杜绝大面积平面被背面剔除） ——
			渲染器.material_bind_set(资源.地面材质);
			//地面基调（冷灰，压暗以免抢走展区色彩）
			渲染器.material_tint_set(0.24f, 0.26f, 0.31f, 1.0f);
			const std::array<float, 16> 地面矩阵 = 变换(0.0f, -0.02f, -70.0f, 9.0f);
			(void)渲染器.mesh_draw(资源.地面, 地面矩阵.data());
			渲染器.material_tint_clear();

			//—— 展区地台（每区一块彩色地砖，作为分区与地面参照） ——
			for (uint32_t 序号 = 0; 序号 < 数量; ++序号)
			{
				渲染器.material_tint_set(表[序号].地色[0], 表[序号].地色[1], 表[序号].地色[2], 1.0f);
				const std::array<float, 16> 地台 = 变换(表[序号].x, 0.02f, 表[序号].z, 0.30f);
				(void)渲染器.mesh_draw(资源.地面, 地台.data());
			}
			渲染器.material_tint_clear();
			渲染器.material_bind_set(0);

			//—— 展区角柱（每区四角一根，一次实例化提交，形成清晰边界） ——
			std::vector<float> 柱矩阵;
			柱矩阵.reserve(static_cast<std::size_t>(数量) * 4 * 16);
			for (uint32_t 序号 = 0; 序号 < 数量; ++序号)
			{
				for (int 横 = -1; 横 <= 1; 横 += 2)
				{
					for (int 纵 = -1; 纵 <= 1; 纵 += 2)
					{
						const std::array<float, 16> 柱 = 非均匀变换(表[序号].x + 5.7f * static_cast<float>(横), 0.36f,
							表[序号].z + 5.7f * static_cast<float>(纵), 0.16f, 0.45f, 0.16f);
						柱矩阵.insert(柱矩阵.end(), 柱.begin(), 柱.end());
					}
				}
			}
			//石灰色角柱（与彩色地台形成对比）
			渲染器.material_tint_set(0.42f, 0.44f, 0.48f, 1.0f);
			(void)渲染器.mesh_draw_instanced(资源.立方体, 柱矩阵.data(), static_cast<uint32_t>(数量 * 4));
			渲染器.material_tint_clear();
		}

		//粒子喷泉 billboard（常驻可用时按容量选 GPU 路径）
		void 绘制粒子喷泉(展示资源& 资源, 展示状态& 状态, Renderer& 渲染器)
		{
			(void)资源;
			(void)状态;
			//未开启：各路径均为空操作，直接返回
			if (!渲染器.particle_enable_get())
				return;
			//常驻路径：容量 ≥ 16384 走 GPU 间接绘制，否则直接 GPU 绘制
			if (渲染器.particle_resident_get())
			{
				if (渲染器.particle_capacity_get() >= 16384)
					(void)渲染器.particle_indirect_draw(0.16f);
				else
					(void)渲染器.particle_gpu_draw(0.16f);
			}
			else
				(void)渲染器.particle_draw(0.16f);
		}

		//全展区三维投影标签
		void 绘制展区标签(展示资源& 资源, 展示状态& 状态, Renderer& 渲染器)
		{
			(void)状态;
			//无字体则跳过（图例面板仍有说明）
			if (资源.字体 == 0)
				return;
			uint32_t 数量 = 0;
			const 展区项* 表 = 展区表(数量);
			const float 屏宽 = static_cast<float>(渲染器.extent_width());
			const float 屏高 = static_cast<float>(渲染器.extent_height());
			//标签描边（深色底描边，保证在亮色地台上仍清晰）
			const float 描边色[3] = { 0.03f, 0.04f, 0.07f };
			渲染器.text_outline_set(0.0025f, 描边色);
			/*
			只画「最近的那一区」标签（第四百二十一轮）
			按 30 单位过滤后仍会有 3–4 条标签同屏（实测在展区附近朝任意方向都能同时落在投影范围内），
			它们会互相重叠成一团文字；改为只画相机水平距离最近且 ≤ 30 单位的那一条，其余等走近再显示。
			*/
			uint32_t 最近序号 = 数量;
			float 最近平方 = 30.0f * 30.0f;
			for (uint32_t 序号 = 0; 序号 < 数量; ++序号)
			{
				const float 距x = 状态.位置[0] - 表[序号].x;
				const float 距z = 状态.位置[2] - 表[序号].z;
				const float 平方 = 距x * 距x + 距z * 距z;
				//更近且仍在过滤半径内
				if (平方 < 最近平方)
				{
					最近平方 = 平方;
					最近序号 = 序号;
				}
			}
			//30 单位内无展区：本帧不画标签
			if (最近序号 >= 数量)
			{
				渲染器.text_outline_set(0.0f, 描边色);
				return;
			}
			{
				//标签世界锚点（展区地台前缘，像展牌一样贴地）
				const float 锚点[3] = { 表[最近序号].x, 0.45f, 表[最近序号].z + 5.6f };
				float u = 0.0f;
				float v = 0.0f;
				float 深度 = 0.0f;
				//若锚点在相机前方
				if (渲染器.camera_project(锚点, &u, &v, &深度))
				{
					float 像素x = u * 屏宽;
					const float 像素y = v * 屏高;
					//按文本宽度居中
					float 文本宽 = 0.0f;
					float 文本高 = 0.0f;
					if (渲染器.text_measure(资源.字体, 表[最近序号].标题, &文本宽, &文本高))
						像素x -= 文本宽 * 0.5f;
					//越界标签直接跳过
					if (像素x >= -400.0f && 像素x <= 屏宽 && 像素y >= 0.0f && 像素y <= 屏高)
						(void)渲染器.text_draw(资源.字体, 表[最近序号].标题, 像素x, 像素y);
				}
			}
			渲染器.text_outline_set(0.0f, 描边色);
		}

		//进区开关
		void 更新进区开关(展示资源& 资源, 展示状态& 状态, Renderer& 渲染器)
		{
			uint32_t 数量 = 0;
			const 展区项* 表 = 展区表(数量);
			//相机到第 N 区的水平距离平方（判定半径 9 单位）
			auto 区内 = [&](uint32_t 编号)
			{
				const 展区项& 区 = 表[编号 - 1];
				const float 距x = 状态.位置[0] - 区.x;
				const float 距z = 状态.位置[2] - 区.z;
				return (距x * 距x + 距z * 距z) < 81.0f;
			};

			/*
			卡通渲染区（⑦）
			渲染层的风格化参数是全局的，故「进入本区即整幅切换、离开即恢复」；
			仅在状态变化时调用门面，避免逐帧重复设置。
			*/
			const bool 进入卡通区 = 区内(7);
			//若激活状态变化
			if (进入卡通区 != 状态.卡通区激活)
			{
				状态.卡通区激活 = 进入卡通区;
				//四阶段：色阶量化（4 档）＋ 菲涅尔边缘光 ＋ 暗部冷紫色调 ＋ 卡通高光阈值
				渲染器.toon_set(进入卡通区 ? 4u : 0u, 进入卡通区 ? 0.65f : 0.0f);
				渲染器.toon_shadow_tint_set(进入卡通区 ? 0.42f : 0.0f,
					进入卡通区 ? 0.46f : 0.0f, 进入卡通区 ? 0.72f : 0.0f);
				渲染器.toon_specular_set(进入卡通区 ? 0.35f : 0.0f);
				//反向外扩描边（深色轮廓沿法线外扩 0.05 世界单位）
				const float 描边色[3] = { 0.06f, 0.05f, 0.10f };
				if (进入卡通区)
					渲染器.outline_set(0.05f, 描边色);
				else
					渲染器.outline_clear();
			}

			/*
			Cluster 灯墙区（⑧）
			进入本区开启簇光照（128 盏灯参与逐簇采样），离开回到 8 盏前向路径。
			*/
			const bool 进入灯墙区 = 区内(8);
			//若激活状态变化
			if (进入灯墙区 != 状态.灯墙区激活)
			{
				状态.灯墙区激活 = 进入灯墙区;
				if (渲染器.cluster_supported())
					渲染器.cluster_enable_set(进入灯墙区);
			}

			/*
			粒子喷泉区（⑨）
			进入开启粒子并优先切常驻路径，离开先关常驻再关粒子（保留池内容）。
			*/
			const bool 进入粒子区 = 区内(9);
			//若激活状态变化
			if (进入粒子区 != 状态.粒子区激活)
			{
				状态.粒子区激活 = 进入粒子区;
				if (渲染器.particle_supported())
				{
					//进入：先开启粒子，再尝试常驻
					if (进入粒子区)
					{
						渲染器.particle_enable_set(true);
						状态.粒子常驻 = 渲染器.particle_resident_set(true);
					}
					//离开：先关常驻，再关粒子
					else
					{
						渲染器.particle_resident_set(false);
						状态.粒子常驻 = false;
						渲染器.particle_enable_set(false);
					}
				}
			}
			//区内逐帧发射与推进
			if (状态.粒子区激活 && 状态.粒子已配置)
			{
				//常驻路径：发射与推进均在 GPU
				if (状态.粒子常驻)
				{
					(void)渲染器.particle_gpu_emit(256);
					(void)渲染器.particle_gpu_step_resident(0.08f);
				}
				//回落路径：CPU 参考模拟
				else
				{
					(void)渲染器.particle_emit(256);
					(void)渲染器.particle_step(0.08f);
				}
			}

			//后处理类与系统类展区的开关（各自文件实现，避免本文件膨胀）
			更新后处理区(资源, 状态, 渲染器);
			更新系统开关区(资源, 状态, 渲染器);
		}
	}
}
