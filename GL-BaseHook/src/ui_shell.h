#pragma once

#include <Windows.h>
#include <gl/GL.h>

namespace Ra2Overlay::UiShell
{
    bool Initialize(HWND window, HDC deviceContext, HGLRC renderContext);
    void RenderFrame(HDC deviceContext);
    void Shutdown();
    void AbandonAfterTimeout();
    bool IsInitialized();
    bool Matches(HWND window, HGLRC renderContext);
    void ToggleMenu();
    bool MenuVisible();
}
