#include "pch.h"

#include "feature_launch_nuke.h"

#include "game_hooks.h"
#include "log.h"

#include <cstring>

// YRpp：与 esp.cpp 同理，仅本文件引入，避免污染其它编译单元。
// 只用字段 / 虚函数(vtable 分发) / 内联方法 / JMP_THIS 方法，不调用任何 RX 自由函数。
#include <YRPP.h>

namespace
{
    // 核弹 (ID: NuclearMissile) 是单次超武，找到己方对应 SuperClass 实例。
    yrpp::SuperClass* FindPlayerNuke(yrpp::HouseClass* player)
    {
        if (!player)
        {
            return nullptr;
        }

        auto& supers = *yrpp::SuperClass::Array;
        for (int i = 0; i < supers.Count; ++i)
        {
            yrpp::SuperClass* const super = supers[i];
            if (!super || super->Owner != player || !super->Type)
            {
                continue;
            }
            if (::_stricmp(super->Type->ID, "NuclearMissile") == 0)
            {
                return super;
            }
        }
        return nullptr;
    }

    // 鼠标在客户区的位置 -> 地图格子；在 UI 面板上时返回 false，不发射。
    bool MouseToCell(yrpp::CellStruct& outCell)
    {
        yrpp::TacticalClass* const tactical = yrpp::TacticalClass::Instance;
        if (!tactical)
        {
            return false;
        }

        const ImVec2& mouse = ImGui::GetIO().MousePos;
        const yrpp::Point2D point{ static_cast<int>(mouse.x), static_cast<int>(mouse.y) };

        // ClientToCoords：像素 -> 世界坐标(lepton)；Coord2Cell：lepton -> 格子。
        const yrpp::CoordStruct coords = tactical->ClientToCoords(point);
        outCell = yrpp::CellClass::Coord2Cell(coords);
        return true;
    }

    void ExecuteLaunchNuke()
    {
        yrpp::HouseClass* const player = yrpp::HouseClass::CurrentPlayer;
        if (!player)
        {
            Ra2Overlay::Log::Write("LaunchNuke: current player not ready");
            return;
        }

        yrpp::SuperClass* const nuke = FindPlayerNuke(player);
        if (!nuke)
        {
            Ra2Overlay::Log::Write("LaunchNuke: no NuclearMissile super weapon for current player");
            return;
        }

        yrpp::CellStruct cell{};
        if (!MouseToCell(cell))
        {
            Ra2Overlay::Log::Write("LaunchNuke: tactical view not ready");
            return;
        }

        // CanFire 先决条件是 IsCharged，直接充满再发射。
        nuke->IsCharged = true;
        nuke->Launch(cell, true);
        Ra2Overlay::Log::Write("LaunchNuke: nuke launched at cell(%d, %d)", cell.X, cell.Y);
    }
}

void Ra2Overlay::LaunchNuke::RenderConfig()
{
    ImGui::TextWrapped("向光标所在的地图格子发射核弹（需当前玩家拥有核弹超级武器）。单次触发。");
    if (ImGui::Button("发射核弹（光标位置）"))
    {
        GameHooks::Submit(ExecuteLaunchNuke);
    }
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("写入操作会导致联机对战同步失败；仅供单人模式使用。");
    }
}