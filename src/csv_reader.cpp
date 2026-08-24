#include "csv_reader.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

namespace {

// Extrae el siguiente campo delimitado por coma desde `cursor`, avanzando
// el puntero. Evita std::stringstream/std::getline (mucho mas lentos) en
// una ruta que se ejecuta millones de veces.
const char* next_field(const char* cursor, char* buffer, size_t buffer_size) {
    size_t i = 0;
    while (*cursor != '\0' && *cursor != ',' && *cursor != '\n' && *cursor != '\r') {
        if (i + 1 < buffer_size) buffer[i++] = *cursor;
        ++cursor;
    }
    buffer[i] = '\0';
    if (*cursor == ',') ++cursor;
    return cursor;
}

}  // namespace

size_t read_trades_csv(const std::string& path, std::vector<Trade>& out_trades) {
    FILE* f = nullptr;
    if (fopen_s(&f, path.c_str(), "rb") != 0 || f == nullptr) {
        throw std::runtime_error("No se pudo abrir el archivo: " + path);
    }

    constexpr size_t kChunkSize = 1 << 20;  // 1 MB
    std::vector<char> chunk(kChunkSize);
    std::string leftover;
    char field[64];
    size_t n_rows_before = out_trades.size();

    size_t bytes_read;
    while ((bytes_read = fread(chunk.data(), 1, kChunkSize, f)) > 0) {
        leftover.append(chunk.data(), bytes_read);

        size_t line_start = 0;
        for (size_t i = 0; i < leftover.size(); ++i) {
            if (leftover[i] == '\n') {
                const char* cursor = leftover.c_str() + line_start;
                if (cursor[0] != '\0') {
                    Trade trade{};
                    cursor = next_field(cursor, field, sizeof(field));  // trade_id (no usado)
                    cursor = next_field(cursor, field, sizeof(field));
                    trade.price = std::atof(field);
                    cursor = next_field(cursor, field, sizeof(field));
                    trade.qty = std::atof(field);
                    cursor = next_field(cursor, field, sizeof(field));  // quote_qty (no usado)
                    cursor = next_field(cursor, field, sizeof(field));
                    trade.timestamp_ms = std::atoll(field);
                    cursor = next_field(cursor, field, sizeof(field));
                    trade.is_buyer_maker = (field[0] == 'T' || field[0] == 't');

                    out_trades.push_back(trade);
                }
                line_start = i + 1;
            }
        }
        leftover.erase(0, line_start);
    }
    fclose(f);
    return out_trades.size() - n_rows_before;
}
