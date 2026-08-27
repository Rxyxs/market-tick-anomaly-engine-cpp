// Buffer circular sin bloqueos (lock-free), single-producer/single-consumer
// (SPSC), C++20. Un solo hilo productor llama try_push, un solo hilo
// consumidor llama try_pop -- concurrentemente, sin mutex ni ninguna
// primitiva de sincronizacion del sistema operativo, solo std::atomic con
// el par de operaciones release/acquire que garantiza que cuando el
// consumidor ve un `tail` avanzado, tambien ve el elemento que el productor
// escribio antes de avanzarlo (happens-before), y viceversa para `head`.
//
// Por que SPSC y no MPMC: un ring buffer multi-productor/multi-consumidor
// lock-free es sustancialmente mas complejo (necesita CAS en el indice, no
// solo load/store) y mas lento en el caso comun; este motor tiene
// exactamente un hilo generando ticks del feed y exactamente un hilo
// consumiendolos para el ensamble de deteccion, asi que SPSC es la
// estructura correcta para el problema real, no la mas general posible.
//
// `head_`, `tail_` y el buffer en si se alinean a la linea de cache
// (`kCacheLineSize`, tipicamente 64 bytes) en variables separadas
// deliberadamente: sin ese padding, el productor escribiendo `tail_` y el
// consumidor escribiendo `head_` invalidarian la misma linea de cache el
// uno al otro en cada operacion (false sharing), destruyendo buena parte
// de la ganancia de evitar el mutex.
#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <new>
#include <type_traits>

namespace streaming {

#if defined(__cpp_lib_hardware_interference_size)
inline constexpr size_t kCacheLineSize = std::hardware_destructive_interference_size;
#else
inline constexpr size_t kCacheLineSize = 64;
#endif

template <typename T, size_t Capacity>
class SpscRingBuffer {
    static_assert((Capacity & (Capacity - 1)) == 0 && Capacity > 1,
                  "Capacity debe ser una potencia de 2 mayor que 1");
    static_assert(std::is_trivially_copyable_v<T>,
                  "T debe ser trivialmente copiable para transferencia lock-free");

public:
    SpscRingBuffer() = default;
    SpscRingBuffer(const SpscRingBuffer&) = delete;
    SpscRingBuffer& operator=(const SpscRingBuffer&) = delete;

    // Intenta encolar `item`. Retorna false si el buffer esta lleno (el
    // consumidor no ha drenado lo suficiente) -- nunca bloquea, nunca
    // asigna memoria: es responsabilidad del llamador reintentar o
    // descartar el evento, exactamente como en un feed de mercado real
    // donde un productor no puede permitirse esperar a un consumidor lento.
    bool try_push(const T& item) noexcept {
        const size_t tail = tail_.load(std::memory_order_relaxed);
        const size_t next_tail = (tail + 1) & kMask;
        if (next_tail == head_.load(std::memory_order_acquire)) {
            return false;  // buffer lleno
        }
        buffer_[tail] = item;
        tail_.store(next_tail, std::memory_order_release);
        return true;
    }

    // Intenta desencolar en `out`. Retorna false si el buffer esta vacio.
    bool try_pop(T& out) noexcept {
        const size_t head = head_.load(std::memory_order_relaxed);
        if (head == tail_.load(std::memory_order_acquire)) {
            return false;  // buffer vacio
        }
        out = buffer_[head];
        head_.store((head + 1) & kMask, std::memory_order_release);
        return true;
    }

    // Tamano aproximado (solo para observabilidad/metricas -- entre el load
    // de tail y el de head el productor/consumidor puede haber avanzado,
    // asi que este numero es una instantanea, no un valor exacto atomico).
    size_t size_approx() const noexcept {
        const size_t tail = tail_.load(std::memory_order_acquire);
        const size_t head = head_.load(std::memory_order_acquire);
        return (tail - head) & kMask;
    }

    static constexpr size_t capacity() noexcept { return Capacity - 1; }

private:
    static constexpr size_t kMask = Capacity - 1;

    alignas(kCacheLineSize) std::atomic<size_t> head_{0};
    alignas(kCacheLineSize) std::atomic<size_t> tail_{0};
    alignas(kCacheLineSize) std::array<T, Capacity> buffer_{};
};

}  // namespace streaming
