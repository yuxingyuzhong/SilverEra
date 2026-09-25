#include "../局部命名空间使用.h"
//获取数据校验器
#include "src/tools/Data_Validator/数据校验器.h"
//获取引擎环境
#include "src/tools/Engine_Env/引擎环境.h"
//获取路径字符串转化方法
#include "src/tools/Auxi_Algorithm/路径字符串转换.h"
//获取日志系统
#include "src/tools/Logging/日志系统.h"

namespace engine
{
	//加载OBJ网格文件
	bool Mesh_Loader::load_obj(const string& mesh_path, Mesh_Data& receiver)
	{
		//清空输出
		receiver.vertices.clear();
		receiver.indices.clear();

		//若路径字符串为空
		if (mesh_path.empty())
		{
			Log::warn("Mesh_Loader::网格路径为空");
			return false;
		}

		//拼可执行文件目录下的绝对路径
		path absolute_path = Engine_Env::absolute_path_get(mesh_path);

		//实际读取路径
		path read_path;
		//若原样路径有效（按当前工作目录解释）
		if (Data_Validator::path_check(mesh_path))
			read_path = string_to_path(mesh_path);
		//若可执行文件目录下的路径有效
		else if (Data_Validator::path_check(absolute_path))
			read_path = absolute_path;
		//若两者均无效
		else
		{
			Log::warn("Mesh_Loader::网格路径不可读取({})", mesh_path);
			return false;
		}

		//异常信息记录
		error_code ec;
		//获取文件体量
		uintmax_t body_size = file_size(read_path, ec);
		//若体量查询失败
		if (ec)
		{
			Log::warn("Mesh_Loader::网格体量查询失败({})", mesh_path);
			return false;
		}
		//若文件体量超限
		if (body_size > max_file_size)
		{
			Log::warn("Mesh_Loader::网格体量超限({} 字节)", body_size);
			return false;
		}

		//打开网格文件
		ifstream file(read_path);
		//若文件打开失败
		if (!file.is_open())
		{
			Log::warn("Mesh_Loader::网格文件打开失败({})", mesh_path);
			return false;
		}

		//逐行解析
		string line;
		while (getline(file, line))
		{
			//行输入流
			istringstream stream(line);
			//行首标记
			string flag;
			stream >> flag;

			//顶点行
			if (flag == "v")
			{
				//顶点坐标分量
				double coordinate_X = 0.0, coordinate_Y = 0.0, coordinate_Z = 0.0;
				//读取前三个分量（OBJ 允许附带齐次分量，本加载器忽略）
				if (!(stream >> coordinate_X >> coordinate_Y >> coordinate_Z))
				{
					Log::warn("Mesh_Loader::顶点行格式非法");
					receiver.vertices.clear();
					receiver.indices.clear();
					return false;
				}
				//顶点数量超限检查
				if (receiver.vertices.size() / 3 >= max_vertex_count)
				{
					Log::warn("Mesh_Loader::顶点数量超限({})", max_vertex_count);
					receiver.vertices.clear();
					receiver.indices.clear();
					return false;
				}
				//记录顶点
				receiver.vertices.push_back(static_cast<float>(coordinate_X));
				receiver.vertices.push_back(static_cast<float>(coordinate_Y));
				receiver.vertices.push_back(static_cast<float>(coordinate_Z));
			}
			//面行
			else if (flag == "f")
			{
				//面顶点索引集合
				vector<int64_t> face;
				//面顶点标记
				string token;
				//逐个读取面顶点
				while (stream >> token)
				{
					//面顶点索引
					int64_t index = 0;
					//解析面顶点索引
					if (!face_index_parse(token, receiver.vertices.size() / 3, index))
					{
						receiver.vertices.clear();
						receiver.indices.clear();
						return false;
					}
					face.push_back(index);
				}
				//若面顶点不足三个
				if (face.size() < 3)
				{
					Log::warn("Mesh_Loader::面顶点不足三个");
					receiver.vertices.clear();
					receiver.indices.clear();
					return false;
				}
				//多边形面三角化
				face_triangulate(face, receiver);
			}
			//其余行（法线、纹理坐标、注释、对象名等）一律跳过
		}

		//若未解析出任何三角面
		if (receiver.indices.empty())
		{
			Log::warn("Mesh_Loader::网格未包含有效三角面({})", mesh_path);
			receiver.vertices.clear();
			return false;
		}

		return true;
	}

	//面顶点索引解析
	bool Mesh_Loader::face_index_parse(const string& token, size_t vertex_count,
		int64_t& receiver)
	{
		//截取首个斜杠之前的内容（顶点索引部分）
		size_t cut = token.find('/');
		string number = (cut == string::npos) ? token : token.substr(0, cut);

		//若索引内容为空
		if (number.empty())
		{
			Log::warn("Mesh_Loader::面顶点索引为空");
			return false;
		}

		//索引数值
		int64_t index = 0;
		try
		{
			//十进制整数转换
			index = stoll(number);
		}
		catch (const std::exception&)
		{
			Log::warn("Mesh_Loader::面顶点索引非法({})", number);
			return false;
		}

		//若为负索引（自末尾起算）
		if (index < 0)
			//换算为绝对下标
			index = static_cast<int64_t>(vertex_count) + index;
		//若为正向索引（OBJ 自 1 起算）
		else
			index -= 1;

		//越界检查
		if (index < 0 || static_cast<size_t>(index) >= vertex_count)
		{
			Log::warn("Mesh_Loader::面顶点索引越界({})", number);
			return false;
		}

		receiver = index;
		return true;
	}

	//多边形面三角化
	void Mesh_Loader::face_triangulate(const vector<int64_t>& face, Mesh_Data& receiver)
	{
		//以首个顶点为扇形中心，逐个三角面展开
		for (size_t i = 1; i + 1 < face.size(); ++i)
		{
			receiver.indices.push_back(static_cast<uint32_t>(face[0]));
			receiver.indices.push_back(static_cast<uint32_t>(face[i]));
			receiver.indices.push_back(static_cast<uint32_t>(face[i + 1]));
		}
	}

}