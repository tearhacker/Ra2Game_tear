#pragma once

#include <Windows.h>

namespace Ra2Overlay::Runtime
{
    DWORD WINAPI WorkerThread(void* moduleParameter);
    void RequestShutdown();
    bool IsShutdownRequested();
    void NotifyRenderStopped();
}
