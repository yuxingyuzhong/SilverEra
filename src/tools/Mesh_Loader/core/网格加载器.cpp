#include "src/tools/Mesh_Loader/局部命名空间使用.h"
//路径字符串转换（UTF-8 → 文件系统路径）
#include "src/tools/Detail/路径字符串转换.h"
//日志系统
#include "src/tools/Logging/日志系统运行包.h"

//文件读取与容器
#include <array>
#include <unordered_map>
#include <fstream>
#include <sstream>
#include <cmath>
namespace engine
{
	namespace
	{
		//文件体量上限（64 MiB；与 EngineCore 的 OBJ 加载器同口径）
		constexpr uint64_t MAX_FILE_BYTES = 64ull * 1024 * 1024;
		//属性数组（v / vt / vn 各一份）数量上限
		constexpr uint32_t MAX_ATTRIBUTES = 4000000u;
		//索引数量上限（约 400 万三角形）
		constexpr uint32_t MAX_INDICES = 12000000u;

		//去掉首尾空白（材质名 / 贴图路径可能带空格）
		std::string trimmed(const std::string& text)
		{
			const std::size_t begin = text.find_first_not_of(" \t\r\n");
			//全空白
			if (begin == std::string::npos)
				return std::string();
			const std::size_t end = text.find_last_not_of(" \t\r\n");
			return text.substr(begin, end - begin + 1);
		}

		//十进制整数解析（失败返回假；不接受浮点 / 空串 / 越界）
		bool integer_parse(const std::string& text, int64_t& value)
		{
			//空串直接失败
			if (text.empty())
				return false;
			std::size_t index = 0;
			bool negative = false;
			//可选正负号
			if (text[0] == '+' || text[0] == '-')
			{
				negative = text[0] == '-';
				index = 1;
			}
			//只有符号没有数字
			if (index >= text.size())
				return false;
			int64_t result = 0;
			for (; index < text.size(); ++index)
			{
				const char ch = text[index];
				//非数字字符
				if (ch < '0' || ch > '9')
					return false;
				result = result * 10 + (ch - '0');
				//上限保护（远小于 int64 上限，避免溢出）
				if (result > 1000000000)
					return false;
			}
			value = negative ? -result : result;
			return true;
		}

		/*
		索引字段解析
		返回 1 起算的正索引下标；负数按「自末尾起算」换算；越界返回假。
		*/
		bool index_field_resolve(const std::string& text, int64_t count, int64_t& index_out)
		{
			int64_t value = 0;
			if (!integer_parse(text, value))
				return false;
			//负索引：自末尾起算
			if (value < 0)
				value = count + value;
			//正索引：OBJ 自 1 起算
			else
				value -= 1;
			//越界
			if (value < 0 || value >= count)
				return false;
			index_out = value;
			return true;
		}

		//顶点唯一键：位置 / 纹理坐标 / 法线三个下标（−1 表示该属性缺省）
		struct Vertex_Key
		{
			int64_t position = -1;
			int64_t texcoord = -1;
			int64_t normal = -1;
		};

		//顶点键哈希（三项混合，避免同序号不同组合相撞）
		struct Vertex_Key_Hash
		{
			std::size_t operator()(const Vertex_Key& key) const
			{
				const uint64_t a = static_cast<uint64_t>(key.position + 1);
				const uint64_t b = static_cast<uint64_t>(key.texcoord + 1);
				const uint64_t c = static_cast<uint64_t>(key.normal + 1);
				uint64_t hash = a * 0x9E3779B97F4A7C15ull;
				hash ^= b * 0xC2B2AE3D27D4EB4Full;
				hash ^= c * 0x165667B19E3779F9ull;
				hash ^= hash >> 29;
				return static_cast<std::size_t>(hash);
			}
		};

		//顶点键相等
		struct Vertex_Key_Equal
		{
			bool operator()(const Vertex_Key& left, const Vertex_Key& right) const
			{
				return left.position == right.position
					&& left.texcoord == right.texcoord
					&& left.normal == right.normal;
			}
		};

		/*
		面顶点标记解析（v / v/vt / v//vn / v/vt/vn）
		位置索引必须合法；纹理坐标与法线索引缺省写 −1。
		*/
		bool face_token_parse(const std::string& token, int64_t position_count,
			int64_t texcoord_count, int64_t normal_count, Vertex_Key& key_out)
		{
			//按斜杠切分（最多三段：v / vt / vn）
			std::string parts[3];
			std::size_t part_count = 0;
			std::size_t begin = 0;
			for (std::size_t index = 0; index <= token.size(); ++index)
			{
				//遇到斜杠或行尾即切一段
				if (index == token.size() || token[index] == '/')
				{
					if (part_count < 3)
						parts[part_count++] = token.substr(begin, index - begin);
					begin = index + 1;
				}
			}
			//位置索引必须存在且合法
			int64_t position = 0;
			if (!index_field_resolve(parts[0], position_count, position))
				return false;
			key_out.position = position;
			//纹理坐标索引（缺省或空段表示无）
			if (part_count >= 2 && !parts[1].empty())
			{
				int64_t texcoord = 0;
				if (!index_field_resolve(parts[1], texcoord_count, texcoord))
					return false;
				key_out.texcoord = texcoord;
			}
			//法线索引（缺省或空段表示无）
			if (part_count >= 3 && !parts[2].empty())
			{
				int64_t normal = 0;
				if (!index_field_resolve(parts[2], normal_count, normal))
					return false;
				key_out.normal = normal;
			}
			return true;
		}

		//三维向量叉积（a × b）
		std::array<float, 3> cross_product(const std::array<float, 3>& a, const std::array<float, 3>& b)
		{
			return {
				a[1] * b[2] - a[2] * b[1],
				a[2] * b[0] - a[0] * b[2],
				a[0] * b[1] - a[1] * b[0],
			};
		}

		/*
		补全缺失法线（按三角形面积加权）
		OBJ 未写 vn 时，顶点法线取相邻三角形叉积之和（叉积模长即两倍面积，天然带权）；
		累加结果为零向量（退化面）时回落 +Z，保证着色器不会拿到零法线。
		*/
		void normals_fill(Mesh_Data& data)
		{
			//是否存在缺法线顶点
			bool missing = false;
			for (const Vertex& vertex : data.vertices)
			{
				if (vertex.normal[0] == 0.0f && vertex.normal[1] == 0.0f && vertex.normal[2] == 0.0f)
				{
					missing = true;
					break;
				}
			}
			//全部已有法线：不做事
			if (!missing)
				return;
			//逐顶点累加法线（初始为零）
			std::vector<std::array<float, 3>> accumulated(data.vertices.size());
			for (std::size_t index = 0; index + 2 < data.indices.size(); index += 3)
			{
				const Vertex& a = data.vertices[data.indices[index]];
				const Vertex& b = data.vertices[data.indices[index + 1]];
				const Vertex& c = data.vertices[data.indices[index + 2]];
				const std::array<float, 3> edge1 = { b.position[0] - a.position[0],
					b.position[1] - a.position[1], b.position[2] - a.position[2] };
				const std::array<float, 3> edge2 = { c.position[0] - a.position[0],
					c.position[1] - a.position[1], c.position[2] - a.position[2] };
				const std::array<float, 3> face = cross_product(edge1, edge2);
				//三个顶点各累加一次
				for (std::size_t corner = 0; corner < 3; ++corner)
				{
					std::array<float, 3>& target = accumulated[data.indices[index + corner]];
					target[0] += face[0];
					target[1] += face[1];
					target[2] += face[2];
				}
			}
			//写回缺法线的顶点
			for (std::size_t index = 0; index < data.vertices.size(); ++index)
			{
				Vertex& vertex = data.vertices[index];
				//已有法线：保持
				if (vertex.normal[0] != 0.0f || vertex.normal[1] != 0.0f || vertex.normal[2] != 0.0f)
					continue;
				const std::array<float, 3>& sum = accumulated[index];
				const float length = std::sqrt(sum[0] * sum[0] + sum[1] * sum[1] + sum[2] * sum[2]);
				//退化（零长度）：回落 +Z
				if (length <= 1.0e-6f)
				{
					vertex.normal[0] = 0.0f;
					vertex.normal[1] = 0.0f;
					vertex.normal[2] = 1.0f;
					continue;
				}
				vertex.normal[0] = sum[0] / length;
				vertex.normal[1] = sum[1] / length;
				vertex.normal[2] = sum[2] / length;
			}
		}
	}

	//从 OBJ 文件加载网格
	bool Mesh_Loader::load_file(const std::string& path, Mesh_File_Data& out)
	{
		//先清空输出（任何失败路径都保持空）
		out.data.vertices.clear();
		out.data.indices.clear();
		out.sections.clear();
		out.names.clear();

		//路径为空
		if (path.empty())
		{
			logger.warn("网格加载器:: OBJ 路径为空");
			return false;
		}

		const std::filesystem::path file_path = detail::string_to_path(path);
		std::error_code code;
		const uintmax_t byte_count = std::filesystem::file_size(file_path, code);
		//文件不存在 / 不可读
		if (code)
		{
			logger.warn("网格加载器:: 无法读取 OBJ 文件 ({})", path);
			return false;
		}
		//空文件或超限
		if (byte_count == 0 || byte_count > MAX_FILE_BYTES)
		{
			logger.warn("网格加载器:: OBJ 体量非法（{} 字节，上限 {}）", byte_count, MAX_FILE_BYTES);
			return false;
		}

		std::ifstream file(file_path, std::ios::binary);
		if (!file)
		{
			logger.warn("网格加载器:: OBJ 打开失败 ({})", path);
			return false;
		}

		//原始属性数组
		std::vector<std::array<float, 3>> positions;
		std::vector<std::array<float, 2>> texcoords;
		std::vector<std::array<float, 3>> normals;
		//顶点去重表（(v,vt,vn) → 本层顶点下标）
		std::unordered_map<Vertex_Key, uint32_t, Vertex_Key_Hash, Vertex_Key_Equal> dedup;
		dedup.reserve(4096u);

		//当前段的起点索引与材质名
		uint32_t section_start = 0;
		std::string section_name;

		std::string line;
		while (std::getline(file, line))
		{
			//去掉行尾回车（Windows 文本）
			if (!line.empty() && line.back() == '\r')
				line.pop_back();
			std::istringstream stream(line);
			std::string tag;
			//空行
			if (!(stream >> tag))
				continue;
			//注释行
			if (tag[0] == '#')
				continue;

			//顶点位置
			if (tag == "v")
			{
				float x = 0.0f;
				float y = 0.0f;
				float z = 0.0f;
				if (!(stream >> x >> y >> z))
				{
					logger.warn("网格加载器:: OBJ 顶点行格式非法 ({})", path);
					return false;
				}
				if (positions.size() >= MAX_ATTRIBUTES)
				{
					logger.warn("网格加载器:: OBJ 顶点数量超限 ({})", path);
					return false;
				}
				positions.push_back({ x, y, z });
			}
			//纹理坐标
			else if (tag == "vt")
			{
				float u = 0.0f;
				float v = 0.0f;
				//部分导出器只写一个分量
				if (!(stream >> u))
				{
					logger.warn("网格加载器:: OBJ 纹理坐标行格式非法 ({})", path);
					return false;
				}
				stream >> v;
				if (texcoords.size() >= MAX_ATTRIBUTES)
				{
					logger.warn("网格加载器:: OBJ 纹理坐标数量超限 ({})", path);
					return false;
				}
				texcoords.push_back({ u, v });
			}
			//法线
			else if (tag == "vn")
			{
				float x = 0.0f;
				float y = 0.0f;
				float z = 0.0f;
				if (!(stream >> x >> y >> z))
				{
					logger.warn("网格加载器:: OBJ 法线行格式非法 ({})", path);
					return false;
				}
				if (normals.size() >= MAX_ATTRIBUTES)
				{
					logger.warn("网格加载器:: OBJ 法线数量超限 ({})", path);
					return false;
				}
				normals.push_back({ x, y, z });
			}
			//材质切换
			else if (tag == "usemtl")
			{
				std::string name;
				std::getline(stream, name);
				name = trimmed(name);
				/*
				收尾上一段（只有确实吃到面时才建段）
				段序号即材质槽，故只在建段时推进；空段（该材质没有面）直接丢弃，名字不占序号。
				*/
				if (!out.data.indices.empty() && section_start < out.data.indices.size())
				{
					Mesh_Section section;
					section.first_index = section_start;
					section.index_count = static_cast<uint32_t>(out.data.indices.size()) - section_start;
					section.material = static_cast<uint32_t>(out.sections.size());
					out.sections.push_back(section);
					out.names.push_back(section_name);
					section_start = static_cast<uint32_t>(out.data.indices.size());
				}
				//后续面归属新材质
				section_name = name;
			}
			//面
			else if (tag == "f")
			{
				//本面的顶点下标集合
				std::vector<uint32_t> face;
				std::string token;
				while (stream >> token)
				{
					Vertex_Key key;
					if (!face_token_parse(token, static_cast<int64_t>(positions.size()),
						static_cast<int64_t>(texcoords.size()), static_cast<int64_t>(normals.size()), key))
					{
						logger.warn("网格加载器:: OBJ 面索引非法 ({})：{}", path, token);
						return false;
					}
					//查重
					auto found = dedup.find(key);
					if (found != dedup.end())
					{
						face.push_back(found->second);
						continue;
					}
					//新顶点：拼装属性
					Vertex vertex{};
					const std::array<float, 3>& position = positions[static_cast<std::size_t>(key.position)];
					vertex.position[0] = position[0];
					vertex.position[1] = position[1];
					vertex.position[2] = position[2];
					//纹理坐标缺省为 (0,0)
					if (key.texcoord >= 0)
					{
						const std::array<float, 2>& texcoord = texcoords[static_cast<std::size_t>(key.texcoord)];
						vertex.uv[0] = texcoord[0];
						vertex.uv[1] = texcoord[1];
					}
					//法线缺省为零向量（后续统一补全）
					if (key.normal >= 0)
					{
						const std::array<float, 3>& normal = normals[static_cast<std::size_t>(key.normal)];
						vertex.normal[0] = normal[0];
						vertex.normal[1] = normal[1];
						vertex.normal[2] = normal[2];
					}
					if (out.data.vertices.size() >= MAX_ATTRIBUTES)
					{
						logger.warn("网格加载器:: OBJ 顶点数量超限 ({})", path);
						return false;
					}
					const uint32_t vertex_index = static_cast<uint32_t>(out.data.vertices.size());
					out.data.vertices.push_back(vertex);
					dedup.emplace(key, vertex_index);
					face.push_back(vertex_index);
				}
				//面顶点不足三个
				if (face.size() < 3)
				{
					logger.warn("网格加载器:: OBJ 面顶点不足三个 ({})", path);
					return false;
				}
				//多边形扇形三角化
				for (std::size_t corner = 1; corner + 1 < face.size(); ++corner)
				{
					if (out.data.indices.size() + 3 > MAX_INDICES)
					{
						logger.warn("网格加载器:: OBJ 索引数量超限 ({})", path);
						return false;
					}
					out.data.indices.push_back(face[0]);
					out.data.indices.push_back(face[corner]);
					out.data.indices.push_back(face[corner + 1]);
				}
			}
			//其余行（mtllib / o / g / s 等）忽略
		}

		//无有效三角面
		if (out.data.indices.empty())
		{
			logger.warn("网格加载器:: OBJ 未包含有效三角面 ({})", path);
			return false;
		}

		//收尾最后一段（若末段没有面则不建段）
		if (section_start < out.data.indices.size())
		{
			Mesh_Section last;
			last.first_index = section_start;
			last.index_count = static_cast<uint32_t>(out.data.indices.size()) - section_start;
			last.material = static_cast<uint32_t>(out.sections.size());
			out.sections.push_back(last);
			out.names.push_back(section_name);
		}

		//补全缺失法线
		normals_fill(out.data);
		return true;
	}

	//加载 OBJ 网格文件（仅顶点与索引）
	bool Mesh_Loader::load_obj(const std::string& mesh_path, Mesh_Data& receiver)
	{
		//先清空输出（失败契约）
		receiver.vertices.clear();
		receiver.indices.clear();
		//完整解析（分段信息丢弃）
		Mesh_File_Data file_data;
		if (!load_file(mesh_path, file_data))
			return false;
		//只取顶点与索引
		receiver.vertices = std::move(file_data.data.vertices);
		receiver.indices = std::move(file_data.data.indices);
		return true;
	}

}