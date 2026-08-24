#include "ensemble.hpp"

#include <algorithm>
#include <cmath>

AnomalyEnsemble::AnomalyEnsemble(double ewma_alpha, double cusum_k, double cusum_h,
                                   size_t volume_window, double volume_burst_threshold,
                                   double alert_threshold)
    : price_z_(ewma_alpha),
      cusum_(cusum_k, cusum_h),
      volume_ratio_(volume_window),
      imbalance_z_(ewma_alpha),
      volume_burst_threshold_(volume_burst_threshold),
      alert_threshold_(alert_threshold) {}

AnomalySignal AnomalyEnsemble::update(const Bar& bar) {
    AnomalySignal signal;
    signal.minute_epoch = bar.minute_epoch;

    if (has_prev_close_ && prev_close_ > 0.0 && bar.close > 0.0) {
        signal.log_return = std::log(bar.close / prev_close_);
    }
    prev_close_ = bar.close;
    has_prev_close_ = true;

    signal.price_zscore = price_z_.update(signal.log_return);
    signal.cusum_signal = cusum_.update(signal.price_zscore);
    signal.volume_burst_ratio = volume_ratio_.update(static_cast<double>(bar.n_trades));
    signal.imbalance_zscore = imbalance_z_.update(bar.order_flow_imbalance());

    double comp_price = std::fabs(signal.price_zscore) / 3.0;
    double comp_cusum = std::fabs(signal.cusum_signal);
    double comp_volume = signal.volume_burst_ratio / volume_burst_threshold_;
    double comp_imbalance = std::fabs(signal.imbalance_zscore) / 3.0;

    signal.ensemble_score_max = std::max({comp_price, comp_cusum, comp_volume, comp_imbalance});
    signal.ensemble_score_avg = (comp_price + comp_cusum + comp_volume + comp_imbalance) / 4.0;
    signal.is_alert = signal.ensemble_score_max >= alert_threshold_;

    return signal;
}
