#include "pch.h"

#include "feature_infinite_health.h"

#include "game_hooks.h"
#include "log.h"

#include <YRPP.h>

namespace Ra2Overlay::InfiniteHealth
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

                const yrpp::ObjectTypeClass* const type = techno->GetType();
                if (!type)
                {
                    continue;
                }

                // 只在不满时补满：避免把 Health > Strength 的异常状态单位写小。
                if (techno->Health < type->Strength)
                {
                    techno->Health = type->Strength;
                }
            }
        }
    }

    void Register()
    {
        const bool added = GameHooks::RegisterFrameCallback(&Tick);
        if (added)
        {
            Log::Write("InfiniteHealth: frame callback registered");
        }
    }

    void RenderConfig()
    {
        bool enabled = Enabled.load(std::memory_order_relaxed);
        if (ImGui::Checkbox("无限生命 (SP)", &enabled))
        {
            Enabled.store(enabled, std::memory_order_relaxed);
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("每逻辑帧将己方所有存活单位与建筑的血量补满。\n写入操作会导致联机对战同步失败；仅供单人模式使用。");
        }
    }
}
