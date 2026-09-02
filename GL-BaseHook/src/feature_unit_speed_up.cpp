#include "pch.h"

#include "feature_unit_speed_up.h"

#include "game_hooks.h"
#include "log.h"

#include <YRPP.h>
#include <Helpers/Cast.h>

namespace Ra2Overlay::UnitSpeedUp
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

                // 建筑没有 SpeedMultiplier，abstract_cast<FootClass*> 对非
                // Foot 派生（建筑/步兵?）返回 nullptr。仅对移动单位提速。
                yrpp::FootClass* const foot = yrpp::abstract_cast<yrpp::FootClass*>(techno);
                if (!foot)
                {
                    continue;
                }

                switch (techno->WhatAmI())
                {
                case yrpp::AbstractType::Infantry:
                    // 步兵倍率过高会进入"抽搐"状态，参考工程经验值 2.0。
                    foot->SpeedMultiplier = 2.0;
                    break;
                case yrpp::AbstractType::Aircraft:
                    // 飞行器参考工程未处理，取保守值 3.0。
                    foot->SpeedMultiplier = 3.0;
                    break;
                default:
                    // 载具 / 舰船：5.0。
                    foot->SpeedMultiplier = 5.0;
                    break;
                }
            }
        }
    }

    void Register()
    {
        const bool added = GameHooks::RegisterFrameCallback(&Tick);
        if (added)
        {
            Log::Write("UnitSpeedUp: frame callback registered");
        }
    }

    void RenderConfig()
    {
        bool enabled = Enabled.load(std::memory_order_relaxed);
        if (ImGui::Checkbox("单位加速 (SP)", &enabled))
        {
            Enabled.store(enabled, std::memory_order_relaxed);
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("每逻辑帧提升己方所有移动单位速度：载具 5倍、步兵 2倍、飞行器 3倍。\n写入操作会导致联机对战同步失败；仅供单人模式使用。");
        }
    }
}
