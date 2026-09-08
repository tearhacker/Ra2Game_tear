#pragma once

namespace Ra2Overlay::DDrawHook
{
    // DirectDraw 演示路径覆盖层：
    // 纯单机 gamemd.exe 走系统 ddraw（IDirectDraw7 主表面 Blt/BltFast/Flip/Unlock），
    // 不会调用 GDI32!SwapBuffers（那是 OpenGL 包装器的路径）。
    // 本钩子捕获 DD 对象与主表面，在每帧演示后用软件渲染绘制 ImGui 菜单。
    // 初始化失败仅记日志（非致命），GL 路径仍然可用。
    bool Initialize();
    void BeginShutdown();
    void Shutdown();
}
