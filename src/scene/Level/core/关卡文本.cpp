#include "src/scene/Level/局部命名空间使用.h"
//日志系统
#include "Engine/EngineCore/src/tools/Logging/日志系统运行包.h"
//UTF-8 字符串 → 文件系统路径
#include "Engine/EngineCore/src/tools/Detail/路径字符串转换.h"

//文件读写与行解析
#include <fstream>
#include <sstream>
//浮点文本输出（%.9g）与不抛异常解析
#include <cstdio>
#include <cstdlib>
//浮点有限性判定
#include <cmath>

namespace engine
{
	namespace
	{
		//关卡文件体量上限（2 MiB；与场景清单同量级）
		constexpr uint64_t MAX_FILE_BYTES = 2ull * 1024 * 1024;
		//点光源 / 聚光灯槽位上限（与本模块光照组数组容量一致）
		constexpr uint32_t MAX_POINT_LIGHTS = 8u;
		constexpr uint32_t MAX_SPOT_LIGHTS = 8u;
		//资产声明行上限
		constexpr uint32_t MAX_ASSET_LINES = 20000u;
		//材质实例声明上限（独立于资产行额度）
		constexpr uint32_t MAX_LEVEL_MATERIALS = 256u;

		/*
		去掉首尾空白
		材质名 / 资产路径可能带空格；本层不依赖渲染层的资产解析工具，故自带一份同语义实现。
		*/
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

		//浮点文本（9 位有效数字：二进制 32 位浮点可唯一还原）
		string number_text(float value)
		{
			char buffer[32] = {};
			std::snprintf(buffer, sizeof(buffer), "%.9g", value);
			return buffer;
		}

		//单浮点解析（必须完整消费且为有限值）
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

		//读取固定个数的浮点字段（不检查后续是否还有字段；供多段混合解析使用）
		bool floats_read_n(std::istringstream& stream, float* values, std::size_t count)
		{
			for (std::size_t index = 0; index < count; ++index)
			{
				string token;
				//缺字段
				if (!(stream >> token))
					return false;
				if (!float_parse(token, values[index]))
					return false;
			}
			return true;
		}

		//读取固定个数的浮点字段（缺字段或多余字段均视为错误）
		bool floats_read(std::istringstream& stream, float* values, std::size_t count)
		{
			if (!floats_read_n(stream, values, count))
				return false;
			//多余字段
			string extra;
			return !(stream >> extra);
		}

		//无符号整数解析（仅十进制数字）
		bool uint_parse(const string& text, uint32_t& value)
		{
			//空串
			if (text.empty())
				return false;
			for (char character : text)
			{
				//非数字
				if (character < '0' || character > '9')
					return false;
			}
			char* end = nullptr;
			const unsigned long long parsed = std::strtoull(text.c_str(), &end, 10);
			//未完全消费或超出 32 位范围
			if (end == nullptr || *end != '\0' || parsed > 4294967295ull)
				return false;
			value = static_cast<uint32_t>(parsed);
			return true;
		}

		//整数解析（可选负号）
		bool int_parse(const string& text, int& value)
		{
			//空串
			if (text.empty())
				return false;
			const std::size_t start = (text[0] == '-') ? 1 : 0;
			//只有负号
			if (start >= text.size())
				return false;
			for (std::size_t index = start; index < text.size(); ++index)
			{
				//非数字
				if (text[index] < '0' || text[index] > '9')
					return false;
			}
			char* end = nullptr;
			const long long parsed = std::strtoll(text.c_str(), &end, 10);
			//未完全消费或超出 int 范围
			if (end == nullptr || *end != '\0' || parsed < -2147483647ll - 1 || parsed > 2147483647ll)
				return false;
			value = static_cast<int>(parsed);
			return true;
		}

		//读取单个无符号整数字段（且不得有多余字段）
		bool uint_field_read(std::istringstream& stream, uint32_t& value)
		{
			string token;
			//缺字段
			if (!(stream >> token) || !uint_parse(token, value))
				return false;
			string extra;
			return !(stream >> extra);
		}

		//读取单个整数字段（且不得有多余字段）
		bool int_field_read(std::istringstream& stream, int& value)
		{
			string token;
			//缺字段
			if (!(stream >> token) || !int_parse(token, value))
				return false;
			string extra;
			return !(stream >> extra);
		}

		//写出「前缀 ＋ 若干浮点」一行
		void floats_write(std::ostream& file, const string& head, const float* values, std::size_t count)
		{
			file << head;
			for (std::size_t index = 0; index < count; ++index)
				file << ' ' << number_text(values[index]);
			file << '\n';
		}

		//写出「前缀 ＋ 单个整数」一行
		void integer_write(std::ostream& file, const string& head, long long value)
		{
			file << head << ' ' << value << '\n';
		}

		//重置输出（失败契约：输出为空）
		void level_reset(Level_Data& out)
		{
			out = Level_Data();
		}

		//单个浮点有限
		bool finite_one(float value)
		{
			return std::isfinite(value);
		}

		//若干浮点全部有限
		bool finite_many(const float* values, std::size_t count)
		{
			for (std::size_t index = 0; index < count; ++index)
			{
				//任一非有限即失败
				if (!finite_one(values[index]))
					return false;
			}
			return true;
		}

		/*
		写出前校验
		版本 ≥ 1；相机 / 光照 / 雾全部浮点（含有效光源槽位）有限。校验失败不落盘。
		*/
		bool level_write_validate(const Level_Data& level)
		{
			//版本
			if (level.version < 1)
				return false;
			const Camera_Data& camera = level.camera;
			if (!finite_many(camera.position, 3) || !finite_many(camera.target, 3)
				|| !finite_many(camera.up, 3) || !finite_many(&camera.fov, 1)
				|| !finite_many(&camera.near_plane, 1) || !finite_many(&camera.far_plane, 1)
				|| !finite_many(&camera.ortho_height, 1) || !finite_many(&camera.aspect, 1)
				|| !finite_many(camera.clear_color, 4))
				return false;

			const Light_Data& light = level.light;
			if (!finite_many(light.direction, 3) || !finite_many(light.color, 3)
				|| !finite_many(&light.intensity, 1) || !finite_many(&light.ambient, 1)
				|| !finite_many(&light.specular_intensity, 2)
				|| !finite_many(light.ambient_color, 3) || !finite_many(light.ambient_ground, 3)
				|| !finite_many(&light.ambient_hemisphere, 1)
				|| !finite_many(light.fill_direction, 3) || !finite_many(light.fill_color, 3)
				|| !finite_many(&light.fill_intensity, 1)
				|| !finite_many(&light.directional_specular, 2)
				|| !finite_many(&light.shadow_bias, 3)
				|| !finite_many(&light.pbr_roughness, 2)
				|| !finite_many(&light.toon_rim, 1) || !finite_many(light.toon_shadow_tint, 3)
				|| !finite_many(&light.toon_specular, 1)
				|| !finite_many(&light.outline_width, 1) || !finite_many(light.outline_color, 3))
				return false;

			/*
			聚光灯阴影质量参数
			偏置 0..0.05、软度 0..1、强度 0..1；越界 / 非数拒绝写出（读取侧同口径）。
			*/
			if (!finite_one(light.spot_shadow_bias) || light.spot_shadow_bias < 0.0f || light.spot_shadow_bias > 0.05f
				|| !finite_one(light.spot_shadow_softness) || light.spot_shadow_softness < 0.0f
				|| light.spot_shadow_softness > 1.0f
				|| !finite_one(light.spot_shadow_strength) || light.spot_shadow_strength < 0.0f
				|| light.spot_shadow_strength > 1.0f)
				return false;
			//聚光灯阴影序号（0..7）
			if (light.spot_shadow_index < 0 || light.spot_shadow_index > 7)
				return false;
			//多盏聚光灯阴影开关（0 / 1）
			if (light.spot_shadow_all < 0 || light.spot_shadow_all > 1)
				return false;
			//聚光灯阴影偏置自校准开关（0 / 1）
			if (light.spot_shadow_bias_auto < 0 || light.spot_shadow_bias_auto > 1)
				return false;
			/*
			逐盏聚光灯阴影质量缩放（第七百零八轮）
			软度缩放 0.25..4、强度缩放 0..1；越界 / 非数拒绝写出（读取侧同口径）。
			*/
			for (uint32_t index = 0; index < 8; ++index)
			{
				if (!finite_one(light.spot_shadow_softness_scale[index])
					|| light.spot_shadow_softness_scale[index] < 0.25f
					|| light.spot_shadow_softness_scale[index] > 4.0f)
					return false;
				if (!finite_one(light.spot_shadow_strength_scale[index])
					|| light.spot_shadow_strength_scale[index] < 0.0f
					|| light.spot_shadow_strength_scale[index] > 1.0f)
					return false;
			}

			//逐盏聚光灯阴影优先级（第七百零九轮；须有限，0 = 默认）
			for (uint32_t index = 0; index < 32; ++index)
				if (!finite_one(light.spot_shadow_priority[index]))
					return false;

			//有效点光槽位
			for (uint32_t index = 0; index < light.point_count && index < MAX_POINT_LIGHTS; ++index)
			{
				if (!finite_many(light.point_position[index], 3) || !finite_many(light.point_color[index], 3)
					|| !finite_one(light.point_radius[index]) || !finite_one(light.point_intensity[index])
					|| !finite_one(light.point_specular[index]) || !finite_one(light.point_shininess[index]))
					return false;
			}
			//有效聚光槽位
			for (uint32_t index = 0; index < light.spot_count && index < MAX_SPOT_LIGHTS; ++index)
			{
				if (!finite_many(light.spot_position[index], 3) || !finite_many(light.spot_direction[index], 3)
					|| !finite_many(light.spot_color[index], 3) || !finite_one(light.spot_radius[index])
					|| !finite_one(light.spot_intensity[index]) || !finite_one(light.spot_inner_deg[index])
					|| !finite_one(light.spot_outer_deg[index]) || !finite_one(light.spot_specular[index])
					|| !finite_one(light.spot_shininess[index]))
					return false;
			}

			const Fog_Data& fog = level.fog;
			if (!finite_many(fog.color, 3) || !finite_many(&fog.density, 1)
				|| !finite_many(&fog.start, 2) || !finite_many(&fog.height_falloff, 1))
				return false;
			//材质实例声明
			for (const Level_Material& material : level.materials)
			{
				//实例名 / 模板名须非空且不含空白（写出后仍可被逐列解析）
				if (material.实例名.empty() || material.模板.empty()
					|| material.实例名.find_first_of(" \t\r\n") != string::npos
					|| material.模板.find_first_of(" \t\r\n") != string::npos)
					return false;
				//覆盖底色四项须有限且在 [0,1]
				if (material.use_tint)
				{
					if (!finite_many(material.tint, 4))
						return false;
					for (std::size_t index = 0; index < 4; ++index)
						if (material.tint[index] < 0.0f || material.tint[index] > 1.0f)
							return false;
				}
				//覆盖自发光须有限且在 [0,1]
				if (material.use_emissive)
					if (!finite_one(material.emissive) || material.emissive < 0.0f || material.emissive > 1.0f)
						return false;
				//逐实例卡通：色阶 0..8、边缘光 / 高光阈值 0..1
				if (material.use_toon)
				{
					if (!finite_one(material.toon_steps) || material.toon_steps < 0.0f || material.toon_steps > 8.0f)
						return false;
					if (!finite_one(material.toon_rim) || material.toon_rim < 0.0f || material.toon_rim > 1.0f)
						return false;
					if (!finite_one(material.toon_specular) || material.toon_specular < 0.0f || material.toon_specular > 1.0f)
						return false;
				}
				//逐实例 PBR：粗糙度 (0.05, 4]、金属度 [0, 1]
				if (material.use_pbr)
				{
					if (!finite_one(material.pbr_roughness) || material.pbr_roughness <= 0.05f || material.pbr_roughness > 4.0f)
						return false;
					if (!finite_one(material.pbr_metallic) || material.pbr_metallic < 0.0f || material.pbr_metallic > 1.0f)
						return false;
				}
				//逐实例清漆：强度 (0, 1]、粗糙度 [0.03, 1]
				if (material.use_clearcoat)
				{
					if (!finite_one(material.clear_coat) || material.clear_coat <= 0.0f || material.clear_coat > 1.0f)
						return false;
					if (!finite_one(material.clear_coat_roughness) || material.clear_coat_roughness < 0.03f || material.clear_coat_roughness > 1.0f)
						return false;
				}
			}
			return true;
		}
	}

	//读取关卡文本
	bool level_text_read(const string& path, Level_Data& out)
	{
		//先清空输出
		level_reset(out);
		//路径为空
		if (path.empty())
		{
			logger.warn("关卡:: 关卡文件路径为空");
			return false;
		}

		const std::filesystem::path file_path = detail::string_to_path(path);
		std::error_code code;
		const uintmax_t byte_count = std::filesystem::file_size(file_path, code);
		//文件不存在 / 不可读
		if (code)
		{
			logger.warn("关卡:: 无法读取关卡文件 ({})", path);
			return false;
		}
		//空文件或超限
		if (byte_count == 0 || byte_count > MAX_FILE_BYTES)
		{
			logger.warn("关卡:: 关卡文件体量非法（{} 字节，上限 {}）", byte_count, MAX_FILE_BYTES);
			return false;
		}

		std::ifstream file(file_path, std::ios::binary);
		//打开失败
		if (!file)
		{
			logger.warn("关卡:: 关卡文件打开失败 ({})", path);
			return false;
		}

		/*
		失败清空守卫
		契约要求「失败时输出为空」；解析途中任何提前返回都要把已收集的内容清掉，
		故用一个小守卫在成功时置位，否则析构时清空。
		*/
		struct 清空守卫
		{
			//被保护的关卡数据
			Level_Data& 关卡;
			//是否成功（成功则不清空）
			bool 成功 = false;
			//析构：失败则清空
			~清空守卫(void)
			{
				if (!成功)
					level_reset(关卡);
			}
		} 守卫{ out };

		string line;
		while (std::getline(file, line))
		{
			line_trim_carriage_return(line);
			std::istringstream stream(line);
			string tag;
			//空行
			if (!(stream >> tag))
				continue;
			//注释行
			if (tag[0] == '#')
				continue;

			//关卡名
			if (tag == "level")
			{
				string rest;
				std::getline(stream, rest);
				out.name = text_trimmed(rest);
				continue;
			}
			//格式版本
			if (tag == "version")
			{
				int value = 0;
				if (!int_field_read(stream, value) || value != 1)
				{
					logger.warn("关卡:: 不支持的格式版本（{}）", path);
					return false;
				}
				out.version = value;
				continue;
			}
			//相机组
			if (tag == "camera")
			{
				string key;
				if (!(stream >> key))
				{
					logger.warn("关卡:: camera 行缺键 ({})", path);
					return false;
				}
				//逐键读取
				if (key == "position")
				{
					if (!floats_read(stream, out.camera.position, 3))
					{
						logger.warn("关卡:: camera position 字段非法");
						return false;
					}
				}
				else if (key == "target")
				{
					if (!floats_read(stream, out.camera.target, 3))
					{
						logger.warn("关卡:: camera target 字段非法");
						return false;
					}
				}
				else if (key == "up")
				{
					if (!floats_read(stream, out.camera.up, 3))
					{
						logger.warn("关卡:: camera up 字段非法");
						return false;
					}
				}
				else if (key == "clear")
				{
					if (!floats_read(stream, out.camera.clear_color, 4))
					{
						logger.warn("关卡:: camera clear 字段非法");
						return false;
					}
				}
				else if (key == "fov")
				{
					if (!floats_read(stream, &out.camera.fov, 1))
					{
						logger.warn("关卡:: camera fov 字段非法");
						return false;
					}
				}
				else if (key == "near")
				{
					if (!floats_read(stream, &out.camera.near_plane, 1))
					{
						logger.warn("关卡:: camera near 字段非法");
						return false;
					}
				}
				else if (key == "far")
				{
					if (!floats_read(stream, &out.camera.far_plane, 1))
					{
						logger.warn("关卡:: camera far 字段非法");
						return false;
					}
				}
				else if (key == "ortho_height")
				{
					if (!floats_read(stream, &out.camera.ortho_height, 1))
					{
						logger.warn("关卡:: camera ortho_height 字段非法");
						return false;
					}
				}
				else if (key == "aspect")
				{
					if (!floats_read(stream, &out.camera.aspect, 1))
					{
						logger.warn("关卡:: camera aspect 字段非法");
						return false;
					}
				}
				else if (key == "ortho")
				{
					uint32_t value = 0;
					if (!uint_field_read(stream, value) || value > 1)
					{
						logger.warn("关卡:: camera ortho 只能为 0 / 1");
						return false;
					}
					out.camera.ortho = (value != 0);
				}
				//未知相机键
				else
				{
					logger.warn("关卡:: camera 出现未知键（{}）", key);
					return false;
				}
				continue;
			}
			//光照组
			if (tag == "light")
			{
				string key;
				if (!(stream >> key))
				{
					logger.warn("关卡:: light 行缺键 ({})", path);
					return false;
				}
				//逐键读取
				if (key == "direction")
				{
					if (!floats_read(stream, out.light.direction, 3))
					{
						logger.warn("关卡:: light direction 字段非法");
						return false;
					}
				}
				else if (key == "color")
				{
					if (!floats_read(stream, out.light.color, 3))
					{
						logger.warn("关卡:: light color 字段非法");
						return false;
					}
				}
				else if (key == "intensity")
				{
					if (!floats_read(stream, &out.light.intensity, 1))
					{
						logger.warn("关卡:: light intensity 字段非法");
						return false;
					}
				}
				else if (key == "ambient")
				{
					if (!floats_read(stream, &out.light.ambient, 1))
					{
						logger.warn("关卡:: light ambient 字段非法");
						return false;
					}
				}
				else if (key == "specular")
				{
					float values[2] = {};
					if (!floats_read(stream, values, 2))
					{
						logger.warn("关卡:: light specular 字段非法");
						return false;
					}
					out.light.specular_intensity = values[0];
					out.light.specular_shininess = values[1];
				}
				else if (key == "attenuation_mode")
				{
					uint32_t value = 0;
					if (!uint_field_read(stream, value))
					{
						logger.warn("关卡:: light attenuation_mode 字段非法");
						return false;
					}
					out.light.attenuation_mode = value;
				}
				else if (key == "ambient_color")
				{
					if (!floats_read(stream, out.light.ambient_color, 3))
					{
						logger.warn("关卡:: light ambient_color 字段非法");
						return false;
					}
				}
				else if (key == "ambient_ground")
				{
					if (!floats_read(stream, out.light.ambient_ground, 3))
					{
						logger.warn("关卡:: light ambient_ground 字段非法");
						return false;
					}
				}
				else if (key == "ambient_hemisphere")
				{
					if (!floats_read(stream, &out.light.ambient_hemisphere, 1))
					{
						logger.warn("关卡:: light ambient_hemisphere 字段非法");
						return false;
					}
				}
				else if (key == "fill_direction")
				{
					if (!floats_read(stream, out.light.fill_direction, 3))
					{
						logger.warn("关卡:: light fill_direction 字段非法");
						return false;
					}
				}
				else if (key == "fill_color")
				{
					if (!floats_read(stream, out.light.fill_color, 3))
					{
						logger.warn("关卡:: light fill_color 字段非法");
						return false;
					}
				}
				else if (key == "fill_intensity")
				{
					if (!floats_read(stream, &out.light.fill_intensity, 1))
					{
						logger.warn("关卡:: light fill_intensity 字段非法");
						return false;
					}
				}
				else if (key == "specular_scale")
				{
					float values[2] = {};
					if (!floats_read(stream, values, 2))
					{
						logger.warn("关卡:: light specular_scale 字段非法");
						return false;
					}
					out.light.directional_specular = values[0];
					out.light.fill_specular = values[1];
				}
				else if (key == "shadow")
				{
					float values[3] = {};
					int resolution = 0;
					string token;
					uint32_t enabled = 0;
					//顺序：enabled bias softness strength resolution
					if (!(stream >> token) || !uint_parse(token, enabled) || enabled > 1
						|| !floats_read_n(stream, values, 3))
					{
						logger.warn("关卡:: light shadow 字段非法");
						return false;
					}
					if (!(stream >> token) || !int_parse(token, resolution))
					{
						logger.warn("关卡:: light shadow 分辨率非法");
						return false;
					}
					out.light.shadow_enabled = (enabled != 0) ? 1 : 0;
					out.light.shadow_bias = values[0];
					out.light.shadow_softness = values[1];
					out.light.shadow_strength = values[2];
					out.light.shadow_resolution = resolution;
				}
				/*
				聚光灯阴影开关与质量参数
				语法：`light spot_shadow <0|1> [<bias> <softness> <strength>]`
				三项可选：不写则沿用默认（0.010 / 0 / 1），写了必须正好三项且全部在范围内。
				*/
				else if (key == "spot_shadow")
				{
					uint32_t enabled = 0;
					string token;
					//开关（缺值 / 非数字 / 越界即失败）
					if (!(stream >> token) || !uint_parse(token, enabled) || enabled > 1)
					{
						logger.warn("关卡:: light spot_shadow 字段非法");
						return false;
					}
					out.light.spot_shadow_enabled = (enabled != 0) ? 1 : 0;
					//质量三项：要么没有，要么正好三项
					if (stream >> token)
					{
						float values[3] = {};
						//首项来自已读出的 token，其余两项顺序读取
						if (!float_parse(token, values[0]) || !floats_read_n(stream, values + 1, 2))
						{
							logger.warn("关卡:: light spot_shadow 质量参数缺值");
							return false;
						}
						//多余字段
						string extra;
						if (stream >> extra)
						{
							logger.warn("关卡:: light spot_shadow 质量参数多余字段");
							return false;
						}
						//范围：偏置 0..0.05、软度 0..1、强度 0..1
						if (!(values[0] >= 0.0f && values[0] <= 0.05f)
							|| !(values[1] >= 0.0f && values[1] <= 1.0f)
							|| !(values[2] >= 0.0f && values[2] <= 1.0f))
						{
							logger.warn("关卡:: light spot_shadow 质量参数越界");
							return false;
						}
						out.light.spot_shadow_bias = values[0];
						out.light.spot_shadow_softness = values[1];
						out.light.spot_shadow_strength = values[2];
					}
				}
				//聚光灯阴影序号（0..7，指向已登记光源槽位）
				else if (key == "spot_shadow_index")
				{
					uint32_t index = 0;
					//越界与多余字段由 uint_field_read 判定
					if (!uint_field_read(stream, index) || index > 7)
					{
						logger.warn("关卡:: light spot_shadow_index 字段非法");
						return false;
					}
					out.light.spot_shadow_index = static_cast<int>(index);
				}
				//多盏聚光灯阴影（0 / 1）
				else if (key == "spot_shadow_all")
				{
					uint32_t enabled = 0;
					//越界与多余字段由 uint_field_read 判定
					if (!uint_field_read(stream, enabled) || enabled > 1)
					{
						logger.warn("关卡:: light spot_shadow_all 字段非法");
						return false;
					}
					out.light.spot_shadow_all = static_cast<int>(enabled);
				}
				//聚光灯阴影偏置自校准（0 / 1）
				else if (key == "spot_shadow_bias_auto")
				{
					uint32_t enabled = 0;
					//越界与多余字段由 uint_field_read 判定
					if (!uint_field_read(stream, enabled) || enabled > 1)
					{
						logger.warn("关卡:: light spot_shadow_bias_auto 字段非法");
						return false;
					}
					out.light.spot_shadow_bias_auto = static_cast<int>(enabled);
				}
				/*
				逐盏聚光灯阴影质量缩放（第七百零八轮）
				语法：`light spot_shadow_quality <槽位 0..7> <软度缩放 0.25..4> <强度缩放 0..1>`，可重复多行。
				*/
				else if (key == "spot_shadow_quality")
				{
					uint32_t slot = 0;
					float values[2] = {};
					string token;
					//顺序：槽位 软度缩放 强度缩放
					if (!(stream >> token) || !uint_parse(token, slot) || slot > 7
						|| !floats_read(stream, values, 2))
					{
						logger.warn("关卡:: light spot_shadow_quality 字段非法");
						return false;
					}
					//范围：软度缩放 0.25..4、强度缩放 0..1
					if (!(values[0] >= 0.25f && values[0] <= 4.0f)
						|| !(values[1] >= 0.0f && values[1] <= 1.0f))
					{
						logger.warn("关卡:: light spot_shadow_quality 参数越界");
						return false;
					}
					out.light.spot_shadow_softness_scale[slot] = values[0];
					out.light.spot_shadow_strength_scale[slot] = values[1];
				}
				/*
				逐盏聚光灯阴影优先级（第七百零九轮）
				语法：`light spot_shadow_priority <槽位 0..31> <优先级>`，可重复多行；优先级须有限。
				*/
				else if (key == "spot_shadow_priority")
				{
					uint32_t slot = 0;
					float values[1] = {};
					string token;
					//顺序：槽位 优先级
					if (!(stream >> token) || !uint_parse(token, slot) || slot > 31
						|| !floats_read(stream, values, 1))
					{
						logger.warn("关卡:: light spot_shadow_priority 字段非法");
						return false;
					}
					out.light.spot_shadow_priority[slot] = values[0];
				}
				else if (key == "pbr")
				{
					uint32_t enabled = 0;
					float values[2] = {};
					string token;
					//顺序：enabled roughness metallic
					if (!(stream >> token) || !uint_parse(token, enabled) || enabled > 1
						|| !floats_read(stream, values, 2))
					{
						logger.warn("关卡:: light pbr 字段非法");
						return false;
					}
					out.light.pbr_enabled = (enabled != 0) ? 1 : 0;
					out.light.pbr_roughness = values[0];
					out.light.pbr_metallic = values[1];
				}
				else if (key == "toon")
				{
					float rim = 0.0f;
					float tint[3] = { 1.0f, 1.0f, 1.0f };
					float specular = 0.0f;
					uint32_t steps = 0;
					string token;
					//顺序：steps rim tint(3) specular
					if (!(stream >> token) || !uint_parse(token, steps) || !floats_read_n(stream, &rim, 1)
						|| !floats_read_n(stream, tint, 3) || !floats_read_n(stream, &specular, 1))
					{
						logger.warn("关卡:: light toon 字段非法");
						return false;
					}
					out.light.toon_steps = steps;
					out.light.toon_rim = rim;
					out.light.toon_shadow_tint[0] = tint[0];
					out.light.toon_shadow_tint[1] = tint[1];
					out.light.toon_shadow_tint[2] = tint[2];
					out.light.toon_specular = specular;
				}
				else if (key == "outline")
				{
					float values[4] = {};
					if (!floats_read(stream, values, 4))
					{
						logger.warn("关卡:: light outline 字段非法");
						return false;
					}
					out.light.outline_width = values[0];
					out.light.outline_color[0] = values[1];
					out.light.outline_color[1] = values[2];
					out.light.outline_color[2] = values[3];
				}
				else if (key == "point")
				{
					//槽位上限
					if (out.light.point_count >= MAX_POINT_LIGHTS)
					{
						logger.warn("关卡:: 点光源超过 {} 盏", MAX_POINT_LIGHTS);
						return false;
					}
					float values[10] = {};
					uint32_t attenuation = 0;
					string token;
					/*
					顺序：位置(3) 颜色(3) 半径 强度 衰减模式 高光缩放 光泽度
					衰减模式为整数，故先读 8 个浮点，再读整数，再读 2 个浮点。
					*/
					if (!floats_read_n(stream, values, 8)
						|| !(stream >> token) || !uint_parse(token, attenuation)
						|| !floats_read_n(stream, values + 8, 2))
					{
						logger.warn("关卡:: light point 字段非法");
						return false;
					}
					const uint32_t index = out.light.point_count;
					//位置与颜色
					for (uint32_t axis = 0; axis < 3; ++axis)
					{
						out.light.point_position[index][axis] = values[axis];
						out.light.point_color[index][axis] = values[3 + axis];
					}
					out.light.point_radius[index] = values[6];
					out.light.point_intensity[index] = values[7];
					out.light.point_attenuation[index] = attenuation;
					out.light.point_specular[index] = values[8];
					out.light.point_shininess[index] = values[9];
					out.light.point_count = index + 1;
				}
				else if (key == "spot")
				{
					//槽位上限
					if (out.light.spot_count >= MAX_SPOT_LIGHTS)
					{
						logger.warn("关卡:: 聚光灯超过 {} 盏", MAX_SPOT_LIGHTS);
						return false;
					}
					float values[16] = {};
					uint32_t attenuation = 0;
					string token;
					/*
					顺序：位置(3) 方向(3) 颜色(3) 半径 强度 内锥 外锥 衰减模式 高光缩放 光泽度
					衰减模式是第 14 个字段，故先读 13 个浮点（位置 3 ＋ 方向 3 ＋ 颜色 3 ＋ 半径 强度 内锥 外锥）。
					*/
					if (!floats_read_n(stream, values, 13)
						|| !(stream >> token) || !uint_parse(token, attenuation)
						|| !floats_read_n(stream, values + 14, 2))
					{
						logger.warn("关卡:: light spot 字段非法");
						return false;
					}
					const uint32_t index = out.light.spot_count;
					for (uint32_t axis = 0; axis < 3; ++axis)
					{
						out.light.spot_position[index][axis] = values[axis];
						out.light.spot_direction[index][axis] = values[3 + axis];
						out.light.spot_color[index][axis] = values[6 + axis];
					}
					out.light.spot_radius[index] = values[9];
					out.light.spot_intensity[index] = values[10];
					out.light.spot_inner_deg[index] = values[11];
					out.light.spot_outer_deg[index] = values[12];
					out.light.spot_attenuation[index] = attenuation;
					out.light.spot_specular[index] = values[14];
					out.light.spot_shininess[index] = values[15];
					out.light.spot_count = index + 1;
				}
				//未知光照键
				else
				{
					logger.warn("关卡:: light 出现未知键（{}）", key);
					return false;
				}
				continue;
			}
			//雾组
			if (tag == "fog")
			{
				string key;
				if (!(stream >> key))
				{
					logger.warn("关卡:: fog 行缺键 ({})", path);
					return false;
				}
				if (key == "color")
				{
					if (!floats_read(stream, out.fog.color, 3))
					{
						logger.warn("关卡:: fog color 字段非法");
						return false;
					}
				}
				else if (key == "density")
				{
					if (!floats_read(stream, &out.fog.density, 1))
					{
						logger.warn("关卡:: fog density 字段非法");
						return false;
					}
				}
				else if (key == "range")
				{
					float values[2] = {};
					if (!floats_read(stream, values, 2))
					{
						logger.warn("关卡:: fog range 字段非法");
						return false;
					}
					out.fog.start = values[0];
					out.fog.end = values[1];
				}
				else if (key == "mode")
				{
					uint32_t value = 0;
					if (!uint_field_read(stream, value))
					{
						logger.warn("关卡:: fog mode 字段非法");
						return false;
					}
					out.fog.mode = value;
				}
				else if (key == "height_falloff")
				{
					if (!floats_read(stream, &out.fog.height_falloff, 1))
					{
						logger.warn("关卡:: fog height_falloff 字段非法");
						return false;
					}
				}
				//未知雾键
				else
				{
					logger.warn("关卡:: fog 出现未知键（{}）", key);
					return false;
				}
				continue;
			}
			//材质实例声明（先于资产行解析）
			if (tag == "material")
			{
				string instance_name;
				string template_name;
				//缺实例名或模板材质名
				if (!(stream >> instance_name) || !(stream >> template_name))
				{
					logger.warn("关卡:: material 行缺实例名或模板材质名 ({})", path);
					return false;
				}
				//实例名重复
				for (const Level_Material& existing : out.materials)
				{
					if (existing.实例名 == instance_name)
					{
						logger.warn("关卡:: material 实例名重复（{}）", instance_name);
						return false;
					}
				}
				//数量上限
				if (out.materials.size() >= MAX_LEVEL_MATERIALS)
				{
					logger.warn("关卡:: 材质实例超限 ({})", path);
					return false;
				}
				Level_Material material;
				material.实例名 = instance_name;
				material.模板 = template_name;
				//可选覆盖项（顺序固定：tint → emissive → toon → pbr → clearcoat）
				string key;
				while (stream >> key)
				{
					//覆盖底色
					if (key == "tint")
					{
						//重复或顺序错误（后置覆盖项之后不得再出现 tint）
						if (material.use_tint || material.use_emissive || material.use_toon || material.use_pbr || material.use_clearcoat)
						{
							logger.warn("关卡:: material 覆盖项顺序非法（{}）", key);
							return false;
						}
						float values[4] = {};
						//缺字段
						if (!floats_read_n(stream, values, 4))
						{
							logger.warn("关卡:: material tint 字段非法");
							return false;
						}
						for (std::size_t index = 0; index < 4; ++index)
						{
							//非有限或越界
							if (!std::isfinite(values[index]) || values[index] < 0.0f || values[index] > 1.0f)
							{
								logger.warn("关卡:: material tint 数值越界");
								return false;
							}
							material.tint[index] = values[index];
						}
						material.use_tint = true;
						continue;
					}
					//覆盖自发光
					if (key == "emissive")
					{
						//重复覆盖或顺序错误（toon / pbr 之后不得再出现 emissive）
						if (material.use_emissive || material.use_toon || material.use_pbr || material.use_clearcoat)
						{
							logger.warn("关卡:: material 覆盖项重复（{}）", key);
							return false;
						}
						float value = 0.0f;
						//缺字段
						if (!floats_read_n(stream, &value, 1))
						{
							logger.warn("关卡:: material emissive 字段非法");
							return false;
						}
						//非有限或越界
						if (!std::isfinite(value) || value < 0.0f || value > 1.0f)
						{
							logger.warn("关卡:: material emissive 数值越界");
							return false;
						}
						material.emissive = value;
						material.use_emissive = true;
						continue;
					}
					//逐实例卡通（三值：色阶 / 边缘光 / 高光阈值）
					if (key == "toon")
					{
						//重复或顺序错误（清漆之后不得再出现 toon）
						if (material.use_toon || material.use_clearcoat)
						{
							logger.warn("关卡:: material 覆盖项重复（{}）", key);
							return false;
						}
						float values[3] = {};
						//缺字段
						if (!floats_read_n(stream, values, 3))
						{
							logger.warn("关卡:: material toon 字段非法");
							return false;
						}
						//非有限或越界（色阶 0..8、边缘光与高光 0..1）
						if (!std::isfinite(values[0]) || values[0] < 0.0f || values[0] > 8.0f
							|| !std::isfinite(values[1]) || values[1] < 0.0f || values[1] > 1.0f
							|| !std::isfinite(values[2]) || values[2] < 0.0f || values[2] > 1.0f)
						{
							logger.warn("关卡:: material toon 数值越界");
							return false;
						}
						material.toon_steps = values[0];
						material.toon_rim = values[1];
						material.toon_specular = values[2];
						material.use_toon = true;
						continue;
					}
					//逐实例 PBR（两值：粗糙度 / 金属度）
					if (key == "pbr")
					{
						//重复或顺序错误（清漆之后不得再出现 pbr）
						if (material.use_pbr || material.use_clearcoat)
						{
							logger.warn("关卡:: material 覆盖项重复（{}）", key);
							return false;
						}
						float values[2] = {};
						//缺字段
						if (!floats_read_n(stream, values, 2))
						{
							logger.warn("关卡:: material pbr 字段非法");
							return false;
						}
						//非有限或越界（粗糙度 (0.05, 4]、金属度 0..1）
						if (!std::isfinite(values[0]) || values[0] <= 0.05f || values[0] > 4.0f
							|| !std::isfinite(values[1]) || values[1] < 0.0f || values[1] > 1.0f)
						{
							logger.warn("关卡:: material pbr 数值越界");
							return false;
						}
						material.pbr_roughness = values[0];
						material.pbr_metallic = values[1];
						material.use_pbr = true;
						continue;
					}
					//逐实例清漆（两值：强度 / 粗糙度）
					if (key == "clearcoat")
					{
						//重复覆盖
						if (material.use_clearcoat)
						{
							logger.warn("关卡:: material 覆盖项重复（{}）", key);
							return false;
						}
						float values[2] = {};
						//缺字段
						if (!floats_read_n(stream, values, 2))
						{
							logger.warn("关卡:: material clearcoat 字段非法");
							return false;
						}
						//非有限或越界（强度 (0, 1]、粗糙度 0.03..1）
						if (!std::isfinite(values[0]) || values[0] <= 0.0f || values[0] > 1.0f
							|| !std::isfinite(values[1]) || values[1] < 0.03f || values[1] > 1.0f)
						{
							logger.warn("关卡:: material clearcoat 数值越界");
							return false;
						}
						material.clear_coat = values[0];
						material.clear_coat_roughness = values[1];
						material.use_clearcoat = true;
						continue;
					}
					//未知覆盖项
					logger.warn("关卡:: material 出现未知覆盖项（{}）", key);
					return false;
				}
				out.materials.push_back(material);
				continue;
			}
			//资产声明行（原样保留，装载阶段交给场景清单链路）
			if (tag == "mesh" || tag == "mtl" || tag == "place")
			{
				//mesh / mtl 需要「ID ＋ 路径」，place 需要「网格ID ＋ 至少一个字段」
				string first;
				if (!(stream >> first))
				{
					logger.warn("关卡:: 资产行缺字段（{}）", tag);
					return false;
				}
				if (tag != "place")
				{
					string rest;
					std::getline(stream, rest);
					if (text_trimmed(rest).empty())
					{
						logger.warn("关卡:: 资产行缺路径（{}）", tag);
						return false;
					}
				}
				else
				{
					string second;
					if (!(stream >> second))
					{
						logger.warn("关卡:: place 行缺坐标或材质（{}）", path);
						return false;
					}
				}
				//数量上限
				if (out.asset_lines.size() >= MAX_ASSET_LINES)
				{
					logger.warn("关卡:: 资产行超限 ({})", path);
					return false;
				}
				out.asset_lines.push_back(text_trimmed(line));
				continue;
			}

			//未知段
			logger.warn("关卡:: 出现未知段（{}）", tag);
			return false;
		}

		//成功：守卫不再清空
		守卫.成功 = true;
		return true;
	}

	//写出关卡文本
	bool level_text_write(const Level_Data& level, const string& path)
	{
		//路径为空
		if (path.empty())
		{
			logger.warn("关卡:: 写出路径为空");
			return false;
		}
		//关卡名含换行会破坏行式格式
		if (level.name.find('\n') != string::npos || level.name.find('\r') != string::npos)
		{
			logger.warn("关卡:: 关卡名含换行，拒绝写出");
			return false;
		}
		//写出前校验（版本 ≥ 1 且全部浮点有限）
		if (!level_write_validate(level))
		{
			logger.warn("关卡:: 写出前校验失败（版本 {} 或存在非有限字段）", level.version);
			return false;
		}

		const std::filesystem::path file_path = detail::string_to_path(path);
		std::error_code code;
		//自动创建父目录
		std::filesystem::create_directories(file_path.parent_path(), code);
		std::ofstream file(file_path, std::ios::binary | std::ios::trunc);
		//打开失败
		if (!file)
		{
			logger.warn("关卡:: 写出打开失败 ({})", path);
			return false;
		}

		file << "# 白银纪元 · 关卡文件（方向六；第四百二十九轮）\n";
		file << "level " << level.name << '\n';
		//格式版本
		file << "version " << level.version << '\n';

		//相机组
		floats_write(file, "camera position", level.camera.position, 3);
		floats_write(file, "camera target", level.camera.target, 3);
		floats_write(file, "camera up", level.camera.up, 3);
		floats_write(file, "camera fov", &level.camera.fov, 1);
		floats_write(file, "camera near", &level.camera.near_plane, 1);
		floats_write(file, "camera far", &level.camera.far_plane, 1);
		integer_write(file, "camera ortho", level.camera.ortho ? 1 : 0);
		floats_write(file, "camera ortho_height", &level.camera.ortho_height, 1);
		floats_write(file, "camera aspect", &level.camera.aspect, 1);
		floats_write(file, "camera clear", level.camera.clear_color, 4);

		//光照组
		floats_write(file, "light direction", level.light.direction, 3);
		floats_write(file, "light color", level.light.color, 3);
		floats_write(file, "light intensity", &level.light.intensity, 1);
		floats_write(file, "light ambient", &level.light.ambient, 1);
		{
			//镜面高光两项
			const float values[2] = { level.light.specular_intensity, level.light.specular_shininess };
			floats_write(file, "light specular", values, 2);
		}
		integer_write(file, "light attenuation_mode", static_cast<long long>(level.light.attenuation_mode));
		floats_write(file, "light ambient_color", level.light.ambient_color, 3);
		floats_write(file, "light ambient_ground", level.light.ambient_ground, 3);
		floats_write(file, "light ambient_hemisphere", &level.light.ambient_hemisphere, 1);
		floats_write(file, "light fill_direction", level.light.fill_direction, 3);
		floats_write(file, "light fill_color", level.light.fill_color, 3);
		floats_write(file, "light fill_intensity", &level.light.fill_intensity, 1);
		{
			//方向光 / 补光的高光缩放
			const float values[2] = { level.light.directional_specular, level.light.fill_specular };
			floats_write(file, "light specular_scale", values, 2);
		}
		{
			//阴影五项：开关为整数，其余为浮点，分辨率整数
			file << "light shadow " << (level.light.shadow_enabled != 0 ? 1 : 0)
				<< ' ' << number_text(level.light.shadow_bias)
				<< ' ' << number_text(level.light.shadow_softness)
				<< ' ' << number_text(level.light.shadow_strength)
				<< ' ' << level.light.shadow_resolution << '\n';
		}
		//聚光灯阴影开关与质量参数（恒写三项，旧版读取方可忽略尾部）
		{
			file << "light spot_shadow " << (level.light.spot_shadow_enabled != 0 ? 1 : 0)
				<< ' ' << number_text(level.light.spot_shadow_bias)
				<< ' ' << number_text(level.light.spot_shadow_softness)
				<< ' ' << number_text(level.light.spot_shadow_strength) << '\n';
		}
		//聚光灯阴影序号
		integer_write(file, "light spot_shadow_index", static_cast<long long>(level.light.spot_shadow_index));
		//多盏聚光灯阴影开关
		integer_write(file, "light spot_shadow_all", static_cast<long long>(level.light.spot_shadow_all));
		//聚光灯阴影偏置自校准开关
		integer_write(file, "light spot_shadow_bias_auto", static_cast<long long>(level.light.spot_shadow_bias_auto));
		/*
		逐盏聚光灯阴影质量缩放（第七百零八轮）
		只写非默认（软度 ≠ 1 或强度 ≠ 1）的槽位，默认槽位不产生行，保持旧关卡文本体积与内容不变。
		*/
		for (uint32_t index = 0; index < 8; ++index)
		{
			//默认槽位跳过
			if (level.light.spot_shadow_softness_scale[index] == 1.0f
				&& level.light.spot_shadow_strength_scale[index] == 1.0f)
				continue;
			file << "light spot_shadow_quality " << index
				<< ' ' << number_text(level.light.spot_shadow_softness_scale[index])
				<< ' ' << number_text(level.light.spot_shadow_strength_scale[index]) << '\n';
		}
		/*
		逐盏聚光灯阴影优先级（第七百零九轮）
		只写非 0 的槽位（默认 0 = 全部同分），保持旧关卡文本内容不变。
		*/
		for (uint32_t index = 0; index < 32; ++index)
		{
			//默认槽位跳过
			if (level.light.spot_shadow_priority[index] == 0.0f)
				continue;
			file << "light spot_shadow_priority " << index
				<< ' ' << number_text(level.light.spot_shadow_priority[index]) << '\n';
		}
		{
			//PBR-lite 三项
			file << "light pbr " << (level.light.pbr_enabled != 0 ? 1 : 0)
				<< ' ' << number_text(level.light.pbr_roughness)
				<< ' ' << number_text(level.light.pbr_metallic) << '\n';
		}
		{
			//卡通六项：色阶 边缘光 暗部色调 高光阈值
			file << "light toon " << level.light.toon_steps
				<< ' ' << number_text(level.light.toon_rim)
				<< ' ' << number_text(level.light.toon_shadow_tint[0])
				<< ' ' << number_text(level.light.toon_shadow_tint[1])
				<< ' ' << number_text(level.light.toon_shadow_tint[2])
				<< ' ' << number_text(level.light.toon_specular) << '\n';
		}
		{
			//外扩描边四项：宽度 ＋ 颜色
			file << "light outline " << number_text(level.light.outline_width)
				<< ' ' << number_text(level.light.outline_color[0])
				<< ' ' << number_text(level.light.outline_color[1])
				<< ' ' << number_text(level.light.outline_color[2]) << '\n';
		}

		//点光源（只写有效槽位）
		for (uint32_t index = 0; index < level.light.point_count && index < MAX_POINT_LIGHTS; ++index)
		{
			file << "light point " << number_text(level.light.point_position[index][0])
				<< ' ' << number_text(level.light.point_position[index][1])
				<< ' ' << number_text(level.light.point_position[index][2])
				<< ' ' << number_text(level.light.point_color[index][0])
				<< ' ' << number_text(level.light.point_color[index][1])
				<< ' ' << number_text(level.light.point_color[index][2])
				<< ' ' << number_text(level.light.point_radius[index])
				<< ' ' << number_text(level.light.point_intensity[index])
				<< ' ' << level.light.point_attenuation[index]
				<< ' ' << number_text(level.light.point_specular[index])
				<< ' ' << number_text(level.light.point_shininess[index])
				<< '\n';
		}
		//聚光灯（只写有效槽位）
		for (uint32_t index = 0; index < level.light.spot_count && index < MAX_SPOT_LIGHTS; ++index)
		{
			file << "light spot " << number_text(level.light.spot_position[index][0])
				<< ' ' << number_text(level.light.spot_position[index][1])
				<< ' ' << number_text(level.light.spot_position[index][2])
				<< ' ' << number_text(level.light.spot_direction[index][0])
				<< ' ' << number_text(level.light.spot_direction[index][1])
				<< ' ' << number_text(level.light.spot_direction[index][2])
				<< ' ' << number_text(level.light.spot_color[index][0])
				<< ' ' << number_text(level.light.spot_color[index][1])
				<< ' ' << number_text(level.light.spot_color[index][2])
				<< ' ' << number_text(level.light.spot_radius[index])
				<< ' ' << number_text(level.light.spot_intensity[index])
				<< ' ' << number_text(level.light.spot_inner_deg[index])
				<< ' ' << number_text(level.light.spot_outer_deg[index])
				<< ' ' << level.light.spot_attenuation[index]
				<< ' ' << number_text(level.light.spot_specular[index])
				<< ' ' << number_text(level.light.spot_shininess[index])
				<< '\n';
		}

		//雾组
		floats_write(file, "fog color", level.fog.color, 3);
		floats_write(file, "fog density", &level.fog.density, 1);
		{
			//起止距离
			const float values[2] = { level.fog.start, level.fog.end };
			floats_write(file, "fog range", values, 2);
		}
		integer_write(file, "fog mode", static_cast<long long>(level.fog.mode));
		floats_write(file, "fog height_falloff", &level.fog.height_falloff, 1);

		//材质实例声明（先于资产行写出）
		for (const Level_Material& material : level.materials)
		{
			file << "material " << material.实例名 << ' ' << material.模板;
			//覆盖底色
			if (material.use_tint)
				file << " tint " << number_text(material.tint[0]) << ' ' << number_text(material.tint[1])
					<< ' ' << number_text(material.tint[2]) << ' ' << number_text(material.tint[3]);
			//覆盖自发光
			if (material.use_emissive)
				file << " emissive " << number_text(material.emissive);
			//逐实例卡通
			if (material.use_toon)
				file << " toon " << number_text(material.toon_steps) << ' ' << number_text(material.toon_rim)
					<< ' ' << number_text(material.toon_specular);
			//逐实例 PBR
			if (material.use_pbr)
				file << " pbr " << number_text(material.pbr_roughness) << ' ' << number_text(material.pbr_metallic);
			//逐实例清漆
			if (material.use_clearcoat)
				file << " clearcoat " << number_text(material.clear_coat) << ' ' << number_text(material.clear_coat_roughness);
			file << '\n';
		}

		//资产声明行（原样）
		for (const string& asset_line : level.asset_lines)
			file << asset_line << '\n';

		file.flush();
		//写入失败
		if (!file.good())
		{
			logger.warn("关卡:: 写出失败 ({})", path);
			return false;
		}
		return true;
	}

	//查找关卡内材质实例声明（按实例名取首个匹配）
	bool level_material_find(const Level_Data& level, const string& name, Level_Material& out)
	{
		//若名称为空
		if (name.empty())
			return false;
		//逐个比对实例名
		for (const Level_Material& material : level.materials)
		{
			//若实例名一致
			if (material.实例名 == name)
			{
				out = material;
				return true;
			}
		}
		return false;
	}

	//更新关卡内材质实例声明（整体替换首个匹配项）
	bool level_material_update(Level_Data& level, const string& name, const Level_Material& data)
	{
		//若名称为空
		if (name.empty())
			return false;
		//逐个比对实例名
		for (Level_Material& material : level.materials)
		{
			//若实例名一致
			if (material.实例名 == name)
			{
				//整体替换（名称以关卡内为准）
				Level_Material 替换 = data;
				替换.实例名 = material.实例名;
				material = 替换;
				return true;
			}
		}
		return false;
	}

	//新增关卡内材质实例声明（追加到声明表末尾）
	bool level_material_add(Level_Data& level, const Level_Material& data)
	{
		//若实例名为空
		if (data.实例名.empty())
			return false;
		//若已达声明上限
		if (level.materials.size() >= MAX_LEVEL_MATERIALS)
			return false;
		//逐个比对实例名（同名拒绝）
		for (const Level_Material& material : level.materials)
		{
			//若实例名一致
			if (material.实例名 == data.实例名)
				return false;
		}
		level.materials.push_back(data);
		return true;
	}

	//删除关卡内材质实例声明（只删首个匹配项）
	bool level_material_remove(Level_Data& level, const string& name)
	{
		//若名称为空
		if (name.empty())
			return false;
		//按顺序定位首个匹配项
		for (auto it = level.materials.begin(); it != level.materials.end(); ++it)
		{
			//若实例名一致
			if (it->实例名 == name)
			{
				//删除该声明（其余顺序不变）
				level.materials.erase(it);
				return true;
			}
		}
		return false;
	}
}
