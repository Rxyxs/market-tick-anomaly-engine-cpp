// Suite de pruebas liviana, sin frameworks externos (Catch2/GoogleTest
// habrian requerido un gestor de paquetes que esta maquina no tiene
// configurado para C++). Cada TEST_CASE es una funcion que usa las macros
// de asercion de abajo; el runner al final reporta el resultado.

#include <cmath>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <vector>

#include "../src/bar_aggregator.hpp"
#include "../src/csv_reader.hpp"
#include "../src/detectors/cusum.hpp"
#include "../src/detectors/ensemble.hpp"
#include "../src/detectors/ewma_zscore.hpp"
#include "../src/detectors/rolling_ratio.hpp"

static int g_failures = 0;
static int g_checks = 0;

#define ASSERT_TRUE(cond)                                                              \
    do {                                                                               \
        g_checks++;                                                                    \
        if (!(cond)) {                                                                 \
            g_failures++;                                                              \
            std::cerr << "FAIL " << __FUNCTION__ << " (" << __LINE__ << "): " #cond "\n"; \
        }                                                                               \
    } while (0)

#define ASSERT_NEAR(a, b, tol)                                                                        \
    do {                                                                                               \
        g_checks++;                                                                                    \
        double diff = std::fabs((a) - (b));                                                             \
        if (diff > (tol)) {                                                                             \
            g_failures++;                                                                               \
            std::cerr << "FAIL " << __FUNCTION__ << " (" << __LINE__ << "): |" #a " - " #b "| = " << diff \
                      << " > " << tol << "\n";                                                           \
        }                                                                                                \
    } while (0)

void test_csv_reader_parses_fields() {
    const std::string path = "test_tmp_trades.csv";
    {
        std::ofstream f(path);
        f << "1,100.50,0.25,25.125,1583884800000,True,True\n";
        f << "2,101.00,0.10,10.100,1583884800500,False,True\n";
    }

    std::vector<Trade> trades;
    size_t n = read_trades_csv(path, trades);
    std::remove(path.c_str());

    ASSERT_TRUE(n == 2);
    ASSERT_NEAR(trades[0].price, 100.50, 1e-9);
    ASSERT_NEAR(trades[0].qty, 0.25, 1e-9);
    ASSERT_TRUE(trades[0].timestamp_ms == 1583884800000LL);
    ASSERT_TRUE(trades[0].is_buyer_maker == true);
    ASSERT_TRUE(trades[1].is_buyer_maker == false);
}

void test_bar_aggregator_ohlcv() {
    std::vector<Trade> trades = {
        {1583884800000LL, 100.0, 1.0, false},  // minuto 0, taker compra
        {1583884800500LL, 105.0, 2.0, false},  // minuto 0, taker compra
        {1583884830000LL, 95.0, 0.5, true},    // minuto 0, taker vende
        {1583884860000LL, 110.0, 1.0, false},  // minuto 1
    };

    std::vector<Bar> bars = aggregate_to_minute_bars(trades);
    ASSERT_TRUE(bars.size() == 2);

    const Bar& b0 = bars[0];
    ASSERT_NEAR(b0.open, 100.0, 1e-9);
    ASSERT_NEAR(b0.high, 105.0, 1e-9);
    ASSERT_NEAR(b0.low, 95.0, 1e-9);
    ASSERT_NEAR(b0.close, 95.0, 1e-9);
    ASSERT_TRUE(b0.n_trades == 3);
    ASSERT_NEAR(b0.taker_buy_volume, 3.0, 1e-9);
    ASSERT_NEAR(b0.taker_sell_volume, 0.5, 1e-9);
}

void test_bar_aggregator_fills_gaps() {
    std::vector<Trade> trades = {
        {1583884800000LL, 100.0, 1.0, false},  // minuto 0
        {1583884980000LL, 100.0, 1.0, false},  // minuto 3 (minutos 1 y 2 sin trades)
    };
    std::vector<Bar> bars = aggregate_to_minute_bars(trades);
    ASSERT_TRUE(bars.size() == 4);
    ASSERT_TRUE(bars[1].n_trades == 0);
    ASSERT_NEAR(bars[1].close, 100.0, 1e-9);  // barra plana al ultimo cierre conocido
}

void test_ewma_zscore_stable_series_near_zero() {
    EwmaZScore z(0.1);
    double last = 0.0;
    for (int i = 0; i < 200; ++i) last = z.update(0.001);  // serie casi constante
    ASSERT_TRUE(std::fabs(last) < 2.0);
}

void test_ewma_zscore_flags_a_spike() {
    EwmaZScore z(0.1);
    for (int i = 0; i < 200; ++i) z.update(0.0);  // establece media/varianza ~0
    double spike_z = z.update(1.0);  // shock grande relativo a la varianza casi nula previa
    ASSERT_TRUE(std::fabs(spike_z) > 5.0);
}

void test_cusum_flags_sustained_drift() {
    Cusum cusum(0.5, 5.0);
    double last = 0.0;
    for (int i = 0; i < 30; ++i) last = cusum.update(1.0);  // deriva sostenida de +1 sigma
    ASSERT_TRUE(last >= 1.0);  // supero el umbral normalizado
}

void test_cusum_does_not_flag_noise_around_zero() {
    Cusum cusum(0.5, 5.0);
    double last = 0.0;
    for (int i = 0; i < 50; ++i) last = cusum.update((i % 2 == 0) ? 0.3 : -0.3);
    ASSERT_TRUE(std::fabs(last) < 1.0);
}

void test_rolling_ratio_flags_burst() {
    RollingRatio ratio(10);
    double last = 1.0;
    for (int i = 0; i < 20; ++i) last = ratio.update(100.0);  // baseline estable
    double burst = ratio.update(500.0);
    ASSERT_TRUE(burst > 4.0);
}

void test_ensemble_flags_a_flash_crash_bar() {
    AnomalyEnsemble ensemble;
    bool any_alert = false;
    double price = 8000.0;
    for (int i = 0; i < 60; ++i) {
        Bar bar{};
        bar.minute_epoch = i;
        bar.close = price;
        bar.volume = 10.0;
        bar.n_trades = 100;
        bar.taker_buy_volume = 5.0;
        bar.taker_sell_volume = 5.0;
        auto s = ensemble.update(bar);
        if (s.is_alert) any_alert = false;  // no deberia alertar durante la fase estable
    }

    // Ahora un colapso brusco: precio cae 40% en un minuto con volumen 10x y venta agresiva dominante
    Bar crash_bar{};
    crash_bar.minute_epoch = 61;
    crash_bar.close = price * 0.6;
    crash_bar.volume = 200.0;
    crash_bar.n_trades = 2000;
    crash_bar.taker_buy_volume = 20.0;
    crash_bar.taker_sell_volume = 180.0;
    auto crash_signal = ensemble.update(crash_bar);
    any_alert = crash_signal.is_alert;

    ASSERT_TRUE(any_alert);
    ASSERT_TRUE(crash_signal.price_zscore < 0);  // shock a la baja
}

int main() {
    test_csv_reader_parses_fields();
    test_bar_aggregator_ohlcv();
    test_bar_aggregator_fills_gaps();
    test_ewma_zscore_stable_series_near_zero();
    test_ewma_zscore_flags_a_spike();
    test_cusum_flags_sustained_drift();
    test_cusum_does_not_flag_noise_around_zero();
    test_rolling_ratio_flags_burst();
    test_ensemble_flags_a_flash_crash_bar();

    std::cout << (g_checks - g_failures) << "/" << g_checks << " aserciones pasaron\n";
    if (g_failures > 0) {
        std::cout << g_failures << " FALLARON\n";
        return 1;
    }
    std::cout << "TODAS LAS PRUEBAS PASARON\n";
    return 0;
}
