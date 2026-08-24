// Tipos de datos compartidos: una transaccion (tick) real de Binance y una
// barra agregada de 1 minuto construida a partir de esos ticks.
#pragma once

#include <cstdint>
#include <string>

struct Trade {
    int64_t timestamp_ms;
    double price;
    double qty;
    bool is_buyer_maker;  // true: el taker fue el vendedor (presion de venta agresiva)
};

struct Bar {
    int64_t minute_epoch;  // minuto desde epoch (timestamp_ms / 60000)
    double open = 0.0;
    double high = 0.0;
    double low = 0.0;
    double close = 0.0;
    double volume = 0.0;
    double taker_buy_volume = 0.0;
    double taker_sell_volume = 0.0;
    int64_t n_trades = 0;

    double order_flow_imbalance() const {
        double total = taker_buy_volume + taker_sell_volume;
        if (total <= 0.0) return 0.0;
        return (taker_buy_volume - taker_sell_volume) / total;
    }

    std::string minute_utc_string() const;
};
