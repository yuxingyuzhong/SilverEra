#pragma once
//预编译头
#include "common/前置头文件包含.h"
//获取碰撞库依赖封装
#include "../../common/core/依赖库封装.h"

namespace engine
{
	//碰撞检测方式
	struct Detection_Mode
	{
		//扫掠检测标记
		bool is_swept_volume = false;
		//离散检测步长
		uint32_t step_length = 0;
	};

	//碰撞体
	struct Collider
	{
		//编号
		uint64_t ID = 0;
		//碰撞对象
		Collision_Object object;
		/*
		网格接口（网格形状的数据源）
		注意：btBvhTriangleMeshShape 只持有网格指针而不接管所有权，
		      故网格数据必须与形状同生共死；此处按成员声明顺序的逆序析构，
		      网格先于形状声明，因此析构时形状先释放、网格后释放。
		*/
		std::unique_ptr<Triangle_Mesh> mesh;
		//几何形状（由碰撞体持有，碰撞对象只挂载指针）
		std::unique_ptr<Collision_Shape> shape;

		//位移向量
		Vector3 displacement_vector{ 0.0f,0.0f,0.0f };
		//检测方式
		Detection_Mode detection_mode;
		//豁免标记
		uint64_t exemption_flag = 0;
		//几何配置（供空间间镜像与转移复用）
		nlohmann::json geometry;

		//默认构造
		Collider()
		{
			//挂载封装结构体指针
			object.setUserPointer(this);
		}

		//默认析构
		~Collider() = default;

		//禁止拷贝（object 加入 world 后不能搬移）
		Collider(const Collider&) = delete;
		Collider& operator=(const Collider&) = delete;

		/*
		移动构造
		默认移动会连同object一起搬走，而object内挂载的自指针仍指向被搬空的源对象，
		导致 Collider::recover 反查失效；故显式实现：搬移后重新挂载自指针，
		并清空源对象自指针，保证反查结果始终有效。
		*/
		Collider(Collider&& other) noexcept
		{
			//搬移编号与数据成员
			ID = other.ID;
			object = other.object;
			mesh = std::move(other.mesh);
			shape = std::move(other.shape);
			displacement_vector = other.displacement_vector;
			detection_mode = other.detection_mode;
			exemption_flag = other.exemption_flag;
			geometry = std::move(other.geometry);

			//重新挂载封装结构体指针
			object.setUserPointer(this);
			//清空源对象自指针
			other.object.setUserPointer(nullptr);
		}

		//移动赋值
		Collider& operator=(Collider&& other) noexcept
		{
			//若为自身赋值则直接返回
			if (this == &other)
				return *this;

			//搬移编号与数据成员
			ID = other.ID;
			object = other.object;
			mesh = std::move(other.mesh);
			shape = std::move(other.shape);
			displacement_vector = other.displacement_vector;
			detection_mode = other.detection_mode;
			exemption_flag = other.exemption_flag;
			geometry = std::move(other.geometry);

			//重新挂载封装结构体指针
			object.setUserPointer(this);
			//清空源对象自指针
			other.object.setUserPointer(nullptr);

			return *this;
		}

		//反查获取封装结构
		static Collider* recover(const Collision_Object* obj)
		{
			return static_cast<Collider*>(obj->getUserPointer());
		}

	};
}
