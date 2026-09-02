#include "pch.h"

#include "feature_veterancy_max.h"

#include "game_hooks.h"
#include "log.h"

#include <YRPP.h>

namespace Ra2Overlay::VeterancyMax
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

                // 已精英则跳过：避免对建筑等不可升级对象每帧无谓写入。
                if (!techno->Veterancy.IsElite())
                {
                    techno->Veterancy.SetElite();
                }
            }
        }
    }

    void Register()
    {
        const bool added = GameHooks::RegisterFrameCallback(&Tick);
        if (added)
        {
            Log::Write("VeterancyMax: frame callback registered");
        }
    }

    void RenderConfig()
    {
        bool enabled = Enabled.load(std::memory_order_relaxed);
        if (ImGui::Checkbox("满级军衔 (SP)", &enabled))
        {
            Enabled.store(enabled, std::memory_order_relaxed);
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("每逻辑帧将己方所有存活单位与建筑的军衔/经验提升至精英级。\n写入操作会导致联机对战同步失败；仅供单人模式使用。");
        }
    }
}
