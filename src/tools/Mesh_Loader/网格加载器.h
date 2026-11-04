#pragma once
//预编译头
#include "Engine/EngineCore/common/前置头文件包含.h"

/*
网格数据类型与 OBJ 解析器（原渲染层 Mesh/Asset 类型，2026-11-04 整体下移本层）
本头同时承担两件事：
  1) 提供引擎唯一的 `engine::Mesh_Data`（顶点 / 索引 / 分段），消除渲染层与本层此前「同名不同定义」的 ODR 隐患；
  2) 提供与渲染层此前等价的 OBJ 解析能力（v / vt / vn / f / usemtl，负索引、扇形三角化、缺法线自动补齐）。
本头 Vulkan-free：渲染层用 `Mesh_Data` 建 GPU 资源，碰撞 / 场景模块可直接用它做几何查询。
*/
namespace engine
{
	/*
	顶点（位置 vec3 + 纹理坐标 vec2 + 法线 vec3；步长 32 字节，与渲染层着色器顶点输入一致）
	法线缺省为零向量（解析器会在缺 vn 时按三角形面积加权自动补齐）。
	*/
	struct Vertex
	{
		//位置（三维）
		float position[3];
		//纹理坐标（二维）
		float uv[2];
		//法线（三维；零向量 = 未提供）
		float normal[3] = { 0.0f, 0.0f, 0.0f };
	};

	/*
	网格分段（物体多材质的子网格级描述）
	first_index 为段在网格索引区内的起始索引（相对索引区起点，uint32 元素计）；
	index_count 为该段索引数（须为 3 的倍数，即完整三角形）；
	material 为材质槽（由调用方解释；0 = 首个材质，越界时绘制入口回落 0）。
	段与段之间允许重叠或留空（不做强制分区校验），由调用方保证语义合理。
	*/
	struct Mesh_Section
	{
		//段起始索引（相对网格索引区起点）
		uint32_t first_index = 0;
		//段索引数（3 的倍数）
		uint32_t index_count = 0;
		//材质槽
		uint32_t material = 0;
	};

	/*
	网格数据（创建入参）
	注意：本结构是引擎唯一一份 `Mesh_Data`（此前渲染层另有一份同名定义，已在本轮合并到本头）。
	*/
	struct Mesh_Data
	{
		//顶点集合
		std::vector<Vertex> vertices;
		//索引集合
		std::vector<uint32_t> indices;
	};

	/*
	网格文件数据（OBJ 解析结果的完整形态）
	data      —— 顶点与索引（可直接交给渲染层 mesh_create / mesh_load_sections）
	sections  —— 按 usemtl 分组的子网格段（material 为组序号 0,1,2…）
	names     —— 段序号 → 材质名（OBJ 里的 usemtl 名称；空串表示该文件未分材质）
	*/
	struct Mesh_File_Data
	{
		//顶点与索引
		Mesh_Data data;
		//子网格分段（至少一段；未分材质时只有一段 material = 0）
		std::vector<Mesh_Section> sections;
		//段序号对应的材质名
		std::vector<std::string> names;
	};

	/*
	网格加载器（OBJ 解析）
	load_obj  —— 只取顶点与索引（不需要分段信息的调用方用这个）
	load_file —— 取完整结果（含 usemtl 分段与材质名；渲染层资产加载用这个）
	两者对同一文件解析出的顶点 / 索引逐字节一致；失败时输出被清空。
	*/
	class Mesh_Loader
	{
	public:
		//加载 OBJ 网格文件（仅顶点与索引）
		static bool load_obj(const std::string& mesh_path, Mesh_Data& receiver);
		//加载 OBJ 网格文件（含分段与材质名）
		static bool load_file(const std::string& mesh_path, Mesh_File_Data& receiver);
	};
}
