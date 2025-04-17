# cmake --build build --parallel --config Debug
cmake --build build --parallel --config Release

if ($LASTEXITCODE -ne 0) {
    Write-Host "[ERROR] Build failed - not running" -ForegroundColor Red
    exit 1
}

# ./build/Debug/NOVA.exe "./resources" "./data/1.5V_withoutpapertowel_2400RPM.aedat4"
# Data path needs to be absolute, but it defaults with an invalid path
./build/Release/NOVA.exe "./resources" "C:/nova/data"
