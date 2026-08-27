//============================================================================
// 配置编辑器 —— 视觉辅助（集中绘制函数）实现
// UI 美容计划 任务 2：渐变 / 投影 / 圆角卡片 / 标签 chip / 档案纸横线 / 圆角图片
// 所有函数均为纯绘制辅助，不持有状态、不触碰数据逻辑。
//============================================================================
#include "src/tools/GUI/Config_Editor/core/配置编辑器_视觉.h"

namespace engine
{
    // =========================================================================
    // 颜色工具
    // =========================================================================

    //两个颜色线性混合（t=0 返回 a，t=1 返回 b）
    ImU32 混合颜色(ImU32 a, ImU32 b, float t)
    {
        if (t <= 0.0f) return a;
        if (t >= 1.0f) return b;
        int ar = (a >> IM_COL32_R_SHIFT) & 0xFF;
        int ag = (a >> IM_COL32_G_SHIFT) & 0xFF;
        int ab = (a >> IM_COL32_B_SHIFT) & 0xFF;
        int aa = (a >> IM_COL32_A_SHIFT) & 0xFF;
        int br = (b >> IM_COL32_R_SHIFT) & 0xFF;
        int bg = (b >> IM_COL32_G_SHIFT) & 0xFF;
        int bb = (b >> IM_COL32_B_SHIFT) & 0xFF;
        int ba = (b >> IM_COL32_A_SHIFT) & 0xFF;
        return IM_COL32(
            (int)(ar + (br - ar) * t),
            (int)(ag + (bg - ag) * t),
            (int)(ab + (bb - ab) * t),
            (int)(aa + (ba - aa) * t));
    }

    //保留 RGB，替换透明度（alpha 0~255）
    ImU32 调整透明度(ImU32 c, int alpha)
    {
        if (alpha < 0) alpha = 0;
        if (alpha > 255) alpha = 255;
        return (c & 0x00FFFFFFu) | ((ImU32)alpha << IM_COL32_A_SHIFT);
    }

    //按字符串哈希取一个稳定的「粉紫系主题色」
    ImU32 类型主题色(const std::string& 文本)
    {
        //FNV-1a 哈希：同一字符串恒定同色
        unsigned int 哈希 = 2166136261u;
        for (char c : 文本)
        {
            哈希 ^= (unsigned char)c;
            哈希 *= 16777619u;
        }

        //在粉紫/蔷薇区间（280°~345°）内散开，避免全屏同色
        const float 色相 = 280.0f + (float)(哈希 % 65);
        //饱和度与亮度在柔和区间轻微抖动，保证文字可读
        const float 饱和度 = 0.38f + (float)((哈希 >> 8) % 14) / 100.0f;
        const float 亮度 = 0.72f + (float)((哈希 >> 12) % 12) / 100.0f;

        //HSV → RGB
        float c = 亮度 * 饱和度;
        float x = c * (1.0f - std::fabs(std::fmod(色相 / 60.0f, 2.0f) - 1.0f));
        float m = 亮度 - c;
        float r = 0.0f, g = 0.0f, b = 0.0f;
        const int 扇区 = (int)(色相 / 60.0f) % 6;
        switch (扇区)
        {
        case 0: r = c; g = x; b = 0.0f; break;
        case 1: r = x; g = c; b = 0.0f; break;
        case 2: r = 0.0f; g = c; b = x; break;
        case 3: r = 0.0f; g = x; b = c; break;
        case 4: r = x; g = 0.0f; b = c; break;
        default: r = c; g = 0.0f; b = x; break;
        }
        return IM_COL32(
            (int)((r + m) * 255.0f),
            (int)((g + m) * 255.0f),
            (int)((b + m) * 255.0f),
            255);
    }

    // =========================================================================
    // 圆角投影卡片（多层半透明矩形模拟柔和阴影）
    // =========================================================================
    void 绘制投影卡片(ImDrawList* 画布,
        const ImVec2& 左上, const ImVec2& 右下,
        ImU32 底色, float 圆角,
        float 阴影偏移, int 阴影层数)
    {
        if (画布 == nullptr) return;
        if (阴影层数 < 1) 阴影层数 = 1;

        //从外到内逐层画半透明深色圆角矩形，alpha 逐层加深，模拟柔和投影
        for (int 层 = 阴影层数; 层 >= 1; --层)
        {
            const float 比例 = (float)层 / (float)阴影层数;
            const float 扩展 = 阴影偏移 * 比例;
            const int alpha = (int)(28 * 比例 * 比例); //外层更淡、内层稍深
            if (alpha <= 0) continue;
            画布->AddRectFilled(
                ImVec2(左上.x - 扩展, 左上.y - 扩展),
                ImVec2(右下.x + 扩展, 右下.y + 扩展),
                IM_COL32(70, 40, 90, alpha),
                圆角 + 扩展 * 0.8f);
        }

        //主体
        画布->AddRectFilled(左上, 右下, 底色, 圆角);
    }

    // =========================================================================
    // 渐变矩形
    // =========================================================================

    //垂直渐变（上色 → 下色）
    void 绘制垂直渐变矩形(ImDrawList* 画布,
        const ImVec2& 左上, const ImVec2& 右下,
        ImU32 上色, ImU32 下色)
    {
        if (画布 == nullptr) return;
        画布->AddRectFilledMultiColor(
            左上, 右下,
            上色, 上色,   //左上 / 右上
            下色, 下色);  //右下 / 左下
    }

    //对角渐变（左上色 → 右下色）
    void 绘制对角渐变矩形(ImDrawList* 画布,
        const ImVec2& 左上, const ImVec2& 右下,
        ImU32 左上色, ImU32 右下色)
    {
        if (画布 == nullptr) return;
        画布->AddRectFilledMultiColor(
            左上, 右下,
            左上色, 混合颜色(左上色, 右下色, 0.5f),  //左上 / 右上（过渡）
            右下色, 混合颜色(左上色, 右下色, 0.5f)); //右下 / 左下（过渡）
    }

    // =========================================================================
    // 彩色圆角标签 chip
    // =========================================================================
    float 绘制标签chip(ImDrawList* 画布,
        const ImVec2& 左上, const char* 文本,
        ImU32 底色, ImU32 文字色,
        float 内边距x, float 内边距y, float 圆角)
    {
        if (画布 == nullptr || 文本 == nullptr || 文本[0] == '\0')
            return 0.0f;

        const ImVec2 文字尺寸 = ImGui::CalcTextSize(文本);
        const ImVec2 右下 = ImVec2(
            左上.x + 内边距x * 2.0f + 文字尺寸.x,
            左上.y + 内边距y * 2.0f + 文字尺寸.y);

        画布->AddRectFilled(左上, 右下, 底色, 圆角);
        画布->AddText(ImVec2(左上.x + 内边距x, 左上.y + 内边距y), 文字色, 文本);

        return 右下.x - 左上.x;
    }

    // =========================================================================
    // 档案纸横线背景
    // =========================================================================
    void 绘制档案纸横线(ImDrawList* 画布,
        const ImVec2& 左上, const ImVec2& 右下,
        float 行距, ImU32 线色)
    {
        if (画布 == nullptr || 行距 <= 1.0f) return;
        //从顶部第一个整数行距处开始，避免与内容文字基线冲突
        for (float y = 左上.y + 行距; y < 右下.y; y += 行距)
        {
            画布->AddLine(ImVec2(左上.x + 4.0f, y), ImVec2(右下.x - 4.0f, y), 线色, 1.0f);
        }
    }

    // =========================================================================
    // 圆角图片（阴影 + 圆角 + 细边框）
    // =========================================================================
    void 绘制圆角图片(ImDrawList* 画布,
        ImTextureRef 纹理,
        const ImVec2& 左上, const ImVec2& 右下,
        float 圆角, ImU32 边框色)
    {
        if (画布 == nullptr) return;

        //柔和阴影（两层即可，图片下阴影不需要太重）
        画布->AddRectFilled(
            ImVec2(左上.x - 4.0f, 左上.y - 3.0f),
            ImVec2(右下.x + 4.0f, 右下.y + 5.0f),
            IM_COL32(70, 40, 90, 40), 圆角 + 4.0f);
        画布->AddRectFilled(
            ImVec2(左上.x - 2.0f, 左上.y - 1.0f),
            ImVec2(右下.x + 2.0f, 右下.y + 3.0f),
            IM_COL32(70, 40, 90, 26), 圆角 + 2.0f);

        //圆角图片
        画布->AddImageRounded(
            纹理,
            左上, 右下,
            ImVec2(0, 0), ImVec2(1, 1),
            IM_COL32(255, 255, 255, 255),
            圆角);

        //细边框
        画布->AddRect(左上, 右下, 边框色, 圆角, 0, 1.5f);
    }
}
