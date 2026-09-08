#pragma once
#include <string>

namespace vars
{
	struct GameProfile
	{
		const wchar_t* displayName;    // 英文名
		const wchar_t* displayNameZh;  // 中文名
		const wchar_t* processName;
		const wchar_t* windowClass;    // 游戏主窗口类名（Spy++ 可查，要求完全匹配）
		const wchar_t* windowTitle;    // 游戏主窗口标题（要求包含匹配）
	};

	// 定位策略：优先按窗口类名/标题找到游戏主窗口所属进程（比进程名可靠，
	// sploader 壳/僵尸实例不拥有窗口，exe 改名也不影响），进程名仅作兜底。
	// 注意 KK 启动器版窗口类名为 "Yuris Revenge"（无撇号），与原版不同。
	inline constexpr GameProfile gameProfiles[] = {
		{ L"Red Alert 2 - Yuri's Revenge",             L"红警2 - 尤里复仇",          L"gamemd.exe",       L"Yuri's Revenge", L"Yuri's Revenge" },
		{ L"Red Alert 2 - Yuri's Revenge (Launcher)",  L"红警2 - 尤里复仇（启动器版）", L"gamemd-spawn.exe", L"Yuris Revenge",  L"Yuris Revenge" },
		{ L"Red Alert 2",                              L"红警2",                    L"ra2.exe",          L"Red Alert 2",    L"Red Alert 2" },
		{ L"Warcraft III (Classic)",                   L"魔兽争霸3（经典版）",        L"war3.exe",         L"Warcraft III",   L"Warcraft III" },
		{ L"Counter-Strike 1.6",                       L"反恐精英 1.6",             L"hl.exe",           L"Valve001",       L"Counter-Strike" },
	};

	inline std::wstring_view str_dll_dir_path{ L"./dlls" };
}
