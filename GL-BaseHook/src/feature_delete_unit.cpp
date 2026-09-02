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
    ImGui::TextWrapped("将当前选中的己方单位（含建筑）从战场移除。单次触发。");
    if (ImGui::Button("移除选中单位"))
    {
        GameHooks::Submit(ExecuteDeleteSelected);
    }
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("写入操作会导致联机对战同步失败；仅供单人模式使用。");
    }
}