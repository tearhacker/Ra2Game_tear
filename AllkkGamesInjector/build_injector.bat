@echo off
REM AllkkGamesInjector x86 build script (single-file exe, Hikari ollvm obfuscated)
REM App sources (main/Menu/injector) compiled with Hikari ollvm clang: string encryption
REM + instruction substitution + control-flow flattening + bogus control flow + indirect branching
REM No naked inline-asm in this project, so app sources use the FULL pass set.
REM imgui third-party sources compiled with plain MSVC cl (no obfuscation, faster build).
setlocal
call "D:\ProgramerDevelop\VS2026\SDK\VC\Auxiliary\Build\vcvarsall.bat" x86 >nul
if errorlevel 1 (
    echo [ERR] vcvarsall x86 init failed
    exit /b 1
)
cd /d "%~dp0"
if not exist bin mkdir bin

REM ==== Hikari ollvm clang (prebuilt, obfuscation passes built-in) ====
set HIKARI_BIN=D:\ProgramerDevelop\clang21_HikariObfuscator_AMD64_Windows\bin
set PATH=%HIKARI_BIN%;%PATH%
set CLANG=%HIKARI_BIN%\clang-cl.exe
if not exist "%CLANG%" (
    echo [ERR] Hikari clang not found: %CLANG%
    exit /b 1
)
REM string encryption + instruction substitution + control-flow flattening + bogus control flow + indirect branching
set OBFFULL=-mllvm -enable-strcry -mllvm -enable-subobf -mllvm -enable-cffobf -mllvm -enable-bcfobf -mllvm -enable-indibran
REM safe set: string encryption + instruction substitution only (use if full passes cause trouble)
set OBFLITE=-mllvm -enable-strcry -mllvm -enable-subobf

set IMGUI=dependency\imgui
set COMMON=/nologo /O2 /EHsc /MT /W3 /utf-8 /std:c++20 /DUNICODE /D_UNICODE /D_CRT_SECURE_NO_WARNINGS /FIpch.h /I.
set APPSRC=main.cpp gui\Menu.cpp injector\injector.cpp
set IMGSRC=%IMGUI%\imgui.cpp %IMGUI%\imgui_draw.cpp %IMGUI%\imgui_tables.cpp %IMGUI%\imgui_widgets.cpp %IMGUI%\backend\imgui_impl_dx9.cpp %IMGUI%\backend\imgui_impl_win32.cpp

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
link /nologo /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup /OUT:bin\potatoInjector.exe bin\main.obj bin\Menu.obj bin\injector.obj bin\imgui.obj bin\imgui_draw.obj bin\imgui_tables.obj bin\imgui_widgets.obj bin\imgui_impl_dx9.obj bin\imgui_impl_win32.obj kernel32.lib user32.lib gdi32.lib imm32.lib d3d9.lib
if errorlevel 1 (
    echo [ERR] link failed
    exit /b 1
)

echo.
echo === Build OK: bin\potatoInjector.exe (app code obfuscated via Hikari ollvm) ===
endlocal
