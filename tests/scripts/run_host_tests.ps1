# Run host unit tests (no ESP-IDF required). Exit 0 = pass.
$ErrorActionPreference = "Stop"
$Root = Resolve-Path (Join-Path $PSScriptRoot "..\..")
$Build = Join-Path $Root "tests\host\build"

Write-Host "esp_rtl_sdr host tests - root=$Root"

function Find-CMake {
    $cmd = Get-Command cmake -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }
    $candidates = @(
        "C:\Program Files\CMake\bin\cmake.exe",
        "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe",
        "C:\Program Files\Microsoft Visual Studio\2022\Professional\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe",
        "C:\Program Files\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
    )
    foreach ($c in $candidates) {
        if (Test-Path $c) { return $c }
    }
    return $null
}

$cmake = Find-CMake
if (-not $cmake) {
    Write-Error "cmake not found on PATH or standard install locations"
}

New-Item -ItemType Directory -Force -Path $Build | Out-Null
Push-Location $Build
try {
    & $cmake .. -DCMAKE_BUILD_TYPE=Debug
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $cmake --build . --config Debug
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    $exe = Join-Path $Build "esp_rtl_sdr_host_tests.exe"
    if (-not (Test-Path $exe)) {
        $exe = Join-Path $Build "Debug\esp_rtl_sdr_host_tests.exe"
    }
    if (-not (Test-Path $exe)) {
        $exe = Join-Path $Build "esp_rtl_sdr_host_tests"
    }
    if (-not (Test-Path $exe)) {
        Write-Error "test binary not found under $Build"
    }
    & $exe
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }
    Write-Host "HOST_TESTS_OK"
    exit 0
} finally {
    Pop-Location
}
