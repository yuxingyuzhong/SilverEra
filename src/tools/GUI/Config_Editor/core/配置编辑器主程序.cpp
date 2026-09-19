//============================================================================
// 配置编辑器主程序 —— main() 入口
// ---------------------------------------------------------------------------
// 职责：
//   1. 初始化 GLFW 窗口 + OpenGL 上下文
//   2. 初始化 Dear ImGui（GLFW + OpenGL3 后端）
//   3. 运行主循环并驱动配置编辑器界面
// 外观函数（中文字体/粉色主题/梦幻背景）已拆分到 配置编辑器主程序_外观.cpp（架构改革 阶段 4）
//============================================================================

#include "common/前置头文件包含.h"
#include "src/tools/GUI/Config_Editor/配置编辑器.h"
#include "src/tools/GUI/Config_Editor/配置编辑器主程序_外观.h"

#include <random>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

//引擎命名空间
using namespace engine;

//=============================================================================
// GLFW 窗口关闭回调（关闭确认接线）
// 由 main 所在编译单元持有：窗口 × 按钮先交给编辑器（有未保存修改则弹确认窗）
//=============================================================================
namespace
{
    // GLFW 窗口关闭回调（关闭确认接线）
    // -----------------------------------------------------------------------
    // 用户点窗口 × 时并不直接关闭：先通知编辑器（有未保存修改则弹确认窗），
    // 并拦截默认关闭；编辑器确认退出后由主循环真正关闭窗口。
    //========================================================================
    void 窗口关闭回调(GLFWwindow* window)
    {
        配置编辑器* 编辑器 = static_cast<配置编辑器*>(glfwGetWindowUserPointer(window));
        if (编辑器 != nullptr)
            编辑器->请求关闭窗口();
        //先拦截默认关闭行为，由编辑器决定何时真正退出
        glfwSetWindowShouldClose(window, GLFW_FALSE);
    }
}

//============================================================================
// 主函数
//============================================================================
int main(void)
{
    //控制台输出切换 UTF-8（调试信息）
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    // —— GLFW 初始化 ——
    if (!glfwInit())
        return -1;

    //请求 OpenGL 3.3 核心上下文
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // —— 窗口尺寸自适应 ——
    // 以主显示器工作区为基准：默认开 92% 大小并最大化，界面比例更大
    const GLFWvidmode* 视频模式 = glfwGetVideoMode(glfwGetPrimaryMonitor());
    int 屏幕宽度 = 视频模式 ? 视频模式->width : 1920;
    int 屏幕高度 = 视频模式 ? 视频模式->height : 1080;
    int 窗口宽度 = (int)(屏幕宽度 * 0.92f);
    int 窗口高度 = (int)(屏幕高度 * 0.92f);

    //创建窗口
    GLFWwindow* window = glfwCreateWindow(窗口宽度, 窗口高度, "配置编辑器", nullptr, nullptr);
    if (window == nullptr)
    {
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);   //垂直同步
    glfwMaximizeWindow(window);   //默认最大化，充分利用屏幕空间

    // —— GLAD 加载 OpenGL 函数 ——
    if (!gladLoadGL(glfwGetProcAddress))
    {
        glfwTerminate();
        return -1;
    }

    // —— Dear ImGui 初始化 ——
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    //梦幻粉色主题（在深色基础上覆盖粉紫配色 + 柔和圆角）
    应用粉色主题();

    //绑定 GLFW + OpenGL3 后端
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 130");

    //加载中文字体（按屏幕宽度自适应：大屏用更大字号，整体界面更饱满）
    float 字体大小 = (屏幕宽度 >= 1920) ? 23.0f : 20.0f;
    ImFont* 中文字体 = 加载中文字体(字体大小);
    if (中文字体 == nullptr)
    {
        //无中文字体时提示（控制台），界面仍可运行（中文显示为方块）
        std::printf("[ConfigEditor] 警告：未找到中文字体，界面中文可能无法显示。\n");
    }

    // —— 配置编辑器 ——
    配置编辑器 编辑器;
    编辑器.加载();

    //关闭确认接线：窗口 × 按钮先交给编辑器（有未保存修改时弹确认窗）
    glfwSetWindowUserPointer(window, &编辑器);
    glfwSetWindowCloseCallback(window, 窗口关闭回调);

    // —— 主循环 ——
    while (!glfwWindowShouldClose(window))
    {
        //处理窗口事件
        glfwPollEvents();

        //开始 ImGui 新帧
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        //绘制梦幻背景（粉紫渐变 + 闪烁星光）
        渲染梦幻背景();

        //渲染编辑器界面
        编辑器.渲染();

        //编辑器确认退出后，真正关闭窗口
        if (编辑器.应关闭窗口())
            glfwSetWindowShouldClose(window, GLFW_TRUE);

        //提交渲染
        ImGui::Render();

        //清屏并绘制（亮紫粉，与渐变顶部呼应，避免窗口边缘露黑）
        int 显示宽度 = 0, 显示高度 = 0;
        glfwGetFramebufferSize(window, &显示宽度, &显示高度);
        glViewport(0, 0, 显示宽度, 显示高度);
        glClearColor(0.73f, 0.38f, 0.82f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        //交换缓冲
        glfwSwapBuffers(window);
    }

    // —— 清理 ——
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}

