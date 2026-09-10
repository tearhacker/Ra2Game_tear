#pragma once

#include <atomic>
#include <mutex>
#include <string>
#include <utility>

// 当前进程是否以管理员提权运行（启动横幅与错误诊断共用）
inline bool isElevated()
{
	HANDLE token = nullptr;
	if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) return false;
	TOKEN_ELEVATION elevation{};
	DWORD returned = 0;
	const BOOL ok = GetTokenInformation(token, TokenElevation, &elevation, sizeof(elevation), &returned);
	CloseHandle(token);
	return ok && elevation.TokenIsElevated != 0;
}

class Injector
{
public:
	Injector() = default;
	~Injector() = default;

	// 注入前等待目标进程出现，随后通过 kernel32!LoadLibraryW 远程线程完成注入
	bool inject(std::wstring dllPath);

	bool shouldAutoExit{ false };

	// 目标进程名（预设游戏下拉或"运行中的进程"下拉写入）
	std::wstring getTargetProcessName() const;
	void setTargetProcessName(const std::wstring& name);

	// 目标游戏主窗口（类名/标题，用于比进程名更可靠的窗口定位）
	std::wstring getTargetWindowClass() const;
	std::wstring getTargetWindowTitle() const;
	void setTargetWindow(std::wstring windowClass, std::wstring windowTitle);

	// 由后台检测线程更新，界面线程读取
	std::atomic_bool targetRunning{ false };

	// 最近一次注入结果快照（供界面线程安全读取）
	std::pair<bool, std::wstring> snapshotResult() const;

private:
	// 失败路径统一入口：记录错误、复位注入状态并返回 false
	bool fail(const std::wstring& message);

	// 写入结果并把 isInjecting 复位为 false（必须最后调用）
	void finish(bool ok);

	DWORD waitForProcess(const std::wstring& processName);

	// 等待失败时生成诊断串：列出疑似红警进程的实际名字/窗口类名，
	// 让用户一眼看出"预设进程名与实际不符"还是"窗口类名不符"。
	std::wstring describeTargetSearch(const std::wstring& processName) const;

	// 返回空串表示成功，否则返回错误描述
	std::wstring injectByLoadLibrary(HANDLE processHandle, const std::wstring& dllPath);

	// 线程劫持注入：挂起目标进程最早的线程（主线程），把它的 EIP 指到一段
	// 自销毁式 shellcode 调用 LoadLibraryW，完成后恢复原始 EIP/寄存器。
	// 不创建远程线程，规避 KK 反作弊 (binkw64.dll) 对陌生线程断点事件的处理崩溃。
	// 返回空串表示成功，否则返回错误描述
	std::wstring injectByThreadHijack(HANDLE processHandle, const std::wstring& dllPath);

	mutable std::mutex m_targetMutex;
	std::wstring m_targetProcessName{ L"gamemd.exe" };
	std::wstring m_targetWindowClass{ L"Yuri's Revenge" };
	std::wstring m_targetWindowTitle{ L"Yuri's Revenge" };

	mutable std::mutex m_resultMutex;
	bool m_lastSucceeded{ false };
	std::wstring m_lastError;
};

inline auto g_injector = std::make_unique<Injector>();
