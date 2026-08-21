#include "pch.h"

#include "feature_instant_build.h"

#include "game_hooks.h"
#include "log.h"

#include <YRPP.h>

namespace Ra2Overlay::InstantBuild
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

            auto& factories = *yrpp::FactoryClass::Array;
            for (int i = 0; i < factories.Count; ++i)
            {
                yrpp::FactoryClass* const factory = factories[i];
                if (!factory)
                {
                    continue;
                }
                if (factory->Owner != player)
                {
                    continue;
                }
                if (factory->OnHold || factory->IsSuspended)
                {
                    continue;
                }

                factory->Production.Timer.TimeLeft = 0;
                factory->Production.HasChanged = true;
            }
        }
    }

    void Register()
    {
        const bool added = GameHooks::RegisterFrameCallback(&Tick);
        if (added)
        {
            Log::Write("InstantBuild: frame callback registered");
        }
    }

    void RenderConfig()
    {
        bool enabled = Enabled.load(std::memory_order_relaxed);
        if (ImGui::Checkbox("快速建造（单机）", &enabled))
        {
            Enabled.store(enabled, std::memory_order_relaxed);
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("每逻辑帧将本方工厂的当前生产计时清零，瞬间完成生产。\n写操作在联机对局中会导致 desync，仅限单机使用。");
        }
    }
}
