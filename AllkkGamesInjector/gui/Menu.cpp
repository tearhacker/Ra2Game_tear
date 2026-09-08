#include "pch.h"
#include "Menu.hpp"

#include <algorithm>
#include <random>

namespace
{
	std::wstring makeRandomWindowTitle()
	{
		static constexpr wchar_t alphabet[] =
			L"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
		static std::random_device randomDevice;
		static std::mt19937 generator(randomDevice());
		static std::uniform_int_distribution<std::size_t> distribution(0, std::size(alphabet) - 2);

		std::wstring title;
		title.reserve(16);
		for (std::size_t i = 0; i < 16; ++i)
			title.push_back(alphabet[distribution(generator)]);

		return title;
	}
}

#include "dependency/imgui/imgui.h"
#include "dependency/imgui/imgui_internal.h"
#include "dependency/imgui/backend/imgui_impl_dx9.h"
#include "dependency/imgui/backend/imgui_impl_win32.h"

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace
{
	void applyRoundedWindowRegion(HWND hWnd)
	{
		RECT clientRect{};
		if (!GetClientRect(hWnd, &clientRect))
			return;

		const int width = clientRect.right - clientRect.left;
		const int height = clientRect.bottom - clientRect.top;
		if (width <= 0 || height <= 0)
			return;

		const int radius = std::min(30, std::min(width, height) / 8);
		HRGN roundedRegion = CreateRoundRectRgn(0, 0, width + 1, height + 1, radius * 2, radius * 2);
		if (roundedRegion != nullptr && !SetWindowRgn(hWnd, roundedRegion, TRUE))
			DeleteObject(roundedRegion);
	}

	ImVec4 canvasColor(bool darkTheme)
	{
		return darkTheme
			? ImVec4(0.035f, 0.047f, 0.078f, 1.0f)
			: ImVec4(0.955f, 0.970f, 0.990f, 1.0f);
	}

	ImVec4 outerCanvasColor(bool darkTheme)
	{
		return darkTheme
			? ImVec4(0.075f, 0.095f, 0.140f, 1.0f)
			: ImVec4(0.875f, 0.910f, 0.960f, 1.0f);
	}

	// 转为 ImGui 所需的 UTF-8；已加载中文字体，中文可正常显示
	std::string toDisplayString(const std::wstring& text)
	{
		if (text.empty())
			return {};

		const int size = ::WideCharToMultiByte(CP_UTF8, 0, text.c_str(),
			static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
		if (size <= 0)
			return {};

		std::string result(static_cast<std::size_t>(size), '\0');
		::WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()),
			result.data(), size, nullptr, nullptr);
		return result;
	}

	// 按系统默认 UI 语言选择初始语言：简/繁中文 → 中文，其他 → 英文
	bool detectSystemChinese()
	{
		const LANGID language = ::GetUserDefaultUILanguage();
		return PRIMARYLANGID(language) == LANG_CHINESE;
	}

	// 加载支持中文的系统字体；失败时退回 ImGui 默认字体（中文将显示为 '?'）
	void setupFonts()
	{
		ImGuiIO& io = ImGui::GetIO();
		static const ImWchar* glyphRanges = io.Fonts->GetGlyphRangesChineseSimplifiedCommon();
		static constexpr const char* fontCandidates[] = {
			"C:\\Windows\\Fonts\\msyh.ttc",   // 微软雅黑
			"C:\\Windows\\Fonts\\simhei.ttf", // 黑体
			"C:\\Windows\\Fonts\\simsun.ttc", // 宋体
		};

		ImFontConfig config;
		config.OversampleH = 2;
		config.OversampleV = 1;
		for (const char* fontPath : fontCandidates)
		{
			if (io.Fonts->AddFontFromFileTTF(fontPath, 16.0f, &config, glyphRanges) != nullptr)
				return;
		}
		io.Fonts->AddFontDefault();
	}
}

bool Menu::initialize()
{
	WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, Menu::WndProc, 0L, 0L, GetModuleHandle(NULL), NULL, NULL, NULL, NULL, L"WC", NULL };
	::RegisterClassEx(&wc);
	const auto nativeWindowTitle = makeRandomWindowTitle();
	this->hwnd = ::CreateWindow(wc.lpszClassName, nativeWindowTitle.c_str(),
		WS_POPUP,
		100, 100, 500, 640, NULL, NULL, wc.hInstance, NULL);
	if (this->hwnd == nullptr)
	{
		::UnregisterClass(wc.lpszClassName, wc.hInstance);
		return false;
	}
	applyRoundedWindowRegion(this->hwnd);

	if (!createD3D9Device(hwnd))
	{
		cleanupD3D9Device();
		::UnregisterClass(wc.lpszClassName, wc.hInstance);
		return false;
	}

	::ShowWindow(hwnd, SW_SHOWDEFAULT);
	::UpdateWindow(hwnd);

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	io.WantSaveIniSettings = false;

	setupFonts();
	setupMenuStyle(true, 1);

	ImGui_ImplWin32_Init(hwnd);
	ImGui_ImplDX9_Init(this->d3dDevice);

	this->isChineseLang = detectSystemChinese();

	g_injector->setTargetProcessName(vars::gameProfiles[0].processName);
	this->isMenuOn = true;
	std::thread(&Menu::detectGame, this).detach();
	std::thread(&Menu::updateFiles, this).detach();

	return true;
}

void Menu::loop()
{
	while (this->isMenuOn)
	{
		MSG msg;
		while (::PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE))
		{
			::TranslateMessage(&msg);
			::DispatchMessage(&msg);
			if (msg.message == WM_QUIT)
				this->isMenuOn = false;
		}
		if (!this->isMenuOn)
			break;

		ImGui_ImplDX9_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

		const ImVec2 displaySize = ImGui::GetIO().DisplaySize;
		constexpr float outerMargin = 6.0f;
		ImGui::SetNextWindowPos(ImVec2(outerMargin, outerMargin), ImGuiCond_Always);
		ImGui::SetNextWindowSize(
			ImVec2(std::max(0.0f, displaySize.x - outerMargin * 2.0f),
				std::max(0.0f, displaySize.y - outerMargin * 2.0f)),
			ImGuiCond_Always);
		ImGui::Begin("Potato Injector", nullptr,
			ImGuiWindowFlags_NoTitleBar |
			ImGuiWindowFlags_NoMove |
			ImGuiWindowFlags_NoCollapse |
			ImGuiWindowFlags_NoResize |
			ImGuiWindowFlags_NoSavedSettings |
			ImGuiWindowFlags_NoScrollbar);
		ImGui::BeginChild("Hero", ImVec2(0, 86), true);
		ImGui::BeginGroup();
		ImGui::TextColored(isDarkTheme ? ImVec4(0.42f, 0.80f, 1.00f, 1.00f) : ImVec4(0.08f, 0.40f, 0.78f, 1.00f), "TEARHACKER INJECTOR");
		ImGui::TextDisabled("%s", langText("A clean workspace for your selected module", "为你所选模块提供的简洁工作区"));
		ImGui::TextDisabled("%s", langText(
			isDarkTheme ? "Night theme  -  live monitoring enabled" : "Day theme  -  live monitoring enabled",
			isDarkTheme ? "夜间主题 - 实时监控已开启" : "日间主题 - 实时监控已开启"));
		ImGui::EndGroup();
		ImGui::EndChild();
		ImGui::Spacing();

		// 独立工具条组件区：语言切换、主题切换、退出程序三个按钮，不与上方文字挤在一行
		ImGui::BeginChild("Toolbar", ImVec2(0, 50), true);
		if (ImGui::Button(isChineseLang ? "EN" : "中文", ImVec2(72.0f, 32.0f)))
			isChineseLang = !isChineseLang;
		ImGui::SameLine(0.0f, 8.0f);
		if (ImGui::Button(isDarkTheme ? langText("Day mode", "日间模式") : langText("Night mode", "夜间模式"), ImVec2(96.0f, 32.0f)))
		{
			isDarkTheme = !isDarkTheme;
			setupMenuStyle(isDarkTheme, 1.0f);
		}
		ImGui::SameLine(0.0f, 8.0f);
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.72f, 0.26f, 0.29f, 0.85f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.86f, 0.32f, 0.35f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.60f, 0.20f, 0.23f, 1.0f));
		if (ImGui::Button(langText("Exit program", "退出程序"), ImVec2(96.0f, 32.0f)))
			::PostMessage(hwnd, WM_CLOSE, 0, 0);
		ImGui::PopStyleColor(3);
		ImGui::EndChild();
		ImGui::Spacing();

		renderStatusPanel();
		renderTargetPanel();
		const auto paths = snapshotDllPaths();
		renderInjectionPanel(paths);

		ImGui::End();

		ImGui::EndFrame();

		this->d3dDevice->SetRenderState(D3DRS_ZENABLE, FALSE);
		this->d3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
		this->d3dDevice->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
		const ImVec4 clearColor = outerCanvasColor(this->isDarkTheme);
		D3DCOLOR clear_col_dx = D3DCOLOR_RGBA((int)(clearColor.x * 255.0f), (int)(clearColor.y * 255.0f), (int)(clearColor.z * 255.0f), 255);
		this->d3dDevice->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, clear_col_dx, 1.0f, 0);
		if (this->d3dDevice->BeginScene() >= 0)
		{
			ImGui::Render();
			ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
			this->d3dDevice->EndScene();
		}
		this->d3dDevice->Present(NULL, NULL, NULL, NULL);
	}
}

bool Menu::createD3D9Device(HWND hWnd)
{
	if ((this->pD3D = Direct3DCreate9(D3D_SDK_VERSION)) == NULL) return false;

	ZeroMemory(&this->d3dpp, sizeof(this->d3dpp));
	this->d3dpp.Windowed = TRUE;
	this->d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
	this->d3dpp.BackBufferFormat = D3DFMT_UNKNOWN;
	this->d3dpp.EnableAutoDepthStencil = TRUE;
	this->d3dpp.AutoDepthStencilFormat = D3DFMT_D16;
	this->d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_ONE;
	this->d3dpp.hDeviceWindow = hWnd;
	const auto result = this->pD3D->CreateDevice(
		D3DADAPTER_DEFAULT,
		D3DDEVTYPE_HAL,
		hwnd,
		D3DCREATE_HARDWARE_VERTEXPROCESSING,
		&this->d3dpp, &this->d3dDevice);
	if (result != S_OK) return false;

	return true;
}

void Menu::cleanupD3D9Device()
{
	if (this->d3dDevice != nullptr)
	{
		this->d3dDevice->Release();
		this->d3dDevice = nullptr;
	}

	if (this->pD3D != nullptr)
	{
		this->pD3D->Release();
		this->pD3D = nullptr;
	}
}

LRESULT __stdcall Menu::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
		return true;

	switch (msg)
	{
	case WM_NCHITTEST:
	{
		POINT cursor{
			static_cast<LONG>(static_cast<short>(LOWORD(lParam))),
			static_cast<LONG>(static_cast<short>(HIWORD(lParam))) };
		::ScreenToClient(hWnd, &cursor);
		// Hero 区为纯文字组件，可整块拖动窗口；按钮已移至下方独立工具条
		if (cursor.y >= 0 && cursor.y < 86)
			return HTCAPTION;
		break;
	}
	case WM_SIZE:
		applyRoundedWindowRegion(hWnd);
		break;
	case WM_SYSCOMMAND:
		if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
			return 0;
		break;
	case WM_DESTROY:
		::PostQuitMessage(0);
		return 0;
	}
	return ::DefWindowProc(hWnd, msg, wParam, lParam);
}

const char* Menu::langText(const char* en, const char* zh) const
{
	return this->isChineseLang ? zh : en;
}

void Menu::renderStatusPanel()
{
	ImGui::BeginChild("StatusPanel", ImVec2(0, 74), true);
	ImGui::TextDisabled("%s", langText("SYSTEM STATUS", "系统状态"));
	ImGui::Separator();

	const auto running = g_injector->targetRunning.load(std::memory_order_acquire);
	const auto statusColor = running ? ImVec4(0.35f, 0.85f, 0.55f, 1.0f) : ImVec4(0.92f, 0.38f, 0.42f, 1.0f);
	ImGui::TextColored(statusColor, ">>");
	ImGui::SameLine();
	ImGui::TextUnformatted(langText("Target process", "目标进程"));
	ImGui::SameLine(178.0f);
	ImGui::TextColored(statusColor, "%s", running ? langText("RUNNING", "运行中") : langText("OFFLINE", "未运行"));
	ImGui::EndChild();
}

void Menu::renderTargetPanel()
{
	ImGui::BeginChild("TargetPanel", ImVec2(0, 150), true);
		ImGui::TextDisabled("%s", langText("TARGET PROCESS", "目标进程"));
		ImGui::Separator();
		ImGui::Checkbox(langText("Auto-close after operation", "操作完成后自动退出"), &g_injector->shouldAutoExit);

		const auto& activeProfile = vars::gameProfiles[std::clamp(this->selectedGame, 0,
			static_cast<int>(std::size(vars::gameProfiles)) - 1)];
		const auto currentGameName = toDisplayString(
			this->isChineseLang ? activeProfile.displayNameZh : activeProfile.displayName);
		if (ImGui::BeginCombo(langText("Preset game", "预设游戏"), currentGameName.c_str()))
		{
			for (int index = 0; index < static_cast<int>(std::size(vars::gameProfiles)); ++index)
			{
				const auto& profile = vars::gameProfiles[index];
				const auto label = toDisplayString(
					this->isChineseLang ? profile.displayNameZh : profile.displayName);
				if (ImGui::Selectable(label.c_str(), index == this->selectedGame))
				{
					this->selectedGame = index;
					g_injector->setTargetProcessName(profile.processName);
				}
			}
			ImGui::EndCombo();
		}

		const auto processSnapshot = snapshotProcessNames();
		if (!processSnapshot.empty())
		{
			std::string processItems;
			std::vector<std::string> displayNames;
			displayNames.reserve(processSnapshot.size());
			for (const auto& name : processSnapshot)
			{
				displayNames.push_back(toDisplayString(name));
				processItems += displayNames.back();
				processItems.push_back('\0');
			}
			processItems.push_back('\0');

			this->selectedProcess = std::clamp(this->selectedProcess, 0, static_cast<int>(processSnapshot.size()) - 1);
			if (ImGui::Combo(langText("Running process", "运行中的进程"), &this->selectedProcess, processItems.c_str()))
				g_injector->setTargetProcessName(processSnapshot[this->selectedProcess]);
		}
		else
		{
			ImGui::TextDisabled("%s", langText("No running processes found", "未发现运行中的进程"));
		}

		ImGui::TextDisabled("%s: %s", langText("Target", "当前目标"), toDisplayString(g_injector->getTargetProcessName()).c_str());
	ImGui::EndChild();
}

std::vector<std::wstring> Menu::snapshotDllPaths()
{
	std::scoped_lock lock(this->mtx);
	return this->filePaths;
}

std::vector<std::wstring> Menu::snapshotProcessNames()
{
	std::scoped_lock lock(this->mtx);
	return this->processNames;
}

void Menu::renderInjectionPanel(const std::vector<std::wstring>& paths)
{
	ImGui::BeginChild("ModulePanel", ImVec2(0, 0), true);
		ImGui::TextDisabled("%s", langText("MODULE WORKSPACE", "模块工作区"));
		ImGui::Separator();
		if (paths.empty())
		{
			ImGui::TextDisabled("%s", langText("No DLL files found in ./dlls", "在 ./dlls 中未找到 DLL 文件"));
			ImGui::Spacing();
			ImGui::TextWrapped("%s", langText(
				"Place a DLL in the dlls folder to make it available here.",
				"将 DLL 文件放入 dlls 文件夹即可在此处选用。"));
		}
		else
		{
			std::string dllItems;
			std::vector<std::string> displayNames;
			displayNames.reserve(paths.size());
			for (const auto& path : paths)
			{
				const auto displayName = toDisplayString(path);
				const auto separator = displayName.find_last_of("\\/");
				displayNames.push_back(displayName.substr(separator == std::string::npos ? 0 : separator + 1));
				dllItems += displayNames.back();
				dllItems.push_back('\0');
			}
			dllItems.push_back('\0');

			this->selectedDLL = std::clamp(this->selectedDLL, 0, static_cast<int>(paths.size()) - 1);
			ImGui::Combo("DLL", &this->selectedDLL, dllItems.c_str());
		}

		ImGui::Spacing();
		const bool canInject = !paths.empty() && !this->isInjecting;
		if (!canInject)
			ImGui::BeginDisabled();
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.52f, 0.92f, canInject ? 1.0f : 0.35f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.28f, 0.63f, 1.0f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.12f, 0.40f, 0.82f, 1.0f));
		if (ImGui::Button(isInjecting ? langText("Injecting...", "注入中...")
			: langText("Inject selected module", "注入所选模块"), ImVec2(-1.0f, 42.0f)) && canInject)
			std::thread(&Injector::inject, g_injector.get(), paths[this->selectedDLL]).detach();
		if (!canInject)
			ImGui::EndDisabled();
		ImGui::PopStyleColor(3);

		if (this->isInjecting)
			ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.25f, 1.0f), "%s", langText("Injecting module...", "正在注入模块..."));
		else
		{
			const auto [succeeded, error] = g_injector->snapshotResult();
			if (succeeded)
				ImGui::TextColored(ImVec4(0.35f, 0.85f, 0.55f, 1.0f), "%s", langText("Injection completed", "注入完成"));
			else if (!error.empty())
				ImGui::TextWrapped("%s", toDisplayString(error).c_str());
		}
	ImGui::EndChild();
}

void Menu::setupMenuStyle(bool isDarkTheme, float alpha)
{
	ImGuiStyle& style = ImGui::GetStyle();

	style = ImGuiStyle();
	style.Alpha = alpha;
	style.WindowPadding = ImVec2(10.0f, 10.0f);
	style.FramePadding = ImVec2(10.0f, 8.0f);
	style.ItemSpacing = ImVec2(10.0f, 10.0f);
	style.ItemInnerSpacing = ImVec2(7.0f, 6.0f);
	style.WindowRounding = 18.0f;
	style.ChildRounding = 12.0f;
	style.FrameRounding = 9.0f;
	style.PopupRounding = 10.0f;
	style.ScrollbarRounding = 10.0f;
	style.GrabRounding = 9.0f;
	style.TabRounding = 9.0f;
	style.WindowBorderSize = 0.0f;
	style.ChildBorderSize = 1.0f;
	style.FrameBorderSize = 0.0f;
	style.WindowTitleAlign = ImVec2(0.08f, 0.5f);

	const ImVec4 accent = isDarkTheme ? ImVec4(0.30f, 0.58f, 1.00f, 1.0f) : ImVec4(0.12f, 0.38f, 0.82f, 1.0f);
	const ImVec4 accentHover = isDarkTheme ? ImVec4(0.40f, 0.66f, 1.00f, 1.0f) : ImVec4(0.18f, 0.47f, 0.94f, 1.0f);
	const ImVec4 canvas = canvasColor(isDarkTheme);
	const ImVec4 panel = isDarkTheme ? ImVec4(0.070f, 0.090f, 0.140f, 1.0f) : ImVec4(0.985f, 0.990f, 0.998f, 1.0f);
	const ImVec4 frame = isDarkTheme ? ImVec4(0.105f, 0.135f, 0.205f, 1.0f) : ImVec4(0.900f, 0.935f, 0.975f, 1.0f);

	style.Colors[ImGuiCol_Text] = isDarkTheme ? ImVec4(0.91f, 0.94f, 0.99f, 1.0f) : ImVec4(0.10f, 0.13f, 0.19f, 1.0f);
	style.Colors[ImGuiCol_TextDisabled] = isDarkTheme ? ImVec4(0.54f, 0.60f, 0.70f, 1.0f) : ImVec4(0.42f, 0.48f, 0.57f, 1.0f);
	style.Colors[ImGuiCol_WindowBg] = canvas;
	style.Colors[ImGuiCol_ChildBg] = panel;
	style.Colors[ImGuiCol_PopupBg] = panel;
	style.Colors[ImGuiCol_Border] = isDarkTheme ? ImVec4(0.20f, 0.30f, 0.46f, 0.72f) : ImVec4(0.67f, 0.76f, 0.88f, 0.85f);
	style.Colors[ImGuiCol_BorderShadow] = ImVec4(0, 0, 0, 0);
	style.Colors[ImGuiCol_FrameBg] = frame;
	style.Colors[ImGuiCol_FrameBgHovered] = isDarkTheme ? ImVec4(0.15f, 0.21f, 0.32f, 1.0f) : ImVec4(0.83f, 0.89f, 0.97f, 1.0f);
	style.Colors[ImGuiCol_FrameBgActive] = isDarkTheme ? ImVec4(0.18f, 0.27f, 0.42f, 1.0f) : ImVec4(0.76f, 0.85f, 0.96f, 1.0f);
	style.Colors[ImGuiCol_TitleBg] = style.Colors[ImGuiCol_WindowBg];
	style.Colors[ImGuiCol_TitleBgActive] = style.Colors[ImGuiCol_WindowBg];
	style.Colors[ImGuiCol_MenuBarBg] = panel;
	style.Colors[ImGuiCol_ScrollbarBg] = style.Colors[ImGuiCol_WindowBg];
	style.Colors[ImGuiCol_ScrollbarGrab] = isDarkTheme ? ImVec4(0.22f, 0.34f, 0.48f, 1.0f) : ImVec4(0.64f, 0.73f, 0.84f, 1.0f);
	style.Colors[ImGuiCol_ScrollbarGrabHovered] = accent;
	style.Colors[ImGuiCol_ScrollbarGrabActive] = accentHover;
	style.Colors[ImGuiCol_CheckMark] = accentHover;
	style.Colors[ImGuiCol_SliderGrab] = accent;
	style.Colors[ImGuiCol_SliderGrabActive] = accentHover;
	style.Colors[ImGuiCol_Button] = ImVec4(accent.x, accent.y, accent.z, 0.25f);
	style.Colors[ImGuiCol_ButtonHovered] = ImVec4(accentHover.x, accentHover.y, accentHover.z, 0.85f);
	style.Colors[ImGuiCol_ButtonActive] = accent;
	style.Colors[ImGuiCol_Header] = ImVec4(accent.x, accent.y, accent.z, 0.22f);
	style.Colors[ImGuiCol_HeaderHovered] = ImVec4(accentHover.x, accentHover.y, accentHover.z, 0.45f);
	style.Colors[ImGuiCol_HeaderActive] = ImVec4(accent.x, accent.y, accent.z, 0.65f);
	style.Colors[ImGuiCol_Separator] = style.Colors[ImGuiCol_Border];
	style.Colors[ImGuiCol_SeparatorHovered] = accentHover;
	style.Colors[ImGuiCol_SeparatorActive] = accent;
	style.Colors[ImGuiCol_NavHighlight] = accent;
	style.Colors[ImGuiCol_TextSelectedBg] = ImVec4(accent.x, accent.y, accent.z, 0.35f);
}

void Menu::detectGame()
{
	while (this->isMenuOn)
	{
		// 单次进程快照同时完成"目标是否在运行"与"进程下拉列表"两项任务
		const auto procList = mem::getProcList();
		const auto targetName = string::toLower(g_injector->getTargetProcessName());

		bool running = false;
		{
			std::scoped_lock lock(this->mtx);
			this->processNames.clear();
			this->processNames.reserve(procList.size());
			for (const auto& process : procList)
			{
				this->processNames.push_back(process.second);
				if (string::toLower(process.second) == targetName)
					running = true;
			}
		}

		g_injector->targetRunning.store(running, std::memory_order_release);
		std::this_thread::sleep_for(1s);
	}
}

void Menu::updateFiles()
{
	std::error_code error;
	if (!std::filesystem::is_directory(vars::str_dll_dir_path, error) || !std::filesystem::exists(vars::str_dll_dir_path, error))
		std::filesystem::create_directory(vars::str_dll_dir_path, error);

	while (this->isMenuOn)
	{
		try
		{
			// 先在锁外构建列表，再整体替换，缩短持锁时间
			std::vector<std::wstring> paths;
			for (const auto& file : std::filesystem::directory_iterator(vars::str_dll_dir_path))
			{
				const auto extension = file.path().extension().wstring();
				if (file.is_regular_file() && _wcsicmp(extension.c_str(), L".dll") == 0)
					paths.push_back(std::filesystem::absolute(file.path()).wstring());
			}

			std::scoped_lock lock(this->mtx);
			this->filePaths = std::move(paths);
		}
		catch (const std::exception&)
		{
			// 目录被删除等异常情况下保留现有列表，下一轮再重试
		}
		std::this_thread::sleep_for(1s);
	}
}
