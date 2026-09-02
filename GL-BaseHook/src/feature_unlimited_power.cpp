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
        if (ImGui::Checkbox("无限电力 (SP)", &enabled))
        {
            Enabled.store(enabled, std::memory_order_relaxed);
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("每逻辑帧将当前玩家电力产量提升至 %d，消除电力短缺。\n写入操作会导致联机对战同步失败；仅供单人模式使用。", kPowerOutput);
        }
    }
}