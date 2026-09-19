# -*- coding: utf-8 -*-
# 阶段4 补拆：配置编辑器主程序.cpp → 外观部分（加载中文字体/应用粉色主题/渲染梦幻背景）
# 只移动不重写：外观函数体原样搬入 配置编辑器主程序_外观.cpp（engine 命名空间共享），
# 主程序保留 窗口关闭回调 + main() 骨架。
import io, os, shutil

CFG_DIR = r"D:\代码存储\代码仓库\游戏引擎\游戏引擎\src\tools\GUI\Config_Editor"
CORE_DIR = os.path.join(CFG_DIR, "core")
SRC = os.path.join(CORE_DIR, "配置编辑器主程序.cpp")

with io.open(SRC, "r", encoding="utf-8-sig", newline="") as f:
    text = f.read()
newline = "\r\n" if "\r\n" in text else "\n"
raw = text.split(newline)
total = len(raw)
print("总行数:", total, "换行符:", repr(newline))

# 锚点（1-based）：外观块 = [外观头注释, 渲染梦幻背景结束) ；窗口关闭回调块 = [271, 286)
# 用字符串定位更稳：找到「中文字体加载」标题行 与「GLFW 窗口关闭回调」标题行
def find_line(substr):
    for i, line in enumerate(raw):
        if substr in line:
            return i + 1
    raise RuntimeError("未找到: " + substr)

外观起始 = find_line("// 中文字体加载")
回调起始 = find_line("GLFW 窗口关闭回调")
main_起始 = find_line("// 主函数")
print(f"外观起始={外观起始} 回调起始={回调起始} main起始={main_起始}")

# 验证切片边界
for i in range(外观起始 - 2, 外观起始 + 2):
    print(f"  外观前 [{i+1}] {raw[i][:70]}")
for i in range(回调起始 - 3, 回调起始 + 2):
    print(f"  回调处 [{i+1}] {raw[i][:70]}")
for i in range(main_起始 - 3, main_起始 + 2):
    print(f"  main处 [{i+1}] {raw[i][:70]}")

# 外观块（含标题注释，不含匿名 namespace 壳）——函数体为 4 空格缩进，放入 engine namespace 恰好一致
外观块 = raw[外观起始 - 1 : 回调起始 - 1]
# 窗口关闭回调块（含注释 + 函数 + 匿名 namespace 结束符）
回调块 = raw[回调起始 - 1 : main_起始 - 1]
# main 块
main块 = raw[main_起始 - 1 :]

print("\n外观块行数:", len(外观块), "首行:", 外观块[0][:60], "末行:", 外观块[-1][:60])
print("回调块行数:", len(回调块), "首行:", 回调块[0][:60], "末行:", 回调块[-1][:60])
print("main块行数:", len(main块), "首行:", main块[0][:60], "末行:", main块[-1][:60])

# ---- 1. 外观共享头 ----
外观头 = (
    "#pragma once" + newline +
    "//============================================================================" + newline +
    "// 配置编辑器主程序 —— 外观（共享声明）" + newline +
    "// 由 配置编辑器主程序.cpp 的匿名命名空间外观函数拆分而来（架构改革 阶段 4）" + newline +
    "// 原外观函数位于匿名命名空间，拆分后改为 engine 命名空间共享函数，行为与拆分前完全一致。" + newline +
    "//============================================================================" + newline +
    '#include "common/前置头文件包含.h"' + newline +
    newline +
    "//引擎命名空间" + newline +
    "namespace engine" + newline +
    "{" + newline +
    "    //收集候选字体路径（按优先级，跨平台回退）" + newline +
    "    std::vector<std::string> 收集候选字体路径();" + newline +
    "    //尝试加载中文字体（失败返回 nullptr）" + newline +
    "    ImFont* 加载中文字体(float 像素大小);" + newline +
    "    //应用梦幻粉色主题（明丽版）" + newline +
    "    void 应用粉色主题();" + newline +
    "    //渲染梦幻背景（粉紫渐变 + 闪烁星光）" + newline +
    "    void 渲染梦幻背景();" + newline +
    "}" + newline
)

# ---- 2. 外观 cpp ----
外观cpp = (
    "//============================================================================" + newline +
    "// 配置编辑器主程序 —— 外观（中文字体 / 粉色主题 / 梦幻背景）" + newline +
    "// 由 配置编辑器主程序.cpp 拆分而来（架构改革 阶段 4），行为与拆分前完全一致" + newline +
    "//============================================================================" + newline +
    '#include "src/tools/GUI/Config_Editor/配置编辑器主程序_外观.h"' + newline +
    newline +
    "#include <random>" + newline +
    "#include <cmath>" + newline +
    "#include <cstdlib>" + newline +
    "#include <filesystem>" + newline +
    "#include <string>" + newline +
    "#include <vector>" + newline +
    newline +
    "//引擎命名空间" + newline +
    "namespace engine" + newline +
    "{" + newline +
    newline.join(外观块) + newline +
    "}" + newline
)

# ---- 3. 主程序（精简为 main 骨架 + 窗口关闭回调） ----
主程序 = (
    "//============================================================================" + newline +
    "// 配置编辑器主程序 —— main() 入口" + newline +
    "// ---------------------------------------------------------------------------" + newline +
    "// 职责：" + newline +
    "//   1. 初始化 GLFW 窗口 + OpenGL 上下文" + newline +
    "//   2. 初始化 Dear ImGui（GLFW + OpenGL3 后端）" + newline +
    "//   3. 运行主循环并驱动配置编辑器界面" + newline +
    "// 外观函数（中文字体/粉色主题/梦幻背景）已拆分到 配置编辑器主程序_外观.cpp（架构改革 阶段 4）" + newline +
    "//============================================================================" + newline +
    newline +
    '#include "common/前置头文件包含.h"' + newline +
    '#include "src/tools/GUI/Config_Editor/配置编辑器.h"' + newline +
    '#include "src/tools/GUI/Config_Editor/配置编辑器主程序_外观.h"' + newline +
    newline +
    "#include <random>" + newline +
    "#include <cmath>" + newline +
    "#include <cstdlib>" + newline +
    "#include <filesystem>" + newline +
    "#include <string>" + newline +
    "#include <vector>" + newline +
    newline +
    "//引擎命名空间" + newline +
    "using namespace engine;" + newline +
    newline +
    "//=============================================================================" + newline +
    "// GLFW 窗口关闭回调（关闭确认接线）" + newline +
    "// 由 main 所在编译单元持有：窗口 × 按钮先交给编辑器（有未保存修改则弹确认窗）" + newline +
    "//=============================================================================" + newline +
    "namespace" + newline +
    "{" + newline +
    newline.join(回调块) + newline +
    "}" + newline +
    newline +
    newline.join(main块) + newline
)

# ---- 写出（先备份 .orig，若不存在） ----
orig = SRC + ".orig"
if not os.path.exists(orig):
    shutil.copy2(SRC, orig)
    print("备份:", orig)
else:
    print("备份已存在，跳过:", orig)

for path, content in [("配置编辑器主程序_外观.h", 外观头),
                      (os.path.join(CORE_DIR, "配置编辑器主程序_外观.cpp"), 外观cpp),
                      (SRC, 主程序)]:
    with io.open(path, "w", encoding="utf-8", newline="") as f:
        f.write(content)
    n = content.count(newline)
    print("写出: %s (%d 行, %d 字节)" % (os.path.basename(path), n, len(content.encode("utf-8"))))

# ---- 完整性自检：外观 + 主程序 应覆盖原文件 外观头..末尾 ----
print("\n===== 完整性自检 =====")
# 原文件所有函数行都应在新文件中出现
关键函数 = ["收集候选字体路径", "加载中文字体", "应用粉色主题", "渲染梦幻背景", "窗口关闭回调", "int main(void)"]
所有文本 = 外观cpp + 主程序
for fn in 关键函数:
    print(("  ✓ " if fn in 所有文本 else "  ✗ 缺失 ") + fn)
print("OK 阶段4 外观拆分完成")
