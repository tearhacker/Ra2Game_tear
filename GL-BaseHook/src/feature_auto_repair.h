#pragma once

#include <atomic>

// 自动维修（单机功能）：
// 每逻辑帧将本方受损建筑的维修进度立即推进（BuildingClass::RepairProgress :293），
// 使维修计时持续到期、快速回血。写操作在联机对局中会导致 desync，仅在单机可用。

namespace Ra2Overlay::AutoRepair
{
    extern std::atomic<bool> Enabled;   // UI 线程写、逻辑线程读，必须原子

    void Register();   // 幂等：将帧回调挂入 GameHooks
    void RenderConfig();
}