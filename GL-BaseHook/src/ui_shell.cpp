#include "pch.h"

#include "diagnostics.h"
#include "esp.h"
#include "feature_infinite_health.h"
#include "feature_instant_build.h"
#include "feature_reveal_map.h"
#include "feature_veterancy_max.h"
#include "log.h"
#include "runtime.h"
#include "ui_shell.h"

namespace
{
    std::atomic_bool g_initialized{ false };
    std::atomic_bool g_menuVisible{ true };
    HWND g_window = nullptr;
    HGLRC g_renderContext = nullptr;

    // ImGui 默认字体(ProggyClean)不含 CJK 字形，中文会被渲染成 "?"。
    // 按优先级从系统字体目录加载一个支持中文的字体，注册全部常用汉字字形。
    void LoadChineseFont()
    {
        static const char* const kFontCandidates[] = {
            "C:\\Windows\\Fonts\\msyh.ttc",   // 微软雅黑
            "C:\\Windows\\Fonts\\msyhbd.ttc", // 微软雅黑 Bold
            "C:\\Windows\\Fonts\\simhei.ttf", // 黑体
            "C:\\Windows\\Fonts\\simsun.ttc", // 宋体
        };

        ImGuiIO& io = ImGui::GetIO();
        for (const char* const path : kFontCandidates)
        {
            if (GetFileAttributesA(path) == INVALID_FILE_ATTRIBUTES)
            {
                continue;
            }
            if (io.Fonts->AddFontFromFileTTF(path, 17.0f, nullptr, io.Fonts->GetGlyphRangesChineseFull()))
            {
                Ra2Overlay::Log::Write("UI font loaded: %s", path);
                return;
            }
        }
        Ra2Overlay::Log::Write("UI font: no Chinese-capable system font found");
    }

    void ApplyRa2Style()
    {
        ImGuiStyle& style = ImGui::GetStyle();
        ImVec4* colors = style.Colors;

        style.WindowPadding = ImVec2(10.0f, 10.0f);
        style.FramePadding = ImVec2(8.0f, 6.0f);
        style.ItemSpacing = ImVec2(8.0f, 7.0f);
        style.FrameRounding = 2.0f;
        style.TabRounding = 2.0f;
        style.FrameBorderSize = 1.0f;
        style.TabBorderSize = 1.0f;
        style.TabBarBorderSize = 1.0f;
        style.TabBarOverlineSize = 3.0f;

        colors[ImGuiCol_WindowBg] = ImVec4(0.025f, 0.040f, 0.055f, 0.96f);
        colors[ImGuiCol_Border] = ImVec4(0.20f, 0.42f, 0.58f, 0.75f);
        colors[ImGuiCol_Text] = ImVec4(0.91f, 0.95f, 0.97f, 1.00f);
        colors[ImGuiCol_TextDisabled] = ImVec4(0.46f, 0.54f, 0.59f, 1.00f);

        colors[ImGuiCol_Button] = ImVec4(0.07f, 0.12f, 0.17f, 1.00f);
        colors[ImGuiCol_ButtonHovered] = ImVec4(0.08f, 0.35f, 0.56f, 1.00f);
        colors[ImGuiCol_ButtonActive] = ImVec4(0.10f, 0.55f, 0.82f, 1.00f);

        colors[ImGuiCol_FrameBg] = ImVec4(0.06f, 0.10f, 0.14f, 1.00f);
        colors[ImGuiCol_FrameBgHovered] = ImVec4(0.08f, 0.28f, 0.42f, 1.00f);
        colors[ImGuiCol_FrameBgActive] = ImVec4(0.09f, 0.43f, 0.64f, 1.00f);

        colors[ImGuiCol_Header] = ImVec4(0.07f, 0.18f, 0.27f, 1.00f);
        colors[ImGuiCol_HeaderHovered] = ImVec4(0.08f, 0.35f, 0.54f, 1.00f);
        colors[ImGuiCol_HeaderActive] = ImVec4(0.10f, 0.50f, 0.74f, 1.00f);

        colors[ImGuiCol_Tab] = ImVec4(0.06f, 0.11f, 0.16f, 1.00f);
        colors[ImGuiCol_TabHovered] = ImVec4(0.30f, 0.20f, 0.04f, 1.00f);
        colors[ImGuiCol_TabSelected] = ImVec4(0.03f, 0.39f, 0.68f, 1.00f);
        colors[ImGuiCol_TabSelectedOverline] = ImVec4(0.55f, 0.92f, 1.00f, 1.00f);
        colors[ImGuiCol_TabDimmed] = ImVec4(0.04f, 0.08f, 0.11f, 1.00f);
        colors[ImGuiCol_TabDimmedSelected] = ImVec4(0.03f, 0.31f, 0.54f, 1.00f);
        colors[ImGuiCol_TabDimmedSelectedOverline] = ImVec4(0.38f, 0.75f, 0.86f, 1.00f);
        colors[ImGuiCol_PopupBg] = ImVec4(0.025f, 0.040f, 0.055f, 0.99f);

        colors[ImGuiCol_CheckMark] = ImVec4(0.28f, 0.84f, 1.00f, 1.00f);
        colors[ImGuiCol_SliderGrab] = ImVec4(0.24f, 0.62f, 0.82f, 1.00f);
        colors[ImGuiCol_SliderGrabActive] = ImVec4(0.95f, 0.78f, 0.24f, 1.00f);
        colors[ImGuiCol_NavCursor] = ImVec4(0.95f, 0.78f, 0.24f, 1.00f);
    }

    void DrawHoverFeedback(ImU32 color, const char* hint)
    {
        if (!ImGui::IsItemHovered())
        {
            return;
        }

        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

        const ImVec2 itemMin = ImGui::GetItemRectMin();
        const ImVec2 itemMax = ImGui::GetItemRectMax();
        ImDrawList* windowDrawList = ImGui::GetWindowDrawList();
        windowDrawList->AddRect(
            ImVec2(itemMin.x - 1.0f, itemMin.y - 1.0f),
            ImVec2(itemMax.x + 1.0f, itemMax.y + 1.0f),
            color,
            ImGui::GetStyle().FrameRounding,
            0,
            2.0f);

        const ImVec2 mousePosition = ImGui::GetIO().MousePos;
        ImGui::GetForegroundDrawList()->AddCircle(mousePosition, 9.0f, color, 20, 2.0f);
        ImGui::SetTooltip("%s", hint);
    }

    void DecorateTab(const char* label, bool selected)
    {
        const ImVec2 itemMin = ImGui::GetItemRectMin();
        const ImVec2 itemMax = ImGui::GetItemRectMax();

        if (selected)
        {
            ImGui::GetWindowDrawList()->AddRectFilled(
                ImVec2(itemMin.x + 2.0f, itemMax.y - 3.0f),
                ImVec2(itemMax.x - 2.0f, itemMax.y),
                IM_COL32(140, 235, 255, 255));
        }

        if (ImGui::IsItemHovered())
        {
            char hint[96]{};
            sprintf_s(
                hint,
                selected ? "Current page: %s" : "Click to open %s",
                label);
            DrawHoverFeedback(IM_COL32(255, 194, 66, 255), hint);
        }
    }

    bool DangerousButton(const char* label)
    {
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.62f, 0.12f, 0.12f, 1.00f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.86f, 0.18f, 0.14f, 1.00f));
        const bool clicked = ImGui::Button(label);
        DrawHoverFeedback(IM_COL32(255, 86, 72, 255), "Click to unload overlay");
        ImGui::PopStyleColor(2);
        return clicked;
    }
}

bool Ra2Overlay::UiShell::Initialize(HWND window, HDC deviceContext, HGLRC renderContext)
{
    if (IsInitialized())
    {
        return Matches(window, renderContext);
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ApplyRa2Style();
    LoadChineseFont();
    ImGui::GetIO().IniFilename = nullptr;

    if (!ImGui_ImplWin32_Init(window))
    {
        Log::Write("ImGui Win32 backend initialization failed");
        ImGui::DestroyContext();
        return false;
    }

    if (!ImGui_ImplOpenGL2_Init())
    {
        Log::Write("ImGui OpenGL2 backend initialization failed");
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
        return false;
    }

    g_window = window;
    g_renderContext = renderContext;
    Diagnostics::Capture(window, deviceContext, renderContext);
    g_initialized.store(true, std::memory_order_release);

    const Diagnostics::Snapshot& info = Diagnostics::Get();
    Log::Write("ImGui initialized: GL_VERSION=%s", info.glVersion.c_str());
    Log::Write("GL_VENDOR=%s", info.glVendor.c_str());
    Log::Write("GL_RENDERER=%s", info.glRenderer.c_str());
    return true;
}

void Ra2Overlay::UiShell::RenderFrame(HDC deviceContext)
{
    if (!IsInitialized())
    {
        return;
    }

    Diagnostics::UpdateViewport(deviceContext);
    ImGui_ImplOpenGL2_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    if (MenuVisible())
    {
        ImGui::SetNextWindowSize(ImVec2(620.0f, 430.0f), ImGuiCond_FirstUseEver);
        bool open = true;
        if (ImGui::Begin("Ra2Overlay", &open))
        {
            if (ImGui::BeginTabBar("MainTabs"))
            {
                const bool statusOpen = ImGui::BeginTabItem("Status");
                DecorateTab("Status", statusOpen);
                if (statusOpen)
                {
                    const Diagnostics::Snapshot& info = Diagnostics::Get();
                    ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
                    ImGui::Text("Client: %d x %d", info.clientWidth, info.clientHeight);
                    ImGui::Text("HWND: 0x%p", info.window);
                    ImGui::Text("HDC: 0x%p", info.deviceContext);
                    ImGui::Text("HGLRC: 0x%p", info.renderContext);
                    ImGui::Separator();
                    ImGui::TextWrapped("GL_VERSION: %s", info.glVersion.c_str());
                    ImGui::TextWrapped("GL_VENDOR: %s", info.glVendor.c_str());
                    ImGui::TextWrapped("GL_RENDERER: %s", info.glRenderer.c_str());
                    ImGui::Separator();
                    ImGui::TextUnformatted("Hook: GDI32!SwapBuffers");
                    ImGui::TextUnformatted("Build: Win32/x86, VS2022 v143");
                    if (DangerousButton("Unload overlay"))
                    {
                        Runtime::RequestShutdown();
                    }
                    ImGui::EndTabItem();
                }

                const bool logOpen = ImGui::BeginTabItem("Log");
                DecorateTab("Log", logOpen);
                if (logOpen)
                {
                    const std::vector<std::string> lines = Log::Snapshot();
                    ImGui::BeginChild("LogLines", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders);
                    for (const std::string& line : lines)
                    {
                        ImGui::TextUnformatted(line.c_str());
                    }
                    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
                    {
                        ImGui::SetScrollHereY(1.0f);
                    }
                    ImGui::EndChild();
                    ImGui::EndTabItem();
                }

                const bool configurationOpen = ImGui::BeginTabItem("Configuration");
                DecorateTab("Configuration", configurationOpen);
                if (configurationOpen)
                {
                    ImGui::TextUnformatted("Insert: show or hide overlay");
                    ImGui::TextUnformatted("End: unload overlay");
                    ImGui::TextUnformatted("Renderer backend: OpenGL2");
                    ImGui::EndTabItem();
                }

                const bool espOpen = ImGui::BeginTabItem("ESP");
                DecorateTab("ESP", espOpen);
                if (espOpen)
                {
                    Ra2Overlay::Esp::RenderConfig();
                    ImGui::EndTabItem();
                }

                const bool revealMapOpen = ImGui::BeginTabItem("Reveal Map");
                DecorateTab("Reveal Map", revealMapOpen);
                if (revealMapOpen)
                {
                    Ra2Overlay::RevealMap::RenderConfig();
                    ImGui::EndTabItem();
                }

                const bool memoryOpen = ImGui::BeginTabItem("Memory Features");
                DecorateTab("Memory Features", memoryOpen);
                if (memoryOpen)
                {
                    Ra2Overlay::InfiniteHealth::RenderConfig();
                    Ra2Overlay::InstantBuild::RenderConfig();
                    Ra2Overlay::VeterancyMax::RenderConfig();
                    ImGui::Separator();
                    ImGui::TextWrapped("写操作在联机对局中会导致 desync，全部功能仅限单机使用。");
                    ImGui::EndTabItem();
                }
                ImGui::EndTabBar();
            }
        }
        ImGui::End();

        if (!open)
        {
            g_menuVisible.store(false, std::memory_order_release);
        }
    }

        // ESP 绘制：在 ImGui 本帧绘制数据定稿前写入前景层，随 SwapBuffers 一起上屏。
        Ra2Overlay::Esp::Render();

        ImGui::Render();
        ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());
}

void Ra2Overlay::UiShell::Shutdown()
{
    if (!g_initialized.exchange(false, std::memory_order_acq_rel))
    {
        return;
    }

    ImGui_ImplOpenGL2_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    g_renderContext = nullptr;
    g_window = nullptr;
    Log::Write("ImGui shut down on render thread");
}

void Ra2Overlay::UiShell::AbandonAfterTimeout()
{
    if (!g_initialized.exchange(false, std::memory_order_acq_rel))
    {
        return;
    }

    if (ImGui::GetCurrentContext())
    {
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
    }
    g_renderContext = nullptr;
    g_window = nullptr;
    Log::Write("ImGui renderer cleanup abandoned because no render callback arrived");
}

bool Ra2Overlay::UiShell::IsInitialized()
{
    return g_initialized.load(std::memory_order_acquire);
}

bool Ra2Overlay::UiShell::Matches(HWND window, HGLRC renderContext)
{
    return IsInitialized() && g_window == window && g_renderContext == renderContext;
}

void Ra2Overlay::UiShell::ToggleMenu()
{
    g_menuVisible.store(!g_menuVisible.load(std::memory_order_acquire), std::memory_order_release);
}

bool Ra2Overlay::UiShell::MenuVisible()
{
    return g_menuVisible.load(std::memory_order_acquire);
}
