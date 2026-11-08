#pragma once
//预编译头
#include "Engine/EngineFoundation/common/前置头文件包含.h"

namespace engine
{
	//标量类型（与子弹库标量的 float 口径一致）
	using Scalar = float;

	/*
	三维矢量
	仅保留碰撞模块实际使用的最小运算集合，避免把外部数值库的依赖带到前端；
	分量访问沿用既有调用点的命名（x/y/z 与 getX/getY/getZ），迁移时调用点无需改写。
	*/
	class Vector3
	{
	private:
		//三个分量
		Scalar value[3] = { 0.0f, 0.0f, 0.0f };
	public:
		//默认构造（零向量）
		Vector3() = default;

		//含参构造
		Vector3(Scalar x_value, Scalar y_value, Scalar z_value)
		{
			value[0] = x_value;
			value[1] = y_value;
			value[2] = z_value;
		}

		//三分量写入
		void setValue(Scalar x_value, Scalar y_value, Scalar z_value)
		{
			value[0] = x_value;
			value[1] = y_value;
			value[2] = z_value;
		}

		//X 分量
		Scalar x(void) const { return value[0]; }
		//Y 分量
		Scalar y(void) const { return value[1]; }
		//Z 分量
		Scalar z(void) const { return value[2]; }

		//X 分量（兼容既有命名）
		Scalar getX(void) const { return value[0]; }
		//Y 分量（兼容既有命名）
		Scalar getY(void) const { return value[1]; }
		//Z 分量（兼容既有命名）
		Scalar getZ(void) const { return value[2]; }

		//长度平方
		Scalar length2(void) const
		{
			return value[0] * value[0] + value[1] * value[1] + value[2] * value[2];
		}

		//长度
		Scalar length(void) const
		{
			return std::sqrt(length2());
		}

		//单位化（零向量保持零向量）
		Vector3 normalized(void) const
		{
			//长度
			Scalar length_value = length();
			//零向量不做除法
			if (length_value == 0.0f)
				return Vector3();
			return *this / length_value;
		}

		//矢量加法
		Vector3 operator+(const Vector3& other) const
		{
			return Vector3(value[0] + other.value[0], value[1] + other.value[1],
				value[2] + other.value[2]);
		}

		//矢量减法
		Vector3 operator-(const Vector3& other) const
		{
			return Vector3(value[0] - other.value[0], value[1] - other.value[1],
				value[2] - other.value[2]);
		}

		//标量乘法
		Vector3 operator*(Scalar factor) const
		{
			return Vector3(value[0] * factor, value[1] * factor, value[2] * factor);
		}

		//标量除法
		Vector3 operator/(Scalar factor) const
		{
			return Vector3(value[0] / factor, value[1] / factor, value[2] / factor);
		}

		//矢量自加
		Vector3& operator+=(const Vector3& other)
		{
			value[0] += other.value[0];
			value[1] += other.value[1];
			value[2] += other.value[2];
			return *this;
		}
	};

	/*
	四元数
	碰撞模块只用到的能力：分量读写、矢量旋转、四元数乘法；
	默认构造为单位四元数，与变换的默认姿态一致。
	*/
	class Quaternion
	{
	private:
		//四个分量（x、y、z、w）
		Scalar value[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	public:
		//默认构造（单位四元数）
		Quaternion() = default;

		//含参构造
		Quaternion(Scalar x_value, Scalar y_value, Scalar z_value, Scalar w_value)
		{
			value[0] = x_value;
			value[1] = y_value;
			value[2] = z_value;
			value[3] = w_value;
		}

		//四分量写入（按 x、y、z、w 顺序）
		void setValue(Scalar x_value, Scalar y_value, Scalar z_value, Scalar w_value)
		{
			value[0] = x_value;
			value[1] = y_value;
			value[2] = z_value;
			value[3] = w_value;
		}

		//X 分量
		Scalar x(void) const { return value[0]; }
		//Y 分量
		Scalar y(void) const { return value[1]; }
		//Z 分量
		Scalar z(void) const { return value[2]; }
		//W 分量
		Scalar w(void) const { return value[3]; }

		//四元数乘法（Hamilton 积）
		Quaternion operator*(const Quaternion& other) const
		{
			return Quaternion(
				w() * other.x() + x() * other.w() + y() * other.z() - z() * other.y(),
				w() * other.y() - x() * other.z() + y() * other.w() + z() * other.x(),
				w() * other.z() + x() * other.y() - y() * other.x() + z() * other.w(),
				w() * other.w() - x() * other.x() - y() * other.y() - z() * other.z());
		}

		//矢量旋转（四元数须为单位四元数）
		Vector3 rotate(const Vector3& vector) const
		{
			//四元数的矢量部分
			Vector3 axis(x(), y(), z());
			//中间量：2 × (轴 × 矢量)
			Vector3 middle = vector_cross(axis, vector) * 2.0f;
			//旋转结果：矢量 + 标量部分 × 中间量 + 轴 × 中间量
			return vector + middle * w() + vector_cross(axis, middle);
		}

		//单位四元数判定（q 与 -q 表示同一旋转）
		bool identity_check(Scalar epsilon = 1e-6f) const
		{
			return std::abs(w()) >= 1.0f - epsilon;
		}

		//四元数长度平方
		Scalar length2(void) const
		{
			return x() * x() + y() * y() + z() * z() + w() * w();
		}

		//单位化（零四元数返回单位四元数）
		Quaternion normalized(void) const
		{
			//四元数长度
			Scalar length_value = std::sqrt(
				x() * x() + y() * y() + z() * z() + w() * w());
			//零四元数不做除法
			if (length_value <= 0.0f)
				return Quaternion();
			return Quaternion(x() / length_value, y() / length_value,
				z() / length_value, w() / length_value);
		}

		//轴角读取（角度范围 [0, π]，单位四元数）
		bool axis_angle_get(Vector3& axis, Scalar& angle) const
		{
			//单位化后读取
			Quaternion unit = normalized();
			//角度
			angle = 2.0f * std::acos(std::clamp(unit.w(), -1.0f, 1.0f));
			//半角正弦
			Scalar half_sine = std::sin(angle * 0.5f);
			//接近零角时轴退化为 X 轴
			if (std::abs(half_sine) <= 1e-6f)
			{
				axis.setValue(1.0f, 0.0f, 0.0f);
				angle = 0.0f;
				return true;
			}
			//归一化轴
			axis.setValue(unit.x() / half_sine, unit.y() / half_sine, unit.z() / half_sine);
			return true;
		}

		//轴角构造
		static Quaternion from_axis_angle(const Vector3& axis, Scalar angle)
		{
			//轴长度
			Scalar axis_length = axis.length();
			//零轴返回单位四元数
			if (axis_length <= 0.0f || angle == 0.0f)
				return Quaternion();
			//单位轴
			Vector3 unit_axis = axis / axis_length;
			//半角
			Scalar half_angle = angle * 0.5f;
			//半角正弦与余弦
			Scalar half_sine = std::sin(half_angle);
			Scalar half_cosine = std::cos(half_angle);
			return Quaternion(unit_axis.x() * half_sine, unit_axis.y() * half_sine,
				unit_axis.z() * half_sine, half_cosine);
		}

	private:
		//矢量叉积
		static Vector3 vector_cross(const Vector3& left, const Vector3& right)
		{
			return Vector3(
				left.y() * right.z() - left.z() * right.y(),
				left.z() * right.x() - left.x() * right.z(),
				left.x() * right.y() - left.y() * right.x());
		}
	};

	/*
	刚体变换
	由平移与旋转组成；默认构造为单位变换。
	乘法语义与子弹库一致：先应用 other，再应用自身。
	*/
	class Transform
	{
	private:
		//平移
		Vector3 origin;
		//旋转
		Quaternion rotation;
	public:
		//默认构造（单位变换）
		Transform() = default;

		//单位变换获取
		static Transform getIdentity(void)
		{
			return Transform();
		}

		//平移写入
		void setOrigin(const Vector3& origin_value) { origin = origin_value; }
		//平移读取（可写）
		Vector3& getOrigin(void) { return origin; }
		//平移读取（只读）
		const Vector3& getOrigin(void) const { return origin; }

		//旋转写入
		void setRotation(const Quaternion& rotation_value) { rotation = rotation_value; }
		//旋转读取（可写）
		Quaternion& getRotation(void) { return rotation; }
		//旋转读取（只读）
		const Quaternion& getRotation(void) const { return rotation; }

		//变换复合（先应用 other，再应用自身）
		Transform operator*(const Transform& other) const
		{
			//复合结果
			Transform result;
			//旋转复合
			result.setRotation(rotation * other.rotation);
			//平移复合：自身旋转作用于 other 的平移，再叠加自身平移
			result.setOrigin(rotation.rotate(other.origin) + origin);
			return result;
		}
	};
}
