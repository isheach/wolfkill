@echo off
rem =============================================
rem  KsG (KusoGrail) build script - C99/C11
rem  finds gcc in PATH or common w64devkit paths
rem =============================================
setlocal enabledelayedexpansion
cd /d "%~dp0"

if not defined CC (
    set "CC=%KSG_CC%"
)
if not defined CC (
    where gcc >nul 2>nul && set "CC=gcc"
)

if not defined CC (
    for %%D in (
        "%LOCALAPPDATA%\Programs\w64devkit"
        "C:\w64devkit"
        "%USERPROFILE%\w64devkit"
        "%TEMP%\w64devkit"
    ) do (
        if exist "%%~D\bin\gcc.exe" set "CC=%%~D\bin\gcc.exe"
    )
)

if not defined CC (
    echo [ERROR] gcc not found. Install w64devkit:
    echo   https://github.com/skeeto/w64devkit/releases
    exit /b 1
)

rem ensure gcc's own bin dir is on PATH (for as, ld, ...)
for %%I in ("%CC%") do set "CC_DIR=%%~dpI"
set "PATH=%CC_DIR%;%PATH%"

echo [BUILD] %CC%
"%CC%" -std=c11 -O2 -Wall -Wextra -o ksg.exe ksg_sim.c ksg_core.c ksg_battle.c ksg_data.c ksg_ui.c ksg_effects.c ksg_status.c ksg_db.c
if errorlevel 1 (
    echo.
    echo [FAIL] compile error.
    exit /b 1
)

echo.
echo [OK] ksg.exe generated.
echo [RUN] ksg.exe          -- interactive shell
echo       ksg.exe demo     -- auto sandbox demo
endlocal