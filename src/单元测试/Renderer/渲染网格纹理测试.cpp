/*
渲染网格与纹理测试（L2 隐藏窗口）
覆盖：未初始化守卫、三角形网格加载与绘制、同一帧多网格绘制、无效句柄绘制被拒、
      卸载后绘制被拒与重复卸载幂等、纹理加载（RGBA8 字节流 / 程序生成棋盘格）、
      纹理绑定与无效句柄回落默认、纹理卸载后再绑定、多纹理创建后逐条卸载。
说明：本用例只按渲染层公共面编写，不涉及 Vulkan / 描述符等底层细节。
*/
#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

//获取渲染器门面与网格数据
#include "Application/Renderer/src/render/Renderer/渲染器.h"

namespace
{
	//构造隐藏窗口配置
	engine::Renderer_Config hidden_config(void)
	{
		engine::Renderer_Config config;
		config.width = 320;
		config.height = 180;
		config.hidden = true;
		return config;
	}

	//构造三角形网格（三顶点、三索引）
	engine::Mesh_Data triangle_data(void)
	{
		engine::Mesh_Data data;
		data.vertices = {
			{ { 0.0f, 0.5f, 0.0f }, { 0.5f, 0.0f } },
			{ { 0.5f, -0.5f, 0.0f }, { 1.0f, 1.0f } },
			{ { -0.5f, -0.5f, 0.0f }, { 0.0f, 1.0f } },
		};
		data.indices = { 0, 1, 2 };
		return data;
	}
}

//网格测试夹具
class Renderer_Mesh_Test : public ::testing::Test
{
};

//未初始化时加载网格失败，句柄保持无效
TEST_F(Renderer_Mesh_Test, 未初始化加载网格失败)
{
	engine::Renderer renderer;
	engine::Mesh_Handle handle = 0;
	EXPECT_FALSE(renderer.mesh_load(handle, triangle_data()));
	EXPECT_EQ(handle, 0u);
}

//加载三角形成功，句柄有效并可绘制、推进一帧
TEST_F(Renderer_Mesh_Test, 加载三角形并绘制)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	engine::Mesh_Handle handle = 0;
	ASSERT_TRUE(renderer.mesh_load(handle, triangle_data()));
	//句柄有效
	EXPECT_NE(handle, 0u);
	//登记单位矩阵绘制
	EXPECT_TRUE(renderer.mesh_draw(handle, nullptr));
	//推进一帧（消费该待绘制项）
	EXPECT_TRUE(renderer.frame_render());
	renderer.mesh_unload(handle);
}

//同一帧内登记两个网格，可由一帧一并消费
TEST_F(Renderer_Mesh_Test, 同帧绘制两个网格)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	engine::Mesh_Handle first = 0;
	engine::Mesh_Handle second = 0;
	ASSERT_TRUE(renderer.mesh_load(first, triangle_data()));
	ASSERT_TRUE(renderer.mesh_load(second, triangle_data()));
	//同一帧内两次登记（各自独立模型矩阵）
	EXPECT_TRUE(renderer.mesh_draw(first, nullptr));
	EXPECT_TRUE(renderer.mesh_draw(second, nullptr));
	//推进一帧，两个网格一并被消费
	EXPECT_TRUE(renderer.frame_render());
	renderer.mesh_unload(first);
	renderer.mesh_unload(second);
}

//绘制无效句柄失败
TEST_F(Renderer_Mesh_Test, 绘制无效句柄失败)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	//未加载过的句柄
	EXPECT_FALSE(renderer.mesh_draw(12345u, nullptr));
}

//卸载后绘制失败，且重复卸载不报错
TEST_F(Renderer_Mesh_Test, 卸载后绘制失败)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	engine::Mesh_Handle handle = 0;
	ASSERT_TRUE(renderer.mesh_load(handle, triangle_data()));
	renderer.mesh_unload(handle);
	//卸载后绘制应失败
	EXPECT_FALSE(renderer.mesh_draw(handle, nullptr));
	//重复卸载不报错
	renderer.mesh_unload(handle);
}

//程序化三角形入口：3 顶点 / 3 索引，可绘制并推进一帧
TEST_F(Renderer_Mesh_Test, 程序化三角形加载)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	engine::Mesh_Handle mesh = 0;
	ASSERT_TRUE(renderer.mesh_triangle_load(mesh));
	uint32_t vertices = 0;
	uint32_t indices = 0;
	ASSERT_TRUE(renderer.mesh_counts(mesh, &vertices, &indices));
	EXPECT_EQ(vertices, 3u);
	EXPECT_EQ(indices, 3u);
	ASSERT_TRUE(renderer.mesh_draw(mesh, nullptr));
	ASSERT_TRUE(renderer.frame_render());
	renderer.mesh_unload(mesh);
}

//程序化立方体入口：24 顶点 / 36 索引；非法半边长被拒绝
TEST_F(Renderer_Mesh_Test, 程序化立方体加载)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	engine::Mesh_Handle mesh = 12345;
	//非法半边长（0）整次拒绝并把句柄复位为 0
	EXPECT_FALSE(renderer.mesh_cube_load(mesh, 0.0f));
	EXPECT_EQ(mesh, 0u);
	ASSERT_TRUE(renderer.mesh_cube_load(mesh, 0.5f));
	uint32_t vertices = 0;
	uint32_t indices = 0;
	ASSERT_TRUE(renderer.mesh_counts(mesh, &vertices, &indices));
	EXPECT_EQ(vertices, 24u);
	EXPECT_EQ(indices, 36u);
	ASSERT_TRUE(renderer.mesh_draw(mesh, nullptr));
	ASSERT_TRUE(renderer.frame_render());
	renderer.mesh_unload(mesh);
}

/*
程序化正二十面体球（第一百八十九轮）
口径：细分 0 → 12 顶点 / 60 索引（20 面）；细分 1 → 42 顶点 / 240 索引（80 面）；
      包围盒应接近 ±半径；非法半径与越界细分整次拒绝。
*/
TEST_F(Renderer_Mesh_Test, 程序化正二十面体球加载)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));

	//非法参数整次拒绝
	engine::Mesh_Handle invalid = 12345;
	EXPECT_FALSE(renderer.mesh_icosphere_load(invalid, 0.0f, 0));
	EXPECT_EQ(invalid, 0u);
	invalid = 12345;
	EXPECT_FALSE(renderer.mesh_icosphere_load(invalid, 1.0f, 5));
	EXPECT_EQ(invalid, 0u);

	//细分 0：正二十面体
	engine::Mesh_Handle level0 = 0;
	ASSERT_TRUE(renderer.mesh_icosphere_load(level0, 0.8f, 0));
	uint32_t vertices = 0;
	uint32_t indices = 0;
	ASSERT_TRUE(renderer.mesh_counts(level0, &vertices, &indices));
	EXPECT_EQ(vertices, 12u);
	EXPECT_EQ(indices, 60u);

	//细分 1：每面拆四
	engine::Mesh_Handle level1 = 0;
	ASSERT_TRUE(renderer.mesh_icosphere_load(level1, 0.8f, 1));
	ASSERT_TRUE(renderer.mesh_counts(level1, &vertices, &indices));
	EXPECT_EQ(vertices, 42u);
	EXPECT_EQ(indices, 240u);

	//包围盒接近 ±半径（顶点在球面上）
	float bounds_min[3] = {};
	float bounds_max[3] = {};
	ASSERT_TRUE(renderer.mesh_bounds(level1, bounds_min, bounds_max));
	for (std::size_t axis = 0; axis < 3; ++axis)
	{
		EXPECT_GE(bounds_max[axis], 0.6f) << "第 " << axis << " 轴包围盒上界过小";
		EXPECT_LE(bounds_max[axis], 0.8001f) << "第 " << axis << " 轴包围盒上界越界";
		EXPECT_LE(bounds_min[axis], -0.6f) << "第 " << axis << " 轴包围盒下界过小";
		EXPECT_GE(bounds_min[axis], -0.8001f) << "第 " << axis << " 轴包围盒下界越界";
	}

	//可正常绘制
	ASSERT_TRUE(renderer.mesh_draw(level1, nullptr));
	ASSERT_TRUE(renderer.frame_render());

	renderer.mesh_unload(level0);
	renderer.mesh_unload(level1);
}

/*
程序化 XZ 平面细分网格（第一百九十三轮）
口径：3×2 分段 → 顶点 4×3 = 12、索引 3×2×6 = 36；包围盒 X 为 ±1.5、Z 为 ±1、Y 恒 0；
     非法参数（宽 0 / 分段 0 / 分段越界）整次拒绝；可正常绘制。
*/
TEST_F(Renderer_Mesh_Test, 程序化水平细分平面加载)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));

	//非法参数整次拒绝
	engine::Mesh_Handle invalid = 12345;
	EXPECT_FALSE(renderer.mesh_ground_grid_load(invalid, 0.0f, 2.0f, 3, 2));
	EXPECT_EQ(invalid, 0u);
	invalid = 12345;
	EXPECT_FALSE(renderer.mesh_ground_grid_load(invalid, 3.0f, 2.0f, 0, 2));
	EXPECT_EQ(invalid, 0u);
	invalid = 12345;
	EXPECT_FALSE(renderer.mesh_ground_grid_load(invalid, 3.0f, 2.0f, 513, 2));
	EXPECT_EQ(invalid, 0u);

	//合法参数：3×2 分段
	engine::Mesh_Handle grid = 0;
	ASSERT_TRUE(renderer.mesh_ground_grid_load(grid, 3.0f, 2.0f, 3, 2));
	uint32_t vertices = 0;
	uint32_t indices = 0;
	ASSERT_TRUE(renderer.mesh_counts(grid, &vertices, &indices));
	EXPECT_EQ(vertices, 12u);
	EXPECT_EQ(indices, 36u);

	//包围盒：X ±1.5、Z ±1、Y 恒 0
	float bounds_min[3] = {};
	float bounds_max[3] = {};
	ASSERT_TRUE(renderer.mesh_bounds(grid, bounds_min, bounds_max));
	EXPECT_NEAR(bounds_min[0], -1.5f, 0.0001f);
	EXPECT_NEAR(bounds_max[0], 1.5f, 0.0001f);
	EXPECT_NEAR(bounds_min[1], 0.0f, 0.0001f);
	EXPECT_NEAR(bounds_max[1], 0.0f, 0.0001f);
	EXPECT_NEAR(bounds_min[2], -1.0f, 0.0001f);
	EXPECT_NEAR(bounds_max[2], 1.0f, 0.0001f);

	//可正常绘制
	ASSERT_TRUE(renderer.mesh_draw(grid, nullptr));
	ASSERT_TRUE(renderer.frame_render());
	renderer.mesh_unload(grid);
}

/*
程序化水平细分平面边界（第二百零五轮）
口径：最小 1×1 分段 → 4 顶点 / 6 索引；负宽 / NaN 宽整次拒绝；较大分段（64×64）被接受。
*/
TEST_F(Renderer_Mesh_Test, 程序化水平细分平面边界)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));

	//最小分段：单格两三角
	engine::Mesh_Handle grid = 0;
	ASSERT_TRUE(renderer.mesh_ground_grid_load(grid, 2.0f, 2.0f, 1, 1));
	uint32_t vertices = 0;
	uint32_t indices = 0;
	ASSERT_TRUE(renderer.mesh_counts(grid, &vertices, &indices));
	EXPECT_EQ(vertices, 4u);
	EXPECT_EQ(indices, 6u);
	renderer.mesh_unload(grid);

	//负宽与 NaN 宽均整次拒绝（非数比较为假 → 拒绝）
	const float nan_width = std::numeric_limits<float>::quiet_NaN();
	engine::Mesh_Handle invalid = 12345;
	EXPECT_FALSE(renderer.mesh_ground_grid_load(invalid, -1.0f, 2.0f, 2, 2));
	EXPECT_EQ(invalid, 0u);
	invalid = 12345;
	EXPECT_FALSE(renderer.mesh_ground_grid_load(invalid, nan_width, 2.0f, 2, 2));
	EXPECT_EQ(invalid, 0u);

	//较大分段：64×64 → 65² 顶点 / 64² × 6 索引
	engine::Mesh_Handle large = 0;
	ASSERT_TRUE(renderer.mesh_ground_grid_load(large, 8.0f, 8.0f, 64, 64));
	ASSERT_TRUE(renderer.mesh_counts(large, &vertices, &indices));
	EXPECT_EQ(vertices, 65u * 65u);
	EXPECT_EQ(indices, 64u * 64u * 6u);
	renderer.mesh_unload(large);
}

//纹理测试夹具
class Renderer_Texture_Test : public ::testing::Test
{
};

//未初始化时加载纹理失败，句柄被复位为零
TEST_F(Renderer_Texture_Test, 未初始化加载纹理失败)
{
	engine::Renderer renderer;
	//4×4 白色像素（RGBA8 紧排）
	const uint8_t pixels[64] = {};
	uint64_t handle = 9999u;
	EXPECT_FALSE(renderer.texture_load(handle, pixels, 4, 4));
	EXPECT_EQ(handle, 0u);
}

//以字节流加载 4×4 纹理由成功，绑定后推进一帧
TEST_F(Renderer_Texture_Test, 加载字节流纹理成功)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	//4×4 不透明红（RGBA8 紧排）
	uint8_t pixels[64] = {};
	for (uint32_t index = 0; index < 64; index += 4)
	{
		pixels[index] = 255;
		pixels[index + 3] = 255;
	}
	uint64_t handle = 0;
	ASSERT_TRUE(renderer.texture_load(handle, pixels, 4, 4));
	EXPECT_NE(handle, 0u);
	//绑定该纹理
	renderer.texture_bind_set(handle);
	EXPECT_TRUE(renderer.frame_render());
	renderer.texture_unload(handle);
}

//程序生成棋盘格纹理，绑定后推进一帧
TEST_F(Renderer_Texture_Test, 棋盘格纹理绑定跑帧)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	uint64_t handle = 0;
	ASSERT_TRUE(renderer.texture_load_checkerboard(handle, 64, 64, 8));
	EXPECT_NE(handle, 0u);
	renderer.texture_bind_set(handle);
	EXPECT_TRUE(renderer.frame_render());
	renderer.texture_unload(handle);
}

//绑定无效句柄回落内置默认白纹理，推进一帧不报错
TEST_F(Renderer_Texture_Test, 绑定无效句柄回落默认)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	//句柄为零即回落默认白纹理
	renderer.texture_bind_set(0);
	EXPECT_TRUE(renderer.frame_render());
}

//纹理卸载后再绑定该句柄，回落默认白纹理且推进一帧不崩
TEST_F(Renderer_Texture_Test, 卸载后重绑定跑帧)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	uint64_t handle = 0;
	ASSERT_TRUE(renderer.texture_load_checkerboard(handle, 32, 32, 4));
	renderer.texture_bind_set(handle);
	ASSERT_TRUE(renderer.frame_render());
	//卸载该纹理
	renderer.texture_unload(handle);
	//再次绑定已卸载句柄（内部回落默认白纹理）
	renderer.texture_bind_set(handle);
	EXPECT_TRUE(renderer.frame_render());
	//重复卸载不报错
	renderer.texture_unload(handle);
}

//多纹理创建后逐条卸载，随后推进一帧不崩
TEST_F(Renderer_Texture_Test, 多纹理创建后逐条卸载)
{
        engine::Renderer renderer;
        ASSERT_TRUE(renderer.init(hidden_config()));
        //四条独立纹理
        uint64_t handles[4] = {};
        for (uint32_t index = 0; index < 4; ++index)
                ASSERT_TRUE(renderer.texture_load_checkerboard(handles[index], 16, 16, 4));
        //逐条卸载（销毁顺序无关）
        for (uint32_t index = 0; index < 4; ++index)
                renderer.texture_unload(handles[index]);
        EXPECT_TRUE(renderer.frame_render());
}

/*
纹理读回（第二十七轮）：texture_readback 读回第 0 级像素与输入逐字节一致
纹理为 RGBA8，读回走原样拷贝，故四个像素的字节应与输入完全相同；
同时覆盖无效句柄与容量不足两条拒绝路径。
*/
TEST_F(Renderer_Texture_Test, 纹理读回与输入一致)
{
        engine::Renderer renderer;
        ASSERT_TRUE(renderer.init(hidden_config()));
        //2×2 四色像素（RGBA8 紧排）
        const unsigned char texels[16] = {
                255, 0, 0, 255,
                0, 255, 0, 255,
                0, 0, 255, 255,
                255, 255, 0, 255,
        };
        uint64_t texture = 0;
        ASSERT_TRUE(renderer.texture_load(texture, texels, 2, 2));
        //读回第 0 级
        std::vector<uint8_t> pixels(16);
        uint32_t width = 0;
        uint32_t height = 0;
        ASSERT_TRUE(renderer.texture_readback(texture, pixels.data(), 16, &width, &height));
        EXPECT_EQ(width, 2u);
        EXPECT_EQ(height, 2u);
        for (std::size_t index = 0; index < 16; ++index)
                EXPECT_EQ(pixels[index], texels[index]) << "第 " << index << " 字节不一致";
        //容量不足 / 无效句柄 / 未初始化语义：均返回假且尺寸为 0
        uint32_t small_width = 9;
        uint32_t small_height = 9;
        EXPECT_FALSE(renderer.texture_readback(texture, pixels.data(), 15, &small_width, &small_height));
        EXPECT_EQ(small_width, 0u);
        EXPECT_EQ(small_height, 0u);
        EXPECT_FALSE(renderer.texture_readback(9999u, pixels.data(), 16, nullptr, nullptr));
        renderer.texture_unload(texture);
        //卸载后句柄失效
        EXPECT_FALSE(renderer.texture_readback(texture, pixels.data(), 16, nullptr, nullptr));
}

/*
纹理区域更新（第一百五十一轮）
口径：4×4 白色纹理，把 (1,1) 起 2×2 区域更新为纯红：
      读回整幅后该区域四像素应为红、其余仍为白；越界区域与空指针均被拒绝且不改内容。
*/
TEST_F(Renderer_Texture_Test, 纹理区域更新)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	//4×4 全白（RGBA8 紧排）
	std::vector<uint8_t> white(4 * 4 * 4, 255);
	uint64_t texture = 0;
	ASSERT_TRUE(renderer.texture_load(texture, white.data(), 4, 4));
	//2×2 纯红
	const unsigned char red[16] = {
		255, 0, 0, 255,
		255, 0, 0, 255,
		255, 0, 0, 255,
		255, 0, 0, 255,
	};
	ASSERT_TRUE(renderer.texture_update_region(texture, 1, 1, 2, 2, red));
	//整幅读回
	std::vector<uint8_t> pixels(4 * 4 * 4, 0);
	ASSERT_TRUE(renderer.texture_readback(texture, pixels.data(), static_cast<uint32_t>(pixels.size()), nullptr, nullptr));
	//区域内四像素为红
	const uint32_t inside[4][2] = { { 1, 1 }, { 2, 1 }, { 1, 2 }, { 2, 2 } };
	for (uint32_t index = 0; index < 4; ++index)
	{
		std::size_t offset = (static_cast<std::size_t>(inside[index][1]) * 4 + inside[index][0]) * 4;
		EXPECT_EQ(pixels[offset], 255) << "区域红通道未更新";
		EXPECT_EQ(pixels[offset + 1], 0) << "区域绿通道未更新";
		EXPECT_EQ(pixels[offset + 2], 0) << "区域蓝通道未更新";
	}
	//区域外仍为白（取左上角与原右下角）
	std::size_t corner = 0;
	EXPECT_EQ(pixels[corner], 255);
	EXPECT_EQ(pixels[corner + 1], 255);
	EXPECT_EQ(pixels[corner + 2], 255);
	std::size_t last = (static_cast<std::size_t>(3) * 4 + 3) * 4;
	EXPECT_EQ(pixels[last], 255);
	EXPECT_EQ(pixels[last + 1], 255);
	//越界 / 空指针被拒绝
	EXPECT_FALSE(renderer.texture_update_region(texture, 3, 3, 2, 2, red));
	EXPECT_FALSE(renderer.texture_update_region(texture, 0, 0, 1, 1, nullptr));
	renderer.texture_unload(texture);
}

/*
纹理区域读回（第二十八轮）：分块读回拼接应与整幅读回逐字节一致；越界整次拒绝
*/
TEST_F(Renderer_Texture_Test, 纹理区域读回与整幅一致)
{
        engine::Renderer renderer;
        ASSERT_TRUE(renderer.init(hidden_config()));
        const unsigned char texels[16] = {
                255, 0, 0, 255,
                0, 255, 0, 255,
                0, 0, 255, 255,
                255, 255, 0, 255,
        };
        uint64_t texture = 0;
        ASSERT_TRUE(renderer.texture_load(texture, texels, 2, 2));
        std::vector<uint8_t> whole(16);
        ASSERT_TRUE(renderer.texture_readback(texture, whole.data(), 16, nullptr, nullptr));
        //四个 1×1 子块
        std::vector<uint8_t> stitched(16, 0);
        const uint32_t origins[4][2] = { { 0, 0 }, { 1, 0 }, { 0, 1 }, { 1, 1 } };
        for (uint32_t index = 0; index < 4; ++index)
        {
                uint8_t block[4] = {};
                uint32_t width = 0;
                uint32_t height = 0;
                ASSERT_TRUE(renderer.texture_readback_region(texture, origins[index][0], origins[index][1],
                        1, 1, block, 4, &width, &height));
                EXPECT_EQ(width, 1u);
                EXPECT_EQ(height, 1u);
                for (uint32_t byte = 0; byte < 4; ++byte)
                        stitched[index * 4 + byte] = block[byte];
        }
        EXPECT_EQ(whole, stitched) << "纹理分块读回与整幅不一致";
        //越界（x=1 时宽 2）整次拒绝
        uint8_t probe[16] = {};
        uint32_t width = 5;
        uint32_t height = 6;
        EXPECT_FALSE(renderer.texture_readback_region(texture, 1, 0, 2, 1, probe, 8, &width, &height));
        EXPECT_EQ(width, 0u);
        EXPECT_EQ(height, 0u);
        renderer.texture_unload(texture);
}

/*
程序化球体与圆柱（第六十六轮）
口径：默认相机下球体（半径 1 / 24 × 12 分段）与圆柱（半径 0.5 / 高 2 / 24 分段）均可加载并同帧出帧；
      非法参数（半径 0、经度 2、高度 0、分段 2）整次拒绝且句柄归零。
*/
TEST_F(Renderer_Mesh_Test, 程序化球体与圆柱可绘制)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	engine::Mesh_Handle sphere = 0;
	engine::Mesh_Handle cylinder = 0;
	ASSERT_TRUE(renderer.mesh_sphere_load(sphere, 1.0f, 24, 12));
	ASSERT_TRUE(renderer.mesh_cylinder_load(cylinder, 0.5f, 2.0f, 24));
	EXPECT_NE(sphere, 0u);
	EXPECT_NE(cylinder, 0u);
	//两网格同帧登记并出帧
	EXPECT_TRUE(renderer.mesh_draw(sphere, nullptr));
	EXPECT_TRUE(renderer.mesh_draw(cylinder, nullptr));
	EXPECT_TRUE(renderer.frame_render());

	//非法参数整次拒绝（句柄归零）
	engine::Mesh_Handle invalid = 99;
	EXPECT_FALSE(renderer.mesh_sphere_load(invalid, 0.0f, 24, 12));
	EXPECT_EQ(invalid, 0u);
	EXPECT_FALSE(renderer.mesh_sphere_load(invalid, 1.0f, 2, 12));
	EXPECT_EQ(invalid, 0u);
	EXPECT_FALSE(renderer.mesh_cylinder_load(invalid, 0.5f, 0.0f, 24));
	EXPECT_EQ(invalid, 0u);
	EXPECT_FALSE(renderer.mesh_cylinder_load(invalid, 0.5f, 2.0f, 2));
	EXPECT_EQ(invalid, 0u);

	renderer.mesh_unload(sphere);
	renderer.mesh_unload(cylinder);
}

/*
程序化圆环（第六十七轮）
口径：圆环（环半径 1 / 管半径 0.3 / 32 × 16 分段）可加载并出帧；
      非法参数（环半径 0、环分段 2）整次拒绝且句柄归零。
*/
TEST_F(Renderer_Mesh_Test, 程序化圆环可绘制)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	engine::Mesh_Handle torus = 0;
	ASSERT_TRUE(renderer.mesh_torus_load(torus, 1.0f, 0.3f, 32, 16));
	EXPECT_NE(torus, 0u);
	EXPECT_TRUE(renderer.mesh_draw(torus, nullptr));
	EXPECT_TRUE(renderer.frame_render());

	//非法参数整次拒绝（句柄归零）
	engine::Mesh_Handle invalid = 77;
	EXPECT_FALSE(renderer.mesh_torus_load(invalid, 0.0f, 0.3f, 32, 16));
	EXPECT_EQ(invalid, 0u);
	EXPECT_FALSE(renderer.mesh_torus_load(invalid, 1.0f, 0.3f, 2, 16));
	EXPECT_EQ(invalid, 0u);

	renderer.mesh_unload(torus);
}

/*
程序化圆锥（第六十八轮）
口径：圆锥（半径 0.8 / 高 2 / 24 分段）可加载并出帧；
      非法参数（半径 0、高度 -1、分段 2）整次拒绝且句柄归零。
*/
TEST_F(Renderer_Mesh_Test, 程序化圆锥可绘制)
{
        engine::Renderer renderer;
        ASSERT_TRUE(renderer.init(hidden_config()));
        engine::Mesh_Handle cone = 0;
        ASSERT_TRUE(renderer.mesh_cone_load(cone, 0.8f, 2.0f, 24));
        EXPECT_NE(cone, 0u);
        EXPECT_TRUE(renderer.mesh_draw(cone, nullptr));
        EXPECT_TRUE(renderer.frame_render());

        //非法参数整次拒绝（句柄归零）
        engine::Mesh_Handle invalid = 88;
        EXPECT_FALSE(renderer.mesh_cone_load(invalid, 0.0f, 2.0f, 24));
        EXPECT_EQ(invalid, 0u);
        EXPECT_FALSE(renderer.mesh_cone_load(invalid, 0.8f, -1.0f, 24));
        EXPECT_EQ(invalid, 0u);
        EXPECT_FALSE(renderer.mesh_cone_load(invalid, 0.8f, 2.0f, 2));
        EXPECT_EQ(invalid, 0u);

        renderer.mesh_unload(cone);
}

/*
程序化四棱锥（第二百三十二轮）
口径：底面半边长 0.8 / 高 1.6 时顶点 16、索引 18，包围盒 Y ∈ [-0.8, 0.8]；
      可绘制出帧；零 / 负参数整次拒绝且句柄归零。
*/
TEST_F(Renderer_Mesh_Test, 程序化四棱锥可绘制)
{
        engine::Renderer renderer;
        ASSERT_TRUE(renderer.init(hidden_config()));
        engine::Mesh_Handle pyramid = 0;
        ASSERT_TRUE(renderer.mesh_pyramid_load(pyramid, 0.8f, 1.6f));
        EXPECT_NE(pyramid, 0u);

        //顶点 / 索引计数（侧面 4 × 3 ＋ 底面 4；索引 4 × 3 ＋ 6）
        uint32_t vertex_count = 0;
        uint32_t index_count = 0;
        ASSERT_TRUE(renderer.mesh_counts(pyramid, &vertex_count, &index_count));
        EXPECT_EQ(vertex_count, 16u);
        EXPECT_EQ(index_count, 18u);

        //包围盒（高 1.6 居中 → Y ∈ [-0.8, 0.8]）
        float bounds_min[3] = { 0.0f, 0.0f, 0.0f };
        float bounds_max[3] = { 0.0f, 0.0f, 0.0f };
        ASSERT_TRUE(renderer.mesh_bounds(pyramid, bounds_min, bounds_max));
        EXPECT_NEAR(bounds_min[1], -0.8f, 1e-4f);
        EXPECT_NEAR(bounds_max[1], 0.8f, 1e-4f);

        EXPECT_TRUE(renderer.mesh_draw(pyramid, nullptr));
        EXPECT_TRUE(renderer.frame_render());

        //非法参数整次拒绝（句柄归零）
        engine::Mesh_Handle invalid = 88;
        EXPECT_FALSE(renderer.mesh_pyramid_load(invalid, 0.0f, 1.6f));
        EXPECT_EQ(invalid, 0u);
        EXPECT_FALSE(renderer.mesh_pyramid_load(invalid, 0.8f, -1.0f));
        EXPECT_EQ(invalid, 0u);

        renderer.mesh_unload(pyramid);
}

/*
程序化胶囊（第六十九轮）
口径：胶囊（半径 0.5 / 中段高 1 / 24 分段 / 8 纬圈）可加载并出帧；中段高 0 退化为球亦可加载；
      非法参数（半径 0、高度 -1、分段 2、纬圈 1）整次拒绝且句柄归零。
*/
TEST_F(Renderer_Mesh_Test, 程序化胶囊可绘制)
{
        engine::Renderer renderer;
        ASSERT_TRUE(renderer.init(hidden_config()));
        engine::Mesh_Handle capsule = 0;
        ASSERT_TRUE(renderer.mesh_capsule_load(capsule, 0.5f, 1.0f, 24, 8));
        EXPECT_NE(capsule, 0u);
        EXPECT_TRUE(renderer.mesh_draw(capsule, nullptr));
        EXPECT_TRUE(renderer.frame_render());

        //中段高 0 → 退化为球，仍可加载
        engine::Mesh_Handle sphere_like = 0;
        ASSERT_TRUE(renderer.mesh_capsule_load(sphere_like, 0.5f, 0.0f, 24, 8));
        EXPECT_NE(sphere_like, 0u);

        //非法参数整次拒绝（句柄归零）
        engine::Mesh_Handle invalid = 99;
        EXPECT_FALSE(renderer.mesh_capsule_load(invalid, 0.0f, 1.0f, 24, 8));
        EXPECT_EQ(invalid, 0u);
        EXPECT_FALSE(renderer.mesh_capsule_load(invalid, 0.5f, -1.0f, 24, 8));
        EXPECT_EQ(invalid, 0u);
        EXPECT_FALSE(renderer.mesh_capsule_load(invalid, 0.5f, 1.0f, 2, 8));
        EXPECT_EQ(invalid, 0u);
        EXPECT_FALSE(renderer.mesh_capsule_load(invalid, 0.5f, 1.0f, 24, 1));
        EXPECT_EQ(invalid, 0u);

        renderer.mesh_unload(sphere_like);
        renderer.mesh_unload(capsule);
}

/*
程序化平面（第七十一轮）
口径：平面（2 × 2）可加载并出帧；宽 0 / 高 -1 整次拒绝且句柄归零。
*/
TEST_F(Renderer_Mesh_Test, 程序化平面可绘制)
{
        engine::Renderer renderer;
        ASSERT_TRUE(renderer.init(hidden_config()));
        engine::Mesh_Handle plane = 0;
        ASSERT_TRUE(renderer.mesh_plane_load(plane, 2.0f, 2.0f));
        EXPECT_NE(plane, 0u);
        EXPECT_TRUE(renderer.mesh_draw(plane, nullptr));
        EXPECT_TRUE(renderer.frame_render());

        //非法参数整次拒绝（句柄归零）
        engine::Mesh_Handle invalid = 55;
        EXPECT_FALSE(renderer.mesh_plane_load(invalid, 0.0f, 2.0f));
        EXPECT_EQ(invalid, 0u);
        EXPECT_FALSE(renderer.mesh_plane_load(invalid, 2.0f, -1.0f));
        EXPECT_EQ(invalid, 0u);

        renderer.mesh_unload(plane);
}

/*
网格包围盒查询（第七十三轮）
口径：半径 1 的球体包围盒约为 [-1,1]（三轴）；2 × 2 平面的 z 分量恒 0；
      未初始化 / 无效句柄返回假。
*/
TEST_F(Renderer_Mesh_Test, 网格包围盒查询)
{
        engine::Renderer renderer;
        float min_value[3] = {};
        float max_value[3] = {};
        //未初始化：返回假
        EXPECT_FALSE(renderer.mesh_bounds(0, min_value, max_value));
        ASSERT_TRUE(renderer.init(hidden_config()));
        engine::Mesh_Handle sphere = 0;
        ASSERT_TRUE(renderer.mesh_sphere_load(sphere, 1.0f, 16, 8));
        ASSERT_TRUE(renderer.mesh_bounds(sphere, min_value, max_value));
        for (int axis = 0; axis < 3; ++axis)
        {
                EXPECT_NEAR(min_value[axis], -1.0f, 0.01f);
                EXPECT_NEAR(max_value[axis], 1.0f, 0.01f);
        }
        //平面：z 恒 0、x 到 ±1
        engine::Mesh_Handle plane = 0;
        ASSERT_TRUE(renderer.mesh_plane_load(plane, 2.0f, 2.0f));
        ASSERT_TRUE(renderer.mesh_bounds(plane, min_value, max_value));
        EXPECT_NEAR(min_value[2], 0.0f, 0.001f);
        EXPECT_NEAR(max_value[2], 0.0f, 0.001f);
        EXPECT_NEAR(max_value[0], 1.0f, 0.001f);
        //无效句柄：返回假
        EXPECT_FALSE(renderer.mesh_bounds(9999u, min_value, max_value));

        renderer.mesh_unload(plane);
        renderer.mesh_unload(sphere);
}

//程序化圆盘（第八十三轮）：可加载出帧、包围盒为 XY 平面上半径范围内的圆盘、非法参数被拒
TEST_F(Renderer_Mesh_Test, 程序化圆盘可绘制)
{
        engine::Renderer renderer;
        engine::Mesh_Handle handle = 7;
        //未初始化：拒绝且句柄归零
        EXPECT_FALSE(renderer.mesh_disc_load(handle, 1.0f, 16));
        EXPECT_EQ(handle, 0u);
        ASSERT_TRUE(renderer.init(hidden_config()));
        ASSERT_TRUE(renderer.mesh_disc_load(handle, 1.0f, 16));
        EXPECT_NE(handle, 0u);

        //包围盒：XY 各到 ±1、z 恒 0
        float min_value[3] = {};
        float max_value[3] = {};
        ASSERT_TRUE(renderer.mesh_bounds(handle, min_value, max_value));
        EXPECT_NEAR(min_value[0], -1.0f, 0.02f);
        EXPECT_NEAR(max_value[0], 1.0f, 0.02f);
        EXPECT_NEAR(min_value[1], -1.0f, 0.02f);
        EXPECT_NEAR(max_value[1], 1.0f, 0.02f);
        EXPECT_NEAR(min_value[2], 0.0f, 0.001f);
        EXPECT_NEAR(max_value[2], 0.0f, 0.001f);

        //出帧成功（登记绘制 + 渲染）
        ASSERT_TRUE(renderer.mesh_draw(handle, nullptr));
        ASSERT_TRUE(renderer.frame_render());

        //非法参数：半径为 0 / 负、分段过少 / 过多，均整次拒绝且句柄归零
        engine::Mesh_Handle invalid = 9;
        EXPECT_FALSE(renderer.mesh_disc_load(invalid, 0.0f, 16));
        EXPECT_EQ(invalid, 0u);
        invalid = 9;
        EXPECT_FALSE(renderer.mesh_disc_load(invalid, -1.0f, 16));
        EXPECT_EQ(invalid, 0u);
        invalid = 9;
        EXPECT_FALSE(renderer.mesh_disc_load(invalid, 1.0f, 2));
        EXPECT_EQ(invalid, 0u);
        invalid = 9;
        EXPECT_FALSE(renderer.mesh_disc_load(invalid, 1.0f, 257));
        EXPECT_EQ(invalid, 0u);

        renderer.mesh_unload(handle);
}

//纹理尺寸查询（第九十轮）：任意尺寸纹理可查回同值；未初始化 / 无效 / 已卸载句柄返回假且出参归零
TEST_F(Renderer_Mesh_Test, 纹理尺寸可查询)
{
        engine::Renderer renderer;
        uint32_t width = 7;
        uint32_t height = 7;
        EXPECT_FALSE(renderer.texture_size(1, &width, &height));
        EXPECT_EQ(width, 0u);
        EXPECT_EQ(height, 0u);

        ASSERT_TRUE(renderer.init(hidden_config()));
        uint64_t texture = 0;
        ASSERT_TRUE(renderer.texture_load_checkerboard(texture, 64, 32, 8));
        uint32_t queried_width = 0;
        uint32_t queried_height = 0;
        EXPECT_TRUE(renderer.texture_size(texture, &queried_width, &queried_height));
        EXPECT_EQ(queried_width, 64u);
        EXPECT_EQ(queried_height, 32u);
        //只取高（宽指针为空）亦有效
        uint32_t only_height = 0;
        EXPECT_TRUE(renderer.texture_size(texture, nullptr, &only_height));
        EXPECT_EQ(only_height, 32u);
        //无效句柄
        EXPECT_FALSE(renderer.texture_size(0, &queried_width, &queried_height));
        EXPECT_EQ(queried_width, 0u);
        EXPECT_FALSE(renderer.texture_size(texture + 99, &queried_width, &queried_height));
        //卸载后失效
        renderer.texture_unload(texture);
        EXPECT_FALSE(renderer.texture_size(texture, &queried_width, &queried_height));
        EXPECT_EQ(queried_height, 0u);
}

//细分平面与网格计数（第九十三轮）：4×3 分段的 2×2 平面 → 20 顶点 / 72 索引；包围盒与非法参数校验
TEST_F(Renderer_Mesh_Test, 细分平面与网格计数)
{
        engine::Renderer renderer;
        uint32_t vertex_count = 7;
        uint32_t index_count = 7;
        EXPECT_FALSE(renderer.mesh_counts(1, &vertex_count, &index_count));
        EXPECT_EQ(vertex_count, 0u);
        EXPECT_EQ(index_count, 0u);

        ASSERT_TRUE(renderer.init(hidden_config()));
        engine::Mesh_Handle grid = 0;
        ASSERT_TRUE(renderer.mesh_grid_load(grid, 2.0f, 2.0f, 4, 3));
        ASSERT_TRUE(renderer.mesh_counts(grid, &vertex_count, &index_count));
        EXPECT_EQ(vertex_count, 20u);
        EXPECT_EQ(index_count, 72u);

        float min_value[3] = {};
        float max_value[3] = {};
        ASSERT_TRUE(renderer.mesh_bounds(grid, min_value, max_value));
        EXPECT_NEAR(min_value[0], -1.0f, 0.001f);
        EXPECT_NEAR(max_value[0], 1.0f, 0.001f);
        EXPECT_NEAR(min_value[1], -1.0f, 0.001f);
        EXPECT_NEAR(max_value[1], 1.0f, 0.001f);
        EXPECT_NEAR(max_value[2], 0.0f, 0.001f);

        //出帧成功
        ASSERT_TRUE(renderer.mesh_draw(grid, nullptr));
        ASSERT_TRUE(renderer.frame_render());

        //非法参数：宽为 0、列为 0 / 257、行为 0，均整次拒绝且句柄归零
        engine::Mesh_Handle invalid = 9;
        EXPECT_FALSE(renderer.mesh_grid_load(invalid, 0.0f, 1.0f, 4, 3));
        EXPECT_EQ(invalid, 0u);
        invalid = 9;
        EXPECT_FALSE(renderer.mesh_grid_load(invalid, 1.0f, 1.0f, 0, 3));
        EXPECT_EQ(invalid, 0u);
        invalid = 9;
        EXPECT_FALSE(renderer.mesh_grid_load(invalid, 1.0f, 1.0f, 257, 3));
        EXPECT_EQ(invalid, 0u);
        invalid = 9;
        EXPECT_FALSE(renderer.mesh_grid_load(invalid, 1.0f, 1.0f, 4, 0));
        EXPECT_EQ(invalid, 0u);

        //无效句柄计数失败且出参归零
        EXPECT_FALSE(renderer.mesh_counts(grid + 99, &vertex_count, &index_count));
        EXPECT_EQ(vertex_count, 0u);

        renderer.mesh_unload(grid);
}

