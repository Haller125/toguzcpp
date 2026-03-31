cmake_minimum_required(VERSION 3.14)
project(toguzcpp LANGUAGES CXX)

# ============================================================================
# Compiler Version Requirements
# ============================================================================
# GCC 9+ or Clang 10+ for C++20 features
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9)
        message(FATAL_ERROR "GCC 9 or newer required (found ${CMAKE_CXX_COMPILER_VERSION})")
    endif()
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 10)
        message(FATAL_ERROR "Clang 10 or newer required (found ${CMAKE_CXX_COMPILER_VERSION})")
    endif()
else()
    message(WARNING "Compiler ${CMAKE_CXX_COMPILER_ID} not officially supported")
endif()

# ============================================================================
# C++ Standard
# ============================================================================
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ============================================================================
# Build Type & Optimization
# ============================================================================
# Default to Release if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
    message(STATUS "Build type not specified, defaulting to Release")
endif()

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")

# Use portable architecture instead of -march=native for consistency
# -march=generic works on all x86-64 systems
set(ARCHITECTURE_FLAGS "-march=x86-64")

# Optimization flags - use target-specific approach
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    # Release: O3 with fast-math and NDEBUG
    set(OPTIMIZATION_FLAGS "-O3 -ffast-math -DNDEBUG")
    set(LTO_FLAGS "-flto=auto")  # Enable Link-Time Optimization
elseif(CMAKE_BUILD_TYPE STREQUAL "Debug")
    # Debug: O0 for debugging
    set(OPTIMIZATION_FLAGS "-O0 -g3")
    set(LTO_FLAGS "")
elseif(CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    # Release with debug info
    set(OPTIMIZATION_FLAGS "-O2 -g2 -DNDEBUG")
    set(LTO_FLAGS "-flto=auto")
else()
    # MinSizeRel
    set(OPTIMIZATION_FLAGS "-Os -DNDEBUG")
    set(LTO_FLAGS "-flto=auto")
endif()

# ============================================================================
# Common Flags
# ============================================================================
set(COMMON_FLAGS "-Wall -Wextra -pedantic")

# Build the flag string
set(CMAKE_CXX_FLAGS "${COMMON_FLAGS} ${ARCHITECTURE_FLAGS} ${OPTIMIZATION_FLAGS} ${LTO_FLAGS}")

message(STATUS "C++ Flags: ${CMAKE_CXX_FLAGS}")

# ============================================================================
# Main Library
# ============================================================================
add_library(toguznative STATIC
    src/toguznative.cpp
    src/search.cpp
)

# AVX2 support - check CPU and compiler capabilities
if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64|x86" AND 
   (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang"))
    # Check if AVX2 is supported
    include(CheckCXXCompilerFlag)
    check_cxx_compiler_flag("-mavx2" COMPILER_SUPPORTS_AVX2)
    
    if(COMPILER_SUPPORTS_AVX2)
        target_compile_options(toguznative PRIVATE -mavx2)
        target_compile_definitions(toguznative PRIVATE HAS_AVX2=1)
        message(STATUS "AVX2 support enabled")
    else()
        message(WARNING "AVX2 not supported by compiler, disabling")
    endif()
endif()

target_include_directories(toguznative PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# ============================================================================
# Executables
# ============================================================================
add_executable(toguz_benchmark main.cpp)
target_link_libraries(toguz_benchmark PRIVATE toguznative)

add_executable(toguz_try try.cpp)
target_link_libraries(toguz_try PRIVATE toguznative)

add_executable(generate src/generater.cpp)
target_link_libraries(generate PRIVATE toguznative)

add_executable(dataset_clean src/clean_dataset.cpp)

add_executable(search_depth_benchmark src/search_depth_benchmark.cpp)
target_link_libraries(search_depth_benchmark PRIVATE toguznative)

add_executable(iterative_deepening_benchmark src/iterative_deepening_benchmark.cpp)
target_link_libraries(iterative_deepening_benchmark PRIVATE toguznative)

# ============================================================================
# Tests
# ============================================================================
enable_testing()

include(FetchContent)
find_package(GTest QUIET)

if(GTest_FOUND OR GTEST_FOUND)
    add_executable(toguznative_tests test/toguznative_test.cpp test/tt_test.cpp)
    target_link_libraries(toguznative_tests PRIVATE toguznative GTest::gtest_main)
    include(GoogleTest)
    gtest_discover_tests(toguznative_tests)
    message(STATUS "Tests enabled")
else()
    message(WARNING "GTest not found, tests disabled")
endif()