//资产缓存桥实现：路径键 ＋ 引用计数 ＋ 异步解析（应用层职责）
#include "src/assets/资产缓存.h"

//日志系统
#include "Engine/EngineCore/src/tools/Logging/日志系统运行包.h"
//OBJ 解析结果类型（后台线程解析，主线程建资源）
#include "Engine/EngineSystem/src/core/Renderer/src/render/Asset/资产加载.h"

//异步任务与容器
#include <cstdint>
#include <future>
#include <map>
#include <memory>
#include <string>
#include <utility>

namespace engine
{
	namespace
	{
		//缓存条目（句柄 ＋ 引用计数）
		struct 缓存条目
		{
			//资源句柄
			uint64_t handle = 0;
			//引用计数（≥ 1）
			uint32_t refs = 0;
		};

		//某个渲染器实例的两类缓存
		struct 渲染器缓存
		{
			//网格缓存（键形如 "mesh:<路径>"）
			std::map<std::string, 缓存条目> mesh;
			//纹理缓存（键形如 "texture:<路径>"）
			std::map<std::string, 缓存条目> texture;
		};

		/*
		异步网格任务状态
		future 完成后才读取 data；handle / built 只在主线程轮询里写，故无需加锁。
		*/
		struct 异步网格状态
		{
			//后台解析任务（结果为「是否解析成功」）
			std::future<bool> future;
			//解析结果（顶点 / 索引 / 分段 / 材质名）
			Mesh_File_Data data;
			//是否已取过 future 结果
			bool settled = false;
			//解析是否成功
			bool parse_ok = false;
			//是否已在主线程建好网格
			bool built = false;
			//建好的网格句柄
			Mesh_Handle handle = 0;
		};

		//按渲染器实例索引的缓存表（进程内单线程使用）
		std::map<const Renderer*, 渲染器缓存> 缓存表;

		//取（必要时创建）指定渲染器的缓存
		渲染器缓存& 缓存取值(const Renderer& renderer)
		{
			return 缓存表[&renderer];
		}

		/*
		缓存键（第四百二十五轮口径）
		形如「前缀:归一化路径」：分隔符统一为 '/'、ASCII 字母转小写（Windows 路径不区分大小写）。
		*/
		std::string 缓存键构建(const char* prefix, const std::string& path)
		{
			std::string key(prefix);
			key.push_back(':');
			key.reserve(key.size() + path.size());
			for (char ch : path)
			{
				char value = ch;
				//路径分隔符统一
				if (value == '\\')
					value = '/';
				//ASCII 大写转小写（非 ASCII 字节原样保留）
				if (value >= 'A' && value <= 'Z')
					value = static_cast<char>(value - 'A' + 'a');
				key.push_back(value);
			}
			return key;
		}
	}

	//共享加载网格
	bool asset_mesh_shared_load(Renderer& renderer, Mesh_Handle& handle_out, const std::string& path)
	{
		//默认无有效句柄
		handle_out = 0;
		//归一化键
		渲染器缓存& 缓存 = 缓存取值(renderer);
		const std::string key = 缓存键构建("mesh", path);
		auto found = 缓存.mesh.find(key);
		//命中：仅引用计数加一
		if (found != 缓存.mesh.end())
		{
			found->second.refs += 1;
			handle_out = static_cast<Mesh_Handle>(found->second.handle);
			return true;
		}
		//首次：正常加载后登记（失败不入缓存）
		Mesh_Handle 句柄 = 0;
		if (!renderer.mesh_load_obj(句柄, path))
			return false;
		缓存条目 entry;
		entry.handle = 句柄;
		entry.refs = 1;
		缓存.mesh.emplace(key, entry);
		handle_out = 句柄;
		return true;
	}

	//释放共享网格
	void asset_mesh_release(Renderer& renderer, Mesh_Handle handle)
	{
		//无效句柄
		if (handle == 0)
			return;
		渲染器缓存& 缓存 = 缓存取值(renderer);
		//按句柄查找登记项（条目数很少，线性查找足够）
		for (auto it = 缓存.mesh.begin(); it != 缓存.mesh.end(); ++it)
		{
			//跳过非目标项
			if (it->second.handle != handle)
				continue;
			//仍有其它引用：只减计数
			if (it->second.refs > 1)
			{
				it->second.refs -= 1;
				return;
			}
			//归零：删条目后真正卸载
			缓存.mesh.erase(it);
			renderer.mesh_unload(handle);
			return;
		}
		//未登记：按非共享资源处理（兼容混用）
		renderer.mesh_unload(handle);
	}

	//共享加载纹理
	bool asset_texture_shared_load(Renderer& renderer, uint64_t& handle_out, const std::string& path)
	{
		//默认无有效句柄
		handle_out = 0;
		渲染器缓存& 缓存 = 缓存取值(renderer);
		const std::string key = 缓存键构建("texture", path);
		auto found = 缓存.texture.find(key);
		//命中：仅引用计数加一
		if (found != 缓存.texture.end())
		{
			found->second.refs += 1;
			handle_out = found->second.handle;
			return true;
		}
		//首次：正常加载后登记
		uint64_t 句柄 = 0;
		if (!renderer.texture_load_file(句柄, path))
			return false;
		缓存条目 entry;
		entry.handle = 句柄;
		entry.refs = 1;
		缓存.texture.emplace(key, entry);
		handle_out = 句柄;
		return true;
	}

	//释放共享纹理
	void asset_texture_release(Renderer& renderer, uint64_t handle)
	{
		//无效句柄
		if (handle == 0)
			return;
		渲染器缓存& 缓存 = 缓存取值(renderer);
		for (auto it = 缓存.texture.begin(); it != 缓存.texture.end(); ++it)
		{
			//跳过非目标项
			if (it->second.handle != handle)
				continue;
			//仍有其它引用：只减计数
			if (it->second.refs > 1)
			{
				it->second.refs -= 1;
				return;
			}
			//归零：删条目后真正卸载
			缓存.texture.erase(it);
			renderer.texture_unload(handle);
			return;
		}
		//未登记：按非共享资源处理
		renderer.texture_unload(handle);
	}

	//当前缓存条目数
	uint32_t asset_cache_count(Renderer& renderer)
	{
		渲染器缓存& 缓存 = 缓存取值(renderer);
		return static_cast<uint32_t>(缓存.mesh.size() + 缓存.texture.size());
	}

	//清空缓存登记
	void asset_cache_clear(Renderer& renderer)
	{
		缓存表.erase(&renderer);
	}

	//提交异步网格加载
	bool asset_mesh_async_load(Renderer& renderer, Asset_Async_Task& task, const std::string& path)
	{
		(void)renderer;
		//路径为空 / 已有在途任务
		if (path.empty() || task.state != nullptr)
			return false;
		//任务状态
		auto state = std::make_shared<异步网格状态>();
		//后台解析（结果直接写进 state->data；future 的同步语义保证轮询时可见）
		state->future = std::async(std::launch::async, [state, path]() -> bool
		{
			return obj_mesh_load(path, state->data);
		});
		task.state = state;
		return true;
	}

	//轮询异步网格任务
	bool asset_mesh_async_poll(Renderer& renderer, Asset_Async_Task& task, Mesh_Handle& handle_out)
	{
		//默认无句柄
		handle_out = 0;
		//无任务
		if (task.state == nullptr)
			return false;
		auto state = std::static_pointer_cast<异步网格状态>(task.state);
		//已建好：幂等返回
		if (state->built)
		{
			handle_out = state->handle;
			return true;
		}
		//解析失败已定案
		if (state->settled && !state->parse_ok)
			return false;
		//尚未完成
		if (!state->settled)
		{
			//非阻塞查询
			if (state->future.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
				return false;
			//取结果（此后 data 才可安全读取）
			state->parse_ok = state->future.get();
			state->settled = true;
			//解析失败
			if (!state->parse_ok)
				return false;
		}
		//成功：主线程建网格（分段随数据一并提交）
		Mesh_Handle 句柄 = 0;
		if (state->data.sections.empty())
		{
			//无分段：按纯网格创建
			if (!renderer.mesh_load(句柄, state->data.data))
				return false;
		}
		else
		{
			//有分段：逐段提交
			if (!renderer.mesh_load_sections(句柄, state->data.data,
				state->data.sections.data(), static_cast<uint32_t>(state->data.sections.size())))
				return false;
		}
		state->handle = 句柄;
		state->built = true;
		handle_out = 句柄;
		return true;
	}
}
