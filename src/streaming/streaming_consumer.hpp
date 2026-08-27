// Consumidor del pipeline de streaming L2: drena el ring buffer y corre
// deteccion de anomalias real (no un no-op) sobre los ticks, reutilizando
// los mismos detectores online del motor batch (EwmaZScore, RollingRatio)
// para que la latencia medida sea la de un consumidor haciendo trabajo
// real, no la de una cola vacia.
//
// Dos senales, cada una con un mecanismo distinto:
//   - z-score EWMA del log-retorno del mid-price, actualizado en cada
//     BookUpdate -- shock de precio a nivel de tick.
//   - razon de rafaga de trades: cada `trade_window_ticks` ticks
//     consumidos, se cuenta cuantos fueron trades y se compara contra el
//     promedio movil de ventanas anteriores (RollingRatio) -- rafaga de
//     actividad de ejecucion, el mismo patron que volume_burst_ratio en
//     el motor batch pero a resolucion de tick en vez de barra de 1 minuto.
#pragma once

#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "../detectors/ewma_zscore.hpp"
#include "../detectors/rolling_ratio.hpp"
#include "l2_types.hpp"

namespace streaming {

struct StreamingMetricsSnapshot {
    size_t ticks_processed = 0;
    size_t alerts_total = 0;
    double elapsed_ms = 0.0;
    double throughput_ticks_per_sec = 0.0;
};

class StreamingConsumer {
public:
    explicit StreamingConsumer(size_t trade_window_ticks = 500, size_t expected_ticks = 0)
        : trade_window_ticks_(trade_window_ticks), price_z_(0.05), trade_burst_ratio_(20) {
        if (expected_ticks > 0) latency_samples_ns_.reserve(expected_ticks);
    }

    // Procesa un tick ya desencolado del ring buffer. `consume_timestamp_ns`
    // debe venir de std::chrono::steady_clock, tomado inmediatamente
    // despues de try_pop, para que la latencia medida sea la del transito
    // productor -> ring buffer -> consumidor, no trabajo de deteccion
    // adicional.
    void on_tick(const L2TickEvent& tick, int64_t consume_timestamp_ns) {
        latency_samples_ns_.push_back(consume_timestamp_ns - tick.producer_timestamp_ns);
        ++ticks_processed_;

        bool is_alert = false;

        if (tick.event_type == L2EventType::kBookUpdate) {
            if (tick.side == Side::kBid) last_bid_ = tick.price;
            else last_ask_ = tick.price;

            if (last_bid_ > 0.0 && last_ask_ > 0.0) {
                double mid = 0.5 * (last_bid_ + last_ask_);
                if (has_prev_mid_ && prev_mid_ > 0.0) {
                    double log_return = std::log(mid / prev_mid_);
                    double z = price_z_.update(log_return);
                    is_alert = is_alert || (std::fabs(z) > kPriceZAlertThreshold);
                }
                prev_mid_ = mid;
                has_prev_mid_ = true;
            }
        } else {
            ++trades_in_window_;
        }

        if (ticks_processed_ % trade_window_ticks_ == 0) {
            double burst_ratio = trade_burst_ratio_.update(static_cast<double>(trades_in_window_));
            trades_in_window_ = 0;
            is_alert = is_alert || (burst_ratio > kTradeBurstAlertThreshold);
        }

        if (is_alert) ++alerts_total_;
    }

    size_t ticks_processed() const { return ticks_processed_; }
    size_t alerts_total() const { return alerts_total_; }
    const std::vector<int64_t>& latency_samples_ns() const { return latency_samples_ns_; }

private:
    static constexpr double kPriceZAlertThreshold = 4.0;
    static constexpr double kTradeBurstAlertThreshold = 3.0;

    size_t trade_window_ticks_;
    EwmaZScore price_z_;
    RollingRatio trade_burst_ratio_;

    double last_bid_ = 0.0;
    double last_ask_ = 0.0;
    double prev_mid_ = 0.0;
    bool has_prev_mid_ = false;
    size_t trades_in_window_ = 0;

    size_t ticks_processed_ = 0;
    size_t alerts_total_ = 0;
    std::vector<int64_t> latency_samples_ns_;
};

}  // namespace streaming
