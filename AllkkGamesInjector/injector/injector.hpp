#pragma once

#include <atomic>
#include <mutex>
#include <string>
#include <utility>

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

	// 返回空串表示成功，否则返回错误描述
	std::wstring injectByLoadLibrary(HANDLE processHandle, const std::wstring& dllPath);

	mutable std::mutex m_targetMutex;
	std::wstring m_targetProcessName{ L"gamemd.exe" };

	mutable std::mutex m_resultMutex;
	bool m_lastSucceeded{ false };
	std::wstring m_lastError;
};

inline auto g_injector = std::make_unique<Injector>();
