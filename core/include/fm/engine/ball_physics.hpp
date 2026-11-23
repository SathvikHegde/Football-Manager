#pragma once
#include "fm/math/vec3.hpp"
#include "fm/model/match_state.hpp"

namespace fm {

namespace BallPhysics {
    constexpr float DRAG_COEFF = 0.006f;
    constexpr float GRAVITY = -9.81f;
    constexpr float RESTITUTION = 0.55f;
    constexpr float TURF_FRICTION = 0.94f;
    constexpr float MIN_BOUNCE_VEL = 0.15f;

    void step(BallState& ball, float dt = 0.05f);
}

} // namespace fm

