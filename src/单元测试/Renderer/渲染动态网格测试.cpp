/*
动态网格顶点更新测试（第一百五十九轮；L2 隐藏窗口 + 像素回读）
覆盖：未初始化 / 无效句柄 / 区间越界 / 空指针拒绝；顶点整体平移后画面随之移动；
      局部包围盒只扩不缩（保守扩张）。
断言口径：以「纹理白像素」的质心行衡量几何位置，允许 ±3 行误差；
          颜色三通道取最小值判定，避免受交换链通道序影响。
*/
#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

//获取渲染器门面与网格数据
#include "Application/Renderer/src/render/Renderer/渲染器.h"

namespace
{
	//窗口尺寸（与回读断言配套）
	constexpr uint32_t WINDOW_WIDTH = 64;
	constexpr uint32_t WINDOW_HEIGHT = 36;
	//纹理白判定阈值（内置默认白纹理）
	constexpr uint8_t WHITE_MIN = 250;
	//顶点平移量（世界单位；相机竖向半高约 1.732，故约上移 8 行）
	constexpr float VERTEX_SHIFT_Y = 0.8f;
	//质心行允许误差（取整与光栅化误差）
	constexpr int CENTROID_TOLERANCE = 3;

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

	//颜色三通道最小值（剔除 alpha；避免受通道序影响）
	uint8_t color_min(const std::array<uint8_t, 4>& value)
	{
		//先取首通道
		uint8_t result = value[0];
		if (value[1] < result)
			result = value[1];
		if (value[2] < result)
			result = value[2];
		return result;
	}

	//判断像素是否为纹理白
	bool pixel_is_white(const std::array<uint8_t, 4>& value)
	{
		return color_min(value) >= WHITE_MIN;
	}

	//统计纹理白像素行质心（无白像素返回 -1）
	float white_centroid_row(const std::vector<uint8_t>& pixels)
	{
		//白像素总行号与数量
		double sum = 0.0;
		uint32_t count = 0;
		for (uint32_t y = 0; y < WINDOW_HEIGHT; ++y)
			for (uint32_t x = 0; x < WINDOW_WIDTH; ++x)
				//命中即累计
				if (pixel_is_white(pixel_get(pixels, x, y)))
				{
					sum += static_cast<double>(y);
					++count;
				}
		if (count == 0)
			return -1.0f;
		return static_cast<float>(sum / static_cast<double>(count));
	}

	//统计全部纹理白像素数量
	uint32_t white_count(const std::vector<uint8_t>& pixels)
	{
		uint32_t count = 0;
		for (uint32_t y = 0; y < WINDOW_HEIGHT; ++y)
			for (uint32_t x = 0; x < WINDOW_WIDTH; ++x)
				//命中即计数
				if (pixel_is_white(pixel_get(pixels, x, y)))
					++count;
		return count;
	}
}

//动态网格顶点更新测试夹具
class Renderer_Dynamic_Mesh_Test : public ::testing::Test
{
};

//未初始化时更新失败
TEST_F(Renderer_Dynamic_Mesh_Test, 未初始化更新失败)
{
	engine::Renderer renderer;
	engine::Vertex vertex = { { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f } };
	EXPECT_FALSE(renderer.mesh_update_vertices(1, 0, 1, &vertex));
}

//无效句柄 / 区间越界 / 空指针均被拒
TEST_F(Renderer_Dynamic_Mesh_Test, 参数校验拒绝)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	engine::Mesh_Handle handle = 0;
	ASSERT_TRUE(renderer.mesh_triangle_load(handle));
	engine::Vertex vertex = { { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f } };

	//无效句柄
	EXPECT_FALSE(renderer.mesh_update_vertices(handle + 99, 0, 1, &vertex));
	//区间越界（三角形只有 3 个顶点）
	EXPECT_FALSE(renderer.mesh_update_vertices(handle, 2, 2, &vertex));
	//起始顶点自身越界
	EXPECT_FALSE(renderer.mesh_update_vertices(handle, 3, 1, &vertex));
	//数量为零 / 空指针
	EXPECT_FALSE(renderer.mesh_update_vertices(handle, 0, 0, &vertex));
	EXPECT_FALSE(renderer.mesh_update_vertices(handle, 0, 1, nullptr));

	//卸载后句柄失效
	renderer.mesh_unload(handle);
	EXPECT_FALSE(renderer.mesh_update_vertices(handle, 0, 1, &vertex));
}

//顶点整体平移后画面随之移动
TEST_F(Renderer_Dynamic_Mesh_Test, 顶点平移改变画面位置)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	//回落内置默认白纹理
	renderer.texture_bind_set(0);
	engine::Mesh_Handle handle = 0;
	ASSERT_TRUE(renderer.mesh_triangle_load(handle));

	//第一帧：三角形居中
	ASSERT_TRUE(renderer.mesh_draw(handle, nullptr));
	std::vector<uint8_t> baseline(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(baseline.data(), static_cast<uint32_t>(baseline.size())));
	float baseline_row = white_centroid_row(baseline);
	uint32_t baseline_white = white_count(baseline);
	ASSERT_GT(baseline_white, 0u) << "基线帧未出现纹理色像素：绘制路径异常";
	ASSERT_GE(baseline_row, 0.0f);

	/*
	第二帧：把三个顶点整体上移
	顶点结构须与创建时一致（位置 / UV / 法线），只改位置 Y。
	*/
	std::array<engine::Vertex, 3> shifted = {
		engine::Vertex{ { -0.5f, -0.5f + VERTEX_SHIFT_Y, 0.0f }, { 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f } },
		engine::Vertex{ { 0.5f, -0.5f + VERTEX_SHIFT_Y, 0.0f }, { 1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f } },
		engine::Vertex{ { 0.0f, 0.5f + VERTEX_SHIFT_Y, 0.0f }, { 0.5f, 1.0f }, { 0.0f, 0.0f, 1.0f } },
	};
	ASSERT_TRUE(renderer.mesh_update_vertices(handle, 0, 3, shifted.data()));

	ASSERT_TRUE(renderer.mesh_draw(handle, nullptr));
	std::vector<uint8_t> shifted_frame(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(shifted_frame.data(), static_cast<uint32_t>(shifted_frame.size())));
	float shifted_row = white_centroid_row(shifted_frame);
	ASSERT_GE(shifted_row, 0.0f) << "更新后帧未出现纹理色像素：顶点写入未生效";

	//质心明显上移（屏幕行号变小）；方向性断言避免受光栅化抖动影响
	EXPECT_LT(shifted_row, baseline_row - 4.0f) << "顶点上移后画面未同步上移：基线行 "
		<< baseline_row << "，更新后行 " << shifted_row;

	//面积量级不变（只看位置变化，不看缩放）
	uint32_t shifted_white = white_count(shifted_frame);
	EXPECT_GE(shifted_white, baseline_white / 2) << "更新后覆盖面积异常缩小：基线 "
		<< baseline_white << "，更新后 " << shifted_white;
	EXPECT_LE(shifted_white, baseline_white * 2) << "更新后覆盖面积异常放大：基线 "
		<< baseline_white << "，更新后 " << shifted_white;

	renderer.mesh_unload(handle);
}

//包围盒只扩不缩（保守扩张）
TEST_F(Renderer_Dynamic_Mesh_Test, 包围盒保守扩张)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	engine::Mesh_Handle handle = 0;
	ASSERT_TRUE(renderer.mesh_triangle_load(handle));

	//基线包围盒（三角形 ±0.5）
	float baseline_min[3] = { 0.0f, 0.0f, 0.0f };
	float baseline_max[3] = { 0.0f, 0.0f, 0.0f };
	ASSERT_TRUE(renderer.mesh_bounds(handle, baseline_min, baseline_max));
	EXPECT_FLOAT_EQ(baseline_min[1], -0.5f);
	EXPECT_FLOAT_EQ(baseline_max[1], 0.5f);

	//把三个顶点整体上移 0.8：上界扩张、下界保持（只扩不缩）
	std::array<engine::Vertex, 3> shifted = {
		engine::Vertex{ { -0.5f, -0.5f + VERTEX_SHIFT_Y, 0.0f }, { 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f } },
		engine::Vertex{ { 0.5f, -0.5f + VERTEX_SHIFT_Y, 0.0f }, { 1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f } },
		engine::Vertex{ { 0.0f, 0.5f + VERTEX_SHIFT_Y, 0.0f }, { 0.5f, 1.0f }, { 0.0f, 0.0f, 1.0f } },
	};
	ASSERT_TRUE(renderer.mesh_update_vertices(handle, 0, 3, shifted.data()));
	float updated_min[3] = { 0.0f, 0.0f, 0.0f };
	float updated_max[3] = { 0.0f, 0.0f, 0.0f };
	ASSERT_TRUE(renderer.mesh_bounds(handle, updated_min, updated_max));
	//下界未收缩
	EXPECT_FLOAT_EQ(updated_min[1], baseline_min[1]);
	//上界扩到新顶点最高处
	EXPECT_FLOAT_EQ(updated_max[1], 0.5f + VERTEX_SHIFT_Y);

	renderer.mesh_unload(handle);
}

/*
索引区域更新（第一百六十轮）
口径：三角形三个索引改成退化值 {0,0,0} 后不产生任何像素；再恢复 {0,1,2} 后覆盖面积回到基线；
      索引越界 / 区间越界 / 空指针均被拒绝。
*/
TEST_F(Renderer_Dynamic_Mesh_Test, 索引区域更新改变覆盖)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	//回落内置默认白纹理
	renderer.texture_bind_set(0);
	engine::Mesh_Handle handle = 0;
	ASSERT_TRUE(renderer.mesh_triangle_load(handle));

	//基线：正常三角形
	ASSERT_TRUE(renderer.mesh_draw(handle, nullptr));
	std::vector<uint8_t> baseline(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(baseline.data(), static_cast<uint32_t>(baseline.size())));
	uint32_t baseline_white = white_count(baseline);
	ASSERT_GT(baseline_white, 0u) << "基线帧未出现纹理色像素";

	//退化索引：三顶点同点，零面积（不产生像素）
	const uint32_t degenerate[3] = { 0, 0, 0 };
	ASSERT_TRUE(renderer.mesh_update_indices(handle, 0, 3, degenerate));
	ASSERT_TRUE(renderer.mesh_draw(handle, nullptr));
	std::vector<uint8_t> degenerated(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(degenerated.data(), static_cast<uint32_t>(degenerated.size())));
	EXPECT_EQ(white_count(degenerated), 0u) << "退化索引仍产生覆盖：索引写入未生效";

	//恢复原索引
	const uint32_t restored[3] = { 0, 1, 2 };
	ASSERT_TRUE(renderer.mesh_update_indices(handle, 0, 3, restored));
	ASSERT_TRUE(renderer.mesh_draw(handle, nullptr));
	std::vector<uint8_t> recovered(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(recovered.data(), static_cast<uint32_t>(recovered.size())));
	EXPECT_EQ(white_count(recovered), baseline_white) << "恢复索引后覆盖面与基线不一致";

	//索引越界（顶点只有 3 个）/ 区间越界 / 空指针均拒绝
	const uint32_t out_of_range[3] = { 0, 1, 3 };
	EXPECT_FALSE(renderer.mesh_update_indices(handle, 0, 3, out_of_range));
	EXPECT_FALSE(renderer.mesh_update_indices(handle, 2, 2, restored));
	EXPECT_FALSE(renderer.mesh_update_indices(handle, 0, 3, nullptr));
	EXPECT_FALSE(renderer.mesh_update_indices(handle + 99, 0, 3, restored));
	renderer.mesh_unload(handle);
	EXPECT_FALSE(renderer.mesh_update_indices(handle, 0, 3, restored));
}
