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
  5. **换机复核点（处置 TEST-70）**：①容差 CHAIN_TOLERANCE = 4 —— 每级各经历一次「附件编码 → 采样解码」
     取整（sRGB / UNORM 附件下均如此），两级累积；本机实测最大通道差 3，若换机 / 换驱动后该值增大，
     按失败信息上调而非放宽断言语义（只执行一级时差达 200 以上，容差仍有决定性）。②「目标尺寸 = 交换链尺寸」
     是硬前提 —— 后处理按帧缓冲坐标 1:1 取源（texelFetch + gl_FragCoord），尺寸不等会采样错位并被
     帧模块范围校验拒绝，故各用例一律以 extent_width / extent_height 建目标。
*/
#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
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

	//按 Rec.709 求像素亮度
	float luma_of(const std::array<uint8_t, 4>& value)
	{
		return 0.2126f * static_cast<float>(value[0]) + 0.7152f * static_cast<float>(value[1])
			+ 0.0722f * static_cast<float>(value[2]);
	}

	//统计「本帧比参考帧暗至少 threshold」的像素数（用于验证描边压暗）
	uint32_t darkened_count(const std::vector<uint8_t>& tested, const std::vector<uint8_t>& reference, float threshold)
	{
		uint32_t count = 0;
		for (uint32_t y = 0; y < WINDOW_HEIGHT; ++y)
		{
			for (uint32_t x = 0; x < WINDOW_WIDTH; ++x)
			{
				//两帧亮度差
				float difference = luma_of(pixel_get(reference, x, y)) - luma_of(pixel_get(tested, x, y));
				if (difference >= threshold)
					++count;
			}
		}
		return count;
	}
	//统计「本帧比参考帧亮至少 threshold」的像素数（用于验证泛光扩散范围）
	uint32_t brightened_count(const std::vector<uint8_t>& tested, const std::vector<uint8_t>& reference, float threshold)
	{
		uint32_t count = 0;
		for (uint32_t y = 0; y < WINDOW_HEIGHT; ++y)
		{
			for (uint32_t x = 0; x < WINDOW_WIDTH; ++x)
			{
				//两帧亮度差
				float difference = luma_of(pixel_get(tested, x, y)) - luma_of(pixel_get(reference, x, y));
				if (difference >= threshold)
					++count;
			}
		}
		return count;
	}
}

//后处理链测试夹具
class Renderer_Post_Chain_Test : public ::testing::Test
{
};

//未初始化时设置后处理链与目标组均无副作用
TEST_F(Renderer_Post_Chain_Test, 未初始化设置无副作用)
{
	engine::Renderer renderer;
	//未初始化时调用应静默返回 / 返回假（不崩溃）
	EXPECT_FALSE(renderer.render_target_present_chain_set(nullptr, 1));
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
		uint64_t chain_targets[2] = { scene_target, second_target };
		ASSERT_TRUE(renderer.render_target_present_chain_set(chain_targets, 2));
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
	uint64_t chain_targets[2] = { scene_target, second_target };
	ASSERT_TRUE(renderer.render_target_present_chain_set(chain_targets, 2));
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
目标尺寸与交换链不一致**不再回落**（第十七轮放宽；原 R-FACADE-08 / R-FRAME-09 的 1:1 要求已解除）
用比交换链窄 8 像素的目标作为「同帧后处理呈现」目标 ＋ 单级反色链：本帧应**仍在后处理路径上**出帧，
故中心（白四边形）被反色成黑、角落（清屏色）被反色成亮。
*/
TEST_F(Renderer_Post_Chain_Test, 目标尺寸不一致仍走后处理)
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
	//本帧应在后处理路径上正常出帧（尺寸不一致不再拒绝）
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
	EXPECT_EQ(renderer.frame_count(), 1u);
	//走了后处理：中心被反色成黑、角落被反色成亮
	std::array<uint8_t, 4> center = pixel_get(pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
	EXPECT_LE(*std::max_element(center.begin(), center.begin() + 3), DARK_THRESHOLD)
		<< "中心未被反色（尺寸不一致时仍回落了原路径）：" << pixel_text(center);
	std::array<uint8_t, 4> corner = pixel_get(pixels, 1, 1);
	EXPECT_GE(*std::min_element(corner.begin(), corner.begin() + 3), 200)
		<< "角落未被反色：" << pixel_text(corner);
	renderer.mesh_unload(handle);
	renderer.pipeline_unload(invert);
	renderer.render_target_destroy(narrow_target);
}

/*
链状态只读诊断入口（post_chain_count / post_chain_get）
断言「计数与级序可读」「卸载中间级后级序压缩」「容量截断与空指针」三件事；
纯只读接口，不改变渲染行为。
*/
TEST_F(Renderer_Post_Chain_Test, 链计数与级序可读)
{
	engine::Renderer renderer;
	//未初始化时一律 0
	EXPECT_EQ(renderer.post_chain_count(), 0u);
	uint64_t probe[4] = { 0, 0, 0, 0 };
	EXPECT_EQ(renderer.post_chain_get(probe, 4), 0u);
	ASSERT_TRUE(renderer.init(hidden_config()));
	//初始化后仍未设链
	EXPECT_EQ(renderer.post_chain_count(), 0u);

	//登记三条后处理管线
	uint64_t invert = post_pipeline_register(renderer, "反色.frag.spv");
	uint64_t threshold = post_pipeline_register(renderer, "亮度阈值.frag.spv");
	uint64_t invert_again = post_pipeline_register(renderer, "反色.frag.spv");
	ASSERT_NE(invert, 0u);
	ASSERT_NE(threshold, 0u);
	ASSERT_NE(invert_again, 0u);
	ASSERT_NE(invert, threshold);
	ASSERT_NE(threshold, invert_again);

	//设三级链
	uint64_t chain[3] = { invert, threshold, invert_again };
	ASSERT_TRUE(renderer.post_chain_set(chain, 3));
	EXPECT_EQ(renderer.post_chain_count(), 3u);
	//逐级读回须与设置顺序一致
	uint64_t read_back[4] = { 0, 0, 0, 0 };
	ASSERT_EQ(renderer.post_chain_get(read_back, 4), 3u);
	EXPECT_EQ(read_back[0], invert);
	EXPECT_EQ(read_back[1], threshold);
	EXPECT_EQ(read_back[2], invert_again);

	//容量不足：按容量截断，且返回实际写入数
	uint64_t single[2] = { 0, 0 };
	ASSERT_EQ(renderer.post_chain_get(single, 1), 1u);
	EXPECT_EQ(single[0], invert);
	//出参为空：返回 0
	EXPECT_EQ(renderer.post_chain_get(nullptr, 4), 0u);

	//卸载中间一级 → 该句柄被摘除、后续级序前移（计数 3 → 2）
	renderer.pipeline_unload(threshold);
	EXPECT_EQ(renderer.post_chain_count(), 2u);
	uint64_t compressed[4] = { 0, 0, 0, 0 };
	ASSERT_EQ(renderer.post_chain_get(compressed, 4), 2u);
	EXPECT_EQ(compressed[0], invert);
	EXPECT_EQ(compressed[1], invert_again);

	//清空链 → 计数归 0
	ASSERT_TRUE(renderer.post_chain_set(nullptr, 0));
	EXPECT_EQ(renderer.post_chain_count(), 0u);
	EXPECT_EQ(renderer.post_chain_get(compressed, 4), 0u);
	renderer.pipeline_unload(invert);
	renderer.pipeline_unload(invert_again);
}

/*
后处理预设 · 灰度：三通道趋同（去色），白四边形与清屏色均被去色
*/
TEST_F(Renderer_Post_Chain_Test, 灰度预设去色生效)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	uint64_t scene_target = 0;
	ASSERT_TRUE(renderer.render_target_create(scene_target, renderer.extent_width(), renderer.extent_height()));
	uint64_t gray = post_pipeline_register(renderer, "灰度.frag.spv");
	ASSERT_NE(gray, 0u);
	ASSERT_TRUE(renderer.post_chain_set(&gray, 1));
	engine::Mesh_Handle handle = 0;
	scene_register(renderer, handle);
	renderer.render_target_present_set(scene_target);
	std::vector<uint8_t> pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));

	//中心（白四边形）与角落（清屏色）都应三通道趋同
	std::array<uint8_t, 4> center = pixel_get(pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
	std::array<uint8_t, 4> corner = pixel_get(pixels, 1, 1);
	EXPECT_LE(std::abs(static_cast<int>(center[0]) - static_cast<int>(center[1])), 2) << "中心 r/g 未趋同：" << pixel_text(center);
	EXPECT_LE(std::abs(static_cast<int>(center[1]) - static_cast<int>(center[2])), 2) << "中心 g/b 未趋同：" << pixel_text(center);
	EXPECT_LE(std::abs(static_cast<int>(corner[0]) - static_cast<int>(corner[1])), 2) << "角落 r/g 未趋同：" << pixel_text(corner);
	EXPECT_LE(std::abs(static_cast<int>(corner[1]) - static_cast<int>(corner[2])), 2) << "角落 g/b 未趋同：" << pixel_text(corner);
	//白四边形去色后仍应亮
	EXPECT_GE(center[0], 200) << "去色后中心过暗：" << pixel_text(center);
	EXPECT_GT(corner[0], 4) << "角落去色后被压成纯黑（应保留清屏色亮度）：" << pixel_text(corner);

	renderer.mesh_unload(handle);
	renderer.pipeline_unload(gray);
	renderer.render_target_destroy(scene_target);
}

/*
后处理预设 · 描边：与灰度帧对比，边界处出现明显压暗（内部差异）
口径：以「同一场景、同一目标的灰度帧」为参考，统计「比参考暗至少 40」的像素数 > 0，
      且中心（无边缘区域）不被大幅压暗 —— 避免依赖具体边界坐标。
*/
TEST_F(Renderer_Post_Chain_Test, 描边预设压暗边界)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	uint64_t scene_target = 0;
	ASSERT_TRUE(renderer.render_target_create(scene_target, renderer.extent_width(), renderer.extent_height()));
	uint64_t gray = post_pipeline_register(renderer, "灰度.frag.spv");
	uint64_t edge = post_pipeline_register(renderer, "描边.frag.spv");
	ASSERT_NE(gray, 0u);
	ASSERT_NE(edge, 0u);
	engine::Mesh_Handle handle = 0;
	engine::Mesh_Handle scene_mesh = 0;
	scene_register(renderer, scene_mesh);
	handle = scene_mesh;
	renderer.render_target_present_set(scene_target);

	//第一帧：灰度（作为参考）
	ASSERT_TRUE(renderer.post_chain_set(&gray, 1));
	std::vector<uint8_t> gray_pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(gray_pixels.data(), static_cast<uint32_t>(gray_pixels.size())));

	//第二帧：描边（同一场景重登记绘制项）
	ASSERT_TRUE(renderer.mesh_draw(handle, nullptr));
	ASSERT_TRUE(renderer.post_chain_set(&edge, 1));
	std::vector<uint8_t> edge_pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(edge_pixels.data(), static_cast<uint32_t>(edge_pixels.size())));

	//边界处应出现明显压暗像素
	uint32_t darkened = darkened_count(edge_pixels, gray_pixels, 40.0f);
	RecordProperty("edge_darkened_pixels", darkened);
	EXPECT_GT(darkened, 0u) << "描边未压暗任何像素（与灰度帧完全一致）";
	//中心（远离四边形边界）不应被明显压暗
	std::array<uint8_t, 4> gray_center = pixel_get(gray_pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
	std::array<uint8_t, 4> edge_center = pixel_get(edge_pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
	EXPECT_LE(luma_of(gray_center) - luma_of(edge_center), 20.0f)
		<< "中心被过度压暗（描边泄漏到内部）：灰 " << pixel_text(gray_center) << " / 边 " << pixel_text(edge_center);

	renderer.mesh_unload(scene_mesh);
	renderer.pipeline_unload(gray);
	renderer.pipeline_unload(edge);
	renderer.render_target_destroy(scene_target);
}

/*
后处理预设 · 色调映射：高光被压缩（白 255 → 约 230），不再顶到最大亮度
*/
TEST_F(Renderer_Post_Chain_Test, 色调映射压缩高亮)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	uint64_t scene_target = 0;
	ASSERT_TRUE(renderer.render_target_create(scene_target, renderer.extent_width(), renderer.extent_height()));
	uint64_t tone = post_pipeline_register(renderer, "色调映射.frag.spv");
	ASSERT_NE(tone, 0u);
	ASSERT_TRUE(renderer.post_chain_set(&tone, 1));
	engine::Mesh_Handle handle = 0;
	scene_register(renderer, handle);
	renderer.render_target_present_set(scene_target);
	std::vector<uint8_t> pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));

	//白四边形（输入线性 1.0）经简化 ACES 后约 0.80 → sRGB 编码约 232
	std::array<uint8_t, 4> center = pixel_get(pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
	RecordProperty("tone_mapped_center", static_cast<int>(center[0]));
	EXPECT_GE(center[0], 200) << "高光被压缩过头：" << pixel_text(center);
	EXPECT_LE(center[0], 246) << "高光未被压缩（仍接近 255）：" << pixel_text(center);
	EXPECT_LE(std::abs(static_cast<int>(center[0]) - static_cast<int>(center[1])), 2);

	renderer.mesh_unload(handle);
	renderer.pipeline_unload(tone);
	renderer.render_target_destroy(scene_target);
}

/*
后处理预设可串联：灰度 → 描边 两级链可正常出帧，且中心仍为去色后的亮色
*/
TEST_F(Renderer_Post_Chain_Test, 预设可串联)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	uint64_t scene_target = 0;
	uint64_t second_target = 0;
	ASSERT_TRUE(renderer.render_target_create(scene_target, renderer.extent_width(), renderer.extent_height()));
	ASSERT_TRUE(renderer.render_target_create(second_target, renderer.extent_width(), renderer.extent_height()));
	uint64_t gray = post_pipeline_register(renderer, "灰度.frag.spv");
	uint64_t edge = post_pipeline_register(renderer, "描边.frag.spv");
	ASSERT_NE(gray, 0u);
	ASSERT_NE(edge, 0u);
	uint64_t chain[2] = { gray, edge };
	ASSERT_TRUE(renderer.post_chain_set(chain, 2));
	engine::Mesh_Handle handle = 0;
	scene_register(renderer, handle);
	renderer.render_target_present_set(scene_target);
	uint64_t chain_targets[2] = { scene_target, second_target };
	ASSERT_TRUE(renderer.render_target_present_chain_set(chain_targets, 2));
	std::vector<uint8_t> pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
	EXPECT_EQ(renderer.frame_count(), 1u);

	//中心仍应为去色后的亮色（两级链未破坏内部）
	std::array<uint8_t, 4> center = pixel_get(pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
	EXPECT_GE(luma_of(center), 180.0f) << "两级链后中心过暗：" << pixel_text(center);
	EXPECT_LE(std::abs(static_cast<int>(center[0]) - static_cast<int>(center[2])), 4)
		<< "两级链后中心未保持去色：" << pixel_text(center);

	renderer.mesh_unload(handle);
	renderer.pipeline_unload(gray);
	renderer.pipeline_unload(edge);
	renderer.render_target_destroy(second_target);
	renderer.render_target_destroy(scene_target);
}

/*
多中间目标 · 三级链「亮部提取 → 模糊 → 灰度」（3 个目标轮转）
预期：中心（白四边形）经亮部提取保留、模糊后仍亮、灰度后三通道趋同且仍亮；
      角落（清屏色）亮度低于阈值被提零，此后保持近黑 —— 即「暗部不变」。
中心三通道趋同即证明确实执行到了第 3 级（前两级都不改三通道相等性之外的东西）。
*/
TEST_F(Renderer_Post_Chain_Test, 三级链亮部提取模糊灰度)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	//三个与交换链同尺寸的目标（满足后处理 1:1 取源）
	uint64_t scene_target = 0;
	uint64_t second_target = 0;
	uint64_t third_target = 0;
	ASSERT_TRUE(renderer.render_target_create(scene_target, renderer.extent_width(), renderer.extent_height()));
	ASSERT_TRUE(renderer.render_target_create(second_target, renderer.extent_width(), renderer.extent_height()));
	ASSERT_TRUE(renderer.render_target_create(third_target, renderer.extent_width(), renderer.extent_height()));
	//三级链：亮部提取 → 模糊 → 灰度
	uint64_t bright = post_pipeline_register(renderer, "亮部提取.frag.spv");
	uint64_t blur = post_pipeline_register(renderer, "模糊.frag.spv");
	uint64_t gray = post_pipeline_register(renderer, "灰度.frag.spv");
	ASSERT_NE(bright, 0u);
	ASSERT_NE(blur, 0u);
	ASSERT_NE(gray, 0u);
	uint64_t chain[3] = { bright, blur, gray };
	ASSERT_TRUE(renderer.post_chain_set(chain, 3));
	//目标组：场景目标 ＋ 两个额外中间目标
	uint64_t chain_targets[3] = { scene_target, second_target, third_target };
	ASSERT_TRUE(renderer.render_target_present_chain_set(chain_targets, 3));
	//登记场景
	engine::Mesh_Handle handle = 0;
	scene_register(renderer, handle);
	std::vector<uint8_t> pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
	EXPECT_EQ(renderer.frame_count(), 1u);

	//中心：亮部被保留（模糊在内部等价原值），灰度后仍亮且三通道趋同（证明确实执行到第 3 级）
	std::array<uint8_t, 4> center = pixel_get(pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
	RecordProperty("bright_center", static_cast<int>(center[0]));
	EXPECT_GE(center[0], 240) << "三级链后中心过暗：" << pixel_text(center);
	EXPECT_LE(std::abs(static_cast<int>(center[0]) - static_cast<int>(center[1])), 2) << "中心 r/g 未趋同：" << pixel_text(center);
	EXPECT_LE(std::abs(static_cast<int>(center[1]) - static_cast<int>(center[2])), 2) << "中心 g/b 未趋同：" << pixel_text(center);
	//角落：清屏色亮度低于阈值 → 亮部提取归零 → 此后恒近黑（暗部不变）
	std::array<uint8_t, 4> corner = pixel_get(pixels, 1, 1);
	EXPECT_LE(*std::max_element(corner.begin(), corner.begin() + 3), DARK_THRESHOLD) << "暗部被提亮（应保持近黑）：" << pixel_text(corner);
	EXPECT_GE(corner[3], SATURATION_THRESHOLD);

	renderer.mesh_unload(handle);
	renderer.pipeline_unload(bright);
	renderer.pipeline_unload(blur);
	renderer.pipeline_unload(gray);
	renderer.render_target_destroy(third_target);
	renderer.render_target_destroy(second_target);
	renderer.render_target_destroy(scene_target);
}

/*
多中间目标 · 二级链「亮部提取 → 模糊」必须与单级「亮部提取」画面不同
口径：模糊只改变亮部边界附近的像素，故两帧逐像素必有差异（max_channel_diff > 0）；
      同时两帧中心都亮、角落都近黑（模糊不改变整体明暗结构）。
*/
TEST_F(Renderer_Post_Chain_Test, 二级链模糊确实参与)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	//两个目标（场景目标 ＋ 模糊级宿）
	uint64_t scene_target = 0;
	uint64_t second_target = 0;
	ASSERT_TRUE(renderer.render_target_create(scene_target, renderer.extent_width(), renderer.extent_height()));
	ASSERT_TRUE(renderer.render_target_create(second_target, renderer.extent_width(), renderer.extent_height()));
	uint64_t bright = post_pipeline_register(renderer, "亮部提取.frag.spv");
	uint64_t blur = post_pipeline_register(renderer, "模糊.frag.spv");
	ASSERT_NE(bright, 0u);
	ASSERT_NE(blur, 0u);
	engine::Mesh_Handle handle = 0;
	scene_register(renderer, handle);
	renderer.render_target_present_set(scene_target);

	//第一帧：单级「亮部提取」
	ASSERT_TRUE(renderer.post_chain_set(&bright, 1));
	std::vector<uint8_t> single_pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(single_pixels.data(), static_cast<uint32_t>(single_pixels.size())));

	//第二帧：二级链「亮部提取 → 模糊」（同一场景重登记绘制项）
	ASSERT_TRUE(renderer.mesh_draw(handle, nullptr));
	uint64_t chain[2] = { bright, blur };
	ASSERT_TRUE(renderer.post_chain_set(chain, 2));
	uint64_t chain_targets[2] = { scene_target, second_target };
	ASSERT_TRUE(renderer.render_target_present_chain_set(chain_targets, 2));
	std::vector<uint8_t> blurred_pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(blurred_pixels.data(), static_cast<uint32_t>(blurred_pixels.size())));

	//模糊必须真实作用于画面（亮部边界被软化）
	int difference = max_channel_diff(single_pixels, blurred_pixels);
	RecordProperty("blur_max_diff", difference);
	EXPECT_GT(difference, 0) << "二级链与单级画面完全一致（模糊级或第二目标未参与）";
	//结构不变：中心仍亮、角落仍近黑
	std::array<uint8_t, 4> center = pixel_get(blurred_pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
	std::array<uint8_t, 4> corner = pixel_get(blurred_pixels, 1, 1);
	EXPECT_GE(center[0], 240) << "模糊后中心过暗：" << pixel_text(center);
	EXPECT_LE(*std::max_element(corner.begin(), corner.begin() + 3), DARK_THRESHOLD) << "模糊后暗部被提亮：" << pixel_text(corner);

	renderer.mesh_unload(handle);
	renderer.pipeline_unload(bright);
	renderer.pipeline_unload(blur);
	renderer.render_target_destroy(second_target);
	renderer.render_target_destroy(scene_target);
}

/*
目标组不足时回落：链长 2 但只设 1 个目标 → 本帧回落单级默认后处理路径并正常出帧
断言画面为**未经两级链**的结果（中心仍是白四边形、角落仍是暗清屏色）。
*/
TEST_F(Renderer_Post_Chain_Test, 目标组不足时回落)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	uint64_t scene_target = 0;
	ASSERT_TRUE(renderer.render_target_create(scene_target, renderer.extent_width(), renderer.extent_height()));
	//两级链「反色 → 亮度阈值」：若被误执行则中心纯黑、角落纯白
	uint64_t invert = post_pipeline_register(renderer, "反色.frag.spv");
	uint64_t threshold = post_pipeline_register(renderer, "亮度阈值.frag.spv");
	ASSERT_NE(invert, 0u);
	ASSERT_NE(threshold, 0u);
	uint64_t chain[2] = { invert, threshold };
	ASSERT_TRUE(renderer.post_chain_set(chain, 2));
	//只设一个目标（目标组不足 2 个）
	renderer.render_target_present_set(scene_target);
	engine::Mesh_Handle handle = 0;
	scene_register(renderer, handle);
	std::vector<uint8_t> pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
	EXPECT_EQ(renderer.frame_count(), 1u);

	//回落即两级链未执行：中心仍为白四边形、角落仍为暗清屏色
	std::array<uint8_t, 4> center = pixel_get(pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
	EXPECT_GE(*std::min_element(center.begin(), center.begin() + 3), 200)
		<< "目标组不足时未回落（画面被两级链处理过）：" << pixel_text(center);
	std::array<uint8_t, 4> corner = pixel_get(pixels, 1, 1);
	EXPECT_LE(*std::max_element(corner.begin(), corner.begin() + 3), 200)
		<< "目标组不足时未回落（角落被反色 / 阈值化为白）：" << pixel_text(corner);

	renderer.mesh_unload(handle);
	renderer.pipeline_unload(invert);
	renderer.pipeline_unload(threshold);
	renderer.render_target_destroy(scene_target);
}

/*
后处理双源叠加（泛光闭环）：三级链「亮部提取 → 模糊 → 泛光叠加」
口径：
  1. 场景用**中高亮灰纹理**（222 灰，线性 ≈0.72，高于亮部提取阈值），使叠加前后有明显亮度差
     （白四边形线性 1.0 会直接饱和，无法区分「叠加」与「未叠加」）；
  2. 参考帧走链长 1 的直通后处理（`后处理.frag`，即场景原样），对照帧走三级叠加链；
  3. 断言：对照帧中心**明显更亮**（≥ 245，参考帧约 222），角落与参考帧一致（暗部不变）
     —— 后者同时证明 binding 1 确实绑定的是「场景目标」而非上一级输出。
说明：双源片元只能放在**链末级**，且链长 ≤ 目标数（否则前面的级会把场景目标覆盖）。
*/
TEST_F(Renderer_Post_Chain_Test, 双源叠加形成泛光)
{
	//建立 222 灰纹理（RGBA8 紧排）
	std::vector<uint8_t> gray_pixels(16 * 16 * 4, 222);
	//参考帧：链长 1 直通
	std::vector<uint8_t> reference(pixel_bytes());
	{
		engine::Renderer renderer;
		ASSERT_TRUE(renderer.init(hidden_config()));
		if (!renderer.readback_supported())
			GTEST_SKIP() << "本机交换链图像不具备传输源用法";
		uint64_t gray_texture = 0;
		ASSERT_TRUE(renderer.texture_load(gray_texture, gray_pixels.data(), 16, 16));
		uint64_t scene_target = 0;
		ASSERT_TRUE(renderer.render_target_create(scene_target, renderer.extent_width(), renderer.extent_height()));
		uint64_t pass = post_pipeline_register(renderer, "后处理.frag.spv");
		ASSERT_NE(pass, 0u);
		ASSERT_TRUE(renderer.post_chain_set(&pass, 1));
		renderer.texture_bind_set(gray_texture);
		engine::Mesh_Handle handle = 0;
		ASSERT_TRUE(renderer.mesh_load(handle, quad_data()));
		ASSERT_TRUE(renderer.mesh_draw(handle, nullptr));
		renderer.render_target_present_set(scene_target);
		ASSERT_TRUE(renderer.frame_render(reference.data(), static_cast<uint32_t>(reference.size())));
		renderer.mesh_unload(handle);
		renderer.pipeline_unload(pass);
		renderer.render_target_destroy(scene_target);
		renderer.texture_unload(gray_texture);
	}
	//对照帧：三级叠加链（3 个目标 > 链长）
	std::vector<uint8_t> overlaid(pixel_bytes());
	{
		engine::Renderer renderer;
		ASSERT_TRUE(renderer.init(hidden_config()));
		if (!renderer.readback_supported())
			GTEST_SKIP() << "本机交换链图像不具备传输源用法";
		uint64_t gray_texture = 0;
		ASSERT_TRUE(renderer.texture_load(gray_texture, gray_pixels.data(), 16, 16));
		uint64_t scene_target = 0;
		uint64_t second_target = 0;
		uint64_t third_target = 0;
		ASSERT_TRUE(renderer.render_target_create(scene_target, renderer.extent_width(), renderer.extent_height()));
		ASSERT_TRUE(renderer.render_target_create(second_target, renderer.extent_width(), renderer.extent_height()));
		ASSERT_TRUE(renderer.render_target_create(third_target, renderer.extent_width(), renderer.extent_height()));
		uint64_t bright = post_pipeline_register(renderer, "亮部提取.frag.spv");
		uint64_t blur = post_pipeline_register(renderer, "模糊.frag.spv");
		uint64_t overlay = post_pipeline_register(renderer, "泛光叠加.frag.spv");
		ASSERT_NE(bright, 0u);
		ASSERT_NE(blur, 0u);
		ASSERT_NE(overlay, 0u);
		uint64_t chain[3] = { bright, blur, overlay };
		ASSERT_TRUE(renderer.post_chain_set(chain, 3));
		uint64_t chain_targets[3] = { scene_target, second_target, third_target };
		ASSERT_TRUE(renderer.render_target_present_chain_set(chain_targets, 3));
		renderer.texture_bind_set(gray_texture);
		engine::Mesh_Handle handle = 0;
		ASSERT_TRUE(renderer.mesh_load(handle, quad_data()));
		ASSERT_TRUE(renderer.mesh_draw(handle, nullptr));
		ASSERT_TRUE(renderer.frame_render(overlaid.data(), static_cast<uint32_t>(overlaid.size())));
		renderer.mesh_unload(handle);
		renderer.pipeline_unload(bright);
		renderer.pipeline_unload(blur);
		renderer.pipeline_unload(overlay);
		renderer.render_target_destroy(third_target);
		renderer.render_target_destroy(second_target);
		renderer.render_target_destroy(scene_target);
		renderer.texture_unload(gray_texture);
	}

	//中心：亮部被叠加提亮（参考约 222，叠加后趋饱和）
	std::array<uint8_t, 4> reference_center = pixel_get(reference, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
	std::array<uint8_t, 4> overlaid_center = pixel_get(overlaid, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
	RecordProperty("overlay_reference_center", static_cast<int>(reference_center[0]));
	RecordProperty("overlay_center", static_cast<int>(overlaid_center[0]));
	EXPECT_LE(*std::max_element(reference_center.begin(), reference_center.begin() + 3), 235)
		<< "参考帧中心过亮（用例前提不成立）：" << pixel_text(reference_center);
	EXPECT_GE(*std::min_element(overlaid_center.begin(), overlaid_center.begin() + 3), 245)
		<< "亮部未被叠加提亮（binding 1 或叠加级未生效）：" << pixel_text(overlaid_center);

	//角落：泛光为 0 → 叠加结果应等于场景原色（暗部不变；同时验证 binding 1 = 场景目标）
	std::array<uint8_t, 4> reference_corner = pixel_get(reference, 1, 1);
	std::array<uint8_t, 4> overlaid_corner = pixel_get(overlaid, 1, 1);
	for (std::size_t index = 0; index < 3; ++index)
	{
		int difference = std::abs(static_cast<int>(reference_corner[index]) - static_cast<int>(overlaid_corner[index]));
		EXPECT_LE(difference, CHAIN_TOLERANCE) << "暗部被改变（应保持场景原色）：参考 "
			<< pixel_text(reference_corner) << " / 叠加 " << pixel_text(overlaid_corner);
	}
}
/*
目标尺寸独立（第十七轮）：半尺寸中间目标的后处理链
口径：
  1. 场景目标为交换链尺寸，第二目标为**半尺寸**（宽高各 / 2），链「亮部提取 → 模糊」：
     第 0 级全尺寸 → 半尺寸（降采样），第 1 级半尺寸 → 交换链（升采样）；
  2. 断言：正常出帧；中心（白四边形）仍亮（≥ 240）、角落（清屏色）仍近黑（≤ 4）——降 / 升采样不破坏明暗结构。
  3. 本用例证明「尺寸不必等于交换链」；逐像素与全尺寸链的差异不做断言（点采样图案依硬件 / 尺寸而变）。
*/
TEST_F(Renderer_Post_Chain_Test, 半尺寸目标链可用)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	//全尺寸场景目标 ＋ 半尺寸中间目标
	uint64_t scene_target = 0;
	uint64_t half_target = 0;
	ASSERT_TRUE(renderer.render_target_create(scene_target, renderer.extent_width(), renderer.extent_height()));
	ASSERT_TRUE(renderer.render_target_create(half_target, renderer.extent_width() / 2, renderer.extent_height() / 2));
	uint64_t bright = post_pipeline_register(renderer, "亮部提取.frag.spv");
	uint64_t blur = post_pipeline_register(renderer, "模糊.frag.spv");
	ASSERT_NE(bright, 0u);
	ASSERT_NE(blur, 0u);
	uint64_t chain[2] = { bright, blur };
	ASSERT_TRUE(renderer.post_chain_set(chain, 2));
	uint64_t chain_targets[2] = { scene_target, half_target };
	ASSERT_TRUE(renderer.render_target_present_chain_set(chain_targets, 2));
	engine::Mesh_Handle handle = 0;
	scene_register(renderer, handle);
	std::vector<uint8_t> pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
	EXPECT_EQ(renderer.frame_count(), 1u);

	//降 / 升采样后结构不变
	std::array<uint8_t, 4> center = pixel_get(pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
	std::array<uint8_t, 4> corner = pixel_get(pixels, 1, 1);
	EXPECT_GE(*std::min_element(center.begin(), center.begin() + 3), 240)
		<< "半尺寸链后中心过暗（尺寸映射有误）：" << pixel_text(center);
	EXPECT_LE(*std::max_element(corner.begin(), corner.begin() + 3), DARK_THRESHOLD)
		<< "半尺寸链后暗部被提亮：" << pixel_text(corner);

	renderer.mesh_unload(handle);
	renderer.pipeline_unload(bright);
	renderer.pipeline_unload(blur);
	renderer.render_target_destroy(half_target);
	renderer.render_target_destroy(scene_target);
}

/*
降采样泛光链（第十八轮）：半尺寸双级模糊的扩散范围大于单级
口径：同一 222 灰纹理场景，分别跑「亮部提取 → 模糊 → 叠加」（单级模糊）与
     「亮部提取 → 模糊 → 模糊 → 叠加」（半尺寸双级模糊）；与无泛光参考帧相比，
     统计「亮度提升 ≥ 5」的像素数，双级必须**严格更多**（相对断言，不固定半径）。
*/
TEST_F(Renderer_Post_Chain_Test, 降采样泛光扩散更广)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	std::vector<uint8_t> gray_pixels(16 * 16 * 4, 222);
	uint64_t gray_texture = 0;
	ASSERT_TRUE(renderer.texture_load(gray_texture, gray_pixels.data(), 16, 16));
	//四个目标：t0 全尺寸、t1/t2 半尺寸、t3 全尺寸
	uint64_t target0 = 0;
	uint64_t target1 = 0;
	uint64_t target2 = 0;
	uint64_t target3 = 0;
	ASSERT_TRUE(renderer.render_target_create(target0, renderer.extent_width(), renderer.extent_height()));
	ASSERT_TRUE(renderer.render_target_create(target1, renderer.extent_width() / 2, renderer.extent_height() / 2));
	ASSERT_TRUE(renderer.render_target_create(target2, renderer.extent_width() / 2, renderer.extent_height() / 2));
	ASSERT_TRUE(renderer.render_target_create(target3, renderer.extent_width(), renderer.extent_height()));
	uint64_t pass = post_pipeline_register(renderer, "后处理.frag.spv");
	uint64_t bright = post_pipeline_register(renderer, "亮部提取.frag.spv");
	uint64_t blur = post_pipeline_register(renderer, "模糊.frag.spv");
	uint64_t overlay = post_pipeline_register(renderer, "泛光叠加.frag.spv");
	ASSERT_NE(pass, 0u);
	ASSERT_NE(bright, 0u);
	ASSERT_NE(blur, 0u);
	ASSERT_NE(overlay, 0u);
	engine::Mesh_Handle handle = 0;
	ASSERT_TRUE(renderer.mesh_load(handle, quad_data()));
	renderer.texture_bind_set(gray_texture);

	//参考帧：无泛光（直通）
	ASSERT_TRUE(renderer.post_chain_set(&pass, 1));
	uint64_t ref_targets[1] = { target0 };
	ASSERT_TRUE(renderer.render_target_present_chain_set(ref_targets, 1));
	ASSERT_TRUE(renderer.mesh_draw(handle, nullptr));
	std::vector<uint8_t> reference(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(reference.data(), static_cast<uint32_t>(reference.size())));

	//单级模糊：t0 → t1（半）→ t3（全）→ 叠加
	uint64_t single_chain[3] = { bright, blur, overlay };
	ASSERT_TRUE(renderer.post_chain_set(single_chain, 3));
	uint64_t single_targets[3] = { target0, target1, target3 };
	ASSERT_TRUE(renderer.render_target_present_chain_set(single_targets, 3));
	ASSERT_TRUE(renderer.mesh_draw(handle, nullptr));
	std::vector<uint8_t> single_pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(single_pixels.data(), static_cast<uint32_t>(single_pixels.size())));

	//双级模糊：t0 → t1（半）→ t2（半）→ t3（全）→ 叠加
	uint64_t double_chain[4] = { bright, blur, blur, overlay };
	ASSERT_TRUE(renderer.post_chain_set(double_chain, 4));
	uint64_t double_targets[4] = { target0, target1, target2, target3 };
	ASSERT_TRUE(renderer.render_target_present_chain_set(double_targets, 4));
	ASSERT_TRUE(renderer.mesh_draw(handle, nullptr));
	std::vector<uint8_t> double_pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(double_pixels.data(), static_cast<uint32_t>(double_pixels.size())));

	//扩散范围：双级严格大于单级
	uint32_t single_count = brightened_count(single_pixels, reference, 5.0f);
	uint32_t double_count = brightened_count(double_pixels, reference, 5.0f);
	RecordProperty("bloom_single_pixels", single_count);
	RecordProperty("bloom_double_pixels", double_count);
	EXPECT_GT(single_count, 0u) << "单级泛光未提亮任何像素（用例前提不成立）";
	EXPECT_GT(double_count, single_count) << "双级模糊的扩散范围未超过单级（半尺寸链可能未生效）";
	//远离四边形的角落应保持场景原色（两链均如此）
	for (const std::vector<uint8_t>* frame : { &single_pixels, &double_pixels })
	{
		std::array<uint8_t, 4> corner = pixel_get(*frame, 1, 1);
		std::array<uint8_t, 4> ref_corner = pixel_get(reference, 1, 1);
		for (std::size_t index = 0; index < 3; ++index)
			EXPECT_LE(std::abs(static_cast<int>(corner[index]) - static_cast<int>(ref_corner[index])), CHAIN_TOLERANCE)
				<< "角落被泛光污染：" << pixel_text(corner) << " / 参考 " << pixel_text(ref_corner);
	}

	renderer.mesh_unload(handle);
	renderer.pipeline_unload(pass);
	renderer.pipeline_unload(bright);
	renderer.pipeline_unload(blur);
	renderer.pipeline_unload(overlay);
	renderer.render_target_destroy(target3);
	renderer.render_target_destroy(target2);
	renderer.render_target_destroy(target1);
	renderer.render_target_destroy(target0);
	renderer.texture_unload(gray_texture);
}

/*
升采样片元（第十八轮）：半尺寸模糊层经双线性升采样后叠加
链：亮部提取（t0 → 半尺寸 t1）→ 模糊（t1 → t2）→ 升采样（t2 → 全尺寸 t3）→ 泛光叠加（t3 ＋ 场景 → 交换链）
断言：中心被提亮（≥ 245）、角落保持场景原色（≤ 容差）。
*/
TEST_F(Renderer_Post_Chain_Test, 升采样链可用)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	std::vector<uint8_t> gray_pixels(16 * 16 * 4, 222);
	uint64_t gray_texture = 0;
	ASSERT_TRUE(renderer.texture_load(gray_texture, gray_pixels.data(), 16, 16));
	uint64_t target0 = 0;
	uint64_t target1 = 0;
	uint64_t target2 = 0;
	uint64_t target3 = 0;
	ASSERT_TRUE(renderer.render_target_create(target0, renderer.extent_width(), renderer.extent_height()));
	ASSERT_TRUE(renderer.render_target_create(target1, renderer.extent_width() / 2, renderer.extent_height() / 2));
	ASSERT_TRUE(renderer.render_target_create(target2, renderer.extent_width() / 2, renderer.extent_height() / 2));
	ASSERT_TRUE(renderer.render_target_create(target3, renderer.extent_width(), renderer.extent_height()));
	uint64_t bright = post_pipeline_register(renderer, "亮部提取.frag.spv");
	uint64_t blur = post_pipeline_register(renderer, "模糊.frag.spv");
	uint64_t upsample = post_pipeline_register(renderer, "升采样.frag.spv");
	uint64_t overlay = post_pipeline_register(renderer, "泛光叠加.frag.spv");
	ASSERT_NE(bright, 0u);
	ASSERT_NE(blur, 0u);
	ASSERT_NE(upsample, 0u);
	ASSERT_NE(overlay, 0u);
	uint64_t chain[4] = { bright, blur, upsample, overlay };
	ASSERT_TRUE(renderer.post_chain_set(chain, 4));
	uint64_t chain_targets[4] = { target0, target1, target2, target3 };
	ASSERT_TRUE(renderer.render_target_present_chain_set(chain_targets, 4));
	engine::Mesh_Handle handle = 0;
	ASSERT_TRUE(renderer.mesh_load(handle, quad_data()));
	ASSERT_TRUE(renderer.mesh_draw(handle, nullptr));
	renderer.texture_bind_set(gray_texture);
	std::vector<uint8_t> pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));

	//中心被提亮、角落接近清屏色（泛光为 0，叠加结果即场景原色）
	std::array<uint8_t, 4> center = pixel_get(pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
	EXPECT_GE(*std::min_element(center.begin(), center.begin() + 3), 245)
		<< "升采样链后中心未被提亮：" << pixel_text(center);
	std::array<uint8_t, 4> corner = pixel_get(pixels, 1, 1);
	EXPECT_LE(*std::max_element(corner.begin(), corner.begin() + 3), 120)
		<< "升采样链后角落异常发亮：" << pixel_text(corner);

	renderer.mesh_unload(handle);
	renderer.pipeline_unload(bright);
	renderer.pipeline_unload(blur);
	renderer.pipeline_unload(upsample);
	renderer.pipeline_unload(overlay);
	renderer.render_target_destroy(target3);
	renderer.render_target_destroy(target2);
	renderer.render_target_destroy(target1);
	renderer.render_target_destroy(target0);
	renderer.texture_unload(gray_texture);
}

/*
多级降采样（第十九轮）：全 → 半 → 四分之一 → 半 → 全 的 4 级链
口径：t0 全（场景）、t1 半、t2 四分之一、t3 半；链「亮部 → 模糊 → 模糊 → 叠加」：
     第 0 级 t0 → t1（半）、第 1 级 t1 → t2（四分之一）、第 2 级 t2 → t3（升采样到半）、
     第 3 级（末级）t3 ＋ 场景 → 交换链（升采样到全）。
     断言：正常出帧；与「单级半尺寸模糊」链相比，提亮像素数**严格更多**（多级扩散更广）；
     角落不被污染（泛光为 0 处等于场景原色）。
*/
TEST_F(Renderer_Post_Chain_Test, 多级降采样链扩散更广)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	std::vector<uint8_t> gray_pixels(16 * 16 * 4, 222);
	uint64_t gray_texture = 0;
	ASSERT_TRUE(renderer.texture_load(gray_texture, gray_pixels.data(), 16, 16));
	//四个目标：全 / 半 / 四分之一 / 半
	uint64_t target0 = 0;
	uint64_t target1 = 0;
	uint64_t target2 = 0;
	uint64_t target3 = 0;
	ASSERT_TRUE(renderer.render_target_create(target0, renderer.extent_width(), renderer.extent_height()));
	ASSERT_TRUE(renderer.render_target_create(target1, renderer.extent_width() / 2, renderer.extent_height() / 2));
	ASSERT_TRUE(renderer.render_target_create(target2, renderer.extent_width() / 4, renderer.extent_height() / 4));
	ASSERT_TRUE(renderer.render_target_create(target3, renderer.extent_width() / 2, renderer.extent_height() / 2));
	uint64_t pass = post_pipeline_register(renderer, "后处理.frag.spv");
	uint64_t bright = post_pipeline_register(renderer, "亮部提取.frag.spv");
	uint64_t blur = post_pipeline_register(renderer, "模糊.frag.spv");
	uint64_t overlay = post_pipeline_register(renderer, "泛光叠加.frag.spv");
	ASSERT_NE(pass, 0u);
	ASSERT_NE(bright, 0u);
	ASSERT_NE(blur, 0u);
	ASSERT_NE(overlay, 0u);
	engine::Mesh_Handle handle = 0;
	ASSERT_TRUE(renderer.mesh_load(handle, quad_data()));
	renderer.texture_bind_set(gray_texture);

	//参考帧：无泛光
	ASSERT_TRUE(renderer.post_chain_set(&pass, 1));
	uint64_t ref_targets[1] = { target0 };
	ASSERT_TRUE(renderer.render_target_present_chain_set(ref_targets, 1));
	ASSERT_TRUE(renderer.mesh_draw(handle, nullptr));
	std::vector<uint8_t> reference(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(reference.data(), static_cast<uint32_t>(reference.size())));

	//单级半尺寸模糊（同第 18 轮：t0 → t1（半）→ t3（半）→ 叠加）
	uint64_t single_chain[3] = { bright, blur, overlay };
	ASSERT_TRUE(renderer.post_chain_set(single_chain, 3));
	uint64_t single_targets[3] = { target0, target1, target3 };
	ASSERT_TRUE(renderer.render_target_present_chain_set(single_targets, 3));
	ASSERT_TRUE(renderer.mesh_draw(handle, nullptr));
	std::vector<uint8_t> single_pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(single_pixels.data(), static_cast<uint32_t>(single_pixels.size())));

	//多级：t0 → t1（半）→ t2（四分之一）→ t3（半）→ 交换链
	uint64_t multi_chain[4] = { bright, blur, blur, overlay };
	ASSERT_TRUE(renderer.post_chain_set(multi_chain, 4));
	uint64_t multi_targets[4] = { target0, target1, target2, target3 };
	ASSERT_TRUE(renderer.render_target_present_chain_set(multi_targets, 4));
	ASSERT_TRUE(renderer.mesh_draw(handle, nullptr));
	std::vector<uint8_t> multi_pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(multi_pixels.data(), static_cast<uint32_t>(multi_pixels.size())));

	uint32_t single_count = brightened_count(single_pixels, reference, 5.0f);
	uint32_t multi_count = brightened_count(multi_pixels, reference, 5.0f);
	RecordProperty("multi_single_pixels", single_count);
	RecordProperty("multi_level_pixels", multi_count);
	EXPECT_GT(multi_count, single_count) << "多级降采样的扩散范围未超过单级";

	//角落不被污染
	std::array<uint8_t, 4> corner = pixel_get(multi_pixels, 1, 1);
	std::array<uint8_t, 4> ref_corner = pixel_get(reference, 1, 1);
	for (std::size_t index = 0; index < 3; ++index)
		EXPECT_LE(std::abs(static_cast<int>(corner[index]) - static_cast<int>(ref_corner[index])), CHAIN_TOLERANCE)
			<< "角落被泛光污染：" << pixel_text(corner) << " / 参考 " << pixel_text(ref_corner);

	renderer.mesh_unload(handle);
	renderer.pipeline_unload(pass);
	renderer.pipeline_unload(bright);
	renderer.pipeline_unload(blur);
	renderer.pipeline_unload(overlay);
	renderer.render_target_destroy(target3);
	renderer.render_target_destroy(target2);
	renderer.render_target_destroy(target1);
	renderer.render_target_destroy(target0);
	renderer.texture_unload(gray_texture);
}

/*
后处理预设 · 对比度：暗部被压得更暗、亮部保持饱和（端点方向断言）
参考帧走默认交换链路径；对照帧走单级「对比度」链。
刻意只断言方向（角落明显变暗、中心仍饱和），不依赖具体曲线数值。
*/
TEST_F(Renderer_Post_Chain_Test, 对比度放大明暗差)
{
	//参考帧：默认交换链路径
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
	//对照帧：单级对比度链
	std::vector<uint8_t> adjusted(pixel_bytes());
	{
		engine::Renderer renderer;
		ASSERT_TRUE(renderer.init(hidden_config()));
		if (!renderer.readback_supported())
			GTEST_SKIP() << "本机交换链图像不具备传输源用法";
		uint64_t scene_target = 0;
		ASSERT_TRUE(renderer.render_target_create(scene_target, renderer.extent_width(), renderer.extent_height()));
		uint64_t contrast = post_pipeline_register(renderer, "对比度.frag.spv");
		ASSERT_NE(contrast, 0u);
		ASSERT_TRUE(renderer.post_chain_set(&contrast, 1));
		engine::Mesh_Handle handle = 0;
		scene_register(renderer, handle);
		renderer.render_target_present_set(scene_target);
		ASSERT_TRUE(renderer.frame_render(adjusted.data(), static_cast<uint32_t>(adjusted.size())));
		renderer.mesh_unload(handle);
		renderer.pipeline_unload(contrast);
		renderer.render_target_destroy(scene_target);
	}

	//暗部（清屏色）：参考帧约 60、对照帧应被压到近黑
	std::array<uint8_t, 4> reference_corner = pixel_get(reference, 1, 1);
	std::array<uint8_t, 4> adjusted_corner = pixel_get(adjusted, 1, 1);
	RecordProperty("contrast_corner_reference", static_cast<int>(reference_corner[0]));
	RecordProperty("contrast_corner_adjusted", static_cast<int>(adjusted_corner[0]));
	EXPECT_GE(reference_corner[0], 30) << "参考帧角落过暗（用例前提不成立）：" << pixel_text(reference_corner);
	EXPECT_LE(*std::max_element(adjusted_corner.begin(), adjusted_corner.begin() + 3), 8)
		<< "暗部未被对比度压暗：" << pixel_text(adjusted_corner);
	//亮部（白四边形）：仍保持饱和
	std::array<uint8_t, 4> adjusted_center = pixel_get(adjusted, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
	EXPECT_GE(*std::min_element(adjusted_center.begin(), adjusted_center.begin() + 3), 250)
		<< "亮部未保持饱和：" << pixel_text(adjusted_center);
}

/*
后处理预设 · 饱和度：色偏区域通道差被放大（清屏色 b > r），纯白保持中性
参考帧走默认交换链路径；对照帧走单级「饱和度」链。
*/
TEST_F(Renderer_Post_Chain_Test, 饱和度放大通道差)
{
	//参考帧：默认交换链路径
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
	//对照帧：单级饱和度链
	std::vector<uint8_t> adjusted(pixel_bytes());
	{
		engine::Renderer renderer;
		ASSERT_TRUE(renderer.init(hidden_config()));
		if (!renderer.readback_supported())
			GTEST_SKIP() << "本机交换链图像不具备传输源用法";
		uint64_t scene_target = 0;
		ASSERT_TRUE(renderer.render_target_create(scene_target, renderer.extent_width(), renderer.extent_height()));
		uint64_t saturation = post_pipeline_register(renderer, "饱和度.frag.spv");
		ASSERT_NE(saturation, 0u);
		ASSERT_TRUE(renderer.post_chain_set(&saturation, 1));
		engine::Mesh_Handle handle = 0;
		scene_register(renderer, handle);
		renderer.render_target_present_set(scene_target);
		ASSERT_TRUE(renderer.frame_render(adjusted.data(), static_cast<uint32_t>(adjusted.size())));
		renderer.mesh_unload(handle);
		renderer.pipeline_unload(saturation);
		renderer.render_target_destroy(scene_target);
	}

	//清屏色（含蓝偏，通道顺序随表面格式而变）的通道差绝对值应被放大
	std::array<uint8_t, 4> reference_corner = pixel_get(reference, 1, 1);
	std::array<uint8_t, 4> adjusted_corner = pixel_get(adjusted, 1, 1);
	int reference_spread = std::abs(static_cast<int>(reference_corner[2]) - static_cast<int>(reference_corner[0]));
	int adjusted_spread = std::abs(static_cast<int>(adjusted_corner[2]) - static_cast<int>(adjusted_corner[0]));
	RecordProperty("saturation_spread_reference", reference_spread);
	RecordProperty("saturation_spread_adjusted", adjusted_spread);
	EXPECT_GE(reference_spread, 5) << "参考帧角落无明显色偏（用例前提不成立）：" << pixel_text(reference_corner);
	EXPECT_GE(adjusted_spread, reference_spread + 5) << "色偏未放大：" << pixel_text(adjusted_corner);
	//白四边形应保持中性（三通道差 ≤ 2）
	std::array<uint8_t, 4> adjusted_center = pixel_get(adjusted, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
	EXPECT_LE(std::abs(static_cast<int>(adjusted_center[0]) - static_cast<int>(adjusted_center[2])), 2)
		<< "纯白被着色：" << pixel_text(adjusted_center);
}

