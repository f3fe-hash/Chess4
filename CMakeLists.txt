# CMake minimum version

cmake_minimum_required(VERSION 3.12)

# Project & version

project(Chess VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard to C++23, and make it required

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# All warnings,
# extra warnings,
# pedantic warnings,
# treat warnings as errors,
# high optimization

add_compile_options(-Wall -Wextra -Werror -Ofast)

# Source files from src/*.cpp

file(GLOB_RECURSE SRC_FILES src/*.cpp)

# Include files from include/

include_directories(include)

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

# ====================
# Chess executable
# ====================

add_executable(${PROJECT_NAME} main.cpp)

target_link_libraries(${PROJECT_NAME} PRIVATE chess)

# ====================
# Unit tests
# ====================

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

include(GoogleTest)

gtest_discover_tests(chess_tests)
