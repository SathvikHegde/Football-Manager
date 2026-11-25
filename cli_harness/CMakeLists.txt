add_executable(fm_sim_cli
    main.cpp
    batch_runner.cpp
)

target_link_libraries(fm_sim_cli PRIVATE
    fm_core
)

set_target_properties(fm_sim_cli PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

target_compile_options(fm_sim_cli PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /WX /permissive- /utf-8 /Zc:__cplusplus /wd4100>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic -Werror>
)
