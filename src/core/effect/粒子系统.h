#pragma once
/*
粒子系统（第三百零八轮迁入引擎核心层）
定位：图形 API-free 粒子池（容量 / 活跃计数 / 分配回收 ＋ CPU 参考模拟）。
约束：默认构造即为默认容量；分配走 LIFO 回收栈，保证回收后立即复用同一索引。
渲染层不再持有模拟状态：由应用层持有本对象，模拟后把状态数组绑定给渲染层绘制。
*/
#include "粒子数据.h"

//标准容器
#include <vector>

//引擎命名空间
namespace engine
{
	//粒子池
	class Particle_System
	{
	public:
		//容量设置（钳制到 [1, PARTICLE_CAPACITY_MAX]；收缩时丢弃超出容量的粒子）
		void capacity_set(uint32_t capacity);
		//当前容量
		uint32_t capacity_get(void) const;
		//当前活跃粒子数
		uint32_t active_count_get(void) const;
		/*
		分配一个新粒子（不初始化状态内容，由调用方填充）
		池满返回假；成功时 index_out 为池内索引（回收栈优先复用）。
		*/
		bool spawn(uint32_t& index_out);
		//回收指定索引（非法 / 重复回收返回假）
		bool recycle(uint32_t index);
		//清空全部粒子并复位回收栈（保留容量）
		void clear(void);
		/*
		推进一个固定步（CPU 参考；与 GPU 推进同公式）
		v += gravity·dt → v *= damping → p += v·dt → age += dt；age ≥ lifetime 即回收。
		返回本步死亡数。
		*/
		uint32_t step(const Particle_Params& params, float dt);
		/*
		发射（CPU 参考；确定性 LCG）
		形状采样位置；速度 = normalize(direction ＋ 0.3·随机单位向量) × [speed_min, speed_max]；
		budget 为本帧最多发射数；返回实际发射数（池满即停止）。
		*/
		uint32_t emit(const Particle_Emitter& emitter, uint32_t budget);
		//重置随机种子（确定性对照用）
		void seed_set(uint32_t seed);
		//粒子状态数组（长度 = 容量；参考模拟与 GPU 上传共用）
		Particle_State* data_get(void);
		const Particle_State* data_get(void) const;
		//逐索引活跃标记（1 = 活跃；长度 = 容量）
		uint8_t* alive_get(void);
		const uint8_t* alive_get(void) const;

	private:
		//粒子状态数组（长度 = 容量）
		std::vector<Particle_State> particles_;
		//空闲索引栈（分配自尾部弹出）
		std::vector<uint32_t> free_indices_;
		//逐索引活跃标记（1 = 活跃）
		std::vector<uint8_t> alive_;
		//活跃粒子数
		uint32_t active_count_ = 0;
		//当前容量
		uint32_t capacity_ = PARTICLE_CAPACITY_DEFAULT;
		//随机状态（LCG；默认固定种子保证确定性）
		uint32_t rng_state_ = 12345u;
	};
}
