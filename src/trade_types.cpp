#include "trade_types.hpp"

#include <chrono>
#include <ctime>
#include <sstream>
#include <iomanip>

// Convertir time_t a tm en UTC de forma reentrante no esta en el estandar: MSVC
// expone gmtime_s y POSIX expone gmtime_r, con los argumentos **al reves** entre
// si. El codigo usaba solo gmtime_s, asi que compilaba unicamente en Windows a
// pesar de no incluir ninguna cabecera de Windows -- lo detecto la primera
// corrida de CI bajo GCC. No se usa el gmtime() a secas porque devuelve un
// puntero a un buffer estatico compartido, y este motor corre con hilos.
namespace {
inline void gmtime_utc(const std::time_t& t, std::tm& out) {
#ifdef _WIN32
    gmtime_s(&out, &t);
#else
    gmtime_r(&t, &out);
#endif
}
}  // namespace

std::string Bar::minute_utc_string() const {
    std::time_t t = static_cast<std::time_t>(minute_epoch * 60);
    std::tm tm_utc{};
    gmtime_utc(t, tm_utc);
    std::ostringstream oss;
    oss << std::put_time(&tm_utc, "%Y-%m-%d %H:%M");
    return oss.str();
}
