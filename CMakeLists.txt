cmake_minimum_required(VERSION 3.20)

# Auto-detect portable Python installation if not already provided
if(NOT DEFINED Python3_EXECUTABLE AND EXISTS "C:/Programs/Python/python.exe")
    set(Python3_EXECUTABLE "C:/Programs/Python/python.exe" CACHE FILEPATH "Path to Python 3 interpreter")
endif()

project(FootballManager VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enforce strict compiler warnings and standards compliance
if(MSVC)
    # Use /Z7 (embedded debug info) to avoid background mspdbsrv server lockouts in sandboxed environments
    string(REPLACE "/Zi" "/Z7" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
    string(REPLACE "/Zi" "/Z7" CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")
    add_compile_options(/W4 /WX /permissive- /utf-8 /Zc:__cplusplus)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Werror)
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

