//图形选择窗口实现：GLFW + OpenGL3 + Dear ImGui 的临时勾选界面
#include "src/主调/图形选择窗口.h"

//GL 函数加载器必须先于 GLFW 头文件
#include <glad/gl.h>
//禁止 GLFW 自带 GL 头，避免与 glad 冲突
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

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
        const int 窗口宽度 = 1120;
        const int 窗口高度 = 760;

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

        //应用粉色主题（明丽版，与系统层配置编辑器同一色系）
        void 应用粉色主题()
        {
            ImGuiStyle& 样式 = ImGui::GetStyle();
            ImVec4* 颜色 = 样式.Colors;

            颜色[ImGuiCol_WindowBg]       = ImVec4(0.36f, 0.20f, 0.40f, 1.00f);
            颜色[ImGuiCol_ChildBg]        = ImVec4(0.44f, 0.26f, 0.48f, 0.55f);
            颜色[ImGuiCol_PopupBg]        = ImVec4(0.55f, 0.32f, 0.58f, 0.98f);
            颜色[ImGuiCol_Border]         = ImVec4(1.00f, 0.75f, 0.95f, 0.55f);
            颜色[ImGuiCol_Text]           = ImVec4(1.00f, 0.96f, 1.00f, 1.00f);
            颜色[ImGuiCol_TextDisabled]   = ImVec4(0.88f, 0.74f, 0.90f, 1.00f);
            颜色[ImGuiCol_FrameBg]        = ImVec4(0.55f, 0.33f, 0.58f, 1.00f);
            颜色[ImGuiCol_FrameBgHovered] = ImVec4(0.65f, 0.42f, 0.68f, 1.00f);
            颜色[ImGuiCol_FrameBgActive]  = ImVec4(0.72f, 0.48f, 0.74f, 1.00f);
            颜色[ImGuiCol_Button]         = ImVec4(0.85f, 0.45f, 0.78f, 1.00f);
            颜色[ImGuiCol_ButtonHovered]  = ImVec4(0.98f, 0.62f, 0.90f, 1.00f);
            颜色[ImGuiCol_ButtonActive]   = ImVec4(0.70f, 0.34f, 0.66f, 1.00f);
            颜色[ImGuiCol_Header]         = ImVec4(0.80f, 0.45f, 0.74f, 0.95f);
            颜色[ImGuiCol_HeaderHovered]  = ImVec4(0.90f, 0.55f, 0.84f, 1.00f);
            颜色[ImGuiCol_HeaderActive]   = ImVec4(0.74f, 0.40f, 0.70f, 1.00f);
            颜色[ImGuiCol_CheckMark]      = ImVec4(1.00f, 0.72f, 0.94f, 1.00f);
            颜色[ImGuiCol_SliderGrab]     = ImVec4(0.96f, 0.58f, 0.88f, 1.00f);
            颜色[ImGuiCol_ScrollbarBg]    = ImVec4(0.36f, 0.20f, 0.40f, 0.80f);
            颜色[ImGuiCol_ScrollbarGrab]  = ImVec4(0.86f, 0.46f, 0.80f, 1.00f);
            颜色[ImGuiCol_TitleBg]        = ImVec4(0.72f, 0.38f, 0.68f, 1.00f);
            颜色[ImGuiCol_TitleBgActive]  = ImVec4(0.85f, 0.48f, 0.80f, 1.00f);

            //柔和圆角与内边距
            样式.WindowRounding = 8.0f;
            样式.FrameRounding  = 5.0f;
            样式.GrabRounding   = 5.0f;
            样式.WindowPadding  = ImVec2(12.0f, 12.0f);
            样式.FramePadding   = ImVec2(8.0f, 5.0f);
            样式.ItemSpacing    = ImVec2(8.0f, 6.0f);
        }

        //关键词匹配（空关键词视为命中）
        bool 含关键词(const std::string& 文本, const std::string& 关键词)
        {
            if (关键词.empty())
                return true;
            return 文本.find(关键词) != std::string::npos;
        }
    }

    //========================================================================
    // 图形选择窗口
    //========================================================================
    bool 图形选择窗口_运行(测试选择模型& 模型, std::string& 过滤串)
    {
        //—— GLFW 初始化 ——
        if (!glfwInit())
            return false;

        //请求 OpenGL 3.3 核心上下文（与系统层配置编辑器一致）
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

        //创建窗口，失败即交回主调退到控制台菜单
        GLFWwindow* 窗口 = glfwCreateWindow(窗口宽度, 窗口高度, "白银纪元 · 测试模块选择", nullptr, nullptr);
        if (窗口 == nullptr)
        {
            glfwTerminate();
            return false;
        }
        glfwMakeContextCurrent(窗口);
        glfwSwapInterval(1);   //垂直同步

        //—— 加载 OpenGL 函数 ——
        if (!gladLoadGL(glfwGetProcAddress))
        {
            glfwDestroyWindow(窗口);
            glfwTerminate();
            return false;
        }

        //—— Dear ImGui 初始化 ——
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        //不写 imgui.ini，保持工作区整洁
        io.IniFilename = nullptr;

        应用粉色主题();
        ImGui_ImplGlfw_InitForOpenGL(窗口, true);
        ImGui_ImplOpenGL3_Init("#version 130");

        //加载中文字体；失败时界面仍可运行（中文显示为方块）
        if (!加载中文字体(19.0f))
            std::printf("[测试选择] 警告：未找到中文字体，界面中文可能无法显示。\n");

        //搜索关键词缓冲
        char 搜索缓冲[128] = "";

        //—— 主循环 ——
        bool 已确认 = false;
        while (!glfwWindowShouldClose(窗口))
        {
            glfwPollEvents();

            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();

            //铺满整个窗口的无标题面板
            const ImGuiViewport* 视口 = ImGui::GetMainViewport();
            ImGui::SetNextWindowPos(视口->WorkPos);
            ImGui::SetNextWindowSize(视口->WorkSize);
            ImGui::Begin("##测试选择", nullptr,
                ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);

            //标题与操作提示
            ImGui::TextUnformatted("白银纪元 · 测试模块选择");
            ImGui::SameLine();
            ImGui::TextDisabled("（勾选要运行的模块或用例；Esc = 全跑）");
            ImGui::Separator();

            //—— 工具条：批量操作 + 搜索 + 计数 ——
            if (ImGui::Button("全选"))
                模型.全部勾选();
            ImGui::SameLine();
            if (ImGui::Button("清空"))
                模型.全部清空();
            ImGui::SameLine();
            if (ImGui::Button("反选"))
                模型.反向勾选();
            ImGui::SameLine();
            ImGui::SetNextItemWidth(260.0f);
            ImGui::InputTextWithHint("##搜索", "搜索模块或用例名…", 搜索缓冲, sizeof(搜索缓冲));
            ImGui::SameLine();
            ImGui::Text("已选 %zu / %zu", 模型.已勾选用例数(), 模型.用例总数());
            ImGui::Separator();

            const std::string 关键词 = 搜索缓冲;
            const std::vector<套件条目>& 套件表 = 模型.套件表();

            //—— 选择树 ——
            ImGui::BeginChild("##选择树", ImVec2(0.0f, -44.0f), ImGuiChildFlags_Borders);
            for (std::size_t 套件序号 = 0; 套件序号 < 套件表.size(); ++套件序号)
            {
                const 套件条目& 套件 = 套件表[套件序号];

                //判断套件名是否命中关键词
                const bool 套件命中 = 含关键词(套件.名称, 关键词);
                //判断是否有用例命中关键词
                bool 有用例命中 = false;
                for (const 用例条目& 用例 : 套件.用例表)
                {
                    if (含关键词(用例.名称, 关键词))
                    {
                        有用例命中 = true;
                        break;
                    }
                }
                //套件名与用例名都未命中：整组隐藏
                if (!套件命中 && !有用例命中)
                    continue;

                ImGui::PushID(static_cast<int>(套件序号));

                //套件级复选框：勾选整组
                bool 套件勾选 = 模型.套件全选(套件序号);
                if (ImGui::Checkbox("##套件勾选", &套件勾选))
                    模型.设置套件勾选(套件序号, 套件勾选);
                ImGui::SameLine();

                //统计本套件已勾选数，与总数一起显示
                std::size_t 套件已选 = 0;
                for (const 用例条目& 用例 : 套件.用例表)
                {
                    if (用例.已勾选)
                        ++套件已选;
                }
                const std::string 节点标签 = 套件.名称 + "（" + std::to_string(套件已选)
                    + "/" + std::to_string(套件.用例表.size()) + "）";

                //展开态由模型持有，避免每帧被复位
                ImGui::SetNextItemOpen(模型.套件已展开(套件序号), ImGuiCond_Always);
                const bool 展开 = ImGui::TreeNodeEx("##节点",
                    ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth,
                    "%s", 节点标签.c_str());
                if (展开 != 模型.套件已展开(套件序号))
                    模型.设置套件展开(套件序号, 展开);

                //展开时逐个用例渲染勾选框
                if (展开)
                {
                    for (std::size_t 用例序号 = 0; 用例序号 < 套件.用例表.size(); ++用例序号)
                    {
                        const 用例条目& 用例 = 套件.用例表[用例序号];
                        //套件名未命中且该用例也未命中时隐藏
                        if (!套件命中 && !含关键词(用例.名称, 关键词))
                            continue;

                        ImGui::PushID(static_cast<int>(用例序号));
                        bool 用例勾选 = 用例.已勾选;
                        if (ImGui::Checkbox(用例.名称.c_str(), &用例勾选))
                            模型.设置用例勾选(套件序号, 用例序号, 用例勾选);
                        ImGui::PopID();
                    }
                    ImGui::TreePop();
                }

                ImGui::PopID();
            }
            ImGui::EndChild();

            //—— 底部按钮：确认 / 取消 ——
            if (ImGui::Button("开始测试", ImVec2(160.0f, 32.0f)) || ImGui::IsKeyPressed(ImGuiKey_Enter))
            {
                过滤串 = 模型.生成过滤串();
                已确认 = true;
            }
            ImGui::SameLine();
            if (ImGui::Button("取消（全跑）", ImVec2(160.0f, 32.0f)) || ImGui::IsKeyPressed(ImGuiKey_Escape))
            {
                过滤串 = "*";
                已确认 = true;
            }
            ImGui::SameLine();
            ImGui::TextDisabled("复跑：EngineTests.exe --gtest_filter=<过滤串>");

            ImGui::End();

            //提交渲染
            ImGui::Render();
            int 显示宽度 = 0;
            int 显示高度 = 0;
            glfwGetFramebufferSize(窗口, &显示宽度, &显示高度);
            glViewport(0, 0, 显示宽度, 显示高度);
            glClearColor(0.30f, 0.16f, 0.34f, 1.00f);
            glClear(GL_COLOR_BUFFER_BIT);
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
            glfwSwapBuffers(窗口);

            //已确认选择：退出循环
            if (已确认)
                break;
        }

        //—— 清理：销毁窗口与 ImGui 上下文 ——
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        glfwDestroyWindow(窗口);
        glfwTerminate();

        //窗口被直接关闭（未点按钮）视为全跑
        if (!已确认)
            过滤串 = "*";
        return true;
    }
}
