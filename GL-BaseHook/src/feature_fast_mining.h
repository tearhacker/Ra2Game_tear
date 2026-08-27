#pragma once

#include <atomic>

// 秒采矿（单机功能）：
// 每逻辑帧把当前玩家所有矿石精炼厂的 CashProductionTimer 清零（BuildingClass::CashProductionTimer :319），
// 使采矿生产计时立即到期，下一帧即产出资金。
// 写操作在联机对局中会导致 desync，仅在单机可用。

namespace Ra2Overlay::FastMining
{
    extern std::atomic<bool> Enabled;   // UI 线程写、逻辑线程读，必须原子

    void Register();   // 幂等：将帧回调挂入 GameHooks
    void RenderConfig();
}