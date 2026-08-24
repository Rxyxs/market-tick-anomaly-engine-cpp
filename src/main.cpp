// Motor de deteccion de anomalias de mercado en tiempo real (C++17, sin
// dependencias externas). Procesa trades historicos REALES de Binance
// (BTC/USDT, 11-13 de marzo de 2020 — el "Jueves Negro" del mercado
// cripto) y valida el ensamble de 4 detectores contra ese evento real y
// documentado, ya que un dataset de mercado real no trae etiquetas de
// "esto fue una anomalia" como si las trae un dataset de fraude.
//
// Uso: market_anomaly_engine.exe <csv1> <csv2> ... <csvN>
// (los archivos deben venir en orden cronologico)

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <vector>

#include "bar_aggregator.hpp"
#include "csv_reader.hpp"
#include "detectors/ensemble.hpp"
#include "svg_writer.hpp"
#include "trade_types.hpp"

namespace {

constexpr size_t kMinutesPerDay = 1440;

void write_report_csv(const std::string& path, const std::vector<Bar>& bars,
                        const std::vector<AnomalySignal>& signals) {
    std::ofstream out(path);
    out << "minute_utc,close,volume,n_trades,log_return,price_zscore,cusum_signal,"
           "volume_burst_ratio,order_flow_imbalance,imbalance_zscore,ensemble_score_max,"
           "ensemble_score_avg,is_alert\n";
    for (size_t i = 0; i < bars.size(); ++i) {
        const auto& b = bars[i];
        const auto& s = signals[i];
        out << b.minute_utc_string() << "," << b.close << "," << b.volume << "," << b.n_trades << ","
            << s.log_return << "," << s.price_zscore << "," << s.cusum_signal << ","
            << s.volume_burst_ratio << "," << b.order_flow_imbalance() << "," << s.imbalance_zscore
            << "," << s.ensemble_score_max << "," << s.ensemble_score_avg << ","
            << (s.is_alert ? 1 : 0) << "\n";
    }
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Uso: " << argv[0] << " <csv1> [csv2] [csv3] ...\n";
        return 1;
    }

    auto t_start = std::chrono::steady_clock::now();

    std::vector<Trade> trades;
    for (int i = 1; i < argc; ++i) {
        size_t n = read_trades_csv(argv[i], trades);
        std::cout << "  " << argv[i] << ": " << n << " trades\n";
    }
    auto t_parsed = std::chrono::steady_clock::now();

    std::vector<Bar> bars = aggregate_to_minute_bars(trades);
    auto t_aggregated = std::chrono::steady_clock::now();

    AnomalyEnsemble ensemble;
    std::vector<AnomalySignal> signals;
    signals.reserve(bars.size());
    for (const auto& bar : bars) {
        signals.push_back(ensemble.update(bar));
    }
    auto t_detected = std::chrono::steady_clock::now();

    // El primer dia (11 de marzo) es la linea base de mercado tranquilo; el
    // "Jueves Negro" documentado cubre el 12 y 13 de marzo de 2020 (BTC cayo
    // de ~US$7.900 a un minimo de ~US$3.800, ~50% en 24 horas).
    CrashWindow crash_window{kMinutesPerDay, bars.size() - 1, "Jueves Negro documentado (12-13 mar 2020)"};

    size_t n_alerts_total = 0, n_alerts_crash = 0, n_bars_crash = 0;
    for (size_t i = 0; i < bars.size(); ++i) {
        bool in_crash_window = i >= crash_window.start_index;
        if (in_crash_window) n_bars_crash++;
        if (signals[i].is_alert) {
            n_alerts_total++;
            if (in_crash_window) n_alerts_crash++;
        }
    }

    write_report_csv("outputs/reports/bars_with_signals.csv", bars, signals);
    write_price_chart_svg("outputs/figures/price_and_alerts.svg", bars, signals, crash_window);
    write_score_chart_svg("outputs/figures/ensemble_score.svg", signals, 1.0, crash_window);

    auto t_end = std::chrono::steady_clock::now();

    auto ms = [](auto a, auto b) {
        return std::chrono::duration_cast<std::chrono::milliseconds>(b - a).count();
    };

    double base_rate = static_cast<double>(n_bars_crash) / static_cast<double>(bars.size());
    double alert_rate_in_crash =
        n_alerts_total > 0 ? static_cast<double>(n_alerts_crash) / static_cast<double>(n_alerts_total) : 0.0;

    std::cout << "\n=== Resumen ===\n";
    std::cout << "Trades procesados:      " << trades.size() << "\n";
    std::cout << "Barras de 1 minuto:     " << bars.size() << "\n";
    std::cout << "Alertas totales:        " << n_alerts_total << "\n";
    std::cout << "Alertas en ventana de crash documentada: " << n_alerts_crash << " / " << n_alerts_total
               << " (" << (alert_rate_in_crash * 100.0) << "%)\n";
    std::cout << "Base rate (fraccion de minutos que son del crash): " << (base_rate * 100.0) << "%\n";
    std::cout << "Lift (enriquecimiento de alertas en la ventana real de crash): "
               << (base_rate > 0 ? alert_rate_in_crash / base_rate : 0.0) << "x\n";
    std::cout << "\n=== Rendimiento ===\n";
    std::cout << "Parseo CSV:     " << ms(t_start, t_parsed) << " ms ("
               << (trades.size() / std::max<long long>(1, ms(t_start, t_parsed))) << "K trades/seg)\n";
    std::cout << "Agregacion:     " << ms(t_parsed, t_aggregated) << " ms\n";
    std::cout << "Deteccion:      " << ms(t_aggregated, t_detected) << " ms\n";
    std::cout << "Total:          " << ms(t_start, t_end) << " ms\n";

    return 0;
}
