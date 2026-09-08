#include "pch.h"

#include "ddraw_hook.h"
#include "log.h"
#include "render_hook.h"
#include "runtime.h"
#include "ui_shell.h"
#include "window_bridge.h"

namespace
{
    using SwapBuffersFunction = BOOL(WINAPI*)(HDC);

    SwapBuffersFunction g_originalSwapBuffers = nullptr;
    void* g_swapBuffersTarget = nullptr;
    std::atomic_bool g_stopping{ false };
    std::atomic_uint32_t g_activeCallbacks{ 0 };
    bool g_minHookInitialized = false;

    class CallbackGuard
    {
    public:
        CallbackGuard()
        {
            g_activeCallbacks.fetch_add(1, std::memory_order_acq_rel);
        }

        ~CallbackGuard()
        {
            g_activeCallbacks.fetch_sub(1, std::memory_order_acq_rel);
        }
    };

    BOOL WINAPI HookSwapBuffers(HDC deviceContext)
    {
        CallbackGuard guard;

        if (g_stopping.load(std::memory_order_acquire))
        {
            if (Ra2Overlay::UiShell::IsInitialized())
            {
                Ra2Overlay::UiShell::Shutdown();
            }
            Ra2Overlay::WindowBridge::Detach();
            Ra2Overlay::Runtime::NotifyRenderStopped();
        }
        else
        {
            const HGLRC renderContext = wglGetCurrentContext();
            const HWND window = WindowFromDC(deviceContext);

            if (renderContext && window)
            {
                if (Ra2Overlay::UiShell::IsInitialized()
                    && !Ra2Overlay::UiShell::Matches(window, renderContext))
                {
                    Ra2Overlay::Log::Write("OpenGL context changed; rebuilding ImGui backends");
                    Ra2Overlay::UiShell::Shutdown();
                    Ra2Overlay::WindowBridge::Detach();
                }

                if (Ra2Overlay::WindowBridge::Attach(window)
                    && Ra2Overlay::UiShell::Initialize(window, deviceContext, renderContext))
                {
                    Ra2Overlay::UiShell::RenderFrame(deviceContext);
                }
                else if (!Ra2Overlay::UiShell::IsInitialized())
                {
                    Ra2Overlay::WindowBridge::Detach();
                }
            }
        }

        return g_originalSwapBuffers ? g_originalSwapBuffers(deviceContext) : FALSE;
    }
}

bool Ra2Overlay::RenderHook::Initialize()
{
    const MH_STATUS initializeStatus = MH_Initialize();
    if (initializeStatus != MH_OK)
    {
        Log::Write("MH_Initialize failed: %s", MH_StatusToString(initializeStatus));
        return false;
    }
    g_minHookInitialized = true;

    const HMODULE gdi32 = GetModuleHandleW(L"gdi32.dll");
    g_swapBuffersTarget = gdi32
        ? reinterpret_cast<void*>(GetProcAddress(gdi32, "SwapBuffers"))
        : nullptr;
    if (!g_swapBuffersTarget)
    {
        Log::Write("GDI32!SwapBuffers was not found");
        Shutdown();
        return false;
    }

    const MH_STATUS createStatus = MH_CreateHook(
        g_swapBuffersTarget,
        reinterpret_cast<void*>(&HookSwapBuffers),
        reinterpret_cast<void**>(&g_originalSwapBuffers));
    if (createStatus != MH_OK)
    {
        Log::Write("MH_CreateHook failed: %s", MH_StatusToString(createStatus));
        Shutdown();
        return false;
    }

    const MH_STATUS enableStatus = MH_EnableHook(g_swapBuffersTarget);
    if (enableStatus != MH_OK)
    {
        Log::Write("MH_EnableHook failed: %s", MH_StatusToString(enableStatus));
        Shutdown();
        return false;
    }

    Log::Write("Hook enabled: GDI32!SwapBuffers at %p", g_swapBuffersTarget);

    // DirectDraw 演示路径（纯单机 gamemd.exe 走系统 ddraw，不经 SwapBuffers）。
    // 失败仅记日志，非致命。
    if (!Ra2Overlay::DDrawHook::Initialize())
    {
        Log::Write("DDraw hook initialization failed (non-fatal)");
    }
    return true;
}

void Ra2Overlay::RenderHook::BeginShutdown()
{
    g_stopping.store(true, std::memory_order_release);
    Ra2Overlay::DDrawHook::BeginShutdown();
    if (!UiShell::IsInitialized())
    {
        WindowBridge::Detach();
        Runtime::NotifyRenderStopped();
    }
}

void Ra2Overlay::RenderHook::Shutdown()
{
    g_stopping.store(true, std::memory_order_release);

    // 先还原 DirectDraw vtable 钩子，避免卸载 MinHook 后仍有 DD 调用进入覆盖层
    Ra2Overlay::DDrawHook::Shutdown();

    if (g_swapBuffersTarget)
    {
        MH_DisableHook(g_swapBuffersTarget);
    }

    for (int attempt = 0; attempt < 2000
        && g_activeCallbacks.load(std::memory_order_acquire) != 0;
        ++attempt)
    {
        Sleep(1);
    }

    WindowBridge::Detach();
    if (UiShell::IsInitialized())
    {
        UiShell::AbandonAfterTimeout();
    }

    if (g_swapBuffersTarget)
    {
        MH_RemoveHook(g_swapBuffersTarget);
        g_swapBuffersTarget = nullptr;
    }
    g_originalSwapBuffers = nullptr;

    if (g_minHookInitialized)
    {
        MH_Uninitialize();
        g_minHookInitialized = false;
    }
}
