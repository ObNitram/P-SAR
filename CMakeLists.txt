cmake_minimum_required(VERSION 3.25)
project(psar C CXX)

set(CMAKE_C_STANDARD 17)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/build)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/build)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/build)

# Gather source and header files
file(GLOB_RECURSE SOURCES "${PROJECT_SOURCE_DIR}/src/*.c")
file(GLOB_RECURSE HEADERS "${PROJECT_SOURCE_DIR}/src/*.h")

# Optionally, exclude main.c from the library sources
list(FILTER SOURCES EXCLUDE REGEX ".*/main\\.c$")

# Create the shared library target from the sources and headers
add_library(psar SHARED ${SOURCES} ${HEADERS})
target_include_directories(psar PUBLIC "${PROJECT_SOURCE_DIR}/src")

# Create an executable target using main.c
add_executable(main-psar "${PROJECT_SOURCE_DIR}/src/main.c")
target_include_directories(main-psar PUBLIC "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(main-psar PRIVATE psar)

# -----------------------------
# Configure GoogleTest using FetchContent
# -----------------------------
include(FetchContent)
FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.16.0.zip
)
FetchContent_MakeAvailable(googletest)

enable_testing()

# Gather test source files
file(GLOB_RECURSE TEST_SOURCES "${PROJECT_SOURCE_DIR}/test/*.cpp")

# Create an executable target for tests
add_executable(test-psar ${TEST_SOURCES})
target_include_directories(test-psar PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(test-psar PRIVATE psar gtest gtest_main)

# Automatically discover tests with GoogleTest
include(GoogleTest)
gtest_discover_tests(test-psar)
