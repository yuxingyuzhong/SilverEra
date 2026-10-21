/*
渲染场景数据测试（L2 隐藏窗口 + 像素回读）
覆盖：未初始化应用场景无副作用；scene_apply 的「相机 ＋ 清屏色」同时生效（相机背对四边形后中心应显示清屏色）；
      clear_color_set 切换清屏色生效；
      **参数校验**（处置 R-FACADE-10）：非法场景描述整次拒绝、清屏色分量越界被钳制；
      **默认单一来源**（处置 R-SCENE-07）：应用默认 Scene_Data 与原默认状态画面完全一致。
断言一律「格式无关」：绿通道恒为字节下标 1（RGBA 与 BGRA 下均然），故绿 / 非绿判定只看第 1 字节；
      红 / 蓝等外色只看「外通道取较大者是否饱和」，不假定具体通道序。
*/
#include <gtest/gtest.h>

#include <array>
#include <cmath>
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

	//构造纯白纹理（RGBA8：rgb = 255，alpha = 255）
	std::vector<uint8_t> solid_white(void)
	{
		return std::vector<uint8_t>(2 * 2 * 4, 255);
	}

	//构造中灰纹理（RGBA8：rgb = 128，alpha = 255；用于观察高光叠加而不被满照饱和）
	std::vector<uint8_t> solid_gray(void)
	{
		std::vector<uint8_t> pixels(2 * 2 * 4);
		for (std::size_t index = 0; index < pixels.size(); index += 4)
		{
			pixels[index] = 128;
			pixels[index + 1] = 128;
			pixels[index + 2] = 128;
			pixels[index + 3] = 255;
		}
		return pixels;
	}

	/*
	构造带法线的四边形（第二十九轮）
	nx / ny / nz 为四个顶点共用的法线；默认相机在 +Z，故 (0,0,1) 即正面朝向相机。
	*/
	engine::Mesh_Data quad_data_with_normal(float nx, float ny, float nz)
	{
		engine::Mesh_Data data;
		data.vertices = {
			{ { -1.0f, -1.0f, 0.0f }, { 0.0f, 1.0f }, { nx, ny, nz } },
			{ { 1.0f, -1.0f, 0.0f }, { 1.0f, 1.0f }, { nx, ny, nz } },
			{ { 1.0f, 1.0f, 0.0f }, { 1.0f, 0.0f }, { nx, ny, nz } },
			{ { -1.0f, 1.0f, 0.0f }, { 0.0f, 0.0f }, { nx, ny, nz } },
		};
		data.indices = { 0, 1, 2, 0, 2, 3 };
		return data;
	}

	//中心像素的 RGB 均值（亮度粗量；通道序无关）
	uint32_t center_brightness(const std::vector<uint8_t>& pixels)
	{
		std::array<uint8_t, 4> center = pixel_get(pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
		return (static_cast<uint32_t>(center[0]) + center[1] + center[2]) / 3;
	}

	//构造带逐面法线的立方体（与网格模块 cube_generate 一致：24 顶点 / 36 索引）
	engine::Mesh_Data cube_data(float half_extent)
	{
		//半边长
		const float h = half_extent;
		engine::Mesh_Data data;
		data.vertices = {
			// +X 面
			{ { +h, -h, -h }, { 0.0f, 0.0f }, { 1.0f, 0.0f, 0.0f } },
			{ { +h, +h, -h }, { 0.0f, 1.0f }, { 1.0f, 0.0f, 0.0f } },
			{ { +h, +h, +h }, { 1.0f, 1.0f }, { 1.0f, 0.0f, 0.0f } },
			{ { +h, -h, +h }, { 1.0f, 0.0f }, { 1.0f, 0.0f, 0.0f } },
			// -X 面
			{ { -h, -h, +h }, { 0.0f, 0.0f }, { -1.0f, 0.0f, 0.0f } },
			{ { -h, +h, +h }, { 0.0f, 1.0f }, { -1.0f, 0.0f, 0.0f } },
			{ { -h, +h, -h }, { 1.0f, 1.0f }, { -1.0f, 0.0f, 0.0f } },
			{ { -h, -h, -h }, { 1.0f, 0.0f }, { -1.0f, 0.0f, 0.0f } },
			// +Y 面
			{ { -h, +h, +h }, { 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f } },
			{ { +h, +h, +h }, { 0.0f, 1.0f }, { 0.0f, 1.0f, 0.0f } },
			{ { +h, +h, -h }, { 1.0f, 1.0f }, { 0.0f, 1.0f, 0.0f } },
			{ { -h, +h, -h }, { 1.0f, 0.0f }, { 0.0f, 1.0f, 0.0f } },
			// -Y 面
			{ { -h, -h, -h }, { 0.0f, 0.0f }, { 0.0f, -1.0f, 0.0f } },
			{ { +h, -h, -h }, { 0.0f, 1.0f }, { 0.0f, -1.0f, 0.0f } },
			{ { +h, -h, +h }, { 1.0f, 1.0f }, { 0.0f, -1.0f, 0.0f } },
			{ { -h, -h, +h }, { 1.0f, 0.0f }, { 0.0f, -1.0f, 0.0f } },
			// +Z 面
			{ { -h, -h, +h }, { 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f } },
			{ { +h, -h, +h }, { 0.0f, 1.0f }, { 0.0f, 0.0f, 1.0f } },
			{ { +h, +h, +h }, { 1.0f, 1.0f }, { 0.0f, 0.0f, 1.0f } },
			{ { -h, +h, +h }, { 1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f } },
			// -Z 面
			{ { +h, -h, -h }, { 0.0f, 0.0f }, { 0.0f, 0.0f, -1.0f } },
			{ { -h, -h, -h }, { 0.0f, 1.0f }, { 0.0f, 0.0f, -1.0f } },
			{ { -h, +h, -h }, { 1.0f, 1.0f }, { 0.0f, 0.0f, -1.0f } },
			{ { +h, +h, -h }, { 1.0f, 0.0f }, { 0.0f, 0.0f, -1.0f } },
		};
		data.indices = {
			0, 1, 2, 2, 3, 0,
			4, 5, 6, 6, 7, 4,
			8, 9, 10, 10, 11, 8,
			12, 13, 14, 14, 15, 12,
			16, 17, 18, 18, 19, 16,
			20, 21, 22, 22, 23, 20,
		};
		return data;
	}

	//构造绕 Y 轴旋转的模型矩阵（列主序；角度为弧度）
	std::array<float, 16> rotation_y_matrix(float angle)
	{
		//余弦 / 正弦
		float c = std::cos(angle);
		float s = std::sin(angle);
		return {
			c, 0.0f, -s, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			s, 0.0f, c, 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f,
		};
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

//非法场景描述整次拒绝：不改变既有相机与清屏色
TEST_F(Renderer_Scene_Test, 非法场景描述整次拒绝)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	//若本机不支持像素回读则跳过
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	//第一帧：应用合法场景（清屏色绿、相机背对四边形）→ 中心为纯绿
	engine::Scene_Data valid{};
	valid.clear_color[0] = 0.0f;
	valid.clear_color[1] = 1.0f;
	valid.clear_color[2] = 0.0f;
	valid.clear_color[3] = 1.0f;
	renderer.scene_apply(valid);
	std::vector<uint8_t> green_pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(green_pixels.data(), static_cast<uint32_t>(green_pixels.size())));
	std::array<uint8_t, 4> green_center = pixel_get(green_pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
	EXPECT_GE(green_center[1], 200) << "合法场景未生效：" << pixel_text(green_center);

	//第二帧：应用非法场景（视场角为零、清屏色红）→ 应被整次拒绝
	engine::Scene_Data invalid{};
	invalid.fov = 0.0f;
	invalid.clear_color[0] = 1.0f;
	invalid.clear_color[1] = 0.0f;
	invalid.clear_color[2] = 0.0f;
	invalid.clear_color[3] = 1.0f;
	renderer.scene_apply(invalid);
	std::vector<uint8_t> rejected_pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(rejected_pixels.data(), static_cast<uint32_t>(rejected_pixels.size())));
	std::array<uint8_t, 4> rejected_center = pixel_get(rejected_pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
	//清屏色仍为绿（非法描述里的红未被采用）
	EXPECT_GE(rejected_center[1], 200) << "非法场景描述被部分采用（清屏色已变）：" << pixel_text(rejected_center);
	EXPECT_LT(rejected_center[0], 60) << "非法场景描述被部分采用（外通道偏高）：" << pixel_text(rejected_center);
	//画面与拒绝前完全一致
	EXPECT_EQ(green_pixels, rejected_pixels) << "非法场景描述改变了画面，整次拒绝未生效";
}

//清屏色分量越界被钳制到 [0,1]
TEST_F(Renderer_Scene_Test, 清屏色越界分量被钳制)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	//若本机不支持像素回读则跳过
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	//越界分量：外通道 2.0（应钳到 1）、绿 -1.0（应钳到 0）
	const float out_of_range[4] = { 2.0f, -1.0f, 0.5f, 1.0f };
	renderer.clear_color_set(out_of_range);
	std::vector<uint8_t> pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
	std::array<uint8_t, 4> center = pixel_get(pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
	//外通道钳到 1 → 饱和；绿钳到 0 → 低
	EXPECT_GE(outer_max(center), 200) << "越界外通道未被钳到 1：" << pixel_text(center);
	EXPECT_LT(center[1], 60) << "负绿通道未被钳到 0：" << pixel_text(center);
}

//应用默认场景描述与原默认状态画面完全一致（默认值单一来源）
TEST_F(Renderer_Scene_Test, 默认场景描述与原默认画面一致)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	//若本机不支持像素回读则跳过
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	//纯红纹理 ＋ 铺满全屏四边形（画面随相机与清屏色变化，故对默认值敏感）
	uint64_t texture = 0;
	std::vector<uint8_t> texels = solid_red();
	ASSERT_TRUE(renderer.texture_load(texture, texels.data(), 2, 2));
	engine::Mesh_Handle mesh = 0;
	ASSERT_TRUE(renderer.mesh_load(mesh, quad_data()));
	renderer.texture_bind_set(texture);
	//第一帧：不调用 scene_apply（使用本层既有默认）
	ASSERT_TRUE(renderer.mesh_draw(mesh, full_screen_matrix().data()));
	std::vector<uint8_t> baseline(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(baseline.data(), static_cast<std::uint32_t>(baseline.size())));
	//第二帧：应用默认构造的 Scene_Data（其默认值应与本层既有默认一致）
	engine::Scene_Data defaults{};
	renderer.scene_apply(defaults);
	ASSERT_TRUE(renderer.mesh_draw(mesh, full_screen_matrix().data()));
	std::vector<uint8_t> applied(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(applied.data(), static_cast<std::uint32_t>(applied.size())));
	//两帧画面应完全一致（默认值已收敛为单一来源）
	EXPECT_EQ(baseline, applied) << "应用默认 Scene_Data 后画面改变，默认值未收敛为单一来源";
	renderer.mesh_unload(mesh);
	renderer.texture_unload(texture);
}

/*
方向光（第二十九轮）：面法线朝向光源的四边形应明显亮于背光四边形
口径：纯白纹理 ＋ 覆盖视口中央的四边形（单位矩阵）；同一光照参数下改用法线 (0,0,-1) 的网格，
      默认光沿 -Z 传播，故 (0,0,1) 满照、(0,0,-1) 照度为 0；只比较中心像素 RGB 均值。
*/
TEST_F(Renderer_Scene_Test, 朝向光源更亮)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	uint64_t texture = 0;
	std::vector<uint8_t> texels = solid_white();
	ASSERT_TRUE(renderer.texture_load(texture, texels.data(), 2, 2));
	renderer.texture_bind_set(texture);
	//正面朝相机（受光）与背面（背光）两种网格
	engine::Mesh_Handle lit_mesh = 0;
	engine::Mesh_Handle dark_mesh = 0;
	ASSERT_TRUE(renderer.mesh_load(lit_mesh, quad_data_with_normal(0.0f, 0.0f, 1.0f)));
	ASSERT_TRUE(renderer.mesh_load(dark_mesh, quad_data_with_normal(0.0f, 0.0f, -1.0f)));

	//渲染一帧并返回中心亮度（lambda 内用 EXPECT：ASSERT 不允许出现在有返回值的函数中）
	auto render_brightness = [&](engine::Mesh_Handle mesh)
	{
		EXPECT_TRUE(renderer.mesh_draw(mesh, nullptr));
		std::vector<uint8_t> pixels(pixel_bytes());
		EXPECT_TRUE(renderer.frame_render(pixels.data(), static_cast<std::uint32_t>(pixels.size())));
		return center_brightness(pixels);
	};

	//受光帧：默认光照（方向 -Z、白、强度 1、环境光 0）→ 照度 1
	uint32_t lit = render_brightness(lit_mesh);
	//背光帧：同一光照，法线与光传播方向同向 → 照度 0
	uint32_t dark = render_brightness(dark_mesh);
	RecordProperty("light_lit", lit);
	RecordProperty("light_dark", dark);
	EXPECT_GE(lit, 200u) << "受光面未达到满照（中心亮度 " << lit << "）";
	EXPECT_LE(dark, 40u) << "背光面未被压暗（中心亮度 " << dark << "）";
	EXPECT_GT(static_cast<int>(lit), static_cast<int>(dark) + 100) << "光照未区分正反面";

	renderer.mesh_unload(lit_mesh);
	renderer.mesh_unload(dark_mesh);
	renderer.texture_unload(texture);
}

/*
未提供法线保持满照（第二十九轮回归口径）：只写位置 / 纹理坐标的旧网格在默认光照下画面不变
*/
TEST_F(Renderer_Scene_Test, 无法线网格保持满照)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	uint64_t texture = 0;
	std::vector<uint8_t> texels = solid_white();
	ASSERT_TRUE(renderer.texture_load(texture, texels.data(), 2, 2));
	engine::Mesh_Handle mesh = 0;
	ASSERT_TRUE(renderer.mesh_load(mesh, quad_data()));
	renderer.texture_bind_set(texture);
	ASSERT_TRUE(renderer.mesh_draw(mesh, full_screen_matrix().data()));
	std::vector<uint8_t> pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<std::uint32_t>(pixels.size())));
	//无法线 → 按满照处理，中心仍为纯白
	EXPECT_GE(center_brightness(pixels), 200u) << "无法线网格被光照压暗，破坏既有画面";
	renderer.mesh_unload(mesh);
	renderer.texture_unload(texture);
}

/*
非法光照参数整次忽略（第二十九轮）：零向量方向 / 非数 / 越界颜色均不改变画面
*/
TEST_F(Renderer_Scene_Test, 非法光照参数整次忽略)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	uint64_t texture = 0;
	std::vector<uint8_t> texels = solid_white();
	ASSERT_TRUE(renderer.texture_load(texture, texels.data(), 2, 2));
	engine::Mesh_Handle mesh = 0;
	ASSERT_TRUE(renderer.mesh_load(mesh, quad_data_with_normal(0.0f, 0.0f, 1.0f)));
	renderer.texture_bind_set(texture);

	//渲染一帧（默认光照）作为基线（lambda 内用 EXPECT：ASSERT 不允许出现在有返回值的函数中）
	auto render_frame = [&]()
	{
		EXPECT_TRUE(renderer.mesh_draw(mesh, nullptr));
		std::vector<uint8_t> pixels(pixel_bytes());
		EXPECT_TRUE(renderer.frame_render(pixels.data(), static_cast<std::uint32_t>(pixels.size())));
		return pixels;
	};
	std::vector<uint8_t> baseline = render_frame();
	//零长度方向（无法归一化）→ 整次拒绝
	const float zero_direction[3] = { 0.0f, 0.0f, 0.0f };
	const float white[3] = { 1.0f, 1.0f, 1.0f };
	renderer.light_set(zero_direction, white, 1.0f, 0.0f);
	//非数方向 → 整次拒绝
	const float nan_direction[3] = { std::nanf(""), 0.0f, -1.0f };
	renderer.light_set(nan_direction, white, 1.0f, 0.0f);
	//越界颜色（1.5）→ 整次拒绝
	const float over_color[3] = { 1.5f, 1.0f, 1.0f };
	const float forward[3] = { 0.0f, 0.0f, -1.0f };
	renderer.light_set(forward, over_color, 1.0f, 0.0f);
	//三次非法调用后画面应与基线逐字节一致
	EXPECT_EQ(baseline, render_frame()) << "非法光照参数改变了画面（未整次拒绝）";

	renderer.mesh_unload(mesh);
	renderer.texture_unload(texture);
}

/*
正交投影（第二十轮）：同一四边形在相机远近不同时屏幕占比不变；透视下则明显变小
口径：纯红纹理 ＋ 覆盖视口中央的四边形（单位矩阵）；统计「红饱和（外通道 ≥ 200）」的像素数。
      透视：z = 5 的红色像素数应显著少于 z = 3；
      正交（ortho_height = 2）：z = 3 与 z = 5 的红色像素数应基本一致（差 ≤ 2%）。
*/
TEST_F(Renderer_Scene_Test, 正交投影下远近尺寸一致)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	uint64_t texture = 0;
	std::vector<uint8_t> texels = solid_red();
	ASSERT_TRUE(renderer.texture_load(texture, texels.data(), 2, 2));
	engine::Mesh_Handle mesh = 0;
	ASSERT_TRUE(renderer.mesh_load(mesh, quad_data()));
	renderer.texture_bind_set(texture);

	//红色像素计数（外通道饱和即视为四边形覆盖）
	auto red_count = [](const std::vector<uint8_t>& pixels)
	{
		uint32_t count = 0;
		for (uint32_t y = 0; y < WINDOW_HEIGHT; ++y)
			for (uint32_t x = 0; x < WINDOW_WIDTH; ++x)
				if (outer_max(pixel_get(pixels, x, y)) >= 200)
					++count;
		return count;
	};
	//按给定相机位置 / 正文模式渲染一帧并计数
	auto render_count = [&](float camera_z, bool ortho)
	{
		engine::Scene_Data scene{};
		scene.position[2] = camera_z;
		scene.ortho = ortho;
		scene.ortho_height = 2.0f;
		renderer.scene_apply(scene);
		renderer.mesh_draw(mesh, nullptr);
		std::vector<uint8_t> pixels(pixel_bytes());
		EXPECT_TRUE(renderer.frame_render(pixels.data(), static_cast<std::uint32_t>(pixels.size())));
		return red_count(pixels);
	};

	//透视：近大远小
	uint32_t perspective_near = render_count(3.0f, false);
	uint32_t perspective_far = render_count(5.0f, false);
	RecordProperty("ortho_perspective_near", perspective_near);
	RecordProperty("ortho_perspective_far", perspective_far);
	EXPECT_GT(perspective_near, 0u) << "透视近距帧无四边形（用例前提不成立）";
	EXPECT_LT(perspective_far, perspective_near * 8 / 10) << "透视下远距未明显变小";

	//正交：远近一致
	uint32_t ortho_near = render_count(3.0f, true);
	uint32_t ortho_far = render_count(5.0f, true);
	RecordProperty("ortho_near", ortho_near);
	RecordProperty("ortho_far", ortho_far);
	EXPECT_GT(ortho_near, 0u) << "正交帧无四边形（用例前提不成立）";
	//允许 2% 的像素级误差
	uint32_t tolerance = ortho_near / 50 + 1;
	EXPECT_LE(std::abs(static_cast<int>(ortho_far) - static_cast<int>(ortho_near)), static_cast<int>(tolerance))
		<< "正交投影下远距画面占比改变（正交" << ortho_near << " / " << ortho_far << "）";

	renderer.mesh_unload(mesh);
	renderer.texture_unload(texture);
}

/*
立方体正面随光照方向明暗反转（第三十轮）
口径：带逐面法线的立方体 ＋ 默认相机（+Z 3）→ 中心像素落在 +Z 面；
      光沿 -Z 传播时该面满照，光沿 +Z 传播时该面照度为 0。
*/
TEST_F(Renderer_Scene_Test, 立方体正面随光照方向明暗反转)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	uint64_t texture = 0;
	std::vector<uint8_t> texels = solid_white();
	ASSERT_TRUE(renderer.texture_load(texture, texels.data(), 2, 2));
	engine::Mesh_Handle mesh = 0;
	ASSERT_TRUE(renderer.mesh_load(mesh, cube_data(0.5f)));
	renderer.texture_bind_set(texture);

	//渲染一帧并返回中心亮度（lambda 内用 EXPECT：ASSERT 不允许出现在有返回值的函数中）
	auto render_brightness = [&]()
	{
		EXPECT_TRUE(renderer.mesh_draw(mesh, nullptr));
		std::vector<uint8_t> pixels(pixel_bytes());
		EXPECT_TRUE(renderer.frame_render(pixels.data(), static_cast<std::uint32_t>(pixels.size())));
		return center_brightness(pixels);
	};

	//光沿 -Z（默认）：+Z 面正对光源 → 满照
	uint32_t lit = render_brightness();
	//光沿 +Z：+Z 面背光 → 照度 0
	const float toward_camera[3] = { 0.0f, 0.0f, 1.0f };
	const float white_light[3] = { 1.0f, 1.0f, 1.0f };
	renderer.light_set(toward_camera, white_light, 1.0f, 0.0f);
	uint32_t dark = render_brightness();
	RecordProperty("cube_lit", lit);
	RecordProperty("cube_dark", dark);
	EXPECT_GE(lit, 200u) << "立方体正面未被照亮（中心亮度 " << lit << "）";
	EXPECT_LE(dark, 40u) << "立方体正面未被压暗（中心亮度 " << dark << "）";

	renderer.mesh_unload(mesh);
	renderer.texture_unload(texture);
}

/*
旋转立方体的两个可见面亮度不同（第三十轮）
口径：立方体绕 Y 轴 -35°（+X 面转向右侧、+Z 面转向左侧），光沿 -X 传播（光源在 +X）：
      +X 面受光（约 0.82）、+Z 面背光（0）；比较左右两侧区域的最大亮度，避开像素级对齐。
*/
TEST_F(Renderer_Scene_Test, 旋转立方体两侧面明暗不同)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	uint64_t texture = 0;
	std::vector<uint8_t> texels = solid_white();
	ASSERT_TRUE(renderer.texture_load(texture, texels.data(), 2, 2));
	engine::Mesh_Handle mesh = 0;
	ASSERT_TRUE(renderer.mesh_load(mesh, cube_data(0.6f)));
	renderer.texture_bind_set(texture);
	//光沿 -X 传播（光源在 +X 一侧）
	const float from_positive_x[3] = { -1.0f, 0.0f, 0.0f };
	const float white_light[3] = { 1.0f, 1.0f, 1.0f };
	renderer.light_set(from_positive_x, white_light, 1.0f, 0.0f);
	//旋转 -35°（+X 面转向相机右侧）
	std::array<float, 16> model = rotation_y_matrix(-0.6109f);
	ASSERT_TRUE(renderer.mesh_draw(mesh, model.data()));
	std::vector<uint8_t> pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<std::uint32_t>(pixels.size())));

	//左右两侧区域的最大亮度（立方体投影约占中心 40% 宽，故取中心两侧各 1/8 宽、纵向中部）
	uint32_t left_max = 0;
	uint32_t right_max = 0;
	for (uint32_t y = WINDOW_HEIGHT / 4; y < WINDOW_HEIGHT * 3 / 4; ++y)
	{
		for (uint32_t x = WINDOW_WIDTH * 3 / 8; x < WINDOW_WIDTH * 7 / 16; ++x)
		{
			std::array<uint8_t, 4> value = pixel_get(pixels, x, y);
			uint32_t brightness = (static_cast<uint32_t>(value[0]) + value[1] + value[2]) / 3;
			if (brightness > left_max)
				left_max = brightness;
		}
		for (uint32_t x = WINDOW_WIDTH * 9 / 16; x < WINDOW_WIDTH * 5 / 8; ++x)
		{
			std::array<uint8_t, 4> value = pixel_get(pixels, x, y);
			uint32_t brightness = (static_cast<uint32_t>(value[0]) + value[1] + value[2]) / 3;
			if (brightness > right_max)
				right_max = brightness;
		}
	}
	RecordProperty("cube_left_max", left_max);
	RecordProperty("cube_right_max", right_max);
	EXPECT_GE(right_max, 120u) << "受光面亮度不足（右侧 " << right_max << "）";
	//左侧取样区含背景（清屏色亮度约 71），故以「不高于背景」为压暗判据
	EXPECT_LE(left_max, 90u) << "背光面未被压暗（左侧 " << left_max << "）";
	EXPECT_GT(static_cast<int>(right_max), static_cast<int>(left_max) + 60) << "两侧面明暗未区分";

	renderer.mesh_unload(mesh);
	renderer.texture_unload(texture);
}

/*
环境光抬升背光面（第三十轮）
口径：法线 (0,0,-1) 的全屏四边形在默认光下照度 0；`light_set` 把环境光抬到 0.5 后中心亮度约 127。
*/
TEST_F(Renderer_Scene_Test, 环境光抬升背光面)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	uint64_t texture = 0;
	std::vector<uint8_t> texels = solid_white();
	ASSERT_TRUE(renderer.texture_load(texture, texels.data(), 2, 2));
	engine::Mesh_Handle mesh = 0;
	ASSERT_TRUE(renderer.mesh_load(mesh, quad_data_with_normal(0.0f, 0.0f, -1.0f)));
	renderer.texture_bind_set(texture);

	//渲染一帧并返回中心亮度（lambda 内用 EXPECT：ASSERT 不允许出现在有返回值的函数中）
	auto render_brightness = [&](const float* model)
	{
		EXPECT_TRUE(renderer.mesh_draw(mesh, model));
		std::vector<uint8_t> pixels(pixel_bytes());
		EXPECT_TRUE(renderer.frame_render(pixels.data(), static_cast<std::uint32_t>(pixels.size())));
		return center_brightness(pixels);
	};
	//默认光照（环境光 0）：背光面全黑
	uint32_t without_ambient = render_brightness(nullptr);
	//环境光 0.5：背光面被抬亮
	const float forward[3] = { 0.0f, 0.0f, -1.0f };
	const float white_light[3] = { 1.0f, 1.0f, 1.0f };
	renderer.light_set(forward, white_light, 1.0f, 0.5f);
	uint32_t with_ambient = render_brightness(nullptr);
	RecordProperty("ambient_off", without_ambient);
	RecordProperty("ambient_half", with_ambient);
	EXPECT_LE(without_ambient, 40u) << "背光面未全黑（亮度 " << without_ambient << "）";
	EXPECT_GE(with_ambient, 90u) << "环境光未抬亮背光面（亮度 " << with_ambient << "）";
	EXPECT_LE(with_ambient, 210u) << "环境光亮度超出预期（亮度 " << with_ambient << "）";

	renderer.mesh_unload(mesh);
	renderer.texture_unload(texture);
}
/*
点光源距离衰减（第三十一轮）
口径：法线 (0,0,1) 的全屏四边形，点光源放在 +Z 轴上不同距离、半径 3：
      距离 1 → 衰减 0.67（亮），距离 2.9 → 衰减 0.03（暗）。
*/
TEST_F(Renderer_Scene_Test, 点光源近远亮度不同)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	uint64_t texture = 0;
	std::vector<uint8_t> texels = solid_white();
	ASSERT_TRUE(renderer.texture_load(texture, texels.data(), 2, 2));
	engine::Mesh_Handle mesh = 0;
	ASSERT_TRUE(renderer.mesh_load(mesh, quad_data_with_normal(0.0f, 0.0f, 1.0f)));
	renderer.texture_bind_set(texture);
	const float white_light[3] = { 1.0f, 1.0f, 1.0f };

	//渲染一帧并返回中心亮度（lambda 内用 EXPECT：ASSERT 不允许出现在有返回值的函数中）
	auto render_brightness = [&]()
	{
		EXPECT_TRUE(renderer.mesh_draw(mesh, nullptr));
		std::vector<uint8_t> pixels(pixel_bytes());
		EXPECT_TRUE(renderer.frame_render(pixels.data(), static_cast<std::uint32_t>(pixels.size())));
		return center_brightness(pixels);
	};

	//近距点光源（距离 1，半径 3）
	const float near_position[3] = { 0.0f, 0.0f, 1.0f };
	renderer.light_point_set(near_position, white_light, 3.0f, 1.0f, 0.0f);
	uint32_t near_brightness = render_brightness();
	//远距点光源（距离 2.9，半径 3 → 衰减约 0.03）
	const float far_position[3] = { 0.0f, 0.0f, 2.9f };
	renderer.light_point_set(far_position, white_light, 3.0f, 1.0f, 0.0f);
	uint32_t far_brightness = render_brightness();
	RecordProperty("point_near", near_brightness);
	RecordProperty("point_far", far_brightness);
	EXPECT_GE(near_brightness, 170u) << "近距点光源未照亮（亮度 " << near_brightness << "）";
	EXPECT_LE(far_brightness, 80u) << "远距点光源未衰减（亮度 " << far_brightness << "）";
	EXPECT_GT(static_cast<int>(near_brightness), static_cast<int>(far_brightness) + 80) << "距离衰减未生效";

	renderer.mesh_unload(mesh);
	renderer.texture_unload(texture);
}

/*
镜面高光提升亮度，非法高光参数整次忽略（第三十一轮）
口径：中灰纹理 ＋ 法线 (0,0,1) 全屏四边形；光沿 -Z、相机在 +Z，故 H = N，高光强度直接叠加到照度上。
      高光 0 时亮度约 188（线性 0.5 的 sRGB 编码）；高光 0.6 时被钳制到 1.0（约 255）。
*/
TEST_F(Renderer_Scene_Test, 镜面高光提升亮度)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	uint64_t texture = 0;
	std::vector<uint8_t> texels = solid_gray();
	ASSERT_TRUE(renderer.texture_load(texture, texels.data(), 2, 2));
	engine::Mesh_Handle mesh = 0;
	ASSERT_TRUE(renderer.mesh_load(mesh, quad_data_with_normal(0.0f, 0.0f, 1.0f)));
	renderer.texture_bind_set(texture);

	//渲染一帧并返回中心亮度（lambda 内用 EXPECT：ASSERT 不允许出现在有返回值的函数中）
	auto render_brightness = [&]()
	{
		EXPECT_TRUE(renderer.mesh_draw(mesh, nullptr));
		std::vector<uint8_t> pixels(pixel_bytes());
		EXPECT_TRUE(renderer.frame_render(pixels.data(), static_cast<std::uint32_t>(pixels.size())));
		return center_brightness(pixels);
	};

	//高光关闭（默认）
	uint32_t plain = render_brightness();
	//高光 0.6、光泽度 16（H = N → pow(1, 16) = 1）
	renderer.specular_set(0.6f, 16.0f);
	uint32_t highlighted = render_brightness();
	RecordProperty("spec_plain", plain);
	RecordProperty("spec_highlight", highlighted);
	EXPECT_GE(highlighted, 230u) << "高光未提升亮度（" << plain << " → " << highlighted << "）";
	EXPECT_GT(static_cast<int>(highlighted), static_cast<int>(plain) + 30) << "高光增量不足";
	//非法高光参数（负强度 / 光泽度小于 1）整次忽略 → 画面与上一帧逐字节一致
	renderer.specular_set(-1.0f, 0.0f);
	EXPECT_TRUE(renderer.mesh_draw(mesh, nullptr));
	std::vector<uint8_t> after(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(after.data(), static_cast<std::uint32_t>(after.size())));
	EXPECT_EQ(center_brightness(after), highlighted) << "非法高光参数改变了画面";

	renderer.mesh_unload(mesh);
	renderer.texture_unload(texture);
}