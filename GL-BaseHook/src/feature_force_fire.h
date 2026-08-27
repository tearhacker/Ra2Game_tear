#pragma once

#include <atomic>

// 强制命中（单机功能，Route A / JMP 补丁）：
// 钩住开火命中/距离判定点，对己方单位强制命中（绕过距离/命中校验）。

namespace Ra2Overlay::ForceFire
{
    extern std::atomic<bool> Enabled;

    void Register();
    void SetEnabled(bool on);
    void RenderConfig();
}
