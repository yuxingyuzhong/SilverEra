#pragma once
/*
渲染后处理链测试辅助（第五十九轮拆分；由原「渲染后处理链测试.cpp」抽出）
提供隐藏窗口配置、像素读取、纹理与网格构造，以及夹具 Renderer_Post_Chain_Test。
*/
#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

//获取渲染器门面（含管线登记与后处理链接口）与网格数据
#include "Application/Renderer/src/render/Renderer/渲染器.h"

namespace
{
	//窗口尺寸（小尺寸便于逐像素遍历；目标与交换链同尺寸以满足后处理 1:1 取源）
	constexpr uint32_t WINDOW_WIDTH = 64;
	constexpr uint32_t WINDOW_HEIGHT = 36;
	//系统字体路径（存在才跑文字兼容用例；字号 16）
	const char* const FONT_PATH = "C:\\Windows\\Fonts\\consola.ttf";
	constexpr uint32_t FONT_PIXEL_HEIGHT = 16;

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

	//构造多重采样配置（第一百九十一轮；尺寸与 hidden_config 相同，仅改采样数）
	engine::Renderer_Config msaa_config(uint32_t samples)
	{
		engine::Renderer_Config config = hidden_config();
		config.msaa_samples = samples;
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
