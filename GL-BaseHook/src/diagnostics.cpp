#include "pch.h"

#include "diagnostics.h"

namespace
{
    Ra2Overlay::Diagnostics::Snapshot g_snapshot;

    std::string ReadGlString(GLenum name)
    {
        const GLubyte* value = glGetString(name);
        return value ? reinterpret_cast<const char*>(value) : "Unavailable";
    }
}

void Ra2Overlay::Diagnostics::Capture(HWND window, HDC deviceContext, HGLRC renderContext)
{
    g_snapshot.window = window;
    g_snapshot.deviceContext = deviceContext;
    g_snapshot.renderContext = renderContext;
    g_snapshot.glVersion = ReadGlString(GL_VERSION);
    g_snapshot.glVendor = ReadGlString(GL_VENDOR);
    g_snapshot.glRenderer = ReadGlString(GL_RENDERER);
    UpdateViewport(deviceContext);
}

void Ra2Overlay::Diagnostics::UpdateViewport(HDC deviceContext)
{
    g_snapshot.deviceContext = deviceContext;
    RECT clientRect{};
    if (g_snapshot.window && GetClientRect(g_snapshot.window, &clientRect))
    {
        g_snapshot.clientWidth = clientRect.right - clientRect.left;
        g_snapshot.clientHeight = clientRect.bottom - clientRect.top;
    }
}

const Ra2Overlay::Diagnostics::Snapshot& Ra2Overlay::Diagnostics::Get()
{
    return g_snapshot;
}
