#include "pch.h"

#include "feature_iam_winner.h"

#include "game_hooks.h"
#include "log.h"

#include <YRPP.h>

namespace
{
    void ExecuteWin()
    {
        yrpp::HouseClass* const player = yrpp::HouseClass::CurrentPlayer;
        if (!player)
        {
            Ra2Overlay::Log::Write("IamWinner: current player not ready");
            return;
        }
        player->Win(true);
        Ra2Overlay::Log::Write("IamWinner: current player won");
    }

    void ExecuteLose()
    {
        yrpp::HouseClass* const player = yrpp::HouseClass::CurrentPlayer;
        if (!player)
        {
            Ra2Overlay::Log::Write("IamWinner: current player not ready");
            return;
        }
        player->Lose(true);
        Ra2Overlay::Log::Write("IamWinner: current player lost");
    }
}

void Ra2Overlay::IamWinner::RenderConfig()
{
    ImGui::TextWrapped("立即结束本局对战。单次触发。");
    if (ImGui::Button("我胜利"))
    {
        GameHooks::Submit(ExecuteWin);
    }
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("写入操作会导致联机对战同步失败；仅供单人模式使用。");
    }

    ImGui::SameLine();
    if (ImGui::Button("让AI失败"))
    {
        GameHooks::Submit(ExecuteLose);
    }
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("写入操作会导致联机对战同步失败；仅供单人模式使用。");
    }
}