#include "pch.h"

#include "game_hooks.h"

#include "log.h"

// 钩住 LogicClass::Update (0x55AFB0)：每逻辑帧由游戏逻辑线程调用。
// 渲染线程等外部线程通过 Submit() 提交动作，全部在此排队并按帧执行，
// 保证游戏对象/地图相关调用（如 MapClass::Reveal）始终运行在逻辑线程。
namespace
{
    // 原函数为 __thiscall(this, ...)。
    using LogicUpdateFn = void(__thiscall*)(void*);

    void* g_updateTarget = nullptr;
    LogicUpdateFn g_originalUpdate = nullptr;

    std::atomic<int> g_activeCallbacks{ 0 };
    std::mutex g_queueMutex;
    std::deque<std::function<void()>> g_pendingActions;
    std::vector<std::function<void()>> g_frameCallbacks;

    void __fastcall HookLogicUpdate(void* logicObject, void* /*edx*/)
    {
        g_activeCallbacks.fetch_add(1, std::memory_order_acq_rel);

        while (true)
        {
            std::function<void()> action;
            {
                std::lock_guard<std::mutex> lock(g_queueMutex);
                if (g_pendingActions.empty())
                {
                    break;
                }
                action = std::move(g_pendingActions.front());
                g_pendingActions.pop_front();
            }

            // 异常不得逃出钩子：否则会跳过原函数并破坏 g_activeCallbacks 平衡（Shutdown 将死等）。
            try
            {
                action();
            }
            catch (...)
            {
                Ra2Overlay::Log::Write("GameHooks: pending action threw");
            }
        }

        // 持续型帧回调：锁内判空/快照，锁外逐个执行，避免回调内再取锁/注册导致死锁。
        std::vector<std::function<void()>> frameCallbacks;
        {
            std::lock_guard<std::mutex> lock(g_queueMutex);
            if (!g_frameCallbacks.empty())
            {
                frameCallbacks = g_frameCallbacks;
            }
        }
        for (const auto& callback : frameCallbacks)
        {
            try
            {
                callback();
            }
            catch (...)
            {
                Ra2Overlay::Log::Write("GameHooks: frame callback threw");
            }
        }

        if (g_originalUpdate)
        {
            g_originalUpdate(logicObject);
        }

        g_activeCallbacks.fetch_sub(1, std::memory_order_acq_rel);
    }
}

bool Ra2Overlay::GameHooks::Initialize()
{
    g_updateTarget = reinterpret_cast<void*>(0x55AFB0);

    const MH_STATUS createStatus = MH_CreateHook(g_updateTarget, &HookLogicUpdate, reinterpret_cast<void**>(&g_originalUpdate));
    if (createStatus != MH_OK)
    {
        Ra2Overlay::Log::Write("GameHooks: MH_CreateHook(0x55AFB0) failed, status=%u", static_cast<unsigned>(createStatus));
        g_updateTarget = nullptr;
        return false;
    }

    const MH_STATUS enableStatus = MH_EnableHook(g_updateTarget);
    if (enableStatus != MH_OK)
    {
        Ra2Overlay::Log::Write("GameHooks: MH_EnableHook(0x55AFB0) failed, status=%u", static_cast<unsigned>(enableStatus));
        MH_RemoveHook(g_updateTarget);
        g_updateTarget = nullptr;
        return false;
    }

    Ra2Overlay::Log::Write("GameHooks: hooked LogicClass::Update (0x55AFB0)");
    return true;
}

void Ra2Overlay::GameHooks::Shutdown()
{
    if (g_updateTarget)
    {
        MH_DisableHook(g_updateTarget);
        MH_RemoveHook(g_updateTarget);
        g_updateTarget = nullptr;
    }

    // 等待本帧仍在执行的钩子回调退出，避免模块卸载后悬空调用。
    while (g_activeCallbacks.load(std::memory_order_acquire) != 0)
    {
        SwitchToThread();
    }

    g_originalUpdate = nullptr;

    {
        std::lock_guard<std::mutex> lock(g_queueMutex);
        g_pendingActions.clear();
        g_frameCallbacks.clear();
    }

    Ra2Overlay::Log::Write("GameHooks: LogicClass::Update restored");
}

void Ra2Overlay::GameHooks::Submit(std::function<void()> action)
{
    std::lock_guard<std::mutex> lock(g_queueMutex);
    g_pendingActions.push_back(std::move(action));
}

// 判重仅对纯函数指针有效（target<void(*)()>）；带捕获的可调用对象无法按目标去重，
// 调用方须自行保证同目标只注册一次。当前全部调用方均注册 &Tick 函数指针。
bool Ra2Overlay::GameHooks::RegisterFrameCallback(std::function<void()> callback)
{
    std::lock_guard<std::mutex> lock(g_queueMutex);

    const std::type_info& targetType = callback.target_type();
    const void* const target = callback.target<void(*)()>();

    for (const auto& existing : g_frameCallbacks)
    {
        if (existing.target_type() == targetType
            && existing.target<void(*)()>() == target)
        {
            return false;
        }
    }

    g_frameCallbacks.push_back(std::move(callback));
    return true;
}

void Ra2Overlay::GameHooks::UnregisterFrameCallback(std::function<void()> callback)
{
    std::lock_guard<std::mutex> lock(g_queueMutex);

    const std::type_info& targetType = callback.target_type();
    const void* const target = callback.target<void(*)()>();

    g_frameCallbacks.erase(
        std::remove_if(g_frameCallbacks.begin(), g_frameCallbacks.end(),
            [&](const std::function<void()>& existing) {
                return existing.target_type() == targetType
                    && existing.target<void(*)()>() == target;
            }),
        g_frameCallbacks.end());
}