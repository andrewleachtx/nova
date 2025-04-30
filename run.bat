@echo OFF
setlocal EnableDelayedExpansion

:: Color codes
set "ESC="
for /F "tokens=1,2 delims=#" %%a in ('"prompt #$H#$E# & echo on & for %%b in (1) do rem"') do (
  set "ESC=%%b"
)

:: Colors
set "RED=%ESC%[91m"
set "GREEN=%ESC%[92m"
set "YELLOW=%ESC%[93m"
set "BLUE=%ESC%[94m"
set "MAGENTA=%ESC%[95m"
set "CYAN=%ESC%[96m"
set "RESET=%ESC%[0m"

:: Set build configuration
set BUILD_CONFIG=Debug
::set BUILD_CONFIG=Release

:: Header
echo %MAGENTA%=============================================%RESET%
echo Building NOVA in %YELLOW%%BUILD_CONFIG%%RESET% mode
echo %MAGENTA%=============================================%RESET%
echo.

if not exist "build" (
    echo %YELLOW%Creating build directory...%RESET%
    mkdir build
    cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" -DCMAKE_BUILD_TYPE=Release
) else (
    echo %BLUE%build/ directory found, skipping CMake generation%RESET%
)

echo %GREEN%Running CMake build...%RESET%
cmake --build build --config %BUILD_CONFIG%

if %ERRORLEVEL% EQU 0 (
    echo %GREEN%Build completed successfully!%RESET%
    echo.
    echo %CYAN%Running the application...%RESET%
    
    ::Data path needs to be absolute, but it defaults with an invalid path
    .\build\%BUILD_CONFIG%\nova.exe "./resources" "C:/nova/<data>"
) else (
    echo %RED%Build failed with error code %ERRORLEVEL%%RESET%
    pause
    exit /b %ERRORLEVEL%
)

pause
exit /b 0