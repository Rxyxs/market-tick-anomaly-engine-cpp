// Agrega una secuencia de trades crudos (ya ordenados por tiempo, como
// vienen los archivos historicos de Binance) en barras OHLCV de 1 minuto,
// incluyendo el desglose de volumen por lado agresor (taker buy/sell),
// necesario para el detector de desequilibrio de flujo de ordenes.
#pragma once

#include <vector>

#include "trade_types.hpp"

std::vector<Bar> aggregate_to_minute_bars(const std::vector<Trade>& trades);
