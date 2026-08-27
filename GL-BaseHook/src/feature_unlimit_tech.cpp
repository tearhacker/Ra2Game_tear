#include "pch.h"

#include "feature_unlimit_tech.h"

#include "memory_hook.h"
#include "log.h"

#include <YRPP.h>

namespace Ra2Overlay::UnlimitTech
{
    std::atomic<bool> Enabled{ false };

    namespace
    {
        // 落在 CanBuild 内部：读取 BuildingTypeClass* 参数，强制返回允许（eax=1）。
        static void __declspec(naked) __cdecl Detour()
        {
            static yrpp::BuildingTypeClass* tech;
            static int should_enable;
            __asm {
                mov eax, [esp + 0x4]
                mov [tech], eax
                pushad
            }
            should_enable = static_cast<int>(Ra2Overlay::MemHook::ShouldEnableTech(tech));
            __asm {
                popad
                mov eax, [should_enable]
                ret 0x0C
            }
        }

        void Apply()
        {
            MemHook::Install(0x004F7870u, 7, &Detour);
        }

        void Revert()
        {
            MemHook::Remove(0x004F7870u);
        }
    }

    void Register()
    {
        Log::Write("UnlimitTech: registered (hook installed on enable)");
    }

    void SetEnabled(bool on)
    {
        if (on)
        {
            Apply();
        }
        else
        {
            Revert();
        }
        Enabled.store(on, std::memory_order_relaxed);
    }

    void RenderConfig()
    {
        bool on = Enabled.load(std::memory_order_relaxed);
        if (ImGui::Checkbox("Unlock All Tech (SP)", &on))
        {
            SetEnabled(on);
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Forces HouseClass::CanBuild to return allowed, unlocking all buildings/units.\nWrite operations cause desync in online matches; single-player only.");
        }
    }
}
