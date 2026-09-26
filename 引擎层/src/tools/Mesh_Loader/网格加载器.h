#pragma once
//预编译头
#include "common/前置头文件包含.h"

namespace engine
{
	//网格数据
	struct Mesh_Data
	{
		//顶点坐标（按 x、y、z 依次平铺）
		std::vector<float> vertices;
		//三角面索引（每三个一组，指向顶点数组）
		std::vector<uint32_t> indices;
	};

	//网格加载器
	class Mesh_Loader
	{
	public:
		//加载OBJ网格文件
		static bool load_obj(const std::string& mesh_path, Mesh_Data& receiver);
	private:
		//文件体量上限（64 MiB）
		inline static constexpr uint64_t max_file_size = 64ull * 1024 * 1024;
		//顶点数量上限
		inline static constexpr size_t max_vertex_count = 1000000;

		//面顶点索引解析（支持 v、v/vt、v/vt/vn、v//vn 四种写法）
		static bool face_index_parse(const std::string& token, size_t vertex_count,
			int64_t& receiver);
		//多边形面三角化（以首个顶点为扇形中心）
		static void face_triangulate(const std::vector<int64_t>& face, Mesh_Data& receiver);
	};

}