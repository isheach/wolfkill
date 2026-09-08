@echo off
rem KsG (Kuso Grail) Godot launcher
rem Usage: double-click run.bat, or run from cmd

setlocal
cd /d "%~dp0"

rem --- 1) Locate Godot ---
set "GODOT_EXE=D:\Godot_v4.7.2-stable_mono_win64\Godot_v4.7.2-stable_mono_win64\Godot_v4.7.2-stable_mono_win64.exe"
if not exist "%GODOT_EXE%" (
  if defined GODOT_BIN set "GODOT_EXE=%GODOT_BIN%"
)
if not exist "%GODOT_EXE%" goto nolocate

set "DOTNET_ROOT=%USERPROFILE%\.dotnet"
set "PATH=%USERPROFILE%\.dotnet;%PATH%"

echo [run] dotnet build ...
call dotnet build KsgGodot.csproj
if errorlevel 1 goto buildfail

echo [run] Starting Godot...
start "" "%GODOT_EXE%" --path "%CD%"
echo [run] Godot launched. Enjoy!
goto end

:nolocate
echo [ERROR] Godot not found: %GODOT_EXE%
echo Edit GODOT_EXE at the top of this file, or set the GODOT_BIN environment variable.
goto end

:buildfail
echo [ERROR] C# build failed. Godot cannot load scripts.
echo Set DOTNET_ROOT and re-run. See README for details.

:end
endlocal