#include "fm/engine/ball_physics.hpp"
#include <cmath>

namespace fm {

namespace BallPhysics {

void step(BallState& ball, float dt) {
    if (ball.owner_slot >= 0) {
        // Ball is in player's possession
        return;
    }

    // Velocity magnitude
    float speed = ball.velocity.length();

    // Drag: a_drag = -C_drag * |v| * v
    Vec3 drag_accel = ball.velocity * (-DRAG_COEFF * speed);
    Vec3 accel = drag_accel + Vec3{0.0f, 0.0f, GRAVITY};

    // Integrate velocity and position
    ball.velocity = ball.velocity + accel * dt;
    ball.position = ball.position + ball.velocity * dt;

    // Ground bounce & friction
    if (ball.position.z <= 0.0f) {
        ball.position.z = 0.0f;
        if (std::abs(ball.velocity.z) < MIN_BOUNCE_VEL) {
            ball.velocity.z = 0.0f;
        } else {
            ball.velocity.z = -ball.velocity.z * RESTITUTION;
        }
        // Turf friction damping
        ball.velocity.x *= TURF_FRICTION;
        ball.velocity.y *= TURF_FRICTION;
    }

    if (ball.claim_lock_ticks > 0) {
        ball.claim_lock_ticks--;
    }
}

} // namespace BallPhysics
} // namespace fm

