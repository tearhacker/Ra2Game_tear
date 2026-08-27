#include "pch.h"

#include "feature_speed_control.h"

#include "game_hooks.h"
#include "log.h"

#include <YRPP.h>

namespace Ra2Overlay::SpeedControl
{
    std::atomic<bool> PauseEnabled{ false };
    std::atomic<int>  GameSpeed{ 2 };   // 原版默认

    namespace
    {
        constexpr int kMinSpeed = 0;
        constexpr int kMaxSpeed = 5;

        void Tick()
        {
            if (!PauseEnabled.load(std::memory_order_relaxed))
            {
                return;
            }

            // 暂停：每次进入对局都强制写入，保持暂停态。
            yrpp::ScenarioClass* const scenario = yrpp::ScenarioClass::Instance;
            if (scenario)
            {
                scenario->IsGamePaused = true;
            }
        }

        // 调速独立于暂停开关：只要值不等于当前就写，避免每帧无谓写入。
        // 注意：游戏速度字段在 GameModeOptionsClass（0xA8B250，与 SessionClass::Instance->Config 同一内存），
        // 不在 SessionClass 上，编译期已由编译器校验。
        void ApplySpeedTick()
        {
            const int speed = GameSpeed.load(std::memory_order_relaxed);

            yrpp::GameModeOptionsClass& options = yrpp::GameModeOptionsClass::Instance;
            if (options.GameSpeed != speed)
            {
                options.GameSpeed = speed;
            }
        }
    }

    void Register()
    {
        const bool added = GameHooks::RegisterFrameCallback(&Tick);
        const bool addedSpeed = GameHooks::RegisterFrameCallback(&ApplySpeedTick);
        if (added)
        {
            Log::Write("SpeedControl: pause frame callback registered");
        }
        if (addedSpeed)
        {
            Log::Write("SpeedControl: speed frame callback registered");
        }
    }

    void RenderConfig()
    {
        bool paused = PauseEnabled.load(std::memory_order_relaxed);
        if (ImGui::Checkbox("Pause Game (SP)", &paused))
        {
            PauseEnabled.store(paused, std::memory_order_relaxed);
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Forces the paused state every logic frame.\nWrite operations cause desync in online matches; single-player only.");
        }

        int speed = GameSpeed.load(std::memory_order_relaxed);
        if (ImGui::SliderInt("Game Speed (SP)", &speed, kMinSpeed, kMaxSpeed))
        {
            GameSpeed.store(speed, std::memory_order_relaxed);
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("0 = slowest, 5 = fastest. Single-player only.");
        }
    }
}