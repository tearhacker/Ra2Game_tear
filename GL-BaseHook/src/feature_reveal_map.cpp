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
        if (ImGui::Checkbox("显示全图 (SP)", &enabled))
        {
            Enabled.store(enabled, std::memory_order_relaxed);
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("每逻辑帧为当前玩家揭开全图，永久清除战争迷雾与黑幕。\n写入操作会导致联机对战同步失败；仅供单人模式使用。");
        }
    }
}
