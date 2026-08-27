#pragma once

#include <atomic>

// 随处建造（单机功能，Route A / MinHook 风格的 JMP 补丁）：
// 钩住陆地/水域的放置合法性检查点，强制返回"可放置"，无视地形/水域限制。
// 钩子仅在勾选时安装，取消时还原原字节。

namespace Ra2Overlay::BuildEveryWhere
{
    extern std::atomic<bool> Enabled;

    void Register();       // 幂等：仅登记，钩子在 Enable 时安装
    void SetEnabled(bool on);
    void RenderConfig();
}
