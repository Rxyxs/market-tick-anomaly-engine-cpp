cmake_minimum_required(VERSION 3.20)
project(market_tick_anomaly_engine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/outputs/bin)

find_package(Threads REQUIRED)

# --- market_anomaly_engine: motor batch sobre CSV historico -----------------
add_executable(market_anomaly_engine
    src/main.cpp
    src/csv_reader.cpp
    src/bar_aggregator.cpp
    src/trade_types.cpp
    src/svg_writer.cpp
    src/detectors/ewma_zscore.cpp
    src/detectors/ensemble.cpp
)
target_include_directories(market_anomaly_engine PRIVATE src)

# --- streaming_demo: pipeline lock-free en vivo (ring buffer + feed L2) -----
add_executable(streaming_demo
    src/streaming_main.cpp
    src/detectors/ewma_zscore.cpp
)
target_include_directories(streaming_demo PRIVATE src)
target_link_libraries(streaming_demo PRIVATE Threads::Threads)

# --- run_tests: suite de pruebas hecha a mano (sin framework externo) ------
add_executable(run_tests
    tests/test_main.cpp
    src/csv_reader.cpp
    src/bar_aggregator.cpp
    src/trade_types.cpp
    src/detectors/ewma_zscore.cpp
    src/detectors/ensemble.cpp
)
target_include_directories(run_tests PRIVATE src)
target_link_libraries(run_tests PRIVATE Threads::Threads)

enable_testing()
add_test(NAME unit_tests COMMAND run_tests)
