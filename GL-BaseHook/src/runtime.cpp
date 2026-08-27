#include "pch.h"

#include "feature_infinite_health.h"
#include "feature_instant_build.h"
#include "feature_veterancy_max.h"
#include "feature_unlimited_money.h"
#include "feature_unlimited_power.h"
#include "feature_fast_mining.h"
#include "feature_unlimited_superweapon.h"
#include "feature_unlimited_firepower.h"
#include "feature_instant_turn.h"
#include "feature_auto_repair.h"
#include "feature_speed_control.h"
#include "feature_reveal_map.h"
#include "feature_build_everywhere.h"
#include "feature_unlimit_tech.h"
#include "feature_range_max.h"
#include "feature_force_fire.h"
#include "feature_unit_speed_up.h"
#include "memory_hook.h"
#include "game_hooks.h"
#include "log.h"
#include "render_hook.h"
#include "runtime.h"
#include "ui_shell.h"

namespace
{
    std::atomic_bool g_shutdownRequested{ false };
    HANDLE g_shutdownEvent = nullptr;
    HANDLE g_renderStoppedEvent = nullptr;
}

DWORD WINAPI Ra2Overlay::Runtime::WorkerThread(void* moduleParameter)
{
    const HMODULE module = static_cast<HMODULE>(moduleParameter);
    g_shutdownEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    g_renderStoppedEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);

    Log::Initialize(module);
    Log::Write("Ra2Overlay worker started (Win32/x86)");

    if (!g_shutdownEvent || !g_renderStoppedEvent || !RenderHook::Initialize())
    {
        Log::Write("Initialization failed; unloading module");
        Log::Shutdown();
        if (g_shutdownEvent)
        {
            CloseHandle(g_shutdownEvent);
        }
        if (g_renderStoppedEvent)
        {
            CloseHandle(g_renderStoppedEvent);
        }
        FreeLibraryAndExitThread(module, 1);
    }

    if (!GameHooks::Initialize())
    {
        Log::Write("Game hooks initialization failed; unloading module");
        RenderHook::Shutdown();
        Log::Shutdown();
        if (g_shutdownEvent)
        {
            CloseHandle(g_shutdownEvent);
        }
        if (g_renderStoppedEvent)
        {
            CloseHandle(g_renderStoppedEvent);
        }
        FreeLibraryAndExitThread(module, 1);
    }

    // 注册持续型功能帧回调（GameHooks 已就绪，渲染线程随后启动）。
    Ra2Overlay::InfiniteHealth::Register();
    Ra2Overlay::InstantBuild::Register();
    Ra2Overlay::VeterancyMax::Register();
    Ra2Overlay::UnlimitedMoney::Register();
    Ra2Overlay::UnlimitedPower::Register();
    Ra2Overlay::FastMining::Register();
    Ra2Overlay::UnlimitedSuperweapon::Register();
    Ra2Overlay::UnlimitedFirepower::Register();
    Ra2Overlay::InstantTurn::Register();
    Ra2Overlay::AutoRepair::Register();
    Ra2Overlay::SpeedControl::Register();
    Ra2Overlay::RevealMap::Register();
    Ra2Overlay::BuildEveryWhere::Register();
    Ra2Overlay::UnlimitTech::Register();
    Ra2Overlay::RangeMax::Register();
    Ra2Overlay::ForceFire::Register();
    Ra2Overlay::UnitSpeedUp::Register();

    while (WaitForSingleObject(g_shutdownEvent, 100) == WAIT_TIMEOUT)
    {
        Log::Flush();
    }

    Log::Write("Shutdown requested");
    RenderHook::BeginShutdown();

    if (UiShell::IsInitialized())
    {
        const DWORD waitResult = WaitForSingleObject(g_renderStoppedEvent, 5000);
        if (waitResult != WAIT_OBJECT_0)
        {
            Log::Write("Timed out waiting for render-thread cleanup");
        }
    }

    GameHooks::Shutdown();

    // 还原所有 JMP 补丁钩子（随处建造/科技全开/射程/强制命中），
    // 确保卸载后游戏代码完全恢复原状。
    MemHook::RemoveAll();

    RenderHook::Shutdown();
    Log::Write("Hook removed; unloading module");
    Log::Shutdown();

    CloseHandle(g_renderStoppedEvent);
    CloseHandle(g_shutdownEvent);
    g_renderStoppedEvent = nullptr;
    g_shutdownEvent = nullptr;
    FreeLibraryAndExitThread(module, 0);
}

void Ra2Overlay::Runtime::RequestShutdown()
{
    g_shutdownRequested.store(true, std::memory_order_release);
    if (g_shutdownEvent)
    {
        SetEvent(g_shutdownEvent);
    }
}

bool Ra2Overlay::Runtime::IsShutdownRequested()
{
    return g_shutdownRequested.load(std::memory_order_acquire);
}

void Ra2Overlay::Runtime::NotifyRenderStopped()
{
    if (g_renderStoppedEvent)
    {
        SetEvent(g_renderStoppedEvent);
    }
}
