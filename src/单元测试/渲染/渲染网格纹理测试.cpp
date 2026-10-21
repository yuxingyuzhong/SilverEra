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
