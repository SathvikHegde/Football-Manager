#pragma once
#include <cmath>

namespace fm {

struct Vec3 {
    float x{0.0f};
    float y{0.0f};
    float z{0.0f};

    constexpr Vec3() = default;
    constexpr Vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}

    constexpr Vec3 operator+(const Vec3& o) const { return {x + o.x, y + o.y, z + o.z}; }
    constexpr Vec3 operator-(const Vec3& o) const { return {x - o.x, y - o.y, z - o.z}; }
    constexpr Vec3 operator*(float scalar) const { return {x * scalar, y * scalar, z * scalar}; }
    constexpr Vec3 operator/(float scalar) const { return {x / scalar, y / scalar, z / scalar}; }

    [[nodiscard]] constexpr float dot(const Vec3& o) const { return x * o.x + y * o.y + z * o.z; }
    [[nodiscard]] float length_sq() const { return x * x + y * y + z * z; }
    [[nodiscard]] float length() const { return std::sqrt(length_sq()); }
    [[nodiscard]] Vec3 normalized() const {
        float len = length();
        return len > 0.0001f ? Vec3{x / len, y / len, z / len} : Vec3{0.0f, 0.0f, 0.0f};
    }

    [[nodiscard]] constexpr bool operator==(const Vec3& o) const {
        return x == o.x && y == o.y && z == o.z;
    }
};

} // namespace fm

