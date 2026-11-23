add_executable(fm_tests
    main.cpp
    test_determinism.cpp
    test_physics.cpp
    test_rollback.cpp
    test_rules.cpp
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

add_test(NAME fm_tests COMMAND fm_tests)

