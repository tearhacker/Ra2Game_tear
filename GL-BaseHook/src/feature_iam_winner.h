#pragma once

// 我赢了 / 让 AI 输（单机功能）：
// 单次按钮触发：HouseClass::Win(true)（JMP_THIS 0x4FC9E0）/ Lose(true)（0x4FCBD0）。
// 写操作在联机对局中会导致 desync，仅在单机可用。

namespace Ra2Overlay::IamWinner
{
    void RenderConfig();
}