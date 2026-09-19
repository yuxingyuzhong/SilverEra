//============================================================================
// 配置编辑器主程序 —— 外观（中文字体 / 粉色主题 / 梦幻背景）
// 由 配置编辑器主程序.cpp 拆分而来（架构改革 阶段 4），行为与拆分前完全一致
//============================================================================
#include "src/tools/GUI/Config_Editor/配置编辑器主程序_外观.h"

#include <random>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

//引擎命名空间
namespace engine
{
    // 中文字体加载
    //============================================================================
    //收集候选字体路径（按优先级，跨平台回退）
    //Windows：用 %WINDIR% 定位系统字体目录（兼容系统盘不在 C 的情况），扩充常见中文字体候选；
    //非 Windows：补充 Linux / macOS 常见中文字体路径，避免界面中文完全无法显示
    std::vector<std::string> 收集候选字体路径()
    {
        std::vector<std::string> 候选;
#ifdef _WIN32
        std::string fonts_dir;
        if (const char* windir = std::getenv("WINDIR"))
            fonts_dir = std::string(windir) + "/Fonts/";
        else
            fonts_dir = "C:/Windows/Fonts/";

        候选.push_back(fonts_dir + "msyh.ttc");      //微软雅黑
        候选.push_back(fonts_dir + "msyhbd.ttc");    //微软雅黑 Bold
        候选.push_back(fonts_dir + "msyhl.ttc");     //微软雅黑 Light
        候选.push_back(fonts_dir + "Deng.ttf");      //等线
        候选.push_back(fonts_dir + "Dengb.ttf");     //等线 Bold
        候选.push_back(fonts_dir + "simhei.ttf");    //黑体
        候选.push_back(fonts_dir + "simsun.ttc");    //宋体
        候选.push_back(fonts_dir + "simkai.ttf");    //楷体
        候选.push_back(fonts_dir + "simfang.ttf");   //仿宋
        候选.push_back(fonts_dir + "msjh.ttc");      //微软正黑（繁体系统）
#else
        候选.push_back("/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc");
        候选.push_back("/usr/share/fonts/opentype/noto/NotoSansCJKsc-Regular.otf");
        候选.push_back("/usr/share/fonts/truetype/wqy/wqy-microhei.ttc");
        候选.push_back("/usr/share/fonts/truetype/wqy/wqy-zenhei.ttc");
        候选.push_back("/usr/share/fonts/truetype/arphic/uming.ttc");
        候选.push_back("/System/Library/Fonts/PingFang.ttc");
        候选.push_back("/System/Library/Fonts/STHeiti Light.ttc");
        候选.push_back("/Library/Fonts/Arial Unicode.ttf");
#endif
        return 候选;
    }

    //尝试加载中文字体（按优先级尝试常见系统字体路径）
    //返回成功加载的字体（失败返回 nullptr）
    ImFont* 加载中文字体(float 像素大小)
    {
        ImGuiIO& io = ImGui::GetIO();

        //用 ImFontGlyphRangesBuilder 合并「中文全范围 + 界面用到的特殊符号」：
        //帮助窗口里的 ♪（音符）、ω（希腊小写）、笑脸字符画、校验结果的 ✓/✗ 等
        //都不在 GetGlyphRangesChineseFull() 范围内，不加进去就会渲染成 '?'
        ImFontGlyphRangesBuilder 字形构建器;
        字形构建器.AddRanges(io.Fonts->GetGlyphRangesChineseFull());
        字形构建器.AddChar(0x266A);   //♪ 音符（帮助正文 / 笑脸字符画）
        字形构建器.AddChar(0x03C9);   //ω 希腊小写（笑脸字符画「＾ω＾」）
        字形构建器.AddChar(0x2713);   //✓ 校验通过
        字形构建器.AddChar(0x2717);   //✗ 校验失败
        字形构建器.AddChar(0x2022);   //• 项目符号（帮助正文）
        字形构建器.AddChar(0x2026);   //… 省略号
        字形构建器.AddChar(0x221A);   //√ 根号（兜底符号）
        字形构建器.AddChar(0x00D7);   //× 乘号（兜底符号）
        ImVector<ImWchar> 字形范围;
        字形构建器.BuildRanges(&字形范围);

        //候选字体（按优先级收集：微软雅黑系列 → 等线 → 黑体 → 宋体 → 楷体 → 仿宋 → 繁体正黑）
        //微软雅黑符号覆盖最全（♪ ω ✓ ✗ 都有字形），放第一位保证特殊符号可显示
        const std::vector<std::string> 候选字体 = 收集候选字体路径();
        for (const std::string& 路径 : 候选字体)
        {
            //先确认文件存在，避免 ImGui 逐个打印加载失败日志
            std::error_code ec;
            if (!std::filesystem::is_regular_file(std::filesystem::path(路径), ec))
                continue;
            ImFont* font = io.Fonts->AddFontFromFileTTF(路径.c_str(), 像素大小, nullptr, 字形范围.Data);
            if (font != nullptr)
                return font;
        }

        //全部失败：回退默认字体（无法显示中文）
        return nullptr;
    }

    //========================================================================
    // 梦幻粉色主题（明丽版）
    // -----------------------------------------------------------------------
    // 明丽 + 渐变 + 星光三连：
    //   1. 背景改为粉紫渐变（上紫粉 → 下亮粉），由渲染梦幻背景() 绘制，
    //      因此 WindowBg 置为全透明，让渐变与星光透出来
    //   2. 控件统一提亮：标题栏 / 按钮 / 选中高亮 / 输入框全部更鲜艳明丽
    //   3. 圆角保持柔和，控件观感更可爱
    //========================================================================
    void 应用粉色主题()
    {
        ImGuiStyle& style = ImGui::GetStyle();
        ImVec4* colors = style.Colors;

        // —— 基础背景：全透明（渐变 + 星光由渲染梦幻背景() 绘制）——
        colors[ImGuiCol_WindowBg]            = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        // Child 面板：亮粉紫 + 更通透的半透明 —— 让背景粉紫渐变与闪烁星光透出来，
        // 面板不再是一片纯色，整体更有「渐变粉」的层次（太不透明会遮住星光）
        colors[ImGuiCol_ChildBg]             = ImVec4(0.72f, 0.48f, 0.80f, 0.32f);
        colors[ImGuiCol_PopupBg]             = ImVec4(0.55f, 0.32f, 0.58f, 0.98f);
        colors[ImGuiCol_MenuBarBg]           = ImVec4(0.62f, 0.36f, 0.64f, 0.95f);
        colors[ImGuiCol_Border]              = ImVec4(1.00f, 0.75f, 0.95f, 0.55f);
        colors[ImGuiCol_BorderShadow]        = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);

        // —— 标题栏（亮玫瑰粉）——
        colors[ImGuiCol_TitleBg]             = ImVec4(0.72f, 0.38f, 0.68f, 1.00f);
        colors[ImGuiCol_TitleBgActive]       = ImVec4(0.85f, 0.48f, 0.80f, 1.00f);
        colors[ImGuiCol_TitleBgCollapsed]    = ImVec4(0.62f, 0.32f, 0.58f, 0.90f);

        // —— 文本（亮奶白粉）——
        colors[ImGuiCol_Text]                = ImVec4(1.00f, 0.96f, 1.00f, 1.00f);
        colors[ImGuiCol_TextDisabled]        = ImVec4(0.88f, 0.74f, 0.90f, 1.00f);

        // —— 输入框背景（提亮的粉紫）——
        colors[ImGuiCol_FrameBg]             = ImVec4(0.55f, 0.33f, 0.58f, 1.00f);
        colors[ImGuiCol_FrameBgHovered]      = ImVec4(0.65f, 0.42f, 0.68f, 1.00f);
        colors[ImGuiCol_FrameBgActive]       = ImVec4(0.72f, 0.48f, 0.74f, 1.00f);

        // —— 按钮（樱花粉 → 亮粉；hover 提亮、active 按下加深）——
        colors[ImGuiCol_Button]              = ImVec4(0.85f, 0.45f, 0.78f, 1.00f);
        colors[ImGuiCol_ButtonHovered]       = ImVec4(0.98f, 0.62f, 0.90f, 1.00f);
        colors[ImGuiCol_ButtonActive]        = ImVec4(0.70f, 0.34f, 0.66f, 1.00f);

        // —— 选中 / 悬停高亮（亮蔷薇粉）——
        colors[ImGuiCol_Header]              = ImVec4(0.80f, 0.45f, 0.74f, 0.95f);
        colors[ImGuiCol_HeaderHovered]       = ImVec4(0.90f, 0.55f, 0.84f, 1.00f);
        colors[ImGuiCol_HeaderActive]        = ImVec4(0.70f, 0.38f, 0.66f, 1.00f);

        // —— 分隔线 / 滚动条（亮粉紫）——
        colors[ImGuiCol_Separator]           = ImVec4(1.00f, 0.68f, 0.92f, 0.60f);
        colors[ImGuiCol_SeparatorHovered]    = ImVec4(1.00f, 0.78f, 0.95f, 0.80f);
        colors[ImGuiCol_SeparatorActive]     = ImVec4(1.00f, 0.85f, 0.98f, 1.00f);
        colors[ImGuiCol_ScrollbarBg]         = ImVec4(0.36f, 0.20f, 0.40f, 1.00f);
        colors[ImGuiCol_ScrollbarGrab]       = ImVec4(0.85f, 0.55f, 0.82f, 1.00f);
        colors[ImGuiCol_ScrollbarGrabHovered]= ImVec4(0.95f, 0.65f, 0.90f, 1.00f);
        colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(1.00f, 0.78f, 0.95f, 1.00f);

        // —— 输入框光标 / 选中文本（亮粉）——
        colors[ImGuiCol_TextSelectedBg]      = ImVec4(1.00f, 0.65f, 0.92f, 0.55f);
        colors[ImGuiCol_CheckMark]           = ImVec4(1.00f, 0.85f, 0.97f, 1.00f);

        // —— 选项卡（统一风格，提亮）——
        colors[ImGuiCol_Tab]                 = ImVec4(0.68f, 0.40f, 0.66f, 1.00f);
        colors[ImGuiCol_TabHovered]          = ImVec4(0.88f, 0.52f, 0.82f, 1.00f);
        colors[ImGuiCol_TabActive]           = ImVec4(0.82f, 0.46f, 0.76f, 1.00f);
        colors[ImGuiCol_TabUnfocused]        = ImVec4(0.55f, 0.32f, 0.54f, 1.00f);
        colors[ImGuiCol_TabUnfocusedActive]  = ImVec4(0.66f, 0.40f, 0.64f, 1.00f);

        // —— 圆角与内边距（统一柔和圆角；加大 FramePadding 让输入框更高更醒目）——
        style.WindowRounding    = 12.0f;
        style.ChildRounding     = 10.0f;
        style.FrameRounding     = 8.0f;
        style.PopupRounding     = 10.0f;
        style.ScrollbarRounding = 12.0f;
        style.GrabRounding      = 10.0f;
        style.TabRounding       = 8.0f;
        style.WindowBorderSize  = 1.0f;
        style.FrameBorderSize   = 0.5f;
        style.FramePadding      = ImVec2(10.0f, 6.0f);
        style.WindowPadding     = ImVec2(12.0f, 12.0f);
        style.ItemSpacing       = ImVec2(10.0f, 8.0f);
    }

    //========================================================================
    // 梦幻背景：粉紫渐变 + 闪烁星光
    // -----------------------------------------------------------------------
    // 在 ImGui 背景层绘制：
    //   1. 全屏垂直渐变（上紫粉 → 下亮粉），明丽梦幻
    //   2. 固定种子的星空（每次启动位置一致），每颗星星按正弦波明灭，
    //      大星星带光晕和十字光芒，营造「时隐时现」的闪烁感
    //========================================================================
    void 渲染梦幻背景()
    {
        ImDrawList* 画布 = ImGui::GetBackgroundDrawList();
        const ImVec2 视口 = ImGui::GetIO().DisplaySize;
        if (视口.x <= 0.0f || 视口.y <= 0.0f)
            return;

        // —— 粉紫渐变（顶部浅粉 → 底部深粉紫，层次更明显）——
        画布->AddRectFilledMultiColor(
            ImVec2(0, 0), 视口,
            IM_COL32(255, 196, 226, 255),  //左上：顶部浅粉
            IM_COL32(255, 196, 226, 255),  //右上：顶部浅粉
            IM_COL32(148, 66, 168, 255),   //右下：底部深粉紫
            IM_COL32(148, 66, 168, 255));  //左下：底部深粉紫

        // —— 闪烁星光 ——
        struct 星光
        {
            float x, y;        //相对位置（0~1）
            float 半径;        //星核半径
            float 速度;        //闪烁速度
            float 相位;        //闪烁相位（错开明灭节奏）
        };
        static 星光 群星[80];
        static bool 已播种 = false;
        if (!已播种)
        {
            //固定种子：每次启动星空分布一致，便于反复调试观感
            std::mt19937 随机(20260814);
            std::uniform_real_distribution<float> 横(0.01f, 0.99f);
            std::uniform_real_distribution<float> 纵(0.05f, 0.95f);
            std::uniform_real_distribution<float> 径(1.2f, 4.0f);
            std::uniform_real_distribution<float> 速(0.6f, 2.4f);
            std::uniform_real_distribution<float> 相(0.0f, 6.2832f);
            for (auto& 星 : 群星)
            {
                星.x = 横(随机);
                星.y = 纵(随机);
                星.半径 = 径(随机);
                星.速度 = 速(随机);
                星.相位 = 相(随机);
            }
            已播种 = true;
        }

        const float 时刻 = (float)ImGui::GetTime();
        for (const auto& 星 : 群星)
        {
            //亮度：正弦起伏后平方，大部分时间偏暗、偶尔亮起 = 时隐时现
            float 亮度 = 0.5f + 0.5f * std::sinf(时刻 * 星.速度 + 星.相位);
            float alpha = 0.18f + 0.82f * 亮度 * 亮度;
            ImVec2 位置(星.x * 视口.x, 星.y * 视口.y);

            //大星星：柔和光晕（更明显）
            if (星.半径 > 2.2f)
            {
                画布->AddCircleFilled(位置, 星.半径 * 3.6f,
                    IM_COL32(255, 220, 245, (int)(alpha * 60)));
            }
            //星核
            画布->AddCircleFilled(位置, 星.半径,
                IM_COL32(255, 255, 255, (int)(alpha * 255)));

            //大星星：十字光芒
            if (星.半径 > 2.6f)
            {
                float 光芒长 = 星.半径 * 6.0f;
                ImU32 光芒色 = IM_COL32(255, 240, 250, (int)(alpha * 180));
                画布->AddLine(ImVec2(位置.x - 光芒长, 位置.y),
                    ImVec2(位置.x + 光芒长, 位置.y), 光芒色, 1.0f);
                画布->AddLine(ImVec2(位置.x, 位置.y - 光芒长),
                    ImVec2(位置.x, 位置.y + 光芒长), 光芒色, 1.0f);
            }
        }
    }
}
