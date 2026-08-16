#include "pch.h"

#include "log.h"

namespace
{
    constexpr size_t kMaxVisibleLines = 512;

    std::mutex g_logMutex;
    std::deque<std::string> g_visibleLines;
    std::vector<std::string> g_pendingLines;
    std::wstring g_filePath;
}

bool Ra2Overlay::Log::Initialize(HMODULE module)
{
    wchar_t modulePath[MAX_PATH]{};
    const DWORD length = GetModuleFileNameW(module, modulePath, MAX_PATH);
    if (length == 0 || length == MAX_PATH)
    {
        return false;
    }

    const std::filesystem::path path(modulePath);
    g_filePath = (path.parent_path() / L"Ra2Overlay.log").wstring();
    Write("Logger initialized");
    return true;
}

void Ra2Overlay::Log::Shutdown()
{
    Flush();
}

void Ra2Overlay::Log::Flush()
{
    std::vector<std::string> pending;
    std::wstring filePath;
    {
        std::lock_guard<std::mutex> lock(g_logMutex);
        if (g_pendingLines.empty() || g_filePath.empty())
        {
            return;
        }
        pending.swap(g_pendingLines);
        filePath = g_filePath;
    }

    std::ofstream output(std::filesystem::path(filePath), std::ios::app);
    if (!output)
    {
        return;
    }

    for (const std::string& line : pending)
    {
        output << line << '\n';
    }
}

void Ra2Overlay::Log::Write(const char* format, ...)
{
    char message[1024]{};
    va_list arguments;
    va_start(arguments, format);
    vsnprintf_s(message, sizeof(message), _TRUNCATE, format, arguments);
    va_end(arguments);

    SYSTEMTIME time{};
    GetLocalTime(&time);

    char line[1200]{};
    snprintf(
        line,
        sizeof(line),
        "[%02u:%02u:%02u.%03u] %s",
        time.wHour,
        time.wMinute,
        time.wSecond,
        time.wMilliseconds,
        message);

    std::lock_guard<std::mutex> lock(g_logMutex);
    g_visibleLines.emplace_back(line);
    g_pendingLines.emplace_back(line);
    while (g_visibleLines.size() > kMaxVisibleLines)
    {
        g_visibleLines.pop_front();
    }
}

std::vector<std::string> Ra2Overlay::Log::Snapshot()
{
    std::lock_guard<std::mutex> lock(g_logMutex);
    return { g_visibleLines.begin(), g_visibleLines.end() };
}

std::wstring Ra2Overlay::Log::FilePath()
{
    std::lock_guard<std::mutex> lock(g_logMutex);
    return g_filePath;
}
