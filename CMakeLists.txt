cmake_minimum_required(VERSION 3.14)
project(toguzcpp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_FLAGS_RELEASE "-O3")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

add_library(toguznative STATIC
	src/toguznative.cpp
)

if (CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64|x86" AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
	target_compile_options(toguznative PRIVATE -mavx2)
endif()

target_include_directories(toguznative PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}/src
)

add_executable(toguz_benchmark
	main.cpp
)

target_link_libraries(toguz_benchmark
	PRIVATE
		toguznative
)

add_executable(toguz_try 
	try.cpp
)

target_link_libraries(toguz_try
	PRIVATE
		toguznative
)

enable_testing()

include(FetchContent)
find_package(GTest QUIET)

add_executable(toguznative_tests
	test/toguznative_test.cpp
)

target_link_libraries(toguznative_tests
	PRIVATE
		toguznative
		GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(toguznative_tests)

