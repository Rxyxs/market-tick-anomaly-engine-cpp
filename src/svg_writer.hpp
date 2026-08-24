// Escritor de graficos SVG minimalista, sin dependencias externas: el
// motor genera sus propias visualizaciones de resultado (precio + alertas,
// score del ensamble en el tiempo) sin necesitar Python/matplotlib ni
// ninguna libreria de graficos de terceros. GitHub renderiza SVG de forma
// nativa, asi que estos archivos se pueden embeber directo en el README.
#pragma once

#include <string>
#include <vector>

#include "detectors/ensemble.hpp"
#include "trade_types.hpp"

struct CrashWindow {
    size_t start_index;
    size_t end_index;
    std::string label;
};

void write_price_chart_svg(const std::string& path, const std::vector<Bar>& bars,
                             const std::vector<AnomalySignal>& signals,
                             const CrashWindow& crash_window);

void write_score_chart_svg(const std::string& path, const std::vector<AnomalySignal>& signals,
                             double alert_threshold, const CrashWindow& crash_window);
