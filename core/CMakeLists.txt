add_library(fm_core STATIC
    # Engine
    src/engine/version.cpp
    src/engine/pitch.cpp
    src/engine/match_simulator.cpp
    src/engine/match_stats.cpp
    src/engine/ball_physics.cpp
    src/engine/player_kinematics.cpp
    src/engine/rules.cpp

    # Math
    src/math/curves.cpp
    src/math/rng.cpp

    # Model
    src/model/player.cpp
    src/model/match_state.cpp
    src/model/tactics.cpp
    src/model/team.cpp

    # Recording
    src/recording/frame_snapshot.cpp
    src/recording/match_recording.cpp
    src/recording/checkpoint_manager.cpp

    # AI
    src/ai/team_phase.cpp
    src/ai/tactical_anchors.cpp
    src/ai/utility_ai.cpp
    src/ai/goalkeeper_ai.cpp
    src/ai/execution.cpp
)

target_include_directories(fm_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

set_target_properties(fm_core PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

target_compile_options(fm_core PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /WX /permissive- /utf-8 /Zc:__cplusplus>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic -Werror>
)

