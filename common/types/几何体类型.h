#pragma once
//预编译头
#include "common/前置头文件包含.h"
//获取坐标类型
#include "common/types/坐标类型.h"

namespace engine
{
	//几何体类型
	enum class GeometryType
	{
		Sphere,
		Box,
		Cylinder,
		Cone,
		Capsule,
		Ellipsoid,
		Torus
	};

    //几何体结构
    struct Geometry 
    {
        //几何体类型
        GeometryType type;

        //尺寸参数联合体
        union
        {
            //球体、圆柱、圆锥、胶囊的半径
            float radius;               
            //长方体、椭球体的半轴长度
            glm::vec3 halfExtents;      
            //圆柱、圆锥、胶囊：半径和高度
            struct { float radius; float height; }cylinder;
            //环面：主半径和管半径
            struct { float majorRadius; float minorRadius; }torus;
        };

        // ---------- 构造函数 ----------
        // 默认构造：半径为0的球体
        Geometry() : type(GeometryType::Sphere), radius(0.0f) {}

        // 便捷静态工厂函数
        static Geometry createSphere(float r) {
            Geometry g;
            g.type = GeometryType::Sphere;
            g.radius = r;
            return g;
        }

        static Geometry createBox(const glm::vec3& halfExt) {
            Geometry g;
            g.type = GeometryType::Box;
            g.halfExtents = halfExt;
            return g;
        }

        static Geometry createEllipsoid(const glm::vec3& halfExt) {
            Geometry g;
            g.type = GeometryType::Ellipsoid;
            g.halfExtents = halfExt;
            return g;
        }

        static Geometry createCylinder(float r, float h) {
            Geometry g;
            g.type = GeometryType::Cylinder;
            g.radius = r;
            g.cylinder.height = h;
            return g;
        }

        static Geometry createCone(float r, float h) {
            Geometry g;
            g.type = GeometryType::Cone;
            g.radius = r;
            g.cylinder.height = h;
            return g;
        }

        static Geometry createCapsule(float r, float h) {
            Geometry g;
            g.type = GeometryType::Capsule;
            g.radius = r;
            g.cylinder.height = h;
            return g;
        }

        static Geometry createTorus(float majorR, float minorR) {
            Geometry g;
            g.type = GeometryType::Torus;
            g.torus.majorRadius = majorR;
            g.torus.minorRadius = minorR;
            return g;
        }

        // ---------- 辅助方法 ----------
        // 计算局部空间的 AABB（不包含缩放和旋转，仅基于局部尺寸）
        glm::vec3 getLocalAABBMin() const {
            switch (type) {
            case GeometryType::Sphere:
                return glm::vec3(-radius);
            case GeometryType::Box:
            case GeometryType::Ellipsoid:
                return -halfExtents;
            case GeometryType::Cylinder:
            case GeometryType::Cone:
            case GeometryType::Capsule:
                return glm::vec3(-radius, -cylinder.height * 0.5f, -radius);
            case GeometryType::Torus:
                return glm::vec3(-(torus.majorRadius + torus.minorRadius), 
                    -torus.minorRadius, -(torus.majorRadius + torus.minorRadius));
            default:
                return glm::vec3(0.0f);
            }
        }

        glm::vec3 getLocalAABBMax() const {
            switch (type) {
            case GeometryType::Sphere:
                return glm::vec3(radius);
            case GeometryType::Box:
            case GeometryType::Ellipsoid:
                return halfExtents;
            case GeometryType::Cylinder:
            case GeometryType::Cone:
            case GeometryType::Capsule:
                return glm::vec3(radius, cylinder.height * 0.5f, radius);
            case GeometryType::Torus:
                return glm::vec3(torus.majorRadius + torus.minorRadius, torus.minorRadius, 
                    torus.majorRadius + torus.minorRadius);
            default:
                return glm::vec3(0.0f);
            }
        }

        // 结合 Transform 计算世界空间的 AABB（简化版，仅处理旋转和缩放，不考虑非均匀缩放带来的复杂形状）
        // 注意：此方法为近似，对于非均匀缩放的球体/胶囊等可能不精确，实际使用时可根据需要细化。
        glm::vec3 getWorldAABBMin(const glm::vec3& position, const glm::quat& rotation, const glm::vec3& scale) const {
            glm::vec3 localMin = getLocalAABBMin();
            glm::vec3 localMax = getLocalAABBMax();
            // 计算旋转后的包围盒（通过旋转四个角点取极值，此处简化：直接用旋转后的中心 ± 旋转后的半尺寸）
            // 更准确的方法是变换8个角点，但这里提供近似（当旋转为90度倍数时精确）
            glm::vec3 halfSize = (localMax - localMin) * 0.5f;
            glm::vec3 center = (localMax + localMin) * 0.5f;
            // 应用缩放
            halfSize = halfSize * scale;
            // 旋转 halfSize 的绝对值（旋转后包围盒各轴范围）
            glm::mat3 rotMat = glm::mat3_cast(rotation);
            glm::vec3 rotatedHalf = glm::abs(rotMat[0]) * halfSize.x +
                glm::abs(rotMat[1]) * halfSize.y +
                glm::abs(rotMat[2]) * halfSize.z;
            glm::vec3 worldCenter = position + rotation * (center * scale);
            return worldCenter - rotatedHalf;
        }

        glm::vec3 getWorldAABBMax(const glm::vec3& position, const glm::quat& rotation, const glm::vec3& scale) const {
            glm::vec3 localMin = getLocalAABBMin();
            glm::vec3 localMax = getLocalAABBMax();
            glm::vec3 halfSize = (localMax - localMin) * 0.5f;
            glm::vec3 center = (localMax + localMin) * 0.5f;
            halfSize = halfSize * scale;
            glm::mat3 rotMat = glm::mat3_cast(rotation);
            glm::vec3 rotatedHalf = glm::abs(rotMat[0]) * halfSize.x +
                glm::abs(rotMat[1]) * halfSize.y +
                glm::abs(rotMat[2]) * halfSize.z;
            glm::vec3 worldCenter = position + rotation * (center * scale);
            return worldCenter + rotatedHalf;
        }
    };

    //变换组件
    struct Transform 
    {
        //位置
        glm::vec3 position;      
        //旋转（四元数，避免万向锁）
        glm::quat rotation;      
        // 缩放（各轴缩放因子）
        glm::vec3 scale;        

        // ---------- 构造函数 ----------
        // 默认构造：单位变换（位于原点，无旋转，缩放为1）
        Transform()
            : position(0.0f),
            rotation(1.0f, 0.0f, 0.0f, 0.0f),  // 单位四元数
            scale(1.0f) {}

        // 带参构造：指定位置、旋转和缩放
        Transform(const glm::vec3& pos,
            const glm::quat& rot = glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
            const glm::vec3& scl = glm::vec3(1.0f))
            : position(pos), rotation(rot), scale(scl) {}

        // 从欧拉角构造（角度制）
        static Transform fromEuler(const glm::vec3& pos, const glm::vec3& eulerDegrees, const glm::vec3& scl = glm::vec3(1.0f)) {
            Transform t;
            t.position = pos;
            t.rotation = glm::quat(glm::radians(eulerDegrees));
            t.scale = scl;
            return t;
        }

        // ---------- 矩阵获取 ----------
        // 获取世界变换矩阵：T * R * S
        glm::mat4 getMatrix() const {
            glm::mat4 T = glm::translate(glm::mat4(1.0f), position);
            glm::mat4 R = glm::mat4_cast(rotation);
            glm::mat4 S = glm::scale(glm::mat4(1.0f), scale);
            return T * R * S;
        }

        // 获取逆变换矩阵（世界到局部）
        glm::mat4 getInverseMatrix() const {
            // 逆变换 = S^-1 * R^-1 * T^-1
            glm::mat4 S_inv = glm::scale(glm::mat4(1.0f), 1.0f / scale);
            glm::mat4 R_inv = glm::mat4_cast(glm::conjugate(rotation)); // 单位四元数的共轭即逆
            glm::mat4 T_inv = glm::translate(glm::mat4(1.0f), -position);
            return S_inv * R_inv * T_inv;
        }

        // ---------- 向量变换 ----------
        // 将局部空间中的点变换到世界空间（应用缩放、旋转和平移）
        glm::vec3 transformPoint(const glm::vec3& localPoint) const {
            return position + rotation * (scale * localPoint);
        }

        // 将世界空间中的点变换到局部空间
        glm::vec3 inverseTransformPoint(const glm::vec3& worldPoint) const {
            glm::vec3 local = worldPoint - position;
            local = glm::conjugate(rotation) * local;
            local = local / scale;
            return local;
        }

        // 将局部空间中的方向变换到世界空间（忽略平移和缩放，仅应用旋转）
        glm::vec3 transformDirection(const glm::vec3& localDir) const {
            return rotation * localDir;
        }

        // 将世界空间中的方向变换到局部空间
        glm::vec3 inverseTransformDirection(const glm::vec3& worldDir) const {
            return glm::conjugate(rotation) * worldDir;
        }

        // ---------- 便捷操作 ----------
        // 平移（在局部坐标系中移动）
        void translateLocal(const glm::vec3& delta) {
            position += rotation * (scale * delta);
        }

        // 平移（在世界坐标系中移动）
        void translateWorld(const glm::vec3& delta) {
            position += delta;
        }

        // 绕局部轴旋转（角度制）
        void rotateLocal(const glm::vec3& axis, float angleDegrees) {
            glm::quat q = glm::angleAxis(glm::radians(angleDegrees), axis);
            rotation = rotation * q;
        }

        // 绕世界轴旋转（角度制）
        void rotateWorld(const glm::vec3& axis, float angleDegrees) {
            glm::quat q = glm::angleAxis(glm::radians(angleDegrees), axis);
            rotation = q * rotation;
        }

        // 设置欧拉角（角度制）
        void setEulerAngles(const glm::vec3& eulerDegrees) {
            rotation = glm::quat(glm::radians(eulerDegrees));
        }

        // 获取欧拉角（角度制，注意可能丢失精度）
        glm::vec3 getEulerAngles() const {
            return glm::degrees(glm::eulerAngles(rotation));
        }
    };
}