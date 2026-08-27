#pragma once

#include <atomic>

// 瞬转炮塔（单机功能）：
// 每逻辑帧将本方单位的主炮塔/枪管朝向锁定到鼠标所在方向（FacingClass::SetCurrent）。
// 写操作在联机对局中会导致 desync，仅在单机可用。

namespace Ra2Overlay::InstantTurn
{
    extern std::atomic<bool> Enabled;   // UI 线程写、逻辑线程读，必须原子

    void Register();   // 幂等：将帧回调挂入 GameHooks
    void RenderConfig();
}