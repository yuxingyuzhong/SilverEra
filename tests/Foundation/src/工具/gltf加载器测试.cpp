//glTF 加载器测试：覆盖网格／节点／材质／纹理解析、交错布局、无索引与各类非法输入
#include <gtest/gtest.h>

//获取 glTF 加载器
#include "Engine/EngineFoundation/src/loader/assets/Gltf_Loader/gltf加载器.h"
//获取引擎环境
#include "Engine/EngineFoundation/src/detail/methods/文件路径获取.h"
//获取路径字符串转化方法
#include "Engine/EngineFoundation/src/detail/methods/路径字符串转换.h"

//临时文件所在目录（相对可执行文件目录）
static const std::string gltf_temp_dir = "tests/Foundation/src/工具/";

//写入文本文件，返回可读取的相对路径
static std::string gltf_text_write(const std::string& file_name, const std::string& content)
{
	//相对路径
	const std::string relative_path = gltf_temp_dir + file_name;
	//写入文件
	std::ofstream file(engine::detail::exe_dir_get() /
		engine::detail::string_to_path(relative_path), std::ios::binary);
	file.write(content.data(), static_cast<std::streamsize>(content.size()));
	file.close();
	return relative_path;
}

//写入二进制文件，返回可读取的相对路径
static std::string gltf_bin_write(const std::string& file_name,
	const std::vector<unsigned char>& bytes)
{
	//相对路径
	const std::string relative_path = gltf_temp_dir + file_name;
	//写入文件
	std::ofstream file(engine::detail::exe_dir_get() /
		engine::detail::string_to_path(relative_path), std::ios::binary);
	file.write(reinterpret_cast<const char*>(bytes.data()),
		static_cast<std::streamsize>(bytes.size()));
	file.close();
	return relative_path;
}

//清理临时文件
static void gltf_remove(const std::string& relative_path)
{
	//异常信息记录
	std::error_code ec;
	//删除文件
	std::filesystem::remove(engine::detail::exe_dir_get() /
		engine::detail::string_to_path(relative_path), ec);
}

//追加一个 float
static void bytes_float(std::vector<unsigned char>& out, float value)
{
	unsigned char buffer[sizeof(float)];
	std::memcpy(buffer, &value, sizeof(float));
	out.insert(out.end(), buffer, buffer + sizeof(float));
}

//追加一个 uint16
static void bytes_u16(std::vector<unsigned char>& out, uint16_t value)
{
	unsigned char buffer[sizeof(uint16_t)];
	std::memcpy(buffer, &value, sizeof(uint16_t));
	out.insert(out.end(), buffer, buffer + sizeof(uint16_t));
}

//按四字节补齐
static void bytes_align4(std::vector<unsigned char>& out)
{
	while (out.size() % 4u != 0u)
		out.push_back(0u);
}

//glTF 加载器测试夹具
class Gltf_Loader_Test : public ::testing::Test
{
};

//单三角网格：位置 / 法线 / 纹理坐标 / 索引 / 材质 / 纹理引用
TEST_F(Gltf_Loader_Test, 三角网格与材质解析)
{
	//二进制数据：三个顶点位置、法线、纹理坐标、三个索引
	std::vector<unsigned char> bytes;
	bytes_float(bytes, 0.0f); bytes_float(bytes, 0.0f); bytes_float(bytes, 0.0f);
	bytes_float(bytes, 1.0f); bytes_float(bytes, 0.0f); bytes_float(bytes, 0.0f);
	bytes_float(bytes, 0.0f); bytes_float(bytes, 1.0f); bytes_float(bytes, 0.0f);
	bytes_float(bytes, 0.0f); bytes_float(bytes, 0.0f); bytes_float(bytes, 1.0f);
	bytes_float(bytes, 0.0f); bytes_float(bytes, 0.0f); bytes_float(bytes, 1.0f);
	bytes_float(bytes, 0.0f); bytes_float(bytes, 0.0f); bytes_float(bytes, 1.0f);
	bytes_float(bytes, 0.0f); bytes_float(bytes, 0.0f);
	bytes_float(bytes, 1.0f); bytes_float(bytes, 0.0f);
	bytes_float(bytes, 0.0f); bytes_float(bytes, 1.0f);
	bytes_u16(bytes, 0u); bytes_u16(bytes, 1u); bytes_u16(bytes, 2u);
	bytes_align4(bytes);

	//临时二进制文件
	const std::string bin_path = gltf_bin_write("三角网格.bin", bytes);
	//临时 glTF 文件
	const std::string gltf_path = gltf_text_write("三角网格.gltf",
		"{\"asset\":{\"version\":\"2.0\"},"
		"\"scene\":0,"
		"\"scenes\":[{\"name\":\"主场景\",\"nodes\":[0]}],"
		"\"nodes\":[{\"name\":\"根\",\"mesh\":0,\"translation\":[1,2,3],\"scale\":[2,3,4]}],"
		"\"meshes\":[{\"name\":\"三角\",\"primitives\":[{"
		"\"attributes\":{\"POSITION\":0,\"NORMAL\":1,\"TEXCOORD_0\":2},"
		"\"indices\":3,\"material\":0}]}],"
		"\"materials\":[{\"name\":\"材质甲\","
		"\"pbrMetallicRoughness\":{\"baseColorFactor\":[0.8,0.1,0.2,1],"
		"\"metallicFactor\":0.25,\"roughnessFactor\":0.75},"
		"\"emissiveFactor\":[0,0.5,0],\"alphaMode\":\"MASK\",\"alphaCutoff\":0.3,"
		"\"doubleSided\":true,\"normalTexture\":{\"index\":0,\"scale\":2.0},"
		"\"occlusionTexture\":{\"index\":1,\"strength\":0.6},"
		"\"emissiveTexture\":{\"index\":2}}],"
		"\"textures\":[{\"source\":0,\"sampler\":0},{\"source\":1},{\"source\":0}],"
		"\"images\":[{\"uri\":\"甲.png\"},{\"uri\":\"乙.png\"}],"
		"\"samplers\":[{\"magFilter\":9729,\"minFilter\":9987,\"wrapS\":33071,\"wrapT\":10497}],"
		"\"buffers\":[{\"uri\":\"三角网格.bin\",\"byteLength\":"
		+ std::to_string(bytes.size()) + "}],"
		"\"bufferViews\":["
		"{\"buffer\":0,\"byteOffset\":0,\"byteLength\":36},"
		"{\"buffer\":0,\"byteOffset\":36,\"byteLength\":36},"
		"{\"buffer\":0,\"byteOffset\":72,\"byteLength\":24},"
		"{\"buffer\":0,\"byteOffset\":96,\"byteLength\":6}],"
		"\"accessors\":["
		"{\"bufferView\":0,\"componentType\":5126,\"count\":3,\"type\":\"VEC3\"},"
		"{\"bufferView\":1,\"componentType\":5126,\"count\":3,\"type\":\"VEC3\"},"
		"{\"bufferView\":2,\"componentType\":5126,\"count\":3,\"type\":\"VEC2\"},"
		"{\"bufferView\":3,\"componentType\":5123,\"count\":3,\"type\":\"SCALAR\"}]}");

	//加载结果
	engine::Gltf_Asset asset;
	ASSERT_TRUE(engine::Gltf_Loader::load_file(gltf_path, asset));

	//场景与默认场景
	ASSERT_EQ(asset.scenes.size(), 1u);
	EXPECT_EQ(asset.scenes[0].name, "主场景");
	ASSERT_EQ(asset.scenes[0].nodes.size(), 1u);
	EXPECT_EQ(asset.default_scene, 0);

	//节点：父子关系与 TRS 合成矩阵
	ASSERT_EQ(asset.nodes.size(), 1u);
	EXPECT_EQ(asset.nodes[0].name, "根");
	EXPECT_EQ(asset.nodes[0].mesh, 0);
	EXPECT_FLOAT_EQ(asset.nodes[0].matrix[0], 2.0f);
	EXPECT_FLOAT_EQ(asset.nodes[0].matrix[5], 3.0f);
	EXPECT_FLOAT_EQ(asset.nodes[0].matrix[10], 4.0f);
	EXPECT_FLOAT_EQ(asset.nodes[0].matrix[12], 1.0f);
	EXPECT_FLOAT_EQ(asset.nodes[0].matrix[13], 2.0f);
	EXPECT_FLOAT_EQ(asset.nodes[0].matrix[14], 3.0f);

	//网格与图元
	ASSERT_EQ(asset.meshes.size(), 1u);
	ASSERT_EQ(asset.meshes[0].primitives.size(), 1u);
	const engine::Mesh_Data& mesh = asset.meshes[0].primitives[0].data;
	ASSERT_EQ(mesh.vertices.size(), 3u);
	ASSERT_EQ(mesh.indices.size(), 3u);
	EXPECT_EQ(mesh.indices[0], 0u);
	EXPECT_EQ(mesh.indices[1], 1u);
	EXPECT_EQ(mesh.indices[2], 2u);
	//位置 / 法线 / 纹理坐标
	EXPECT_FLOAT_EQ(mesh.vertices[1].position[0], 1.0f);
	EXPECT_FLOAT_EQ(mesh.vertices[0].normal[2], 1.0f);
	EXPECT_FLOAT_EQ(mesh.vertices[1].uv[0], 1.0f);
	EXPECT_EQ(asset.meshes[0].primitives[0].material, 0);

	//材质字段
	ASSERT_EQ(asset.materials.size(), 1u);
	EXPECT_EQ(asset.materials[0].name, "材质甲");
	EXPECT_FLOAT_EQ(asset.materials[0].base_color[0], 0.8f);
	EXPECT_FLOAT_EQ(asset.materials[0].metallic, 0.25f);
	EXPECT_FLOAT_EQ(asset.materials[0].roughness, 0.75f);
	EXPECT_FLOAT_EQ(asset.materials[0].emissive[1], 0.5f);
	EXPECT_EQ(asset.materials[0].alpha_mode, 1);
	EXPECT_FLOAT_EQ(asset.materials[0].alpha_cutoff, 0.3f);
	EXPECT_TRUE(asset.materials[0].double_sided);
	EXPECT_EQ(asset.materials[0].normal_texture, 0);
	EXPECT_FLOAT_EQ(asset.materials[0].normal_scale, 2.0f);
	EXPECT_EQ(asset.materials[0].occlusion_texture, 1);
	EXPECT_FLOAT_EQ(asset.materials[0].occlusion_strength, 0.6f);
	EXPECT_EQ(asset.materials[0].emissive_texture, 2);

	//纹理 / 图像 / 采样器
	ASSERT_EQ(asset.textures.size(), 3u);
	EXPECT_EQ(asset.textures[0].source, 0);
	EXPECT_EQ(asset.textures[1].source, 1);
	EXPECT_EQ(asset.textures[1].sampler, -1);
	ASSERT_EQ(asset.images.size(), 2u);
	EXPECT_EQ(asset.images[0].uri, "甲.png");
	//外部 URI 图像按规范不带 mimeType
	EXPECT_TRUE(asset.images[0].mime_type.empty());
	EXPECT_EQ(asset.images[1].uri, "乙.png");
	ASSERT_EQ(asset.samplers.size(), 1u);
	EXPECT_EQ(asset.samplers[0].mag_filter, 9729);
	EXPECT_EQ(asset.samplers[0].wrap_s, 33071);

	//清理临时文件
	gltf_remove(gltf_path);
	gltf_remove(bin_path);
}

//节点层级与旋转：父节点下标与四元数合成的列主序矩阵
TEST_F(Gltf_Loader_Test, 节点层级与旋转矩阵)
{
	//临时 glTF 文件（父节点 0，子节点 1 绕 Z 轴旋转 90 度）
	const std::string gltf_path = gltf_text_write("节点层级.gltf",
		"{\"asset\":{\"version\":\"2.0\"},"
		"\"scenes\":[{\"nodes\":[0]}],"
		"\"nodes\":[{\"name\":\"父\",\"children\":[1]},"
		"{\"name\":\"子\",\"rotation\":[0,0,0.70710677,0.70710677],"
		"\"translation\":[1,2,3],\"scale\":[2,2,2]}],"
		"\"buffers\":[],\"bufferViews\":[],\"accessors\":[]}");

	//加载结果
	engine::Gltf_Asset asset;
	ASSERT_TRUE(engine::Gltf_Loader::load_file(gltf_path, asset));

	ASSERT_EQ(asset.nodes.size(), 2u);
	//父节点下标
	EXPECT_EQ(asset.nodes[1].parent, 0);
	EXPECT_EQ(asset.nodes[0].parent, -1);
	//子节点集合
	ASSERT_EQ(asset.nodes[0].children.size(), 1u);
	EXPECT_EQ(asset.nodes[0].children[0], 1);
	//旋转 90 度 + 缩放 2：X 轴映射到 Y 轴
	EXPECT_NEAR(asset.nodes[1].matrix[0], 0.0f, 1.0e-5f);
	EXPECT_NEAR(asset.nodes[1].matrix[1], 2.0f, 1.0e-5f);
	EXPECT_NEAR(asset.nodes[1].matrix[4], -2.0f, 1.0e-5f);
	EXPECT_NEAR(asset.nodes[1].matrix[5], 0.0f, 1.0e-5f);
	//平移写入末列
	EXPECT_FLOAT_EQ(asset.nodes[1].matrix[12], 1.0f);
	EXPECT_FLOAT_EQ(asset.nodes[1].matrix[13], 2.0f);
	EXPECT_FLOAT_EQ(asset.nodes[1].matrix[14], 3.0f);

	//清理临时文件
	gltf_remove(gltf_path);
}

//材质缺省值：只给名字时按 glTF 规范取默认值
TEST_F(Gltf_Loader_Test, 材质缺省值)
{
	//临时 glTF 文件
	const std::string gltf_path = gltf_text_write("材质缺省.gltf",
		"{\"asset\":{\"version\":\"2.0\"},"
		"\"materials\":[{\"name\":\"默认\"}],"
		"\"buffers\":[],\"bufferViews\":[],\"accessors\":[]}");

	//加载结果
	engine::Gltf_Asset asset;
	ASSERT_TRUE(engine::Gltf_Loader::load_file(gltf_path, asset));

	ASSERT_EQ(asset.materials.size(), 1u);
	//基础颜色缺省白
	EXPECT_FLOAT_EQ(asset.materials[0].base_color[0], 1.0f);
	EXPECT_FLOAT_EQ(asset.materials[0].base_color[3], 1.0f);
	//金属度 / 粗糙度缺省 1
	EXPECT_FLOAT_EQ(asset.materials[0].metallic, 1.0f);
	EXPECT_FLOAT_EQ(asset.materials[0].roughness, 1.0f);
	//自发光缺省黑
	EXPECT_FLOAT_EQ(asset.materials[0].emissive[0], 0.0f);
	//不透明 / 单面
	EXPECT_EQ(asset.materials[0].alpha_mode, 0);
	EXPECT_FALSE(asset.materials[0].double_sided);
	//贴图缺省
	EXPECT_EQ(asset.materials[0].base_color_texture, -1);
	EXPECT_EQ(asset.materials[0].normal_texture, -1);

	//清理临时文件
	gltf_remove(gltf_path);
}

//无索引：自动生成 0..n-1
TEST_F(Gltf_Loader_Test, 无索引自动生成)
{
	//二进制数据：三个顶点位置
	std::vector<unsigned char> bytes;
	bytes_float(bytes, 0.0f); bytes_float(bytes, 0.0f); bytes_float(bytes, 0.0f);
	bytes_float(bytes, 1.0f); bytes_float(bytes, 0.0f); bytes_float(bytes, 0.0f);
	bytes_float(bytes, 0.0f); bytes_float(bytes, 1.0f); bytes_float(bytes, 0.0f);

	//临时文件
	const std::string bin_path = gltf_bin_write("无索引.bin", bytes);
	const std::string gltf_path = gltf_text_write("无索引.gltf",
		"{\"asset\":{\"version\":\"2.0\"},"
		"\"meshes\":[{\"primitives\":[{\"attributes\":{\"POSITION\":0}}]}],"
		"\"buffers\":[{\"uri\":\"无索引.bin\",\"byteLength\":" + std::to_string(bytes.size()) + "}],"
		"\"bufferViews\":[{\"buffer\":0,\"byteOffset\":0,\"byteLength\":36}],"
		"\"accessors\":[{\"bufferView\":0,\"componentType\":5126,\"count\":3,\"type\":\"VEC3\"}]}");

	//加载结果
	engine::Gltf_Asset asset;
	ASSERT_TRUE(engine::Gltf_Loader::load_file(gltf_path, asset));

	ASSERT_EQ(asset.meshes.size(), 1u);
	ASSERT_EQ(asset.meshes[0].primitives.size(), 1u);
	ASSERT_EQ(asset.meshes[0].primitives[0].data.indices.size(), 3u);
	EXPECT_EQ(asset.meshes[0].primitives[0].data.indices[0], 0u);
	EXPECT_EQ(asset.meshes[0].primitives[0].data.indices[2], 2u);
	//缺法线时按三角形面积加权补齐（XY 平面三角形 → +Z）
	EXPECT_FLOAT_EQ(asset.meshes[0].primitives[0].data.vertices[0].normal[2], 1.0f);

	//清理临时文件
	gltf_remove(gltf_path);
	gltf_remove(bin_path);
}

//非三角面图元：被跳过且不阻断整体加载
TEST_F(Gltf_Loader_Test, 非三角面图元被跳过)
{
	//二进制数据：两个顶点
	std::vector<unsigned char> bytes;
	bytes_float(bytes, 0.0f); bytes_float(bytes, 0.0f); bytes_float(bytes, 0.0f);
	bytes_float(bytes, 1.0f); bytes_float(bytes, 0.0f); bytes_float(bytes, 0.0f);

	//临时文件（mode = 1 为 LINES）
	const std::string bin_path = gltf_bin_write("线段.bin", bytes);
	const std::string gltf_path = gltf_text_write("线段.gltf",
		"{\"asset\":{\"version\":\"2.0\"},"
		"\"meshes\":[{\"primitives\":[{\"attributes\":{\"POSITION\":0},\"mode\":1}]}],"
		"\"buffers\":[{\"uri\":\"线段.bin\",\"byteLength\":" + std::to_string(bytes.size()) + "}],"
		"\"bufferViews\":[{\"buffer\":0,\"byteOffset\":0,\"byteLength\":24}],"
		"\"accessors\":[{\"bufferView\":0,\"componentType\":5126,\"count\":2,\"type\":\"VEC3\"}]}");

	//加载结果
	engine::Gltf_Asset asset;
	ASSERT_TRUE(engine::Gltf_Loader::load_file(gltf_path, asset));

	ASSERT_EQ(asset.meshes.size(), 1u);
	EXPECT_TRUE(asset.meshes[0].primitives.empty());

	//清理临时文件
	gltf_remove(gltf_path);
	gltf_remove(bin_path);
}

//交错布局：byteStride 下位置与纹理坐标并存
TEST_F(Gltf_Loader_Test, 交错顶点布局)
{
	//二进制数据：每个顶点 3 位置 + 2 纹理坐标（步长 20）
	std::vector<unsigned char> bytes;
	bytes_float(bytes, 0.0f); bytes_float(bytes, 0.0f); bytes_float(bytes, 0.0f);
	bytes_float(bytes, 0.0f); bytes_float(bytes, 0.0f);
	bytes_float(bytes, 1.0f); bytes_float(bytes, 0.0f); bytes_float(bytes, 0.0f);
	bytes_float(bytes, 1.0f); bytes_float(bytes, 0.0f);
	bytes_float(bytes, 0.0f); bytes_float(bytes, 1.0f); bytes_float(bytes, 0.0f);
	bytes_float(bytes, 0.0f); bytes_float(bytes, 1.0f);
	bytes_u16(bytes, 0u); bytes_u16(bytes, 1u); bytes_u16(bytes, 2u);
	bytes_align4(bytes);

	//临时文件
	const std::string bin_path = gltf_bin_write("交错.bin", bytes);
	const std::string gltf_path = gltf_text_write("交错.gltf",
		"{\"asset\":{\"version\":\"2.0\"},"
		"\"meshes\":[{\"primitives\":[{\"attributes\":{\"POSITION\":0,\"TEXCOORD_0\":1},"
		"\"indices\":2}]}],"
		"\"buffers\":[{\"uri\":\"交错.bin\",\"byteLength\":" + std::to_string(bytes.size()) + "}],"
		"\"bufferViews\":["
		"{\"buffer\":0,\"byteOffset\":0,\"byteLength\":60,\"byteStride\":20},"
		"{\"buffer\":0,\"byteOffset\":60,\"byteLength\":6}],"
		"\"accessors\":["
		"{\"bufferView\":0,\"byteOffset\":0,\"componentType\":5126,\"count\":3,\"type\":\"VEC3\"},"
		"{\"bufferView\":0,\"byteOffset\":12,\"componentType\":5126,\"count\":3,\"type\":\"VEC2\"},"
		"{\"bufferView\":1,\"componentType\":5123,\"count\":3,\"type\":\"SCALAR\"}]}");

	//加载结果
	engine::Gltf_Asset asset;
	ASSERT_TRUE(engine::Gltf_Loader::load_file(gltf_path, asset));

	ASSERT_EQ(asset.meshes.size(), 1u);
	ASSERT_EQ(asset.meshes[0].primitives.size(), 1u);
	const engine::Mesh_Data& mesh = asset.meshes[0].primitives[0].data;
	ASSERT_EQ(mesh.vertices.size(), 3u);
	//第二个顶点的位置与纹理坐标
	EXPECT_FLOAT_EQ(mesh.vertices[1].position[0], 1.0f);
	EXPECT_FLOAT_EQ(mesh.vertices[1].uv[0], 1.0f);
	EXPECT_FLOAT_EQ(mesh.vertices[2].uv[1], 1.0f);

	//清理临时文件
	gltf_remove(gltf_path);
	gltf_remove(bin_path);
}

//索引越界：整体失败且输出清空
TEST_F(Gltf_Loader_Test, 索引越界时加载失败)
{
	//二进制数据：三个顶点 + 一个越界索引
	std::vector<unsigned char> bytes;
	bytes_float(bytes, 0.0f); bytes_float(bytes, 0.0f); bytes_float(bytes, 0.0f);
	bytes_float(bytes, 1.0f); bytes_float(bytes, 0.0f); bytes_float(bytes, 0.0f);
	bytes_float(bytes, 0.0f); bytes_float(bytes, 1.0f); bytes_float(bytes, 0.0f);
	bytes_u16(bytes, 0u); bytes_u16(bytes, 1u); bytes_u16(bytes, 5u);
	bytes_align4(bytes);

	//临时文件
	const std::string bin_path = gltf_bin_write("越界索引.bin", bytes);
	const std::string gltf_path = gltf_text_write("越界索引.gltf",
		"{\"asset\":{\"version\":\"2.0\"},"
		"\"meshes\":[{\"primitives\":[{\"attributes\":{\"POSITION\":0},\"indices\":1}]}],"
		"\"buffers\":[{\"uri\":\"越界索引.bin\",\"byteLength\":" + std::to_string(bytes.size()) + "}],"
		"\"bufferViews\":["
		"{\"buffer\":0,\"byteOffset\":0,\"byteLength\":36},"
		"{\"buffer\":0,\"byteOffset\":36,\"byteLength\":6}],"
		"\"accessors\":["
		"{\"bufferView\":0,\"componentType\":5126,\"count\":3,\"type\":\"VEC3\"},"
		"{\"bufferView\":1,\"componentType\":5123,\"count\":3,\"type\":\"SCALAR\"}]}");

	//加载结果（先写入脏数据，验证失败时被清空）
	engine::Gltf_Asset asset;
	asset.nodes.push_back(engine::Gltf_Node{});
	EXPECT_FALSE(engine::Gltf_Loader::load_file(gltf_path, asset));
	EXPECT_TRUE(asset.nodes.empty());
	EXPECT_TRUE(asset.meshes.empty());

	//清理临时文件
	gltf_remove(gltf_path);
	gltf_remove(bin_path);
}

//访问器越界：bufferView 声明长度不足时整体失败
TEST_F(Gltf_Loader_Test, 访问器越界时加载失败)
{
	//二进制数据：只有一个顶点（12 字节）
	std::vector<unsigned char> bytes;
	bytes_float(bytes, 0.0f); bytes_float(bytes, 0.0f); bytes_float(bytes, 0.0f);

	//临时文件（访问器声明三个顶点）
	const std::string bin_path = gltf_bin_write("越界访问器.bin", bytes);
	const std::string gltf_path = gltf_text_write("越界访问器.gltf",
		"{\"asset\":{\"version\":\"2.0\"},"
		"\"meshes\":[{\"primitives\":[{\"attributes\":{\"POSITION\":0}}]}],"
		"\"buffers\":[{\"uri\":\"越界访问器.bin\",\"byteLength\":" + std::to_string(bytes.size()) + "}],"
		"\"bufferViews\":[{\"buffer\":0,\"byteOffset\":0,\"byteLength\":12}],"
		"\"accessors\":[{\"bufferView\":0,\"componentType\":5126,\"count\":3,\"type\":\"VEC3\"}]}");

	//加载结果
	engine::Gltf_Asset asset;
	EXPECT_FALSE(engine::Gltf_Loader::load_file(gltf_path, asset));
	EXPECT_TRUE(asset.meshes.empty());

	//清理临时文件
	gltf_remove(gltf_path);
	gltf_remove(bin_path);
}

//非法输入：空路径 / 不存在 / 空文件 / 非 JSON 均失败
TEST_F(Gltf_Loader_Test, 非法输入加载失败)
{
	//加载结果
	engine::Gltf_Asset asset;
	//空路径
	EXPECT_FALSE(engine::Gltf_Loader::load_file("", asset));
	EXPECT_TRUE(asset.nodes.empty());
	//不存在的文件
	EXPECT_FALSE(engine::Gltf_Loader::load_file(
		"tests/Foundation/src/工具/不存在的模型.gltf", asset));
	EXPECT_TRUE(asset.nodes.empty());
	//空文件
	const std::string empty_path = gltf_text_write("空模型.gltf", "");
	EXPECT_FALSE(engine::Gltf_Loader::load_file(empty_path, asset));
	EXPECT_TRUE(asset.nodes.empty());
	//非 JSON 内容
	const std::string bad_path = gltf_text_write("非法模型.gltf", "not a gltf");
	EXPECT_FALSE(engine::Gltf_Loader::load_file(bad_path, asset));
	EXPECT_TRUE(asset.nodes.empty());

	//清理临时文件
	gltf_remove(empty_path);
	gltf_remove(bad_path);
}

//重复加载：同一文件两次加载结果一致
TEST_F(Gltf_Loader_Test, 重复加载结果一致)
{
	//二进制数据：单三角
	std::vector<unsigned char> bytes;
	bytes_float(bytes, 0.0f); bytes_float(bytes, 0.0f); bytes_float(bytes, 0.0f);
	bytes_float(bytes, 1.0f); bytes_float(bytes, 0.0f); bytes_float(bytes, 0.0f);
	bytes_float(bytes, 0.0f); bytes_float(bytes, 1.0f); bytes_float(bytes, 0.0f);
	bytes_u16(bytes, 0u); bytes_u16(bytes, 1u); bytes_u16(bytes, 2u);
	bytes_align4(bytes);

	//临时文件
	const std::string bin_path = gltf_bin_write("重复加载.bin", bytes);
	const std::string gltf_path = gltf_text_write("重复加载.gltf",
		"{\"asset\":{\"version\":\"2.0\"},"
		"\"meshes\":[{\"primitives\":[{\"attributes\":{\"POSITION\":0},\"indices\":1}]}],"
		"\"buffers\":[{\"uri\":\"重复加载.bin\",\"byteLength\":" + std::to_string(bytes.size()) + "}],"
		"\"bufferViews\":["
		"{\"buffer\":0,\"byteOffset\":0,\"byteLength\":36},"
		"{\"buffer\":0,\"byteOffset\":36,\"byteLength\":6}],"
		"\"accessors\":["
		"{\"bufferView\":0,\"componentType\":5126,\"count\":3,\"type\":\"VEC3\"},"
		"{\"bufferView\":1,\"componentType\":5123,\"count\":3,\"type\":\"SCALAR\"}]}");

	//两次加载结果
	engine::Gltf_Asset first;
	engine::Gltf_Asset second;
	ASSERT_TRUE(engine::Gltf_Loader::load_file(gltf_path, first));
	ASSERT_TRUE(engine::Gltf_Loader::load_file(gltf_path, second));

	//网格数量与顶点 / 索引一致
	ASSERT_EQ(first.meshes.size(), second.meshes.size());
	ASSERT_EQ(first.meshes[0].primitives.size(), second.meshes[0].primitives.size());
	const engine::Mesh_Data& first_mesh = first.meshes[0].primitives[0].data;
	const engine::Mesh_Data& second_mesh = second.meshes[0].primitives[0].data;
	ASSERT_EQ(first_mesh.vertices.size(), second_mesh.vertices.size());
	ASSERT_EQ(first_mesh.indices, second_mesh.indices);
	//逐个顶点按位置分量比对
	for (size_t index = 0; index < first_mesh.vertices.size(); ++index)
		EXPECT_FLOAT_EQ(first_mesh.vertices[index].position[0],
			second_mesh.vertices[index].position[0]);

	//清理临时文件
	gltf_remove(gltf_path);
	gltf_remove(bin_path);
}
