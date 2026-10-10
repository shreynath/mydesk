#pragma once

#include <cmath>
#include <ostream>

namespace mydesk::geometry {

struct Vec2 {
    double x{0.0};
    double y{0.0};

    constexpr Vec2() = default;
    constexpr Vec2(double x_, double y_) : x(x_), y(y_) {}

    [[nodiscard]] constexpr Vec2 operator+(Vec2 o) const { return {x + o.x, y + o.y}; }
    [[nodiscard]] constexpr Vec2 operator-(Vec2 o) const { return {x - o.x, y - o.y}; }
    [[nodiscard]] constexpr Vec2 operator*(double s) const { return {x * s, y * s}; }
    [[nodiscard]] constexpr Vec2 operator/(double s) const { return {x / s, y / s}; }
    constexpr Vec2& operator+=(Vec2 o) {
        x += o.x;
        y += o.y;
        return *this;
    }
    constexpr Vec2& operator-=(Vec2 o) {
        x -= o.x;
        y -= o.y;
        return *this;
    }

    [[nodiscard]] double length() const { return std::hypot(x, y); }
    [[nodiscard]] double lengthSquared() const { return x * x + y * y; }
    [[nodiscard]] double distanceTo(Vec2 o) const { return (*this - o).length(); }

    [[nodiscard]] bool nearlyEqual(Vec2 o, double eps = 1e-9) const {
        return std::abs(x - o.x) <= eps && std::abs(y - o.y) <= eps;
    }
};

[[nodiscard]] inline constexpr Vec2 operator*(double s, Vec2 v) { return v * s; }

inline std::ostream& operator<<(std::ostream& os, Vec2 v) {
    return os << '(' << v.x << ", " << v.y << ')';
}

} // namespace mydesk::geometry
