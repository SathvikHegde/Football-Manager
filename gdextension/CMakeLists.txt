if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/godot-cpp/CMakeLists.txt")
    set(GODOTCPP_API_VERSION "4.7" CACHE STRING "" FORCE)
    add_subdirectory(godot-cpp)

    add_library(fm_gdextension SHARED
        src/register_types.cpp
        src/match_engine_bridge.cpp
    )

    target_include_directories(fm_gdextension PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

    target_link_libraries(fm_gdextension PRIVATE
        godot::cpp
        fm_core
    )

    if(MSVC)
        target_compile_options(fm_gdextension PRIVATE /WX-)
    endif()

    set_target_properties(fm_gdextension PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/godot_client/bin"
        RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_SOURCE_DIR}/godot_client/bin"
        RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/godot_client/bin"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/godot_client/bin"
    )
else()
    message(STATUS "godot-cpp not found in gdextension/godot-cpp. fm_gdextension target will be skipped until godot-cpp submodule is added.")
endif()

