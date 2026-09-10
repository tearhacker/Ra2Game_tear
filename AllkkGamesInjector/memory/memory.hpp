#pragma once
#include "utils/utils.hpp"

#include <set>
#include <tlhelp32.h>

namespace mem
{
	struct CompareProc {
		bool operator()(const std::pair<std::uint32_t, std::wstring>& lhs, const std::pair<std::uint32_t, std::wstring>& rhs) const {
			return lhs.second < rhs.second;
		}
	};

	inline bool isSystemProcess(const std::wstring& name) {
		static const std::set<std::wstring> systemProcesses = {
			L"System", L"svchost.exe", L"csrss.exe", L"smss.exe", L"wininit.exe", L"services.exe"
		};
		return systemProcesses.find(name) != systemProcesses.end();
	}

	inline std::set<std::pair<std::uint32_t, std::wstring>, CompareProc> getProcList() {
		std::set<std::pair<std::uint32_t, std::wstring>, CompareProc> procList;

		const auto hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, NULL);
		if (hSnap == INVALID_HANDLE_VALUE) return procList;

		PROCESSENTRY32 e;
		e.dwSize = sizeof(e);

		if (!Process32First(hSnap, &e)) {
			CloseHandle(hSnap);
			return procList;
		}

		do {
			if (!isSystemProcess(e.szExeFile)) {
				procList.insert(std::make_pair(e.th32ProcessID, e.szExeFile));
			}
		} while (Process32Next(hSnap, &e));

		CloseHandle(hSnap);
		return procList;
	}

	inline bool getProcCreationTime(DWORD pid, LONGLONG& outTime) {
		const HANDLE handle = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
		if (!handle) return false;
		FILETIME created{}, exited{}, kernel{}, user{};
		const bool ok = GetProcessTimes(handle, &created, &exited, &kernel, &user) != 0;
		CloseHandle(handle);
		if (!ok) return false;
		outTime = (static_cast<LONGLONG>(created.dwHighDateTime) << 32) | created.dwLowDateTime;
		return true;
	}

	struct WindowPidQuery {
		const std::vector<DWORD>* pids;
		DWORD bestPid = 0;
		LONGLONG bestTime = -1;
	};

	// sploader 加载链会产生多个同名进程（壳派生真身后退出，且可能残留
	// 无窗口旧实例还挂着旧 DLL）。只接受拥有可见顶层窗口的实例
	// （真游戏窗口；壳与僵尸都没有窗口），多个窗口实例取创建时间最新者。
	// 找不到窗口实例返回 0：waitForProcess 会持续轮询，直到游戏窗口出现，
	// 同时天然避开"注进壳进程、壳随后退出"的竞态。
	inline DWORD getProcID(std::wstring_view procname) {
		const auto procList = getProcList();
		if (procList.empty() || procname.empty()) return NULL;

		const auto targetName = string::toLower(std::wstring(procname));

		std::vector<DWORD> matches;
		for (const auto& proc : procList)
		{
			if (string::toLower(proc.second) == targetName)
			{
				matches.push_back(proc.first);
			}
		}
		if (matches.empty()) return NULL;

		WindowPidQuery query{ &matches, 0, -1 };
		EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL {
			auto* q = reinterpret_cast<WindowPidQuery*>(lParam);
			DWORD owner = 0;
			GetWindowThreadProcessId(hwnd, &owner);
			if (owner == 0 || !IsWindowVisible(hwnd)) return TRUE;
			for (const DWORD candidate : *q->pids)
			{
				if (candidate == owner)
				{
					LONGLONG created = 0;
					getProcCreationTime(owner, created);
					if (q->bestPid == 0 || created > q->bestTime)
					{
						q->bestPid = owner;
						q->bestTime = created;
					}
				}
			}
			return TRUE;
			}, reinterpret_cast<LPARAM>(&query));

		return query.bestPid;
	}

	struct WindowTargetQuery {
		const std::wstring* windowClass;
		const std::wstring* windowTitle;
		DWORD pid = 0;
	};

	// Spy++ 式窗口定位：按游戏主窗口的类名/标题直接找到窗口所属进程。
	// 比按进程名查找可靠——壳与僵尸实例不拥有窗口，exe 被改名也不影响。
	// 类名要求完全相等（忽略大小写），标题要求包含（忽略大小写）。
	inline DWORD getProcIDByWindow(std::wstring_view windowClass, std::wstring_view windowTitle) {
		if (windowClass.empty() && windowTitle.empty()) return NULL;

		const auto classLower = string::toLower(std::wstring(windowClass));
		const auto titleLower = string::toLower(std::wstring(windowTitle));
		WindowTargetQuery query{ &classLower, &titleLower, 0 };

		EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL {
			auto* q = reinterpret_cast<WindowTargetQuery*>(lParam);
			if (!IsWindowVisible(hwnd)) return TRUE;

			wchar_t className[256]{};
			GetClassNameW(hwnd, className, 256);
			const bool classMatch = !q->windowClass->empty()
				&& string::toLower(className) == *q->windowClass;

			// 标题匹配独立于类名：类名对不上时仍允许靠标题命中。
			// 不同启动渠道的窗口类名差异很大（"Yuri's Revenge" 与 "Yuris Revenge"
			// 仅差一个撇号），要求两者同时满足会让预设极难命中，因此任一命中即可。
			bool titleMatch = false;
			if (!q->windowTitle->empty())
			{
				wchar_t titleText[256]{};
				GetWindowTextW(hwnd, titleText, 256);
				titleMatch = string::toLower(titleText).find(*q->windowTitle) != std::wstring::npos;
			}

			if (!classMatch && !titleMatch) return TRUE;

			DWORD owner = 0;
			GetWindowThreadProcessId(hwnd, &owner);
			if (owner == 0) return TRUE;
			q->pid = owner;
			return FALSE;  // 命中即停止枚举
			}, reinterpret_cast<LPARAM>(&query));

		return query.pid;
	}

	inline bool openProcess(std::wstring exePath, std::vector<std::wstring> args, PROCESS_INFORMATION& pi) {
		STARTUPINFO si;
		{
			ZeroMemory(&si, sizeof(si));
			si.cb = sizeof(si);
		}

		ZeroMemory(&pi, sizeof(pi));

		std::wstring procCmdLine = exePath;
		for (auto& arg : args) {
			procCmdLine += L" " + arg;
		}

		return CreateProcess(nullptr, procCmdLine.data(), nullptr, nullptr, false, NULL, nullptr,
			nullptr, &si, &pi);
	}
}