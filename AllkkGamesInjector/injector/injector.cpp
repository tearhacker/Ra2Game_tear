#include "pch.h"
#include "injector.hpp"

namespace
{
	constexpr auto kProcessWaitTimeout = 30s;
	constexpr auto kLoadWaitTimeout = 10s;
	constexpr auto kPollInterval = 250ms;

	// LoadLibraryW 注入所需的最小权限集合，不使用 ALL_ACCESS 以提高兼容性
	constexpr DWORD kProcessAccess =
		PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION |
		PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ;

	bool rangeWithin(std::size_t offset, std::size_t length, std::size_t total)
	{
		return offset <= total && length <= total - offset;
	}

	// 目标均为 x86 进程，DLL 必须是 Win32 镜像，这里做轻量 PE 头校验
	bool isX86Image(const std::vector<BYTE>& buffer, std::wstring& reason)
	{
		if (!rangeWithin(0, sizeof(IMAGE_DOS_HEADER), buffer.size()))
		{
			reason = L"file is smaller than IMAGE_DOS_HEADER";
			return false;
		}

		const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(buffer.data());
		if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0)
		{
			reason = L"invalid DOS header";
			return false;
		}

		const auto ntOffset = static_cast<std::size_t>(dos->e_lfanew);
		if (!rangeWithin(ntOffset, sizeof(IMAGE_NT_HEADERS32), buffer.size()))
		{
			reason = L"NT header is outside the file";
			return false;
		}

		const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS32*>(buffer.data() + ntOffset);
		if (nt->Signature != IMAGE_NT_SIGNATURE)
		{
			reason = L"invalid NT signature";
			return false;
		}

		if (nt->FileHeader.Machine != IMAGE_FILE_MACHINE_I386)
		{
			reason = L"image is not a Win32 (x86) PE";
			return false;
		}

		return true;
	}

	void logEvent(std::wstring_view event, std::wstring_view detail)
	{
		std::wostringstream stream;
		stream << L"[injector] event=" << event << L" detail=" << detail << L'\n';
		OutputDebugStringW(stream.str().c_str());
	}
}

std::wstring Injector::getTargetProcessName() const
{
	std::scoped_lock lock(m_targetMutex);
	return m_targetProcessName;
}

void Injector::setTargetProcessName(const std::wstring& name)
{
	std::scoped_lock lock(m_targetMutex);
	m_targetProcessName = name;
}

std::pair<bool, std::wstring> Injector::snapshotResult() const
{
	std::scoped_lock lock(m_resultMutex);
	return { m_lastSucceeded, m_lastError };
}

bool Injector::fail(const std::wstring& message)
{
	{
		std::scoped_lock lock(m_resultMutex);
		m_lastError = message;
		m_lastSucceeded = false;
	}
	logEvent(L"inject-failure", message);
	finish(false);
	return false;
}

void Injector::finish(bool ok)
{
	{
		std::scoped_lock lock(m_resultMutex);
		m_lastSucceeded = ok;
	}
	if (ok && shouldAutoExit)
		g_menu->isMenuOn.store(false, std::memory_order_release);
	// 必须在所有共享状态写入完成后复位，isInjecting 即为完成信号
	g_menu->isInjecting.store(false, std::memory_order_release);
}

bool Injector::inject(std::wstring dllPath)
{
	g_menu->isInjecting.store(true, std::memory_order_release);
	{
		std::scoped_lock lock(m_resultMutex);
		m_lastError.clear();
		m_lastSucceeded = false;
	}

	const auto processName = getTargetProcessName();
	const auto dllAbsolutePath = std::filesystem::absolute(dllPath).wstring();

	// 注入前先在本进程校验 PE 头，尽早发现架构不匹配等问题
	std::vector<BYTE> buffer;
	if (!utils::readFileToMem(dllAbsolutePath, buffer) || buffer.empty())
		return fail(L"Failed to read DLL file: " + dllAbsolutePath);

	std::wstring reason;
	if (!isX86Image(buffer, reason))
		return fail(L"Not a valid Win32 (x86) DLL: " + reason);

	const auto pid = waitForProcess(processName);
	if (pid == 0)
		return fail(L"Target process not found within 30s: " + processName);

	const auto processHandle = OpenProcess(kProcessAccess, FALSE, pid);
	if (!processHandle)
		return fail(L"OpenProcess failed (error " + std::to_wstring(GetLastError())
			+ L"). If the game runs as administrator, run the injector as administrator too.");

	const auto error = injectByLoadLibrary(processHandle, dllAbsolutePath);
	CloseHandle(processHandle);
	if (!error.empty())
		return fail(error);

	logEvent(L"inject-success", L"module loaded into " + processName);
	finish(true);
	return true;
}

DWORD Injector::waitForProcess(const std::wstring& processName)
{
	const auto deadline = std::chrono::steady_clock::now() + kProcessWaitTimeout;
	while (std::chrono::steady_clock::now() < deadline)
	{
		if (const auto pid = mem::getProcID(processName); pid != 0)
			return pid;
		std::this_thread::sleep_for(kPollInterval);
	}
	return 0;
}

std::wstring Injector::injectByLoadLibrary(HANDLE processHandle, const std::wstring& dllPath)
{
	const auto pathBytes = (dllPath.size() + 1) * sizeof(wchar_t);
	const auto remoteMemory = VirtualAllocEx(
		processHandle, nullptr, pathBytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	if (!remoteMemory)
		return L"VirtualAllocEx failed (error " + std::to_wstring(GetLastError()) + L")";

	if (!WriteProcessMemory(processHandle, remoteMemory, dllPath.c_str(), pathBytes, nullptr))
	{
		const auto message = L"WriteProcessMemory failed (error " + std::to_wstring(GetLastError()) + L")";
		VirtualFreeEx(processHandle, remoteMemory, 0, MEM_RELEASE);
		return message;
	}

	const auto loadLibraryW = reinterpret_cast<LPTHREAD_START_ROUTINE>(
		GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "LoadLibraryW"));
	if (!loadLibraryW)
	{
		VirtualFreeEx(processHandle, remoteMemory, 0, MEM_RELEASE);
		return L"kernel32!LoadLibraryW was not found";
	}

	const auto remoteThread = CreateRemoteThread(
		processHandle, nullptr, 0, loadLibraryW, remoteMemory, 0, nullptr);
	if (!remoteThread)
	{
		const auto message = L"CreateRemoteThread failed (error " + std::to_wstring(GetLastError()) + L")";
		VirtualFreeEx(processHandle, remoteMemory, 0, MEM_RELEASE);
		return message;
	}

	const auto waitResult = WaitForSingleObject(
		remoteThread, static_cast<DWORD>(std::chrono::milliseconds(kLoadWaitTimeout).count()));
	DWORD remoteModule = 0;
	if (waitResult == WAIT_OBJECT_0)
	{
		GetExitCodeThread(remoteThread, &remoteModule);
		// LoadLibraryW 已返回，路径缓冲区可以回收
		VirtualFreeEx(processHandle, remoteMemory, 0, MEM_RELEASE);
	}
	// 超时时不释放缓冲区：远程线程可能仍持有指针，泄漏少量内存比崩溃安全
	CloseHandle(remoteThread);

	if (waitResult != WAIT_OBJECT_0)
		return L"LoadLibraryW did not return within 10s (DllMain may be deadlocked)";

	if (remoteModule == 0)
		return L"LoadLibraryW failed inside the target process (check the DLL's own log)";

	return {};
}
