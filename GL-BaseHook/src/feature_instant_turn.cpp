#include "pch.h"

#include "feature_instant_turn.h"

#include "game_hooks.h"
#include "log.h"

// YRpp：与 esp.cpp 同理，仅本文件引入，避免污染其它编译单元。
// 只用字段 / 内联方法（FacingClass::SetCurrent）与 JMP_THIS 方法，不调用任何 RX 自由函数。
#include <YRPP.h>

#include <cmath>

namespace Ra2Overlay::InstantTurn
{
    std::atomic<bool> Enabled{ false };

    namespace
    {
        // 鼠标在客户区的位置 -> 世界坐标(lepton)。
        bool MouseToWorld(yrpp::CoordStruct& outCoords)
        {
            yrpp::TacticalClass* const tactical = yrpp::TacticalClass::Instance;
            if (!tactical)
            {
                return false;
            }

            const ImVec2& mouse = ImGui::GetIO().MousePos;
            const yrpp::Point2D point{ static_cast<int>(mouse.x), static_cast<int>(mouse.y) };
            outCoords = tactical->ClientToCoords(point);
            return true;
        }

        // 计算从 from 指向 to 的方向（RA2 坐标系：+X 向东，+Y 向南）。
        yrpp::DirStruct DirectionTo(const yrpp::CoordStruct& from, const yrpp::CoordStruct& to)
        {
            const int dx = to.X - from.X;
            const int dy = to.Y - from.Y;
            // 数学角：东=0，北=+Pi/2（屏幕 y 向下，故 Y 轴取负）。
            const double rad = std::atan2(-static_cast<double>(dy), static_cast<double>(dx));
            yrpp::DirStruct dir;
            dir.SetRadian<65536>(rad);
            return dir;
        }

        void Tick()
        {
            if (!Enabled.load(std::memory_order_relaxed))
            {
                return;
            }

            yrpp::HouseClass* const player = yrpp::HouseClass::CurrentPlayer;
            if (!player)
            {
                return;
            }

            yrpp::CoordStruct mouseWorld{};
            if (!MouseToWorld(mouseWorld))
            {
                return;
            }

            auto& units = *yrpp::TechnoClass::Array;
            for (int i = 0; i < units.Count; ++i)
            {
                yrpp::TechnoClass* const techno = units[i];
                if (!techno || !techno->IsAlive || techno->InLimbo)
                {
                    continue;
                }
                if (techno->GetOwningHouse() != player)
                {
                    continue;
                }

                const yrpp::DirStruct facing = DirectionTo(techno->Location, mouseWorld);
                techno->PrimaryFacing.SetCurrent(facing);
                techno->BarrelFacing.SetCurrent(facing);
            }
        }
    }

    void Register()
    {
        const bool added = GameHooks::RegisterFrameCallback(&Tick);
        if (added)
        {
            Log::Write("InstantTurn: frame callback registered");
        }
    }

    void RenderConfig()
    {
        bool enabled = Enabled.load(std::memory_order_relaxed);
        if (ImGui::Checkbox("炮塔瞬转 (SP)", &enabled))
        {
            Enabled.store(enabled, std::memory_order_relaxed);
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("每逻辑帧将己方单位的炮塔/炮管瞬转朝向光标。\n写入操作会导致联机对战同步失败；仅供单人模式使用。");
        }
    }
}