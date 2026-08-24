#include "bar_aggregator.hpp"

#include <algorithm>
#include <map>

std::vector<Bar> aggregate_to_minute_bars(const std::vector<Trade>& trades) {
    std::map<int64_t, Bar> bars_by_minute;

    for (const auto& trade : trades) {
        int64_t minute = trade.timestamp_ms / 60000;
        auto it = bars_by_minute.find(minute);
        if (it == bars_by_minute.end()) {
            Bar bar{};
            bar.minute_epoch = minute;
            bar.open = bar.high = bar.low = bar.close = trade.price;
            it = bars_by_minute.emplace(minute, bar).first;
        }
        Bar& bar = it->second;
        bar.high = std::max(bar.high, trade.price);
        bar.low = std::min(bar.low, trade.price);
        bar.close = trade.price;
        bar.volume += trade.qty;
        bar.n_trades += 1;
        // is_buyer_maker=true implica que el taker (lado agresor) fue el
        // vendedor; is_buyer_maker=false implica que el taker fue el
        // comprador. Es el estandar de clasificacion "lado agresor" en
        // microestructura de mercado.
        if (trade.is_buyer_maker) {
            bar.taker_sell_volume += trade.qty;
        } else {
            bar.taker_buy_volume += trade.qty;
        }
    }

    if (bars_by_minute.empty()) return {};

    // Rellena minutos sin trades con una barra plana (mismo precio, volumen
    // cero), para que los detectores de series de tiempo trabajen sobre una
    // cadencia uniforme de 1 minuto sin huecos.
    std::vector<Bar> bars;
    bars.reserve(bars_by_minute.size());
    int64_t first_minute = bars_by_minute.begin()->first;
    int64_t last_minute = bars_by_minute.rbegin()->first;
    double last_close = bars_by_minute.begin()->second.open;

    for (int64_t minute = first_minute; minute <= last_minute; ++minute) {
        auto it = bars_by_minute.find(minute);
        if (it != bars_by_minute.end()) {
            bars.push_back(it->second);
            last_close = it->second.close;
        } else {
            Bar flat{};
            flat.minute_epoch = minute;
            flat.open = flat.high = flat.low = flat.close = last_close;
            bars.push_back(flat);
        }
    }
    return bars;
}
