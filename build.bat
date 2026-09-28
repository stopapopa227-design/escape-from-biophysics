@echo off
setlocal
cd /d "%~dp0"
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" goto missing
for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSINSTALL=%%i"
if not defined VSINSTALL goto missing
call "%VSINSTALL%\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
if not exist build mkdir build
cl /nologo /std:c++17 /EHsc /O2 /W4 /utf-8 /DUNICODE /D_UNICODE /DNOMINMAX /DWIN32_LEAN_AND_MEAN src\main.cpp /Fo:build\main.obj /Fe:build\EscapeFromBiophysics.exe /link /SUBSYSTEM:WINDOWS opengl32.lib gdi32.lib user32.lib windowscodecs.lib ole32.lib
exit /b %errorlevel%
:missing
echo Install Visual Studio 2022 Build Tools with Desktop development with C++.
exit /b 1
