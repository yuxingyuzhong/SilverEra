/*
渲染离屏目标测试（L2 隐藏窗口 + 像素回读）
覆盖：渲染目标的创建 / 销毁；把场景渲染到离屏目标并回读目标内容；
      以目标作纹理采样做「离屏 → 采样合成」闭环；无效目标句柄回落交换链路径。
断言一律「格式无关」：绿判定只依赖绿通道（RGBA 与 BGRA 下恒为第 2 字节，下标 1），
      R / B 通道序随表面格式互换，故用「外通道取较大者」判定。
纯绿（0 与 1 两种端点）在 UNORM 与 sRGB 附件下编码一致，故判定不随附件传输函数变化。
*/
#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

//获取渲染器门面（含混合档枚举）与网格数据
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
	构造「铺满全屏」模型矩阵（列主序）
	x / y 各放大 4 倍，使四边形在 x / y 上越过 NDC 边界被裁剪，恰好铺满整个渲染目标；
	放大倍数足够大，故铺满效果不依赖相机视场角的具体取值。
	*/
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

//渲染离屏目标测试夹具
class Renderer_Offscreen_Test : public ::testing::Test
{
};

//未初始化时创建渲染目标失败
TEST_F(Renderer_Offscreen_Test, 未初始化创建目标失败)
{
	engine::Renderer renderer;
	//未 init 直接创建
	uint64_t handle = 7;
	EXPECT_FALSE(renderer.render_target_create(handle, 32, 32));
	//句柄须被复位为 0（无效）
	EXPECT_EQ(handle, 0u);
}

//渲染目标的创建与销毁幂等
TEST_F(Renderer_Offscreen_Test, 创建销毁目标幂等)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	//首次创建
	uint64_t first = 0;
	ASSERT_TRUE(renderer.render_target_create(first, 32, 32));
	EXPECT_GE(first, 1u);
	//销毁后再销毁（幂等）不崩溃
	renderer.render_target_destroy(first);
	renderer.render_target_destroy(first);
	//槽可复用：再次创建成功
	uint64_t second = 0;
	ASSERT_TRUE(renderer.render_target_create(second, 32, 32));
	EXPECT_GE(second, 1u);
	renderer.render_target_destroy(second);
}

//把场景渲染到离屏目标并回读目标内容
TEST_F(Renderer_Offscreen_Test, 渲染到目标并回读)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	//若本机不支持像素回读则跳过
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	//创建与窗口同尺寸的渲染目标
	uint64_t target = 0;
	ASSERT_TRUE(renderer.render_target_create(target, WINDOW_WIDTH, WINDOW_HEIGHT));
	//加载纯绿纹理与四边形
	uint64_t texture = 0;
	std::vector<uint8_t> texels = solid_green(2, 2);
	ASSERT_TRUE(renderer.texture_load(texture, texels.data(), 2, 2));
	engine::Mesh_Handle handle = 0;
	ASSERT_TRUE(renderer.mesh_load(handle, quad_data()));

	//本帧渲染到目标
	renderer.render_target_set(target);
	renderer.texture_bind_set(texture);
	std::array<float, 16> full = full_screen_matrix();
	ASSERT_TRUE(renderer.mesh_draw(handle, full.data()));
	//回读来源为目标颜色附件
	std::vector<uint8_t> pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
	//目标内容应为纯绿（铺满全屏）——证明离屏渲染确实发生
	EXPECT_TRUE(pixel_is_green(pixel_get(pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2)))
		<< "离屏目标中心非纯绿：" << pixel_text(pixel_get(pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2));
	EXPECT_TRUE(pixel_is_green(pixel_get(pixels, 2, 2)))
		<< "离屏目标左上角非纯绿：" << pixel_text(pixel_get(pixels, 2, 2));
	EXPECT_TRUE(pixel_is_green(pixel_get(pixels, WINDOW_WIDTH - 3, WINDOW_HEIGHT - 3)))
		<< "离屏目标右下角非纯绿：" << pixel_text(pixel_get(pixels, WINDOW_WIDTH - 3, WINDOW_HEIGHT - 3));

	//渲染到目标不改变交换链路径的可用性：切回交换链再渲染一帧应正常
	renderer.render_target_set(0);
	ASSERT_TRUE(renderer.mesh_draw(handle, full.data()));
	std::vector<uint8_t> swapchain_pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(swapchain_pixels.data(), static_cast<uint32_t>(swapchain_pixels.size())));
	EXPECT_TRUE(pixel_is_green(pixel_get(swapchain_pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2)))
		<< "切回交换链路径后中心非纯绿";

	renderer.mesh_unload(handle);
	renderer.texture_unload(texture);
	renderer.render_target_destroy(target);
}

//以目标作纹理采样：完成「离屏渲染 → 采样合成」闭环
TEST_F(Renderer_Offscreen_Test, 目标可作纹理采样闭环)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	//创建渲染目标
	uint64_t target = 0;
	ASSERT_TRUE(renderer.render_target_create(target, WINDOW_WIDTH, WINDOW_HEIGHT));
	//加载纯绿纹理与四边形
	uint64_t texture = 0;
	std::vector<uint8_t> texels = solid_green(2, 2);
	ASSERT_TRUE(renderer.texture_load(texture, texels.data(), 2, 2));
	engine::Mesh_Handle handle = 0;
	ASSERT_TRUE(renderer.mesh_load(handle, quad_data()));
	std::array<float, 16> full = full_screen_matrix();

	//第一帧：把纯绿场景渲染进离屏目标（不呈现、不回读）
	renderer.render_target_set(target);
	renderer.texture_bind_set(texture);
	ASSERT_TRUE(renderer.mesh_draw(handle, full.data()));
	ASSERT_TRUE(renderer.frame_render());

	/*
	第二帧：切回交换链并把目标颜色绑定为当前纹理
	先显式绑定内置默认白纹理，使「采样失败」会呈现为白（而非恰好同为绿），
	故本用例能真正区分「采样到目标」与「回落默认纹理」。
	*/
	renderer.render_target_set(0);
	renderer.texture_bind_set(0);
	renderer.texture_bind_target(target);
	ASSERT_TRUE(renderer.mesh_draw(handle, full.data()));
	std::vector<uint8_t> pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
	//交换链内容应为目标的绿（若采样失败则回落为默认白，绿通道判定会失败）
	std::array<uint8_t, 4> center = pixel_get(pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
	EXPECT_TRUE(pixel_is_green(center)) << "采样合成后中心非目标绿（疑似回落默认纹理）：" << pixel_text(center);
	//角落同样为目标的绿（全屏铺满）
	EXPECT_TRUE(pixel_is_green(pixel_get(pixels, 2, 2))) << "采样合成后角落非目标绿";

	renderer.mesh_unload(handle);
	renderer.texture_unload(texture);
	renderer.render_target_destroy(target);
}

//无效渲染目标句柄回落交换链路径
TEST_F(Renderer_Offscreen_Test, 无效目标句柄回落交换链)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	//设定一个从未创建的渲染目标句柄
	renderer.render_target_set(9999u);
	//加载纯绿纹理与四边形
	uint64_t texture = 0;
	std::vector<uint8_t> texels = solid_green(2, 2);
	ASSERT_TRUE(renderer.texture_load(texture, texels.data(), 2, 2));
	renderer.texture_bind_set(texture);
	engine::Mesh_Handle handle = 0;
	ASSERT_TRUE(renderer.mesh_load(handle, quad_data()));
	std::array<float, 16> full = full_screen_matrix();
	ASSERT_TRUE(renderer.mesh_draw(handle, full.data()));
	//应回落交换链路径：回读到交换链的绿
	std::vector<uint8_t> pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
	EXPECT_TRUE(pixel_is_green(pixel_get(pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2)))
		<< "无效目标句柄未回落交换链路径";

	renderer.mesh_unload(handle);
	renderer.texture_unload(texture);
}

/*
离屏目标读回（第二十六轮）：render_target_readback 直接从目标颜色图像取像素
口径：先把绿场景渲染进目标（frame_render 不回读），再读回目标内容；
      应得到与目标尺寸一致的绿；连续两次读回结果一致（读回不破坏内容与布局）。
*/
TEST_F(Renderer_Offscreen_Test, 读回离屏目标内容)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	//创建与窗口同尺寸的目标
	uint64_t target = 0;
	ASSERT_TRUE(renderer.render_target_create(target, WINDOW_WIDTH, WINDOW_HEIGHT));
	//绿纹理与全屏四边形
	uint64_t texture = 0;
	std::vector<uint8_t> texels = solid_green(2, 2);
	ASSERT_TRUE(renderer.texture_load(texture, texels.data(), 2, 2));
	engine::Mesh_Handle handle = 0;
	ASSERT_TRUE(renderer.mesh_load(handle, quad_data()));
	std::array<float, 16> full = full_screen_matrix();
	renderer.render_target_set(target);
	renderer.texture_bind_set(texture);
	ASSERT_TRUE(renderer.mesh_draw(handle, full.data()));
	ASSERT_TRUE(renderer.frame_render());

	//读回目标内容
	std::vector<uint8_t> pixels(pixel_bytes());
	uint32_t width = 0;
	uint32_t height = 0;
	ASSERT_TRUE(renderer.render_target_readback(target, pixels.data(),
		static_cast<uint32_t>(pixels.size()), &width, &height));
	EXPECT_EQ(width, WINDOW_WIDTH);
	EXPECT_EQ(height, WINDOW_HEIGHT);
	std::array<uint8_t, 4> center = pixel_get(pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
	EXPECT_TRUE(pixel_is_green(center)) << "目标读回中心非纯绿：" << pixel_text(center);
	EXPECT_TRUE(pixel_is_green(pixel_get(pixels, 2, 2))) << "目标读回角落非纯绿";

	//第二次读回：内容不因读回而改变
	std::vector<uint8_t> again(pixel_bytes());
	ASSERT_TRUE(renderer.render_target_readback(target, again.data(), static_cast<uint32_t>(again.size()), nullptr, nullptr));
	EXPECT_EQ(pixels, again);

	renderer.mesh_unload(handle);
	renderer.texture_unload(texture);
	renderer.render_target_destroy(target);
}

/*
离屏目标子区域读回（第二十七轮）：分块读回拼接应与整幅读回逐字节一致
口径：绿场景 → 渲染进 64×36 目标 → 整幅读回一次；再按「左上 / 右上 / 左下 / 右下」四块读回并拼接；
      逐字节比较两者；越界区域应整次拒绝（返回假、尺寸出参保持 0）。
*/
TEST_F(Renderer_Offscreen_Test, 子区域读回与整幅一致)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	uint64_t target = 0;
	ASSERT_TRUE(renderer.render_target_create(target, WINDOW_WIDTH, WINDOW_HEIGHT));
	uint64_t texture = 0;
	std::vector<uint8_t> texels = solid_green(2, 2);
	ASSERT_TRUE(renderer.texture_load(texture, texels.data(), 2, 2));
	engine::Mesh_Handle handle = 0;
	ASSERT_TRUE(renderer.mesh_load(handle, quad_data()));
	std::array<float, 16> full = full_screen_matrix();
	renderer.render_target_set(target);
	renderer.texture_bind_set(texture);
	ASSERT_TRUE(renderer.mesh_draw(handle, full.data()));
	ASSERT_TRUE(renderer.frame_render());

	//整幅读回（基准）
	std::vector<uint8_t> whole(pixel_bytes());
	ASSERT_TRUE(renderer.render_target_readback(target, whole.data(), static_cast<uint32_t>(whole.size()), nullptr, nullptr));

	//四块读回并拼接
	const uint32_t half_width = WINDOW_WIDTH / 2;
	const uint32_t half_height = WINDOW_HEIGHT / 2;
	std::vector<uint8_t> stitched(pixel_bytes(), 0);
	const uint32_t origins[4][2] = { { 0, 0 }, { half_width, 0 }, { 0, half_height }, { half_width, half_height } };
	for (const auto& origin : origins)
	{
		std::vector<uint8_t> block(static_cast<std::size_t>(half_width) * half_height * 4);
		uint32_t block_width = 0;
		uint32_t block_height = 0;
		ASSERT_TRUE(renderer.render_target_readback_region(target, origin[0], origin[1],
			half_width, half_height, block.data(), static_cast<uint32_t>(block.size()),
			&block_width, &block_height));
		EXPECT_EQ(block_width, half_width);
		EXPECT_EQ(block_height, half_height);
		//逐行拷入拼接缓冲
		for (uint32_t row = 0; row < half_height; ++row)
		{
			std::size_t dst = (static_cast<std::size_t>(origin[1] + row) * WINDOW_WIDTH + origin[0]) * 4;
			std::size_t src = static_cast<std::size_t>(row) * half_width * 4;
			for (uint32_t byte = 0; byte < half_width * 4; ++byte)
				stitched[dst + byte] = block[src + byte];
		}
	}
	EXPECT_EQ(whole, stitched) << "分块读回拼接与整幅读回不一致";

	//越界区域整次拒绝
	std::vector<uint8_t> probe(static_cast<std::size_t>(half_width) * half_height * 4);
	uint32_t width = 77;
	uint32_t height = 88;
	EXPECT_FALSE(renderer.render_target_readback_region(target, half_width + 1, 0,
		half_width, half_height, probe.data(), static_cast<uint32_t>(probe.size()), &width, &height));
	EXPECT_EQ(width, 0u);
	EXPECT_EQ(height, 0u);

	renderer.mesh_unload(handle);
	renderer.texture_unload(texture);
	renderer.render_target_destroy(target);
}

/*
离屏目标读回的参数校验（第二十六轮）：未初始化 / 句柄无效 / 容量不足均返回假
*/
TEST_F(Renderer_Offscreen_Test, 读回参数校验)
{
	engine::Renderer renderer;
	std::vector<uint8_t> pixels(64 * 36 * 4);
	//未初始化
	EXPECT_FALSE(renderer.render_target_readback(1, pixels.data(), static_cast<uint32_t>(pixels.size()), nullptr, nullptr));
	ASSERT_TRUE(renderer.init(hidden_config()));
	//句柄无效（从未创建）
	EXPECT_FALSE(renderer.render_target_readback(9999u, pixels.data(), static_cast<uint32_t>(pixels.size()), nullptr, nullptr));
	//容量不足
	uint64_t target = 0;
	ASSERT_TRUE(renderer.render_target_create(target, WINDOW_WIDTH, WINDOW_HEIGHT));
	uint32_t width = 123;
	uint32_t height = 456;
	EXPECT_FALSE(renderer.render_target_readback(target, pixels.data(), 16, &width, &height));
	//失败时尺寸出参保持 0
	EXPECT_EQ(width, 0u);
        EXPECT_EQ(height, 0u);
        renderer.render_target_destroy(target);
}

/*
深度读回（第二十八轮）：全屏四边形覆盖处深度 < 1，清屏背景深度 ≈ 1
口径：绿场景渲染进目标后读深度原始值（D32_SFLOAT，按 float 解释）；
      中心被四边形覆盖，应落在 (0, 1) 内且明显小于 1；角落无几何，应为清屏深度 1.0。
*/
TEST_F(Renderer_Offscreen_Test, 深度读回区分几何与清屏)
{
        engine::Renderer renderer;
        ASSERT_TRUE(renderer.init(hidden_config()));
        uint64_t target = 0;
        ASSERT_TRUE(renderer.render_target_create(target, WINDOW_WIDTH, WINDOW_HEIGHT));
        uint64_t texture = 0;
        std::vector<uint8_t> texels = solid_green(2, 2);
        ASSERT_TRUE(renderer.texture_load(texture, texels.data(), 2, 2));
        engine::Mesh_Handle handle = 0;
        ASSERT_TRUE(renderer.mesh_load(handle, quad_data()));
        //用单位矩阵（四边形不放大）：中心被覆盖、四角仍为清屏背景
        renderer.render_target_set(target);
        renderer.texture_bind_set(texture);
        ASSERT_TRUE(renderer.mesh_draw(handle, nullptr));
        ASSERT_TRUE(renderer.frame_render());

        //读回深度（宽×高 个 32 位值）
        std::vector<uint32_t> depth(static_cast<std::size_t>(WINDOW_WIDTH) * WINDOW_HEIGHT);
        uint32_t width = 0;
        uint32_t height = 0;
        ASSERT_TRUE(renderer.render_target_readback_depth(target, depth.data(),
                static_cast<uint32_t>(depth.size()), &width, &height));
        EXPECT_EQ(width, WINDOW_WIDTH);
        EXPECT_EQ(height, WINDOW_HEIGHT);
        //中心与角落的深度（按 float 解释）
        float center_depth = 0.0f;
        float corner_depth = 0.0f;
        std::size_t center_index = static_cast<std::size_t>(WINDOW_HEIGHT / 2) * WINDOW_WIDTH + WINDOW_WIDTH / 2;
        std::memcpy(&center_depth, &depth[center_index], 4);
        std::memcpy(&corner_depth, &depth[2 * WINDOW_WIDTH + 2], 4);
        RecordProperty("depth_center", center_depth);
        RecordProperty("depth_corner", corner_depth);
        EXPECT_GT(center_depth, 0.0f);
        EXPECT_LT(center_depth, 0.999f) << "四边形覆盖处深度未小于 1";
        EXPECT_GT(corner_depth, 0.999f) << "清屏区域深度应接近 1";

        //容量不足被拒（元素个数不足）
        std::vector<uint32_t> tiny(16);
        uint32_t small_width = 9;
        uint32_t small_height = 9;
        EXPECT_FALSE(renderer.render_target_readback_depth(target, tiny.data(),
                static_cast<uint32_t>(tiny.size()), &small_width, &small_height));
        EXPECT_EQ(small_width, 0u);
        EXPECT_EQ(small_height, 0u);

        renderer.mesh_unload(handle);
        renderer.texture_unload(texture);
        renderer.render_target_destroy(target);
}

/*
渲染目标尺寸查询（第七十七轮）
口径：创建任意尺寸的目标后可直接查回实际尺寸；未初始化 / 无效句柄 / 已销毁句柄返回假且出参归零。
*/
TEST_F(Renderer_Offscreen_Test, 目标尺寸可查询)
{
        engine::Renderer renderer;
        //未初始化：返回假且出参归零
        uint32_t width = 7;
        uint32_t height = 7;
        EXPECT_FALSE(renderer.render_target_size(1, &width, &height));
        EXPECT_EQ(width, 0u);
        EXPECT_EQ(height, 0u);

        ASSERT_TRUE(renderer.init(hidden_config()));
        //任意尺寸（与交换链不同）也应原样查回
        uint64_t target = 0;
        uint32_t target_width = WINDOW_WIDTH / 2;
        uint32_t target_height = WINDOW_HEIGHT / 2;
        ASSERT_TRUE(renderer.render_target_create(target, target_width, target_height));
        uint32_t queried_width = 0;
        uint32_t queried_height = 0;
        EXPECT_TRUE(renderer.render_target_size(target, &queried_width, &queried_height));
        EXPECT_EQ(queried_width, target_width);
        EXPECT_EQ(queried_height, target_height);

        //只取宽（高指针为空）亦有效
        uint32_t only_width = 0;
        EXPECT_TRUE(renderer.render_target_size(target, &only_width, nullptr));
        EXPECT_EQ(only_width, target_width);

        //无效句柄被拒且出参归零
        uint32_t invalid_width = 5;
        uint32_t invalid_height = 5;
        EXPECT_FALSE(renderer.render_target_size(0, &invalid_width, &invalid_height));
        EXPECT_EQ(invalid_width, 0u);
        EXPECT_EQ(invalid_height, 0u);
        EXPECT_FALSE(renderer.render_target_size(target + 99, &invalid_width, &invalid_height));

        //销毁后同样失效
        renderer.render_target_destroy(target);
        EXPECT_FALSE(renderer.render_target_size(target, &invalid_width, &invalid_height));
        EXPECT_EQ(invalid_width, 0u);
}
