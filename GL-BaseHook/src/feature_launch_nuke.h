#pragma once

// 发射核弹（单机功能）：
// 一次性按钮触发：找到当前玩家的 NuclearMissile 超武，充满能量（IsCharged=true），
// 朝鼠标所在格子 Launch(cell, true)（SuperClass::Launch JMP_THIS 0x6CC390）。
// 写操作在联机对局中会导致 desync，仅在单机可用。

namespace Ra2Overlay::LaunchNuke
{
    void RenderConfig();
}