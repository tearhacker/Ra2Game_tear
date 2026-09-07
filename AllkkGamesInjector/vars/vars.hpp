#pragma once
#include <string>

namespace vars
{
	struct GameProfile
	{
		const wchar_t* displayName;
		const wchar_t* processName;
	};

	// 预设的 x86 游戏目标；若本机进程名不同，可在界面中改选"运行中的进程"
	inline constexpr GameProfile gameProfiles[] = {
		{ L"Red Alert 2 - Yuri's Revenge", L"gamemd.exe" },
		{ L"Red Alert 2",                  L"ra2.exe" },
		{ L"Warcraft III (Classic)",       L"war3.exe" },
		{ L"Counter-Strike 1.6",           L"hl.exe" },
	};

	inline std::wstring_view str_dll_dir_path{ L"./dlls" };
}
