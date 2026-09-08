#include "pch.h"

#include "ddraw_hook.h"

#include "log.h"
#include "runtime.h"
#include "ui_shell.h"
#include "window_bridge.h"

#include <ddraw.h>

// ---------------------------------------------------------------------------
// x86 COM 调用约定说明：
//   COM 方法 = thiscall（this 经 ECX 传递，栈参由被调方清理）。
//   本文件所有 vtable 钩子一律使用 MSVC 经典手法：
//       HRESULT __fastcall Hook(void* self, void* edx, ...实参...)
//   fastcall 前 2 个参数走 ECX/EDX，其余走栈、被调方清理 —— 与 thiscall
//   在机器层面完全一致（EDX 是调用方遗留垃圾，忽略即可）。
//   调用原函数时同样通过 fastcall 函数指针类型传回 self/edx，保证寄存器
//   状态与原始调用完全一致。
//
// vtable 槽位依据 Windows SDK (10.0.26100.0) um\ddraw.h 的接口声明顺序：
//   IDirectDraw7:       QueryInterface(0) AddRef(1) Release(2) Compact(3)
//                       CreateClipper(4) CreatePalette(5) CreateSurface(6) ...
//                       SetCooperativeLevel(20)
//   IDirectDrawSurface7: QueryInterface(0) AddRef(1) Release(2)
//                       AddAttachedSurface(3) AddOverlayDirtyRect(4) Blt(5)
//                       BltBatch(6) BltFast(7) ... Flip(11) ... Unlock(25)
// ---------------------------------------------------------------------------
namespace
{
    // {15E65EC0-3B9C-11D2-B92F-00609797EA5A}
    const GUID kIID_IDirectDraw7 = {
        0x15e65ec0, 0x3b9c, 0x11d2, {0xb9, 0x2f, 0x00, 0x60, 0x97, 0x97, 0xea, 0x5a} };

    constexpr int kSlotCreateSurface = 6;
    constexpr int kSlotSetCooperativeLevel = 20;
    constexpr int kSlotBlt = 5;
    constexpr int kSlotBltFast = 7;
    constexpr int kSlotFlip = 11;
    constexpr int kSlotUnlock = 25;

    using FnSetCooperativeLevel = HRESULT(__fastcall*)(IDirectDraw7*, void*, HWND, DWORD);
    using FnCreateSurface = HRESULT(__fastcall*)(
        IDirectDraw7*, void*, LPDDSURFACEDESC2, LPDIRECTDRAWSURFACE7*, IUnknown*);
    using FnBlt = HRESULT(__fastcall*)(
        IDirectDrawSurface7*, void*, LPRECT, LPDIRECTDRAWSURFACE7, LPRECT, DWORD, LPDDBLTFX);
    using FnBltFast = HRESULT(__fastcall*)(
        IDirectDrawSurface7*, void*, DWORD, DWORD, LPDIRECTDRAWSURFACE7, LPRECT, DWORD);
    using FnFlip = HRESULT(__fastcall*)(IDirectDrawSurface7*, void*, LPDIRECTDRAWSURFACE7, DWORD);
    using FnUnlock = HRESULT(__fastcall*)(IDirectDrawSurface7*, void*, LPRECT);

    // 前向声明：HookCreateSurface 中引用（定义在其后）
    HRESULT __fastcall HookBlt(IDirectDrawSurface7*, void*, LPRECT, LPDIRECTDRAWSURFACE7, LPRECT, DWORD, LPDDBLTFX);
    HRESULT __fastcall HookBltFast(IDirectDrawSurface7*, void*, DWORD, DWORD, LPDIRECTDRAWSURFACE7, LPRECT, DWORD);
    HRESULT __fastcall HookFlip(IDirectDrawSurface7*, void*, LPDIRECTDRAWSURFACE7, DWORD);
    HRESULT __fastcall HookUnlock(IDirectDrawSurface7*, void*, LPRECT);

    FnSetCooperativeLevel g_origSetCooperativeLevel = nullptr;
    FnCreateSurface g_origCreateSurface = nullptr;
    FnBlt g_origBlt = nullptr;
    FnBltFast g_origBltFast = nullptr;
    FnFlip g_origFlip = nullptr;
    FnUnlock g_origUnlock = nullptr;

    void** g_ddVtbl = nullptr;   // IDirectDraw7 共享 vtable（位于 ddraw.dll 静态数据）
    void** g_surfVtbl = nullptr; // IDirectDrawSurface7 共享 vtable
    IDirectDrawSurface7* g_primarySurface = nullptr;
    HWND g_gameWindow = nullptr;
    HMODULE g_ddrawModule = nullptr;

    std::atomic_bool g_stopping{ false };
    std::atomic_bool g_ddHooked{ false };
    std::atomic_bool g_surfHooked{ false };
    std::atomic_uint32_t g_activePresents{ 0 };
    bool g_createExHooked = false;
    void* g_createExTarget = nullptr;
    void* g_origCreateEx = nullptr;

    using DirectDrawCreateExFn = HRESULT(WINAPI*)(GUID*, LPDIRECTDRAW7*, REFIID, IUnknown*);

    // PresentOverlay 活动计数：卸载时等待其归零再还原 vtable
    struct PresentGuard
    {
        PresentGuard() { g_activePresents.fetch_add(1, std::memory_order_acq_rel); }
        ~PresentGuard() { g_activePresents.fetch_sub(1, std::memory_order_acq_rel); }
    };

    LONGLONG g_qpcFreq = 0;
    LONGLONG g_lastPresentQpc = 0;
    constexpr LONG kPresentIntervalMs = 8; // 覆盖层重绘节流

    bool IsInDdrawModule(const void* address)
    {
        HMODULE module = nullptr;
        return GetModuleHandleExW(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            static_cast<LPCWSTR>(address),
            &module)
            && module == g_ddrawModule;
    }

    bool PatchVtableSlot(void** vtbl, int slot, void* detour, void** originalOut)
    {
        void* const target = vtbl[slot];
        if (!IsInDdrawModule(target))
        {
            Ra2Overlay::Log::Write("DDraw: vtable slot %d (%p) not inside ddraw.dll; refusing to patch", slot, target);
            return false;
        }
        DWORD oldProtect = 0;
        if (!VirtualProtect(&vtbl[slot], sizeof(void*), PAGE_EXECUTE_READWRITE, &oldProtect))
        {
            Ra2Overlay::Log::Write("DDraw: VirtualProtect failed (slot %d, err=%lu)", slot, GetLastError());
            return false;
        }
        vtbl[slot] = detour;
        VirtualProtect(&vtbl[slot], sizeof(void*), oldProtect, &oldProtect);
        *originalOut = target;
        return true;
    }

    void RestoreVtableSlot(void** vtbl, int slot, void* original)
    {
        if (!vtbl || !original)
        {
            return;
        }
        DWORD oldProtect = 0;
        if (VirtualProtect(&vtbl[slot], sizeof(void*), PAGE_EXECUTE_READWRITE, &oldProtect))
        {
            vtbl[slot] = original;
            VirtualProtect(&vtbl[slot], sizeof(void*), oldProtect, &oldProtect);
        }
    }

    // 每帧演示（Blt/BltFast/Flip/Unlock）之后调用：把 ImGui 菜单画到主表面上。
    void PresentOverlay(IDirectDrawSurface7* surface)
    {
        if (g_stopping.load(std::memory_order_acquire))
        {
            if (Ra2Overlay::UiShell::IsInitialized())
            {
                Ra2Overlay::UiShell::Shutdown();
            }
            Ra2Overlay::WindowBridge::Detach();
            Ra2Overlay::Runtime::NotifyRenderStopped();
            return;
        }

        // 节流：DD 主表面可能每帧多次 Lock/Unlock/Blt
        LARGE_INTEGER liNow{};
        QueryPerformanceCounter(&liNow);
        const LONGLONG now = liNow.QuadPart;
        if (g_lastPresentQpc != 0
            && (now - g_lastPresentQpc) * 1000 < g_qpcFreq * kPresentIntervalMs)
        {
            return;
        }
        g_lastPresentQpc = now;

        HWND window = g_gameWindow;
        if (!window || !IsWindow(window))
        {
            window = GetActiveWindow();
        }
        if (!window)
        {
            return;
        }

        if (!Ra2Overlay::WindowBridge::Attach(window))
        {
            return;
        }

        if (Ra2Overlay::UiShell::IsInitialized()
            && !Ra2Overlay::UiShell::Matches(window, nullptr))
        {
            Ra2Overlay::Log::Write("DDraw: window changed; rebuilding UI shell");
            Ra2Overlay::UiShell::Shutdown();
            Ra2Overlay::WindowBridge::Detach();
        }

        if (!Ra2Overlay::UiShell::IsInitialized()
            && !Ra2Overlay::UiShell::Initialize(window, nullptr, nullptr))
        {
            Ra2Overlay::WindowBridge::Detach();
            return;
        }

        HDC surfaceDc = nullptr;
        if (FAILED(surface->GetDC(&surfaceDc)) || !surfaceDc)
        {
            return;
        }
        Ra2Overlay::UiShell::RenderFrame(surfaceDc);
        surface->ReleaseDC(surfaceDc);
    }

    HRESULT __fastcall HookSetCooperativeLevel(IDirectDraw7* self, void* edx, HWND window, DWORD flags)
    {
        if (window)
        {
            g_gameWindow = window;
        }
        return g_origSetCooperativeLevel(self, edx, window, flags);
    }

    HRESULT __fastcall HookCreateSurface(
        IDirectDraw7* self, void* edx, LPDDSURFACEDESC2 desc,
        LPDIRECTDRAWSURFACE7* outSurface, IUnknown* outer)
    {
        const HRESULT hr = g_origCreateSurface(self, edx, desc, outSurface, outer);
        if (SUCCEEDED(hr) && outSurface && *outSurface && desc && !g_surfHooked.load(std::memory_order_acquire)
            && (desc->ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE))
        {
            void** const vtbl = *reinterpret_cast<void***>(*outSurface);
            if (IsInDdrawModule(*reinterpret_cast<void**>(vtbl)))
            {
                void* originals[4] = {};
                bool allOk = PatchVtableSlot(vtbl, kSlotBlt, reinterpret_cast<void*>(&HookBlt), &originals[0])
                    && PatchVtableSlot(vtbl, kSlotBltFast, reinterpret_cast<void*>(&HookBltFast), &originals[1])
                    && PatchVtableSlot(vtbl, kSlotFlip, reinterpret_cast<void*>(&HookFlip), &originals[2])
                    && PatchVtableSlot(vtbl, kSlotUnlock, reinterpret_cast<void*>(&HookUnlock), &originals[3]);
                if (allOk)
                {
                    g_origBlt = reinterpret_cast<FnBlt>(originals[0]);
                    g_origBltFast = reinterpret_cast<FnBltFast>(originals[1]);
                    g_origFlip = reinterpret_cast<FnFlip>(originals[2]);
                    g_origUnlock = reinterpret_cast<FnUnlock>(originals[3]);
                    g_surfVtbl = vtbl;
                    g_primarySurface = *outSurface;
                    g_surfHooked.store(true, std::memory_order_release);
                    LARGE_INTEGER freq{};
                    QueryPerformanceFrequency(&freq);
                    g_qpcFreq = freq.QuadPart;
                    Ra2Overlay::Log::Write("DDraw: primary surface %p hooked (Blt/BltFast/Flip/Unlock)", *outSurface);
                }
                else
                {
                    // 部分成功时统一回滚，避免半钩子状态
                    RestoreVtableSlot(vtbl, kSlotBlt, originals[0]);
                    RestoreVtableSlot(vtbl, kSlotBltFast, originals[1]);
                    RestoreVtableSlot(vtbl, kSlotFlip, originals[2]);
                    RestoreVtableSlot(vtbl, kSlotUnlock, originals[3]);
                    Ra2Overlay::Log::Write("DDraw: primary surface vtable patch failed; DirectDraw overlay disabled");
                }
            }
            else
            {
                Ra2Overlay::Log::Write("DDraw: primary surface vtable not in ddraw.dll; skip");
            }
        }
        return hr;
    }

    HRESULT __fastcall HookBlt(
        IDirectDrawSurface7* self, void* edx, LPRECT destRect, LPDIRECTDRAWSURFACE7 srcSurface,
        LPRECT srcRect, DWORD flags, LPDDBLTFX bltFx)
    {
        const HRESULT hr = g_origBlt(self, edx, destRect, srcSurface, srcRect, flags, bltFx);
        if (SUCCEEDED(hr) && self == g_primarySurface)
        {
            PresentOverlay(self);
        }
        return hr;
    }

    HRESULT __fastcall HookBltFast(
        IDirectDrawSurface7* self, void* edx, DWORD destX, DWORD destY,
        LPDIRECTDRAWSURFACE7 srcSurface, LPRECT srcRect, DWORD flags)
    {
        const HRESULT hr = g_origBltFast(self, edx, destX, destY, srcSurface, srcRect, flags);
        if (SUCCEEDED(hr) && self == g_primarySurface)
        {
            PresentOverlay(self);
        }
        return hr;
    }

    HRESULT __fastcall HookFlip(IDirectDrawSurface7* self, void* edx, LPDIRECTDRAWSURFACE7 target, DWORD flags)
    {
        const HRESULT hr = g_origFlip(self, edx, target, flags);
        if (SUCCEEDED(hr) && self == g_primarySurface)
        {
            PresentOverlay(self);
        }
        return hr;
    }

    HRESULT __fastcall HookUnlock(IDirectDrawSurface7* self, void* edx, LPRECT rect)
    {
        const HRESULT hr = g_origUnlock(self, edx, rect);
        if (SUCCEEDED(hr) && self == g_primarySurface)
        {
            PresentOverlay(self);
        }
        return hr;
    }

    HRESULT WINAPI HookDirectDrawCreateEx(GUID* guid, LPDIRECTDRAW7* outDd, REFIID iid, IUnknown* outer)
    {
        const HRESULT hr = reinterpret_cast<DirectDrawCreateExFn>(g_origCreateEx)(guid, outDd, iid, outer);
        if (SUCCEEDED(hr) && outDd && *outDd && IsEqualGUID(iid, kIID_IDirectDraw7)
            && !g_ddHooked.load(std::memory_order_acquire))
        {
            void** const vtbl = *reinterpret_cast<void***>(*outDd);
            if (IsInDdrawModule(*reinterpret_cast<void**>(vtbl)))
            {
                void* origCreateSurface = nullptr;
                void* origSetCooperative = nullptr;
                if (PatchVtableSlot(vtbl, kSlotCreateSurface, reinterpret_cast<void*>(&HookCreateSurface), &origCreateSurface)
                    && PatchVtableSlot(vtbl, kSlotSetCooperativeLevel, reinterpret_cast<void*>(&HookSetCooperativeLevel), &origSetCooperative))
                {
                    g_origCreateSurface = reinterpret_cast<FnCreateSurface>(origCreateSurface);
                    g_origSetCooperativeLevel = reinterpret_cast<FnSetCooperativeLevel>(origSetCooperative);
                    g_ddVtbl = vtbl;
                    g_ddHooked.store(true, std::memory_order_release);
                    Ra2Overlay::Log::Write("DDraw: IDirectDraw7 vtable hooked (CreateSurface/SetCooperativeLevel)");
                }
                else
                {
                    RestoreVtableSlot(vtbl, kSlotCreateSurface, origCreateSurface);
                    RestoreVtableSlot(vtbl, kSlotSetCooperativeLevel, origSetCooperative);
                    Ra2Overlay::Log::Write("DDraw: IDirectDraw7 vtable patch failed");
                }
            }
            else
            {
                Ra2Overlay::Log::Write("DDraw: IDirectDraw7 vtable not in ddraw.dll; skip");
            }
        }
        return hr;
    }

    // 仅当宿主 exe 的静态导入表包含 ddraw.dll 时才启用 DD 钩子。
    // （gamemd-spawn.exe 的导入被 KK 改写为 EDRAW.dll，走 OpenGL 路径，
    //   此时强载系统 ddraw 只会造成双重渲染。）
    bool HostImportsDdraw()
    {
        const HMODULE exe = GetModuleHandleW(nullptr);
        if (!exe)
        {
            return false;
        }
        const BYTE* base = reinterpret_cast<const BYTE*>(exe);
        const auto dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
        if (dos->e_magic != IMAGE_DOS_SIGNATURE)
        {
            return false;
        }
        const auto nt = reinterpret_cast<const IMAGE_NT_HEADERS32*>(base + dos->e_lfanew);
        if (nt->Signature != IMAGE_NT_SIGNATURE)
        {
            return false;
        }
        const IMAGE_DATA_DIRECTORY& importDir =
            nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
        if (!importDir.VirtualAddress || !importDir.Size)
        {
            return false;
        }
        auto* importDesc = reinterpret_cast<const IMAGE_IMPORT_DESCRIPTOR*>(base + importDir.VirtualAddress);
        for (; importDesc->Name; ++importDesc)
        {
            const char* name = reinterpret_cast<const char*>(base + importDesc->Name);
            if (_stricmp(name, "ddraw.dll") == 0)
            {
                return true;
            }
        }
        return false;
    }
}

bool Ra2Overlay::DDrawHook::Initialize()
{
    if (!HostImportsDdraw())
    {
        Ra2Overlay::Log::Write("DDraw: host does not import ddraw.dll; DirectDraw path skipped");
        return true;
    }

    HMODULE ddraw = GetModuleHandleW(L"ddraw.dll");
    if (!ddraw)
    {
        // 注入时机足够晚时游戏已完成静态导入；此处兜底强载
        ddraw = LoadLibraryW(L"ddraw.dll");
    }
    if (!ddraw)
    {
        Ra2Overlay::Log::Write("DDraw: ddraw.dll unavailable; DirectDraw path skipped");
        return true;
    }
    g_ddrawModule = ddraw;

    void* const target = reinterpret_cast<void*>(GetProcAddress(ddraw, "DirectDrawCreateEx"));
    if (!target)
    {
        Ra2Overlay::Log::Write("DDraw: DirectDrawCreateEx not found; DirectDraw path skipped");
        return true;
    }

    const MH_STATUS createStatus = MH_CreateHook(
        target, reinterpret_cast<void*>(&HookDirectDrawCreateEx), &g_origCreateEx);
    if (createStatus != MH_OK)
    {
        Ra2Overlay::Log::Write("DDraw: MH_CreateHook(DirectDrawCreateEx) failed: %s", MH_StatusToString(createStatus));
        return true; // 非致命：GL 路径仍可用
    }

    const MH_STATUS enableStatus = MH_EnableHook(target);
    if (enableStatus != MH_OK)
    {
        Ra2Overlay::Log::Write("DDraw: MH_EnableHook(DirectDrawCreateEx) failed: %s", MH_StatusToString(enableStatus));
        MH_RemoveHook(target);
        return true;
    }

    g_createExTarget = target;
    g_createExHooked = true;
    Ra2Overlay::Log::Write("DDraw: DirectDrawCreateEx hooked at %p (waiting for game to create DD7)", target);
    return true;
}

void Ra2Overlay::DDrawHook::BeginShutdown()
{
    g_stopping.store(true, std::memory_order_release);
}

void Ra2Overlay::DDrawHook::Shutdown()
{
    g_stopping.store(true, std::memory_order_release);

    // 等待进行中的 PresentOverlay 结束（最多 ~2s），再动 vtable
    for (int attempt = 0; attempt < 2000
        && g_activePresents.load(std::memory_order_acquire) != 0;
        ++attempt)
    {
        Sleep(1);
    }

    // vtable 位于 ddraw.dll 静态数据中，进程存活期内地址恒定，可安全还原
    RestoreVtableSlot(g_surfVtbl, kSlotBlt, reinterpret_cast<void*>(g_origBlt));
    RestoreVtableSlot(g_surfVtbl, kSlotBltFast, reinterpret_cast<void*>(g_origBltFast));
    RestoreVtableSlot(g_surfVtbl, kSlotFlip, reinterpret_cast<void*>(g_origFlip));
    RestoreVtableSlot(g_surfVtbl, kSlotUnlock, reinterpret_cast<void*>(g_origUnlock));
    RestoreVtableSlot(g_ddVtbl, kSlotCreateSurface, reinterpret_cast<void*>(g_origCreateSurface));
    RestoreVtableSlot(g_ddVtbl, kSlotSetCooperativeLevel, reinterpret_cast<void*>(g_origSetCooperativeLevel));
    g_surfVtbl = nullptr;
    g_ddVtbl = nullptr;
    g_primarySurface = nullptr;
    g_ddHooked.store(false, std::memory_order_release);
    g_surfHooked.store(false, std::memory_order_release);

    if (g_createExHooked && g_createExTarget)
    {
        MH_DisableHook(g_createExTarget);
        MH_RemoveHook(g_createExTarget);
        g_createExHooked = false;
        g_createExTarget = nullptr;
        g_origCreateEx = nullptr;
    }
    Ra2Overlay::Log::Write("DDraw: hooks removed");
}
