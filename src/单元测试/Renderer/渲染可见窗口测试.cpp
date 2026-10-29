/*
渲染可见窗口测试（L3，默认运行）
可见窗口会弹窗，用于人工目视确认「清屏色 / 三角形 / 棋盘格纹理」这类真实呈现效果。
默认运行（2026-10-24 用户确认两个用例可稳定通过后取消默认跳过）；
如需在无人值守环境关闭，设置环境变量 BYJY_RUN_L3=0 即可跳过。
显式只跑本套件示例：EngineTests.exe --gtest_filter=Renderer_Visible_Test.*
*/
#include <gtest/gtest.h>

//获取渲染器门面与网格数据
#include "Application/Renderer/src/render/Renderer/渲染器.h"
//环境变量读取
#include <cstdlib>
#include <string>

namespace
{
	//是否允许运行可见窗口用例（默认运行，环境变量置 0 时跳过）
	bool l3_enabled(void)
	{
		//读取环境变量开关
		const char* flag = std::getenv("BYJY_RUN_L3");
		//未设置时默认运行
		if (flag == nullptr)
			return true;
		const std::string text(flag);
		//显式置 0 时跳过
		return text != "0";
	}

	//构造可见窗口配置
	engine::Renderer_Config visible_config(void)
	{
		engine::Renderer_Config config;
		config.width = 640;
		config.height = 360;
		config.hidden = false;
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

//可见窗口测试夹具
class Renderer_Visible_Test : public ::testing::Test
{
};

//可见窗口连续呈现纯色清屏（L3-01）
TEST_F(Renderer_Visible_Test, 可见窗口呈现纯色)
{
	//未开启则跳过
	if (!l3_enabled())
		GTEST_SKIP() << "BYJY_RUN_L3=0，跳过可见窗口用例";
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(visible_config())) << "可见窗口初始化失败";
	//连续呈现 120 帧供目视确认（清屏色稳定、无黑屏闪烁）
	for (uint32_t index = 0; index < 120; ++index)
	{
		EXPECT_TRUE(renderer.frame_render());
		//若窗口被关闭则提前收敛
		if (renderer.close_requested())
			break;
	}
	EXPECT_GT(renderer.frame_count(), 0u);
}

//可见窗口呈现三角形并采样棋盘格纹理（L3-02 / L3-03）
TEST_F(Renderer_Visible_Test, 可见窗口呈现三角形与棋盘格)
{
	//未开启则跳过
	if (!l3_enabled())
		GTEST_SKIP() << "BYJY_RUN_L3=0，跳过可见窗口用例";
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(visible_config()));
	//把相机拉到前方并注视原点
	renderer.camera_position_set(0.0f, 0.0f, 2.0f);
	renderer.camera_target_set(0.0f, 0.0f, 0.0f);
	//生成并绑定棋盘格纹理
	uint64_t texture = 0;
	ASSERT_TRUE(renderer.texture_load_checkerboard(texture, 64, 64, 8));
	renderer.texture_bind_set(texture);
	//加载三角形网格
	engine::Mesh_Handle mesh = 0;
	ASSERT_TRUE(renderer.mesh_load(mesh, triangle_data()));
	//连续呈现 120 帧供目视确认（三角形与棋盘格纹理）
	for (uint32_t index = 0; index < 120; ++index)
	{
		EXPECT_TRUE(renderer.mesh_draw(mesh, nullptr));
		EXPECT_TRUE(renderer.frame_render());
		//若窗口被关闭则提前收敛
		if (renderer.close_requested())
			break;
	}
	renderer.mesh_unload(mesh);
	renderer.texture_unload(texture);
}
