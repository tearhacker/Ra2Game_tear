#pragma once

#include <Windows.h>
#include <gl/GL.h>

#include <string>

namespace Ra2Overlay::Diagnostics
{
    struct Snapshot
    {
        HWND window = nullptr;
        HDC deviceContext = nullptr;
        HGLRC renderContext = nullptr;
        int clientWidth = 0;
        int clientHeight = 0;
        std::string glVersion;
        std::string glVendor;
        std::string glRenderer;
    };

    void Capture(HWND window, HDC deviceContext, HGLRC renderContext);
    void UpdateViewport(HDC deviceContext);
    const Snapshot& Get();
}
