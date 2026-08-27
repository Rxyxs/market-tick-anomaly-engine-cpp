// Tipos de datos para un tick de Order Book L2 (a diferencia de `Trade` en
// trade_types.hpp, que es solo una ejecucion): un tick L2 puede ser una
// actualizacion de nivel de precio (bid/ask, add/replace/cancel implicito
// por reemplazo del nivel) o una operacion ejecutada. `L2TickEvent` es POD
// trivialmente copiable a proposito -- es el tipo que viaja por el ring
// buffer sin bloqueos (spsc_ring_buffer.hpp), y un tipo lock-free real
// necesita una representacion de tamano fijo, sin punteros ni asignacion,
// para poder copiarse con una simple copia de memoria.
#pragma once

#include <cstdint>
#include <type_traits>

namespace streaming {

enum class L2EventType : uint8_t {
    kBookUpdate = 0,  // nuevo precio/cantidad en un nivel del libro (top-of-book o mas profundo)
    kTrade = 1,       // ejecucion contra el libro
};

enum class Side : uint8_t {
    kBid = 0,
    kAsk = 1,
};

struct L2TickEvent {
    int64_t sequence = 0;               // numero de secuencia monotono del feed simulado
    int64_t producer_timestamp_ns = 0;   // steady_clock, momento en que el productor genero el evento
    L2EventType event_type = L2EventType::kBookUpdate;
    Side side = Side::kBid;
    uint8_t price_level = 0;   // 0 = top of book (mejor bid/ask), profundidad creciente en adelante
    double price = 0.0;
    double quantity = 0.0;
};

static_assert(std::is_trivially_copyable_v<L2TickEvent>,
              "L2TickEvent debe ser trivialmente copiable para viajar por el ring buffer lock-free");

}  // namespace streaming
