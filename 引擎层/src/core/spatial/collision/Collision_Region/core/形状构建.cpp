#include "../局部命名空间使用.h"
//获取数据校验器
#include "src/tools/Data_Validator/数据校验器.h"
//获取日志系统
#include "src/tools/Logging/日志系统.h"
//获取网格加载器
#include "src/tools/Mesh_Loader/网格加载器.h"

namespace engine
{
	//文件内部辅助设施（不对外暴露）
	namespace
	{
		/*
		内存分配
		子弹库的形状类通过 BT_DECLARE_ALIGNED_ALLOCATOR 只声明了抛异常形式的内存分配算子，
		无法使用 new(nothrow)；此处统一包成失败返回，保持本模块的错误处理约定。
		*/
		template <typename Object_Type, typename Receiver_Type, typename... Args>
		bool memory_malloc(unique_ptr<Receiver_Type>& receiver, Args&&... args)
		{
			try
			{
				//构造目标对象
				unique_ptr<Object_Type> object(new Object_Type(std::forward<Args>(args)...));
				//移交所有权
				receiver = std::move(object);
				return true;
			}
			catch (const std::exception&)
			{
				Log::warn("Collision_Region::内存分配失败");
				return false;
			}
		}
	}

	//浮点字段读取
	bool Collision_Region::scalar_read(const nlohmann::json& config, const std::string& field,
		double& receiver)
	{
		//浮点格式检查
		if (Data_Validator::field_check<double>(config, field))
			receiver = config[field].get<double>();
		//整数格式检查（整数可作浮点使用）
		else if (Data_Validator::field_check<int64_t>(config, field))
			receiver = static_cast<double>(config[field].get<int64_t>());
		//其余格式一律判为非法
		else
			return false;

		return true;
	}

	//三元数组字段读取
	bool Collision_Region::vector_read(const nlohmann::json& config, const std::string& field,
		Vector3& receiver)
	{
		//数组格式检查
		if (!Data_Validator::field_check<std::vector<double>>(config, field))
			return false;

		//读取数组内容
		vector<double> values = config[field].get<std::vector<double>>();
		//数组长度检查
		if (values.size() != 3)
		{
			Log::warn("Collision_Region::字段 {} 数组长度非三", field);
			return false;
		}

		//写入矢量
		receiver.setValue(values[0], values[1], values[2]);
		return true;
	}

	//四元数组字段读取
	bool Collision_Region::quaternion_read(const nlohmann::json& config, const std::string& field,
		Quaternion& receiver)
	{
		//数组格式检查
		if (!Data_Validator::field_check<std::vector<double>>(config, field))
			return false;

		//读取数组内容
		vector<double> values = config[field].get<std::vector<double>>();
		//数组长度检查
		if (values.size() != 4)
		{
			Log::warn("Collision_Region::字段 {} 数组长度非四", field);
			return false;
		}

		//写入四元数（按 x、y、z、w 顺序解释）
		receiver.setValue(values[0], values[1], values[2], values[3]);
		return true;
	}

	//网格形状构建
	bool Collision_Region::mesh_shape_build(const string& mesh_path,
		std::unique_ptr<Collision_Shape>& shape_receiver,
		std::unique_ptr<Triangle_Mesh>& mesh_receiver)
	{
		//网格数据
		Mesh_Data mesh_data;
		//加载网格文件
		if (!Mesh_Loader::load_obj(mesh_path, mesh_data))
			return false;

		//分配三角形网格内存（32 位索引 + btVector3 顶点）
		if (!memory_malloc<Triangle_Mesh>(mesh_receiver, true, true))
			return false;

		//预留顶点与索引容量
		mesh_receiver->preallocateVertices(static_cast<int>(mesh_data.vertices.size() / 3));
		mesh_receiver->preallocateIndices(static_cast<int>(mesh_data.indices.size()));

		/*
		写入顶点坐标
		findOrAddVertex 在关闭查重时按调用顺序追加顶点并返回其顺序下标，
		故按索引顺序写入后，顶点数组下标与网格数据下标一一对应。
		*/
		for (size_t i = 0; i + 2 < mesh_data.vertices.size(); i += 3)
			mesh_receiver->findOrAddVertex(Vector3(mesh_data.vertices[i], mesh_data.vertices[i + 1],
				mesh_data.vertices[i + 2]), false);
		//写入三角面索引（顶点须先行写入）
		for (size_t i = 0; i + 2 < mesh_data.indices.size(); i += 3)
			mesh_receiver->addTriangleIndices(mesh_data.indices[i], mesh_data.indices[i + 1],
				mesh_data.indices[i + 2]);

		//分配BVH三角形网格形状（数据源由调用方一并持有）
		if (!memory_malloc<Bvh_Triangle_Mesh>(shape_receiver, mesh_receiver.get(), true, true))
		{
			mesh_receiver.reset();
			return false;
		}

		return true;
	}

	//几何形状构建
	bool Collision_Region::shape_build(const nlohmann::json& geometry_config,
		std::unique_ptr<Collision_Shape>& shape_receiver,
		std::unique_ptr<Triangle_Mesh>& mesh_receiver)
	{
		//形状类型字段检查
		if (!Data_Validator::field_check<std::string>(geometry_config, "type"))
		{
			Log::warn("Collision_Region::几何配置缺少有效字段(type)");
			return false;
		}
		//形状类型
		string shape_type = geometry_config["type"].get<std::string>();

		//---------- 盒体 ----------
		if (shape_type == "box")
		{
			//盒体半长
			Vector3 half_extent;
			//读取盒体半长
			if (!vector_read(geometry_config, "half_extent", half_extent))
			{
				Log::warn("Collision_Region::盒体缺少有效字段(half_extent)");
				return false;
			}
			//半长取值检查
			if (half_extent.getX() <= 0 || half_extent.getY() <= 0 || half_extent.getZ() <= 0)
			{
				Log::warn("Collision_Region::盒体半长非正");
				return false;
			}
			//分配盒体形状
			if (!memory_malloc<Box_Shape>(shape_receiver, half_extent))
				return false;
		}
		//---------- 球体 ----------
		else if (shape_type == "sphere")
		{
			//球体半径
			double radius = 0.0;
			//读取球体半径
			if (!scalar_read(geometry_config, "radius", radius) || radius <= 0)
			{
				Log::warn("Collision_Region::球体缺少有效字段(radius)");
				return false;
			}
			//分配球体形状
			if (!memory_malloc<Sphere_Shape>(shape_receiver, static_cast<Scalar>(radius)))
				return false;
		}
		//---------- 胶囊 ----------
		else if (shape_type == "capsule")
		{
			//胶囊半径与全高
			double radius = 0.0, height = 0.0;
			//读取胶囊半径
			if (!scalar_read(geometry_config, "radius", radius) || radius <= 0)
			{
				Log::warn("Collision_Region::胶囊缺少有效字段(radius)");
				return false;
			}
			//读取胶囊全高
			if (!scalar_read(geometry_config, "height", height) || height <= 0)
			{
				Log::warn("Collision_Region::胶囊缺少有效字段(height)");
				return false;
			}
			//全高须容纳两端半球
			if (height < 2.0 * radius)
			{
				Log::warn("Collision_Region::胶囊全高({})不足以容纳两端半球", height);
				return false;
			}
			//分配胶囊形状（沿 Y 轴，子弹库第二参数为圆柱段长度）
			if (!memory_malloc<Capsule_Shape>(shape_receiver, static_cast<Scalar>(radius),
				static_cast<Scalar>(height - 2.0 * radius)))
				return false;
		}
		//---------- 圆柱 ----------
		else if (shape_type == "cylinder")
		{
			//圆柱半径与全高
			double radius = 0.0, height = 0.0;
			//读取圆柱半径
			if (!scalar_read(geometry_config, "radius", radius) || radius <= 0)
			{
				Log::warn("Collision_Region::圆柱缺少有效字段(radius)");
				return false;
			}
			//读取圆柱全高
			if (!scalar_read(geometry_config, "height", height) || height <= 0)
			{
				Log::warn("Collision_Region::圆柱缺少有效字段(height)");
				return false;
			}
			//分配圆柱形状（沿 Y 轴，子弹库以半长描述）
			if (!memory_malloc<Cylinder_Shape>(shape_receiver,
				Vector3(static_cast<Scalar>(radius), static_cast<Scalar>(height * 0.5),
					static_cast<Scalar>(radius))))
				return false;
		}
		//---------- 圆锥 ----------
		else if (shape_type == "cone")
		{
			//圆锥半径与全高
			double radius = 0.0, height = 0.0;
			//读取圆锥半径
			if (!scalar_read(geometry_config, "radius", radius) || radius <= 0)
			{
				Log::warn("Collision_Region::圆锥缺少有效字段(radius)");
				return false;
			}
			//读取圆锥全高
			if (!scalar_read(geometry_config, "height", height) || height <= 0)
			{
				Log::warn("Collision_Region::圆锥缺少有效字段(height)");
				return false;
			}
			//分配圆锥形状（沿 Y 轴）
			if (!memory_malloc<Cone_Shape>(shape_receiver, static_cast<Scalar>(radius),
				static_cast<Scalar>(height)))
				return false;
		}
		//---------- 网格 ----------
		else if (shape_type == "mesh")
		{
			//网格路径字段检查
			if (!Data_Validator::field_check<std::string>(geometry_config, "mesh_path"))
			{
				Log::warn("Collision_Region::网格缺少有效字段(mesh_path)");
				return false;
			}
			//构建网格形状
			if (!mesh_shape_build(geometry_config["mesh_path"].get<std::string>(), shape_receiver,
				mesh_receiver))
			{
				Log::warn("Collision_Region::网格形状构建失败");
				return false;
			}
		}
		//---------- 未知形状类型 ----------
		else
		{
			Log::warn("Collision_Region::未知形状类型({})", shape_type);
			return false;
		}

		//碰撞边距（缺省沿用子弹库默认边距）
		double margin = 0.04;
		//若显式指定了碰撞边距
		if (geometry_config.contains("margin"))
		{
			//读取碰撞边距
			if (!scalar_read(geometry_config, "margin", margin) || margin < 0)
			{
				Log::warn("Collision_Region::字段(margin)非法");
				shape_receiver.reset();
				mesh_receiver.reset();
				return false;
			}
		}
		//下发碰撞边距（盒体、圆柱、胶囊会自行修正隐式尺寸以保持总体尺寸不变）
		shape_receiver->setMargin(static_cast<Scalar>(margin));
		return true;
	}
}
