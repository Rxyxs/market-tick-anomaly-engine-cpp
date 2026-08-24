// Ensamble de 4 detectores en linea, cada uno capturando un mecanismo de
// anomalia distinto en la microestructura del mercado:
//
//   1. price_zscore       - shock de precio puntual (EWMA z-score sobre
//                            el log-retorno minuto a minuto).
//   2. cusum_signal        - deriva sostenida de precio (CUSUM sobre la
//                            misma serie ya estandarizada por (1)).
//   3. volume_burst_ratio  - rafaga de actividad (razon del conteo de
//                            trades del minuto vs. su promedio movil).
//   4. imbalance_zscore    - presion direccional del flujo de ordenes
//                            (EWMA z-score sobre el desequilibrio
//                            comprador-agresor vs. vendedor-agresor).
//
// El score de ensamble es el maximo de las 4 componentes normalizadas
// (cualquier mecanismo fuerte dispara la alerta), mas el promedio como
// score secundario — el mismo patron de combinacion "max + average" usado
// en el ensamble PyOD del proyecto hermano de deteccion de LA.
#pragma once

#include <cstdint>

#include "cusum.hpp"
#include "ewma_zscore.hpp"
#include "rolling_ratio.hpp"
#include "../trade_types.hpp"

struct AnomalySignal {
    int64_t minute_epoch = 0;
    double log_return = 0.0;
    double price_zscore = 0.0;
    double cusum_signal = 0.0;
    double volume_burst_ratio = 1.0;
    double imbalance_zscore = 0.0;
    double ensemble_score_max = 0.0;
    double ensemble_score_avg = 0.0;
    bool is_alert = false;
};

class AnomalyEnsemble {
public:
    AnomalyEnsemble(double ewma_alpha = 0.05, double cusum_k = 0.5, double cusum_h = 5.0,
                     size_t volume_window = 30, double volume_burst_threshold = 3.0,
                     double alert_threshold = 1.0);

    AnomalySignal update(const Bar& bar);

private:
    EwmaZScore price_z_;
    Cusum cusum_;
    RollingRatio volume_ratio_;
    EwmaZScore imbalance_z_;
    double volume_burst_threshold_;
    double alert_threshold_;
    bool has_prev_close_ = false;
    double prev_close_ = 0.0;
};
