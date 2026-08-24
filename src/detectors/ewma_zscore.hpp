// Z-score en linea (streaming) contra una media y varianza EWMA
// (exponentially weighted moving average). O(1) por actualizacion, sin
// guardar historia — el tipo de detector que un motor de verdad en tiempo
// real necesita, a diferencia de un z-score sobre una ventana completa
// recalculada cada vez.
//
// El z-score de cada punto se calcula usando el estado PREVIO (antes de
// incorporar el punto actual), para que un shock puntual no se diluya a si
// mismo en la media que lo esta evaluando.
//
// Los primeros `warmup_bars` puntos NO producen un z-score (retornan 0):
// se usan solo para estimar una media/varianza inicial razonable via un
// promedio simple. Sin este calentamiento, el segundo dato ya recibido
// (con varianza aun en 0) produce un z-score explosivo por division contra
// un piso de desviacion estandar casi nulo — exactamente el tipo de
// artefacto de arranque en frio que un motor real no puede permitirse.
#pragma once

class EwmaZScore {
public:
    explicit EwmaZScore(double alpha, int warmup_bars = 30, double std_floor = 1e-6);

    double update(double x);

    double mean() const { return mean_; }
    double std_dev() const;

private:
    double alpha_;
    double mean_ = 0.0;
    double var_ = 0.0;
    double std_floor_;
    int warmup_bars_;
    int count_ = 0;
    double warmup_sum_ = 0.0;
    double warmup_sum_sq_ = 0.0;
};
