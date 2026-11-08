#pragma once
//预编译头
#include "Engine/EngineFoundation/common/前置头文件包含.h"

namespace engine
{
	//逻辑帧
	class Logics
	{
	public:
		//逻辑帧率设置
		static void logic_frames_set(const uint64_t& frame_rate)
		{
			//加阻塞锁避免外界读/写旧数据
			std::lock_guard<std::mutex> lock(frame_mtx);
			//修改逻辑帧率数值
			logic_frames = frame_rate;
		}
		//逻辑帧率读取
		static uint64_t logic_frames_get(void) 
		{
			//加阻塞锁避免外界读/写旧数据
			std::lock_guard<std::mutex> lock(frame_mtx);
			//修改逻辑帧率数值
			return logic_frames;
		}
		/*
		逻辑帧计数自增
		每个逻辑帧流逝时由外部主循环调用一次，
		计数只增不减，用于各模块自行取模判定周期内的作用时机。
		*/
		static void frame_tick(void)
		{
			//加阻塞锁避免外界读/写旧数据
			std::lock_guard<std::mutex> lock(frame_mtx);
			//递增逻辑帧计数
			++frame_count;
		}
		//逻辑帧计数读取
		static uint64_t frame_count_get(void)
		{
			//加阻塞锁避免外界读/写旧数据
			std::lock_guard<std::mutex> lock(frame_mtx);
			//返回逻辑帧计数
			return frame_count;
		}
		//逻辑帧计数设定
		static void frame_count_set(const uint64_t& count)
		{
			//加阻塞锁避免外界读/写旧数据
			std::lock_guard<std::mutex> lock(frame_mtx);
			//写入逻辑帧计数
			frame_count = count;
		}
	private:
		//逻辑帧率
		inline static uint64_t logic_frames = 0;
		//逻辑帧计数(逻辑帧流逝计数，只增不减)
		inline static uint64_t frame_count = 0;
		//逻辑帧率互斥锁
		inline static std::mutex frame_mtx;
	};
}