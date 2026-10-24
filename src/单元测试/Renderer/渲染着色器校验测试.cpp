/*
渲染着色器校验测试（L1 无窗口）
覆盖：着色器目录缺失、着色器文件为空、字节码长度非 4 字节倍数、SPIR-V 魔数不匹配
      —— 四种异常着色器输入均应使初始化失败。
做法：在 out/ 下的临时目录（命名单用 ASCII，避免中文路径在本地码页下的解析歧义）
      伪造异常 .spv，设置 Renderer_Config::shader_dir 指向该目录，用例结束删除。
*/
#include <gtest/gtest.h>

//获取渲染器门面与配置
#include "Application/Renderer/src/render/Renderer/渲染器.h"
//文件系统与文件流
#include <filesystem>
#include <fstream>
#include <cstdint>
#include <string>
#include <system_error>

namespace
{
	//临时着色器根目录（相对当前工作目录，纯 ASCII 命名）
	const char* shader_probe_dir = "out/test_tmp/render_shader_check";
	//「不存在的着色器目录」路径（用于目录缺失用例）
	const char* shader_missing_dir = "out/test_tmp/render_shader_missing";
	//渲染层运行期加载的顶点着色器文件名
	const char* vertex_shader_name = "场景.vert.spv";

	//以二进制写入指定字节内容到文件
	bool write_bytes(const std::filesystem::path& path, const void* data, std::size_t size)
	{
		//以二进制、截断方式打开
		std::ofstream stream(path, std::ios::binary | std::ios::trunc);
		//若打开失败
		if (!stream.is_open())
			return false;
		//写出字节
		stream.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(size));
		stream.close();
		return true;
	}

	//构造隐藏窗口配置，并指定着色器目录
	engine::Renderer_Config shader_config(const std::string& shader_dir)
	{
		engine::Renderer_Config config;
		config.width = 320;
		config.height = 180;
		config.hidden = true;
		config.shader_dir = shader_dir;
		return config;
	}
}

//着色器校验测试夹具
class Renderer_Shader_Test : public ::testing::Test
{
protected:
	void SetUp() override
	{
		//清掉两处临时目录并重建着色器目录
		std::error_code code;
		std::filesystem::remove_all(shader_probe_dir, code);
		std::filesystem::remove_all(shader_missing_dir, code);
		std::filesystem::create_directories(shader_probe_dir, code);
	}
	void TearDown() override
	{
		//用例结束清理临时目录
		std::error_code code;
		std::filesystem::remove_all(shader_probe_dir, code);
		std::filesystem::remove_all(shader_missing_dir, code);
	}
};

//着色器目录不存在时初始化失败
TEST_F(Renderer_Shader_Test, 着色器目录缺失初始化失败)
{
	engine::Renderer renderer;
	//指向从未创建的目录
	EXPECT_FALSE(renderer.init(shader_config(shader_missing_dir)));
	EXPECT_FALSE(renderer.valid());
}

//着色器文件为空时初始化失败
TEST_F(Renderer_Shader_Test, 空着色器文件初始化失败)
{
	//创建零字节的顶点着色器
	const std::filesystem::path path = std::filesystem::path(shader_probe_dir) / vertex_shader_name;
	ASSERT_TRUE(write_bytes(path, nullptr, 0));
	engine::Renderer renderer;
	EXPECT_FALSE(renderer.init(shader_config(shader_probe_dir)));
	EXPECT_FALSE(renderer.valid());
}

//字节码长度非 4 字节倍数时初始化失败
TEST_F(Renderer_Shader_Test, 长度非四倍字节初始化失败)
{
	//六字节：含正确魔数但长度不是 4 的倍数
	const uint8_t bytes[6] = { 0x03, 0x02, 0x23, 0x07, 0x00, 0x00 };
	const std::filesystem::path path = std::filesystem::path(shader_probe_dir) / vertex_shader_name;
	ASSERT_TRUE(write_bytes(path, bytes, sizeof(bytes)));
	engine::Renderer renderer;
	EXPECT_FALSE(renderer.init(shader_config(shader_probe_dir)));
	EXPECT_FALSE(renderer.valid());
}

//SPIR-V 魔数不匹配时初始化失败
TEST_F(Renderer_Shader_Test, 魔数不匹配初始化失败)
{
	//八字节：长度为 4 的倍数但首字并非 SPIR-V 魔数
	const uint8_t bytes[8] = { 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x01 };
	const std::filesystem::path path = std::filesystem::path(shader_probe_dir) / vertex_shader_name;
	ASSERT_TRUE(write_bytes(path, bytes, sizeof(bytes)));
	engine::Renderer renderer;
	EXPECT_FALSE(renderer.init(shader_config(shader_probe_dir)));
	EXPECT_FALSE(renderer.valid());
}
