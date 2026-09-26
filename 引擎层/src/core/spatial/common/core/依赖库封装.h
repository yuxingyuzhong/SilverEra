#pragma once
//预编译头
#include "common/前置头文件包含.h"
//获取碰撞检测库
#include "btBulletCollisionCommon.h"
//获取持久流形（碰撞检测的接触点容器）
#include "BulletCollision/NarrowPhaseCollision/btPersistentManifold.h"
//获取变换类型
#include "LinearMath/btTransform.h"
//获取四元数类型
#include "LinearMath/btQuaternion.h"

//展开命名空间
namespace engine
{
	//碰撞配置别名
	using Collision_Config = btDefaultCollisionConfiguration;
	//窄阶段检测调度器别名
	using Collision_Dispatcher = btCollisionDispatcher;
	//宽阶段检测接口别名
	using Collision_Interface = btBroadphaseInterface;

	//BVH树别名
	using Bvh_Tree = btDbvtBroadphase;
	//扫描线别名
	using SAP = btAxisSweep3;

	//碰撞世界别名
	using Collision_World = btCollisionWorld;
    //碰撞对象别名
    using Collision_Object = btCollisionObject;
    //BVH三角形网格别名
    using Bvh_Triangle_Mesh = btBvhTriangleMeshShape;

	//三维矢量别名
	using Vector3 = btVector3;

	//变换别名
	using Transform = btTransform;
	//四元数别名
	using Quaternion = btQuaternion;
	//标量别名
	using Scalar = btScalar;

	//碰撞形状基类别名
	using Collision_Shape = btCollisionShape;
	//凸包形状基类别名
	using Convex_Shape = btConvexShape;
	//盒体形状别名
	using Box_Shape = btBoxShape;
	//球体形状别名
	using Sphere_Shape = btSphereShape;
	//胶囊形状别名
	using Capsule_Shape = btCapsuleShape;
	//圆柱形状别名
	using Cylinder_Shape = btCylinderShape;
	//圆锥形状别名
	using Cone_Shape = btConeShape;
	//三角形网格别名
	using Triangle_Mesh = btTriangleMesh;
	//持久流形别名
	using Persistent_Manifold = btPersistentManifold;

	//扫掠检测命中回调别名
	using Swept_Callback = Collision_World::ConvexResultCallback;
	//扫掠检测命中结果别名
	using Swept_Result = Collision_World::LocalConvexResult;

	//静态碰撞对象标记
	inline constexpr int Static_Object_Flag = btCollisionObject::CF_STATIC_OBJECT;

	//碰撞检测后端
	struct Collision_Backend
	{
		//配置
		std::shared_ptr<Collision_Config> config;
		//窄阶段调度器
		std::unique_ptr<Collision_Dispatcher> dispatcher;
		//宽阶段接口
		std::unique_ptr<Collision_Interface> broadphase;
		//世界
		std::unique_ptr<Collision_World> world;

		//含参构造(配置共享)
		explicit Collision_Backend(std::shared_ptr<Collision_Config> shared_config)
		{
			//移动配置
			config = std::move(shared_config);
			//分配调度器内存
			dispatcher.reset(new(std::nothrow) Collision_Dispatcher(config.get()));
			//分配宽阶段内存
			broadphase.reset(new (std::nothrow) Bvh_Tree{});
			//分配碰撞世界内存
			world.reset(new (std::nothrow) Collision_World
			(dispatcher.get(), broadphase.get(), config.get()));

			//若存在失败分配
			if (!config || !dispatcher || !broadphase || !world)
			{
				//释放所有已分配内存
				config.reset();
				dispatcher.reset();
				broadphase.reset();
				world.reset();
				return;
			}
		}

		//默认构造
		Collision_Backend(void) : Collision_Backend(std::make_shared<Collision_Config>())
		{}

		//默认析构函数
		~Collision_Backend() = default;

		//禁止拷贝
		Collision_Backend(const Collision_Backend&) = delete;
		Collision_Backend& operator=(const Collision_Backend&) = delete;

		//默认移动构造/赋值
		Collision_Backend(Collision_Backend&&) = default;
		Collision_Backend& operator=(Collision_Backend&&) = default;

		//有效性检查
		bool valid() const
		{
			return config != nullptr
				&& dispatcher != nullptr
				&& broadphase != nullptr
				&& world != nullptr;
		}
	};

}