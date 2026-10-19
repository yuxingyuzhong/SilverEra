/*
渲染多管线测试（L2 隐藏窗口 + 像素回读）
覆盖：具名图形管线的登记 / 卸载 / 选择入口；同帧两项分别挂「默认场景管线」与
      「具名纯色管线」时各自像素结果正确；无效句柄回落默认场景管线。
断言一律「格式无关」：绿 / 红判定只依赖绿通道（RGBA 与 BGRA 下恒为第 2 字节，下标 1），
      R / B 通道序随表面格式互换，故用「外通道取较大者」判定红色。
两种端点色（0 与 1）在 UNORM 与 sRGB 附件下编码一致，故红绿判定不随附件传输函数变化。
*/
#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

//获取渲染器门面（含混合档枚举）与网格数据
#include "Application/Renderer/src/render/Renderer/渲染器.h"

namespace
{
	//窗口尺寸（与回读断言配套）
	constexpr uint32_t WINDOW_WIDTH = 64;
	constexpr uint32_t WINDOW_HEIGHT = 36;
	//具名纯色管线所用着色器对（顶点复用场景顶点着色器；片元为纯红）
	const char* const VERTEX_SHADER = "场景.vert.spv";
	const char* const SOLID_FRAGMENT_SHADER = "纯色.frag.spv";

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

	//判断像素是否近似纯绿（绿通道高、外通道低）
	bool pixel_is_green(const std::array<uint8_t, 4>& value)
	{
		//绿通道（下标 1）须为高
		if (value[1] < 200)
			return false;
		//外通道须为低
		if (outer_max(value) > 60)
			return false;
		return true;
	}

	//判断像素是否近似纯红（绿通道低、外通道高）
	bool pixel_is_red(const std::array<uint8_t, 4>& value)
	{
		//绿通道须为低
		if (value[1] > 60)
			return false;
		//外通道须为高（红通道在 RGBA 与 BGRA 下分别落在外通道之一）
		if (outer_max(value) < 200)
			return false;
		return true;
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

	//构造纯绿纹理（RGBA8：rgb = 绿，alpha = 255）
	std::vector<uint8_t> solid_green(uint32_t width, uint32_t height)
	{
		std::vector<uint8_t> pixels(static_cast<std::size_t>(width) * height * 4);
		for (std::size_t index = 0; index < pixels.size(); index += 4)
		{
			pixels[index] = 0;
			pixels[index + 1] = 255;
			pixels[index + 2] = 0;
			pixels[index + 3] = 255;
		}
		return pixels;
	}

	/*
	构造「左半屏」模型矩阵（列主序）
	x / y 各放大 3 倍并把 x 左移 3，使四边形横向覆盖世界 x ∈ [-6, 0]、纵向覆盖世界 y ∈ [-3, 3]；
	经投影与裁剪后恰好占据左半屏（NDC x ∈ [-1, 0]，纵向占满）。
	放大 3 倍足以让四边形在 x 上越过 NDC 边界被裁剪，故「左半屏」不依赖相机视场角的具体取值。
	*/
	std::array<float, 16> left_half_matrix(void)
	{
		return {
			3.0f, 0.0f, 0.0f, 0.0f,
			0.0f, 3.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			-3.0f, 0.0f, 0.0f, 1.0f,
		};
	}

	//构造「右半屏」模型矩阵（列主序）：x / y 各放大 3 倍并把 x 右移 3，覆盖右半屏（NDC x ∈ [0, 1]）
	std::array<float, 16> right_half_matrix(void)
	{
		return {
			3.0f, 0.0f, 0.0f, 0.0f,
			0.0f, 3.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			3.0f, 0.0f, 0.0f, 1.0f,
		};
	}
}

//渲染多管线测试夹具
class Renderer_Multi_Pipeline_Test : public ::testing::Test
{
};

//未初始化时登记管线失败
TEST_F(Renderer_Multi_Pipeline_Test, 未初始化登记管线失败)
{
	engine::Renderer renderer;
	//未 init 直接登记
	uint64_t handle = 12345;
	EXPECT_FALSE(renderer.pipeline_load(handle, VERTEX_SHADER, SOLID_FRAGMENT_SHADER));
	//句柄须被复位为 0（无效）
	EXPECT_EQ(handle, 0u);
}

//着色器文件不存在时登记失败
TEST_F(Renderer_Multi_Pipeline_Test, 不存在的着色器登记失败)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	//片元着色器文件名不存在
	uint64_t handle = 0;
	EXPECT_FALSE(renderer.pipeline_load(handle, VERTEX_SHADER, "不存在的着色器.frag.spv"));
	EXPECT_EQ(handle, 0u);
	//帧仍正常推进（登记失败不破坏帧循环）
	std::vector<uint8_t> pixels(pixel_bytes());
	EXPECT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
}

//同帧两项分别挂默认场景管线与具名纯色管线，各自像素结果正确
TEST_F(Renderer_Multi_Pipeline_Test, 同帧两项不同管线各自正确)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	//若本机不支持像素回读则跳过
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	//加载纯绿纹理并绑定（供默认场景管线采样）
	uint64_t texture = 0;
	std::vector<uint8_t> texels = solid_green(2, 2);
	ASSERT_TRUE(renderer.texture_load(texture, texels.data(), 2, 2));
	renderer.texture_bind_set(texture);
	//加载四边形
	engine::Mesh_Handle handle = 0;
	ASSERT_TRUE(renderer.mesh_load(handle, quad_data()));

	//登记具名纯色管线（片元输出纯红）
	uint64_t solid_pipeline = 0;
	ASSERT_TRUE(renderer.pipeline_load(solid_pipeline, VERTEX_SHADER, SOLID_FRAGMENT_SHADER));
	EXPECT_GE(solid_pipeline, 1u);

	//第一项：默认场景管线（贴绿纹理）画左半屏
	std::array<float, 16> left = left_half_matrix();
	ASSERT_TRUE(renderer.mesh_draw(handle, left.data()));
	//切换到具名纯色管线
	renderer.pipeline_bind_set(solid_pipeline);
	//第二项：具名纯色管线画右半屏
	std::array<float, 16> right = right_half_matrix();
	ASSERT_TRUE(renderer.mesh_draw(handle, right.data()));

	//渲染并回读
	std::vector<uint8_t> pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
	//左 / 右半屏的采样列（分别落在两个四边形各自的覆盖区内）
	const uint32_t left_x = WINDOW_WIDTH / 4;
	const uint32_t right_x = WINDOW_WIDTH * 3 / 4;
	//三个纵向采样行（顶 / 中 / 底）
	std::array<uint32_t, 3> sample_rows = { 2u, WINDOW_HEIGHT / 2, WINDOW_HEIGHT - 3 };
	for (uint32_t sample_y : sample_rows)
	{
		//左半屏采样：默认场景管线 → 纹理绿
		std::array<uint8_t, 4> left_value = pixel_get(pixels, left_x, sample_y);
		EXPECT_TRUE(pixel_is_green(left_value)) << "左半屏非纹理绿（y=" << sample_y << "）：" << pixel_text(left_value);
		//右半屏采样：具名纯色管线 → 纯红
		std::array<uint8_t, 4> right_value = pixel_get(pixels, right_x, sample_y);
		EXPECT_TRUE(pixel_is_red(right_value)) << "右半屏非纯色红（y=" << sample_y << "）：" << pixel_text(right_value);
	}

	//切回默认场景管线后重复绘制左半屏，确认档位可往返（同帧登记须逐帧重做）
	renderer.pipeline_bind_set(0);
	ASSERT_TRUE(renderer.mesh_draw(handle, left.data()));
	std::vector<uint8_t> again(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(again.data(), static_cast<uint32_t>(again.size())));
	EXPECT_TRUE(pixel_is_green(pixel_get(again, left_x, WINDOW_HEIGHT / 2)))
		<< "切回默认管线后未恢复纹理色";

	renderer.pipeline_unload(solid_pipeline);
	renderer.mesh_unload(handle);
	renderer.texture_unload(texture);
}

//无效句柄回落默认场景管线
TEST_F(Renderer_Multi_Pipeline_Test, 无效句柄回落默认管线)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	//加载纯绿纹理并绑定
	uint64_t texture = 0;
	std::vector<uint8_t> texels = solid_green(2, 2);
	ASSERT_TRUE(renderer.texture_load(texture, texels.data(), 2, 2));
	renderer.texture_bind_set(texture);
	//加载四边形
	engine::Mesh_Handle handle = 0;
	ASSERT_TRUE(renderer.mesh_load(handle, quad_data()));

	//选择一个从未登记的句柄
	renderer.pipeline_bind_set(99999u);
	//全屏绘制（单位矩阵）
	ASSERT_TRUE(renderer.mesh_draw(handle, nullptr));
	std::vector<uint8_t> pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
	//中心应为默认场景管线的纹理绿
	EXPECT_TRUE(pixel_is_green(pixel_get(pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2)))
		<< "无效句柄未回落默认管线";

	renderer.mesh_unload(handle);
	renderer.texture_unload(texture);
}

//卸载具名管线后其句柄失效并回落默认场景管线
TEST_F(Renderer_Multi_Pipeline_Test, 卸载后句柄失效)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	//加载纯绿纹理并绑定
	uint64_t texture = 0;
	std::vector<uint8_t> texels = solid_green(2, 2);
	ASSERT_TRUE(renderer.texture_load(texture, texels.data(), 2, 2));
	renderer.texture_bind_set(texture);
	//加载四边形
	engine::Mesh_Handle handle = 0;
	ASSERT_TRUE(renderer.mesh_load(handle, quad_data()));

	//登记并选择具名纯色管线
	uint64_t solid_pipeline = 0;
	ASSERT_TRUE(renderer.pipeline_load(solid_pipeline, VERTEX_SHADER, SOLID_FRAGMENT_SHADER));
	renderer.pipeline_bind_set(solid_pipeline);

	//卸载该管线（当前所选即被卸载，应自动回落默认）
	renderer.pipeline_unload(solid_pipeline);
	//卸载后再次选择该句柄（已失效）亦应回落默认，不崩溃
	renderer.pipeline_bind_set(solid_pipeline);

	//全屏绘制 → 应走默认场景管线（纹理绿），且帧正常推进
	ASSERT_TRUE(renderer.mesh_draw(handle, nullptr));
	std::vector<uint8_t> pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
	EXPECT_TRUE(pixel_is_green(pixel_get(pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2)))
		<< "卸载后未回落默认场景管线";

	renderer.mesh_unload(handle);
	renderer.texture_unload(texture);
}
