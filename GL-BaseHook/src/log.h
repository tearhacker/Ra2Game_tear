#pragma once

#include <Windows.h>

#include <string>
#include <vector>

namespace Ra2Overlay::Log
{
    bool Initialize(HMODULE module);
    void Shutdown();
    void Flush();
    void Write(const char* format, ...);
    std::vector<std::string> Snapshot();
    std::wstring FilePath();
}
