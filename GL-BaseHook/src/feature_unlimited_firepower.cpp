#include "pch.h"

#include "feature_unlimited_firepower.h"

#include "game_hooks.h"
#include "log.h"

#include <YRPP.h>

namespace Ra2Overlay::UnlimitedFirepower
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

                const yrpp::TechnoTypeClass* const type = techno->GetTechnoType();
                if (!type)
                {
                    continue;
                }

                // 装填清零：所有可开火对象都走 ReloadTimer。
                techno->ReloadTimer.TimeLeft = 0;

                // 弹药补满：只有类型带弹夹上限（Ammo>0）的单位才消耗弹药。
                if (type->Ammo > 0 && techno->Ammo < type->Ammo)
                {
                    techno->Ammo = type->Ammo;
                }
            }
        }
    }

    void Register()
    {
        const bool added = GameHooks::RegisterFrameCallback(&Tick);
        if (added)
        {
            Log::Write("UnlimitedFirepower: frame callback registered");
        }
    }

    void RenderConfig()
    {
        bool enabled = Enabled.load(std::memory_order_relaxed);
        if (ImGui::Checkbox("无限火力 (SP)", &enabled))
        {
            Enabled.store(enabled, std::memory_order_relaxed);
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("每逻辑帧清空己方所有单位的装填计时器并补满弹药，消除开火冷却。\n写入操作会导致联机对战同步失败；仅供单人模式使用。");
        }
    }
}