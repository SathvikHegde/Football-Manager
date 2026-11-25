add_executable(fm_tests
    main.cpp
    test_determinism.cpp
    test_physics.cpp
    test_rollback.cpp
    test_rules.cpp
    test_match_loop.cpp
    test_calibration.cpp
)

target_include_directories(fm_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(fm_tests PRIVATE
    fm_core
)

set_target_properties(fm_tests PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

target_compile_options(fm_tests PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /WX /permissive- /utf-8 /Zc:__cplusplus>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic -Werror>
)

add_test(NAME fm_tests COMMAND fm_tests)
