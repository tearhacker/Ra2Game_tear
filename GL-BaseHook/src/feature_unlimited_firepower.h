#pragma once

#include <atomic>

// 火力无限（单机功能）：
// 每逻辑帧把本方所有单位的装填计时清零（ReloadTimer.TimeLeft=0, :587），
// 并把需要弹药的单位弹药补满（Ammo=type->Ammo, :650）。
// 写操作在联机对局中会导致 desync，仅在单机可用。

namespace Ra2Overlay::UnlimitedFirepower
{
    extern std::atomic<bool> Enabled;   // UI 线程写、逻辑线程读，必须原子

    void Register();   // 幂等：将帧回调挂入 GameHooks
    void RenderConfig();
}