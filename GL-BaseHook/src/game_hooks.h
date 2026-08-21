#pragma once

#include <functional>
#include <typeinfo>

namespace Ra2Overlay::GameHooks
{
    bool Initialize();
    void Shutdown();
    void Submit(std::function<void()> action);

    // 注册一个「每逻辑帧执行一次」的持续型回调（幂等：同目标重复注册返回 false）。
    // 适用于常驻功能开关，回调内只读现场数据或对游戏对象做帧级刷新。
    bool RegisterFrameCallback(std::function<void()> callback);
    void UnregisterFrameCallback(std::function<void()> callback);
}