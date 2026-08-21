#include "pch.h"

#include "feature_reveal_map.h"

#include "game_hooks.h"
#include "log.h"

// YRpp：与 esp.cpp 同理，仅本文件引入，避免污染其它编译单元。
// 只用字段 / 虚函数 / 内联方法，不调用任何 RX 自由函数。
#include <YRPP.h>

namespace
{
    std::atomic_bool g_revealed{ false };
    std::atomic_bool g_pending{ false };

    void ExecuteRevealAllMap()
    {
        g_pending.store(false, std::memory_order_release);

        yrpp::MapClass* const map = yrpp::MapClass::Instance;
        yrpp::HouseClass* const player = yrpp::HouseClass::CurrentPlayer;
        if (!map || !player)
        {
            Ra2Overlay::Log::Write("RevealMap: skipped, map or current player not ready");
            return;
        }

        // MapClass::Reveal(0x577D90)：在逻辑线程清除当前玩家全图黑幕/迷雾。
        map->Reveal(player);
        g_revealed.store(true, std::memory_order_release);
        Ra2Overlay::Log::Write("RevealMap: full map revealed for current player");
    }
}

void Ra2Overlay::RevealMap::RequestRevealAllMap()
{
    if (g_revealed.load(std::memory_order_acquire))
    {
        return;
    }
    if (g_pending.exchange(true, std::memory_order_acq_rel))
    {
        return;
    }

    GameHooks::Submit(ExecuteRevealAllMap);
    Log::Write("RevealMap: request queued to logic thread");
}

void Ra2Overlay::RevealMap::RenderConfig()
{
    if (g_revealed.load(std::memory_order_acquire))
    {
        ImGui::TextUnformatted("Map fully revealed for the current player.");
        return;
    }

    if (g_pending.load(std::memory_order_acquire))
    {
        ImGui::TextUnformatted("Reveal pending - will apply on the next logic frame...");
        return;
    }

    ImGui::TextWrapped("Clears shroud and fog of war for the current player across the whole map. One-shot for this session.");
    if (ImGui::Button("Reveal entire map"))
    {
        RequestRevealAllMap();
    }
}