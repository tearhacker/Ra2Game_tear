@echo off
REM ============================================================================
REM setup_yrpp_link.bat
REM
REM Creates the ASCII junction  third_party\yrpp  ->  ..\分析第三方项目\YRpp
REM
REM WHY THIS EXISTS
REM   The real YRpp directory has a Chinese name. MSVC cl.exe parses its command
REM   line using the console codepage, so /I "..\分析第三方项目\YRpp" fails with
REM   C1083 "cannot open include file YRPP.h" even though the path is correct.
REM   Reaching it through an ASCII junction sidesteps the codepage issue for both
REM   clang-cl and cl.
REM
REM The junction is git-ignored; run this once after cloning, or whenever the
REM real YRpp directory moves. Safe to re-run.
REM ============================================================================
setlocal
cd /d "%~dp0"

set "LINK=%~dp0third_party\yrpp"
set "TARGET=%~dp0..\分析第三方项目\YRpp"

if exist "%LINK%\YRPP.h" (
    echo [ok] junction already present: %LINK%
    goto :done
)

if not exist "%TARGET%\YRPP.h" (
    echo [ERR] real YRpp dir not found: %TARGET%
    echo       Expected YRPP.h inside it. Fix the path in this script.
    exit /b 1
)

REM mklink /J needs no admin rights (unlike /D or /H)
mklink /J "%LINK%" "%TARGET%"
if errorlevel 1 (
    echo [ERR] mklink failed. If third_party\yrpp already exists as a real dir,
    echo       delete it first and re-run.
    exit /b 1
)

:done
if exist "%LINK%\YRPP.h" (
    echo [ok] YRpp reachable at third_party\yrpp
) else (
    echo [ERR] junction does not resolve to YRPP.h
    exit /b 1
)
endlocal
