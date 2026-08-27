#pragma once
//============================================================================
// 配置编辑器 —— 视觉辅助（集中绘制函数）
// UI 美容计划 任务 2：把渐变 / 投影 / 圆角卡片 / 标签 chip / 档案纸横线 /
// 圆角图片 等绘制逻辑集中到这一处，供主界面 / 档案列表 / 档案详情复用。
// 纯绘制辅助：不持有状态、不触碰数据逻辑，所有颜色与尺寸均显式传入。
//============================================================================
#include "common/前置头文件包含.h"

#include <string>

namespace engine
{
    // —— 颜色工具 ——

    //两个颜色线性混合（t=0 返回 a，t=1 返回 b；仅用于颜色，不改变 alpha 语义）
    ImU32 混合颜色(ImU32 a, ImU32 b, float t);
    //保留 RGB，替换透明度（alpha 0~255）
    ImU32 调整透明度(ImU32 c, int alpha);
    //按字符串哈希取一个稳定的「粉紫系主题色」（档案列表左侧色条 / 详情 chip 底色）
    //同一字符串始终得到同一颜色，不同字符串在粉紫区间内柔和散开
    ImU32 类型主题色(const std::string& 文本);

    // —— 圆角投影卡片 ——

    //在 左上~右下 先画 阴影层数 层半透明矩形模拟柔和投影，再画主体圆角矩形
    //阴影向外扩散 阴影偏移 像素，逐层减弱透明度，观感更「浮起」
    void 绘制投影卡片(ImDrawList* 画布,
        const ImVec2& 左上, const ImVec2& 右下,
        ImU32 底色, float 圆角,
        float 阴影偏移 = 6.0f, int 阴影层数 = 4);

    // —— 渐变矩形 ——

    //垂直渐变（上色 → 下色）
    void 绘制垂直渐变矩形(ImDrawList* 画布,
        const ImVec2& 左上, const ImVec2& 右下,
        ImU32 上色, ImU32 下色);
    //对角渐变（左上色 → 右下色，用于封面/标题装饰线等）
    void 绘制对角渐变矩形(ImDrawList* 画布,
        const ImVec2& 左上, const ImVec2& 右下,
        ImU32 左上色, ImU32 右下色);

    // —— 彩色圆角标签 chip ——

    //在 左上 画一个带文字的彩色圆角小标签，返回标签宽度（方便连续摆放多个 chip）
    float 绘制标签chip(ImDrawList* 画布,
        const ImVec2& 左上, const char* 文本,
        ImU32 底色, ImU32 文字色,
        float 内边距x = 8.0f, float 内边距y = 3.0f, float 圆角 = 6.0f);

    // —— 档案纸横线背景 ——

    //在 左上~右下 内按 行距 循环画淡色横线（档案纸质感），线色默认极淡黑
    void 绘制档案纸横线(ImDrawList* 画布,
        const ImVec2& 左上, const ImVec2& 右下,
        float 行距 = 22.0f, ImU32 线色 = IM_COL32(0, 0, 0, 14));

    // —— 圆角图片（阴影 + 圆角 + 细边框） ——

    //先画柔和阴影，再画圆角图片，最后描一圈细边框；纹理传 ImTextureID 即可（可隐式转 ImTextureRef）
    void 绘制圆角图片(ImDrawList* 画布,
        ImTextureRef 纹理,
        const ImVec2& 左上, const ImVec2& 右下,
        float 圆角, ImU32 边框色 = IM_COL32(255, 255, 255, 90));
}
