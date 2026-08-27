@echo off
setlocal
call "D:\ProgramerDevelop\VS2026\SDK\VC\Auxiliary\Build\vcvarsall.bat" x86
if errorlevel 1 (echo [ERR] vcvarsall failed & exit /b 1)
"D:\ProgramerDevelop\VS2026\SDK\MSBuild\Current\Bin\MSBuild.exe" "Ra2Overlay.sln" /noautoresponse /p:Configuration=Release /p:Platform=x86 /p:PlatformToolset=v145 /p:WindowsTargetPlatformVersion=10.0.26100.0 /m /v:minimal
echo [EXIT] %ERRORLEVEL%
