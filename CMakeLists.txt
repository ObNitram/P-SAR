cmake_minimum_required(VERSION 3.25)
project(P_SAR C)

set(CMAKE_C_STANDARD 17)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/build)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/build)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/build)

file(GLOB_RECURSE SOURCES "${PROJECT_SOURCE_DIR}/src/*.c")
file(GLOB_RECURSE HEADERS "${PROJECT_SOURCE_DIR}/src/*.h")

add_library(P_SAR SHARED ${SOURCES} ${HEADERS})
target_include_directories(P_SAR PUBLIC "${PROJECT_SOURCE_DIR}/src")

# Add an executable target using main.c.
add_executable(P_SAR_main "${PROJECT_SOURCE_DIR}/src/main.c")
target_include_directories(P_SAR_main PUBLIC "${PROJECT_SOURCE_DIR}/src")
