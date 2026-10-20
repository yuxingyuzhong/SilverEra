/*
渲染场景数据测试（L2 隐藏窗口 + 像素回读）
覆盖：未初始化应用场景无副作用；scene_apply 的「相机 ＋ 清屏色」同时生效（相机背对四边形后中心应显示清屏色）；
      clear_color_set 切换清屏色生效。
断言一律「格式无关」：绿通道恒为字节下标 1（RGBA 与 BGRA 下均然），故绿 / 非绿判定只看第 1 字节；
      红 / 蓝等外色只看「外通道取较大者是否饱和」，不假定具体通道序。
*/
#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

//获取渲染器门面（含场景描述）与网格数据
#include "Application/Renderer/src/render/Renderer/渲染器.h"

namespace
{
	//窗口尺寸（与回读断言配套）
	constexpr uint32_t WINDOW_WIDTH = 64;
	constexpr uint32_t WINDOW_HEIGHT = 36;

	//构造隐藏窗口配置
	engine::Renderer_Config hidden_config(void)
	{
		engine::Renderer_Config config;
		config.width = WINDOW_WIDTH;
		config.height = WINDOW_HEIGHT;
		config.hidden = true;
		return config;
	}

	//本帧像素字节数（RGBA8 紧排）
	uint32_t pixel_bytes(void)
	{
		return WINDOW_WIDTH * WINDOW_HEIGHT * 4;
	}

	//取指定像素的四个字节
	std::array<uint8_t, 4> pixel_get(const std::vector<uint8_t>& pixels, uint32_t x, uint32_t y)
	{
		//行内偏移（每行宽 × 4 字节）
		std::size_t offset = (static_cast<std::size_t>(y) * WINDOW_WIDTH + x) * 4;
		std::array<uint8_t, 4> value{};
		for (std::size_t index = 0; index < 4; ++index)
			value[index] = pixels[offset + index];
		return value;
	}

	//失败诊断文本
	std::string pixel_text(const std::array<uint8_t, 4>& value)
	{
		return std::to_string(value[0]) + "/" + std::to_string(value[1]) + "/"
			+ std::to_string(value[2]) + "/" + std::to_string(value[3]);
	}

	//外通道较大者（R 与 B 通道；避开 std::max —— 本层包含链有 min/max 宏）
	uint8_t outer_max(const std::array<uint8_t, 4>& value)
	{
		return value[0] > value[2] ? value[0] : value[2];
	}

	//构造覆盖视口中央的四边形（NDC 尺度 1，位于 z = 0；默认相机在 +Z 3 单位处）
	engine::Mesh_Data quad_data(void)
	{
		engine::Mesh_Data data;
		data.vertices = {
			{ { -1.0f, -1.0f, 0.0f }, { 0.0f, 1.0f } },
			{ { 1.0f, -1.0f, 0.0f }, { 1.0f, 1.0f } },
			{ { 1.0f, 1.0f, 0.0f }, { 1.0f, 0.0f } },
			{ { -1.0f, 1.0f, 0.0f }, { 0.0f, 0.0f } },
		};
		data.indices = { 0, 1, 2, 0, 2, 3 };
		return data;
	}

	//构造纯红纹理（RGBA8：rgb = 红，alpha = 255）
	std::vector<uint8_t> solid_red(void)
	{
		std::vector<uint8_t> pixels(2 * 2 * 4);
		for (std::size_t index = 0; index < pixels.size(); index += 4)
		{
			pixels[index] = 255;
			pixels[index + 1] = 0;
			pixels[index + 2] = 0;
			pixels[index + 3] = 255;
		}
		return pixels;
	}

	//构造「铺满全屏」模型矩阵（列主序；放大 4 倍越过 NDC 边界被裁剪）
	std::array<float, 16> full_screen_matrix(void)
	{
		return {
			4.0f, 0.0f, 0.0f, 0.0f,
			0.0f, 4.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f,
		};
	}
}

//场景数据测试夹具
class Renderer_Scene_Test : public ::testing::Test
{
};

//未初始化时应用场景与设置清屏色均无副作用
TEST_F(Renderer_Scene_Test, 未初始化应用场景无副作用)
{
	engine::Renderer renderer;
	engine::Scene_Data scene{};
	renderer.scene_apply(scene);
	const float color[4] = { 1.0f, 0.0f, 0.0f, 1.0f };
	renderer.clear_color_set(color);
	//设置不改变状态、不崩溃
	EXPECT_FALSE(renderer.valid());
}

//scene_apply 的相机与清屏色同时生效：相机背对四边形后中心应显示清屏色
TEST_F(Renderer_Scene_Test, 应用场景描述相机与清屏色同时生效)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	//若本机不支持像素回读则跳过
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	//纯红纹理 ＋ 铺满全屏四边形
	uint64_t texture = 0;
	std::vector<uint8_t> texels = solid_red();
	ASSERT_TRUE(renderer.texture_load(texture, texels.data(), 2, 2));
	engine::Mesh_Handle mesh = 0;
	ASSERT_TRUE(renderer.mesh_load(mesh, quad_data()));
	renderer.texture_bind_set(texture);
	//第一帧：默认相机（机位 +Z 3、注视原点）→ 四边形可见，中心为纹理红（绿通道低）
	ASSERT_TRUE(renderer.mesh_draw(mesh, full_screen_matrix().data()));
	std::vector<uint8_t> before(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(before.data(), static_cast<uint32_t>(before.size())));
	std::array<uint8_t, 4> center_before = pixel_get(before, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
	EXPECT_LT(center_before[1], 60) << "默认相机下中心非纹理红：" << pixel_text(center_before);
	EXPECT_GE(outer_max(center_before), 200) << "默认相机下中心外通道未饱和：" << pixel_text(center_before);

	//第二帧：scene_apply 令相机背对四边形（注视点在机位前方 +Z）并把清屏色设为纯绿
	engine::Scene_Data scene{};
	scene.position[0] = 0.0f;
	scene.position[1] = 0.0f;
	scene.position[2] = 3.0f;
	scene.target[0] = 0.0f;
	scene.target[1] = 0.0f;
	scene.target[2] = 6.0f;
	scene.clear_color[0] = 0.0f;
	scene.clear_color[1] = 1.0f;
	scene.clear_color[2] = 0.0f;
	scene.clear_color[3] = 1.0f;
	renderer.scene_apply(scene);
	ASSERT_TRUE(renderer.mesh_draw(mesh, full_screen_matrix().data()));
	std::vector<uint8_t> after(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(after.data(), static_cast<uint32_t>(after.size())));
	std::array<uint8_t, 4> center_after = pixel_get(after, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
	//四边形已被相机移出视野 → 中心应为清屏纯绿
	EXPECT_GE(center_after[1], 200) << "相机未被应用（中心仍见四边形）：" << pixel_text(center_after);
	EXPECT_LT(center_after[0], 60) << "清屏色未被应用（绿通道旁分量偏高）：" << pixel_text(center_after);
	EXPECT_LT(center_after[2], 60) << "清屏色未被应用（绿通道旁分量偏高）：" << pixel_text(center_after);

	renderer.mesh_unload(mesh);
	renderer.texture_unload(texture);
}

//clear_color_set 切换清屏色生效：由绿改为红后中心变为饱和外色且非绿
TEST_F(Renderer_Scene_Test, 切换清屏色生效)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	//若本机不支持像素回读则跳过
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	//先设纯绿清屏色并回读
	const float green[4] = { 0.0f, 1.0f, 0.0f, 1.0f };
	renderer.clear_color_set(green);
	std::vector<uint8_t> green_pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(green_pixels.data(), static_cast<uint32_t>(green_pixels.size())));
	std::array<uint8_t, 4> green_center = pixel_get(green_pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
	EXPECT_GE(green_center[1], 200) << "绿色清屏色未生效：" << pixel_text(green_center);
	//再设纯红清屏色并回读（绿通道应降低、外通道应饱和）
	const float red[4] = { 1.0f, 0.0f, 0.0f, 1.0f };
	renderer.clear_color_set(red);
	std::vector<uint8_t> red_pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(red_pixels.data(), static_cast<uint32_t>(red_pixels.size())));
	std::array<uint8_t, 4> red_center = pixel_get(red_pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
	EXPECT_LT(red_center[1], 60) << "红色清屏色未生效（绿通道仍高）：" << pixel_text(red_center);
	EXPECT_GE(outer_max(red_center), 200) << "红色清屏色未生效（外通道未饱和）：" << pixel_text(red_center);
	//两次清屏色不同 → 画面应可区分
	EXPECT_NE(green_pixels, red_pixels);
}
