@echo off
REM ============================================================================
REM AllkkGamesInjector x86 build script (single-file exe, Hikari ollvm obfuscated)
REM
REM App sources (main/Menu/injector) compiled with Hikari ollvm clang:
REM   string encryption + instruction substitution + control-flow flattening
REM   + bogus control flow + indirect branching
REM No naked inline-asm in this project, so app sources use the FULL pass set.
REM imgui third-party sources compiled with plain MSVC cl (no obfuscation).
REM
REM NOTE: this file MUST keep CRLF line endings. cmd.exe mis-parses LF-only
REM       batch files, which silently breaks the build. See .gitattributes.
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
if exist "%VCVARS%" (
    call "%VCVARS%" x86 >nul 2>&1
)

REM ---- Step 2: verify; fall back to a hand-built environment -----------------
REM vcvarsall can abort early (e.g. when reg.exe / vswhere is unavailable),
REM leaving INCLUDE/LIB incomplete while still reporting success. Verify for real.
where cl >nul 2>&1
if errorlevel 1 goto :manual_env
if not defined INCLUDE goto :manual_env

REM vcvarsall sometimes sets only the VC paths and never appends the Windows SDK
REM paths (it queries the SDK version via reg.exe). Detect that and patch it up,
REM otherwise <crtdbg.h> / <windows.h> resolve to nothing and the build dies.
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
echo        Set SDK_ROOT / SDK_VER at the top of this script.
exit /b 1

:manual_env
echo [WARN] vcvarsall did not configure the MSVC environment; using fallback.
if not exist "%MSVC_ROOT%\bin\Hostx64\x86\cl.exe" (
    echo [ERR] MSVC compiler not found: %MSVC_ROOT%\bin\Hostx64\x86\cl.exe
    exit /b 1
)
if not exist "%SDK_ROOT%\Include\%SDK_VER%\um\windows.h" (
    echo [ERR] Windows SDK not found: %SDK_ROOT%\Include\%SDK_VER%\um\windows.h
    exit /b 1
)
set "PATH=%MSVC_ROOT%\bin\Hostx64\x86;%SDK_ROOT%\bin\%SDK_VER%\x86;%PATH%"
set "INCLUDE=%MSVC_ROOT%\include;%SDK_ROOT%\Include\%SDK_VER%\ucrt;%SDK_ROOT%\Include\%SDK_VER%\um;%SDK_ROOT%\Include\%SDK_VER%\shared;%SDK_ROOT%\Include\%SDK_VER%\winrt;%SDK_ROOT%\Include\%SDK_VER%\cppwinrt"
set "LIB=%MSVC_ROOT%\lib\x86;%SDK_ROOT%\Lib\%SDK_VER%\ucrt\x86;%SDK_ROOT%\Lib\%SDK_VER%\um\x86"
set "LIBPATH=%LIB%"

:env_ready
echo [env] INCLUDE base: %MSVC_ROOT%\include

if not exist bin mkdir bin

REM ==== Hikari ollvm clang (prebuilt, obfuscation passes built-in) ====
set "PATH=%HIKARI_BIN%;%PATH%"
set "CLANG=%HIKARI_BIN%\clang-cl.exe"
if not exist "%CLANG%" (
    echo [ERR] Hikari clang not found: %CLANG%
    exit /b 1
)

REM string encryption + instruction substitution + control-flow flattening + bogus control flow + indirect branching
set "OBFFULL=-mllvm -enable-strcry -mllvm -enable-subobf -mllvm -enable-cffobf -mllvm -enable-bcfobf -mllvm -enable-indibran"
REM safe set: string encryption + instruction substitution only (use if full passes cause trouble)
set "OBFLITE=-mllvm -enable-strcry -mllvm -enable-subobf"

set "IMGUI=dependency\imgui"
set "COMMON=/nologo /O2 /EHsc /MT /W3 /utf-8 /std:c++20 /DUNICODE /D_UNICODE /D_CRT_SECURE_NO_WARNINGS /FIpch.h /I."
set "APPSRC=main.cpp gui\Menu.cpp injector\injector.cpp"
set "IMGSRC=%IMGUI%\imgui.cpp %IMGUI%\imgui_draw.cpp %IMGUI%\imgui_tables.cpp %IMGUI%\imgui_widgets.cpp %IMGUI%\backend\imgui_impl_dx9.cpp %IMGUI%\backend\imgui_impl_win32.cpp"

echo === Building obfuscated app objects (Hikari ollvm, cffobf may take a while) ===
"%CLANG%" --target=i686-pc-windows-msvc %COMMON% %OBFFULL% -c %APPSRC% /Fo:bin\
if errorlevel 1 (
    echo [ERR] clang-cl obfuscated compile failed
    exit /b 1
)

echo === Building imgui objects (plain cl) ===
cl %COMMON% -c %IMGSRC% /Fo:bin\
if errorlevel 1 (
    echo [ERR] cl compile failed: imgui
    exit /b 1
)

echo === Linking bin\potatoInjector.exe ===
REM /MANIFESTUAC requireAdministrator: game processes spawned by KK/WeGame platform
REM run elevated; a non-elevated injector cannot OpenProcess/CreateRemoteThread on them.
link /nologo /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup /MANIFEST:EMBED /MANIFESTUAC:"level='requireAdministrator' uiAccess='false'" /OUT:bin\potatoInjector.exe bin\main.obj bin\Menu.obj bin\injector.obj bin\imgui.obj bin\imgui_draw.obj bin\imgui_tables.obj bin\imgui_widgets.obj bin\imgui_impl_dx9.obj bin\imgui_impl_win32.obj kernel32.lib user32.lib gdi32.lib imm32.lib d3d9.lib advapi32.lib shell32.lib
if errorlevel 1 (
    echo [ERR] link failed
    exit /b 1
)

echo.
echo === Build OK: bin\potatoInjector.exe (app code obfuscated via Hikari ollvm) ===
endlocal
