/*
能力展示显示排查（第四百零二轮：展示重构后的可见性回归）
用途：以「多距离 ＋ 多角度」扫过整条展廊，量化每格画面与「纯天空」的差异，定位「场景无显示」。
口径：
1. 参考帧 = 同一相机、不调用 frame_draw（只有清屏 ＋ 天空）；
2. 展示帧 = 调用 frame_draw 后回读；统计与参考帧逐通道差 ≥ 6 的像素数；
3. 每格取 12 / 20 / 30 三个距离与 −20° / 0° / +20° 三个角度，另加一个「高空俯视」角度；
4. 断言每格都存在可见覆盖（差像素数 > 200；地面 ＋ 地台是底色，正常情况应远大于此）。
文字口径：23 条展区标题一次性绘制后 font_skipped_codepoints == 0。
*/
#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <string>
#include <vector>

#include "Application/Renderer/src/render/Renderer/渲染器.h"
#include "src/主调/能力展示/能力展示场景.h"
#include "src/主调/能力展示/展区公共.h"

namespace
{
	//排查窗口尺寸（256 × 144：既能在秒级跑完数百帧，又足以看清展台与标签是否真的画出来）
	constexpr uint32_t PROBE_WIDTH = 256;
	constexpr uint32_t PROBE_HEIGHT = 144;
	//「与参考帧不同」的逐通道阈值
	constexpr int VISIBLE_THRESHOLD = 6;
	//每格可见覆盖的判定下限（差像素数；随分辨率等比放大）
	constexpr uint32_t VISIBLE_MIN_PIXELS = 400;

	engine::Renderer_Config probe_config(void)
	{
		engine::Renderer_Config config;
		config.width = PROBE_WIDTH;
		config.height = PROBE_HEIGHT;
		config.hidden = true;
		return config;
	}

	//本帧像素字节数
	uint32_t probe_bytes(void)
	{
		return PROBE_WIDTH * PROBE_HEIGHT * 4;
	}

	//整帧平均亮度（Rec.709 加权）
	float average_luma(const std::vector<uint8_t>& pixels)
	{
		double sum = 0.0;
		for (std::size_t index = 0; index + 3 < pixels.size(); index += 4)
			sum += 0.2126 * pixels[index] + 0.7152 * pixels[index + 1] + 0.0722 * pixels[index + 2];
		return static_cast<float>(sum / static_cast<double>(PROBE_WIDTH * PROBE_HEIGHT));
	}

	//统计两帧之间逐通道差 ≥ 阈值的像素数
	uint32_t changed_pixels(const std::vector<uint8_t>& left, const std::vector<uint8_t>& right)
	{
		uint32_t count = 0;
		for (std::size_t index = 0; index + 3 < left.size(); index += 4)
		{
			int maximum = 0;
			for (std::size_t channel = 0; channel < 3; ++channel)
			{
				int delta = static_cast<int>(left[index + channel]) - static_cast<int>(right[index + channel]);
				if (delta < 0)
					delta = -delta;
				if (maximum < delta)
					maximum = delta;
			}
			if (maximum >= VISIBLE_THRESHOLD)
				++count;
		}
		return count;
	}

	/*
	近黑像素占比（判定「相机贴进几何」）
	相机被墙、门框、地台等几何贴脸时整屏会被暗面填满；这类样本不代表「展区没显示」，
	应跳过而不是判失败。判据：亮度 < 10 的像素占比。
	*/
	float dark_fraction(const std::vector<uint8_t>& pixels)
	{
		uint32_t dark = 0;
		for (std::size_t index = 0; index + 3 < pixels.size(); index += 4)
		{
			const float luma = 0.2126 * pixels[index] + 0.7152 * pixels[index + 1] + 0.0722 * pixels[index + 2];
			if (luma < 10.0f)
				++dark;
		}
		return static_cast<float>(dark) / static_cast<float>(PROBE_WIDTH * PROBE_HEIGHT);
	}

	//按方向应用相机位姿（内部状态 ＋ update 提交；标签与进区开关随真实相机刷新）
	void apply_pose(engine::能力展示场景& scene, engine::Renderer& renderer,
		const float 眼位[3], const float 方向[3])
	{
		//方向 Y 分量钳到 [-1, 1]（避免 asin 定义域外）
		float 纵向 = 方向[1];
		if (纵向 > 1.0f)
			纵向 = 1.0f;
		if (纵向 < -1.0f)
			纵向 = -1.0f;
		const float 俯仰 = std::asin(纵向);
		//偏航由前向定义反解：前向 = (sin(偏航), 0, −cos(偏航))
		const float 偏航 = std::atan2(方向[0], -方向[2]);
		scene.相机位姿设置(眼位, 偏航, 俯仰);
		scene.update(renderer, engine::展示输入{}, 1.0f / 60.0f);
	}

	//把相机放到展区周围：yaw 让视线偏移，dist 为观察距离；俯视角度用于第 4 种角度
	void aim_camera(engine::能力展示场景& scene, engine::Renderer& renderer,
		float x, float z, float dist, float yaw_deg, bool overhead)
	{
		const float 弧度 = yaw_deg * 3.14159265f / 180.0f;
		const float 眼x = x + std::sin(弧度) * dist;
		const float 眼z = z + std::cos(弧度) * dist;
		//高空俯视：抬高眼位并向下看
		const float 眼y = overhead ? 18.0f : 2.2f;
		const float 目标y = overhead ? 0.5f : 1.4f;
		//眼位 → 注视点方向（单位化）
		float 方向[3] = { x - 眼x, 目标y - 眼y, z - 眼z };
		const float 长度 = std::sqrt(方向[0] * 方向[0] + 方向[1] * 方向[1] + 方向[2] * 方向[2]);
		if (长度 > 1.0e-6f)
		{
			方向[0] /= 长度;
			方向[1] /= 长度;
			方向[2] /= 长度;
		}
		const float 眼位[3] = { 眼x, 眼y, 眼z };
		apply_pose(scene, renderer, 眼位, 方向);
	}

	/* ————————————————— PNG 落盘（零外部依赖） —————————————————
	PNG 允许 deflate「存储块」不压缩，故只需 CRC32 ＋ Adler32 ＋ 分块封装，
	不必引入 zlib / stb_image_write。像素按传入通道序原样写出。
	*/
	//CRC32 查表（首次调用惰性生成）
	uint32_t crc32_bytes(const uint8_t* data, std::size_t size)
	{
		static uint32_t table[256];
		static bool ready = false;
		if (!ready)
		{
			for (uint32_t index = 0; index < 256; ++index)
			{
				uint32_t value = index;
				for (uint32_t bit = 0; bit < 8; ++bit)
					value = (value & 1u) ? (0xEDB88320u ^ (value >> 1)) : (value >> 1);
				table[index] = value;
			}
			ready = true;
		}
		uint32_t crc = 0xFFFFFFFFu;
		for (std::size_t index = 0; index < size; ++index)
			crc = table[(crc ^ data[index]) & 0xFFu] ^ (crc >> 8);
		return crc ^ 0xFFFFFFFFu;
	}

	//Adler32（zlib 流尾校验）
	uint32_t adler32_bytes(const std::vector<uint8_t>& data)
	{
		uint32_t a = 1u;
		uint32_t b = 0u;
		for (uint8_t value : data)
		{
			a = (a + value) % 65521u;
			b = (b + a) % 65521u;
		}
		return (b << 16) | a;
	}

	//按 zlib「存储块」格式封装（每块 ≤ 65535 字节，末块置 BFINAL）
	std::vector<uint8_t> zlib_stored(const std::vector<uint8_t>& raw)
	{
		std::vector<uint8_t> out;
		out.push_back(0x78u);
		out.push_back(0x01u);
		std::size_t position = 0;
		do
		{
			const std::size_t block = std::min<std::size_t>(65535u, raw.size() - position);
			const bool last = position + block >= raw.size();
			out.push_back(last ? 1u : 0u);
			//LEN 与 NLEN 均按小端写
			out.push_back(static_cast<uint8_t>(block & 0xFFu));
			out.push_back(static_cast<uint8_t>((block >> 8) & 0xFFu));
			const uint16_t nlen = static_cast<uint16_t>(~static_cast<uint16_t>(block));
			out.push_back(static_cast<uint8_t>(nlen & 0xFFu));
			out.push_back(static_cast<uint8_t>((nlen >> 8) & 0xFFu));
			out.insert(out.end(), raw.begin() + position, raw.begin() + position + block);
			position += block;
		} while (position < raw.size());
		const uint32_t adler = adler32_bytes(raw);
		out.push_back(static_cast<uint8_t>((adler >> 24) & 0xFFu));
		out.push_back(static_cast<uint8_t>((adler >> 16) & 0xFFu));
		out.push_back(static_cast<uint8_t>((adler >> 8) & 0xFFu));
		out.push_back(static_cast<uint8_t>(adler & 0xFFu));
		return out;
	}

	//写一个 PNG 分块（长度 ＋ 类型 ＋ 数据 ＋ CRC）
	void png_chunk_write(std::ofstream& file, const char type[4], const std::vector<uint8_t>& data)
	{
		const uint32_t length = static_cast<uint32_t>(data.size());
		uint8_t header[8] = {
			static_cast<uint8_t>((length >> 24) & 0xFFu), static_cast<uint8_t>((length >> 16) & 0xFFu),
			static_cast<uint8_t>((length >> 8) & 0xFFu), static_cast<uint8_t>(length & 0xFFu),
			static_cast<uint8_t>(type[0]), static_cast<uint8_t>(type[1]),
			static_cast<uint8_t>(type[2]), static_cast<uint8_t>(type[3]),
		};
		file.write(reinterpret_cast<const char*>(header), 8);
		if (length != 0)
			file.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(length));
		std::vector<uint8_t> crc_feed;
		crc_feed.assign(header + 4, header + 8);
		crc_feed.insert(crc_feed.end(), data.begin(), data.end());
		const uint32_t crc = crc32_bytes(crc_feed.data(), crc_feed.size());
		uint8_t tail[4] = {
			static_cast<uint8_t>((crc >> 24) & 0xFFu), static_cast<uint8_t>((crc >> 16) & 0xFFu),
			static_cast<uint8_t>((crc >> 8) & 0xFFu), static_cast<uint8_t>(crc & 0xFFu),
		};
		file.write(reinterpret_cast<const char*>(tail), 4);
	}

	/*
	把 RGBA 像素写成 PNG（8 位、不压缩、逐行 filter 0）
	swapped 为真时按 BGRA → RGBA 交换首尾通道（交换链优先 B8G8R8A8，回读字节序随之）
	*/
	bool png_write_rgba(const std::filesystem::path& path, const std::vector<uint8_t>& pixels,
		uint32_t width, uint32_t height, bool swapped)
	{
		std::ofstream file(path, std::ios::binary);
		if (!file)
			return false;
		//原始扫描线：每行一个 filter 字节 ＋ 4 × 宽字节
		std::vector<uint8_t> raw;
		raw.reserve(static_cast<std::size_t>(height) * (1 + 4 * static_cast<std::size_t>(width)));
		for (uint32_t y = 0; y < height; ++y)
		{
			raw.push_back(0u);
			for (uint32_t x = 0; x < width; ++x)
			{
				const std::size_t offset = (static_cast<std::size_t>(y) * width + x) * 4;
				raw.push_back(swapped ? pixels[offset + 2] : pixels[offset]);
				raw.push_back(pixels[offset + 1]);
				raw.push_back(swapped ? pixels[offset] : pixels[offset + 2]);
				raw.push_back(pixels[offset + 3]);
			}
		}
		//PNG 签名
		const uint8_t signature[8] = { 0x89u, 0x50u, 0x4Eu, 0x47u, 0x0Du, 0x0Au, 0x1Au, 0x0Au };
		file.write(reinterpret_cast<const char*>(signature), 8);
		//IHDR：宽 / 高 / 8 位 / RGBA(6) / 压缩 0 / 过滤 0 / 隔行 0
		std::vector<uint8_t> ihdr = {
			static_cast<uint8_t>((width >> 24) & 0xFFu), static_cast<uint8_t>((width >> 16) & 0xFFu),
			static_cast<uint8_t>((width >> 8) & 0xFFu), static_cast<uint8_t>(width & 0xFFu),
			static_cast<uint8_t>((height >> 24) & 0xFFu), static_cast<uint8_t>((height >> 16) & 0xFFu),
			static_cast<uint8_t>((height >> 8) & 0xFFu), static_cast<uint8_t>(height & 0xFFu),
			8u, 6u, 0u, 0u, 0u,
		};
		png_chunk_write(file, "IHDR", ihdr);
		png_chunk_write(file, "IDAT", zlib_stored(raw));
		png_chunk_write(file, "IEND", {});
		return file.good();
	}

	/*
	回读通道序探测（清屏色置纯红后回读首像素）
	返回真表示字节序为 RGBA（首字节为红）；否则为 BGRA（末前字节为红）。
	*/
	bool probe_channel_order_swapped(void)
	{
		engine::Renderer probe;
		if (!probe.init(probe_config()) || !probe.readback_supported())
			return true;
		const float red[4] = { 1.0f, 0.0f, 0.0f, 1.0f };
		probe.clear_color_set(red);
		std::vector<uint8_t> pixels(probe_bytes());
		if (!probe.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())))
			return true;
		//首像素红通道位置：首字节更大则为 RGBA
		return pixels[0] < pixels[2];
	}

	//把方向按 Y 轴旋转（yaw，弧度）
	void direction_yaw_rotate(float dir[3], float radians)
	{
		const float cosine = std::cos(radians);
		const float sine = std::sin(radians);
		const float x = dir[0] * cosine + dir[2] * sine;
		const float z = -dir[0] * sine + dir[2] * cosine;
		dir[0] = x;
		dir[2] = z;
	}

	//把方向按「水平右向量」旋转（pitch，弧度；右向量取与世界上方叉积）
	void direction_pitch_rotate(float dir[3], float radians)
	{
		//水平右向量（与 Y 轴垂直）
		float right[3] = { dir[2], 0.0f, -dir[0] };
		const float length = std::sqrt(right[0] * right[0] + right[2] * right[2]);
		if (length <= 1.0e-6f)
			return;
		right[0] /= length;
		right[2] /= length;
		const float cosine = std::cos(radians);
		const float sine = std::sin(radians);
		//罗德里格斯旋转：dir' = dir·cos + (right × dir)·sin
		const float cross_x = -right[2] * dir[1];
		const float cross_y = right[2] * dir[0] - right[0] * dir[2];
		const float cross_z = right[0] * dir[1];
		dir[0] = dir[0] * cosine + cross_x * sine;
		dir[1] = dir[1] * cosine + cross_y * sine;
		dir[2] = dir[2] * cosine + cross_z * sine;
	}
}

/*
多距离 ＋ 多角度可见性（第四百零二轮）
展廊为 3 列（x = −14 / 0 / 14）× 8 排（z = −10 … −150），逐格扫描并输出覆盖像素数。
*/
TEST(Renderer_Showcase_Probe_Test, 各格多距离多角度均有显示)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(probe_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";

	//回读字节序（暗样本落盘 PNG 需要正确通道序）
	const bool swapped = probe_channel_order_swapped();
	//暗样本输出目录（每个展区格一张「最暗样本」PNG，便于回看后处理 / 灯光切换的影响）
	std::error_code code;
	const std::filesystem::path 暗样本目录 = std::filesystem::path(L"out") / std::filesystem::path(L"巡检测试暗样本");
	std::filesystem::create_directories(暗样本目录, code);

	engine::能力展示场景 scene;
	ASSERT_TRUE(scene.init(renderer));

	const float columns[3] = { -14.0f, 0.0f, 14.0f };
	//四种距离 ＋ 五种朝向（含侧视 90° 与背视 180°）＋ 高空俯视＝ 21 种组合
	const float distances[4] = { 12.0f, 20.0f, 30.0f, 50.0f };
	const float yaws[5] = { -20.0f, 0.0f, 20.0f, 90.0f, 180.0f };
	uint32_t empty_cases = 0;
	for (uint32_t row = 0; row < 8; ++row)
	{
		const float z = -10.0f - 20.0f * static_cast<float>(row);
		for (uint32_t column = 0; column < 3; ++column)
		{
			const float x = columns[column];
			uint32_t minimum_count = 0xFFFFFFFFu;
			uint32_t maximum_count = 0;
			float minimum_luma = 1.0e9f;
			//本格最暗样本的画面（循环结束后落盘）
			std::vector<uint8_t> 最暗画面;
			uint32_t 跳过样本 = 0;
			//四种距离 × 五种朝向 ＋ 高空俯视
			for (uint32_t sample = 0; sample < 21; ++sample)
			{
				if (sample < 20)
					aim_camera(scene, renderer, x, z, distances[sample / 5], yaws[sample % 5], false);
				else
					aim_camera(scene, renderer, x, z, 18.0f, 0.0f, true);
				//参考帧（同相机、只清屏 ＋ 天空）
				std::vector<uint8_t> reference(probe_bytes());
				ASSERT_TRUE(renderer.frame_render(reference.data(), static_cast<uint32_t>(reference.size())));
				//展示帧
				scene.frame_draw(renderer);
				std::vector<uint8_t> shown(probe_bytes());
				ASSERT_TRUE(renderer.frame_render(shown.data(), static_cast<uint32_t>(shown.size())));
				const uint32_t count = changed_pixels(reference, shown);
				const float luma = average_luma(shown);
				const float 近黑占比 = dark_fraction(shown);
				//相机贴进几何（整屏被暗面填满）：跳过，不计入「看不到展区」
				if (luma < 12.0f && 近黑占比 > 0.8f)
				{
					++跳过样本;
					continue;
				}
				if (count < minimum_count)
					minimum_count = count;
				if (count > maximum_count)
					maximum_count = count;
				if (luma < minimum_luma)
				{
					minimum_luma = luma;
					最暗画面 = shown;
				}
			}
			std::cout << "[展示排查] 格 (" << x << ", " << z << ") 覆盖 " << minimum_count
				<< " .. " << maximum_count << " 最低亮度 " << minimum_luma
				<< " 跳过(贴几何) " << 跳过样本 << std::endl;
			//最暗样本落盘（文件名用整数格坐标，便于与日志对照）
			if (!最暗画面.empty())
			{
				wchar_t 名称[64] = {};
				std::swprintf(名称, 64, L"格%+03d_%+04d_最暗.png", static_cast<int>(x), static_cast<int>(z));
				if (!png_write_rgba(暗样本目录 / std::filesystem::path(名称), 最暗画面,
					PROBE_WIDTH, PROBE_HEIGHT, swapped))
					std::cout << "[展示排查] 暗样本 PNG 写入失败" << std::endl;
			}
			//画面过暗也视为「看不清」（条形灯与地台都应有可见亮度）
			if (minimum_count < VISIBLE_MIN_PIXELS || minimum_luma < 18.0f)
				++empty_cases;
		}
	}
	EXPECT_EQ(empty_cases, 0u) << "存在显示覆盖不足的展区格";
	scene.shutdown(renderer);
}

/*
文字字形完整性（第四百零二轮）
把 23 条展区标题一次性绘制（22 px 字号），断言无码点被跳过。
*/
TEST(Renderer_Showcase_Probe_Test, 展区标题无缺字)
{
	std::error_code code;
	const char* candidates[] = {
		"C:\\Windows\\Fonts\\simhei.ttf",
		"C:\\Windows\\Fonts\\msyh.ttc",
	};
	std::string font_path;
	for (const char* candidate : candidates)
	{
		if (std::filesystem::exists(std::filesystem::path(candidate), code))
		{
			font_path = candidate;
			break;
		}
	}
	//本机无中文字体：跳过
	if (font_path.empty())
		GTEST_SKIP() << "本机无测试用中文字体";

	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(probe_config()));
	uint64_t font = 0;
	ASSERT_TRUE(renderer.font_load_file(font, font_path, 22u));
	engine::能力展示场景 scene;
	ASSERT_TRUE(scene.init(renderer));
	//23 条标题一次性绘制（长中文串连绘，验证图集扩容）
	for (uint32_t index = 0; index < scene.展区数量(); ++index)
		(void)renderer.text_draw(font, scene.展区标题(index), 4.0f, 4.0f + 24.0f * static_cast<float>(index));
	std::vector<uint8_t> pixels(probe_bytes());
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
	EXPECT_EQ(renderer.font_skipped_codepoints(font), 0u) << "展区标题存在被跳过的字形";
	renderer.font_unload(font);
	scene.shutdown(renderer);
}

/*
各展区随机位置与朝向均有显示（第四百二十一轮）
需求口径（用户提出）：
1. 每个展区附近「坐标随机生成」——眼位在以展区中心为圆心、半径 7..22 的随机方位与随机高度 1.2..7.0；
2. 「角度以该展区方向一定范围内随机」——先指向展区中心，再叠加 yaw ±15°、pitch ±6° 的随机偏转；
3. 「截图验证」——每个样本都把画面落盘为 PNG（`out/巡检测试随机/`），失败样本文件名加 `F_` 前缀；
4. 判定仍用数值口径（同相机空场景基线的差像素数 ＋ 整帧平均亮度），数值只用于回归断言，PNG 供人工看图复核。
每区 30 组 × 23 区 ＝ 690 样本；随机种子固定为 20261102，保证同一版本可复现。
*/
TEST(Renderer_Showcase_Probe_Test, 各展区随机位置与朝向均有显示)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(probe_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";

	//回读字节序（PNG 需要正确的通道序，否则红蓝互换）
	const bool swapped = probe_channel_order_swapped();

	uint32_t 展区数量 = 0;
	const engine::展示::展区项* 表 = engine::展示::展区表(展区数量);

	engine::能力展示场景 scene;
	ASSERT_TRUE(scene.init(renderer));

	//输出目录（相对可执行文件所在目录；全用宽字符路径，避免窄字符转换触发编码异常）
	std::error_code code;
	const std::filesystem::path 输出目录 = std::filesystem::path(L"out") / std::filesystem::path(L"巡检测试随机");
	std::filesystem::create_directories(输出目录, code);
	//样本清单（参数 → 文件名映射，便于对照 PNG）
	std::ofstream manifest(输出目录 / std::filesystem::path(L"样本清单.csv"), std::ios::binary);
	if (manifest)
		manifest << "展区,样本,眼x,眼y,眼z,距离,方位角,俯仰抖动,偏航抖动,差像素,亮度,文件名\n";

	const uint32_t 每区样本数 = 30;
	std::mt19937 随机(20261102u);
	std::uniform_real_distribution<float> 方位分布(0.0f, 6.2831853f);
	std::uniform_real_distribution<float> 距离分布(9.0f, 22.0f);
	std::uniform_real_distribution<float> 高度分布(1.2f, 7.0f);
	std::uniform_real_distribution<float> 偏航分布(-15.0f, 15.0f);
	std::uniform_real_distribution<float> 俯仰分布(-6.0f, 6.0f);

	uint32_t 失败样本 = 0;
	uint32_t 跳过样本 = 0;
	uint32_t 总样本 = 0;
	wchar_t 宽文件名[96] = {};
	char 文件名[96] = {};
	for (uint32_t 区 = 0; 区 < 展区数量; ++区)
	{
		const engine::展示::展区项& 项 = 表[区];
		uint32_t 最小覆盖 = 0xFFFFFFFFu;
		float 最低亮度 = 1.0e9f;
		uint32_t 本区失败 = 0;
		for (uint32_t 样本 = 0; 样本 < 每区样本数; ++样本)
		{
			//随机眼位：绕展区中心的一圈随机方位 ＋ 随机半径 ＋ 随机高度
			float 方位 = 0.0f;
			float 距离 = 0.0f;
			float 眼x = 0.0f;
			float 眼y = 0.0f;
			float 眼z = 0.0f;
			/*
			避让邻区几何：眼位若落在其它展区中心 6 单位内，容易嵌进灯墙 / 门框 / 地台（画面全黑），
			这类样本不代表「本区没显示」。重抽最多 4 次，仍不满足则接受最后一次。
			*/
			for (uint32_t 尝试 = 0; 尝试 < 4; ++尝试)
			{
				方位 = 方位分布(随机);
				距离 = 距离分布(随机);
				眼x = 项.x + std::sin(方位) * 距离;
				眼y = 高度分布(随机);
				眼z = 项.z + std::cos(方位) * 距离;
				//与其它展区中心的最近水平距离
				float 最近 = 1.0e9f;
				for (uint32_t 其它 = 0; 其它 < 展区数量; ++其它)
				{
					//跳过本区自身
					if (其它 == 区)
						continue;
					const float 差x = 眼x - 表[其它].x;
					const float 差z = 眼z - 表[其它].z;
					const float 平方 = 差x * 差x + 差z * 差z;
					if (平方 < 最近)
						最近 = 平方;
				}
				if (最近 >= 36.0f)
					break;
			}
			//基准方向：指向展区中心（略高，瞄准地台上方的展品）
			float 方向[3] = { 项.x - 眼x, 1.4f - 眼y, 项.z - 眼z };
			const float 长度 = std::sqrt(方向[0] * 方向[0] + 方向[1] * 方向[1] + 方向[2] * 方向[2]);
			if (长度 > 1.0e-6f)
			{
				方向[0] /= 长度;
				方向[1] /= 长度;
				方向[2] /= 长度;
			}
			//在「朝向展区」的范围内随机偏转（前两项为角度抖动，后两项为实际朝向偏转量）
			const float 偏航 = 偏航分布(随机);
			const float 俯仰 = 俯仰分布(随机);
			direction_yaw_rotate(方向, 偏航 * 3.14159265f / 180.0f);
			direction_pitch_rotate(方向, 俯仰 * 3.14159265f / 180.0f);
			const float 眼位[3] = { 眼x, 眼y, 眼z };
			apply_pose(scene, renderer, 眼位, 方向);

			//参考帧（同相机、只清屏 ＋ 天空）与展示帧
			std::vector<uint8_t> 参考(probe_bytes());
			ASSERT_TRUE(renderer.frame_render(参考.data(), static_cast<uint32_t>(参考.size())));
			scene.frame_draw(renderer);
			std::vector<uint8_t> 画面(probe_bytes());
			ASSERT_TRUE(renderer.frame_render(画面.data(), static_cast<uint32_t>(画面.size())));

			//覆盖像素与亮度
			const uint32_t 覆盖 = changed_pixels(参考, 画面);
			const float 亮度 = average_luma(画面);
			const float 近黑占比 = dark_fraction(画面);
			//相机贴进几何（整屏被暗面填满）：跳过，不计入失败
			const bool 贴几何 = 亮度 < 12.0f && 近黑占比 > 0.8f;
			if (贴几何)
			{
				++跳过样本;
				std::cout << "[随机巡查·跳过] 区 " << 项.编号 << " 样本 " << 样本
					<< " 眼位 (" << 眼x << ", " << 眼y << ", " << 眼z << ") 近黑占比 " << 近黑占比 << std::endl;
			}
			if (!贴几何)
			{
				if (覆盖 < 最小覆盖)
					最小覆盖 = 覆盖;
				if (亮度 < 最低亮度)
					最低亮度 = 亮度;
			}

			//判定：覆盖不足或画面过暗都算「没显示」（被几何贴脸的样本不计）
			const bool 通过 = !贴几何 && 覆盖 >= VISIBLE_MIN_PIXELS && 亮度 >= 18.0f;
			if (!通过)
			{
				++失败样本;
				++本区失败;
				std::cout << "[随机巡查·失败] 区 " << 项.编号 << " 样本 " << 样本
					<< " 眼位 (" << 眼x << ", " << 眼y << ", " << 眼z << ") 距离 " << 距离
					<< " 覆盖 " << 覆盖 << " 亮度 " << 亮度 << std::endl;
			}

			//落盘文件名（F_ = 失败、S_ = 跳过-贴几何、无前缀 = 通过；宽字符构造，日志另用窄字符编号）
			const char* 前缀 = 贴几何 ? "S_" : (通过 ? "" : "F_");
			const wchar_t* 宽前缀 = 贴几何 ? L"S_" : (通过 ? L"" : L"F_");
			std::swprintf(宽文件名, 96, L"%ls区%02u_样本%02u.png", 宽前缀, 项.编号, 样本);
			std::snprintf(文件名, sizeof(文件名), "%s%d_%d.png", 前缀, 项.编号, 样本);
			const std::filesystem::path 路径 = 输出目录 / std::filesystem::path(宽文件名);
			if (!png_write_rgba(路径, 画面, PROBE_WIDTH, PROBE_HEIGHT, swapped))
				std::cout << "[随机巡查] PNG 写入失败：" << 文件名 << std::endl;

			++总样本;
			//清单记录（角度与判定一并写出，便于复现单条样本）
			if (manifest)
				manifest << 项.编号 << "," << 样本 << "," << 眼x << "," << 眼y << "," << 眼z << ","
					<< 距离 << "," << 方位 << "," << 俯仰 << "," << 偏航 << ","
					<< 覆盖 << "," << 亮度 << "," << 文件名 << "\n";
		}
		std::cout << "[随机巡查] 区 " << 项.编号 << " 最小覆盖 " << 最小覆盖
			<< " 最低亮度 " << 最低亮度 << " 失败 " << 本区失败 << " / " << 每区样本数 << std::endl;
	}

	std::cout << "[随机巡查] 总样本 " << 总样本 << " 失败 " << 失败样本
		<< " 跳过(贴几何) " << 跳过样本 << " 输出目录 out/巡检测试随机" << std::endl;
	EXPECT_EQ(失败样本, 0u) << "存在未显示或过暗的随机样本（详见输出目录内 F_ 前缀 PNG）";
	scene.shutdown(renderer);
}

/*
区③ 八盏聚光瓦片（第五百三十一轮）
口径：相机进入展区③ 后进区开关开启簇光照（8 聚光 ＋ 3 点光共 11 盏超前向 8 盏上限），
断言本帧 `spot_shadow_tiles == 8`；离开区③ 回到展廊起点后簇光照关闭。
*/
TEST(Renderer_Showcase_Probe_Test, 区三八盏聚光瓦片)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(probe_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	if (!renderer.cluster_supported())
		GTEST_SKIP() << "本机不支持 Cluster 计算路径";

	engine::能力展示场景 scene;
	ASSERT_TRUE(scene.init(renderer));

	//进入区③（区中心 (14, -10)，眼位在半径 9 判定圈内）
	const float 眼位[3] = { 14.0f, 3.0f, -4.0f };
	const float 方向[3] = { 0.0f, -0.35f, -1.0f };
	apply_pose(scene, renderer, 眼位, 方向);
	EXPECT_TRUE(renderer.cluster_enable_get());
	scene.frame_draw(renderer);
	ASSERT_TRUE(renderer.frame_render());
	EXPECT_EQ(renderer.frame_stats_get().spot_shadow_tiles, 8u) << "区③ 未按八盏瓦片投影";

	//回到展廊起点：簇光照关闭
	const float 起点[3] = { 0.0f, 9.5f, 14.0f };
	const float 起点方向[3] = { 0.0f, -0.3f, -1.0f };
	apply_pose(scene, renderer, 起点, 起点方向);
	EXPECT_FALSE(renderer.cluster_enable_get());
	scene.frame_draw(renderer);
	ASSERT_TRUE(renderer.frame_render());
	scene.shutdown(renderer);
}
