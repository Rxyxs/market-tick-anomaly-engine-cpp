// Simulador de un feed de streaming de Order Book L2 (el productor del
// pipeline lock-free). No implementa el protocolo de red WebSocket en si
// -- este proyecto mantiene la misma politica de cero dependencias
// externas del resto del repo, y una libreria WebSocket real (websocketpp,
// Boost.Beast) rompe esa restriccion -- simula lo que un handler de
// WebSocket real entregaria aguas abajo: una secuencia de eventos L2
// (actualizaciones de bid/ask y trades) generada a la maxima tasa que el
// productor pueda sostener, exactamente el patron productor/consumidor que
// un handler de WebSocket real alimentaria hacia el mismo ring buffer.
//
// Genera un random walk de mid-price con spread bid/ask y una fraccion de
// eventos de trade, reproducible por semilla (mt19937_64, semilla fija),
// consistente con el patron de datos sinteticos reproducibles usado en el
// resto del portafolio. Header-only (como spsc_ring_buffer.hpp): la
// funcion es un template sobre `Capacity`, asi que su definicion tiene que
// ser visible en cada unidad de compilacion que la instancia.
#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <random>

#include "l2_types.hpp"
#include "spsc_ring_buffer.hpp"

namespace streaming {

struct FeedSimulatorConfig {
    uint64_t seed = 42;
    double initial_mid_price = 50000.0;   // ej. BTC/USDT
    double tick_size = 0.5;
    double price_volatility = 2.0;        // desvio estandar del paso del random walk, en ticks
    double half_spread_ticks = 2.0;
    double trade_probability = 0.15;      // fraccion de eventos que son trade en vez de book update
};

// Genera `n_ticks` eventos L2 y los empuja al ring buffer, reintentando
// (spin) mientras el buffer este lleno -- sin bloquear el hilo via el
// sistema operativo, consistente con el diseño lock-free del ring buffer.
// Retorna la cantidad de reintentos por buffer lleno (metrica de
// contencion productor/consumidor).
template <size_t Capacity>
size_t run_feed_simulator(SpscRingBuffer<L2TickEvent, Capacity>& ring, size_t n_ticks,
                           const FeedSimulatorConfig& config = {}) {
    std::mt19937_64 rng(config.seed);
    std::normal_distribution<double> price_step(0.0, config.price_volatility);
    std::uniform_real_distribution<double> unit(0.0, 1.0);
    std::uniform_real_distribution<double> qty_dist(0.001, 2.0);

    double mid_price = config.initial_mid_price;
    size_t full_retries = 0;

    for (size_t i = 0; i < n_ticks; ++i) {
        mid_price += price_step(rng) * config.tick_size;
        if (mid_price <= 0.0) mid_price = config.initial_mid_price;

        L2TickEvent event{};
        event.sequence = static_cast<int64_t>(i);
        event.quantity = qty_dist(rng);

        const bool is_trade = unit(rng) < config.trade_probability;
        if (is_trade) {
            event.event_type = L2EventType::kTrade;
            event.side = (unit(rng) < 0.5) ? Side::kBid : Side::kAsk;
            event.price_level = 0;
            event.price = mid_price;
        } else {
            event.event_type = L2EventType::kBookUpdate;
            const bool is_bid = unit(rng) < 0.5;
            event.side = is_bid ? Side::kBid : Side::kAsk;
            event.price_level = 0;
            double spread_offset = config.half_spread_ticks * config.tick_size;
            event.price = is_bid ? (mid_price - spread_offset) : (mid_price + spread_offset);
        }

        event.producer_timestamp_ns =
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now().time_since_epoch())
                .count();

        while (!ring.try_push(event)) {
            ++full_retries;  // el consumidor no ha drenado a tiempo -- reintenta (spin), no bloquea
        }
    }

    return full_retries;
}

}  // namespace streaming
