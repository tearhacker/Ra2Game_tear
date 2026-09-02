#include "pch.h"

#include "feature_build_everywhere.h"

#include "memory_hook.h"
#include "log.h"

#include <YRPP.h>

namespace Ra2Overlay::BuildEveryWhere
{
    std::atomic<bool> Enabled{ false };

    namespace
    {
        // 陆地放置校验：直接返回"允许"（1）。
        static void __declspec(naked) __cdecl DetourGround()
        {
            static const uint32_t jmp_return = 0x004A9063u;
            __asm {
                mov eax, 1
                jmp [jmp_return]
            }
        }

        // 水域放置校验：若是当前玩家建造，强制允许（ecx=3）。
        static void __declspec(naked) __cdecl DetourWater()
        {
            static const uint32_t jmp_back = 0x0047C9CDu + 7u;
            static yrpp::HouseClass* house;
            __asm {
                mov ecx, [esp + 0x24]
                mov [house], ecx
                pushad
            }
            if (Ra2Overlay::MemHook::ShouldProtect(house)) {
                __asm {
                    popad
                    mov ecx, 3
                }
            } else {
                __asm {
                    popad
                    mov ecx, [esp + 0x1C]
                }
            }
            __asm {
                cmp ecx, -1
                jmp [jmp_back]
            }
        }

        void Apply()
        {
            MemHook::Install(0x004A8EB0u, 5, &DetourGround);
            MemHook::Install(0x0047C9CDu, 7, &DetourWater);
        }

        void Revert()
        {
            MemHook::Remove(0x004A8EB0u);
            MemHook::Remove(0x0047C9CDu);
        }
    }

    void Register()
    {
        Log::Write("BuildEveryWhere: registered (hook installed on enable)");
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
        if (ImGui::Checkbox("随处建造 (SP)", &on))
        {
            SetEnabled(on);
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("强制通过放置校验，可在任意地形或水域建造。\n写入操作会导致联机对战同步失败；仅供单人模式使用。");
        }
    }
}
