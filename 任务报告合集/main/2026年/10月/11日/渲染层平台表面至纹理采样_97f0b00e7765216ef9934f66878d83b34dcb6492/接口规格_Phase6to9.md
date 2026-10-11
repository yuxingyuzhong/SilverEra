# 渲染层 Phase 6~9 接口规格（权威契约 · 实现必须逐字对齐）

> 本文件是 `Application/Renderer` 层 Phase 6~9 各新增模块的**接口契约**。实现者不得改动这里列出的签名、成员与语义；如需偏离，先回报。
> 依据：`任务报告合集/main/2026年/10月/10日/渲染层架构设计_待实施/渲染层架构设计.md` ＋ 本轮用户决策 U3~U6。
> 编码风格（硬约束，见《执行计划》第一节）：TAB 缩进；注释中文、`//` 连续少于三行、三行及以上用 `/* */`、**换行注释**（禁行尾注释）；大括号换行；单语句 `if/for` 不加代码块；禁异常、`new(std::nothrow)`；函数/变量小写下划线且**名词在前动词在后**；无参函数写 `(void)`；自定义类型首字母大写；文件名中文。

---

## 0. 全局事实（省去重复说明）

- 本层预编译头：`Application/Renderer/common/前置头文件包含.h`（每个头文件第一行 `#pragma once` 后即包含它）。
- 每个模块目录结构：`模块/对外头.h` ＋ `模块/局部命名空间使用.h` ＋ `模块/core/*.cpp`。`局部命名空间使用.h` 用 `using` 引入该模块 .cpp/.hpp 用到的全部类型（`using std::vector;` 等），模块内 .cpp 一律不写命名空间前缀。
- 唯一 `VkResult` 检查出口：`#include "Application/Renderer/src/render/Renderer/core/结果检查.h"`，提供 `bool vk_result_check(VkResult, const char* 调用点)` 与 `bool vk_result_acceptable(VkResult)`。
- 日志：`logger.info/warn/error(...)`（EngineCore 提供，随结果检查.h 一并可见）。
- 资源创建与销毁同属一个模块；`destroy` 幂等。
- 头文件若需暴露 Vulkan/VMA 类型，可以（模块头属本层实现细节，不进公共面）。
- **`Application/Renderer/CMakeLists.txt` 已由主控改好，实现者一律不得改动它**（新增模块目录由 GLOB 自动收编）。

## 1. 公共面（`Renderer/渲染器.h`）新增内容（U6）

```cpp
//顶点（位置 vec3 + 纹理坐标 vec2；步长 20 字节，与着色器顶点输入一致）
struct Vertex
{
	float position[3];
	float uv[2];
};

//网格数据（创建入参）
struct Mesh_Data
{
	std::vector<Vertex> vertices;
	std::vector<uint32_t> indices;
};

//网格句柄（0 表示无效）
using Mesh_Handle = uint64_t;
```

`class Renderer` 新增方法：

```cpp
//加载网格（顶点 / 索引缓冲上传至设备本地）；失败返回假，句柄保持无效
bool mesh_load(Mesh_Handle& handle_out, const Mesh_Data& data);
//卸载网格
void mesh_unload(Mesh_Handle handle);
//绘制网格（transform 为列主序 16 浮点模型矩阵，可为 nullptr 表示单位矩阵）
bool mesh_draw(Mesh_Handle handle, const float* transform);
//纹理加载（RGBA8 紧排像素；返回纹理句柄，0 表示失败）
bool texture_load(uint64_t& handle_out, const uint8_t* pixels, uint32_t width, uint32_t height);
//程序生成棋盘格纹理
bool texture_load_checkerboard(uint64_t& handle_out, uint32_t width, uint32_t height, uint32_t cell_size);
//设置当前绘制所用纹理（不设则用内置默认白纹理）
void texture_bind_set(uint64_t texture_handle);
//相机：位置 / 注视点 / 视场角（弧度） / 近远平面；aspect 默认取当前交换链宽高比
void camera_position_set(float x, float y, float z);
void camera_target_set(float x, float y, float z);
void camera_fov_set(float radians);
void camera_near_far_set(float near_plane, float far_plane);
```

> `渲染器.h` **只允许**出现上列引擎语义类型；不得出现任何 Vulkan / VMA / SDL3 / glm 类型。`Vertex` 与 `Mesh_Data` 定义在 `Mesh/网格数据.h`（Vulkan-free），由 `渲染器.h` include。

## 2. 共享类型头 `src/render/Mesh/网格数据.h`

Vulkan-free；是公共 API 与 Mesh 模块的共用型。

```cpp
#pragma once
//预编译头
#include "Application/Renderer/common/前置头文件包含.h"

namespace engine
{
	//顶点（位置 vec3 + 纹理坐标 vec2；步长 20 字节）
	struct Vertex { float position[3]; float uv[2]; };
	//网格数据
	struct Mesh_Data { std::vector<Vertex> vertices; std::vector<uint32_t> indices; };
}
```

## 3. `Shader/` 模块 —— `src/render/Shader/着色器.h`

```cpp
namespace engine
{
	/*
	着色器上下文
	把 .spv 字节码加载为 VkShaderModule；模块生命周期由调用方显式管理（创建与销毁同属本类）。
	生命周期：build → 使用 → destroy；destroy 幂等，析构兜底。
	*/
	class Shader_Context
	{
	public:
		Shader_Context(void) = default;
		~Shader_Context(void);
		Shader_Context(const Shader_Context&) = delete;
		Shader_Context& operator=(const Shader_Context&) = delete;

		//构建（记录逻辑设备；不加载任何着色器）
		bool build(VkDevice device);
		//销毁（幂等；仅复位状态，着色器模块须由调用方先行销毁）
		void destroy(void);
		//是否已构建完成
		bool valid(void) const;

		//设置着色器目录（空则用「exe 目录 / shaders」）
		void directory_set(const std::string& directory);
		//读取着色器文件字节（file_name 如 "场景.vert.spv"）
		bool file_bytes_read(std::vector<char>& bytes_out, const std::string& file_name) const;
		//加载着色器模块（内部经 file_bytes_read；失败返回假）
		bool module_load(VkShaderModule& module_out, const std::string& file_name) const;
		//销毁着色器模块
		void module_destroy(VkShaderModule module) const;
	private:
		//着色器目录解析（空 → Engine_Env 的 exe 目录 / shaders；实现见 core/着色器.cpp）
		std::string directory_resolve(void) const;
	private:
		VkDevice device = VK_NULL_HANDLE;
		std::string directory;
	};
}
```

- 文件定位**必须**走 `Engine_Env`（锚定 exe 目录，已处理中文 UTF-8→path）；不得用相对 CWD 的路径。
- 拆分为 `core/着色器.cpp`（build/destroy/valid/directory_set/directory_resolve）与 `core/着色器模块.cpp`（file_bytes_read/module_load/module_destroy）。

## 4. `Pipeline/` 模块 —— `src/render/Pipeline/管线.h`

```cpp
namespace engine
{
	//图形管线配置
	struct Pipeline_Config
	{
		//颜色附件格式（交换链格式）
		VkFormat color_format = VK_FORMAT_UNDEFINED;
		//深度附件格式
		VkFormat depth_format = VK_FORMAT_UNDEFINED;
		//在途帧数（描述符集按帧槽分配）
		uint32_t frame_overlap = 2;
	};

	class Pipeline_Context
	{
	public:
		Pipeline_Context(void) = default;
		~Pipeline_Context(void);
		Pipeline_Context(const Pipeline_Context&) = delete;
		Pipeline_Context& operator=(const Pipeline_Context&) = delete;

		//构建（建立描述符集布局 set0=场景UBO / set1=采样器、管线布局、描述符池并分配每帧槽描述符集）
		bool build(VkDevice device, const Pipeline_Config& config);
		//销毁（幂等）
		void destroy(void);
		//是否已构建完成
		bool valid(void) const;

		//构建图形管线（顶点输入 position vec3 + uv vec2；推送常量 mat4 模型矩阵；深度测试开 / 写开；背面剔除 BACK）
		bool pipeline_build(VkShaderModule vert_module, VkShaderModule frag_module);

		VkPipeline pipeline_get(void) const;
		VkPipelineLayout layout_get(void) const;
		//第 slot 帧槽的场景 UBO 描述符集（set 0）
		VkDescriptorSet scene_set_get(uint32_t slot) const;
		//第 slot 帧槽的采样器描述符集（set 1）
		VkDescriptorSet sampler_set_get(uint32_t slot) const;
		//向第 slot 帧槽写场景 UBO（set 0）
		bool scene_bind(uint32_t slot, VkBuffer ubo_buffer);
		//向第 slot 帧槽写采样器（set 1）
		bool texture_bind(uint32_t slot, VkSampler sampler, VkImageView view);
	private:
		//描述符集布局创建（实现见 core/描述符布局.cpp）
		bool descriptor_layouts_create(void);
		//管线布局创建（实现见 core/管线布局.cpp）
		bool pipeline_layout_create(void);
		//图形管线创建（实现见 core/图形管线.cpp）
		bool graphics_pipeline_create(VkShaderModule vert_module, VkShaderModule frag_module);
	private:
		VkDevice device = VK_NULL_HANDLE;
		Pipeline_Config config;
		VkDescriptorSetLayout scene_set_layout = VK_NULL_HANDLE;    //set 0：场景 UBO
		VkDescriptorSetLayout sampler_set_layout = VK_NULL_HANDLE;  //set 1：采样器
		VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
		VkDescriptorPool descriptor_pool = VK_NULL_HANDLE;
		std::vector<VkDescriptorSet> scene_sets;    //长度 = frame_overlap
		std::vector<VkDescriptorSet> sampler_sets;  //长度 = frame_overlap
		VkPipeline pipeline = VK_NULL_HANDLE;
	};
}
```

- 推送常量：`VkPushConstantRange{ stageFlags = VK_SHADER_STAGE_VERTEX_BIT, offset = 0, size = 64 }`（mat4）。
- 描述符池容量：`VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER` × frame_overlap ＋ `VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER` × frame_overlap。
- 顶点绑定：`binding 0，stride = sizeof(Vertex) = 20`；属性 0 = `VK_FORMAT_R32G32B32_SFLOAT`（offset 0），属性 1 = `VK_FORMAT_R32G32_SFLOAT`（offset 12）。
- 动态渲染：`VkPipelineRenderingCreateInfo{ colorAttachmentCount=1, pColorAttachmentFormats=&config.color_format, depthAttachmentFormat=config.depth_format }` 经 `pNext` 链入 `VkGraphicsPipelineCreateInfo`。
- 拆分为 `core/管线.cpp`（build/destroy/valid/scene_bind/texture_bind/两个 getter）、`core/描述符布局.cpp`、`core/管线布局.cpp`、`core/图形管线.cpp`。

## 5. `Mesh/` 模块 —— `src/render/Mesh/网格.h`

```cpp
namespace engine
{
	//网格记录（顶点 / 索引缓冲与其索引数）
	struct Mesh_Record
	{
		VkBuffer vertex_buffer = VK_NULL_HANDLE;
		VmaAllocation vertex_allocation = VK_NULL_HANDLE;
		VkBuffer index_buffer = VK_NULL_HANDLE;
		VmaAllocation index_allocation = VK_NULL_HANDLE;
		uint32_t index_count = 0;
		bool valid = false;
	};

	class Mesh_Context
	{
	public:
		Mesh_Context(void) = default;
		~Mesh_Context(void);
		Mesh_Context(const Mesh_Context&) = delete;
		Mesh_Context& operator=(const Mesh_Context&) = delete;

		//构建（记录内存模块；不创建任何网格）
		bool build(Memory_Context& memory);
		//销毁（幂等；销毁全部网格缓冲）
		void destroy(void);
		//是否已构建完成
		bool valid(void) const;

		//创建网格（顶点 / 索引缓冲上传至设备本地；句柄自 1 起，0 表无效）
		bool mesh_create(Mesh_Handle& handle_out, const Mesh_Data& data);
		//销毁网格
		void mesh_destroy(Mesh_Handle handle);
		//取网格缓冲与索引数（句柄无效返回假）
		bool mesh_record_get(Mesh_Handle handle, VkBuffer& vertex_buffer_out, VkBuffer& index_buffer_out, uint32_t& index_count_out) const;

		//生成三角形（顶点 3 / 索引 3）
		static Mesh_Data triangle_generate(void);
		//生成轴对齐立方体（边长 2*half_extent；24 顶点 / 36 索引）
		static Mesh_Data cube_generate(float half_extent);
	private:
		Memory_Context* memory = nullptr;
		std::vector<Mesh_Record> records;   //下标 = 句柄 - 1
	};
}
```

- 顶点/索引缓冲：设备本地（`host_visible = false`）＋ 经 `Memory_Context::upload_buffer` 上传（内部做暂存）。
- 顶点属性与着色器一致：position vec3 + uv vec2。
- 拆分为 `core/网格缓冲.cpp`（build/destroy/valid/mesh_create/mesh_destroy/mesh_record_get）与 `core/网格加载器.cpp`（triangle_generate/cube_generate）。

## 6. `Scene/` 模块 —— `src/render/Scene/相机.h`

```cpp
namespace engine
{
	/*
	相机
	生成视图矩阵与透视投影矩阵；投影按 Vulkan 约定（深度 Z ∈ [0,1]）构造并翻转 Y（K1）。
	内部用 glm 计算，但本头不出现 glm 类型。
	*/
	class Camera
	{
	public:
		Camera(void) = default;

		void position_set(float x, float y, float z);
		void target_set(float x, float y, float z);
		void up_set(float x, float y, float z);
		void fov_set(float radians);
		void aspect_set(float aspect);
		void near_far_set(float near_plane, float far_plane);

		//绕「上方向」围绕注视点轨道旋转（供演示；弧度）
		void orbit(float radians_delta);

		//写出「视图 × 投影」矩阵（列主序 16 浮点，可直接写入 UBO）
		void view_proj_get(float out[16]) const;
	private:
		float position[3] = { 0.0f, 0.0f, 3.0f };
		float target[3] = { 0.0f, 0.0f, 0.0f };
		float up[3] = { 0.0f, 1.0f, 0.0f };
		float fov = 1.0472f;      //60 度
		float aspect = 16.0f / 9.0f;
		float near_plane = 0.1f;
		float far_plane = 100.0f;
	};
}
```

- 实现**必须**用 `#include <glm/glm.hpp>` ＋ `<glm/gtc/matrix_transform.hpp>`；`GLM_FORCE_DEPTH_ZERO_TO_ONE` 与 `GLM_FORCE_RADIANS` 已由 CMake 以 PRIVATE 定义给出（不要在本层再定义，也不得写进任何头文件）。
- 投影用 `glm::perspectiveZO`（或 `glm::perspective` 配合 `GLM_FORCE_DEPTH_ZERO_TO_ONE`）；Y 翻转：对投影矩阵 `proj[1][1] *= -1.0f`（K1）。
- 单文件实现：`core/相机.cpp`。

## 7. `Texture/` 模块 —— `src/render/Texture/纹理.h`

```cpp
namespace engine
{
	//纹理记录（图像 + 视图 + 采样器）
	struct Texture_Record
	{
		VkImage image = VK_NULL_HANDLE;
		VmaAllocation allocation = VK_NULL_HANDLE;
		VkImageView view = VK_NULL_HANDLE;
		VkSampler sampler = VK_NULL_HANDLE;
		bool valid = false;
	};

	class Texture_Context
	{
	public:
		Texture_Context(void) = default;
		~Texture_Context(void);
		Texture_Context(const Texture_Context&) = delete;
		Texture_Context& operator=(const Texture_Context&) = delete;

		//构建（记录内存模块与逻辑设备）
		bool build(Memory_Context& memory, VkDevice device);
		//销毁（幂等；销毁全部纹理）
		void destroy(void);
		//是否已构建完成
		bool valid(void) const;

		//从内存像素创建纹理（RGBA8 紧排；句柄自 1 起，0 表无效）—— 预留的文件加载入口
		bool texture_create_from_memory(uint64_t& handle_out, const uint8_t* pixels, uint32_t width, uint32_t height);
		//程序生成棋盘格纹理（RGBA8，两种灰度交替）
		bool texture_create_checkerboard(uint64_t& handle_out, uint32_t width, uint32_t height, uint32_t cell_size);
		//销毁纹理
		void texture_destroy(uint64_t handle);
		//取图像视图与采样器（供描述符写入；句柄无效返回假）
		bool texture_bind_get(uint64_t handle, VkImageView& view_out, VkSampler& sampler_out) const;
	private:
		//创建采样器（线性过滤 / REPEAT 寻址 / mipmap 关闭；实现见 core/采样器创建.cpp）
		bool sampler_create(VkSampler& sampler_out);
		Memory_Context* memory = nullptr;
		VkDevice device = VK_NULL_HANDLE;
		std::vector<Texture_Record> records;   //下标 = 句柄 - 1
	};
}
```

- 图像创建走 `Memory_Context::image_create`（格式 `VK_FORMAT_R8G8B8A8_SRGB`，usage 含 `SAMPLED` ＋ `TRANSFER_DST`）；像素经 `Memory_Context::upload_image` 上传。
- 拆分为 `core/纹理创建.cpp`（build/destroy/valid/三个 create/destroy/bind_get）与 `core/采样器创建.cpp`。

## 8. `Frame/` 模块改动（U3）

`Frame_Render_Item` 新增于 `帧上下文.h`：

```cpp
	/*
	渲染项
	持裸 Vulkan 句柄，由门面组装后交给帧模块录制（不引入模块间依赖边）。
	*/
	struct Frame_Render_Item
	{
		//图形管线
		VkPipeline pipeline = VK_NULL_HANDLE;
		//管线布局（推送常量与描述符集绑定需要）
		VkPipelineLayout layout = VK_NULL_HANDLE;
		//场景 UBO 描述符集（set 0；可为空）
		VkDescriptorSet scene_set = VK_NULL_HANDLE;
		//采样器描述符集（set 1；可为空）
		VkDescriptorSet sampler_set = VK_NULL_HANDLE;
		//描述符集数量（0/1/2）
		uint32_t descriptor_set_count = 0;
		//顶点缓冲
		VkBuffer vertex_buffer = VK_NULL_HANDLE;
		//索引缓冲
		VkBuffer index_buffer = VK_NULL_HANDLE;
		//索引数
		uint32_t index_count = 0;
		//推送常量数据（可为空）
		const void* push_constants = nullptr;
		//推送常量字节数（0 表不推送）
		uint32_t push_constant_size = 0;
	};
```

`Frame_Context::render` 签名改为：

```cpp
		/*
		渲染一帧（取图像 → 录制 → 提交 → 呈现）
		swapchain：当前交换链；rebuild_needed_out：置真表示本帧遇到过期 / 次优，需上层重建；
		items / item_count：渲染项列表（可为空，此时只清屏）；clear_color：清屏色。
		*/
		bool render(Swapchain_Context& swapchain, const Frame_Render_Item* items, uint32_t item_count,
			bool& rebuild_needed_out, const float clear_color[4]);
```

- `command_record` 增参 `const Frame_Render_Item* items, uint32_t item_count`：清屏后对每个有效项依次 `vkCmdBindPipeline` → `vkCmdBindDescriptorSets` →（有推送常量时）`vkCmdPushConstants` → `vkCmdBindVertexBuffers` → `vkCmdBindIndexBuffer` → `vkCmdDrawIndexed`。
- **深度附件**（U5）：`Swapchain_Context` 新增深度图像（**每交换链图像一张**，随交换链重建重建）：

```cpp
		//获取指定索引的深度图像 / 视图
		VkImage depth_image_get(uint32_t index) const;
		VkImageView depth_view_get(uint32_t index) const;
		//获取深度格式（已选定的设备支持格式）
		VkFormat depth_format_get(void) const;
```

  `Swapchain_Context::build` 增参 `Memory_Context& memory`（放第二位）；深度格式选择规则：优先 `VK_FORMAT_D32_SFLOAT`，回退 `D32_SFLOAT_S8_UINT` → `D24_UNORM_S8_UINT`，均不可用则 `logger.error` 返回假（先经 `vkGetPhysicalDeviceFormatProperties` 的 `optimalTilingFeatures` 查 `DEPTH_STENCIL_ATTACHMENT_BIT`）。
  深度布局转换：`UNDEFINED → DEPTH_ATTACHMENT_OPTIMAL`（`srcStage=TOP_OF_PIPE`、`srcAccess=0`、`dstStage=EARLY_FRAGMENT_TESTS|LATE_FRAGMENT_TESTS`、`dstAccess=DEPTH_STENCIL_ATTACHMENT_WRITE`）；本层不做后续读回，第二道转换转 `DEPTH_ATTACHMENT_OPTIMAL` 保持不变即可（首帧前转一次，后续帧沿用，可用「是否首次」标志记录）。
- 动态渲染需 `VkRenderingAttachmentInfo` 增深度附件：`VkRenderingAttachmentInfo{ imageView=depth_view, imageLayout=DEPTH_ATTACHMENT_OPTIMAL, loadOp=CLEAR, storeOp=STORE, clearValue.depthStencil={1.0f,0} }` 挂到 `VkRenderingInfo::pDepthAttachment`。

## 9. `Renderer/` 门面改动

`Renderer_Config` 不新增字段（`shader_dir` 已存在）。`Renderer::Impl` 增持 `Shader_Context shader`、`Pipeline_Context pipeline`、`Mesh_Context mesh`、`Texture_Context texture`、`Camera camera`、`std::vector<VkBuffer> scene_ubos` ＋ `std::vector<VmaAllocation> scene_ubo_allocs`（长度 = frame_overlap，持久映射）＋ `std::vector<Frame_Render_Item> pending_items` ＋ `uint64_t current_texture` ＋ `uint64_t default_texture`。

`init` 顺序（在 Phase 5 的 ⑧ 之后追加）：

```
⑨ Shader：build(device)；directory_set(config.shader_dir)
⑩ 加载 场景.vert.spv / 场景.frag.spv → VkShaderModule
⑪ Pipeline：build(device, {color_format = swapchain.format_get(), depth_format = swapchain.depth_format_get(), frame_overlap})
⑫ Pipeline：pipeline_build(vert, frag)
⑬ 场景 UBO：frame_overlap 个 host_visible 缓冲（大小 64），写入相机的 view_proj，scene_bind(slot, ubo)
⑭ Texture：build(memory, device)；生成内置默认白纹理（2×2 全白）→ default_texture；texture_bind(slot, 默认采样器与视图)
⑮ Mesh：build(memory)
⑯ 相机 aspect_set(交换链宽高比)
```

- `frame_render`：在调用 `frame.render` 之前，用 `camera.view_proj_get` 刷新当前帧槽的 UBO（`buffer_write`）与 `scene_bind`；组装 `pending_items`（长度 0 或 1，由 `mesh_draw` 填充）后传入。
- `mesh_load` → `mesh.mesh_create`；`mesh_draw` → 记录一个待绘制项（含 `pipeline_get()` / `layout_get()` / `scene_set_get(slot)` / `sampler_set_get(slot)` / `mesh_record_get` / 推送常量 = transform）。
- `shutdown` 逆序补：`mesh.destroy` → `texture.destroy` → 场景 UBO 销毁 → `pipeline.destroy` → 着色器模块销毁 → `shader.destroy`，全部排在 `frame.destroy` 之后、`swapchain.destroy` 之前（**注意**：管线与纹理依赖设备与内存，必须在 `memory.destroy` / `device.destroy` 之前；交换链销毁会连带销毁深度图像，故深度相关不能在交换链之后使用）。
- 建议实际销毁顺序：`frame.destroy` → `mesh.destroy` → `texture.destroy` → 场景 UBO → `pipeline.destroy` → 着色器模块 → `shader.destroy` → `swapchain.destroy` → `memory.destroy` → `device.destroy` → `platform.destroy`。

## 10. 着色器（已由主控写定，不要改）

`assets/shaders/场景.vert` / `场景.frag`，产物 `场景.vert.spv` / `场景.frag.spv`。
- vert：`location 0 = vec3 in_position`，`location 1 = vec2 in_uv`；`out_uv`；`set 0 binding 0` = `mat4 view_proj`；`push_constant` = `mat4 model`；`gl_Position = view_proj * model * vec4(position, 1)`。
- frag：`set 1 binding 0` = `sampler2D albedo`；`out_color = texture(albedo, in_uv)`。
