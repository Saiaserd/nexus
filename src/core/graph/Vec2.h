#pragma once

namespace nexus::core {

// Plain 2D vector used by the graph layout (double precision, no Qt dependency).
struct Vec2 {
    double x = 0.0;
    double y = 0.0;

    friend bool operator==(const Vec2& a, const Vec2& b) { return a.x == b.x && a.y == b.y; }
};

} // namespace nexus::core
