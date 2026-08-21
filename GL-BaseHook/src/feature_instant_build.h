#pragma once

#include <atomic>

// 快速建造（单机功能）：
// 每逻辑帧把本方所有工厂的当前生产阶段计时清零（Production.Timer.TimeLeft = 0），
// 使 StageClass::Update 在下一帧立即完成当前生产项。
// 写操作在联机对局中会导致 desync，仅在单机可用。

namespace Ra2Overlay::InstantBuild
{
    extern std::atomic<bool> Enabled;   // UI 线程写、逻辑线程读，必须原子

    void Register();   // 幂等：将帧回调挂入 GameHooks
    void RenderConfig();
}
