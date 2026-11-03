#include "src/scene/Manifest/局部命名空间使用.h"
//日志系统
#include "Engine/EngineCore/src/tools/Logging/日志系统运行包.h"
//UTF-8 字符串 → 文件系统路径
#include "Engine/EngineCore/src/tools/Detail/路径字符串转换.h"

//文件读取与行解析
#include <fstream>
#include <sstream>
//std::strtof（浮点字段解析，不抛异常）
#include <cmath>
#include <cstdlib>

namespace engine
{
	namespace
	{
		//清单文件体量上限（2 MiB）
		constexpr uint64_t MAX_FILE_BYTES = 2ull * 1024 * 1024;
		//摆放记录数量上限
		constexpr uint32_t MAX_PLACES = 20000u;

		//去掉首尾空白（路径可能带空格）
		string text_trimmed(const string& text)
		{
			const std::size_t begin = text.find_first_not_of(" \t\r\n");
			//全空白
			if (begin == string::npos)
				return string();
			const std::size_t end = text.find_last_not_of(" \t\r\n");
			return text.substr(begin, end - begin + 1);
		}

		//去掉行尾回车（Windows 文本）
		void line_trim_carriage_return(string& line)
		{
			if (!line.empty() && line.back() == '\r')
				line.pop_back();
		}

		//浮点解析（必须完整消费且为有限值）
		bool float_parse(const string& text, float& value)
		{
			//空串
			if (text.empty())
				return false;
			char* end = nullptr;
			const float parsed = std::strtof(text.c_str(), &end);
			//未完全消费（含尾随字符）或非有限值
			if (end == nullptr || *end != '\0' || !std::isfinite(parsed))
				return false;
			value = parsed;
			return true;
		}

		//读取「ID ＋ 余下整段作为路径」
		bool id_and_path_read(std::istringstream& stream, string& id_out, string& path_out)
		{
			//ID
			if (!(stream >> id_out))
				return false;
			//余下内容（路径可能含空格）
			string rest;
			std::getline(stream, rest);
			path_out = text_trimmed(rest);
			return !path_out.empty();
		}

		//清单清空（失败契约）
		void manifest_reset(Scene_Manifest& manifest)
		{
			manifest.meshes.clear();
			manifest.materials.clear();
			manifest.places.clear();
		}

		/*
		解析单行清单记录
		空行 / 注释行直接跳过；未知指令 / 字段缺失 / ID 重复 / 引用未知 ID 一律失败。
		*/
		bool manifest_line_parse(const string& line, Scene_Manifest& out,
			const vector<string>& 实例名表)
		{
			std::istringstream stream(line);
			string tag;
			//空行
			if (!(stream >> tag))
				return true;
			//注释行
			if (tag[0] == '#')
				return true;

			//网格引用
			if (tag == "mesh")
			{
				Scene_Mesh_Ref ref;
				if (!id_and_path_read(stream, ref.id, ref.path))
				{
					logger.warn("场景清单:: mesh 行缺字段");
					return false;
				}
				//ID 重复
				for (const Scene_Mesh_Ref& existing : out.meshes)
				{
					if (existing.id == ref.id)
					{
						logger.warn("场景清单:: mesh ID 重复（{}）", ref.id);
						return false;
					}
				}
				out.meshes.push_back(ref);
			}
			//材质库引用
			else if (tag == "mtl")
			{
				Scene_Mtl_Ref ref;
				if (!id_and_path_read(stream, ref.id, ref.path))
				{
					logger.warn("场景清单:: mtl 行缺字段");
					return false;
				}
				//ID 重复
				for (const Scene_Mtl_Ref& existing : out.materials)
				{
					if (existing.id == ref.id)
					{
						logger.warn("场景清单:: mtl ID 重复（{}）", ref.id);
						return false;
					}
				}
				out.materials.push_back(ref);
			}
			//摆放记录
			else if (tag == "place")
			{
				Scene_Place place;
				if (!(stream >> place.mesh_id))
				{
					logger.warn("场景清单:: place 行缺网格 ID");
					return false;
				}
				/*
				材质列可省略：下一个标记若能解析为数字，则视为坐标起始，材质 ID 留空。
				*/
				string token;
				if (stream >> token)
				{
					float probe = 0.0f;
					if (float_parse(token, probe))
						place.position[0] = probe;
					else
					{
						place.material_id = token;
						if (!(stream >> token) || !float_parse(token, place.position[0]))
						{
							logger.warn("场景清单:: place 行坐标非法");
							return false;
						}
					}
				}
				//余下两轴位置与三轴缩放
				{
					float values[5] = {};
					for (std::size_t index = 0; index < 5; ++index)
					{
						if (!(stream >> token) || !float_parse(token, values[index]))
						{
							logger.warn("场景清单:: place 行坐标非法");
							return false;
						}
					}
					place.position[1] = values[0];
					place.position[2] = values[1];
					place.scale[0] = values[2];
					place.scale[1] = values[3];
					place.scale[2] = values[4];
				}
				//可选三轴旋转（度）
				{
					string extra;
					if (stream >> extra)
					{
						float rotation[3] = {};
						if (!float_parse(extra, rotation[0])
							|| !(stream >> extra) || !float_parse(extra, rotation[1])
							|| !(stream >> extra) || !float_parse(extra, rotation[2]))
						{
							logger.warn("场景清单:: place 行旋转非法（需三个角度）");
							return false;
						}
						//旋转之后不允许多余字段
						if (stream >> extra)
						{
							logger.warn("场景清单:: place 行字段过多");
							return false;
						}
						place.rotation_deg[0] = rotation[0];
						place.rotation_deg[1] = rotation[1];
						place.rotation_deg[2] = rotation[2];
					}
				}
				//网格 ID 必须存在
				bool mesh_found = false;
				for (const Scene_Mesh_Ref& existing : out.meshes)
				{
					if (existing.id == place.mesh_id)
					{
						mesh_found = true;
						break;
					}
				}
				if (!mesh_found)
				{
					logger.warn("场景清单:: place 引用了未知网格（{}）", place.mesh_id);
					return false;
				}
				//材质列（若非空）须命中实例名或材质库 ID
				if (!place.material_id.empty())
				{
					//先查实例表
					bool instance_found = false;
					for (const string& existing : 实例名表)
					{
						if (existing == place.material_id)
						{
							instance_found = true;
							break;
						}
					}
					bool mtl_found = false;
					for (const Scene_Mtl_Ref& existing : out.materials)
					{
						if (existing.id == place.material_id)
						{
							mtl_found = true;
							break;
						}
					}
					//实例与材质库均未命中
					if (!instance_found && !mtl_found)
					{
						logger.warn("场景清单:: place 引用了未知材质或实例（{}）", place.material_id);
						return false;
					}
				}
				if (out.places.size() >= MAX_PLACES)
				{
					logger.warn("场景清单:: 摆放记录超限");
					return false;
				}
				out.places.push_back(place);
			}
			//未知指令
			else
			{
				logger.warn("场景清单:: 出现未知指令（{}）", tag);
				return false;
			}
			return true;
		}
	}

	//解析场景清单文件
	bool scene_manifest_parse(const string& path, Scene_Manifest& out,
		const vector<string>& 实例名表)
	{
		//先清空输出
		manifest_reset(out);
		//路径为空
		if (path.empty())
		{
			logger.warn("场景清单:: 路径为空");
			return false;
		}

		const std::filesystem::path file_path = detail::string_to_path(path);
		std::error_code code;
		const uintmax_t byte_count = std::filesystem::file_size(file_path, code);
		//文件不存在 / 不可读
		if (code)
		{
			logger.warn("场景清单:: 无法读取 ({})", path);
			return false;
		}
		//空文件或超限
		if (byte_count == 0 || byte_count > MAX_FILE_BYTES)
		{
			logger.warn("场景清单:: 体量非法（{} 字节，上限 {}）", byte_count, MAX_FILE_BYTES);
			return false;
		}

		std::ifstream file(file_path, std::ios::binary);
		if (!file)
		{
			logger.warn("场景清单:: 打开失败 ({})", path);
			return false;
		}

		/*
		失败清空守卫
		契约要求「失败时输出为空」；解析途中任何提前返回都要把已收集的内容清掉。
		*/
		struct 清空守卫
		{
			//被保护的清单
			Scene_Manifest& 清单;
			//是否成功（成功则不清空）
			bool 成功 = false;
			//析构：失败则清空
			~清空守卫(void)
			{
				if (!成功)
					manifest_reset(清单);
			}
		} 守卫{ out };

		string line;
		while (std::getline(file, line))
		{
			line_trim_carriage_return(line);
			//逐行解析
			if (!manifest_line_parse(line, out, 实例名表))
				return false;
		}

		//没有任何摆放记录
		if (out.places.empty())
		{
			logger.warn("场景清单:: 未包含任何 place ({})", path);
			return false;
		}
		//成功：守卫不再清空
		守卫.成功 = true;
		return true;
	}

	//按内存行解析场景清单
	bool scene_manifest_parse_lines(const vector<string>& lines, Scene_Manifest& out,
		const vector<string>& 实例名表)
	{
		//先清空输出
		manifest_reset(out);
		//没有任何行
		if (lines.empty())
		{
			logger.warn("场景清单:: 行集合为空");
			return false;
		}

		//失败清空守卫
		struct 清空守卫
		{
			//被保护的清单
			Scene_Manifest& 清单;
			//是否成功（成功则不清空）
			bool 成功 = false;
			//析构：失败则清空
			~清空守卫(void)
			{
				if (!成功)
					manifest_reset(清单);
			}
		} 守卫{ out };

		for (const string& line : lines)
		{
			//逐行解析
			if (!manifest_line_parse(line, out, 实例名表))
				return false;
		}

		//没有任何摆放记录
		if (out.places.empty())
		{
			logger.warn("场景清单:: 行集合未包含任何 place");
			return false;
		}
		守卫.成功 = true;
		return true;
	}
}
