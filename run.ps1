cmake --build build --parallel

if ($LASTEXITCODE -ne 0) {
    Write-Host "[ERROR] Build failed - not running" -ForegroundColor Red
    exit 1
}

# ./build/Debug/NOVA.exe "./resources" "./data/test_data.aedat4"
./build/Debug/NOVA.exe "./resources" "./data/toy.aedat"
./build/Debug/NOVA.exe "./resources" "./data/EBBINNOT_AEDAT4/Recording/20180711_Site1_3pm_12mm_01.aedat4"