#pragma once
//预编译头
#include "Engine/EngineFoundation/common/前置头文件包含.h"
//获取自有几何类型
#include "几何类型.h"
//获取碰撞库类型（仅后端使用，直接使用原始类型名）
#include "btBulletCollisionCommon.h"
//获取变换类型
#include "LinearMath/btTransform.h"
//获取四元数类型
#include "LinearMath/btQuaternion.h"

namespace engine
{
	/*
	几何类型转换
	自有几何类型与子弹库类型互转；本头仅允许后端源码包含，
	前端公共面不得出现任何子弹库类型。
	*/
	namespace conversion
	{
		//矢量转换（自有 → 子弹库）
		inline btVector3 to_bt(const Vector3& source)
		{
			return btVector3(source.x(), source.y(), source.z());
		}

		//矢量转换（子弹库 → 自有）
		inline Vector3 to_engine(const btVector3& source)
		{
			return Vector3(source.x(), source.y(), source.z());
		}

		//四元数转换（自有 → 子弹库）
		inline btQuaternion to_bt(const Quaternion& source)
		{
			return btQuaternion(source.x(), source.y(), source.z(), source.w());
		}

		//四元数转换（子弹库 → 自有）
		inline Quaternion to_engine(const btQuaternion& source)
		{
			return Quaternion(source.x(), source.y(), source.z(), source.w());
		}

		//变换转换（自有 → 子弹库）
		inline btTransform to_bt(const Transform& source)
		{
			return btTransform(to_bt(source.getRotation()), to_bt(source.getOrigin()));
		}

		//变换转换（子弹库 → 自有）
		inline Transform to_engine(const btTransform& source)
		{
			//转换结果
			Transform result;
			//写入平移与旋转
			result.setOrigin(to_engine(source.getOrigin()));
			result.setRotation(to_engine(source.getRotation()));
			return result;
		}
	}
}
