cmake_minimum_required(VERSION 3.15)
project(UltraLowLatencyHFT VERSION 1.0.0 LANGUAGES CXX)

# ====
# C++ Standard and Compiler Requirements
# ====
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ====
# Build Type (default to Release with optimizations)
# ====
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

# ====
# Ultra-Low-Latency Compiler Flags
# ====
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    # Aggressive optimization flags for sub-microsecond latency
    # Note: Enabled exceptions because the project uses them
    set(CMAKE_CXX_FLAGS_RELEASE 
        "-O3 -DNDEBUG -march=native -mtune=native -flto -ffast-math \
        -funroll-loops -finline-functions -fomit-frame-pointer \
        -pthread"
    )
    
    # Additional flags for determinism and cache optimization
    add_compile_options(
        -Wall -Wextra -Wpedantic
        -Wno-unused-parameter
        -fno-builtin-malloc -fno-builtin-calloc 
        -fno-builtin-realloc -fno-builtin-free
    )
    
    # Link-time optimization
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
    
    # Debug flags (with some optimizations for realistic testing)
    set(CMAKE_CXX_FLAGS_DEBUG "-g -O1 -march=native -pthread")
    
elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    # MSVC optimization flags
    set(CMAKE_CXX_FLAGS_RELEASE "/O2 /Oi /Ot /GL /DNDEBUG /arch:AVX2")
    set(CMAKE_CXX_FLAGS_DEBUG "/Od /Zi")
endif()

# Enable sanitizers for debug builds
option(ENABLE_ASAN "Enable Address Sanitizer" OFF)
option(ENABLE_TSAN "Enable Thread Sanitizer" OFF)

if(ENABLE_ASAN)
    add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address)
endif()

if(ENABLE_TSAN)
    add_compile_options(-fsanitize=thread)
    add_link_options(-fsanitize=thread)
endif()

# ====
# Platform-specific optimizations
# ====
if(APPLE)
    # Enable non-optimal build on Mac (ARM64) for development
    add_compile_definitions(ALLOW_NON_OPTIMAL_BUILD)
    message(WARNING "Building on macOS (Apple Silicon). Production optimizations disabled.")
endif()

if(UNIX AND NOT APPLE)
    # Linux-specific: prepare for kernel bypass and DPDK
    add_compile_definitions(
        _GNU_SOURCE
        __STDC_LIMIT_MACROS
        __STDC_CONSTANT_MACROS
    )
endif()

# Link with pthread for atomic operations and threading (all Unix systems)
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

# ====
# Find Dependencies
# ====
if(APPLE)
    set(Boost_NO_SYSTEM_PATHS ON)
    set(BOOST_ROOT /opt/homebrew)
endif()

find_package(Boost REQUIRED)
find_package(OpenSSL REQUIRED)
if(APPLE)
    include_directories(/opt/homebrew/include)
    link_directories(/opt/homebrew/lib)
endif()

# ====
# Include Directories
# ====
include_directories(${PROJECT_SOURCE_DIR}/include)
include_directories(${Boost_INCLUDE_DIRS})
include_directories(${OPENSSL_INCLUDE_DIR})

# ====
# Source Files
# ====
set(SOURCES
    src/main.cpp
)

# ====
# Executable Targets
# ====
add_executable(hft_system ${SOURCES})
add_executable(backtest_demo src/backtest_demo.cpp)
add_executable(perf_benchmark src/perf_benchmark.cpp)

# ====
# Linking
# ====
if(UNIX)
    target_link_libraries(hft_system PRIVATE Threads::Threads OpenSSL::SSL OpenSSL::Crypto)
    target_link_libraries(backtest_demo PRIVATE Threads::Threads OpenSSL::SSL OpenSSL::Crypto)
    target_link_libraries(perf_benchmark PRIVATE Threads::Threads OpenSSL::SSL OpenSSL::Crypto)
    
    if(Boost_FOUND)
        target_include_directories(hft_system PRIVATE ${Boost_INCLUDE_DIRS})
        target_include_directories(backtest_demo PRIVATE ${Boost_INCLUDE_DIRS})
    endif()
    
    # Link with real-time extensions for high-resolution timers (Linux only)
    if(NOT APPLE)
        target_link_libraries(hft_system PRIVATE rt)
        target_link_libraries(backtest_demo PRIVATE rt)
    endif()
    
    # Link with Rust library if available
    if(EXISTS "${PROJECT_SOURCE_DIR}/target/release/libhft_rust_core.a")
        target_link_libraries(hft_system PRIVATE 
            "${PROJECT_SOURCE_DIR}/target/release/libhft_rust_core.a")
        message(STATUS "Rust library found and linked")
    endif()
endif()

# ====
# CPU Affinity and NUMA (optional)
# ====
option(ENABLE_NUMA "Enable NUMA optimizations" OFF)
if(ENABLE_NUMA)
    find_library(NUMA_LIB numa)
    if(NUMA_LIB)
        target_link_libraries(hft_system PRIVATE ${NUMA_LIB})
        target_compile_definitions(hft_system PRIVATE ENABLE_NUMA)
        message(STATUS "NUMA support enabled: ${NUMA_LIB}")
    endif()
endif()

# ====
# Installation
# ====
install(TARGETS hft_system DESTINATION bin)

# ====
# Testing (optional)
# ====
option(BUILD_TESTS "Build unit tests" OFF)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# ====
# Print Configuration Summary
# ====
message(STATUS "")
message(STATUS "=== Ultra-Low-Latency HFT System Configuration ===")
message(STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ Standard: C++${CMAKE_CXX_STANDARD}")
message(STATUS "LTO Enabled: ${CMAKE_INTERPROCEDURAL_OPTIMIZATION}")
message(STATUS "NUMA Enabled: ${ENABLE_NUMA}")
message(STATUS "Install Prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "==================================================")
message(STATUS "")
