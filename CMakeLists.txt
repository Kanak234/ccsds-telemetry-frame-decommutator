cmake_minimum_required(VERSION 3.20)
project(ccsds-telemetry-frame-decommutator
    VERSION 1.0.0
    DESCRIPTION "High-performance CCSDS Telemetry Frame Decommutator Engine"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(ENABLE_COVERAGE "Enable code coverage instrumentation" OFF)

# Enforce strict compiler hygiene: warnings treated as errors
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Werror -pedantic -Wconversion -Wsign-conversion -O3)
endif()

if(ENABLE_COVERAGE AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(--coverage -O0 -g)
    add_link_options(--coverage)
endif()

# Core Library
add_library(ccsds_core STATIC
    src/galois_field.cpp
    src/reed_solomon.cpp
    src/descrambler.cpp
    src/crc16.cpp
    src/frame_sync.cpp
    src/transfer_frame.cpp
    src/packet_extractor.cpp
    src/decommutator.cpp
)

target_include_directories(ccsds_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# CLI Binary
add_executable(ccsds_decommutator src/main.cpp)
target_link_libraries(ccsds_decommutator PRIVATE ccsds_core)

# Benchmark Binary
add_executable(benchmark_throughput benchmarks/benchmark_throughput.cpp)
target_link_libraries(benchmark_throughput PRIVATE ccsds_core)

# Unit & Integration Tests
enable_testing()

set(TEST_SOURCES
    test_galois_field
    test_reed_solomon
    test_descrambler
    test_crc16
    test_frame_sync
    test_transfer_frame
    test_packet_extractor
    test_end_to_end
    test_decommutator_edge_cases
)

foreach(TEST_NAME ${TEST_SOURCES})
    add_executable(${TEST_NAME} tests/${TEST_NAME}.cpp)
    target_link_libraries(${TEST_NAME} PRIVATE ccsds_core)
    target_compile_options(${TEST_NAME} PRIVATE -UNDEBUG)
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
endforeach()

# Installation Targets
include(GNUInstallDirs)
install(TARGETS ccsds_core ccsds_decommutator
    EXPORT ccsdsTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(DIRECTORY include/ccsds DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
