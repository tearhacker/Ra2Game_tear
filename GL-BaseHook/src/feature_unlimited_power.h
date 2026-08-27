#pragma once

#include <atomic>

// 无限电力（单机功能）：
// 每逻辑帧把当前玩家的 PowerOutput 字段拉高到 99999（:882），电力不再报警。
// 写操作在联机对局中会导致 desync，仅在单机可用。

namespace Ra2Overlay::UnlimitedPower
{
    extern std::atomic<bool> Enabled;   // UI 线程写、逻辑线程读，必须原子

    void Register();   // 幂等：将帧回调挂入 GameHooks
    void RenderConfig();
}