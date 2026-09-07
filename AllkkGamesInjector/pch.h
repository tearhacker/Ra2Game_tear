#pragma once

//includes
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <sstream>
#include <thread>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <d3d9.h>

#pragma comment(lib, "d3d9.lib")

#include "injector/injector.hpp"
#include "memory/memory.hpp"
#include "vars/vars.hpp"
#include "utils/utils.hpp"
#include "gui/Menu.hpp"

using namespace std::chrono_literals;
