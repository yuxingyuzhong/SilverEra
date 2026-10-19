/*
纹理解码测试（L2 隐藏窗口 + 像素回读）
覆盖：未初始化守卫、不存在的文件被拒、BMP 解码并绘制、PNG 解码并绘制。
素材由用例**现场生成**（BMP 与 PNG 均为手写编码器），不依赖仓库内任何素材文件；
用纯色（红）而非灰度：只有「解码真的发生」才会解出红色，故可证解码路径而非默认白纹理。
断言口径：通道序无关（按三通道排序比较）+ 颜色与 alpha 分离（alpha 恒取第 4 字节）；
      不使用 std::min（本层包含链存在 min 宏，会触发 C2589）。
*/
#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

//获取渲染器门面与网格数据
#include "Application/Renderer/src/render/Renderer/渲染器.h"

namespace
{
	//窗口尺寸（与回读断言配套）
	constexpr uint32_t WINDOW_WIDTH = 64;
	constexpr uint32_t WINDOW_HEIGHT = 36;
	//素材文件名（相对用例工作目录；纯 ASCII，避免本机代码页对路径的影响）
	const char* const BMP_FILE = "解码测试素材.bmp";
	const char* const PNG_FILE = "解码测试素材.png";
	//素材颜色（纯红：只有解码成功才会得到该色）
	constexpr uint8_t SAMPLE_RED = 255;
	constexpr uint8_t SAMPLE_GREEN = 0;
	constexpr uint8_t SAMPLE_BLUE = 0;

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

	/*
	判定像素是否为「接近纯红」
	对通道序无关：把前三个通道排序后要求「两低一高」，高 ≥ 240、低 ≤ 20。
	*/
	bool pixel_is_red(std::array<uint8_t, 4> value)
	{
		//排序前三通道
		std::array<uint8_t, 3> color = { value[0], value[1], value[2] };
		std::sort(color.begin(), color.end());
		//通道判定
		return color[0] <= 20 && color[1] <= 20 && color[2] >= 240;
	}

	//构造中心四边形（世界边长为 2 × half，位于 z = 0）
	engine::Mesh_Data quad_data(float half)
	{
		engine::Mesh_Data data;
		data.vertices = {
			{ { -half, -half, 0.0f }, { 0.0f, 0.0f } },
			{ { half, -half, 0.0f }, { 1.0f, 0.0f } },
			{ { half, half, 0.0f }, { 1.0f, 1.0f } },
			{ { -half, half, 0.0f }, { 0.0f, 1.0f } },
		};
		data.indices = { 0, 1, 2, 0, 2, 3 };
		return data;
	}

	//把字节写入文件
	bool write_file(const char* path, const std::vector<uint8_t>& bytes)
	{
		//二进制截断写入
		std::ofstream file(path, std::ios::binary | std::ios::trunc);
		//若打不开
		if (!file)
			return false;
		file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
		return file.good();
	}

	//删除素材文件（用例善后）
	void remove_file(const char* path)
	{
		std::remove(path);
	}

	//CRC32（PNG 分块校验用；反射多项式 0xEDB88320）
	uint32_t crc32_of(const uint8_t* data, std::size_t size)
	{
		uint32_t crc = 0xFFFFFFFFu;
		for (std::size_t index = 0; index < size; ++index)
		{
			crc ^= data[index];
			for (int bit = 0; bit < 8; ++bit)
				crc = (crc & 1u) ? ((crc >> 1) ^ 0xEDB88320u) : (crc >> 1);
		}
		return crc ^ 0xFFFFFFFFu;
	}

	//Adler32（zlib 校验用）
	uint32_t adler32_of(const uint8_t* data, std::size_t size)
	{
		uint32_t a = 1;
		uint32_t b = 0;
		for (std::size_t index = 0; index < size; ++index)
		{
			a = (a + data[index]) % 65521u;
			b = (b + a) % 65521u;
		}
		return (b << 16) | a;
	}

	//按大端把 32 位值推入字节流
	void push_u32_be(std::vector<uint8_t>& out, uint32_t value)
	{
		out.push_back(static_cast<uint8_t>(value >> 24));
		out.push_back(static_cast<uint8_t>(value >> 16));
		out.push_back(static_cast<uint8_t>(value >> 8));
		out.push_back(static_cast<uint8_t>(value));
	}

	//按小端把 16 / 32 位值推入字节流
	void push_u16_le(std::vector<uint8_t>& out, uint16_t value)
	{
		out.push_back(static_cast<uint8_t>(value & 0xFFu));
		out.push_back(static_cast<uint8_t>(value >> 8));
	}
	void push_u32_le(std::vector<uint8_t>& out, uint32_t value)
	{
		out.push_back(static_cast<uint8_t>(value & 0xFFu));
		out.push_back(static_cast<uint8_t>((value >> 8) & 0xFFu));
		out.push_back(static_cast<uint8_t>((value >> 16) & 0xFFu));
		out.push_back(static_cast<uint8_t>((value >> 24) & 0xFFu));
	}

	//推入一个 PNG 分块（长度 + 类型 + 数据 + CRC）
	void png_push_chunk(std::vector<uint8_t>& out, const char type[4], const std::vector<uint8_t>& data)
	{
		//长度（大端）
		push_u32_be(out, static_cast<uint32_t>(data.size()));
		//类型 4 字节 + 数据
		std::vector<uint8_t> crc_source;
		crc_source.reserve(4 + data.size());
		for (int index = 0; index < 4; ++index)
		{
			out.push_back(static_cast<uint8_t>(type[index]));
			crc_source.push_back(static_cast<uint8_t>(type[index]));
		}
		out.insert(out.end(), data.begin(), data.end());
		crc_source.insert(crc_source.end(), data.begin(), data.end());
		//CRC（覆盖类型 + 数据）
		push_u32_be(out, crc32_of(crc_source.data(), crc_source.size()));
	}

	/*
	手写 2×2 纯色 PNG 编码器
	IDAT 用 zlib「存储（未压缩）deflate 块」承载，故无需压缩算法即可生成合法 PNG。
	*/
	std::vector<uint8_t> png_encode_2x2(uint8_t red, uint8_t green, uint8_t blue)
	{
		//文件头
		std::vector<uint8_t> out = { 0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A };

		//IHDR：宽 / 高 / 位深 8 / 颜色类型 6（RGBA）/ 压缩 0 / 滤波 0 / 隔行 0
		std::vector<uint8_t> ihdr;
		push_u32_be(ihdr, 2);
		push_u32_be(ihdr, 2);
		ihdr.push_back(8);
		ihdr.push_back(6);
		ihdr.push_back(0);
		ihdr.push_back(0);
		ihdr.push_back(0);
		png_push_chunk(out, "IHDR", ihdr);

		//原始扫描线数据：每行 = 滤波字节 0 + 2 像素 × 4 通道
		std::vector<uint8_t> raw;
		for (uint32_t row = 0; row < 2; ++row)
		{
			//本行滤波字节
			raw.push_back(0);
			for (uint32_t column = 0; column < 2; ++column)
			{
				raw.push_back(red);
				raw.push_back(green);
				raw.push_back(blue);
				raw.push_back(255);
			}
		}

		//zlib 流：头两字节 + 存储 deflate 块 + Adler32
		std::vector<uint8_t> zlib_stream;
		zlib_stream.push_back(0x78);
		zlib_stream.push_back(0x01);
		//存储块：BFINAL = 1、BTYPE = 00
		zlib_stream.push_back(0x01);
		//LEN 与其反码（小端）
		push_u16_le(zlib_stream, static_cast<uint16_t>(raw.size()));
		push_u16_le(zlib_stream, static_cast<uint16_t>(~raw.size() & 0xFFFFu));
		zlib_stream.insert(zlib_stream.end(), raw.begin(), raw.end());
		//Adler32（大端）
		push_u32_be(zlib_stream, adler32_of(raw.data(), raw.size()));
		png_push_chunk(out, "IDAT", zlib_stream);

		//IEND（空数据）
		png_push_chunk(out, "IEND", std::vector<uint8_t>{});

		return out;
	}

	/*
	手写 2×2 纯色 BMP 编码器（24 位、自下而上、行按 4 字节对齐）
	*/
	std::vector<uint8_t> bmp_encode_2x2(uint8_t red, uint8_t green, uint8_t blue)
	{
		//像素行字节数与其 4 字节对齐填充
		uint32_t row_bytes = 2 * 3;
		uint32_t padding = (4 - (row_bytes % 4)) % 4;
		uint32_t pixel_bytes = (row_bytes + padding) * 2;
		//文件总长 = 14（文件头）+ 40（信息头）+ 像素
		uint32_t file_bytes = 14 + 40 + pixel_bytes;

		std::vector<uint8_t> out;
		out.reserve(file_bytes);

		//文件头（14 字节）
		out.push_back('B');
		out.push_back('M');
		push_u32_le(out, file_bytes);
		push_u16_le(out, 0);
		push_u16_le(out, 0);
		push_u32_le(out, 54);

		//信息头（40 字节，BITMAPINFOHEADER）
		push_u32_le(out, 40);
		push_u32_le(out, 2);
		push_u32_le(out, 2);
		push_u16_le(out, 1);
		push_u16_le(out, 24);
		push_u32_le(out, 0);
		push_u32_le(out, pixel_bytes);
		push_u32_le(out, 2835);
		push_u32_le(out, 2835);
		push_u32_le(out, 0);
		push_u32_le(out, 0);

		//像素（BGR，自下而上）
		for (uint32_t row = 0; row < 2; ++row)
		{
			for (uint32_t column = 0; column < 2; ++column)
			{
				out.push_back(blue);
				out.push_back(green);
				out.push_back(red);
			}
			//行填充
			for (uint32_t index = 0; index < padding; ++index)
				out.push_back(0);
		}
		return out;
	}
}

//纹理解码测试夹具
class Renderer_Decode_Test : public ::testing::Test
{
};

//未初始化时从文件加载纹理失败
TEST_F(Renderer_Decode_Test, 未初始化加载文件纹理失败)
{
	engine::Renderer renderer;
	uint64_t handle = 123;
	EXPECT_FALSE(renderer.texture_load_file(handle, PNG_FILE));
	//失败时句柄复位为零
	EXPECT_EQ(handle, 0u);
}

//不存在的文件被拒
TEST_F(Renderer_Decode_Test, 不存在的文件加载失败)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	uint64_t handle = 123;
	EXPECT_FALSE(renderer.texture_load_file(handle, "此文件不存在_解码测试.png"));
	//失败时句柄复位为零
	EXPECT_EQ(handle, 0u);
}

//BMP 解码并绘制（现场生成素材）
TEST_F(Renderer_Decode_Test, BMP解码并绘制)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	//现场写出 2×2 纯红 BMP
	ASSERT_TRUE(write_file(BMP_FILE, bmp_encode_2x2(SAMPLE_RED, SAMPLE_GREEN, SAMPLE_BLUE)));
	uint64_t texture = 0;
	ASSERT_TRUE(renderer.texture_load_file(texture, BMP_FILE));
	ASSERT_NE(texture, 0u);
	renderer.texture_bind_set(texture);
	//加载覆盖视口中央的四边形并登记绘制
	engine::Mesh_Handle handle = 0;
	ASSERT_TRUE(renderer.mesh_load(handle, quad_data(1.0f)));
	ASSERT_TRUE(renderer.mesh_draw(handle, nullptr));
	std::vector<uint8_t> pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
	//中心应为解码出的纯红（若非解码则会是默认白纹理）
	std::array<uint8_t, 4> center = pixel_get(pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
	EXPECT_TRUE(pixel_is_red(center)) << "BMP 解码结果未落到画面上：" << pixel_text(center);
	renderer.mesh_unload(handle);
	renderer.texture_unload(texture);
	remove_file(BMP_FILE);
}

//PNG 解码并绘制（现场生成素材；手写编码器用存储 deflate 块）
TEST_F(Renderer_Decode_Test, PNG解码并绘制)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	//现场写出 2×2 纯红 PNG
	ASSERT_TRUE(write_file(PNG_FILE, png_encode_2x2(SAMPLE_RED, SAMPLE_GREEN, SAMPLE_BLUE)));
	uint64_t texture = 0;
	ASSERT_TRUE(renderer.texture_load_file(texture, PNG_FILE));
	ASSERT_NE(texture, 0u);
	renderer.texture_bind_set(texture);
	//加载覆盖视口中央的四边形并登记绘制
	engine::Mesh_Handle handle = 0;
	ASSERT_TRUE(renderer.mesh_load(handle, quad_data(1.0f)));
	ASSERT_TRUE(renderer.mesh_draw(handle, nullptr));
	std::vector<uint8_t> pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
	//中心应为解码出的纯红
	std::array<uint8_t, 4> center = pixel_get(pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
	EXPECT_TRUE(pixel_is_red(center)) << "PNG 解码结果未落到画面上：" << pixel_text(center);
	renderer.mesh_unload(handle);
	renderer.texture_unload(texture);
	remove_file(PNG_FILE);
}
