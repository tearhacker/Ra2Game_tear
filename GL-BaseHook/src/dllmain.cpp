#include "pch.h"

#include "runtime.h"

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, void*)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(module);
        HANDLE worker = CreateThread(
            nullptr,
            0,
            &Ra2Overlay::Runtime::WorkerThread,
            module,
            0,
            nullptr);
        if (worker)
        {
            CloseHandle(worker);
        }
        else
        {
            return FALSE;
        }
    }
    return TRUE;
}
