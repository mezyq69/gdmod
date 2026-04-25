@echo off
setlocal

cd /d "%~dp0"
title Favorite Levels Build

echo [1/3] Checking Geode CLI...
where geode >nul 2>nul
if errorlevel 1 (
    echo Geode CLI was not found in PATH.
    echo Install or add Geode CLI, then try again.
    goto :end
)

echo [2/3] Checking C++ build tools...
where cl >nul 2>nul
if errorlevel 1 (
    set "VCVARS="

    if exist "%ProgramFiles%\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" set "VCVARS=%ProgramFiles%\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
    if not defined VCVARS if exist "%ProgramFiles%\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" set "VCVARS=%ProgramFiles%\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
    if not defined VCVARS if exist "%ProgramFiles%\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat" set "VCVARS=%ProgramFiles%\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat"
    if not defined VCVARS if exist "%ProgramFiles%\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat" set "VCVARS=%ProgramFiles%\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat"
    if not defined VCVARS if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvars64.bat" set "VCVARS=%ProgramFiles(x86)%\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
    if not defined VCVARS if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat" set "VCVARS=%ProgramFiles(x86)%\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat"

    if defined VCVARS (
        echo Loading MSVC environment...
        call "%VCVARS%" >nul
    )
)

where cl >nul 2>nul
if errorlevel 1 (
    echo C++ compiler was not found.
    echo Install Visual Studio Build Tools with Desktop development with C++.
    goto :end
)

echo [3/3] Building mod...
geode build --ninja

echo.
if errorlevel 1 (
    echo Build failed.
) else (
    echo Build completed successfully.
    echo Check the build folder for the generated .geode file.
)

:end
echo.
pause
endlocal
