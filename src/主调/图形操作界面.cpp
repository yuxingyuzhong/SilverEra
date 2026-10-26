//图形操作界面实现：渲染层 UI 通道（SDL3 + Vulkan + Dear ImGui）的常驻测试操作界面
#include "src/主调/图形操作界面.h"

//渲染器门面与 UI 通道（含 Vulkan / SDL3 类型；相关包含根由渲染层对外面传递）
#include "Application/Renderer/src/render/Renderer/渲染器.h"
#include "Application/Renderer/src/render/Renderer/渲染器_UI通道.h"

//测试运行器与测试界面
#include "src/主调/测试运行器.h"
#include "src/主调/测试界面.h"
//能力展示场景（测试层入口：单场景集中呈现渲染层能力，相机可漫游）
#include "src/主调/能力展示场景.h"

//Dear ImGui 与两条后端（平台后端用 SDL3、渲染后端用 Vulkan）
#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_vulkan.h"

#include <chrono>
#include <cmath>
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
            //渲染器帧统计（主循环每帧刷新，界面只读）
            Ui_Runtime_Stats 运行状态{};
            //是否处于能力展示模式（本帧界面只画展示叠加层）
            bool 展示模式 = false;
            //界面请求进入 / 退出展示模式（主循环执行资源搭建与释放）
            bool 请求进入展示 = false;
            bool 请求退出展示 = false;
            //展示模式的相机输入（界面采集，主循环消费）
            展示输入 输入{};
			//能力展示场景（非空即表示已搭建）
			能力展示场景* 展示场景 = nullptr;
			//展示模式的原生输入状态（SDL 事件直采；不依赖 ImGui 焦点与面板可见性）
			bool 键前 = false;
			bool 键后 = false;
			bool 键左 = false;
			bool 键右 = false;
			bool 键升 = false;
			bool 键降 = false;
			bool 右键按下 = false;
			//本帧累计的鼠标相对位移与滚轮（消费后归零）
			float 鼠标差x = 0.0f;
			float 鼠标差y = 0.0f;
			float 滚轮差 = 0.0f;
        };

        //展示模式的叠加层（能力清单 ＋ 相机与帧统计 ＋ 操作提示 ＋ 返回按钮）
        void 绘制展示叠加层(Ui_会话上下文& 上下文, const 能力展示场景& 场景)
        {
            //左上角信息面板
            const ImGuiViewport* 视口 = ImGui::GetMainViewport();
            ImGui::SetNextWindowPos(ImVec2(视口->WorkPos.x + 16.0f, 视口->WorkPos.y + 16.0f));
            ImGui::SetNextWindowSize(ImVec2(430.0f, 0.0f));
            //展示模式下没有蓝色渐变背景，本面板必须自带不透明底色，否则整个面板不可见
            ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.04f, 0.07f, 0.14f, 0.92f));
            ImGui::Begin("##能力展示信息", nullptr,
                ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings
                | ImGuiWindowFlags_NoFocusOnAppearing);
            ImGui::TextUnformatted("渲染能力展示 · 可漫游场景");
            ImGui::Separator();
            ImGui::TextUnformatted("本场景同时呈现：");
            ImGui::Bullet(); ImGui::TextUnformatted("十种程序化网格入口（含立方体 / 三角形）＋ 棋盘 / 法线 / 粗糙度纹理");
            ImGui::Bullet(); ImGui::TextUnformatted("方向光 / 补光 / 三盏彩色点光 / 聚光灯 / 半球环境光");
            ImGui::Bullet(); ImGui::TextUnformatted("PBR-lite：金属度 / 粗糙度 / 法线贴图 / 自发光 / 色调");
            ImGui::Bullet(); ImGui::TextUnformatted("方向光阴影 ＋ 软边 ＋ 三级级联（接缝淡化）");
            ImGui::Bullet(); ImGui::TextUnformatted("实例化绘制 ＋ 静态合批 ＋ 视锥剔除");
            ImGui::Bullet(); ImGui::TextUnformatted("立方体天空盒 / 简易 IBL ＋ 距离雾（线性区间 / 指数平方双模式）");
            ImGui::Bullet(); ImGui::TextUnformatted("后处理链：亮部提取 → 模糊 → 暗角 → 3D LUT → 屏幕空间遮蔽 → FXAA 平滑");
            ImGui::Bullet(); ImGui::TextUnformatted("文字渲染（中文标题与区域标注）＋ 标题描边 ＋ 动态纹理区域更新");
            ImGui::Separator();
            float 位置[3] = { 0.0f, 0.0f, 0.0f };
            场景.相机位置(位置);
            ImGui::Text("相机：%.1f, %.1f, %.1f   偏航 %.2f / 俯仰 %.2f", 位置[0], 位置[1], 位置[2],
                场景.偏航(), 场景.俯仰());
            ImGui::Text("帧数 %llu   绘制 %llu   合并 %llu   剔除 %llu   %.2f ms",
                static_cast<unsigned long long>(上下文.运行状态.frames),
                static_cast<unsigned long long>(上下文.运行状态.drawn_items),
                static_cast<unsigned long long>(上下文.运行状态.last_merged_items),
                static_cast<unsigned long long>(上下文.运行状态.last_culled_items),
                上下文.运行状态.frame_ms);
            ImGui::Text("最近一帧交给录制 %llu 项（若为 0 说明绘制项被跳过）",
                static_cast<unsigned long long>(上下文.运行状态.last_draw_items));
            ImGui::Separator();
            ImGui::TextUnformatted("操作：WASD 移动 · E / 空格 上升 · Q / Shift 下降 · 右键拖拽看向 · 滚轮调速 · ESC 返回");
            if (ImGui::Button("返回测试界面", ImVec2(150.0f, 30.0f)))
                上下文.请求退出展示 = true;
            ImGui::SameLine();
            if (ImGui::Button("退出程序", ImVec2(110.0f, 30.0f)))
                上下文.退出请求 = true;
            ImGui::End();
            ImGui::PopStyleColor();
        }

        //测试界面的展示入口按钮（右上角悬浮）
        void 绘制展示入口(Ui_会话上下文& 上下文)
        {
            const ImGuiViewport* 视口 = ImGui::GetMainViewport();
            ImGui::SetNextWindowPos(ImVec2(视口->WorkPos.x + 视口->WorkSize.x - 232.0f, 视口->WorkPos.y + 12.0f));
            ImGui::SetNextWindowSize(ImVec2(220.0f, 0.0f));
            ImGui::Begin("##展示入口", nullptr,
                ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings
                | ImGuiWindowFlags_NoFocusOnAppearing);
            if (ImGui::Button("进入渲染能力展示", ImVec2(-1.0f, 34.0f)))
                上下文.请求进入展示 = true;
            ImGui::End();
        }

        //每帧 UI 绘制回调（渲染层帧链路在场景通道内、呈现转换前调用）
        void ui_绘制回调(VkCommandBuffer 命令缓冲, void* 用户数据)
        {
            Ui_会话上下文& 上下文 = *static_cast<Ui_会话上下文*>(用户数据);

            //本帧 ImGui 帧开始（界面在场景通道内构建并提交绘制）
            //次序按后端文档：平台后端先更新显示尺寸与输入，渲染后端再取帧信息
            ImGui_ImplSDL3_NewFrame();
            ImGui_ImplVulkan_NewFrame();
            ImGui::NewFrame();

            //先铺蓝色渐变，再画操作界面
            if (上下文.展示模式)
            {
                //展示模式：场景由渲染层绘制，本层只画叠加层并采集相机输入
                绘制展示叠加层(上下文, *上下文.展示场景);
                //相机输入取自 SDL 事件直采状态（ImGui 仅负责叠加层绘制）
                展示输入& 输入 = 上下文.输入;
                输入.前后 = (上下文.键前 ? 1.0f : 0.0f) - (上下文.键后 ? 1.0f : 0.0f);
                输入.左右 = (上下文.键右 ? 1.0f : 0.0f) - (上下文.键左 ? 1.0f : 0.0f);
                输入.上下 = (上下文.键升 ? 1.0f : 0.0f) - (上下文.键降 ? 1.0f : 0.0f);
                //右键按住拖拽 → 偏航 / 俯仰增量（消费后清零）
                if (上下文.右键按下)
                {
                    输入.偏航增量 += 上下文.鼠标差x * 0.0025f;
                    输入.俯仰增量 -= 上下文.鼠标差y * 0.0025f;
                }
                上下文.鼠标差x = 0.0f;
                上下文.鼠标差y = 0.0f;
                //滚轮调节速度倍率（0.25..4）
                if (上下文.滚轮差 != 0.0f)
                {
                    输入.速度倍率 *= std::pow(1.15f, 上下文.滚轮差);
                    上下文.滚轮差 = 0.0f;
                }
                if (输入.速度倍率 < 0.25f)
                    输入.速度倍率 = 0.25f;
                if (输入.速度倍率 > 4.0f)
                    输入.速度倍率 = 4.0f;
            }
            else
            {
                绘制蓝色渐变背景();

                //铺满整个窗口的无标题面板
                const ImGuiViewport* 视口 = ImGui::GetMainViewport();
                ImGui::SetNextWindowPos(视口->WorkPos);
                ImGui::SetNextWindowSize(视口->WorkSize);
                ImGui::Begin("##测试操作界面", nullptr,
                    ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);

                //绘制测试操作界面（树 / 控制 / 结果 / 输出镜像 / 轮次 / 日志）
                //注意：必须在面板窗口内绘制，控件才有有效窗口可放置与命中
                测试界面_绘制(*上下文.运行器, *上下文.模型, 上下文.退出请求, &上下文.运行状态);

                ImGui::End();

                //右上角悬浮「进入渲染能力展示」入口
                绘制展示入口(上下文);
            }

            //提交绘制：交给渲染层帧链路命令缓冲（后端画在已开启的动态渲染通道内）
            ImGui::Render();
            ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), 命令缓冲);
        }

        //SDL 事件转发回调（渲染层事件泵逐个事件转发）
        //职责：① 交给 ImGui 平台后端（叠加层输入）；② 直采展示模式的键鼠状态
        void ui_事件回调(const SDL_Event* 事件, void* 用户数据)
        {
            Ui_会话上下文& 上下文 = *static_cast<Ui_会话上下文*>(用户数据);
            ImGui_ImplSDL3_ProcessEvent(事件);
            //按键按下 / 抬起（按物理键位，避免输入法 / 布局差异）
            if (事件->type == SDL_EVENT_KEY_DOWN || 事件->type == SDL_EVENT_KEY_UP)
            {
                const bool 按下 = 事件->type == SDL_EVENT_KEY_DOWN;
                switch (事件->key.scancode)
                {
                case SDL_SCANCODE_W: 上下文.键前 = 按下; break;
                case SDL_SCANCODE_S: 上下文.键后 = 按下; break;
                case SDL_SCANCODE_A: 上下文.键左 = 按下; break;
                case SDL_SCANCODE_D: 上下文.键右 = 按下; break;
                case SDL_SCANCODE_Q: 上下文.键降 = 按下; break;
                case SDL_SCANCODE_E: 上下文.键升 = 按下; break;
                //备用升降键：空格上升、Shift 下降（与 Q/E 并存）
                case SDL_SCANCODE_SPACE: 上下文.键升 = 按下; break;
                case SDL_SCANCODE_LSHIFT:
                case SDL_SCANCODE_RSHIFT: 上下文.键降 = 按下; break;
                case SDL_SCANCODE_ESCAPE:
                    //ESC：展示模式下返回测试界面
                    if (按下 && 上下文.展示模式)
                        上下文.请求退出展示 = true;
                    break;
                default:
                    break;
                }
                return;
            }
            //右键按下 / 抬起（拖拽看向用）
            if (事件->type == SDL_EVENT_MOUSE_BUTTON_DOWN || 事件->type == SDL_EVENT_MOUSE_BUTTON_UP)
            {
                if (事件->button.button == SDL_BUTTON_RIGHT)
                    上下文.右键按下 = 事件->type == SDL_EVENT_MOUSE_BUTTON_DOWN;
                return;
            }
            //鼠标相对位移（仅右键按住时累计）
            if (事件->type == SDL_EVENT_MOUSE_MOTION)
            {
                if (上下文.右键按下)
                {
                    上下文.鼠标差x += 事件->motion.xrel;
                    上下文.鼠标差y += 事件->motion.yrel;
                }
                return;
            }
            //滚轮
            if (事件->type == SDL_EVENT_MOUSE_WHEEL)
                上下文.滚轮差 += 事件->wheel.y;
        }
    }

    //========================================================================
    // 图形操作界面
    //========================================================================
    bool 图形操作界面_运行(Test_Selection_Model& 模型, bool 直接进入展示)
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
        通道->event_callback_set(ui_事件回调, &上下文);

        //—— 主循环：测试界面推进渲染层帧链路；展示模式改为推进可漫游场景 ——
        //测试界面清屏色取渐变背景的深蓝基色，使窗口未覆盖处与界面背景衔接
        auto 恢复测试界面场景 = [&渲染器]()
        {
            Scene_Data 场景{};
            场景.clear_color[0] = 0.035f;
            场景.clear_color[1] = 0.050f;
            场景.clear_color[2] = 0.105f;
            场景.clear_color[3] = 1.0f;
            渲染器.scene_apply(场景);
        };
        恢复测试界面场景();

        //能力展示场景（进入时搭建、返回时释放）
        能力展示场景 展示;
        bool 展示已搭建 = false;
        //命令行直达展示模式
        if (直接进入展示)
            上下文.请求进入展示 = true;

        std::chrono::steady_clock::time_point 上帧时刻 = std::chrono::steady_clock::now();
        while (true)
        {
            //处理「进入展示」请求
            if (上下文.请求进入展示 && !上下文.展示模式)
            {
                上下文.请求进入展示 = false;
                if (展示.init(渲染器))
                {
                    展示已搭建 = true;
                    上下文.展示场景 = &展示;
                    上下文.展示模式 = true;
                }
            }
            //处理「退出展示」请求（返回测试界面）
            if (上下文.请求退出展示)
            {
                上下文.请求退出展示 = false;
                if (展示已搭建)
                {
                    展示.shutdown(渲染器);
                    展示已搭建 = false;
                }
                上下文.展示场景 = nullptr;
                上下文.展示模式 = false;
                恢复测试界面场景();
            }

            //帧间隔（限制上限，避免卡顿后瞬移）
            const std::chrono::steady_clock::time_point 现时刻 = std::chrono::steady_clock::now();
            float 秒 = std::chrono::duration<float>(现时刻 - 上帧时刻).count();
            上帧时刻 = 现时刻;
            if (秒 > 0.1f)
                秒 = 0.1f;

            //展示模式：推进相机并登记本帧绘制项
            if (上下文.展示模式 && 展示已搭建)
            {
                展示.update(渲染器, 上下文.输入, 秒);
                展示.frame_draw(渲染器);
                //本帧鼠标增量已消费
                上下文.输入.偏航增量 = 0.0f;
                上下文.输入.俯仰增量 = 0.0f;
            }

            //取上一帧统计供本帧界面显示（界面回调在 frame_render 内部执行）
            const Frame_Stats 统计 = 渲染器.frame_stats_get();
            上下文.运行状态.frame_ms = 统计.frame_ms;
            上下文.运行状态.gpu_ms = 统计.gpu_frame_ms;
            上下文.运行状态.frames = 统计.frames;
            上下文.运行状态.draw_items = 统计.draw_items;
            上下文.运行状态.drawn_items = 统计.drawn_items;
            上下文.运行状态.last_draw_items = 统计.last_draw_items;
            上下文.运行状态.last_culled_items = 统计.last_culled_items;
            上下文.运行状态.last_merged_items = 统计.last_merged_items;
            上下文.运行状态.instances = 统计.instances;
            上下文.运行状态.post_stages = 统计.post_stages;
            上下文.运行状态.gpu_supported = 渲染器.gpu_timestamp_supported();

            //推进一帧
            if (!渲染器.frame_render())
                break;
            //窗口被直接关闭或界面请求退出
            if (渲染器.close_requested() || 上下文.退出请求)
                break;
        }

        //退出前释放展示资源
        if (展示已搭建)
        {
            展示.shutdown(渲染器);
            展示已搭建 = false;
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
