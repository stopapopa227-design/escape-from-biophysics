@echo off
cd /d "%~dp0"
if not exist build\EscapeFromBiophysics.exe call build.bat
if not exist build\EscapeFromBiophysics.exe (
  pause
  exit /b 1
)
start "" build\EscapeFromBiophysics.exe
