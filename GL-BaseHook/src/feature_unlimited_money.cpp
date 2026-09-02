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
        if (ImGui::Checkbox("无限金钱 (SP)", &enabled))
        {
            Enabled.store(enabled, std::memory_order_relaxed);
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("每逻辑帧为当前玩家增加 %d 资金。\n写入操作会导致联机对战同步失败；仅供单人模式使用。", kPerFrameAmount);
        }
    }
}