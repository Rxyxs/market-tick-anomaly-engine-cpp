// CUSUM tabular (Page, 1954): acumula desviaciones sostenidas de la media
// en cualquier direccion, para detectar un cambio de regimen (deriva
// persistente) que un z-score puntual — que solo mira una observacion a la
// vez — no necesariamente captura si cada paso individual es moderado pero
// la tendencia es sostenida.
#pragma once

#include <algorithm>

class Cusum {
public:
    // k: holgura (cuanta desviacion se tolera antes de acumular, tipicamente
    //    ~0.5 desvios estandar del input ya estandarizado).
    // h: umbral de decision (tipicamente 4-5 desvios estandar acumulados).
    Cusum(double k = 0.5, double h = 5.0) : k_(k), h_(h) {}

    // `x` se espera ya estandarizado (p.ej. la salida de un EwmaZScore).
    // Retorna la magnitud acumulada normalizada por el umbral (1.0 = en el
    // umbral de decision), con signo segun la direccion del corrimiento:
    // positivo para una deriva sostenida al alza, negativo a la baja.
    double update(double x) {
        s_pos_ = std::max(0.0, s_pos_ + x - k_);
        s_neg_ = std::min(0.0, s_neg_ + x + k_);

        if (s_pos_ >= -s_neg_) {
            return s_pos_ / h_;
        }
        return s_neg_ / h_;
    }

private:
    double k_;
    double h_;
    double s_pos_ = 0.0;
    double s_neg_ = 0.0;
};
