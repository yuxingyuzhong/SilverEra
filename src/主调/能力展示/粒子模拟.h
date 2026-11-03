#pragma once
//预编译头
#include "Application/Renderer/common/前置头文件包含.h"
//渲染器门面（只做 GPU 上传与 billboard 绘制登记）
#include "Application/Renderer/src/render/Renderer/渲染器.h"
//粒子池与模拟语义（已下沉到 EngineCore）
#include "Engine/EngineCore/src/core/effect/粒子系统.h"

//数学
#include <cmath>

/*
粒子模拟宿主（应用层；渲染层职责下沉第二批）
渲染层不再持有粒子模拟状态：本类持有 `Particle_System`（池 / 发射 / 力场 / 碰撞 / 生命周期），
在构造时把状态数组绑定给渲染器，渲染器只负责上传与绘制。
生命周期：构造即绑定，析构解绑（避免渲染器持有悬空指针）。
*/
namespace engine
{
	//粒子模拟宿主
	class Particle_Host
	{
	public:
		//构造：初始化池容量并绑定（默认容量与渲染器缓冲同口径）
		explicit Particle_Host(Renderer& renderer) : 渲染器(renderer)
		{
			//池默认构造不分配空闲栈，须显式设定容量后才可发射
			const uint32_t capacity = 渲染器.particle_capacity_get();
			池.capacity_set(capacity == 0 ? PARTICLE_CAPACITY_DEFAULT : capacity);
			渲染器.particle_source_bind(池.data_get(), 池.alive_get(), 池.capacity_get());
		}
		/*
		析构不解绑
		宿主可能比渲染器活得久（静态宿主），解绑会写入已销毁的渲染器；
		绘制前各入口都会重新绑定（见 bind），故无需在析构时回收。
		*/
		~Particle_Host() = default;
		//禁止复制（持有池状态）
		Particle_Host(const Particle_Host&) = delete;
		Particle_Host& operator=(const Particle_Host&) = delete;

		/*
		容量同步
		池容量与渲染器 GPU 缓冲容量保持一致（钳制口径由渲染器与池各自实现），并重新绑定。
		*/
		bool capacity_set(uint32_t capacity)
		{
			//渲染器拒绝（未初始化等）
			if (!渲染器.particle_capacity_set(capacity))
				return false;
			池.capacity_set(渲染器.particle_capacity_get());
			渲染器.particle_source_bind(池.data_get(), 池.alive_get(), 池.capacity_get());
			return true;
		}
		//容量同步（外部已按渲染器钳制后的容量）
		void capacity_sync(uint32_t capacity)
		{
			池.capacity_set(capacity);
			渲染器.particle_source_bind(池.data_get(), 池.alive_get(), 池.capacity_get());
		}
		//当前池容量
		uint32_t capacity_get(void) const
		{
			return 池.capacity_get();
		}
		//发射（渲染器关闭 / 未配置发射器时返回 0）
		uint32_t emit(uint32_t count)
		{
			//渲染器关闭
			if (!渲染器.particle_enable_get() || count == 0)
				return 0;
			const uint32_t spawned = 池.emit(渲染器.particle_emitter_get(), count);
			//出生计数上报（帧统计观测）
			渲染器.particle_stats_add(spawned, 0);
			return spawned;
		}
		//推进（渲染器关闭 / 步长非法时返回 0）
		uint32_t step(float dt)
		{
			//渲染器关闭
			if (!渲染器.particle_enable_get())
				return 0;
			//步长非法
			if (!std::isfinite(dt) || dt <= 0.0f)
				return 0;
			const uint32_t died = 池.step(渲染器.particle_params_get(), dt);
			//死亡计数上报（帧统计观测）
			渲染器.particle_stats_add(0, died);
			return died;
		}
		//当前活跃粒子数（常驻模式取 GPU 活跃计数；否则按池的活跃标记统计）
		uint32_t active_count(void) const
		{
			//渲染器关闭
			if (!渲染器.particle_enable_get())
				return 0;
			//常驻模式
			if (渲染器.particle_resident_get())
				return 渲染器.particle_gpu_active_count();
			//GPU 推进会直接改写活跃标记，故按标记统计而非池计数器
			uint32_t count = 0;
			const uint8_t* alive = 池.alive_get();
			for (uint32_t index = 0; index < 池.capacity_get(); ++index)
				if (alive[index] != 0u)
					count += 1;
			return count;
		}
		//读取指定池索引的粒子状态（非活跃 / 越界返回假）
		bool state_get(uint32_t index, Particle_State& state) const
		{
			//渲染器关闭
			if (!渲染器.particle_enable_get())
				return false;
			//索引越界
			if (index >= 池.capacity_get())
				return false;
			//非活跃槽位
			if (池.alive_get()[index] == 0u)
				return false;
			state = 池.data_get()[index];
			return true;
		}
		//粒子池（供需要直接访问状态数组的用例）
		Particle_System& system_get(void)
		{
			return 池;
		}
		const Particle_System& system_get(void) const
		{
			return 池;
		}
		//重新绑定（池容量被外部改变后调用）
		void bind(void)
		{
			渲染器.particle_source_bind(池.data_get(), 池.alive_get(), 池.capacity_get());
		}

	private:
		//所属渲染器
		Renderer& 渲染器;
		//粒子池（模拟状态归应用层）
		Particle_System 池;
	};
}
