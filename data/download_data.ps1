# Descarga los trades reales de BTC/USDT en Binance para el 11-13 de marzo
# de 2020 (el "Jueves Negro" del mercado cripto: BTC cayo de ~US$7.900 a un
# minimo de ~US$3.800 en 24 horas). Fuente publica, sin autenticacion:
# https://data.binance.vision (espejo oficial de datos historicos de Binance).
#
# Ejecutar desde la raiz del repo: powershell -File data\download_data.ps1

$ErrorActionPreference = "Stop"
$dates = @("2020-03-11", "2020-03-12", "2020-03-13")
$rawDir = Join-Path $PSScriptRoot "raw"
New-Item -ItemType Directory -Force -Path $rawDir | Out-Null

foreach ($date in $dates) {
    $csvPath = Join-Path $rawDir "BTCUSDT-trades-$date.csv"
    if (Test-Path $csvPath) {
        Write-Host "Ya existe: $csvPath"
        continue
    }

    $zipUrl = "https://data.binance.vision/data/spot/daily/trades/BTCUSDT/BTCUSDT-trades-$date.zip"
    $zipPath = Join-Path $rawDir "BTCUSDT-trades-$date.zip"
    Write-Host "Descargando $zipUrl ..."
    Invoke-WebRequest -Uri $zipUrl -OutFile $zipPath

    Write-Host "Descomprimiendo $zipPath ..."
    Expand-Archive -Path $zipPath -DestinationPath $rawDir -Force
    Remove-Item $zipPath
}

Write-Host "Datos listos en $rawDir"
