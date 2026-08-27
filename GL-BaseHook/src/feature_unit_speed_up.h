#pragma once

namespace Ra2Overlay::UnitSpeedUp
{
    // 注册每逻辑帧回调（提速仅勾选时生效）。
    void Register();

    // 复选框 UI：勾选/取消即时生效。
    void RenderConfig();
}
