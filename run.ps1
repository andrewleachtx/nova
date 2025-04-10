# cmake --build build --parallel --config Debug
cmake --build build --parallel --config Release

if ($LASTEXITCODE -ne 0) {
    Write-Host "[ERROR] Build failed - not running" -ForegroundColor Red
    exit 1
}

./build/Debug/NOVA.exe "./resources" "./data/"
./build/Release/NOVA.exe "./resources" "./data/"
