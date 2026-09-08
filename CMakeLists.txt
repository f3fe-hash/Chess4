# To run tests:
# cmake --build . --target chess_tests_verbose

# CMake minimum version

cmake_minimum_required(VERSION 3.12)

# Project & version

project(Chess VERSION 1.0.0 LANGUAGES C CXX)

option(CHESS_UCI_SERVER "Build Chess as a TCP UCI server" OFF)
option(BUILD_CHESS_TESTS "Build Chess unit tests" ON)

# Set C++ standard to C++26, and make it required

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set C standard to C23, and make it required

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)

# All warnings,
# extra warnings,
# high optimization

add_compile_options(-Wall -Wextra -Ofast)

# Source files from src/*.cpp & src/*.c

file(GLOB_RECURSE SRC_FILES src/*.cpp src/*.c)

# Include files from include/

include_directories(include)

# ====================
# Dependencies
# ====================

# ncurses

find_package(Curses REQUIRED)

# ====================
# PGO
# ====================

# ---
# cmake -DCMAKE_BUILD_TYPE=Release -DPGO_MODE=GENERATE ..
# cmake --build .
# ---
# ./Chess
# ---
# cmake -DPGO_MODE=USE ..
# cmake --build . --target clean
# cmake --build .
# ---

# Set the default build type to Release for optimization

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Define a PGO_MODE variable that can be: OFF, GENERATE, or USE

set(PGO_MODE "OFF" CACHE STRING "PGO build phase: OFF, GENERATE, or USE")
set_property(CACHE PGO_MODE PROPERTY STRINGS "OFF" "GENERATE" "USE")

# Configure profile path (defaults to the build directory)

set(PGO_PROFILE_DIR "${CMAKE_BINARY_DIR}/pgo_data")

# Apply GCC flags based on the PGO phase

if(PGO_MODE STREQUAL "GENERATE")

    message(STATUS "PGO Mode: Generating profile instrumentation...")

    add_compile_options(-fprofile-generate=${PGO_PROFILE_DIR})
    add_link_options(-fprofile-generate=${PGO_PROFILE_DIR})

elseif(PGO_MODE STREQUAL "USE")

    message(STATUS "PGO Mode: Applying profile data for optimization...")

    add_compile_options(
        -fprofile-use=${PGO_PROFILE_DIR}
        -fprofile-correction
    )

    add_link_options(-fprofile-use=${PGO_PROFILE_DIR})

    # Optional: Treat profile mismatches as warnings rather than hard errors

    add_compile_options(-Wno-error=coverage-mismatch)

endif()

# ====================
# Chess library
# ====================

add_library(chess STATIC ${SRC_FILES})

target_include_directories(chess PUBLIC include)

# ncurses is required by Console.cpp

target_include_directories(chess PRIVATE ${CURSES_INCLUDE_DIR})

target_link_libraries(chess PRIVATE ${CURSES_LIBRARIES})

# ====================
# Chess executable
# ====================

add_executable(${PROJECT_NAME} main.cpp)

target_link_libraries(${PROJECT_NAME} PRIVATE chess)

if(CHESS_UCI_SERVER)
    target_compile_definitions(${PROJECT_NAME} PRIVATE UCI_SERVER)
else()
    target_compile_definitions(${PROJECT_NAME} PRIVATE MATCH_TEST)
endif()

# ====================
# Unit tests
# ====================

if(BUILD_CHESS_TESTS)
    include(FetchContent)

    FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.17.0.zip
    )

    FetchContent_MakeAvailable(googletest)

    enable_testing()

    file(GLOB_RECURSE TEST_FILES tests/*.cpp)

    add_executable(chess_tests ${TEST_FILES})

    target_link_libraries(chess_tests
        PRIVATE
        chess
        GTest::gtest_main
    )

    add_test(NAME chess_tests COMMAND chess_tests)

    add_custom_target(chess_tests_verbose
        COMMAND chess_tests
        DEPENDS chess_tests
        USES_TERMINAL
    )
endif()
