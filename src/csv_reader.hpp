// Lector de los archivos CSV de trades historicos reales de Binance
// (formato: trade_id,price,qty,quote_qty,time_ms,is_buyer_maker,is_best_match
// sin fila de encabezado). Parseo manual, sin dependencias externas: el
// formato es fijo y conocido, y evitar un parser CSV generico es
// deliberado para el propósito de rendimiento de este motor.
#pragma once

#include <string>
#include <vector>

#include "trade_types.hpp"

// Lee un archivo de trades y agrega cada registro al vector de salida
// (append, no reemplaza), para poder concatenar varios dias en una sola
// pasada. Retorna la cantidad de filas leidas.
size_t read_trades_csv(const std::string& path, std::vector<Trade>& out_trades);
