// Demo del pipeline de streaming lock-free: un hilo productor (simulador
// de feed L2, streaming/l2_feed_simulator.hpp) y un hilo consumidor
// (deteccion de anomalias en tiempo real, streaming/streaming_consumer.hpp)
// corriendo concurrentemente, comunicados exclusivamente por el ring
// buffer sin bloqueos de streaming/spsc_ring_buffer.hpp -- sin mutex, sin
// variable de condicion, sin cola del sistema operativo.
//
// Uso: streaming_demo.exe [n_ticks]  (default 5,000,000)

#include <atomic>
#include <chrono>
#include <cstdio>
#include <iostream>
#include <thread>

#include "streaming/l2_feed_simulator.hpp"
#include "streaming/metrics_writer.hpp"
#include "streaming/spsc_ring_buffer.hpp"
#include "streaming/streaming_consumer.hpp"

namespace {

constexpr size_t kRingCapacity = 1 << 16;  // 65,536 slots (65,535 usables)
constexpr size_t kSnapshotIntervalTicks = 500'000;

}  // namespace

int main(int argc, char** argv) {
    size_t n_ticks = 5'000'000;
    if (argc > 1) {
        n_ticks = static_cast<size_t>(std::stoull(argv[1]));
    }

    std::cout << "=== Streaming L2 Tick Pipeline (lock-free SPSC ring buffer) ===\n";
    std::cout << "Ticks a generar: " << n_ticks << "\n";
    std::cout << "Capacidad del ring buffer: " << streaming::SpscRingBuffer<streaming::L2TickEvent, kRingCapacity>::capacity()
               << " slots\n\n";

    const std::string ndjson_path = "outputs/reports/streaming_metrics.ndjson";
    const std::string summary_path = "outputs/reports/streaming_benchmark.json";
    std::remove(ndjson_path.c_str());  // corrida limpia, sin acumular de corridas anteriores

    static streaming::SpscRingBuffer<streaming::L2TickEvent, kRingCapacity> ring;
    std::atomic<bool> producer_done{false};
    size_t full_retries = 0;

    streaming::StreamingConsumer consumer(/*trade_window_ticks=*/500, /*expected_ticks=*/n_ticks);

    auto t_start = std::chrono::steady_clock::now();

    std::thread producer_thread([&]() {
        streaming::FeedSimulatorConfig config{};
        full_retries = streaming::run_feed_simulator(ring, n_ticks, config);
        producer_done.store(true, std::memory_order_release);
    });

    std::thread consumer_thread([&]() {
        streaming::L2TickEvent tick;
        size_t next_snapshot_at = kSnapshotIntervalTicks;

        while (true) {
            if (ring.try_pop(tick)) {
                int64_t consume_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                                          std::chrono::steady_clock::now().time_since_epoch())
                                          .count();
                consumer.on_tick(tick, consume_ns);

                if (consumer.ticks_processed() >= next_snapshot_at) {
                    auto now = std::chrono::steady_clock::now();
                    double elapsed_ms =
                        std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(now - t_start).count();
                    double throughput = elapsed_ms > 0.0
                                             ? static_cast<double>(consumer.ticks_processed()) / (elapsed_ms / 1000.0)
                                             : 0.0;
                    streaming::append_ndjson_snapshot(ndjson_path, consumer.ticks_processed(), elapsed_ms,
                                                        throughput, consumer.alerts_total(), ring.size_approx());
                    next_snapshot_at += kSnapshotIntervalTicks;
                }
            } else if (producer_done.load(std::memory_order_acquire)) {
                // Buffer vacio Y el productor ya no va a generar mas ticks
                // -- no hay nada mas por lo que esperar.
                break;
            }
            // Buffer vacio pero el productor sigue corriendo: reintenta
            // (spin), sin bloquear via el sistema operativo.
        }
    });

    producer_thread.join();
    consumer_thread.join();

    auto t_end = std::chrono::steady_clock::now();
    double elapsed_ms = std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(t_end - t_start).count();
    double throughput = elapsed_ms > 0.0 ? static_cast<double>(consumer.ticks_processed()) / (elapsed_ms / 1000.0) : 0.0;

    auto latency = streaming::compute_latency_percentiles_us(consumer.latency_samples_ns());

    streaming::write_benchmark_summary(summary_path, consumer.ticks_processed(), elapsed_ms, throughput,
                                         consumer.alerts_total(), full_retries, latency);

    std::cout << "=== Resumen ===\n";
    std::cout << "Ticks procesados:        " << consumer.ticks_processed() << "\n";
    std::cout << "Tiempo total:            " << elapsed_ms << " ms\n";
    std::cout << "Throughput:              " << static_cast<long long>(throughput) << " ticks/seg\n";
    std::cout << "Alertas generadas:       " << consumer.alerts_total() << "\n";
    std::cout << "Reintentos buffer lleno: " << full_retries << "\n";
    std::cout << "\n=== Latencia productor -> consumidor (ring buffer) ===\n";
    std::cout << "p50: " << latency.p50_us << " us\n";
    std::cout << "p95: " << latency.p95_us << " us\n";
    std::cout << "p99: " << latency.p99_us << " us\n";
    std::cout << "max: " << latency.max_us << " us\n";
    std::cout << "\nResumen escrito en: " << summary_path << "\n";
    std::cout << "Instantaneas periodicas: " << ndjson_path << "\n";

    return 0;
}
