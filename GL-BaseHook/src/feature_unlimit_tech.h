#pragma once

#include <atomic>

// 科技全开（单机功能，Route A / JMP 补丁）：
// 钩住 HouseClass::CanBuild 内部，强制返回"允许"，解锁全部建筑/单位（无视科技等级）。

namespace Ra2Overlay::UnlimitTech
{
    extern std::atomic<bool> Enabled;

    void Register();
    void SetEnabled(bool on);
    void RenderConfig();
}
