@echo off
setlocal

cd /d "%~dp0"
title Favorite Levels Install

set "MODS_DIR=%LOCALAPPDATA%\GeometryDash\geode\mods"
set "SOURCE_FILE=%~1"

if not exist "%MODS_DIR%" (
    echo Geometry Dash Geode mods folder was not found:
    echo %MODS_DIR%
    goto :end
)

if not defined SOURCE_FILE (
    for /f "delims=" %%F in ('dir /b /s /o-d "*.geode" 2^>nul') do (
        set "SOURCE_FILE=%%F"
        goto :found
    )
)

:found
if not defined SOURCE_FILE (
    echo No .geode file was found in this project folder.
    echo Put the built artifact here, or pass the file path as an argument.
    goto :end
)

if not exist "%SOURCE_FILE%" (
    echo File not found:
    echo %SOURCE_FILE%
    goto :end
)

echo Installing:
echo %SOURCE_FILE%
echo.

copy /y "%SOURCE_FILE%" "%MODS_DIR%\" >nul
if errorlevel 1 (
    echo Failed to copy the mod file.
    goto :end
)

echo Installed successfully to:
echo %MODS_DIR%

:end
echo.
pause
endlocal
