/*
渲染同帧后处理链测试（L2 隐藏窗口）
覆盖：未初始化守卫、无效句柄回落、单级链（count = 1）可用、
      两级链「反色 → 反色」＝ 原画面（复合恒等，与直接交换链路径画面一致）、
      两级链「反色 → 亮度阈值」＝ 中心纯黑 / 角落纯白（两级效果与预期一致）。
说明：
  1. 断言一律按「通道序无关 ＋ 容差」编写，不依赖具体表面格式；不涉及 Vulkan 细节。
  2. 「反色 → 反色」为恒等：取反两次在 sRGB / UNORM 附件下均近似还原原像素，故用「与交换链路径
     画面一致」作强断言，可证明确实按序执行了两级；若只执行一级则画面整体反色、必然不一致。
  3. 「反色 → 亮度阈值」的终点饱和到 0 / 255，与被采样附件的编码无关，故可作确定性像素级验收。
  4. Debug 构建下渲染器默认开启校验层，本路径较新，若屏障 / 描述符集有误会由校验层报出，
     用例随渲染流程一并实跑判定。
*/
#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

//获取渲染器门面（含管线登记与后处理链接口）与网格数据
#include "Application/Renderer/src/render/Renderer/渲染器.h"

namespace
{
	//窗口尺寸（小尺寸便于逐像素遍历；目标与交换链同尺寸以满足后处理 1:1 取源）
	constexpr uint32_t WINDOW_WIDTH = 64;
	constexpr uint32_t WINDOW_HEIGHT = 36;

	/*
	两级链画面与参考画面的最大允许逐通道差
	「反色 → 反色」的每一级都要经历一次「附件编码 → 采样解码」的取整，两级累积故容差取 4；
	若只执行了一级则画面整体反色（中心与角落的通道差均达 200 以上），故该容差仍具决定性。
	*/
	constexpr int CHAIN_TOLERANCE = 4;
	//「纯黑 / 纯白」判定的通道阈值（阈值化后应饱和到 0 / 255）
	constexpr int SATURATION_THRESHOLD = 250;
	//「近黑」判定的通道上限
	constexpr int DARK_THRESHOLD = 4;

	//把像素写成可读文本（失败诊断用）
	std::string pixel_text(const std::array<uint8_t, 4>& value)
	{
		return std::to_string(value[0]) + "/" + std::to_string(value[1]) + "/"
			+ std::to_string(value[2]) + "/" + std::to_string(value[3]);
	}

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
		std::size_t offset = (static_cast<std::size_t>(y) * WINDOW_WIDTH + x) * 4;
		std::array<uint8_t, 4> value{};
		for (std::size_t index = 0; index < 4; ++index)
			value[index] = pixels[offset + index];
		return value;
	}

	//构造覆盖视口中央的四边形（NDC 尺度 1，位于 z = 0；默认相机在 +Z 3 单位处，故四角留清屏色）
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

	//登记「白纹理四边形」场景（供各路径复用，保证输入一致）
	void scene_register(engine::Renderer& renderer, engine::Mesh_Handle& handle_out)
	{
		//回落内置默认白纹理
		renderer.texture_bind_set(0);
		//加载四边形并登记绘制
		ASSERT_TRUE(renderer.mesh_load(handle_out, quad_data()));
		ASSERT_TRUE(renderer.mesh_draw(handle_out, nullptr));
	}

	//登记一条后处理管线（顶点复用 后处理.vert，片元取给定文件名）
	uint64_t post_pipeline_register(engine::Renderer& renderer, const std::string& frag_shader)
	{
		uint64_t handle = 0;
		EXPECT_TRUE(renderer.pipeline_load(handle, "后处理.vert.spv", frag_shader));
		return handle;
	}

	//两帧回读画面的最大逐通道差
	int max_channel_diff(const std::vector<uint8_t>& left, const std::vector<uint8_t>& right)
	{
		int maximum = 0;
		for (std::size_t index = 0; index < left.size(); ++index)
		{
			int delta = static_cast<int>(left[index]) - static_cast<int>(right[index]);
			if (delta < 0)
				delta = -delta;
			if (delta > maximum)
				maximum = delta;
		}
		return maximum;
	}
}

//后处理链测试夹具
class Renderer_Post_Chain_Test : public ::testing::Test
{
};

//未初始化时设置后处理链与第二目标均无副作用
TEST_F(Renderer_Post_Chain_Test, 未初始化设置无副作用)
{
	engine::Renderer renderer;
	//未初始化时调用应静默返回 / 返回假（不崩溃）
	renderer.render_target_present_secondary_set(1);
	EXPECT_FALSE(renderer.post_chain_set(nullptr, 0));
	EXPECT_FALSE(renderer.valid());
}

//未登记句柄被整链拒绝，且无效呈现目标被回落
TEST_F(Renderer_Post_Chain_Test, 无效句柄回落)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	//含未登记句柄的链应被整链拒绝
	uint64_t unregistered = 12345;
	EXPECT_FALSE(renderer.post_chain_set(&unregistered, 1));
	//设定一个未登记的呈现目标（应关闭该路径）
	renderer.render_target_present_set(999);
	std::vector<uint8_t> pixels(pixel_bytes());
	uint32_t width = 0;
	uint32_t height = 0;
	//仍应走默认交换链路径并成功出帧
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size()), &width, &height));
	EXPECT_EQ(width, WINDOW_WIDTH);
	EXPECT_EQ(height, WINDOW_HEIGHT);
	EXPECT_EQ(renderer.frame_count(), 1u);
}

//单级链（count = 1，反色）生效：中心（四边形）被反色为黑、角落（清屏色）被反色为亮
TEST_F(Renderer_Post_Chain_Test, 单级链反色生效)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	//创建场景目标
	uint64_t scene_target = 0;
	ASSERT_TRUE(renderer.render_target_create(scene_target, renderer.extent_width(), renderer.extent_height()));
	//登记反色管线并设为单级链
	uint64_t invert = post_pipeline_register(renderer, "反色.frag.spv");
	ASSERT_NE(invert, 0u);
	ASSERT_TRUE(renderer.post_chain_set(&invert, 1));
	//登记场景并设定同帧后处理呈现
	engine::Mesh_Handle handle = 0;
	scene_register(renderer, handle);
	renderer.render_target_present_set(scene_target);
	std::vector<uint8_t> pixels(pixel_bytes());
	//一帧内完成（帧计数仅推进 1）
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
	EXPECT_EQ(renderer.frame_count(), 1u);
	//中心为四边形（白 → 反色后为黑）
	std::array<uint8_t, 4> center = pixel_get(pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
	EXPECT_LE(*std::max_element(center.begin(), center.begin() + 3), DARK_THRESHOLD) << "中心应为反色后的黑：" << pixel_text(center);
	EXPECT_GE(center[3], SATURATION_THRESHOLD);
	//角落为清屏色（暗 → 反色后为亮）
	std::array<uint8_t, 4> corner = pixel_get(pixels, 1, 1);
	int corner_min = *std::min_element(corner.begin(), corner.begin() + 3);
	EXPECT_GE(corner_min, 200) << "角落应为反色后的亮色：" << pixel_text(corner);
	renderer.mesh_unload(handle);
	renderer.pipeline_unload(invert);
	renderer.render_target_destroy(scene_target);
}

//两级链「反色 → 反色」＝ 原画面：与直接走交换链路径的回读画面一致
TEST_F(Renderer_Post_Chain_Test, 两级反色恒等于原画面)
{
	//参考帧：直接走交换链路径
	std::vector<uint8_t> reference(pixel_bytes());
	{
		engine::Renderer renderer;
		ASSERT_TRUE(renderer.init(hidden_config()));
		if (!renderer.readback_supported())
			GTEST_SKIP() << "本机交换链图像不具备传输源用法";
		engine::Mesh_Handle handle = 0;
		scene_register(renderer, handle);
		ASSERT_TRUE(renderer.frame_render(reference.data(), static_cast<uint32_t>(reference.size())));
		renderer.mesh_unload(handle);
	}
	//对照帧：两级「反色 → 反色」链
	std::vector<uint8_t> chained(pixel_bytes());
	{
		engine::Renderer renderer;
		ASSERT_TRUE(renderer.init(hidden_config()));
		if (!renderer.readback_supported())
			GTEST_SKIP() << "本机交换链图像不具备传输源用法";
		//创建 ping-pong 成对目标
		uint64_t scene_target = 0;
		uint64_t second_target = 0;
		ASSERT_TRUE(renderer.render_target_create(scene_target, renderer.extent_width(), renderer.extent_height()));
		ASSERT_TRUE(renderer.render_target_create(second_target, renderer.extent_width(), renderer.extent_height()));
		//登记两级反色管线（各自独立句柄，共用同一顶点着色器）
		uint64_t stages[2] = { 0, 0 };
		stages[0] = post_pipeline_register(renderer, "反色.frag.spv");
		stages[1] = post_pipeline_register(renderer, "反色.frag.spv");
		ASSERT_NE(stages[0], 0u);
		ASSERT_NE(stages[1], 0u);
		ASSERT_NE(stages[0], stages[1]);
		ASSERT_TRUE(renderer.post_chain_set(stages, 2));
		engine::Mesh_Handle handle = 0;
		scene_register(renderer, handle);
		renderer.render_target_present_set(scene_target);
		renderer.render_target_present_secondary_set(second_target);
		ASSERT_TRUE(renderer.frame_render(chained.data(), static_cast<uint32_t>(chained.size())));
		EXPECT_EQ(renderer.frame_count(), 1u);
		renderer.mesh_unload(handle);
		renderer.pipeline_unload(stages[0]);
		renderer.pipeline_unload(stages[1]);
		renderer.render_target_destroy(scene_target);
		renderer.render_target_destroy(second_target);
	}
	//两画面逐通道一致（证明确实按序执行了两级并复合为恒等）
	int difference = max_channel_diff(reference, chained);
	EXPECT_LE(difference, CHAIN_TOLERANCE) << "两级反色链画面与原画面不一致（最大通道差 " << difference << "）";
}

//两级链「反色 → 亮度阈值」：中心（四边形）为纯黑、角落（清屏色）为纯白
TEST_F(Renderer_Post_Chain_Test, 两级反色与阈值叠加)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	//创建 ping-pong 成对目标
	uint64_t scene_target = 0;
	uint64_t second_target = 0;
	ASSERT_TRUE(renderer.render_target_create(scene_target, renderer.extent_width(), renderer.extent_height()));
	ASSERT_TRUE(renderer.render_target_create(second_target, renderer.extent_width(), renderer.extent_height()));
	//登记两级管线：一级反色、二级亮度阈值（顶点复用 后处理.vert）
	uint64_t stages[2] = { 0, 0 };
	stages[0] = post_pipeline_register(renderer, "反色.frag.spv");
	stages[1] = post_pipeline_register(renderer, "亮度阈值.frag.spv");
	ASSERT_NE(stages[0], 0u);
	ASSERT_NE(stages[1], 0u);
	ASSERT_TRUE(renderer.post_chain_set(stages, 2));
	//登记场景并设定成对目标
	engine::Mesh_Handle handle = 0;
	scene_register(renderer, handle);
	renderer.render_target_present_set(scene_target);
	renderer.render_target_present_secondary_set(second_target);
	std::vector<uint8_t> pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
	EXPECT_EQ(renderer.frame_count(), 1u);
	/*
	预期：四边形为白（线性 1.0）→ 反色 0 → 亮度 0 < 0.5 → 纯黑；
	      清屏色为暗（约 0.05）→ 反色约 0.95 → 亮度 ≥ 0.5 → 纯白。
	*/
	std::array<uint8_t, 4> center = pixel_get(pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
	EXPECT_LE(*std::max_element(center.begin(), center.begin() + 3), DARK_THRESHOLD) << "中心应为纯黑：" << pixel_text(center);
	EXPECT_GE(center[3], SATURATION_THRESHOLD);
	std::array<uint8_t, 4> corner = pixel_get(pixels, 1, 1);
	EXPECT_GE(*std::min_element(corner.begin(), corner.begin() + 3), SATURATION_THRESHOLD) << "角落应为纯白：" << pixel_text(corner);
	EXPECT_GE(corner[3], SATURATION_THRESHOLD);
	renderer.mesh_unload(handle);
	renderer.pipeline_unload(stages[0]);
	renderer.pipeline_unload(stages[1]);
	renderer.render_target_destroy(scene_target);
	renderer.render_target_destroy(second_target);
}

/*
链长 1 与既有单级默认路径逐字节等价（收口后 `render_to_target_present` 为 `render_chain_present` 的薄封装）
参考帧走「不设链」的既有单级默认路径（内置 后处理.vert / 后处理.frag）；
对照帧走「链长 1 ＋ 经 pipeline_load 登记同一条 后处理 片元着色器」。
两条路径现共用同一套录制实现，故两帧画面应**逐字节相同**（除取整外无任何差异）。
*/
TEST_F(Renderer_Post_Chain_Test, 单级链与既有单级路径等价)
{
	//参考帧：既有单级默认路径（不设链）
	std::vector<uint8_t> reference(pixel_bytes());
	{
		engine::Renderer renderer;
		ASSERT_TRUE(renderer.init(hidden_config()));
		if (!renderer.readback_supported())
			GTEST_SKIP() << "本机交换链图像不具备传输源用法";
		uint64_t scene_target = 0;
		ASSERT_TRUE(renderer.render_target_create(scene_target, renderer.extent_width(), renderer.extent_height()));
		engine::Mesh_Handle handle = 0;
		scene_register(renderer, handle);
		renderer.render_target_present_set(scene_target);
		ASSERT_TRUE(renderer.frame_render(reference.data(), static_cast<uint32_t>(reference.size())));
		renderer.mesh_unload(handle);
		renderer.render_target_destroy(scene_target);
	}
	//对照帧：链长 1（同一片元着色器，经 pipeline_load 登记）
	std::vector<uint8_t> chained(pixel_bytes());
	{
		engine::Renderer renderer;
		ASSERT_TRUE(renderer.init(hidden_config()));
		if (!renderer.readback_supported())
			GTEST_SKIP() << "本机交换链图像不具备传输源用法";
		uint64_t scene_target = 0;
		ASSERT_TRUE(renderer.render_target_create(scene_target, renderer.extent_width(), renderer.extent_height()));
		uint64_t stage = post_pipeline_register(renderer, "后处理.frag.spv");
		ASSERT_NE(stage, 0u);
		ASSERT_TRUE(renderer.post_chain_set(&stage, 1));
		engine::Mesh_Handle handle = 0;
		scene_register(renderer, handle);
		renderer.render_target_present_set(scene_target);
		ASSERT_TRUE(renderer.frame_render(chained.data(), static_cast<uint32_t>(chained.size())));
		renderer.mesh_unload(handle);
		renderer.pipeline_unload(stage);
		renderer.render_target_destroy(scene_target);
	}
	//两条路径共用同一录制实现 → 画面应逐字节相同
	int difference = max_channel_diff(reference, chained);
	EXPECT_EQ(difference, 0) << "链长 1 与既有单级默认路径画面不一致（最大通道差 " << difference << "）";
}

/*
目标范围与交换链不一致时回落原路径（处置 R-FACADE-08 / R-FRAME-09）
用比交换链窄的目标作为「同帧后处理呈现」目标：门面预检应告警并回落，本帧正常出帧且画面为**未后处理**的场景。
*/
TEST_F(Renderer_Post_Chain_Test, 目标范围不一致时回落)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	//比交换链窄的目标（触发范围不一致）
	uint64_t narrow_target = 0;
	ASSERT_TRUE(renderer.render_target_create(narrow_target, renderer.extent_width() - 8, renderer.extent_height()));
	//链长 1 的反色链（若误走后处理路径则中心会被反色成黑）
	uint64_t invert = post_pipeline_register(renderer, "反色.frag.spv");
	ASSERT_NE(invert, 0u);
	ASSERT_TRUE(renderer.post_chain_set(&invert, 1));
	engine::Mesh_Handle handle = 0;
	scene_register(renderer, handle);
	renderer.render_target_present_set(narrow_target);
	std::vector<uint8_t> pixels(pixel_bytes());
	//本帧应回落原路径并正常出帧（不作为硬错误）
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
	EXPECT_EQ(renderer.frame_count(), 1u);
	//回落即未经后处理：中心仍为四边形的亮色（而非反色后的黑）
	std::array<uint8_t, 4> center = pixel_get(pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
	EXPECT_GE(*std::min_element(center.begin(), center.begin() + 3), 200)
		<< "目标范围不一致时未回落（画面被后处理过）：" << pixel_text(center);
	renderer.mesh_unload(handle);
	renderer.pipeline_unload(invert);
	renderer.render_target_destroy(narrow_target);
}

