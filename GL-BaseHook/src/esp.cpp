#include "pch.h"
#include "esp.h"

// YRpp：RA2 1.001 类布局库。只在本文件引入，避免污染其它编译单元。
// 所有用到的成员都是「字段」或「虚函数(vtable 分发)」或「内联方法」，
// 不调用任何 RX(需 Syringe 解析) 的自由函数，因此普通 ASI 直接调用安全。
#include <YRPP.h>

namespace Ra2Overlay::Esp
{
    bool Enabled = false;
    bool ShowHealth = true;
    bool ShowName = true;
    bool EnemyOnly = false;
    float BoxSize = 22.0f;

    namespace
    {
        ImU32 TeamColor(yrpp::HouseClass* owner)
        {
            if (!owner)
                return IM_COL32(150, 150, 150, 255); // 无主 -> 灰
            return IM_COL32(owner->Color.R, owner->Color.G, owner->Color.B, 255);
        }
    }

    void Render()
    {
        if (!Enabled)
            return;

        // 相机 / 投影（0x887324）。菜单或未进对局时可能为 null。
        yrpp::TacticalClass* tac = yrpp::TacticalClass::Instance;
        if (!tac)
            return;

        yrpp::HouseClass* cur = yrpp::HouseClass::CurrentPlayer; // 当前玩家，菜单可能为 null

        ImDrawList* dl = ImGui::GetForegroundDrawList();
        if (!dl)
            return;

        // 遍历全图科技对象（单位/步兵/飞机/建筑）。0xA8EC78，只读。
        // DynamicVectorClass 有效元素数为 Count（非 Capacity），用索引遍历最稳。
        auto& units = *yrpp::TechnoClass::Array;
        for (int i = 0; i < units.Count; ++i)
        {
            auto* obj = units[i];
            if (!obj)
                continue;
            if (obj->IsDead())
                continue;

            yrpp::HouseClass* owner = obj->GetOwningHouse();

            if (EnemyOnly && cur)
            {
                bool isMine = (owner == cur);
                bool allied = owner && cur->IsAlliedWith(owner);
                if (isMine || allied)
                    continue; // 跳过自己与盟友，只留敌方
            }

            // 世界坐标 -> 屏幕像素；不可见（不在屏内/被裁掉）返回 false。
            yrpp::Point2D screen{};
            if (!tac->CoordsToClient(obj->Location, &screen))
                continue;

            // 海拔抬升：把方框画在头顶而不是脚底。
            int zOff = yrpp::TacticalClass::AdjustForZ(obj->GetHeight());

            const float cx = static_cast<float>(screen.X);
            const float cy = static_cast<float>(screen.Y - zOff);
            const float half = BoxSize * 0.5f;

            const ImU32 col = TeamColor(owner);

            // 方框（队伍色）
            dl->AddRect(
                ImVec2(cx - half, cy - half),
                ImVec2(cx + half, cy + half),
                col, 0.0f, 0, 1.5f);

            // 血条（顶部）
            if (ShowHealth)
            {
                double ratio = obj->GetHealthPercentage(); // Health / Strength，内联
                if (ratio < 0.0) ratio = 0.0;
                if (ratio > 1.0) ratio = 1.0;

                const float bw = BoxSize;
                const float by = cy - half - 6.0f;
                dl->AddRectFilled(ImVec2(cx - bw * 0.5f, by),
                                  ImVec2(cx + bw * 0.5f, by + 3.0f),
                                  IM_COL32(0, 0, 0, 180));
                const ImU32 hc = ratio > 0.5
                    ? IM_COL32(0, 200, 0, 255)
                    : (ratio > 0.25 ? IM_COL32(220, 200, 0, 255) : IM_COL32(220, 40, 40, 255));
                dl->AddRectFilled(ImVec2(cx - bw * 0.5f, by),
                                  ImVec2(cx - bw * 0.5f + bw * static_cast<float>(ratio), by + 3.0f),
                                  hc);
            }

            // 类型名（底部）
            if (ShowName)
            {
                const char* id = obj->get_ID(); // 类型 ID 字符串，内联
                if (id && *id)
                    dl->AddText(ImVec2(cx - half, cy + half + 2.0f), col, id);
            }
        }
    }

    void RenderConfig()
    {
        ImGui::Checkbox("Enable ESP", &Enabled);
        ImGui::Checkbox("Show health bar", &ShowHealth);
        ImGui::Checkbox("Show name", &ShowName);
        ImGui::Checkbox("Enemies only", &EnemyOnly);
        ImGui::SliderFloat("Box size", &BoxSize, 10.0f, 60.0f);
        ImGui::Separator();
        ImGui::TextWrapped(
            "ESP is read-only drawing (never writes game state) -> safe in multiplayer. "
            "See anti-cheat doc S9.");
    }
}
