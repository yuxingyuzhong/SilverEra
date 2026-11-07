/*
水平面可见性测试（第156轮）
背景：展示场景里「贴地的大面积水平面」（地面、天空背景）始终画不出来，而竖直平面正常；
      本用例用最小场景复现，区分「水平面特有」与「平面渲染整体」两类问题。

口径：
- 隐藏窗口 64×36（与既有隐藏用例一致），相机 (0,6,10) 看向原点，默认方向光满照；
- 水平面用例：20×20 的 XZ 平面（法线 +Y），若正常应占满画面下半部；
- 对照用例：2×2 的 XY 平面（法线 +Z）放在相机正前方，若正常应出现在画面中央。
两者使用同一套顶点格式与绕序，仅平面朝向不同。
*/
#include "渲染场景测试辅助.h"

namespace
{
	//XZ 水平面（法线朝 +Y；绕序与场景用例的四边形一致）
	engine::Mesh_Data 水平面数据(float 半宽)
	{
		engine::Mesh_Data 数据;
		数据.vertices = {
			{ { -半宽, 0.0f, -半宽 }, { 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f } },
			{ { 半宽, 0.0f, -半宽 }, { 1.0f, 0.0f }, { 0.0f, 1.0f, 0.0f } },
			{ { 半宽, 0.0f, 半宽 }, { 1.0f, 1.0f }, { 0.0f, 1.0f, 0.0f } },
			{ { -半宽, 0.0f, 半宽 }, { 0.0f, 1.0f }, { 0.0f, 1.0f, 0.0f } },
		};
		数据.indices = { 0, 1, 2, 0, 2, 3 };
		return 数据;
	}

	//XY 竖直面（法线朝 +Z；与场景用例的四边形完全同构）
	engine::Mesh_Data 竖直面数据(float 半宽, float 半高)
	{
		engine::Mesh_Data 数据;
		数据.vertices = {
			{ { -半宽, -半高, 0.0f }, { 0.0f, 1.0f }, { 0.0f, 0.0f, 1.0f } },
			{ { 半宽, -半高, 0.0f }, { 1.0f, 1.0f }, { 0.0f, 0.0f, 1.0f } },
			{ { 半宽, 半高, 0.0f }, { 1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f } },
			{ { -半宽, 半高, 0.0f }, { 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f } },
		};
		数据.indices = { 0, 1, 2, 0, 2, 3 };
		return 数据;
	}

	//把指定行区间内「亮度超过阈值」的像素数计出来
	uint32_t bright_pixels(const std::vector<uint8_t>& pixels, uint32_t 起始行, uint32_t 结束行, uint32_t 阈值)
	{
		uint32_t 计数 = 0;
		for (uint32_t 行 = 起始行; 行 < 结束行; ++行)
		{
			for (uint32_t 列 = 0; 列 < WINDOW_WIDTH; ++列)
			{
				const std::array<uint8_t, 4> 像素 = pixel_get(pixels, 列, 行);
				const uint32_t 亮度 = (static_cast<uint32_t>(像素[0]) + 像素[1] + 像素[2]) / 3;
				if (亮度 >= 阈值)
					++计数;
			}
		}
		return 计数;
	}

	//相机与光照（水平面与竖直面对照共用）
	void 应用场景(engine::Renderer& 渲染器)
	{
		engine::Camera_Data 相机;
		相机.position[0] = 0.0f;
		相机.position[1] = 6.0f;
		相机.position[2] = 10.0f;
		相机.target[0] = 0.0f;
		相机.target[1] = 0.0f;
		相机.target[2] = 0.0f;
		相机.fov = 1.0472f;
		相机.far_plane = 200.0f;
		engine::Light_Data 光照;
		光照.direction[0] = 0.35f;
		光照.direction[1] = -0.75f;
		光照.direction[2] = -0.5f;
		光照.intensity = 1.0f;
		光照.ambient = 0.25f;
		渲染器.scene_apply(相机, 光照, engine::Fog_Data{});
	}
}

/*
水平面必须可见（第156轮）
若该用例失败，说明渲染层对「法线朝上的大面积平面」存在绘制问题（与展示场景的现象一致）。
*/
TEST_F(Renderer_Scene_Test, 水平面可见)
{
	engine::Renderer 渲染器;
	ASSERT_TRUE(渲染器.init(hidden_config()));
	if (!渲染器.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	应用场景(渲染器);
	//白色纹理 ＋ 20×20 水平面
	渲染器.texture_bind_set(0);
	engine::Mesh_Handle 地面 = 0;
	ASSERT_TRUE(渲染器.mesh_load(地面, 水平面数据(20.0f)));
	ASSERT_TRUE(渲染器.mesh_draw(地面, nullptr));
	std::vector<uint8_t> 像素(pixel_bytes());
	ASSERT_TRUE(渲染器.frame_render(像素.data(), static_cast<uint32_t>(像素.size())));
	//下半部（地面应覆盖的区域）的亮像素
	const uint32_t 下半亮像素 = bright_pixels(像素, WINDOW_HEIGHT / 2, WINDOW_HEIGHT, 60);
	RecordProperty("horizontal_bright_pixels", 下半亮像素);
	EXPECT_GT(下半亮像素, 50u) << "水平面未被绘制（下半部亮像素过少）";
	渲染器.mesh_unload(地面);
}

/*
竖直面对照（第156轮）
同一套数据格式与绕序，仅朝向不同；用于确认渲染环境本身正常。
*/
TEST_F(Renderer_Scene_Test, 竖直面可见)
{
	engine::Renderer 渲染器;
	ASSERT_TRUE(渲染器.init(hidden_config()));
	if (!渲染器.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	应用场景(渲染器);
	//白色纹理 ＋ 2×2 竖直面，放在相机正前方
	渲染器.texture_bind_set(0);
	engine::Mesh_Handle 立板 = 0;
	ASSERT_TRUE(渲染器.mesh_load(立板, 竖直面数据(1.0f, 1.0f)));
	const std::array<float, 16> 平移 = {
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 2.0f, -2.0f, 1.0f,
	};
	ASSERT_TRUE(渲染器.mesh_draw(立板, 平移.data()));
	std::vector<uint8_t> 像素(pixel_bytes());
	ASSERT_TRUE(渲染器.frame_render(像素.data(), static_cast<uint32_t>(像素.size())));
	//中央区域（竖直面应覆盖）
	const uint32_t 中央亮像素 = bright_pixels(像素, WINDOW_HEIGHT / 4, WINDOW_HEIGHT * 3 / 4, 60);
	RecordProperty("vertical_bright_pixels", 中央亮像素);
	EXPECT_GT(中央亮像素, 50u) << "竖直面未被绘制（渲染环境异常）";
	渲染器.mesh_unload(立板);
}
