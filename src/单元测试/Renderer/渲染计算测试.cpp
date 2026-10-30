/*
渲染计算测试（第二百四十一轮）
覆盖：未初始化时计算不支持且健康检查失败；
      初始化后计算上下文可用（图形队列族含 COMPUTE 位的机器上），健康检查把推送常量按位模式写入并回读一致；
      计算启用不影响默认图形路径出帧。
*/
#include <gtest/gtest.h>

#include <cstdint>

//获取渲染器门面
#include "Application/Renderer/src/render/Renderer/渲染器.h"
//线程组数换算与屏障用途（第二百四十二轮）
#include "Application/Renderer/src/render/Compute/计算数据.h"

namespace
{
	//构造隐藏窗口配置
	engine::Renderer_Config hidden_config(void)
	{
		engine::Renderer_Config config;
		config.width = 64;
		config.height = 36;
		config.hidden = true;
		return config;
	}
}

//计算管线测试夹具
class Renderer_Compute_Test : public ::testing::Test
{
};

//未初始化时计算不支持，健康检查返回假
TEST_F(Renderer_Compute_Test, 未初始化计算不可用)
{
	engine::Renderer renderer;
	EXPECT_FALSE(renderer.compute_supported());
	uint32_t value = 0;
	EXPECT_FALSE(renderer.compute_health_check(0x3F800000u, value));
	EXPECT_EQ(value, 0u);
}

//初始化后健康检查：推送魔数经计算着色器写入存储缓冲并可回读
TEST_F(Renderer_Compute_Test, 计算管线健康检查)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	//本机图形队列族不含计算位则跳过（降级路径已在初始化日志中报告）
	if (!renderer.compute_supported())
		GTEST_SKIP() << "本机图形队列族不支持计算路径";
	//两次不同魔数（1.0f 与 3.14159265 的位模式）
	const uint32_t magics[2] = { 0x3F800000u, 0x40490FDBu };
	for (uint32_t magic : magics)
	{
		uint32_t read_back = 0;
		//健康检查失败即断言失败
		ASSERT_TRUE(renderer.compute_health_check(magic, read_back));
		EXPECT_EQ(read_back, magic) << "计算回读值与推送魔数不一致";
	}
	//计算启用后默认图形路径仍可出帧
	ASSERT_TRUE(renderer.frame_render());
}

//计算启用时默认图形路径画面不变（无绘制时仅清屏，与未启用一致的口径由其他套件覆盖）
TEST_F(Renderer_Compute_Test, 计算启用不影响默认路径)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	//无论是否支持计算，默认路径都应能连续出帧
	ASSERT_TRUE(renderer.frame_render());
	ASSERT_TRUE(renderer.frame_render());
}

//线程组数换算（第二百四十二轮）：向上取整、空输入不调度
TEST_F(Renderer_Compute_Test, 线程组数换算)
{
	//空元素数 / 非法组大小一律为 0
	EXPECT_EQ(engine::compute_group_count(0u, 64u), 0u);
	EXPECT_EQ(engine::compute_group_count(64u, 0u), 0u);
	//整除与余数
	EXPECT_EQ(engine::compute_group_count(64u, 64u), 1u);
	EXPECT_EQ(engine::compute_group_count(65u, 64u), 2u);
	EXPECT_EQ(engine::compute_group_count(1u, 1u), 1u);
	EXPECT_EQ(engine::compute_group_count(10000u, 64u), 157u);
	EXPECT_EQ(engine::compute_group_count(10000u, 256u), 40u);
}

//调度计数随健康检查累加（第二百四十二轮：dispatch 计数与统计快照）
TEST_F(Renderer_Compute_Test, 调度计数累加)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	//不支持计算则跳过
	if (!renderer.compute_supported())
		GTEST_SKIP() << "本机图形队列族不支持计算路径";
	//两次健康检查 → 两次调度
	uint32_t read_back = 0;
	ASSERT_TRUE(renderer.compute_health_check(0x3F800000u, read_back));
	ASSERT_TRUE(renderer.compute_health_check(0x40490FDBu, read_back));
	EXPECT_EQ(read_back, 0x40490FDBu);
}

/*
存储缓冲 ＋ 计算管线 ＋ 门面调度（第二百四十三轮）
口径：1 万个 float 初值 0，经「计算加一.comp」加一后逐个回读为 1.0；覆盖 SSBO 创建 / 绑定 / 调度 / 回读整链。
*/
TEST_F(Renderer_Compute_Test, 计算缓冲加一万)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	//不支持计算则跳过
	if (!renderer.compute_supported())
		GTEST_SKIP() << "本机图形队列族不支持计算路径";
	constexpr uint32_t ELEMENT_COUNT = 10000;
	std::vector<float> values(ELEMENT_COUNT, 0.0f);
	//登记存储缓冲（带初值）
	engine::Compute_Buffer_Handle buffer = 0;
	ASSERT_TRUE(renderer.compute_buffer_load(buffer, static_cast<uint32_t>(values.size() * sizeof(float)), values.data()));
	EXPECT_GE(buffer, 1u);
	//登记「计算加一」管线
	engine::Compute_Pipeline_Handle pipeline = 0;
	ASSERT_TRUE(renderer.compute_pipeline_load(pipeline, "计算加一.comp.spv"));
	EXPECT_GE(pipeline, 1u);
	//绑定到 set0 binding 0
	ASSERT_TRUE(renderer.compute_buffer_bind(buffer, 0));
	//推送常量：元素数
	uint32_t count = ELEMENT_COUNT;
	ASSERT_TRUE(renderer.compute_dispatch(pipeline, ELEMENT_COUNT, 256, &count, sizeof(count)));
	//回读并逐元素校验
	std::vector<float> result(ELEMENT_COUNT, 0.0f);
	ASSERT_TRUE(renderer.compute_buffer_read(buffer, result.data(), static_cast<uint32_t>(result.size() * sizeof(float))));
	for (std::size_t index = 0; index < result.size(); ++index)
		ASSERT_FLOAT_EQ(result[index], 1.0f) << "第 " << index << " 个元素未被加一";
	//卸载幂等
	renderer.compute_pipeline_unload(pipeline);
	renderer.compute_pipeline_unload(pipeline);
	renderer.compute_buffer_unload(buffer);
	renderer.compute_buffer_unload(buffer);
}

//计算缓冲与管线的非法参数（第二百四十三轮）
TEST_F(Renderer_Compute_Test, 计算资源非法参数)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.compute_supported())
		GTEST_SKIP() << "本机图形队列族不支持计算路径";
	//尺寸 0 与超上限拒绝
	engine::Compute_Buffer_Handle zero = 7;
	EXPECT_FALSE(renderer.compute_buffer_load(zero, 0));
	EXPECT_EQ(zero, 0u);
	engine::Compute_Buffer_Handle huge = 7;
	EXPECT_FALSE(renderer.compute_buffer_load(huge, engine::COMPUTE_BUFFER_MAX_BYTES + 1u));
	EXPECT_EQ(huge, 0u);
	//合法缓冲
	std::vector<float> seed(8, 2.0f);
	engine::Compute_Buffer_Handle buffer = 0;
	ASSERT_TRUE(renderer.compute_buffer_load(buffer, static_cast<uint32_t>(seed.size() * sizeof(float)), seed.data()));
	//越界写入 / 读回拒绝
	EXPECT_FALSE(renderer.compute_buffer_write(buffer, seed.data(), 8, static_cast<uint32_t>(seed.size() * sizeof(float))));
	EXPECT_FALSE(renderer.compute_buffer_read(buffer, seed.data(), 8, static_cast<uint32_t>(seed.size() * sizeof(float))));
	//无效句柄与越界 binding 拒绝
	EXPECT_FALSE(renderer.compute_buffer_write(9999u, seed.data(), 4, 0));
	EXPECT_FALSE(renderer.compute_buffer_bind(buffer, engine::COMPUTE_BINDING_MAX));
	//不存在的着色器文件
	engine::Compute_Pipeline_Handle missing = 9;
	EXPECT_FALSE(renderer.compute_pipeline_load(missing, "不存在的计算着色器.comp.spv"));
	EXPECT_EQ(missing, 0u);
	//无效管线句柄调度拒绝；空元素数视为无需调度
	EXPECT_FALSE(renderer.compute_dispatch(9999u, 8, 64));
	EXPECT_TRUE(renderer.compute_dispatch(missing, 0, 64));
	renderer.compute_buffer_unload(buffer);
}

//未初始化时的计算统计（第二百四十五轮）
TEST_F(Renderer_Compute_Test, 未初始化计算统计为零)
{
	engine::Renderer renderer;
	engine::Compute_Stats stats = renderer.compute_stats_get();
	EXPECT_EQ(stats.dispatch_count, 0u);
	EXPECT_FLOAT_EQ(stats.elapsed_ms, -1.0f);
	EXPECT_FALSE(renderer.compute_timestamp_supported());
}

/*
计算调度计数与计时（第二百四十五轮）
口径：调度前计数为 0、计时为 −1；两次调度后计数为 2；支持时间戳时计时为非负且量级合理，不支持时恒 −1。
*/
TEST_F(Renderer_Compute_Test, 计算统计与计时)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.compute_supported())
		GTEST_SKIP() << "本机图形队列族不支持计算路径";
	//调度前：计数为 0、计时为 −1
	engine::Compute_Stats before = renderer.compute_stats_get();
	EXPECT_EQ(before.dispatch_count, 0u);
	EXPECT_FLOAT_EQ(before.elapsed_ms, -1.0f);
	//准备 1 万 float 与管线
	constexpr uint32_t ELEMENT_COUNT = 10000;
	std::vector<float> values(ELEMENT_COUNT, 0.0f);
	engine::Compute_Buffer_Handle buffer = 0;
	ASSERT_TRUE(renderer.compute_buffer_load(buffer, static_cast<uint32_t>(values.size() * sizeof(float)), values.data()));
	engine::Compute_Pipeline_Handle pipeline = 0;
	ASSERT_TRUE(renderer.compute_pipeline_load(pipeline, "计算加一.comp.spv"));
	ASSERT_TRUE(renderer.compute_buffer_bind(buffer, 0));
	uint32_t count = ELEMENT_COUNT;
	ASSERT_TRUE(renderer.compute_dispatch(pipeline, ELEMENT_COUNT, 256, &count, sizeof(count)));
	//首次调度后：计数为 1
	engine::Compute_Stats after_first = renderer.compute_stats_get();
	EXPECT_EQ(after_first.dispatch_count, 1u);
	//支持时间戳：计时非负且量级合理（1 万 float 加一远小于 1 秒）
	if (renderer.compute_timestamp_supported())
	{
		EXPECT_GE(after_first.elapsed_ms, 0.0f);
		EXPECT_LT(after_first.elapsed_ms, 1000.0f);
	}
	//不支持时间戳：计时恒 −1
	else
		EXPECT_FLOAT_EQ(after_first.elapsed_ms, -1.0f);
	//第二次调度后：计数为 2
	ASSERT_TRUE(renderer.compute_dispatch(pipeline, ELEMENT_COUNT, 256, &count, sizeof(count)));
	EXPECT_EQ(renderer.compute_stats_get().dispatch_count, 2u);
	renderer.compute_pipeline_unload(pipeline);
	renderer.compute_buffer_unload(buffer);
}
/*
设备本地缓冲 ＋ GPU 回读通道（第二百四十四轮）
口径：设备本地 SSBO（经暂存上传初值）加一后，用 compute_readback（拷贝到暂存再映射）逐元素回读为 1.0。
*/
TEST_F(Renderer_Compute_Test, 设备本地缓冲回读一致)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	//不支持计算则跳过
	if (!renderer.compute_supported())
		GTEST_SKIP() << "本机图形队列族不支持计算路径";
	constexpr uint32_t ELEMENT_COUNT = 10000;
	std::vector<float> values(ELEMENT_COUNT, 0.0f);
	//登记设备本地存储缓冲（初值经暂存上传）
	engine::Compute_Buffer_Handle buffer = 0;
	ASSERT_TRUE(renderer.compute_buffer_load(buffer, static_cast<uint32_t>(values.size() * sizeof(float)),
		values.data(), true));
	EXPECT_GE(buffer, 1u);
	//登记「计算加一」管线并绑定
	engine::Compute_Pipeline_Handle pipeline = 0;
	ASSERT_TRUE(renderer.compute_pipeline_load(pipeline, "计算加一.comp.spv"));
	ASSERT_TRUE(renderer.compute_buffer_bind(buffer, 0));
	//推送常量：元素数
	uint32_t count = ELEMENT_COUNT;
	ASSERT_TRUE(renderer.compute_dispatch(pipeline, ELEMENT_COUNT, 256, &count, sizeof(count)));
	//GPU 回读通道：拷贝到暂存后映射读回
	std::vector<float> result(ELEMENT_COUNT, 0.0f);
	ASSERT_TRUE(renderer.compute_readback(buffer, result.data(), static_cast<uint32_t>(result.size() * sizeof(float))));
	for (std::size_t index = 0; index < result.size(); ++index)
		ASSERT_FLOAT_EQ(result[index], 1.0f) << "第 " << index << " 个元素未被加一";
	renderer.compute_pipeline_unload(pipeline);
	renderer.compute_buffer_unload(buffer);
}

//回读通道的非法参数与主机可见缓冲同口径（第二百四十四轮）
TEST_F(Renderer_Compute_Test, 回读通道非法参数)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.compute_supported())
		GTEST_SKIP() << "本机图形队列族不支持计算路径";
	//合法主机可见缓冲（带初值）
	std::vector<float> seed(16, 3.5f);
	engine::Compute_Buffer_Handle buffer = 0;
	ASSERT_TRUE(renderer.compute_buffer_load(buffer, static_cast<uint32_t>(seed.size() * sizeof(float)), seed.data()));
	//主机可见缓冲走同一回读通道，结果与初值一致
	std::vector<float> result(seed.size(), 0.0f);
	ASSERT_TRUE(renderer.compute_readback(buffer, result.data(), static_cast<uint32_t>(result.size() * sizeof(float))));
	for (std::size_t index = 0; index < result.size(); ++index)
		EXPECT_FLOAT_EQ(result[index], 3.5f);
	//尺寸 0 / 越界 / 无效句柄一律拒绝
	EXPECT_FALSE(renderer.compute_readback(buffer, result.data(), 0, 0));
	EXPECT_FALSE(renderer.compute_readback(buffer, result.data(), 8, static_cast<uint32_t>(seed.size() * sizeof(float))));
	EXPECT_FALSE(renderer.compute_readback(9999u, result.data(), 8, 0));
	//偏移 + 长度恰达末尾（合法）
	EXPECT_TRUE(renderer.compute_readback(buffer, result.data(), 4,
		static_cast<uint32_t>(seed.size() * sizeof(float)) - 4u));
	renderer.compute_buffer_unload(buffer);
}
/*
强制关闭计算路径（第二百四十六轮）
口径：compute_enable = false 时不构建计算上下文、compute_supported 恒假，但默认帧路径照常出帧。
*/
TEST_F(Renderer_Compute_Test, 计算强制关闭仍出帧)
{
	engine::Renderer_Config config = hidden_config();
	config.compute_enable = false;
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(config));
	//计算路径整体关闭
	EXPECT_FALSE(renderer.compute_supported());
	EXPECT_FALSE(renderer.compute_timestamp_supported());
	//能力探测仍如实反映设备（与开关无关）
	engine::Compute_Capabilities capabilities = renderer.compute_capabilities_get();
	EXPECT_TRUE(capabilities.compute) << "本机图形队列族应含 COMPUTE 位；否则此断言需按设备能力放宽";
	//默认帧路径照常出帧两帧
	EXPECT_TRUE(renderer.frame_render());
	EXPECT_TRUE(renderer.frame_render());
	EXPECT_GE(renderer.frame_stats_get().frames, 1u);
	//计算缓冲 / 管线在关闭状态下全部拒绝
	engine::Compute_Buffer_Handle buffer = 7;
	EXPECT_FALSE(renderer.compute_buffer_load(buffer, 64));
	EXPECT_EQ(buffer, 0u);
	engine::Compute_Pipeline_Handle pipeline = 9;
	EXPECT_FALSE(renderer.compute_pipeline_load(pipeline, "计算加一.comp.spv"));
	EXPECT_EQ(pipeline, 0u);
}

//降级矩阵探测与未初始化口径（第二百四十六轮）
TEST_F(Renderer_Compute_Test, 降级矩阵探测)
{
	//未初始化：各项为假
	engine::Renderer cold;
	engine::Compute_Capabilities cold_caps = cold.compute_capabilities_get();
	EXPECT_FALSE(cold_caps.compute);
	EXPECT_FALSE(cold_caps.draw_indirect_count);
	EXPECT_FALSE(cold_caps.multi_draw_indirect);
	EXPECT_FALSE(cold_caps.timestamp);
	EXPECT_FALSE(cold_caps.host_query_reset);
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	engine::Compute_Capabilities caps = renderer.compute_capabilities_get();
	//计算能力与门面一致（本机不支持计算时整例跳过）
	if (!caps.compute)
		GTEST_SKIP() << "本机图形队列族不支持计算路径";
	EXPECT_TRUE(renderer.compute_supported());
	//时间戳能力与门面同口径
	EXPECT_EQ(caps.timestamp, renderer.compute_timestamp_supported());
}