#include "pch.h"
#include "injector.hpp"

namespace
{
	constexpr auto kProcessWaitTimeout = 30s;
	constexpr auto kLoadWaitTimeout = 10s;
	constexpr auto kPollInterval = 250ms;
	constexpr auto kHijackPollInterval = 50ms;

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

	// 1 = 已提权，0 = 未提权，-1 = 查询失败
	int queryElevation(HANDLE process)
	{
		HANDLE token = nullptr;
		if (!OpenProcessToken(process, TOKEN_QUERY, &token)) return -1;
		TOKEN_ELEVATION elevation{};
		DWORD returned = 0;
		const BOOL ok = GetTokenInformation(token, TokenElevation, &elevation, sizeof(elevation), &returned);
		CloseHandle(token);
		return ok ? (elevation.TokenIsElevated ? 1 : 0) : -1;
	}

	int queryProcessElevation(DWORD pid)
	{
		const HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
		if (!process) return -1;
		const int result = queryElevation(process);
		CloseHandle(process);
		return result;
	}

	const wchar_t* elevationText(int state)
	{
		switch (state)
		{
		case 1: return L"yes";
		case 0: return L"no";
		default: return L"unknown";
		}
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

std::wstring Injector::getTargetWindowClass() const
{
	std::scoped_lock lock(m_targetMutex);
	return m_targetWindowClass;
}

std::wstring Injector::getTargetWindowTitle() const
{
	std::scoped_lock lock(m_targetMutex);
	return m_targetWindowTitle;
}

void Injector::setTargetWindow(std::wstring windowClass, std::wstring windowTitle)
{
	std::scoped_lock lock(m_targetMutex);
	m_targetWindowClass = std::move(windowClass);
	m_targetWindowTitle = std::move(windowTitle);
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
	logger::g_logger.write(logger::Level::Err, message);
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
	namespace lg = logger;
	g_menu->isInjecting.store(true, std::memory_order_release);
	{
		std::scoped_lock lock(m_resultMutex);
		m_lastError.clear();
		m_lastSucceeded = false;
	}

	const auto processName = getTargetProcessName();
	const auto dllAbsolutePath = std::filesystem::absolute(dllPath).wstring();

	lg::g_logger.write(lg::Level::Info, L"========== 开始注入 ==========");
	lg::g_logger.write(lg::Level::Info, L"[1/8] 目标进程: " + processName + L"，模块: " + dllAbsolutePath);

	// 注入前先在本进程校验 PE 头，尽早发现架构不匹配等问题
	lg::g_logger.write(lg::Level::Info, L"[2/8] 读取 DLL 文件并校验 PE 头...");
	std::vector<BYTE> buffer;
	if (!utils::readFileToMem(dllAbsolutePath, buffer) || buffer.empty())
		return fail(L"[2/8] DLL 文件读取失败: " + dllAbsolutePath);
	lg::g_logger.write(lg::Level::Info, L"[2/8] 文件大小: " + std::to_wstring(buffer.size()) + L" 字节");

	std::wstring reason;
	if (!isX86Image(buffer, reason))
		return fail(L"[2/8] PE 校验失败（非 Win32 x86 镜像）: " + reason);
	lg::g_logger.write(lg::Level::Ok, L"[2/8] Win32 x86 镜像校验通过");

	lg::g_logger.write(lg::Level::Info,
		L"[3/8] 等待目标进程（优先窗口定位，进程名兜底，最长 30 秒）...");
	const auto pid = waitForProcess(processName);
	if (pid == 0)
		return fail(L"[3/8] 30 秒内未发现目标进程: " + processName);
	lg::g_logger.write(lg::Level::Ok, L"[3/8] 已定位目标进程 pid=" + std::to_wstring(pid));

	lg::g_logger.write(lg::Level::Info, L"[4/8] OpenProcess 打开目标进程...");
	const auto processHandle = OpenProcess(kProcessAccess, FALSE, pid);
	if (!processHandle)
	{
		// error 5 几乎都是权限不匹配：游戏以管理员运行而注入器没有。
		// 双方提权状态直接打印出来，用户截图即可定位
		std::wostringstream detail;
		detail << L"[4/8] OpenProcess 失败 (error " << GetLastError()
			<< L", pid " << pid
			<< L", game admin=" << elevationText(queryProcessElevation(pid))
			<< L", injector admin=" << elevationText(queryElevation(GetCurrentProcess()))
			<< L")。请右键注入器选择\"以管理员身份运行\"后再注入";
		return fail(detail.str());
	}
	lg::g_logger.write(lg::Level::Ok, L"[4/8] 进程句柄获取成功");

	const auto error = injectByThreadHijack(processHandle, dllAbsolutePath);
	if (error.empty())
	{
		CloseHandle(processHandle);
		lg::g_logger.write(lg::Level::Ok, L"========== 注入成功，模块已加载 ==========");
		finish(true);
		return true;
	}

	// 劫持失败（线程打开失败等环境原因）时回退经典远程线程注入。
	// 即便劫持半途 LoadLibrary 已成功，二次 LoadLibraryW 也只会增加引用计数，
	// DllMain 不会重复执行，回退是安全的。
	lg::g_logger.write(lg::Level::Info,
		L"[5/8] 线程劫持失败: " + error + L"；回退 CreateRemoteThread 注入...");
	const auto fallbackError = injectByLoadLibrary(processHandle, dllAbsolutePath);
	CloseHandle(processHandle);
	if (!fallbackError.empty())
		return fail(fallbackError);

	lg::g_logger.write(lg::Level::Ok, L"========== 注入成功，模块已加载 ==========");
	finish(true);
	return true;
}

DWORD Injector::waitForProcess(const std::wstring& processName)
{
	const auto deadline = std::chrono::steady_clock::now() + kProcessWaitTimeout;
	while (std::chrono::steady_clock::now() < deadline)
	{
		// 优先按游戏主窗口类名/标题定位（Spy++ 思路，最可靠），进程名兜底
		if (const auto pid = mem::getProcIDByWindow(getTargetWindowClass(), getTargetWindowTitle()); pid != 0)
			return pid;
		if (const auto pid = mem::getProcID(processName); pid != 0)
			return pid;
		std::this_thread::sleep_for(kPollInterval);
	}
	return 0;
}

std::wstring Injector::injectByLoadLibrary(HANDLE processHandle, const std::wstring& dllPath)
{
	namespace lg = logger;
	const auto pathBytes = (dllPath.size() + 1) * sizeof(wchar_t);

	lg::g_logger.write(lg::Level::Info,
		L"[5/8] VirtualAllocEx 在目标进程分配内存 (" + std::to_wstring(pathBytes) + L" 字节)...");
	const auto remoteMemory = VirtualAllocEx(
		processHandle, nullptr, pathBytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	if (!remoteMemory)
		return L"[5/8] VirtualAllocEx 失败 (error " + std::to_wstring(GetLastError()) + L")";
	{
		std::wostringstream detail;
		detail << L"[5/8] 远程内存分配成功 @ 0x" << std::hex << reinterpret_cast<uintptr_t>(remoteMemory);
		lg::g_logger.write(lg::Level::Ok, detail.str());
	}

	lg::g_logger.write(lg::Level::Info, L"[6/8] WriteProcessMemory 写入 DLL 路径...");
	if (!WriteProcessMemory(processHandle, remoteMemory, dllPath.c_str(), pathBytes, nullptr))
	{
		const auto message = L"[6/8] WriteProcessMemory 失败 (error " + std::to_wstring(GetLastError()) + L")";
		VirtualFreeEx(processHandle, remoteMemory, 0, MEM_RELEASE);
		return message;
	}
	lg::g_logger.write(lg::Level::Ok, L"[6/8] DLL 路径写入成功");

	const auto loadLibraryW = reinterpret_cast<LPTHREAD_START_ROUTINE>(
		GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "LoadLibraryW"));
	if (!loadLibraryW)
	{
		VirtualFreeEx(processHandle, remoteMemory, 0, MEM_RELEASE);
		return L"[7/8] kernel32!LoadLibraryW 未找到";
	}

	lg::g_logger.write(lg::Level::Info, L"[7/8] CreateRemoteThread 创建远程线程调用 LoadLibraryW...");
	const auto remoteThread = CreateRemoteThread(
		processHandle, nullptr, 0, loadLibraryW, remoteMemory, 0, nullptr);
	if (!remoteThread)
	{
		const auto message = L"[7/8] CreateRemoteThread 失败 (error " + std::to_wstring(GetLastError()) + L")";
		VirtualFreeEx(processHandle, remoteMemory, 0, MEM_RELEASE);
		return message;
	}
	{
		std::wostringstream detail;
		detail << L"[7/8] 远程线程已创建 tid=" << GetThreadId(remoteThread)
			<< L"，等待目标进程内 DllMain 完成（最长 10 秒）...";
		lg::g_logger.write(lg::Level::Info, detail.str());
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
		return L"[7/8] LoadLibraryW 10 秒内未返回（目标进程内 DllMain 可能死锁）";

	if (remoteModule == 0)
		return L"[7/8] LoadLibraryW 在目标进程内返回 NULL（DLL 初始化失败，详见游戏目录 Ra2Overlay.log）";

	{
		std::wostringstream detail;
		detail << L"[8/8] 模块基址 0x" << std::hex << remoteModule << L"，远程内存已释放";
		lg::g_logger.write(lg::Level::Ok, detail.str());
	}

	return {};
}

namespace
{
	// 目标进程内"游戏窗口线程"的 tid：窗口线程必然长寿且被反作弊熟知，
	// 劫持它比随机线程更接近"游戏自己加载 DLL"的正常路径。找不到返回 0。
	DWORD findWindowThreadTid(DWORD pid)
	{
		struct Query { DWORD pid; DWORD tid; } query{ pid, 0 };
		EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL {
			auto* q = reinterpret_cast<Query*>(lParam);
			DWORD owner = 0;
			const DWORD tid = GetWindowThreadProcessId(hwnd, &owner);
			if (owner == q->pid && IsWindowVisible(hwnd))
			{
				q->tid = tid;
				return FALSE;
			}
			return TRUE;
			}, reinterpret_cast<LPARAM>(&query));
		return query.tid;
	}
}

std::wstring Injector::injectByThreadHijack(HANDLE processHandle, const std::wstring& dllPath)
{
	namespace lg = logger;
	const DWORD pid = GetProcessId(processHandle);

	// ---- [5/8] 选定劫持线程：优先游戏窗口线程，兜底创建最早的线程（主线程）----
	constexpr DWORD kThreadAccess =
		THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_SET_CONTEXT | THREAD_QUERY_INFORMATION;

	DWORD targetTid = findWindowThreadTid(pid);
	if (targetTid != 0)
	{
		lg::g_logger.write(lg::Level::Info,
			L"[5/8] 已锁定游戏窗口线程 tid=" + std::to_wstring(targetTid) + L" 作为劫持目标");
	}

	const auto snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
	if (snapshot == INVALID_HANDLE_VALUE)
		return L"CreateToolhelp32Snapshot(THREAD) 失败 (error " + std::to_wstring(GetLastError()) + L")";

	DWORD bestTid = 0;
	LONGLONG bestCreateTime = -1;
	THREADENTRY32 entry{};
	entry.dwSize = sizeof(entry);
	if (Thread32First(snapshot, &entry))
	{
		do
		{
			if (entry.th32OwnerProcessID != pid) continue;
			const HANDLE candidate = OpenThread(kThreadAccess, FALSE, entry.th32ThreadID);
			if (!candidate) continue;
			FILETIME created{}, exited{}, kernel{}, user{};
			LONGLONG createTime = -1;
			if (GetThreadTimes(candidate, &created, &exited, &kernel, &user))
				createTime = (static_cast<LONGLONG>(created.dwHighDateTime) << 32) | created.dwLowDateTime;
			CloseHandle(candidate);
			if (bestTid == 0 || (createTime != -1 && (bestCreateTime == -1 || createTime < bestCreateTime)))
			{
				bestTid = entry.th32ThreadID;
				bestCreateTime = createTime;
			}
		} while (Thread32Next(snapshot, &entry));
	}
	CloseHandle(snapshot);
	if (bestTid == 0)
		return L"目标进程内未找到可劫持的线程";
	if (targetTid == 0)
	{
		targetTid = bestTid;
		lg::g_logger.write(lg::Level::Info,
			L"[5/8] 未找到游戏窗口线程，改用最早创建的线程 tid=" + std::to_wstring(targetTid));
	}

	const HANDLE thread = OpenThread(kThreadAccess, FALSE, targetTid);
	if (!thread)
		return L"OpenThread(tid=" + std::to_wstring(targetTid) + L") 失败 (error " + std::to_wstring(GetLastError()) + L")";

	// ---- 远程内存布局: [0]=doneFlag [4]=hmodule [0x10]=DLL路径 [对齐]=shellcode ----
	const SIZE_T pathBytes = (dllPath.size() + 1) * sizeof(wchar_t);
	constexpr SIZE_T kShellcodeSize = 64;
	constexpr SIZE_T kPathOffset = 0x10;
	const SIZE_T shellcodeOffset = (kPathOffset + pathBytes + 15) & ~static_cast<SIZE_T>(15);
	const SIZE_T totalSize = shellcodeOffset + kShellcodeSize;

	const auto remoteBase = VirtualAllocEx(
		processHandle, nullptr, totalSize, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
	if (!remoteBase)
	{
		CloseHandle(thread);
		return L"VirtualAllocEx 失败 (error " + std::to_wstring(GetLastError()) + L")";
	}
	const auto base = reinterpret_cast<uintptr_t>(remoteBase);
	const auto remotePath = base + kPathOffset;
	const auto remoteShellcode = base + shellcodeOffset;
	const auto addrPtr = [](uintptr_t address) { return reinterpret_cast<LPVOID>(address); };

	// ---- 构造 x86 shellcode（纯字节序列，无内联汇编）----
	//   pushfd; pushad
	//   push <dllPath>; mov eax,<LoadLibraryW>; call eax
	//   mov edi,<result>; mov [edi],eax
	//   mov edi,<doneFlag>; mov dword [edi],1
	//   popad; popfd
	//   push <origEip>; ret     ; 恢复被劫持线程的原始执行位置
	BYTE code[kShellcodeSize]{};
	SIZE_T n = 0;
	SIZE_T loadLibraryOperandPos = 0;
	SIZE_T origEipOperandPos = 0;
	const auto emit8 = [&](BYTE value) { code[n++] = value; };
	const auto emit32 = [&](DWORD value) { memcpy(code + n, &value, sizeof(DWORD)); n += sizeof(DWORD); };
	emit8(0x9C);                                                  // pushfd
	emit8(0x60);                                                  // pushad
	emit8(0x68); emit32(static_cast<DWORD>(remotePath));           // push dllPath
	emit8(0xB8); loadLibraryOperandPos = n; emit32(0);             // mov eax, LoadLibraryW
	emit8(0xFF); emit8(0xD0);                                     // call eax
	emit8(0xBF); emit32(static_cast<DWORD>(base + 4));             // mov edi, result
	emit8(0x89); emit8(0x07);                                     // mov [edi], eax
	emit8(0xBF); emit32(static_cast<DWORD>(base));                 // mov edi, doneFlag
	emit8(0xC7); emit8(0x07); emit32(1);                          // mov dword [edi], 1
	emit8(0x61);                                                  // popad
	emit8(0x9D);                                                  // popfd
	emit8(0x68); origEipOperandPos = n; emit32(0);                 // push origEip
	emit8(0xC3);                                                  // ret

	// ---- [6/8] 写入数据区与 shellcode ----
	SIZE_T written = 0;
	const DWORD zero[2] = { 0, 0 };
	if (!WriteProcessMemory(processHandle, addrPtr(base), zero, sizeof(zero), &written)
		|| !WriteProcessMemory(processHandle, addrPtr(remotePath), dllPath.c_str(), pathBytes, &written)
		|| !WriteProcessMemory(processHandle, addrPtr(remoteShellcode), code, sizeof(code), &written))
	{
		const auto message = L"[6/8] WriteProcessMemory 失败 (error " + std::to_wstring(GetLastError()) + L")";
		VirtualFreeEx(processHandle, remoteBase, 0, MEM_RELEASE);
		CloseHandle(thread);
		return message;
	}

	const auto loadLibraryW = static_cast<DWORD>(reinterpret_cast<uintptr_t>(
		GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "LoadLibraryW")));
	if (loadLibraryW == 0)
	{
		VirtualFreeEx(processHandle, remoteBase, 0, MEM_RELEASE);
		CloseHandle(thread);
		return L"[6/8] kernel32!LoadLibraryW 未找到";
	}
	if (!WriteProcessMemory(processHandle,
		addrPtr(remoteShellcode + loadLibraryOperandPos), &loadLibraryW, sizeof(DWORD), &written))
	{
		const auto message = L"[6/8] 回填 LoadLibraryW 地址失败 (error " + std::to_wstring(GetLastError()) + L")";
		VirtualFreeEx(processHandle, remoteBase, 0, MEM_RELEASE);
		CloseHandle(thread);
		return message;
	}

	// ---- [7/8] 挂起线程 -> 抓原始上下文 -> EIP 指向 shellcode -> 恢复执行 ----
	if (SuspendThread(thread) == static_cast<DWORD>(-1))
	{
		const auto message = L"[7/8] SuspendThread 失败 (error " + std::to_wstring(GetLastError()) + L")";
		VirtualFreeEx(processHandle, remoteBase, 0, MEM_RELEASE);
		CloseHandle(thread);
		return message;
	}

	bool resumed = false;
	const auto resumeOnce = [&]() {
		if (!resumed)
		{
			ResumeThread(thread);
			resumed = true;
		}
	};

	CONTEXT context{};
	context.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER;
	if (!GetThreadContext(thread, &context) || context.Eip == 0)
	{
		resumeOnce();
		VirtualFreeEx(processHandle, remoteBase, 0, MEM_RELEASE);
		CloseHandle(thread);
		return L"[7/8] GetThreadContext 失败或 EIP 无效 (error " + std::to_wstring(GetLastError()) + L")";
	}

	const DWORD origEip = context.Eip;
	if (!WriteProcessMemory(processHandle,
		addrPtr(remoteShellcode + origEipOperandPos), &origEip, sizeof(DWORD), &written))
	{
		resumeOnce();
		VirtualFreeEx(processHandle, remoteBase, 0, MEM_RELEASE);
		CloseHandle(thread);
		return L"[7/8] 回填原始 EIP 失败 (error " + std::to_wstring(GetLastError()) + L")";
	}

	context.Eip = static_cast<DWORD>(remoteShellcode);
	if (!SetThreadContext(thread, &context))
	{
		resumeOnce();
		VirtualFreeEx(processHandle, remoteBase, 0, MEM_RELEASE);
		CloseHandle(thread);
		return L"[7/8] SetThreadContext 失败 (error " + std::to_wstring(GetLastError()) + L")";
	}
	resumeOnce();

	std::wostringstream hijackDetail;
	hijackDetail << L"[7/8] 线程 tid=" << targetTid
		<< L" 已劫持执行 LoadLibraryW（原始 EIP 0x" << std::hex << origEip
		<< L" 已保存），等待加载完成（最长 10 秒）...";
	lg::g_logger.write(lg::Level::Info, hijackDetail.str());

	// 轮询完成标志：doneFlag 由 shellcode 在 LoadLibraryW 返回后置 1
	DWORD flagAndResult[2] = { 0, 0 };
	const auto deadline = std::chrono::steady_clock::now() + kLoadWaitTimeout;
	while (std::chrono::steady_clock::now() < deadline)
	{
		if (!ReadProcessMemory(processHandle, addrPtr(base),
			flagAndResult, sizeof(flagAndResult), &written))
		{
			break;  // 读取失败多半是目标进程已退出
		}
		if (flagAndResult[0] != 0)
			break;
		std::this_thread::sleep_for(kHijackPollInterval);
	}

	if (flagAndResult[0] == 0)
	{
		// 超时不释放内存：线程可能仍在 shellcode/LoadLibrary 内，
		// 泄漏几 KB 远比释放后线程悬空执行安全。目标线程已恢复运行，无需再动。
		CloseHandle(thread);
		return L"[7/8] 线程劫持 10 秒未完成（被劫持线程可能持有 loader 锁，稍后重试或重启游戏）";
	}

	// 完成标志置位后线程只剩 popad/popfd/push/ret 四条指令即回到原始 EIP，
	// 轮询间隔 50ms 早已覆盖该窗口，可安全回收内存
	Sleep(100);
	VirtualFreeEx(processHandle, remoteBase, 0, MEM_RELEASE);
	CloseHandle(thread);

	if (flagAndResult[1] == 0)
		return L"[8/8] LoadLibraryW 在目标进程内返回 NULL（DLL 初始化失败，详见游戏目录 Ra2Overlay.log）";

	std::wostringstream okDetail;
	okDetail << L"[8/8] 模块基址 0x" << std::hex << flagAndResult[1] << L"，远程内存已释放";
	lg::g_logger.write(lg::Level::Ok, okDetail.str());

	return {};
}
