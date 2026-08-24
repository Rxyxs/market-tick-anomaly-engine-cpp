// Razon entre el valor actual y el promedio movil de una ventana trailing
// (excluyendo el punto actual), usada como detector de rafagas de
// actividad: cuantas veces por sobre lo "normal reciente" esta el valor de
// este minuto.
#pragma once

#include <deque>

class RollingRatio {
public:
    explicit RollingRatio(size_t window_size) : window_size_(window_size) {}

    // Retorna x / promedio(ventana previa), o 1.0 si aun no hay historia
    // suficiente (evita falsos positivos al arrancar la serie).
    double update(double x) {
        double ratio = 1.0;
        if (!window_.empty()) {
            double mean = sum_ / static_cast<double>(window_.size());
            ratio = mean > 0.0 ? x / mean : 1.0;
        }

        window_.push_back(x);
        sum_ += x;
        if (window_.size() > window_size_) {
            sum_ -= window_.front();
            window_.pop_front();
        }
        return ratio;
    }

private:
    size_t window_size_;
    std::deque<double> window_;
    double sum_ = 0.0;
};
