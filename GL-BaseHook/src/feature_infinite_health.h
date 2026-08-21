#pragma once

#include <atomic>

// 无限血（单机功能）：
// 每逻辑帧把本方所有存活单位/建筑的 HP 刷回满值（Health = GetType()->Strength）。
// 写操作在联机对局中会导致 desync，仅在单机可用。

namespace Ra2Overlay::InfiniteHealth
{
    extern std::atomic<bool> Enabled;   // UI 线程写、逻辑线程读，必须原子

    void Register();   // 幂等：将帧回调挂入 GameHooks
    void RenderConfig();
}
