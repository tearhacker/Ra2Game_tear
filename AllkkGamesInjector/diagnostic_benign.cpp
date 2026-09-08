// 诊断用良性 DLL：DllMain 直接返回，无线程、无钩子、无内存操作。
// 用于 A/B 隔离测试：
//   劫持注入本 DLL 若存活  -> 触发处决的是 Ra2Overlay 的钩子/线程活动
//   劫持注入本 DLL 仍被杀  -> 劫持注入行为本身被反作弊标记
#include <windows.h>

BOOL APIENTRY DllMain(HMODULE, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(GetModuleHandleW(nullptr));
    }
    return TRUE;
}
