#pragma once

#include <atomic>

// 超武无限 / 秒充能（单机功能）：
// 每逻辑帧遍历当前玩家全部超级武器（SuperClass::Array 0xA83CB8），
// 置 IsCharged=true（:137）并清空 RechargeTimer.TimeLeft（:122），使其始终可发射。
// 写操作在联机对局中会导致 desync，仅在单机可用。

namespace Ra2Overlay::UnlimitedSuperweapon
{
    extern std::atomic<bool> Enabled;   // UI 线程写、逻辑线程读，必须原子

    void Register();   // 幂等：将帧回调挂入 GameHooks
    void RenderConfig();
}