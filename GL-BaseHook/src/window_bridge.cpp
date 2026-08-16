#include "pch.h"

#include "log.h"
#include "runtime.h"
#include "ui_shell.h"
#include "window_bridge.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(
    HWND window,
    UINT message,
    WPARAM wParam,
    LPARAM lParam);

namespace
{
    HWND g_window = nullptr;
    WNDPROC g_originalWindowProc = nullptr;

    bool IsKeyboardMessage(UINT message)
    {
        return message >= WM_KEYFIRST && message <= WM_KEYLAST;
    }

    bool IsMouseMessage(UINT message)
    {
        return message >= WM_MOUSEFIRST && message <= WM_MOUSELAST;
    }

    bool IsReleaseMessage(UINT message)
    {
        return message == WM_KEYUP
            || message == WM_SYSKEYUP
            || message == WM_LBUTTONUP
            || message == WM_RBUTTONUP
            || message == WM_MBUTTONUP
            || message == WM_XBUTTONUP;
    }

    LRESULT CALLBACK OverlayWindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
    {
        const bool firstKeyPress = (lParam & (1LL << 30)) == 0;
        if ((message == WM_KEYDOWN || message == WM_SYSKEYDOWN) && firstKeyPress)
        {
            if (wParam == VK_INSERT)
            {
                Ra2Overlay::UiShell::ToggleMenu();
            }
            else if (wParam == VK_END)
            {
                Ra2Overlay::Runtime::RequestShutdown();
            }
        }

        if (Ra2Overlay::UiShell::IsInitialized() && Ra2Overlay::UiShell::MenuVisible())
        {
            ImGui_ImplWin32_WndProcHandler(window, message, wParam, lParam);
            const ImGuiIO& io = ImGui::GetIO();
            const bool capture = (IsMouseMessage(message) && io.WantCaptureMouse)
                || (IsKeyboardMessage(message) && io.WantCaptureKeyboard);
            if (capture && !IsReleaseMessage(message))
            {
                return TRUE;
            }
        }

        return g_originalWindowProc
            ? CallWindowProcW(g_originalWindowProc, window, message, wParam, lParam)
            : DefWindowProcW(window, message, wParam, lParam);
    }
}

bool Ra2Overlay::WindowBridge::Attach(HWND window)
{
    if (!window)
    {
        return false;
    }
    if (g_window == window && g_originalWindowProc)
    {
        return true;
    }

    Detach();
    SetLastError(ERROR_SUCCESS);
    const LONG_PTR previous = SetWindowLongPtrW(
        window,
        GWLP_WNDPROC,
        reinterpret_cast<LONG_PTR>(&OverlayWindowProc));
    if (previous == 0 && GetLastError() != ERROR_SUCCESS)
    {
        Log::Write("SetWindowLongPtrW failed: %lu", GetLastError());
        return false;
    }

    g_window = window;
    g_originalWindowProc = reinterpret_cast<WNDPROC>(previous);
    Log::Write("WndProc attached to window %p", window);
    return true;
}

void Ra2Overlay::WindowBridge::Detach()
{
    if (g_window && g_originalWindowProc)
    {
        SetWindowLongPtrW(
            g_window,
            GWLP_WNDPROC,
            reinterpret_cast<LONG_PTR>(g_originalWindowProc));
        Log::Write("WndProc restored for window %p", g_window);
    }
    g_originalWindowProc = nullptr;
    g_window = nullptr;
}

HWND Ra2Overlay::WindowBridge::Window()
{
    return g_window;
}
