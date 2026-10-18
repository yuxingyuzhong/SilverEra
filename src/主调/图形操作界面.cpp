//图形操作界面实现：渲染层 UI 通道（SDL3 + Vulkan + Dear ImGui）的常驻测试操作界面
#include "src/主调/图形操作界面.h"

//渲染器门面与 UI 通道（含 Vulkan / SDL3 类型；相关包含根由渲染层对外面传递）
#include "Application/Renderer/src/render/Renderer/渲染器.h"
#include "Application/Renderer/src/render/Renderer/渲染器_UI通道.h"

//测试运行器与测试界面
#include "src/主调/测试运行器.h"
#include "src/主调/测试界面.h"

//Dear ImGui 与两条后端（平台后端用 SDL3、渲染后端用 Vulkan）
#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_vulkan.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

//引擎命名空间
namespace engine
{
    //文件内部工具
    namespace
    {
        //临时窗口尺寸
        const uint32_t 窗口宽度 = 1280;
        const uint32_t 窗口高度 = 800;
        //窗口标题
        const char* 窗口标题 = "白银纪元 · 测试操作界面";
        //中文字体像素大小
        const float 字体像素大小 = 19.0f;
        //Vulkan 后端自建内部描述符池时的采样图像描述符数
        const uint32_t 描述符池规模 = 64;

        //收集候选中文字体路径（与系统层配置编辑器保持同一回退顺序）
        std::vector<std::string> 收集候选字体路径()
        {
            std::vector<std::string> 候选;
#ifdef _WIN32
            std::string 字体目录;
            //优先用 %WINDIR% 定位字体目录，兼容系统盘不在 C 的情况
            if (const char* 系统目录 = std::getenv("WINDIR"))
                字体目录 = std::string(系统目录) + "/Fonts/";
            else
                字体目录 = "C:/Windows/Fonts/";

            候选.push_back(字体目录 + "msyh.ttc");      //微软雅黑
            候选.push_back(字体目录 + "msyhbd.ttc");    //微软雅黑 Bold
            候选.push_back(字体目录 + "msyhl.ttc");     //微软雅黑 Light
            候选.push_back(字体目录 + "Deng.ttf");      //等线
            候选.push_back(字体目录 + "simhei.ttf");    //黑体
            候选.push_back(字体目录 + "simsun.ttc");    //宋体
            候选.push_back(字体目录 + "simkai.ttf");    //楷体
#else
            候选.push_back("/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc");
            候选.push_back("/usr/share/fonts/truetype/wqy/wqy-microhei.ttc");
            候选.push_back("/usr/share/fonts/truetype/wqy/wqy-zenhei.ttc");
            候选.push_back("/System/Library/Fonts/PingFang.ttc");
            候选.push_back("/System/Library/Fonts/Arial Unicode.ttf");
#endif
            return 候选;
        }

        //尝试加载中文字体，成功返回 true
        bool 加载中文字体(float 像素大小)
        {
            ImGuiIO& io = ImGui::GetIO();

            //合并「中文全范围 + 界面用到的特殊符号」，避免渲染成 '?'
            ImFontGlyphRangesBuilder 字形构建器;
            字形构建器.AddRanges(io.Fonts->GetGlyphRangesChineseFull());
            字形构建器.AddChar(0x266A);   //♪ 音符
            字形构建器.AddChar(0x2022);   //• 项目符号
            字形构建器.AddChar(0x2026);   //… 省略号
            字形构建器.AddChar(0x2713);   //✓ 通过
            ImVector<ImWchar> 字形范围;
            字形构建器.BuildRanges(&字形范围);

            //按优先级尝试候选字体，命中即用
            const std::vector<std::string> 候选字体 = 收集候选字体路径();
            for (const std::string& 路径 : 候选字体)
            {
                //先确认文件存在，避免 ImGui 逐个打印加载失败日志
                std::error_code 错误码;
                if (!std::filesystem::is_regular_file(std::filesystem::path(路径), 错误码))
                    continue;
                //加载成功即返回
                if (io.Fonts->AddFontFromFileTTF(路径.c_str(), 像素大小, nullptr, 字形范围.Data) != nullptr)
                    return true;
            }

            //全部失败：回退默认字体（中文会显示为方块）
            return false;
        }

        //渐变三段色：顶端深蓝、中段中蓝、底端亮蓝（背景绘制用）
        const ImVec4 渐变顶端色(0.05f, 0.11f, 0.26f, 1.00f);
        const ImVec4 渐变中段色(0.12f, 0.28f, 0.55f, 1.00f);
        const ImVec4 渐变底端色(0.26f, 0.52f, 0.85f, 1.00f);

        //应用蓝色主题（明丽版，与旧粉色主题不同）
        void 应用蓝色主题()
        {
            ImGuiStyle& 样式 = ImGui::GetStyle();
            ImVec4* 颜色 = 样式.Colors;

            //窗口底色全透明：让背景绘制列表里的蓝色渐变透出来
            颜色[ImGuiCol_WindowBg]       = ImVec4(0.10f, 0.22f, 0.44f, 0.00f);
            颜色[ImGuiCol_ChildBg]        = ImVec4(0.08f, 0.18f, 0.38f, 0.55f);
            颜色[ImGuiCol_PopupBg]        = ImVec4(0.12f, 0.25f, 0.50f, 0.98f);
            颜色[ImGuiCol_Border]         = ImVec4(0.55f, 0.78f, 1.00f, 0.55f);
            颜色[ImGuiCol_Text]           = ImVec4(0.96f, 0.98f, 1.00f, 1.00f);
            颜色[ImGuiCol_TextDisabled]   = ImVec4(0.72f, 0.82f, 0.95f, 1.00f);
            颜色[ImGuiCol_FrameBg]        = ImVec4(0.16f, 0.33f, 0.62f, 1.00f);
            颜色[ImGuiCol_FrameBgHovered] = ImVec4(0.22f, 0.44f, 0.76f, 1.00f);
            颜色[ImGuiCol_FrameBgActive]  = ImVec4(0.28f, 0.52f, 0.84f, 1.00f);
            颜色[ImGuiCol_Button]         = ImVec4(0.20f, 0.44f, 0.80f, 1.00f);
            颜色[ImGuiCol_ButtonHovered]  = ImVec4(0.32f, 0.60f, 0.94f, 1.00f);
            颜色[ImGuiCol_ButtonActive]   = ImVec4(0.15f, 0.35f, 0.68f, 1.00f);
            颜色[ImGuiCol_Header]         = ImVec4(0.24f, 0.48f, 0.82f, 0.95f);
            颜色[ImGuiCol_HeaderHovered]  = ImVec4(0.34f, 0.60f, 0.92f, 1.00f);
            颜色[ImGuiCol_HeaderActive]   = ImVec4(0.20f, 0.42f, 0.74f, 1.00f);
            颜色[ImGuiCol_CheckMark]      = ImVec4(0.62f, 0.86f, 1.00f, 1.00f);
            颜色[ImGuiCol_SliderGrab]     = ImVec4(0.40f, 0.68f, 0.96f, 1.00f);
            颜色[ImGuiCol_ScrollbarBg]    = ImVec4(0.08f, 0.18f, 0.38f, 0.80f);
            颜色[ImGuiCol_ScrollbarGrab]  = ImVec4(0.28f, 0.54f, 0.86f, 1.00f);
            颜色[ImGuiCol_TitleBg]        = ImVec4(0.16f, 0.36f, 0.68f, 1.00f);
            颜色[ImGuiCol_TitleBgActive]  = ImVec4(0.24f, 0.48f, 0.82f, 1.00f);

            //柔和圆角与内边距
            样式.WindowRounding = 8.0f;
            样式.FrameRounding  = 5.0f;
            样式.GrabRounding   = 5.0f;
            样式.WindowPadding  = ImVec2(12.0f, 12.0f);
            样式.FramePadding   = ImVec2(8.0f, 5.0f);
            样式.ItemSpacing    = ImVec2(8.0f, 6.0f);
        }

        //绘制蓝色渐变背景（两段连续垂直渐变：深蓝 → 中蓝 → 亮蓝）
        void 绘制蓝色渐变背景()
        {
            const ImGuiViewport* 视口 = ImGui::GetMainViewport();
            //渐变覆盖整个视口
            const ImVec2 左上 = 视口->Pos;
            const float 宽度 = 视口->Size.x;
            const float 中点 = 视口->Size.y * 0.5f;

            const ImU32 顶端色 = ImGui::GetColorU32(渐变顶端色);
            const ImU32 中段色 = ImGui::GetColorU32(渐变中段色);
            const ImU32 底端色 = ImGui::GetColorU32(渐变底端色);

            //背景绘制列表位于所有窗口之后
            ImDrawList* 绘制列表 = ImGui::GetBackgroundDrawList();
            绘制列表->AddRectFilledMultiColor(左上,
                ImVec2(左上.x + 宽度, 左上.y + 中点),
                顶端色, 顶端色, 中段色, 中段色);
            绘制列表->AddRectFilledMultiColor(
                ImVec2(左上.x, 左上.y + 中点),
                ImVec2(左上.x + 宽度, 左上.y + 视口->Size.y),
                中段色, 中段色, 底端色, 底端色);
        }

        //界面会话上下文（交给每帧绘制回调）
        struct Ui_会话上下文
        {
            //选择模型
            Test_Selection_Model* 模型 = nullptr;
            //测试运行器
            Test_Runner* 运行器 = nullptr;
            //界面请求退出（由界面按钮置位，主循环检查）
            bool 退出请求 = false;
        };

        //每帧 UI 绘制回调（渲染层帧链路在场景通道内、呈现转换前调用）
        void ui_绘制回调(VkCommandBuffer 命令缓冲, void* 用户数据)
        {
            Ui_会话上下文& 上下文 = *static_cast<Ui_会话上下文*>(用户数据);

            //本帧 ImGui 帧开始（界面在场景通道内构建并提交绘制）
            ImGui_ImplVulkan_NewFrame();
            ImGui_ImplSDL3_NewFrame();
            ImGui::NewFrame();

            //先铺蓝色渐变，再画操作界面
            绘制蓝色渐变背景();

            //铺满整个窗口的无标题面板
            const ImGuiViewport* 视口 = ImGui::GetMainViewport();
            ImGui::SetNextWindowPos(视口->WorkPos);
            ImGui::SetNextWindowSize(视口->WorkSize);
            ImGui::Begin("##测试操作界面", nullptr,
                ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);

            //绘制测试操作界面（树 / 控制 / 结果 / 输出镜像 / 轮次 / 日志）
            //注意：必须在面板窗口内绘制，控件才有有效窗口可放置与命中
            测试界面_绘制(*上下文.运行器, *上下文.模型, 上下文.退出请求);

            ImGui::End();

            //提交绘制：交给渲染层帧链路命令缓冲（后端画在已开启的动态渲染通道内）
            ImGui::Render();
            ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), 命令缓冲);
        }

        //SDL 事件转发回调（渲染层事件泵逐个事件转发；交给 ImGui 的 SDL3 后端处理输入）
        void ui_事件回调(const SDL_Event* 事件, void* 用户数据)
        {
            (void)用户数据;
            ImGui_ImplSDL3_ProcessEvent(事件);
        }
    }

    //========================================================================
    // 图形操作界面
    //========================================================================
    bool 图形操作界面_运行(Test_Selection_Model& 模型)
    {
        //—— 渲染器初始化（可见窗口；Vulkan 不可用则返回 false） ——
        Renderer_Config 配置{};
        配置.width = 窗口宽度;
        配置.height = 窗口高度;
        配置.title = 窗口标题;
        配置.frame_overlap = 2;
        配置.vsync = true;
        配置.hidden = false;

        Renderer 渲染器;
        if (!渲染器.init(配置))
            return false;

        //—— 取 UI 通道 ——
        Ui_Channel_Context* 通道 = 渲染器.ui_channel();
        if (通道 == nullptr || !通道->valid())
        {
            渲染器.shutdown();
            return false;
        }
        const Ui_Channel_Info& 通道信息 = 通道->info_get();

        //—— Dear ImGui 初始化 ——
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        //不写 imgui.ini
        io.IniFilename = nullptr;

        应用蓝色主题();

        //平台后端：绑定渲染层自建的 SDL3 窗口
        if (!ImGui_ImplSDL3_InitForVulkan(通道信息.window))
        {
            ImGui::DestroyContext();
            渲染器.shutdown();
            return false;
        }

        //Vulkan 后端：按渲染层 UI 通道信息填充（管线信息位于 PipelineInfoMain 下；描述符池交后端自建）
        ImGui_ImplVulkan_InitInfo 后端信息{};
        后端信息.ApiVersion = 通道信息.api_version;
        后端信息.Instance = 通道信息.instance;
        后端信息.PhysicalDevice = 通道信息.physical_device;
        后端信息.Device = 通道信息.device;
        后端信息.QueueFamily = 通道信息.graphics_family;
        后端信息.Queue = 通道信息.graphics_queue;
        后端信息.DescriptorPoolSize = 描述符池规模;
        后端信息.MinImageCount = 通道信息.min_image_count;
        后端信息.ImageCount = 通道信息.image_count;
        后端信息.UseDynamicRendering = 通道信息.use_dynamic_rendering;
        后端信息.PipelineInfoMain.PipelineRenderingCreateInfo = 通道信息.pipeline_rendering;
        if (!ImGui_ImplVulkan_Init(&后端信息))
        {
            ImGui_ImplSDL3_Shutdown();
            ImGui::DestroyContext();
            渲染器.shutdown();
            return false;
        }

        //加载中文字体；失败时界面仍可运行（中文显示为方块）
        if (!加载中文字体(字体像素大小))
            std::printf("[测试选择] 警告：未找到中文字体，界面中文可能无法显示。\n");

        //—— 测试运行器：界面模式安装（接入 gtest 输出镜像、释放默认打印器） ——
        Test_Runner 运行器;
        运行器.ui_install();

        //—— 接线：每帧绘制回调 + SDL 事件转发 ——
        Ui_会话上下文 上下文;
        上下文.模型 = &模型;
        上下文.运行器 = &运行器;
        通道->draw_callback_set(ui_绘制回调, &上下文);
        通道->event_callback_set(ui_事件回调, nullptr);

        //—— 主循环：推进渲染层帧链路，界面驱动多轮测试 ——
        while (渲染器.frame_render())
        {
            //窗口被直接关闭或界面请求退出
            if (渲染器.close_requested() || 上下文.退出请求)
                break;
        }

        //—— 清理：先解接线，再停运行器，最后销毁 UI 后端与渲染器 ——
        通道->event_callback_clear();
        通道->draw_callback_clear();
        运行器.shutdown();
        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext();
        渲染器.shutdown();

        return true;
    }
}
