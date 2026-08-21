#pragma once

#include <Windows.h>

// ESP 绘制模块（只读，不改游戏状态 —— 联机安全，见反作弊文档 §9）
// 读取游戏内存靠 YRpp（仅在 esp.cpp 内 include，隔离编译影响）。
namespace Ra2Overlay::Esp
{
    // 每帧由 UiShell::RenderFrame 调用，往 ImGui 前景绘制层画方框/血条/名称。
    void Render();

    // 由 UiShell 的配置页调用，画开关与参数。
    void RenderConfig();

    // 配置项（UI 通过 RenderConfig 修改）
    extern bool Enabled;     // 总开关
    extern bool ShowHealth;  // 血条
    extern bool ShowName;    // 类型名
    extern bool EnemyOnly;   // 只画敌人（非自己且非盟友）
    extern float BoxSize;    // 方框半边长（像素）
}
