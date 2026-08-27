#include "pch.h"

#include "feature_range_max.h"

#include "memory_hook.h"
#include "log.h"

#include <YRPP.h>

namespace Ra2Overlay::RangeMax
{
    std::atomic<bool> Enabled{ false };

    namespace
    {
        static void __declspec(naked) __cdecl Detour()
        {
            static const uint32_t jmp_original = 0x006F7248u + 6u;
            static yrpp::TechnoClass* target;
            __asm {
                mov [target], esi
                mov edi, [ebx + 0xB4]
                pushad
            }
            if (Ra2Overlay::MemHook::ShouldProtect(target)) {
                __asm {
                    popad
                    mov edi, 0xF900
                    jmp [jmp_original]
                }
            } else {
                __asm {
                    popad
                    jmp [jmp_original]
                }
            }
        }

        void Apply()
        {
            MemHook::Install(0x006F7248u, 6, &Detour);
        }

        void Revert()
        {
            MemHook::Remove(0x006F7248u);
        }
    }

    void Register()
    {
        Log::Write("RangeMax: registered (hook installed on enable)");
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
        if (ImGui::Checkbox("Max Range (SP)", &on))
        {
            SetEnabled(on);
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Forces the weapon range of friendly units to 0xF900, effectively full-map reach.\nWrite operations cause desync in online matches; single-player only.");
        }
    }
}
