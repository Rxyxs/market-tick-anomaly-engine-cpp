// Escritura de metricas del pipeline de streaming a disco, en dos formas:
//   - `outputs/reports/streaming_metrics.ndjson`: una linea JSON por
//     instantanea periodica durante la corrida (append), para que un
//     consumidor externo (ej. `tools/consume_streaming_metrics.py`) pueda
//     hacer `tail -f` sobre metricas reales mientras el pipeline corre --
//     el equivalente, sin dependencias externas, de un consumidor
//     conectandose al feed de metricas de un servicio real.
//   - `outputs/reports/streaming_benchmark.json`: el resumen final
//     (throughput, percentiles de latencia p50/p95/p99 en microsegundos),
//     escrito una sola vez al terminar -- necesita todas las muestras
//     ordenadas, asi que no puede ser incremental.
#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

namespace streaming {

struct LatencyPercentiles {
    double p50_us = 0.0;
    double p95_us = 0.0;
    double p99_us = 0.0;
    double max_us = 0.0;
};

inline LatencyPercentiles compute_latency_percentiles_us(std::vector<int64_t> samples_ns) {
    LatencyPercentiles result{};
    if (samples_ns.empty()) return result;

    std::sort(samples_ns.begin(), samples_ns.end());
    auto at = [&](double fraction) {
        size_t idx = static_cast<size_t>(fraction * static_cast<double>(samples_ns.size() - 1));
        return static_cast<double>(samples_ns[idx]) / 1000.0;  // ns -> us
    };

    result.p50_us = at(0.50);
    result.p95_us = at(0.95);
    result.p99_us = at(0.99);
    result.max_us = static_cast<double>(samples_ns.back()) / 1000.0;
    return result;
}

inline void append_ndjson_snapshot(const std::string& path, size_t ticks_processed, double elapsed_ms,
                                     double throughput_ticks_per_sec, size_t alerts_total,
                                     size_t ring_buffer_size_approx) {
    std::ofstream out(path, std::ios::app);
    out << "{\"ticks_processed\":" << ticks_processed << ",\"elapsed_ms\":" << elapsed_ms
        << ",\"throughput_ticks_per_sec\":" << throughput_ticks_per_sec << ",\"alerts_total\":" << alerts_total
        << ",\"ring_buffer_size_approx\":" << ring_buffer_size_approx << "}\n";
}

inline void write_benchmark_summary(const std::string& path, size_t n_ticks, double elapsed_ms,
                                      double throughput_ticks_per_sec, size_t alerts_total,
                                      size_t full_retries, const LatencyPercentiles& latency) {
    std::ofstream out(path);
    out << "{\n"
        << "  \"n_ticks\": " << n_ticks << ",\n"
        << "  \"elapsed_ms\": " << elapsed_ms << ",\n"
        << "  \"throughput_ticks_per_sec\": " << throughput_ticks_per_sec << ",\n"
        << "  \"alerts_total\": " << alerts_total << ",\n"
        << "  \"producer_full_buffer_retries\": " << full_retries << ",\n"
        << "  \"ring_to_consumer_latency_us\": {\n"
        << "    \"p50\": " << latency.p50_us << ",\n"
        << "    \"p95\": " << latency.p95_us << ",\n"
        << "    \"p99\": " << latency.p99_us << ",\n"
        << "    \"max\": " << latency.max_us << "\n"
        << "  }\n"
        << "}\n";
}

}  // namespace streaming
