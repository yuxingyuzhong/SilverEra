#pragma once
//预编译头
#include "Application/Renderer/common/前置头文件包含.h"
//渲染器门面（资源创建 / 卸载 / 绘制登记）
#include "Application/Renderer/src/render/Renderer/渲染器.h"

/*
资产缓存桥（应用层；渲染层职责下沉第三批）
渲染层此前自带的「路径键 ＋ 引用计数 ＋ 异步解析」属于资源生命周期管理，不是绘制职责，
现由应用层承担；渲染层只保留单次加载 / 卸载（mesh_load_obj / texture_load_file / mesh_unload 等）。

缓存登记按「渲染器实例 ＋ 归一化路径」索引：同一路径只创建一次资源，
释放时引用计数归零才真正调用渲染层卸载。异步任务同样由本层用 std::async 调度，
解析完成后在主线程轮询调用渲染层建资源。
*/
namespace engine
{
	/*
	异步网格任务（公共头只持有不透明状态，不引底层并发类型）
	由 `asset_mesh_async_load` 建立，由 `asset_mesh_async_poll` 在主线程轮询。
	*/
	struct Asset_Async_Task
	{
		//内部状态（解析结果 ＋ 完成标记 ＋ 已建句柄）
		std::shared_ptr<void> state;
	};

	/*
	共享加载网格
	同路径只创建一次：首次创建（refs = 1）、后续命中仅引用计数 +1 并返回同一句柄；
	释放须走 `asset_mesh_release`。路径按 UTF-8 解释，键做「分隔符统一 ＋ ASCII 小写」归一化。
	*/
	bool asset_mesh_shared_load(Renderer& renderer, Mesh_Handle& handle_out, const std::string& path);

	/*
	释放共享网格（引用计数 -1；归零才真正卸载并删缓存条目）
	未登记的句柄按非共享资源直接卸载（兼容混用）。
	*/
	void asset_mesh_release(Renderer& renderer, Mesh_Handle handle);

	/*
	共享加载纹理文件
	语义同 `asset_mesh_shared_load`；释放须走 `asset_texture_release`。
	*/
	bool asset_texture_shared_load(Renderer& renderer, uint64_t& handle_out, const std::string& path);

	//释放共享纹理（引用计数 -1；归零才真正卸载）
	void asset_texture_release(Renderer& renderer, uint64_t handle);

	//当前缓存条目数（网格 ＋ 纹理；只读诊断）
	uint32_t asset_cache_count(Renderer& renderer);

	//清空指定渲染器的缓存登记（只清表，不卸载资源；渲染器销毁后调用以释放登记）
	void asset_cache_clear(Renderer& renderer);

	/*
	提交异步网格加载
	解析在后台线程执行，本调用立即返回；任务状态写入 task。
	`asset_mesh_async_poll` 在主线程轮询：未完成返回假，完成时建资源并写出句柄。
	路径为空 / 任务已存在时返回假（task.state 保持为空或原值）。
	*/
	bool asset_mesh_async_load(Renderer& renderer, Asset_Async_Task& task, const std::string& path);

	/*
	轮询异步网格任务
	未完成 → 返回假且 handle_out = 0；解析失败 → 返回假（后续轮询仍假）；
	成功 → 首次轮询建网格资源并持有句柄，后续轮询返回同一句柄（幂等）。
	*/
	bool asset_mesh_async_poll(Renderer& renderer, Asset_Async_Task& task, Mesh_Handle& handle_out);
}
