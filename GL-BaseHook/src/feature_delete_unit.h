#pragma once

// 删除选中单位（单机功能）：
// 单次按钮触发：遍历全图科技对象，把当前玩家选中（ObjectClass::IsSelected）的单位
// 调用 TechnoClass::Limbo()（JMP_THIS 0x6F6AC0）移出战场。
// 写操作在联机对局中会导致 desync，仅在单机可用。

namespace Ra2Overlay::DeleteUnit
{
    void RenderConfig();
}