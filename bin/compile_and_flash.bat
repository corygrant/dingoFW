@echo off
setlocal

rem Usage: compile_and_flash.bat [BOARD]
rem Default board matches the Makefile default.
rem this script would auto-download pixi
rem but it expects STM32Cube in the default location

set "BOARD=%~1"
if "%BOARD%"=="" set "BOARD=canboard_v2"

rem Repository root is one level above this script.
set "ROOT=%~dp0.."

where pixi >nul 2>nul
if not errorlevel 1 goto build

rem Also find an existing installation before the terminal's PATH is refreshed.
if not defined PIXI_HOME set "PIXI_HOME=%USERPROFILE%\.pixi"
set "PATH=%PIXI_HOME%\bin;%PATH%"
where pixi >nul 2>nul
if not errorlevel 1 goto build

echo Pixi not found. Installing Pixi...
powershell -NoProfile -ExecutionPolicy Bypass -Command "$ErrorActionPreference = 'Stop'; Invoke-RestMethod https://pixi.sh/install.ps1 | Invoke-Expression"
if errorlevel 1 (
    echo Failed to install Pixi.
    exit /b 1
)

where pixi >nul 2>nul
if errorlevel 1 (
    echo Pixi is still unavailable after installation.
    exit /b 1
)

:build
pixi run --manifest-path "%ROOT%\pixi.toml" make -C "%ROOT%" -j12 BOARD=%BOARD%
if errorlevel 1 exit /b %errorlevel%

rem Flash using STM32CubeProgrammer CLI.
rem connect: SWD, mode=UR = connect under reset; -w write, -v verify, -rst reset & run
set CLI=STM32_Programmer_CLI
where %CLI% >nul 2>nul || set CLI="C:\Program Files\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe"

%CLI% -c port=SWD mode=UR -w "%ROOT%\build\%BOARD%.hex" -v -rst
