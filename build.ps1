# Compila el motor (market_anomaly_engine.exe) y la suite de pruebas
# (run_tests.exe) con MSVC (cl.exe). Requiere Visual Studio / Build Tools
# con el workload "Desktop development with C++".
#
# Ejecutar desde la raiz del repo: powershell -File build.ps1

$ErrorActionPreference = "Stop"

$vcvarsCandidates = @(
    "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat",
    "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat",
    "C:\Program Files\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
)
$vcvars = $vcvarsCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $vcvars) {
    throw "No se encontro vcvars64.bat. Instala Visual Studio Build Tools (workload 'Desktop development with C++')."
}

$repoRoot = $PSScriptRoot
$outDir = Join-Path $repoRoot "outputs\bin"
New-Item -ItemType Directory -Force -Path $outDir | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $repoRoot "outputs\reports") | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $repoRoot "outputs\figures") | Out-Null

Write-Host "Usando vcvars64.bat: $vcvars"

$engineSources = @(
    "src\main.cpp", "src\csv_reader.cpp", "src\bar_aggregator.cpp", "src\trade_types.cpp",
    "src\svg_writer.cpp", "src\detectors\ewma_zscore.cpp", "src\detectors\ensemble.cpp"
) -join " "

$testSources = @(
    "tests\test_main.cpp", "src\csv_reader.cpp", "src\bar_aggregator.cpp", "src\trade_types.cpp",
    "src\detectors\ewma_zscore.cpp", "src\detectors\ensemble.cpp"
) -join " "

$buildCmd = "call `"$vcvars`" && cd /d `"$repoRoot`" && " +
    "cl /nologo /std:c++17 /O2 /EHsc /Fe:`"$outDir\market_anomaly_engine.exe`" $engineSources && " +
    "cl /nologo /std:c++17 /O2 /EHsc /Fe:`"$outDir\run_tests.exe`" $testSources"

cmd /c $buildCmd
if ($LASTEXITCODE -ne 0) {
    throw "Compilacion fallida (codigo $LASTEXITCODE)"
}

Write-Host "Compilado OK -> $outDir\market_anomaly_engine.exe, $outDir\run_tests.exe"
