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
	//
	// ⚠ 窗口类名必填且要求完全匹配（忽略大小写）。不同启动渠道的类名不同：
	//    - 原版 / 多数整合版: "Yuri's Revenge"（带撇号）
	//    - KK 平台 / 部分启动器: "Yuris Revenge"（无撇号）
	//    - 部分汉化/重打包版本类名仍是 "Yuri's Revenge" 但标题被改写
	//   如果预设都对不上，请用 Spy++ 或 PowerShell 查出实际类名后在此补充。
	//
	// 进程名兜底要求该进程拥有可见顶层窗口；找不到则注入会等待 30 秒后失败。
	// 失败时注入器会把检测到的疑似进程名与窗口类名打进日志，照日志补预设即可。
	inline constexpr GameProfile gameProfiles[] = {
		{ L"Red Alert 2 - Yuri's Revenge",             L"红警2 - 尤里复仇",          L"gamemd.exe",       L"Yuri's Revenge", L"Yuri" },
		{ L"Red Alert 2 - Yuri's Revenge (Launcher)",  L"红警2 - 尤里复仇（启动器版/KK平台）", L"gamemd-spawn.exe", L"Yuris Revenge",  L"Yuri" },
		{ L"Red Alert 2",                              L"红警2",                    L"ra2.exe",          L"Red Alert 2",    L"Red Alert 2" },
		{ L"Warcraft III (Classic)",                   L"魔兽争霸3（经典版）",        L"war3.exe",         L"Warcraft III",   L"Warcraft III" },
		{ L"Counter-Strike 1.6",                       L"反恐精英 1.6",             L"hl.exe",           L"Valve001",       L"Counter-Strike" },
	};

	inline std::wstring_view str_dll_dir_path{ L"./dlls" };
}
