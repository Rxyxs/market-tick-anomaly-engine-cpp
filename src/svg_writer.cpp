#include "svg_writer.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>

namespace {

constexpr double kWidth = 1200.0;
constexpr double kHeight = 420.0;
constexpr double kMarginLeft = 60.0;
constexpr double kMarginRight = 20.0;
constexpr double kMarginTop = 30.0;
constexpr double kMarginBottom = 40.0;

double map_x(size_t i, size_t n) {
    if (n <= 1) return kMarginLeft;
    double t = static_cast<double>(i) / static_cast<double>(n - 1);
    return kMarginLeft + t * (kWidth - kMarginLeft - kMarginRight);
}

double map_y(double value, double min_v, double max_v) {
    double range = (max_v - min_v);
    if (range <= 0.0) range = 1.0;
    double t = (value - min_v) / range;
    return (kHeight - kMarginBottom) - t * (kHeight - kMarginTop - kMarginBottom);
}

std::string svg_header(const std::string& title) {
    std::ostringstream oss;
    oss << "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 " << kWidth << " " << kHeight
        << "\" font-family=\"sans-serif\">\n";
    oss << "<rect x=\"0\" y=\"0\" width=\"" << kWidth << "\" height=\"" << kHeight
        << "\" fill=\"#0e1117\"/>\n";
    oss << "<text x=\"" << kMarginLeft << "\" y=\"18\" fill=\"#e6e6e6\" font-size=\"15\">" << title
        << "</text>\n";
    return oss.str();
}

std::string crash_window_rect(const CrashWindow& window, size_t n) {
    double x0 = map_x(window.start_index, n);
    double x1 = map_x(window.end_index, n);
    std::ostringstream oss;
    oss << "<rect x=\"" << x0 << "\" y=\"" << kMarginTop << "\" width=\"" << (x1 - x0)
        << "\" height=\"" << (kHeight - kMarginTop - kMarginBottom)
        << "\" fill=\"#e74c3c\" opacity=\"0.12\"/>\n";
    oss << "<text x=\"" << (x0 + 4) << "\" y=\"" << (kMarginTop + 14)
        << "\" fill=\"#e74c3c\" font-size=\"11\">" << window.label << "</text>\n";
    return oss.str();
}

std::string polyline(const std::vector<std::pair<double, double>>& points, const std::string& color,
                       double stroke_width = 1.3) {
    std::ostringstream oss;
    oss << "<polyline fill=\"none\" stroke=\"" << color << "\" stroke-width=\"" << stroke_width
        << "\" points=\"";
    for (const auto& [x, y] : points) oss << x << "," << y << " ";
    oss << "\"/>\n";
    return oss.str();
}

}  // namespace

void write_price_chart_svg(const std::string& path, const std::vector<Bar>& bars,
                             const std::vector<AnomalySignal>& signals,
                             const CrashWindow& crash_window) {
    size_t n = bars.size();
    double min_price = bars[0].close, max_price = bars[0].close;
    for (const auto& bar : bars) {
        min_price = std::min(min_price, bar.low);
        max_price = std::max(max_price, bar.high);
    }

    std::ostringstream body;
    body << svg_header("BTC/USDT - Precio real (Binance) y alertas del ensamble - 11-13 marzo 2020");
    body << crash_window_rect(crash_window, n);

    std::vector<std::pair<double, double>> price_points;
    price_points.reserve(n);
    for (size_t i = 0; i < n; ++i) {
        price_points.emplace_back(map_x(i, n), map_y(bars[i].close, min_price, max_price));
    }
    body << polyline(price_points, "#3498db", 1.1);

    for (size_t i = 0; i < n; ++i) {
        if (signals[i].is_alert) {
            double x = map_x(i, n);
            double y = map_y(bars[i].close, min_price, max_price);
            body << "<circle cx=\"" << x << "\" cy=\"" << y << "\" r=\"2.6\" fill=\"#f39c12\"/>\n";
        }
    }

    body << "<text x=\"" << kMarginLeft << "\" y=\"" << (kHeight - 10)
         << "\" fill=\"#999\" font-size=\"11\">11 mar 00:00 UTC -&gt; 13 mar 23:59 UTC (4320 barras de 1 minuto)</text>\n";
    body << "<text x=\"" << (kWidth - 260) << "\" y=\"" << (kHeight - 10)
         << "\" fill=\"#f39c12\" font-size=\"11\">o alerta del ensamble</text>\n";
    body << "</svg>\n";

    std::ofstream out(path);
    out << body.str();
}

void write_score_chart_svg(const std::string& path, const std::vector<AnomalySignal>& signals,
                             double alert_threshold, const CrashWindow& crash_window) {
    size_t n = signals.size();
    double max_score = alert_threshold;
    for (const auto& s : signals) max_score = std::max(max_score, s.ensemble_score_max);
    max_score *= 1.05;

    std::ostringstream body;
    body << svg_header("Score de anomalia del ensamble (maximo de 4 detectores) en el tiempo");
    body << crash_window_rect(crash_window, n);

    double y_threshold = map_y(alert_threshold, 0.0, max_score);
    body << "<line x1=\"" << kMarginLeft << "\" y1=\"" << y_threshold << "\" x2=\""
         << (kWidth - kMarginRight) << "\" y2=\"" << y_threshold
         << "\" stroke=\"#f39c12\" stroke-width=\"1\" stroke-dasharray=\"4,3\"/>\n";
    body << "<text x=\"" << (kWidth - 150) << "\" y=\"" << (y_threshold - 4)
         << "\" fill=\"#f39c12\" font-size=\"11\">umbral de alerta</text>\n";

    std::vector<std::pair<double, double>> score_points;
    score_points.reserve(n);
    for (size_t i = 0; i < n; ++i) {
        score_points.emplace_back(map_x(i, n), map_y(signals[i].ensemble_score_max, 0.0, max_score));
    }
    body << polyline(score_points, "#2ecc71", 1.0);
    body << "</svg>\n";

    std::ofstream out(path);
    out << body.str();
}
