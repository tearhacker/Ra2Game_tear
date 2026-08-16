#pragma once

#include <Windows.h>

namespace Ra2Overlay::WindowBridge
{
    bool Attach(HWND window);
    void Detach();
    HWND Window();
}
