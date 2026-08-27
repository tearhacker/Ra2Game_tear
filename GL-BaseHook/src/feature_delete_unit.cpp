#include "pch.h"

#include "feature_delete_unit.h"

#include "game_hooks.h"
#include "log.h"

#include <YRPP.h>

namespace
{
    void ExecuteDeleteSelected()
    {
        yrpp::HouseClass* const player = yrpp::HouseClass::CurrentPlayer;
        if (!player)
        {
            Ra2Overlay::Log::Write("DeleteUnit: current player not ready");
            return;
        }

        // 先收集再删除：Limbo 会改变数组，遍历中直接删除会跳过后续元素。
        std::vector<yrpp::TechnoClass*> targets;
        targets.reserve(32);

        auto& units = *yrpp::TechnoClass::Array;
        for (int i = 0; i < units.Count; ++i)
        {
            yrpp::TechnoClass* const techno = units[i];
            if (!techno || techno->InLimbo)
            {
                continue;
            }
            if (techno->GetOwningHouse() != player)
            {
                continue;
            }
            if (!techno->IsSelected)
            {
                continue;
            }
            targets.push_back(techno);
        }

        for (yrpp::TechnoClass* const techno : targets)
        {
            techno->Limbo();
        }

        Ra2Overlay::Log::Write("DeleteUnit: %zu selected unit(s) removed", targets.size());
    }
}

void Ra2Overlay::DeleteUnit::RenderConfig()
{
    ImGui::TextWrapped("Removes the currently selected friendly units (including buildings) from the battlefield. Single trigger.");
    if (ImGui::Button("Remove Selected Units"))
    {
        GameHooks::Submit(ExecuteDeleteSelected);
    }
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Write operations cause desync in online matches; single-player only.");
    }
}