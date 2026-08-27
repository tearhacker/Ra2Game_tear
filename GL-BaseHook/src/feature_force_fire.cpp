#include "pch.h"

#include "feature_force_fire.h"

#include "memory_hook.h"
#include "log.h"

#include <YRPP.h>

namespace Ra2Overlay::ForceFire
{
    std::atomic<bool> Enabled{ false };

    namespace
    {
        static void __declspec(naked) __cdecl Detour()
        {
            static const uint32_t jmp_return = 0x00701399u;
            static yrpp::TechnoClass* target;
            __asm {
                mov [target], esi
                pushad
            }
            if (Ra2Overlay::MemHook::ShouldProtect(target)) {
                __asm {
                    popad
                    mov eax, 0xF900
                }
            } else {
                __asm {
                    popad
                    mov eax, ebx
                }
            }
            __asm {
                pop esi
                jmp [jmp_return]
            }
        }

        void Apply()
        {
            MemHook::Install(0x0070138Fu, 7, &Detour);
        }

        void Revert()
        {
            MemHook::Remove(0x0070138Fu);
        }
    }

    void Register()
    {
        Log::Write("ForceFire: registered (hook installed on enable)");
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
        if (ImGui::Checkbox("Force Fire (SP)", &on))
        {
            SetEnabled(on);
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Forces friendly units' shots to hit, bypassing distance/hit checks.\nWrite operations cause desync in online matches; single-player only.");
        }
    }
}
