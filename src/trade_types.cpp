#include "trade_types.hpp"

#include <chrono>
#include <ctime>
#include <sstream>
#include <iomanip>

std::string Bar::minute_utc_string() const {
    std::time_t t = static_cast<std::time_t>(minute_epoch * 60);
    std::tm tm_utc{};
    gmtime_s(&tm_utc, &t);
    std::ostringstream oss;
    oss << std::put_time(&tm_utc, "%Y-%m-%d %H:%M");
    return oss.str();
}
