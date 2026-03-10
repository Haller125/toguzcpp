cmake_minimum_required(VERSION 3.14)
project(toguzcpp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(toguznative STATIC
	src/toguznative.cpp
)

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

