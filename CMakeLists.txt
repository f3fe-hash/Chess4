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

# Set the default build type to Release for optimization

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Optimizations

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    # Max optimization
    add_compile_options(-Ofast)

    # Allow maximum optmization
    add_compile_definitions(RELEASE)
elseif(CMAKE_BUILD_TYPE STREQUAL "Debug")
    # Debug optimization
    add_compile_options(-Og -g)

    # Set debug flag for extra debug in program
    add_compile_definitions(DEBUG)
endif()

# All warnings,
# extra warnings

add_compile_options(-Wall -Wextra)

# Source files from src/*.cpp & src/*.c

file(GLOB_RECURSE SRC_FILES src/*.cpp src/*.c)

# Include files from include/

include_directories(include)

# ====================
# Dependencies
# ====================

# ncurses
find_package(Curses REQUIRED)

# ONNX Runtime
find_path(ONNXRUNTIME_INCLUDE_DIR
    NAMES onnxruntime_cxx_api.h
    PATHS
        /usr/include
        /usr/local/include
        $ENV{HOME}/onnxruntime/include
    PATH_SUFFIXES
        onnxruntime
)

find_library(ONNXRUNTIME_LIBRARY
    NAMES onnxruntime
    PATHS
        /usr/lib
        /usr/lib/aarch64-linux-gnu
        /usr/local/lib
        $ENV{HOME}/onnxruntime/lib
)

if(NOT ONNXRUNTIME_INCLUDE_DIR)
    message(FATAL_ERROR
        "Could not find ONNX Runtime headers (onnxruntime_cxx_api.h)"
    )
endif()

if(NOT ONNXRUNTIME_LIBRARY)
    message(FATAL_ERROR
        "Could not find ONNX Runtime library (libonnxruntime.so)"
    )
endif()

get_filename_component(
    ONNXRUNTIME_LIBRARY_DIR
    "${ONNXRUNTIME_LIBRARY}"
    DIRECTORY
)

message(STATUS
    "ONNX Runtime include: ${ONNXRUNTIME_INCLUDE_DIR}"
)

message(STATUS
    "ONNX Runtime library: ${ONNXRUNTIME_LIBRARY}"
)

# ====================
# PGO
# ====================

set(PGO_MODE "OFF" CACHE STRING "PGO build phase: OFF, GENERATE, or USE")
set_property(CACHE PGO_MODE PROPERTY STRINGS "OFF" "GENERATE" "USE")

set(PGO_PROFILE_DIR "${CMAKE_BINARY_DIR}/pgo_data")

# All source files under src/, except src/interface/.
file(GLOB_RECURSE PGO_SOURCES
    CONFIGURE_DEPENDS
    src/*.cpp
    src/*.c
)

list(FILTER PGO_SOURCES EXCLUDE REGEX
    ".*/src/interface/.*"
)

if(PGO_MODE STREQUAL "GENERATE")

    message(STATUS "PGO Mode: Generating profile instrumentation...")

    foreach(PGO_SOURCE IN LISTS PGO_SOURCES)
        set_property(
            SOURCE "${PGO_SOURCE}"
            APPEND PROPERTY COMPILE_OPTIONS
            "-fprofile-generate=${PGO_PROFILE_DIR}"
        )
    endforeach()

elseif(PGO_MODE STREQUAL "USE")

    message(STATUS "PGO Mode: Applying profile data for optimization...")

    foreach(PGO_SOURCE IN LISTS PGO_SOURCES)
        set_property(
            SOURCE "${PGO_SOURCE}"
            APPEND PROPERTY COMPILE_OPTIONS
            "-fprofile-use=${PGO_PROFILE_DIR}"
            "-fprofile-correction"
        )
    endforeach()

elseif(NOT PGO_MODE STREQUAL "OFF")

    message(FATAL_ERROR
        "Invalid PGO_MODE='${PGO_MODE}'. "
        "Expected OFF, GENERATE, or USE."
    )

endif()

# ====================
# Chess library
# ====================

add_library(chess STATIC ${SRC_FILES})

target_include_directories(chess PUBLIC
    include
)

# ncurses is required by Console.cpp
target_include_directories(chess PRIVATE
    ${CURSES_INCLUDE_DIR}
)

target_link_libraries(chess PRIVATE
    ${CURSES_LIBRARIES}
)

# ONNX Runtime
target_include_directories(chess PUBLIC
    ${ONNXRUNTIME_INCLUDE_DIR}
)

target_link_libraries(chess PUBLIC
    ${ONNXRUNTIME_LIBRARY}
)

set_target_properties(chess PROPERTIES
    BUILD_RPATH "${ONNXRUNTIME_LIBRARY_DIR}"
)

# ====================
# Chess executable
# ====================

add_executable(${PROJECT_NAME} main.cpp)

if(PGO_MODE STREQUAL "GENERATE")
    target_link_options(${PROJECT_NAME} PRIVATE
        "-fprofile-generate=${PGO_PROFILE_DIR}"
    )
elseif(PGO_MODE STREQUAL "USE")
    target_link_options(${PROJECT_NAME} PRIVATE
        "-fprofile-use=${PGO_PROFILE_DIR}"
    )
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE chess)

if(PGO_MODE STREQUAL "GENERATE")
    target_compile_definitions(${PROJECT_NAME} PRIVATE PGO_TEST)
elseif(CHESS_UCI_SERVER)
    target_compile_definitions(${PROJECT_NAME} PRIVATE UCI_SERVER)
elseif(CHESS_CONSOLE_APP)
    target_compile_definitions(${PROJECT_NAME} PRIVATE CONSOLE_APP)
elseif(CHESS_MATCH_TEST)
    target_compile_definitions(${PROJECT_NAME} PRIVATE MATCH_TEST)
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
