#include "../局部命名空间使用.h"
//获取数据校验器
#include "src/tools/Data_Validator/数据校验器.h"
//获取日志系统
#include "src/tools/Logging/日志系统.h"
//获取网格加载器
#include "src/tools/Mesh_Loader/网格加载器.h"

namespace engine
{
	//默认构造
	Collision_Region::Collision_Region() : Collision_Region(string())
	{
	}

	//含参构造(指定碰撞空间名称)
	Collision_Region::Collision_Region(const string& region_name)
	{
		//记录碰撞空间名称
		name = region_name;
		//预分配空间边界编号（编号0恒保留给空间边界）
		region_boundary.ID = ID_allocator.get();
	}

	//析构函数
	Collision_Region::~Collision_Region()
	{
		//若碰撞世界不可用则无需摘除
		if (!backend.world)
			return;

		//摘除空间边界碰撞对象
		if (region_boundary.shape)
			backend.world->removeCollisionObject(&region_boundary.object);
		//摘除全部碰撞体碰撞对象（碰撞体先于碰撞世界析构）
		for (auto& pair : mapping)
			backend.world->removeCollisionObject(&pair.second.object);
	}

	//碰撞空间有效性检查
	bool Collision_Region::valid(void) const
	{
		return backend.valid();
	}

	//碰撞空间状态设置
	void Collision_Region::state_set(bool active)
	{
		is_active = active;
	}

	//碰撞空间状态查看
	bool Collision_Region::state_check() const
	{
		return is_active;
	}

	//边界构建
	bool Collision_Region::boundary_build(const string& mesh_path)
	{
		//空间有效性检查
		if (!valid())
		{
			Log::warn("Collision_Region::碰撞空间后端不可用，空间边界无法构建");
			return false;
		}

		//网格形状与其数据源
		unique_ptr<Collision_Shape> shape;
		unique_ptr<Triangle_Mesh> mesh;
		//构建网格形状
		if (!mesh_shape_build(mesh_path, shape, mesh))
		{
			Log::warn("Collision_Region::空间边界网格构建失败({})", mesh_path);
			return false;
		}

		//若已存在空间边界则先卸载
		boundary_unload();

		//挂载边界形状与网格数据
		region_boundary.mesh = std::move(mesh);
		region_boundary.shape = std::move(shape);
		region_boundary.object.setCollisionShape(region_boundary.shape.get());
		//空间边界恒为静态对象
		region_boundary.object.setCollisionFlags(Static_Object_Flag);
		//空间边界置于原点
		region_boundary.object.setWorldTransform(Transform::getIdentity());
		//空间边界加入碰撞世界
		backend.world->addCollisionObject(&region_boundary.object);
		return true;
	}

	//边界卸载
	void Collision_Region::boundary_unload(void)
	{
		//若空间边界尚未构建
		if (!region_boundary.shape)
			return;

		//摘除空间边界碰撞对象
		backend.world->removeCollisionObject(&region_boundary.object);
		//卸载空间边界形状与网格数据
		region_boundary.object.setCollisionShape(nullptr);
		region_boundary.shape.reset();
		region_boundary.mesh.reset();
	}
}
