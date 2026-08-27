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
    ImGui::TextWrapped("Ends the match immediately. Single trigger.");
    if (ImGui::Button("I Win"))
    {
        GameHooks::Submit(ExecuteWin);
    }
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Write operations cause desync in online matches; single-player only.");
    }

    ImGui::SameLine();
    if (ImGui::Button("Make AI Lose"))
    {
        GameHooks::Submit(ExecuteLose);
    }
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Write operations cause desync in online matches; single-player only.");
    }
}