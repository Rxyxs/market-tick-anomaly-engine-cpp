#include "ewma_zscore.hpp"

#include <algorithm>
#include <cmath>

EwmaZScore::EwmaZScore(double alpha, int warmup_bars, double std_floor)
    : alpha_(alpha), std_floor_(std_floor), warmup_bars_(warmup_bars) {}

double EwmaZScore::std_dev() const {
    return std::sqrt(var_) + std_floor_;
}

double EwmaZScore::update(double x) {
    count_++;

    if (count_ <= warmup_bars_) {
        warmup_sum_ += x;
        warmup_sum_sq_ += x * x;
        if (count_ == warmup_bars_) {
            mean_ = warmup_sum_ / count_;
            double variance = warmup_sum_sq_ / count_ - mean_ * mean_;
            var_ = std::max(variance, 0.0);
        }
        return 0.0;
    }

    double z = (x - mean_) / std_dev();

    double delta = x - mean_;
    mean_ += alpha_ * delta;
    // Varianza EWMA (Welford-style para promedios exponenciales): pondera
    // la desviacion respecto de la media ANTES y DESPUES de actualizarla.
    var_ = (1.0 - alpha_) * (var_ + alpha_ * delta * delta);

    return z;
}
