#include "pch.h"

#include "feature_auto_repair.h"

#include "game_hooks.h"
#include "log.h"

#include <YRPP.h>

namespace Ra2Overlay::AutoRepair
{
    std::atomic<bool> Enabled{ false };

    namespace
    {
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

            auto& buildings = *yrpp::BuildingClass::Array;
            for (int i = 0; i < buildings.Count; ++i)
            {
                yrpp::BuildingClass* const building = buildings[i];
                if (!building || !building->IsAlive || building->InLimbo)
                {
                    continue;
                }
                if (building->GetOwningHouse() != player)
                {
                    continue;
                }

                const yrpp::ObjectTypeClass* const type = building->GetType();
                if (!type)
                {
                    continue;
                }

                // 只处理受损建筑；满血不干扰。
                if (building->Health >= type->Strength)
                {
                    continue;
                }

                yrpp::StageClass& repair = building->RepairProgress;

                // 激活维修状态机：游戏只在 IsBeingRepaired（:332）置位时才消费 RepairProgress，
                // 否则仅改 stage 不会触发回血。
                building->IsBeingRepaired = true;

                // 维修速率必须非 0，StageClass::Update 才会推进（游戏自身维修流程读取该 stage）。
                if (repair.Rate == 0)
                {
                    repair.Rate = 1;
                }
                repair.Timer.TimeLeft = 0;    // 计时立即到期
                repair.HasChanged = true;     // 标记 stage 变化，驱动游戏维修流程
            }
        }
    }

    void Register()
    {
        const bool added = GameHooks::RegisterFrameCallback(&Tick);
        if (added)
        {
            Log::Write("AutoRepair: frame callback registered");
        }
    }

    void RenderConfig()
    {
        bool enabled = Enabled.load(std::memory_order_relaxed);
        if (ImGui::Checkbox("Auto-Repair (SP)", &enabled))
        {
            Enabled.store(enabled, std::memory_order_relaxed);
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Forces the repair progress of damaged friendly buildings to expire immediately every logic frame, speeding up healing.\nWrite operations cause desync in online matches; single-player only.");
        }
    }
}