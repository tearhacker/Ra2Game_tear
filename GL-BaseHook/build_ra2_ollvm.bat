@echo off
REM ============================================================================
REM GL-BaseHook / Ra2Overlay  x86 DLL  --  Hikari ollvm obfuscated build
REM
REM WHY THIS IS A HYBRID BUILD
REM   YRpp (..\<chinese-dir>\YRpp) is an MSVC-only class library: it relies on
REM   MSVC's non-standard base-class member lookup and locks struct layouts with
REM   sizeof static_asserts. clang cannot compile it (hundreds of errors).
REM   22 of the 32 project sources include <YRPP.h>, so those MUST stay on MSVC.
REM   The other 10 sources (injection / hooking / rendering / UI core) do NOT
REM   touch YRpp and ARE compiled with Hikari -> that is where the obfuscation
REM   value is anyway.
REM
REM PASS SCHEDULE
REM   [1] pch.cpp                  Hikari LITE  (strcry + subobf)
REM   [2] core sources, no __asm   Hikari FULL  (strcry+subobf+cffobf+bcfobf+indibran)
REM   [3] YRpp-dependent sources   plain MSVC cl   (clang cannot parse YRpp)
REM   [4] third_party (imgui/mh)   plain MSVC cl   (no obfuscation, faster)
REM
REM ABI SAFETY (verified before writing this script)
REM   No STL type crosses the Hikari<->MSVC boundary. The only cross-boundary
REM   calls are plain-C entry points such as Log::Write(const char*, ...).
REM   Log::Snapshot() (returns std::vector<std::string>) is called only from
REM   ui_shell.cpp, i.e. both sides are Hikari-compiled -> same toolchain.
REM
REM NOTE: this file MUST keep CRLF line endings AND be saved in the OEM
REM       codepage (GBK/936) because the include path contains Chinese
REM       characters. UTF-8 would be misread by cmd.exe. See .gitattributes.
REM ============================================================================
setlocal enabledelayedexpansion

cd /d "%~dp0"

REM ---- Toolchain locations (single place to edit if VS moves) ----------------
set "VCVARS=D:\ProgramerDevelop\VS2026\SDK\VC\Auxiliary\Build\vcvarsall.bat"
set "MSVC_VER=14.51.36231"
set "MSVC_ROOT=D:\ProgramerDevelop\VS2026\SDK\VC\Tools\MSVC\%MSVC_VER%"
set "SDK_ROOT=D:\Windows Kits\10"
set "SDK_VER=10.0.26100.0"
set "HIKARI_BIN=D:\ProgramerDevelop\clang21_HikariObfuscator_AMD64_Windows\bin"

REM ---- Step 1: try vcvarsall (normal path) -----------------------------------
if exist "%VCVARS%" call "%VCVARS%" x86 >nul 2>&1

REM ---- Step 2: verify; patch or fall back to a hand-built environment --------
where cl >nul 2>&1
if errorlevel 1 goto :manual_env
if not defined INCLUDE goto :manual_env

set "SDK_NEED=%SDK_ROOT%\Include\%SDK_VER%\ucrt"
if not exist "%SDK_NEED%\crtdbg.h" goto :sdk_missing
set "NEED_PATCH=1"
if not "!INCLUDE:%SDK_NEED%=!"=="!INCLUDE!" set "NEED_PATCH="
if defined NEED_PATCH goto :patch_sdk
goto :env_ready

:patch_sdk
echo [WARN] vcvarsall omitted the Windows SDK paths; appending them manually.
set "INCLUDE=%INCLUDE%;%SDK_ROOT%\Include\%SDK_VER%\ucrt;%SDK_ROOT%\Include\%SDK_VER%\um;%SDK_ROOT%\Include\%SDK_VER%\shared;%SDK_ROOT%\Include\%SDK_VER%\winrt;%SDK_ROOT%\Include\%SDK_VER%\cppwinrt"
set "LIB=%LIB%;%SDK_ROOT%\Lib\%SDK_VER%\ucrt\x86;%SDK_ROOT%\Lib\%SDK_VER%\um\x86"
set "LIBPATH=%LIB%"
set "PATH=%SDK_ROOT%\bin\%SDK_VER%\x86;%PATH%"
goto :env_ready

:sdk_missing
echo [ERR] Windows SDK headers not found: %SDK_NEED%\crtdbg.h
exit /b 1

:manual_env
echo [WARN] vcvarsall did not configure the MSVC environment; using fallback.
if not exist "%MSVC_ROOT%\bin\Hostx64\x86\cl.exe" (
    echo [ERR] MSVC compiler not found: %MSVC_ROOT%\bin\Hostx64\x86\cl.exe
    exit /b 1
)
set "PATH=%MSVC_ROOT%\bin\Hostx64\x86;%SDK_ROOT%\bin\%SDK_VER%\x86;%PATH%"
set "INCLUDE=%MSVC_ROOT%\include;%SDK_ROOT%\Include\%SDK_VER%\ucrt;%SDK_ROOT%\Include\%SDK_VER%\um;%SDK_ROOT%\Include\%SDK_VER%\shared;%SDK_ROOT%\Include\%SDK_VER%\winrt;%SDK_ROOT%\Include\%SDK_VER%\cppwinrt"
set "LIB=%MSVC_ROOT%\lib\x86;%SDK_ROOT%\Lib\%SDK_VER%\ucrt\x86;%SDK_ROOT%\Lib\%SDK_VER%\um\x86"
set "LIBPATH=%LIB%"

:env_ready
echo [env] MSVC: %MSVC_ROOT%
echo [env] SDK : %SDK_ROOT%\%SDK_VER%

REM ==== Hikari ollvm clang ====
set "PATH=%HIKARI_BIN%;%PATH%"
set "CLANG=%HIKARI_BIN%\clang-cl.exe"
if not exist "%CLANG%" (
    echo [ERR] Hikari clang not found: %CLANG%
    exit /b 1
)

REM ---- YRpp is reached through an ASCII junction (see setup_yrpp_link.bat) ----
if not exist "third_party\yrpp\YRPP.h" (
    echo [info] YRpp junction missing; creating it...
    call setup_yrpp_link.bat
    if not exist "third_party\yrpp\YRPP.h" (
        echo [ERR] could not resolve third_party\yrpp\YRPP.h
        exit /b 1
    )
)


set "OBJ=build\obj\ollvm"
set "OUTDIR=build\Win32\Release"
set "OUT=%OUTDIR%\Ra2Overlay.dll"
if not exist "%OBJ%" mkdir "%OBJ%"
if not exist "%OUTDIR%" mkdir "%OUTDIR%"

REM ---- obfuscation pass sets ----
set "OBFFULL=-mllvm -enable-strcry -mllvm -enable-subobf -mllvm -enable-cffobf -mllvm -enable-bcfobf -mllvm -enable-indibran"
set "OBFLITE=-mllvm -enable-strcry -mllvm -enable-subobf"

REM ---- compile flags (aligned with vcxproj Release|Win32) ----
REM NOTE: the YRpp path has no spaces, so it is NOT quoted; nested quotes inside
REM       `set "VAR=..."` would break parsing.
set "INC=/I src /I third_party\imgui /I third_party\imgui\backends /I third_party\minhook\include /I third_party\minhook\src /I third_party\atl_stub /I third_party\yrpp"
set "DEF=/DWIN32_LEAN_AND_MEAN /DNOMINMAX /D_CRT_SECURE_NO_WARNINGS /DNDEBUG /DUNICODE /D_UNICODE"
set "CFLAGS=/nologo /c /O2 /EHsc /MT /W4 /utf-8 /std:c++20 /wd4100 /wd4458 /wd4324 /wd4201 /wd4731 /wd4740 /FIpch.h"
set "CLANGFLAGS=--target=i686-pc-windows-msvc %CFLAGS% %INC% %DEF%"
set "MSCFLAGS=%CFLAGS% %INC% %DEF%"

REM ---- [2] Hikari FULL: core sources that do NOT include YRpp ----
set "CORE=src\ddraw_hook.cpp src\diagnostics.cpp src\dllmain.cpp src\game_hooks.cpp src\log.cpp src\render_hook.cpp src\runtime.cpp src\sw_render.cpp src\ui_shell.cpp src\window_bridge.cpp"

REM ---- [3] MSVC: every source that includes YRpp ----
set "YRPP=src\esp.cpp src\memory_hook.cpp src\feature_auto_repair.cpp src\feature_build_everywhere.cpp src\feature_delete_unit.cpp src\feature_fast_mining.cpp src\feature_force_fire.cpp src\feature_iam_winner.cpp src\feature_infinite_health.cpp src\feature_instant_build.cpp src\feature_instant_turn.cpp src\feature_launch_nuke.cpp src\feature_range_max.cpp src\feature_reveal_map.cpp src\feature_speed_control.cpp src\feature_unit_speed_up.cpp src\feature_unlimit_tech.cpp src\feature_unlimited_firepower.cpp src\feature_unlimited_money.cpp src\feature_unlimited_power.cpp src\feature_unlimited_superweapon.cpp src\feature_veterancy_max.cpp"

echo.
echo === [1/4] precompiled header (Hikari LITE) ===
"%CLANG%" %CLANGFLAGS% %OBFLITE% -Fo"%OBJ%\pch.obj" src\pch.cpp
if errorlevel 1 ( echo [ERR] pch compile failed & exit /b 1 )

echo.
echo === [2/4] core sources, obfuscated (Hikari FULL) ===
"%CLANG%" %CLANGFLAGS% %OBFFULL% -Fo"%OBJ%\\" -c %CORE%
if errorlevel 1 ( echo [ERR] Hikari FULL-pass compile failed & exit /b 1 )

echo.
echo === [3/4] YRpp-dependent sources (plain MSVC - clang cannot parse YRpp) ===
cl %MSCFLAGS% -Fo"%OBJ%\\" -c %YRPP%
if errorlevel 1 ( echo [ERR] MSVC compile of YRpp sources failed & exit /b 1 )

echo.
echo === [4/4] third_party imgui + minhook (plain MSVC) ===
REM imgui/minhook must NOT get /FIpch.h (vcxproj marks them PrecompiledHeader=NotUsing).
REM imgui needs IMGUI_DEFINE_MATH_OPERATORS defined before imgui.h.
set "MSCTP=/nologo /c /O2 /EHsc /MT /W4 /utf-8 /std:c++20 %INC% /DWIN32_LEAN_AND_MEAN /DNOMINMAX /D_CRT_SECURE_NO_WARNINGS /DNDEBUG /DIMGUI_DEFINE_MATH_OPERATORS"
cl %MSCTP% -Fo"%OBJ%\\" -c third_party\imgui\imgui.cpp third_party\imgui\imgui_draw.cpp third_party\imgui\imgui_tables.cpp third_party\imgui\imgui_widgets.cpp third_party\imgui\backends\imgui_impl_opengl2.cpp third_party\imgui\backends\imgui_impl_win32.cpp
if errorlevel 1 ( echo [ERR] imgui compile failed & exit /b 1 )

cl /nologo /c /O2 /MT /W4 /utf-8 /D_CRT_SECURE_NO_WARNINGS -Fo"%OBJ%\\" -c third_party\minhook\src\buffer.c third_party\minhook\src\hook.c third_party\minhook\src\trampoline.c third_party\minhook\src\hde\hde32.c
if errorlevel 1 ( echo [ERR] minhook compile failed & exit /b 1 )

echo.
echo === Linking %OUT% ===
link /nologo /DLL /SUBSYSTEM:WINDOWS /MACHINE:X86 /OPT:REF /OPT:ICF /OUT:"%OUT%" "%OBJ%\*.obj" opengl32.lib gdi32.lib user32.lib
if errorlevel 1 ( echo [ERR] link failed & exit /b 1 )

echo.
echo === Build OK: %OUT% ===
echo     core (injection/hook/render/ui) obfuscated via Hikari ollvm
endlocal
