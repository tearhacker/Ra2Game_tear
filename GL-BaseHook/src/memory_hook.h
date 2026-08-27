#pragma once

#include <cstdint>

namespace yrpp
{
    class TechnoClass;
    class HouseClass;
    class BuildingTypeClass;
}

namespace Ra2Overlay::MemHook
{
    // 仅原版 1.001（基址 0x00400000）支持固定 VA 钩子；版本不符拒绝装钩，避免跳进未知地址崩溃。
    bool IsGameVersionSupported();

    // 在 address 处写入 JMP 补丁（patchBytes 字节）跳转到 detour，保存原字节以便还原。
    // 幂等：重复安装返回 true 且不重复保存。版本不符或写保护失败返回 false。
    bool Install(uint32_t address, uint32_t patchBytes, void* detour);
    bool Remove(uint32_t address);
    bool IsInstalled(uint32_t address);
    void RemoveAll();

    // 参考工程 Trainer::ShouldProtect / ShouldEnableTech 的等价实现。
    // 单机场景下「保护 = 属于当前玩家」；科技全开则一律解锁。
    bool ShouldProtect(yrpp::TechnoClass* obj);
    bool ShouldProtect(yrpp::HouseClass* house);
    bool ShouldEnableTech(yrpp::BuildingTypeClass* tech);
}
