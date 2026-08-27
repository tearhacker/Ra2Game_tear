#pragma once

#include <atomic>

// 射程拉满（单机功能，Route A / JMP 补丁）：
// 钩住武器射程计算点，对己方单位把射程强制放大到 0xF900（等效全图打击）。

namespace Ra2Overlay::RangeMax
{
    extern std::atomic<bool> Enabled;

    void Register();
    void SetEnabled(bool on);
    void RenderConfig();
}
