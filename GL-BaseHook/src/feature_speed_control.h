#pragma once

#include <atomic>

// 暂停 / 调速（单机功能）：
// 暂停：写 ScenarioClass::Instance->IsGamePaused（:139）。
// 调速：写 SessionClass::Instance.GameSpeed（:64，范围 0~5，越界会出逻辑异常）。
// 写操作在联机对局中会导致 desync，仅在单机可用。

namespace Ra2Overlay::SpeedControl
{
    extern std::atomic<bool> PauseEnabled;   // UI 线程写、逻辑线程读，必须原子
    extern std::atomic<int>  GameSpeed;      // 0~5

    void Register();   // 幂等：将帧回调挂入 GameHooks
    void RenderConfig();
}