# =============================================================================
# build.ps1 — Windows PowerShell One-Click Build & Test Script
# =============================================================================

$ErrorActionPreference = "Stop"

Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host " Building C-Subset Mini-Compiler (Windows PowerShell)" -ForegroundColor Cyan
Write-Host "==========================================================" -ForegroundColor Cyan

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $ScriptDir

# Auto-discover WinGet LLVM-MinGW if clang++/g++ is not in PATH
$wingetBin = Get-ChildItem -Path "$env:LOCALAPPDATA\Microsoft\WinGet\Packages" -Filter "clang++.exe" -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1
if ($wingetBin) {
    $binDir = Split-Path -Parent $wingetBin.FullName
    $env:PATH = "$binDir;$env:PATH"
}

# Check if cmake is available
$hasCMake = Get-Command cmake -ErrorAction SilentlyContinue
$hasGxx = Get-Command g++ -ErrorAction SilentlyContinue
$hasClang = Get-Command clang++ -ErrorAction SilentlyContinue

if ($hasCMake) {
    Write-Host "[INFO] Detected CMake. Configuring build directory..." -ForegroundColor Yellow
    if (!(Test-Path "build")) {
        New-Item -ItemType Directory -Path "build" | Out-Null
    }
    cmake -B build -S .
    Write-Host "[INFO] Compiling mini_compiler..." -ForegroundColor Yellow
    cmake --build build --config Release
} elseif ($hasGxx -or $hasClang) {
    $compiler = if ($hasGxx) { "g++" } else { "clang++" }
    Write-Host "[INFO] Compiling with $compiler -std=c++17 directly..." -ForegroundColor Yellow
    $sources = @(
        "mini_compiler/src/lexer.cpp",
        "mini_compiler/src/parser.cpp",
        "mini_compiler/src/codegen.cpp",
        "mini_compiler/src/vm.cpp",
        "mini_compiler/src/main.cpp"
    )
    & $compiler -std=c++17 -O2 -I mini_compiler/include $sources -o mini_compiler.exe
} else {
    Write-Error "Neither cmake, g++, nor clang++ was found in PATH. Please install MinGW-w64 or Visual Studio C++ build tools."
}

Write-Host "`n[SUCCESS] Build completed successfully." -ForegroundColor Green
Write-Host "`n==========================================================" -ForegroundColor Cyan
Write-Host " Running Automated Verification Testbed via Python" -ForegroundColor Cyan
Write-Host "==========================================================" -ForegroundColor Cyan

$hasPython = Get-Command python -ErrorAction SilentlyContinue
if ($hasPython) {
    python verification/verify_runner.py
} else {
    Write-Warning "Python 3 not detected in PATH. Skipping automated test runner."
}
