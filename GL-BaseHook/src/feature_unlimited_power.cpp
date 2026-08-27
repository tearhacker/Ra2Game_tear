#include "pch.h"

#include "feature_unlimited_power.h"

#include "game_hooks.h"
#include "log.h"

#include <YRPP.h>

namespace Ra2Overlay::UnlimitedPower
{
    std::atomic<bool> Enabled{ false };

    namespace
    {
        constexpr int kPowerOutput = 99999;

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

            player->PowerOutput = kPowerOutput;
        }
    }

    void Register()
    {
        const bool added = GameHooks::RegisterFrameCallback(&Tick);
        if (added)
        {
            Log::Write("UnlimitedPower: frame callback registered");
        }
    }

    void RenderConfig()
    {
        bool enabled = Enabled.load(std::memory_order_relaxed);
        if (ImGui::Checkbox("Unlimited Power (SP)", &enabled))
        {
            Enabled.store(enabled, std::memory_order_relaxed);
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Raises the current player's power output to %d every logic frame, eliminating power shortage.\nWrite operations cause desync in online matches; single-player only.", kPowerOutput);
        }
    }
}