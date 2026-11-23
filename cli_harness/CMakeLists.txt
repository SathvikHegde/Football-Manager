add_executable(fm_sim_cli
    main.cpp
)

target_link_libraries(fm_sim_cli PRIVATE
    fm_core
)

if(MSVC)
    target_compile_options(fm_sim_cli PRIVATE /wd4100)
endif()

set_target_properties(fm_sim_cli PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

