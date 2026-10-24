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
