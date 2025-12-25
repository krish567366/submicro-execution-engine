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
    set(CMAKE_CXX_FLAGS_RELEASE 
        "-O3 -DNDEBUG -march=native -mtune=native -flto -ffast-math \
        -funroll-loops -finline-functions -fomit-frame-pointer \
        -fno-exceptions -fno-rtti -pthread"
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
if(UNIX AND NOT APPLE)
    # Linux-specific: prepare for kernel bypass and DPDK
    add_compile_definitions(
        _GNU_SOURCE
        __STDC_LIMIT_MACROS
        __STDC_CONSTANT_MACROS
    )
    
    # Link with pthread for atomic operations and threading
    set(THREADS_PREFER_PTHREAD_FLAG ON)
    find_package(Threads REQUIRED)
endif()

# ====
# Include Directories
# ====
include_directories(${PROJECT_SOURCE_DIR}/include)

# ====
# Source Files
# ====
set(SOURCES
    src/main.cpp
)

# ====
# Executable Target
# ====
add_executable(hft_system ${SOURCES})

# Backtest Demo with Institutional Logging
add_executable(backtest_demo src/backtest_demo.cpp)

# ====
# Linking
# ====
if(UNIX)
    target_link_libraries(hft_system PRIVATE Threads::Threads)
    target_link_libraries(backtest_demo PRIVATE Threads::Threads)
    
    # Link with real-time extensions for high-resolution timers
    target_link_libraries(hft_system PRIVATE rt)
    target_link_libraries(backtest_demo PRIVATE rt)
    
    # Link with OpenSSL for SHA256 checksums (institutional logging)
    find_package(OpenSSL REQUIRED)
    target_link_libraries(hft_system PRIVATE OpenSSL::SSL OpenSSL::Crypto)
    target_link_libraries(backtest_demo PRIVATE OpenSSL::SSL OpenSSL::Crypto)
    message(STATUS "OpenSSL found: ${OPENSSL_VERSION}")
    
    # Link with Rust library if available
    if(EXISTS "${PROJECT_SOURCE_DIR}/target/release/libhft_rust_core.a")
        target_link_libraries(hft_system PRIVATE 
            "${PROJECT_SOURCE_DIR}/target/release/libhft_rust_core.a")
        message(STATUS "Rust library found and linked")
    else()
        message(STATUS "Rust library not found - build with 'cargo build --release'")
    endif()
endif()

# ====
# Memory Allocator Options (for production)
# Uncomment to use high-performance allocators
# ====
# option(USE_JEMALLOC "Use jemalloc allocator" OFF)
# option(USE_TCMALLOC "Use TCMalloc allocator" OFF)
# 
# if(USE_JEMALLOC)
#     find_library(JEMALLOC_LIB jemalloc)
#     if(JEMALLOC_LIB)
#         target_link_libraries(hft_system PRIVATE ${JEMALLOC_LIB})
#         message(STATUS "Using jemalloc: ${JEMALLOC_LIB}")
#     endif()
# endif()
# 
# if(USE_TCMALLOC)
#     find_library(TCMALLOC_LIB tcmalloc)
#     if(TCMALLOC_LIB)
#         target_link_libraries(hft_system PRIVATE ${TCMALLOC_LIB})
#         message(STATUS "Using TCMalloc: ${TCMALLOC_LIB}")
#     endif()
# endif()

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
# DPDK Support (optional - for production kernel bypass)
# ====
option(ENABLE_DPDK "Enable DPDK integration" OFF)
if(ENABLE_DPDK)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(DPDK REQUIRED libdpdk)
    
    target_include_directories(hft_system PRIVATE ${DPDK_INCLUDE_DIRS})
    target_link_libraries(hft_system PRIVATE ${DPDK_LINK_LIBRARIES})
    target_compile_definitions(hft_system PRIVATE ENABLE_DPDK)
    
    message(STATUS "DPDK enabled")
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
# Documentation
# ====
option(BUILD_DOCS "Build documentation" OFF)
if(BUILD_DOCS)
    find_package(Doxygen)
    if(DOXYGEN_FOUND)
        add_custom_target(docs
            COMMAND ${DOXYGEN_EXECUTABLE} ${PROJECT_SOURCE_DIR}/Doxyfile
            WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
            COMMENT "Generating API documentation with Doxygen"
            VERBATIM
        )
    endif()
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
message(STATUS "DPDK Enabled: ${ENABLE_DPDK}")
message(STATUS "NUMA Enabled: ${ENABLE_NUMA}")
message(STATUS "Install Prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "==================================================")
message(STATUS "")
