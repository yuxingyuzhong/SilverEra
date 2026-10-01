#pragma once
//预编译头
#include "common/前置头文件包含.h"

namespace engine
{
	//随机数生成器（按整数类型生成对应范围的随机数）
	template<typename T>
		requires std::is_integral_v<T>
	class Random_Generator
	{
		//PCG32 状态结构体
		struct Pcg32
		{
			uint64_t state = 0;
			uint64_t inc = 0;
		} g_pcg32;

		//随机数生成种子
		uint64_t seed = 0;

		//底层原始 32 位随机数生成（PCG32 核心）
		uint32_t pcg32_random_raw()
		{
			//获取旧状态
			uint64_t old_state = g_pcg32.state;
			//更新状态
			g_pcg32.state = old_state * 6364136223846793005ULL + g_pcg32.inc;
			//计算异或移位值
			uint32_t xorshifted = (uint32_t)(((old_state >> 18u) ^ old_state) >> 27u);
			//计算旋转量
			uint32_t rot = (uint32_t)(old_state >> 59u);
			//返回旋转后的结果
			return (xorshifted >> rot) | (xorshifted << ((-(int)rot) & 31));
		}

		//生成原始 64 位随机数
		uint64_t generate_raw_64()
		{
			//生成高 32 位
			uint64_t high = pcg32_random_raw();
			//生成低 32 位
			uint64_t low = pcg32_random_raw();
			//组合并返回
			return (high << 32) | low;
		}

		//在 [u_min, u_min + span - 1] 范围内生成随机数（span 为 0 表示 2^64 全范围）
		uint64_t generate_in_range(uint64_t u_min, uint64_t span)
		{
			//生成原始 64 位随机数
			uint64_t r = generate_raw_64();
			//若为全范围
			if (span == 0)
				return r;
			//若仅一个取值
			if (span == 1)
				return u_min;
			//获取 uint64_t 最大值
			uint64_t u_max = (std::numeric_limits<uint64_t>::max)();
			//拒绝采样保证无偏
			uint64_t limit = u_max - u_max % span;
			while (r >= limit)
				r = generate_raw_64();
			//加下界偏移返回
			return u_min + (r % span);
		}

	public:
		//默认构造函数
		Random_Generator()
		{
			//使用默认种子初始化
			pcg32_seed_init();
		}

		//使用指定种子构造
		explicit Random_Generator(uint64_t seed_out)
		{
			//使用指定种子初始化
			pcg32_seed_init(seed_out);
		}

		//使用指定种子初始化 PCG32
		void pcg32_seed_init(uint64_t seed_out)
		{
			//重置状态
			g_pcg32.state = 0;
			//设置增量
			g_pcg32.inc = (seed_out << 1u) | 1u;
			//预热一次
			(void)pcg32_random_raw();
		}

		//使用默认种子初始化 PCG32
		void pcg32_seed_init()
		{
			//静态计数器
			static uint64_t counter = 0;
			//获取当前时间
			auto now = std::chrono::high_resolution_clock::now();
			//转换为纳秒
			auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();
			//组合种子
			uint64_t seed = static_cast<uint64_t>(ns) ^ static_cast<uint64_t>(std::time(nullptr)) ^ (++counter);
			//重置状态
			g_pcg32.state = 0;
			//设置增量
			g_pcg32.inc = (seed << 1u) | 1u;
			//预热一次
			(void)pcg32_random_raw();
		}

		//生成 T 全范围随机数
		T operator()()
		{
			//范围下界（按无符号位模式）
			uint64_t u_min;
			//范围跨度（回绕为 0 表示 2^64 全范围）
			uint64_t span;
			//若为有符号整数类型
			if constexpr (std::is_signed_v<T>)
			{
				//下界为类型最小值
				u_min = static_cast<uint64_t>((std::numeric_limits<T>::min)());
				//跨度为最大值减最小值加一
				span = static_cast<uint64_t>((std::numeric_limits<T>::max)()) - u_min + 1ULL;
			}
			//若为无符号整数类型
			else
			{
				//下界为 0
				u_min = 0;
				//跨度为最大值加一
				span = static_cast<uint64_t>((std::numeric_limits<T>::max)()) + 1ULL;
			}
			//生成对应范围内的随机数
			return static_cast<T>(generate_in_range(u_min, span));
		}

		//生成 [min, max] 范围内的随机数
		T operator()(T min, T max)
		{
			//若最小值大于最大值
			if (min > max)
				//交换
				std::swap(min, max);
			//下界按无符号位模式
			uint64_t u_min = static_cast<uint64_t>(min);
			//跨度为最大值减下界加一
			uint64_t span = static_cast<uint64_t>(max) - u_min + 1ULL;
			//生成对应范围内的随机数
			return static_cast<T>(generate_in_range(u_min, span));
		}
	};
}