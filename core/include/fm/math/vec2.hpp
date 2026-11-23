#pragma once
#include <cmath>

namespace fm {

struct Vec2 {
    float x{0.0f};
    float y{0.0f};

    constexpr Vec2() = default;
    constexpr Vec2(float x_, float y_) : x(x_), y(y_) {}

    constexpr Vec2 operator+(const Vec2& o) const { return {x + o.x, y + o.y}; }
    constexpr Vec2 operator-(const Vec2& o) const { return {x - o.x, y - o.y}; }
    constexpr Vec2 operator*(float scalar) const { return {x * scalar, y * scalar}; }
    constexpr Vec2 operator/(float scalar) const { return {x / scalar, y / scalar}; }

    [[nodiscard]] constexpr float dot(const Vec2& o) const { return x * o.x + y * o.y; }
    [[nodiscard]] float length_sq() const { return x * x + y * y; }
    [[nodiscard]] float length() const { return std::sqrt(length_sq()); }
    [[nodiscard]] Vec2 normalized() const {
        float len = length();
        return len > 0.0001f ? Vec2{x / len, y / len} : Vec2{0.0f, 0.0f};
    }

    [[nodiscard]] constexpr bool operator==(const Vec2& o) const {
        return x == o.x && y == o.y;
    }
};

} // namespace fm

