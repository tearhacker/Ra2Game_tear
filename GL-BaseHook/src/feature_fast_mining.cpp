#include "pch.h"

#include "feature_fast_mining.h"

#include "game_hooks.h"
#include "log.h"

#include <YRPP.h>

namespace Ra2Overlay::FastMining
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

                // 只管在产的（CashProductionTimer 正在计数）；TimeLeft=0 下次更新即出产。
                building->CashProductionTimer.TimeLeft = 0;
            }
        }
    }

    void Register()
    {
        const bool added = GameHooks::RegisterFrameCallback(&Tick);
        if (added)
        {
            Log::Write("FastMining: frame callback registered");
        }
    }

    void RenderConfig()
    {
        bool enabled = Enabled.load(std::memory_order_relaxed);
        if (ImGui::Checkbox("极速采矿 (SP)", &enabled))
        {
            Enabled.store(enabled, std::memory_order_relaxed);
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("每逻辑帧重置己方矿厂的采集计时器，实现资金快速产出。\n写入操作会导致联机对战同步失败；仅供单人模式使用。");
        }
    }
}