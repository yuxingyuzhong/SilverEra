//============================================================================
// 配置编辑器 —— 图片加载
// 由 配置编辑器.cpp 拆分而来（架构改革 阶段 1），行为与拆分前完全一致
//============================================================================
#include <cstdio>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include "gui/Config_Editor/配置编辑器.h"
#include "gui/Config_Editor/配置编辑器_内部工具.h"
#include "gui/Config_Editor/实体配置模型_内部工具.h"

//Win32 文件选择对话框（GetOpenFileNameW）；NOMINMAX 避免 windows.h 的 min/max 宏污染
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <commdlg.h>
#pragma comment(lib, "comdlg32.lib")
#endif

namespace engine
{
    //加载帮助窗口右侧的昔涟图片（stb_image 解码 + OpenGL 纹理）
    //只在首次需要时加载一次；失败则保持纹理为 0，渲染层回退为字符画
    bool 配置编辑器::加载帮助图片()
    {
        if (帮助图片已尝试)
            return 帮助图片纹理 != 0;
        帮助图片已尝试 = true;

        //请求 RGBA 通道（简化纹理格式，避免行对齐问题）
        //注意：不要翻转！ImGui 的纹理约定是 uv(0,0)=纹理左上角=数据首行，
        //stb_image 默认首行就是图像顶部，直接上传即正立；翻转反而会上下颠倒
        stbi_set_flip_vertically_on_load(false);
        int 通道数 = 0;
        unsigned char* 像素 = nullptr;
        std::string 已用路径;

        //图片随程序一起分发，固定从 exe 同级 assets/UI/ 目录加载（程序自包含，不依赖外部路径）：
        //  1. 首选 cyrene_help.png（帮助窗口专用图）
        //  2. 缺失时兜底 cyrene_portrait.jpg（旧图）
        //解析资源路径 会先试 CWD、再用 exe 所在目录拼接，保证两种启动方式都能命中
        {
            std::string 路径 = 解析资源路径("assets/UI/cyrene_help.png");
            像素 = stbi_load(路径.c_str(), &帮助图片宽, &帮助图片高, &通道数, 4);
            if (像素 != nullptr)
                已用路径 = 路径;
        }

        //2. 兜底：旧图
        if (像素 == nullptr)
        {
            std::string 路径 = 解析资源路径("assets/UI/cyrene_portrait.jpg");
            像素 = stbi_load(路径.c_str(), &帮助图片宽, &帮助图片高, &通道数, 4);
            if (像素 != nullptr)
                已用路径 = 路径;
        }

        if (像素 == nullptr || 帮助图片宽 <= 0 || 帮助图片高 <= 0)
        {
            std::printf("[ConfigEditor] 警告：无法加载帮助图片（%s）\n",
                stbi_failure_reason() ? stbi_failure_reason() : "未知错误");
            帮助图片宽 = 帮助图片高 = 0;
            return false;
        }

        //创建 OpenGL 纹理
        glGenTextures(1, &帮助图片纹理);
        glBindTexture(GL_TEXTURE_2D, 帮助图片纹理);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 帮助图片宽, 帮助图片高,
            0, GL_RGBA, GL_UNSIGNED_BYTE, 像素);
        glBindTexture(GL_TEXTURE_2D, 0);
        stbi_image_free(像素);

        std::printf("[ConfigEditor] 已加载帮助图片：%s（%dx%d）\n",
            已用路径.c_str(), 帮助图片宽, 帮助图片高);
        return true;
    }

    //加载主界面封面图（stb_image 解码 + OpenGL 纹理，架构改革 阶段 4）
    //路径：assets/UI/cyrene_cover.jpg（随程序分发，从 exe 同级 assets/UI/ 加载）
    //只在首次需要时加载一次；失败则保持纹理为 0，主界面回退为占位文本
    bool 配置编辑器::加载封面图()
    {
        if (封面图已尝试)
            return 封面图纹理 != 0;
        封面图已尝试 = true;

        //与帮助图片一致：不翻转，RGBA 通道，uv(0,0)=纹理左上角=数据首行
        stbi_set_flip_vertically_on_load(false);
        int 通道数 = 0;
        std::string 路径 = 解析资源路径("assets/UI/cyrene_cover.jpg");
        unsigned char* 像素 = stbi_load(路径.c_str(), &封面图宽, &封面图高, &通道数, 4);

        if (像素 == nullptr || 封面图宽 <= 0 || 封面图高 <= 0)
        {
            std::printf("[ConfigEditor] 警告：无法加载主界面封面图（%s）\n",
                stbi_failure_reason() ? stbi_failure_reason() : "未知错误");
            封面图宽 = 封面图高 = 0;
            return false;
        }

        //创建 OpenGL 纹理
        glGenTextures(1, &封面图纹理);
        glBindTexture(GL_TEXTURE_2D, 封面图纹理);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 封面图宽, 封面图高,
            0, GL_RGBA, GL_UNSIGNED_BYTE, 像素);
        glBindTexture(GL_TEXTURE_2D, 0);
        stbi_image_free(像素);

        std::printf("[ConfigEditor] 已加载主界面封面图：%s（%dx%d）\n",
            路径.c_str(), 封面图宽, 封面图高);
        return true;
    }

    //把实体类型清理为安全文件名（保留中英文/数字/下划线/连字符，其余替换为下划线）
    //与 实体配置仓库::文件名清理 语义一致（仅本文件用，避免改动仓库私有接口）
    static std::string 实体图片文件名清理(const std::string& 名称)
    {
        std::string 结果 = 名称;
        for (char& c : 结果)
        {
            const bool 保留 =
                (c >= 'a' && c <= 'z') ||
                (c >= 'A' && c <= 'Z') ||
                (c >= '0' && c <= '9') ||
                c == '_' || c == '-' ||
                (c & 0x80) != 0;      //UTF-8 多字节（中文等）保留
            if (!保留)
                c = '_';
        }
        if (结果.empty())
            结果 = "entity";
        return 结果;
    }

    //加载指定实体的档案图片为 OpenGL 纹理（按 entity_image.json 元数据；失败返回 false 并占位）
    //只在首次需要时加载一次（纹理表缓存）；上传图片后由 上传实体图片 显式失效旧纹理再调用
    bool 配置编辑器::加载实体图片(const std::string& type)
    {
        //已有记录（无论成败）直接返回，避免每帧重复磁盘 IO
        auto 纹理it = 实体图片纹理表.find(type);
        if (纹理it != 实体图片纹理表.end())
            return 纹理it->second != 0;

        unsigned int 纹理 = 0;
        int 宽 = 0;
        int 高 = 0;
        std::string 相对路径 = 仓库.获取实体图片(type);
        if (!相对路径.empty())
        {
            //相对路径以 assets/ 为基准，拼绝对路径（utf8_path 保证中文路径正确）
            std::filesystem::path 绝对路径 = 仓库.资源目录() / utf8_path(相对路径);
            if (std::filesystem::is_regular_file(绝对路径))
            {
                //与帮助图片/封面图一致：不翻转，RGBA 通道
                stbi_set_flip_vertically_on_load(false);
                int 通道数 = 0;
                unsigned char* 像素 = stbi_load(
                    绝对路径.string().c_str(), &宽, &高, &通道数, 4);
                if (像素 != nullptr && 宽 > 0 && 高 > 0)
                {
                    glGenTextures(1, &纹理);
                    glBindTexture(GL_TEXTURE_2D, 纹理);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 宽, 高,
                        0, GL_RGBA, GL_UNSIGNED_BYTE, 像素);
                    glBindTexture(GL_TEXTURE_2D, 0);
                    stbi_image_free(像素);
                }
                else
                {
                    std::printf("[ConfigEditor] 警告：无法加载实体图片 %s（%s）\n",
                        绝对路径.string().c_str(),
                        stbi_failure_reason() ? stbi_failure_reason() : "未知错误");
                }
            }
        }

        实体图片纹理表[type] = 纹理;
        实体图片宽表[type] = 宽;
        实体图片高表[type] = 高;
        return 纹理 != 0;
    }

    //打开文件选择对话框选择图片并设为指定实体的档案图
    //（复制到 assets/UI/entity_images/、更新 entity_image.json、重新加载纹理；非 Windows 回退为路径输入）
    void 配置编辑器::上传实体图片(const std::string& type)
    {
        std::filesystem::path 源路径;
#ifdef _WIN32
        //GetOpenFileNameW：宽字符对话框，返回路径可直接构造 std::filesystem::path（中文路径安全）
        wchar_t 文件名[MAX_PATH] = {};
        OPENFILENAMEW ofn = {};
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = nullptr;
        ofn.lpstrFilter =
            L"图片文件\0*.png;*.jpg;*.jpeg;*.bmp;*.gif;*.webp\0"
            L"所有文件\0*.*\0";
        ofn.lpstrFile = 文件名;
        ofn.nMaxFile = MAX_PATH;
        ofn.lpstrTitle = L"选择实体图片";
        ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
        if (!GetOpenFileNameW(&ofn))
            return;   //用户取消，不视为错误
        源路径 = std::filesystem::path(文件名);
#else
        //非 Windows 回退：手动路径输入（计划要求 #ifdef _WIN32 隔离）
        ImGui::OpenPopup("##上传图片路径");
        if (ImGui::BeginPopupModal("##上传图片路径", nullptr,
                ImGuiWindowFlags_AlwaysAutoResize))
        {
            static char 缓冲[1024] = {};
            ImGui::InputText("图片路径", 缓冲, sizeof(缓冲));
            if (ImGui::Button("确定"))
            {
                源路径 = std::filesystem::path(缓冲);
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("取消"))
                ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }
        if (源路径.empty())
            return;
#endif

        if (!std::filesystem::is_regular_file(源路径))
        {
            状态消息 = "所选文件不存在：" + 源路径.string();
            return;
        }

        //目标目录：assets/UI/entity_images/（相对 assets/ 为 UI/entity_images/）
        std::filesystem::path 目标目录 = 仓库.资源目录() / "UI" / "entity_images";
        std::error_code ec;
        std::filesystem::create_directories(目标目录, ec);

        //目标文件名：实体类型清理 + 原扩展名（小写化）
        std::string 扩展名 = 源路径.extension().string();
        std::string 清理名 = 实体图片文件名清理(type);
        std::filesystem::path 目标路径 = 目标目录 / (清理名 + 扩展名);
        std::filesystem::copy_file(
            源路径, 目标路径,
            std::filesystem::copy_options::overwrite_existing, ec);
        if (ec)
        {
            状态消息 = "复制图片失败：" + ec.message();
            return;
        }

        //更新内存表并持久化元数据（相对 assets/ 的路径）
        std::string 相对路径 = "UI/entity_images/" + 清理名 + 扩展名;
        仓库.设置实体图片(type, 相对路径);
        std::string error;
        if (!仓库.保存实体图片表(error))
        {
            状态消息 = "图片已复制但元数据保存失败：" + error;
            return;
        }
        状态消息 = "已设置实体图片：" + 相对路径;

        //失效旧纹理并重新加载，本帧即可看到新图
        auto it = 实体图片纹理表.find(type);
        if (it != 实体图片纹理表.end())
        {
            if (it->second != 0)
                glDeleteTextures(1, &it->second);
            实体图片纹理表.erase(it);
            实体图片宽表.erase(type);
            实体图片高表.erase(type);
        }
        加载实体图片(type);

        std::printf("[ConfigEditor] 实体 %s 已设置图片：%s\n",
            type.c_str(), 相对路径.c_str());
    }

}
