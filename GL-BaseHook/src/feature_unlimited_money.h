#pragma once

#include <atomic>

// 无限金钱（单机功能）：
// 每逻辑帧调用 HouseClass::GiveMoney(1000) 为当前玩家不断加钱（JMP_THIS 0x4F9950）。
// 写操作在联机对局中会导致 desync，仅在单机可用。

namespace Ra2Overlay::UnlimitedMoney
{
    extern std::atomic<bool> Enabled;   // UI 线程写、逻辑线程读，必须原子

    void Register();   // 幂等：将帧回调挂入 GameHooks
    void RenderConfig();
}