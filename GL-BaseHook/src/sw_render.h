#pragma once

#include "pch.h"

namespace Ra2Overlay::SoftwareRender
{
    // 将 ImGui 帧软件光栅化（CPU 侧 premultiplied RGBA32 DIB），
    // 再经 GdiAlphaBlend 按每像素 alpha 混合到目标 DC。
    // 用于无 GL 上下文的 DirectDraw 演示路径（纯单机 gamemd.exe 走系统 ddraw）。
    bool RenderDrawData(ImDrawData* drawData, HDC targetDc);
}
