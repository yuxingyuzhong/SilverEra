#pragma once
/*
碰撞测试辅助（第五十九轮拆分；由原「碰撞测试.cpp」抽出）
提供临时网格 / 边界文件构造、事件构造等静态辅助，以及三个夹具：
Collider_Test、Collision_Region_Test、Collision_Proxy_Test。
*/
//碰撞模块测试：覆盖碰撞体搬移、碰撞空间构建与检测、空间边界与碰撞代理器的事件接入
#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>

//获取碰撞代理器（含碰撞空间、碰撞体与事件系统）
#include "Engine/EngineCore/src/core/spatial/collision/Collision_Proxy/碰撞代理器.h"

//构造盒体几何配置
static nlohmann::json box_config(const uint64_t collider_ID, const double position_X)
{
	//几何配置
	nlohmann::json config = nlohmann::json::object();
	//写入目标碰撞体编号
	config["collider_ID"] = collider_ID;
	//写入形状类型
	config["type"] = "box";
	//写入盒体半长
	config["half_extent"] = nlohmann::json::array({ 1.0, 1.0, 1.0 });
	//写入初始位置
	config["position"] = nlohmann::json::array({ position_X, 0.0, 0.0 });
	return config;
}

//构造碰撞体事件载荷
static nlohmann::json collider_payload(const std::string& region, const uint64_t collider_ID)
{
	//事件载荷
	nlohmann::json payload = nlohmann::json::object();
	//写入空间名称与碰撞体编号
	payload["region"] = region;
	payload["collider_ID"] = collider_ID;
	return payload;
}

//构造碰撞模块事件
static std::shared_ptr<engine::Event> make_event(const std::string& tag,
	const nlohmann::json& config)
{
	return std::make_shared<engine::Event>("", "", "Collision", tag, config);
}

//临时网格文件（相对可执行文件目录）
static const std::string temp_mesh_name = "src/单元测试/EngineCore/核心/碰撞测试临时网格.obj";

//写入手工立方体网格文件，返回可读取的相对路径
static std::string temp_mesh_write(void)
{
	//写入路径
	std::filesystem::path mesh_path =
		engine::Engine_Env::exe_dir_get() / engine::detail::string_to_path(temp_mesh_name);

	//打开临时网格文件
	std::ofstream file(mesh_path);
	//写入八个顶点（立方体半长为一）
	file << "v -1.0 -1.0 -1.0\n";
	file << "v 1.0 -1.0 -1.0\n";
	file << "v 1.0 1.0 -1.0\n";
	file << "v -1.0 1.0 -1.0\n";
	file << "v -1.0 -1.0 1.0\n";
	file << "v 1.0 -1.0 1.0\n";
	file << "v 1.0 1.0 1.0\n";
	file << "v -1.0 1.0 1.0\n";
	//写入十二个三角面
	file << "f 1 2 3\n" << "f 1 3 4\n";
	file << "f 5 7 6\n" << "f 5 8 7\n";
	file << "f 1 6 2\n" << "f 1 5 6\n";
	file << "f 2 7 3\n" << "f 2 6 7\n";
	file << "f 3 8 4\n" << "f 3 7 8\n";
	file << "f 4 5 1\n" << "f 4 8 5\n";
	//关闭文件
	file.close();

	return temp_mesh_name;
}

//临时边界配置文件（相对可执行文件目录）
static const std::string temp_boundary_name = "src/单元测试/EngineCore/核心/碰撞测试临时边界.json";

//写入边界配置文件（登记空间的边界网格路径），返回可读取的相对路径
static std::string temp_boundary_write(const std::string& region, const std::string& mesh_name)
{
	//写入路径
	std::filesystem::path config_path =
		engine::Engine_Env::exe_dir_get() / engine::detail::string_to_path(temp_boundary_name);

	//打开临时边界配置文件
	std::ofstream file(config_path);
	//写入边界配置（空间数组：空间名称 + 边界网格路径）
	file << "[{\"region\":\"" << region << "\",\"mesh_path\":\"" << mesh_name << "\"}]";
	//关闭文件
	file.close();

	return temp_boundary_name;
}

//碰撞体测试夹具
class Collider_Test : public ::testing::Test
{
};

//碰撞体：默认构造即挂载自指针，可直接反查
class Collision_Region_Test : public ::testing::Test
{
protected:
	//被测碰撞空间
	engine::Collision_Region region{ "测试空间" };
};

//碰撞空间：后端可用、默认非激活、状态可设置
class Collision_Proxy_Test : public ::testing::Test
{
protected:
	//被测碰撞代理器
	engine::Collision_Proxy proxy;
};

//碰撞代理器：空间构建、重复构建与卸载