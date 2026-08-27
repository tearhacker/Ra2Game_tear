#include "pch.h"

#include "feature_reveal_map.h"

#include "game_hooks.h"
#include "log.h"

// YRpp：与 esp.cpp 同理，仅本文件引入，避免污染其它编译单元。
// 只用字段 / 虚函数 / 内联方法，不调用任何 RX 自由函数。
#include <YRPP.h>

namespace Ra2Overlay::RevealMap
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

            yrpp::MapClass* const map = yrpp::MapClass::Instance;
            yrpp::HouseClass* const player = yrpp::HouseClass::CurrentPlayer;
            if (!map || !player)
            {
                return;
            }

            // 每逻辑帧重新揭图，维持"永久开图"。
            // 一次性揭图会被游戏的迷雾恢复逻辑覆盖，故需持续调用。
            map->Reveal(player);
        }
    }

    void Register()
    {
        const bool added = GameHooks::RegisterFrameCallback(&Tick);
        if (added)
        {
            Log::Write("RevealMap: per-frame callback registered");
        }
    }

    void RenderConfig()
    {
        bool enabled = Enabled.load(std::memory_order_relaxed);
        if (ImGui::Checkbox("Reveal Map (SP)", &enabled))
        {
            Enabled.store(enabled, std::memory_order_relaxed);
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Reveals the entire map for the current player every logic frame, keeping fog and shroud cleared permanently.\nWrite operations cause desync in online matches; single-player only.");
        }
    }
}
