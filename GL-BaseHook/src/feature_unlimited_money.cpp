#include "pch.h"

#include "feature_unlimited_money.h"

#include "game_hooks.h"
#include "log.h"

#include <YRPP.h>

namespace Ra2Overlay::UnlimitedMoney
{
    std::atomic<bool> Enabled{ false };

    namespace
    {
        constexpr int kPerFrameAmount = 1000;

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

            // GiveMoney 走游戏内经济结算（JMP_THIS 真实地址），比直写 Balance 更稳妥。
            player->GiveMoney(kPerFrameAmount);
        }
    }

    void Register()
    {
        const bool added = GameHooks::RegisterFrameCallback(&Tick);
        if (added)
        {
            Log::Write("UnlimitedMoney: frame callback registered");
        }
    }

    void RenderConfig()
    {
        bool enabled = Enabled.load(std::memory_order_relaxed);
        if (ImGui::Checkbox("Unlimited Money (SP)", &enabled))
        {
            Enabled.store(enabled, std::memory_order_relaxed);
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Adds %d credits to the current player every logic frame.\nWrite operations cause desync in online matches; single-player only.", kPerFrameAmount);
        }
    }
}