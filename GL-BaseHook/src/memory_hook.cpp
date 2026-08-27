#include "pch.h"

#include "memory_hook.h"

#include <windows.h>
#include <cstdint>
#include <unordered_map>
#include <vector>

#include <YRPP.h>

#include "log.h"

namespace Ra2Overlay::MemHook
{
    namespace
    {
        // address -> 被覆盖的原始字节（用于还原）
        std::unordered_map<uint32_t, std::vector<uint8_t>> g_saved;

        bool GameBaseIsOriginal()
        {
            const HMODULE base = GetModuleHandleA(nullptr);
            return reinterpret_cast<uintptr_t>(base) == 0x00400000u;
        }
    }

    bool IsGameVersionSupported()
    {
        return GameBaseIsOriginal();
    }

    bool Install(uint32_t address, uint32_t patchBytes, void* detour)
    {
        if (!GameBaseIsOriginal())
        {
            Log::Write("MemHook: game base != 0x00400000, refusing hook at %08X", address);
            return false;
        }
        if (patchBytes < 5)
        {
            patchBytes = 5;
        }
        if (g_saved.find(address) != g_saved.end())
        {
            return true; // 已安装，幂等
        }

        // 1) 读出并保存原字节
        std::vector<uint8_t> original(patchBytes);
        {
            DWORD oldProtect = 0;
            if (!VirtualProtect(reinterpret_cast<void*>(address), patchBytes, PAGE_EXECUTE_READWRITE, &oldProtect))
            {
                Log::Write("MemHook: VirtualProtect(RW) failed at %08X", address);
                return false;
            }
            memcpy(original.data(), reinterpret_cast<void*>(address), patchBytes);
            VirtualProtect(reinterpret_cast<void*>(address), patchBytes, oldProtect, &oldProtect);
        }

        // 2) 构造补丁：E9 rel32 + NOP 补齐
        std::vector<uint8_t> patch(patchBytes, 0x90); // 默认 NOP
        patch[0] = 0xE9;                              // JMP rel32
        const int32_t rel = static_cast<int32_t>(
            reinterpret_cast<intptr_t>(detour) - static_cast<intptr_t>(address + 5));
        *reinterpret_cast<int32_t*>(&patch[1]) = rel;

        // 3) 写回补丁
        {
            DWORD oldProtect = 0;
            if (!VirtualProtect(reinterpret_cast<void*>(address), patchBytes, PAGE_EXECUTE_READWRITE, &oldProtect))
            {
                Log::Write("MemHook: VirtualProtect(RW) failed at %08X", address);
                return false;
            }
            memcpy(reinterpret_cast<void*>(address), patch.data(), patchBytes);
            VirtualProtect(reinterpret_cast<void*>(address), patchBytes, oldProtect, &oldProtect);
        }

        g_saved[address] = std::move(original);
        Log::Write("MemHook: installed at %08X (%u bytes)", address, patchBytes);
        return true;
    }

    bool Remove(uint32_t address)
    {
        auto it = g_saved.find(address);
        if (it == g_saved.end())
        {
            return true; // 未安装，幂等
        }
        const std::vector<uint8_t>& original = it->second;
        DWORD oldProtect = 0;
        if (!VirtualProtect(reinterpret_cast<void*>(address), original.size(), PAGE_EXECUTE_READWRITE, &oldProtect))
        {
            Log::Write("MemHook: VirtualProtect(RW) failed at %08X", address);
            return false;
        }
        memcpy(reinterpret_cast<void*>(address), original.data(), original.size());
        VirtualProtect(reinterpret_cast<void*>(address), original.size(), oldProtect, &oldProtect);
        g_saved.erase(it);
        Log::Write("MemHook: removed at %08X", address);
        return true;
    }

    bool IsInstalled(uint32_t address)
    {
        return g_saved.find(address) != g_saved.end();
    }

    void RemoveAll()
    {
        for (const auto& kv : g_saved)
        {
            const uint32_t address = kv.first;
            const std::vector<uint8_t>& original = kv.second;
            DWORD oldProtect = 0;
            if (VirtualProtect(reinterpret_cast<void*>(address), original.size(), PAGE_EXECUTE_READWRITE, &oldProtect))
            {
                memcpy(reinterpret_cast<void*>(address), original.data(), original.size());
                VirtualProtect(reinterpret_cast<void*>(address), original.size(), oldProtect, &oldProtect);
            }
        }
        g_saved.clear();
        Log::Write("MemHook: all hooks removed");
    }

    bool ShouldProtect(yrpp::TechnoClass* obj)
    {
        if (!obj)
        {
            return false;
        }
        yrpp::HouseClass* const me = yrpp::HouseClass::CurrentPlayer;
        if (!me)
        {
            return false;
        }
        return obj->GetOwningHouse() == me;
    }

    bool ShouldProtect(yrpp::HouseClass* house)
    {
        if (!house)
        {
            return false;
        }
        yrpp::HouseClass* const me = yrpp::HouseClass::CurrentPlayer;
        if (!me)
        {
            return false;
        }
        return house == me;
    }

    bool ShouldEnableTech(yrpp::BuildingTypeClass* /*tech*/)
    {
        return true; // 科技全开：解锁全部
    }
}
