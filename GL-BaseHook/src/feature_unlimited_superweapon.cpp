#include "pch.h"

#include "feature_unlimited_superweapon.h"

#include "game_hooks.h"
#include "log.h"

#include <YRPP.h>

namespace Ra2Overlay::UnlimitedSuperweapon
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

            auto& supers = *yrpp::SuperClass::Array;
            for (int i = 0; i < supers.Count; ++i)
            {
                yrpp::SuperClass* const super = supers[i];
                if (!super)
                {
                    continue;
                }
                if (super->Owner != player)
                {
                    continue;
                }

                super->IsCharged = true;
                super->RechargeTimer.TimeLeft = 0;
            }
        }
    }

    void Register()
    {
        const bool added = GameHooks::RegisterFrameCallback(&Tick);
        if (added)
        {
            Log::Write("UnlimitedSuperweapon: frame callback registered");
        }
    }

    void RenderConfig()
    {
        bool enabled = Enabled.load(std::memory_order_relaxed);
        if (ImGui::Checkbox("无限超武 (SP)", &enabled))
        {
            Enabled.store(enabled, std::memory_order_relaxed);
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("每逻辑帧将己方所有超级武器充满，随时可发射。\n写入操作会导致联机对战同步失败；仅供单人模式使用。");
        }
    }
}