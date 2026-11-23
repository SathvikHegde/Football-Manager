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

    // Normal force cancels gravity when resting on turf
    float vertical_accel = GRAVITY;
    if (ball.position.z <= 0.0f && std::abs(ball.velocity.z) < 1e-4f) {
        vertical_accel = 0.0f;
        ball.velocity.z = 0.0f;
        ball.position.z = 0.0f;
    }

    Vec3 accel = drag_accel + Vec3{0.0f, 0.0f, vertical_accel};

    // Integrate velocity and position
    ball.velocity = ball.velocity + accel * dt;
    ball.position = ball.position + ball.velocity * dt;

    // Ground bounce & friction
    if (ball.position.z <= 0.0f) {
        ball.position.z = 0.0f;
        float bounce_vz = -ball.velocity.z * RESTITUTION;
        if (bounce_vz < (MIN_BOUNCE_VEL + std::abs(GRAVITY) * dt)) {
            ball.velocity.z = 0.0f;
        } else {
            ball.velocity.z = bounce_vz;
        }
        // Turf friction damping
        ball.velocity.x *= TURF_FRICTION;
        ball.velocity.y *= TURF_FRICTION;

        // Stop rolling if speed drops below threshold (5 cm/s)
        if (std::abs(ball.velocity.x) < 0.05f) ball.velocity.x = 0.0f;
        if (std::abs(ball.velocity.y) < 0.05f) ball.velocity.y = 0.0f;
    }

    if (ball.claim_lock_ticks > 0) {
        ball.claim_lock_ticks--;
    }
}

} // namespace BallPhysics
} // namespace fm

