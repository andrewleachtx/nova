cmake --build build --parallel

if ($LASTEXITCODE -ne 0) {
    Write-Host "[ERROR] Build failed - not running" -ForegroundColor Red
    exit 1
}

# ./build/Debug/NOVA.exe "./resources" "./data/test_data.aedat4"
# ./build/Debug/NOVA.exe "./resources" "./data/lights.aedat4"

# ./build/Release/NOVA.exe "./resources" "./data/test_data.aedat4"
./build/Release/NOVA.exe "./resources" "./data/lights.aedat4"