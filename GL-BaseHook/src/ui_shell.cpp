#include "pch.h"

#include "diagnostics.h"
#include "esp.h"
#include "feature_infinite_health.h"
#include "feature_instant_build.h"
#include "feature_reveal_map.h"
#include "feature_veterancy_max.h"
#include "feature_unlimited_money.h"
#include "feature_unlimited_power.h"
#include "feature_fast_mining.h"
#include "feature_unlimited_superweapon.h"
#include "feature_launch_nuke.h"
#include "feature_unlimited_firepower.h"
#include "feature_instant_turn.h"
#include "feature_auto_repair.h"
#include "feature_speed_control.h"
#include "feature_iam_winner.h"
#include "feature_delete_unit.h"
#include "feature_build_everywhere.h"
#include "feature_unlimit_tech.h"
#include "feature_range_max.h"
#include "feature_force_fire.h"
#include "feature_unit_speed_up.h"
#include "log.h"
#include "runtime.h"
#include "ui_shell.h"

#include <shellapi.h>
#include <cstring>

namespace
{
    std::atomic_bool g_initialized{ false };
    std::atomic_bool g_menuVisible{ true };
    HWND g_window = nullptr;
    HGLRC g_renderContext = nullptr;

    // ImGui 默认字体(ProggyClean)不含 CJK 字形，中文会被渲染成 "?"。
    //
    // 根因（实测于 ImGui 1.92.9）：
    //   1) 1.92 起的字体架构已重写为「按需懒烘焙」——字形在渲染时才被 FontBakedLoadGlyph
    //      逐字符烘焙进 atlas。ImFontConfig::GlyphRanges 在 1.92 中仅对 legacy backend 有
    //      作用，OpenGL2 backend 不是 legacy，因此自造的 kGlyphRanges 实际上被忽略；
    //      字符是否可显示只取决于「AddFont 成功」+「运行时字符被查找并烘焙」。
    //   2) 手工 OversampleH/OversampleV=1 + PixelSnapH=true 在 1.92 是反优化：
    //      OversampleH=0(自动) 才能让 17px CJK 烘焙出足够笔画细节；PixelSnapH=true
    //      与 OversampleH=1 叠加会导致字符宽度截断。
    //   3) AddFontFromFileTTF 返回非空 ≠ 字体可用：stbtt_InitFont / stbtt_GetFontOffsetForIndex
    //      失败时只在 IM_ASSERT_USER_ERROR 触发，Release 下不会进 Log，无法定位。
    //   4) 1.92 不再在 backend 初始化时 Build 字体集；OpenGL2 backend 通过
    //      ImTextureStatus_WantCreate 在首帧 NewFrame 自行创建纹理。
    //
    // 修复策略：
    //   a) 不再手造 GlyphRanges——1.92 会按字符查找；为兼容老 backend 仍传入权威范围表
    //      （ImFontAtlas::GetGlyphRangesChineseSimplifiedCommon），并用 ImFontGlyphRangesBuilder
    //      在 ASCII/标点/全角/常用汉字之外做覆盖加成。
    //   b) OversampleH/V 设为 0（自动），PixelSnapH 设为 false，避免 17px CJK 被截断。
    //   c) 用 ImFont::IsGlyphInFont() 探测代表 CJK 字符是否真的被字源承载——
    //      这条路径与真实渲染查找完全一致；不通过 Build() 强制（在新 backend 下会 assert）。
    //   d) 对 TTC 显式 FontNo=0（集合第一字面），与原行为一致。
    //   e) 失败时逐条记录「文件是否存在 / AddFont 成功否 / 字符探测结果」，
    //      并在全部失败时回退到 ImFontAtlas::AddFontDefault()（ProggyClean）以保证
    //      至少 ASCII 可见，避免完全无字可用。
    // 探测某个 CJK 字符是否真的被这套字体的字源承载（而非退化到 fallback 方框）
    // ImGui 1.92.9 提供 ImFont::IsGlyphInFont(c)：遍历所有 FontSrc，
    // 调用 stb_truetype 的 FontSrcContainsGlyph → stbtt_FindGlyphIndex。
    // 这是判断「中文字形真实存在」的最直接 API，比直接调 baked->FindGlyph 更稳
    // （后者在 lazy-bake 模型下会触发按需烘焙，语义混淆）。
    static bool ProbeChineseGlyph(ImFont* font, ImWchar cp)
    {
        if (font == nullptr)
        {
            return false;
        }
        return font->IsGlyphInFont(cp);
    }

    void LoadChineseFont()
    {
        static const char* const kFontCandidates[] = {
            "C:\\Windows\\Fonts\\simhei.ttf",   // 黑体（纯 TTF，单字面，stb_truetype 兼容性最好）
            "C:\\Windows\\Fonts\\msyh.ttc",     // 微软雅黑 Regular（TTC, FontNo=0）
            "C:\\Windows\\Fonts\\msyhbd.ttc",   // 微软雅黑 Bold（TTC, FontNo=0）
            "C:\\Windows\\Fonts\\simsun.ttc",   // 宋体（TTC, FontNo=0）
        };

        // 探测用的代表 CJK 字符：覆盖「经济/超武/战斗/战略」四个中文 Tab 都用到的字
        // 选择「战」是因为它在功能菜单中确定出现；「字」覆盖最常见汉字。
        static const ImWchar kProbeChinese[] = {
            0x4E2D, // 中
            0x6587, // 文
            0x5B57, // 字
            0x6218, // 战
            0x7565, // 略
            0x7ECF, // 经
            0x6D4B, // 测
        };

        ImGuiIO& io = ImGui::GetIO();
        ImFontAtlas* atlas = io.Fonts;

        // ImFontConfig 默认：OversampleH/V=0(自动), PixelSnapH=false
        // 不要手写 Oversample=1 / PixelSnapH=true，1.92 下会破坏 CJK 烘焙质量
        ImFontConfig config;
        config.FontNo = 0;             // TTC 集合第一字面
        config.OversampleH = 0;        // 自动
        config.OversampleV = 0;        // 自动
        config.PixelSnapH = false;     // 重要：true 会让 17px CJK 宽度被截断
        config.PixelSnapV = false;

        // 用 ImFontGlyphRangesBuilder 构造权威字形范围：
        // 1) Default 基础拉丁
        // 2) 通用标点
        // 3) CJK 符号/标点 + 假名
        // 4) 全角形式
        // 5) 常用汉字（与原 0x4E00-0x9FA5 范围等价但不依赖自定义）
        // 6) 探测字符集合
        // 这套范围同时满足「legacy backend 的 GlyphRanges 字段」与「1.92 懒烘焙后的
        // 可视字符集合一致」两种语义。
        ImFontGlyphRangesBuilder builder;
        builder.AddRanges(atlas->GetGlyphRangesDefault());                 // ASCII
        builder.AddRanges(atlas->GetGlyphRangesChineseSimplifiedCommon()); // 1.92 权威中文常用集
        for (ImWchar cp : kProbeChinese)
        {
            builder.AddChar(cp);
        }
        ImVector<ImWchar> ranges;
        builder.BuildRanges(&ranges);

        ImFont* loadedFont = nullptr;
        std::string failureSummary;

        for (const char* const path : kFontCandidates)
        {
            // 1) 文件存在性 — 单独的 Win32 API 检查，区分「路径错误」与「解析失败」
            const DWORD attrs = GetFileAttributesA(path);
            const bool fileExists = (attrs != INVALID_FILE_ATTRIBUTES &&
                                     !(attrs & FILE_ATTRIBUTE_DIRECTORY));
            if (!fileExists)
            {
                failureSummary += std::string(path) + ": not found; ";
                continue;
            }

            // 2) AddFont（包含读取 + stb_truetype parse）
            ImFont* font = atlas->AddFontFromFileTTF(path, 17.0f, &config, ranges.Data);
            if (font == nullptr)
            {
                failureSummary += std::string(path) + ": AddFontFromFileTTF failed (stb_truetype parse error); ";
                continue;
            }

            // 3) 探测代表字符是否真的被承载（不是被 fallback 到方框）
            //    注意：在 1.92 下，IsGlyphInFont 走 stb_truetype 的 FontSrcContainsGlyph
            //    → stbtt_FindGlyphIndex，路径与真实渲染查找一致。
            //    不调 atlas->Build()：OpenGL2 backend (1.92+) 已设置
            //    RendererHasTextures，此时 Build() 是 obsoleted no-op（debug 下会 assert）。
            bool glyphOk = true;
            for (ImWchar cp : kProbeChinese)
            {
                if (!ProbeChineseGlyph(font, cp))
                {
                    glyphOk = false;
                    break;
                }
            }
            if (!glyphOk)
            {
                failureSummary += std::string(path) + ": parsed but no CJK glyphs found; ";
                // 移除刚加入的 font，避免脏数据影响 atlas
                atlas->RemoveFont(font);
                continue;
            }

            // 命中：代表 CJK 字符全部被字源承载（IsGlyphInFont 已证明），
            // 运行时渲染会按需逐字符烘焙，无需在此预烘焙。
            loadedFont = font;
            Ra2Overlay::Log::Write(
                "UI font loaded: %s (probeCJK=ok, %zu probe chars verified)",
                path, sizeof(kProbeChinese) / sizeof(kProbeChinese[0]));
            break;
        }

        if (loadedFont != nullptr)
        {
            io.FontDefault = loadedFont;
            // 注意：不在此处调用 io.Fonts->Build()——OpenGL2 backend (1.92+) 在
            // NewFrame 中通过 ImTextureStatus_WantCreate 自行完成纹理上传；
            // 提前 Build 反而会因「无 GL 上下文」触发问题。让 backend 接管。
            return;
        }

        // 全部失败：明确诊断 + 可靠回退
        Ra2Overlay::Log::Write("UI font: no Chinese-capable system font usable. %s", failureSummary.c_str());

        // 回退到 ImGui 内置 ProggyClean（无 CJK，但 ASCII 仍可读）
        // 传 nullptr：内置字体自带尺寸，避免复用上面的 config(SizePixels=0) 造成歧义
        ImFont* fallback = atlas->AddFontDefault(nullptr);
        if (fallback != nullptr)
        {
            io.FontDefault = fallback;
            Ra2Overlay::Log::Write("UI font: fell back to ImGui default (ProggyClean, ASCII only)");
        }
        else
        {
            Ra2Overlay::Log::Write("UI font: AddFontDefault also failed; ImGui will use nullptr font");
        }
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

    // About 页：在系统浏览器中打开 URL。
    void OpenUrl(const char* url)
    {
        ShellExecuteA(nullptr, "open", url, nullptr, nullptr, SW_SHOWNORMAL);
    }

    // About 页：把 QQ 群号复制到剪贴板，方便用户加群。
    void CopyToClipboard(const char* text)
    {
        if (!OpenClipboard(nullptr))
        {
            return;
        }
        EmptyClipboard();

        const SIZE_T bytes = (std::strlen(text) + 1) * sizeof(char);
        HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, bytes);
        if (hMem)
        {
            void* const p = GlobalLock(hMem);
            if (p)
            {
                std::memcpy(p, text, bytes);
                GlobalUnlock(hMem);
                SetClipboardData(CF_TEXT, hMem);
            }
            else
            {
                GlobalFree(hMem);
            }
        }
        CloseClipboard();
    }

    // About 页：可点击的链接行（打开网页 / 复制群号），带悬停手型与提示。
    void AboutLink(const char* label, const char* action, bool copyMode)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.45f, 0.75f, 0.95f, 1.00f));
        const bool clicked = ImGui::Selectable(label);
        ImGui::PopStyleColor();
        if (ImGui::IsItemHovered())
        {
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
            ImGui::SetTooltip("%s", action);
        }
        if (clicked)
        {
            if (copyMode)
            {
                CopyToClipboard(action);
            }
            else
            {
                OpenUrl(action);
            }
        }
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
                    if (ImGui::BeginTabBar("MemorySubTabs"))
                    {
                        // Economy
                        if (ImGui::BeginTabItem("Economy"))
                        {
                            Ra2Overlay::UnlimitedMoney::RenderConfig();
                            Ra2Overlay::UnlimitedPower::RenderConfig();
                            Ra2Overlay::FastMining::RenderConfig();
                            Ra2Overlay::InstantBuild::RenderConfig();
                            Ra2Overlay::BuildEveryWhere::RenderConfig();
                            Ra2Overlay::UnlimitTech::RenderConfig();
                            ImGui::EndTabItem();
                        }
                        // Superweapons
                        if (ImGui::BeginTabItem("Superweapons"))
                        {
                            Ra2Overlay::UnlimitedSuperweapon::RenderConfig();
                            Ra2Overlay::LaunchNuke::RenderConfig();
                            ImGui::EndTabItem();
                        }
                        // Combat
                        if (ImGui::BeginTabItem("Combat"))
                        {
                            Ra2Overlay::UnlimitedFirepower::RenderConfig();
                            Ra2Overlay::InstantTurn::RenderConfig();
                            Ra2Overlay::AutoRepair::RenderConfig();
                            Ra2Overlay::InfiniteHealth::RenderConfig();
                            Ra2Overlay::VeterancyMax::RenderConfig();
                            Ra2Overlay::RangeMax::RenderConfig();
                            Ra2Overlay::ForceFire::RenderConfig();
                            Ra2Overlay::UnitSpeedUp::RenderConfig();
                            ImGui::EndTabItem();
                        }
                        // Strategy
                        if (ImGui::BeginTabItem("Strategy"))
                        {
                            Ra2Overlay::SpeedControl::RenderConfig();
                            Ra2Overlay::IamWinner::RenderConfig();
                            Ra2Overlay::DeleteUnit::RenderConfig();
                            ImGui::EndTabItem();
                        }
                        ImGui::EndTabBar();
                    }
                    ImGui::Separator();
                    ImGui::TextWrapped("Write operations cause desync in online matches; all features are for single-player use only.");
                    ImGui::EndTabItem();
                }

                const bool aboutOpen = ImGui::BeginTabItem("About");
                DecorateTab("About", aboutOpen);
                if (aboutOpen)
                {
                    ImGui::TextWrapped("Ra2Overlay - RA2: Yuri's Revenge (YR 1.001) single-player trainer overlay. All features are for offline use only.");
                    ImGui::Separator();
                    ImGui::TextUnformatted("Author: tearhacker");
                    ImGui::Separator();
                    AboutLink("QQ Group: 435539500", "435539500", true);
                    AboutLink("Website: http://teargamestorem.top/", "http://teargamestorem.top/", false);
                    AboutLink("GitHub: https://github.com/tearhacker/Ra2Game_tear", "https://github.com/tearhacker/Ra2Game_tear", false);
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
