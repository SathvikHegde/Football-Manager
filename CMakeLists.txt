cmake_minimum_required(VERSION 3.20)
project(FootballManager VERSION 0.3.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Standard MSVC configuration across all targets
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL" CACHE STRING "MSVC runtime library")
    # Use /Z7 (embedded debug info) to avoid background mspdbsrv server lockouts in sandboxed/CI environments
    string(REPLACE "/Zi" "/Z7" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
    string(REPLACE "/Zi" "/Z7" CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")
endif()

enable_testing()

# 1. Pure C++20 Core Simulation Static Library
add_subdirectory(core)

# 2. Standalone Simulation CLI Harness
add_subdirectory(cli_harness)

# 3. Unit and Determinism Test Suite
add_subdirectory(tests)

# 4. Godot 4 GDExtension Dynamic Library Bridge
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/gdextension/CMakeLists.txt")
    add_subdirectory(gdextension)
endif()
