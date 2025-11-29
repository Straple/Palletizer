cmake_minimum_required(VERSION 3.16)

project(palletizer)

set(CMAKE_CXX_STANDARD 20)

# Compile-time configuration options
option(ENABLE_ASSERT "Enable assertions" ON)

# Configurable compile-time values
set(THREADS_NUM "8" CACHE STRING "Allowed number of threads")

# Apply configuration
if (ENABLE_ASSERT)
    add_definitions(-DENABLE_ASSERT)
    message(STATUS "Enable assertions")
endif ()

# Set compile-time constants
add_definitions(-DTHREADS_NUM=${THREADS_NUM})

message(STATUS "THREADS_NUM set to: ${THREADS_NUM}")

find_package(Threads REQUIRED)

include_directories("src")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

file(GLOB SOURCES
        src/utils/*.cpp
        src/objects/*.cpp
        src/solvers/*.cpp
        src/solvers/greedy/*.cpp
        )

add_executable(main src/main.cpp ${SOURCES})
target_link_libraries(main Threads::Threads)
