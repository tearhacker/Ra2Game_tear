#pragma once

#include <atomic>

// 士兵种类直接升阶满级（单机功能）：
// 每逻辑帧将本方所有存活单位/建筑的军衔经验拉满（Veterancy.Veterancy = 2.0f => Elite）。
// 写操作在联机对局中会导致 desync，仅在单机可用。

namespace Ra2Overlay::VeterancyMax
{
    extern std::atomic<bool> Enabled;   // UI 线程写、逻辑线程读，必须原子

    void Register();   // 幂等：将帧回调挂入 GameHooks
    void RenderConfig();
}
