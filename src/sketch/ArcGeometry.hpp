#pragma once
#include "Sketch.hpp"
#include <cmath>
#include <optional>
namespace cad::sketch {
inline constexpr double arcPi = 3.14159265358979323846, arcTau = 2 * arcPi;
inline Vec2 arcPoint(Vec2 center, double radius, double angle) {
    return {center.x + radius * std::cos(angle), center.y + radius * std::sin(angle)};
}
inline Vec2 curveEnd(const Sketch &s, const Entity &e, int end) {
    domain::check((end == 0 || end == 1) && (e.kind == "Line" || e.kind == "Arc"), "InvalidInput",
                  "Choose a line or arc endpoint");
    return e.kind == "Line" ? s.entity(end ? e.b : e.a).point
                            : arcPoint(s.entity(e.a).point, e.radius, e.start + end * e.sweep);
}
inline Vec2 outgoing(const Sketch &s, const Entity &e, int end) {
    Vec2 v;
    if (e.kind == "Line") {
        auto p = curveEnd(s, e, end), q = curveEnd(s, e, 1 - end);
        v = {p.x - q.x, p.y - q.y};
    } else {
        double angle = e.start + end * e.sweep, sign = end ? 1 : -1;
        v = {-sign * std::sin(angle), sign * std::cos(angle)};
    }
    double n = std::hypot(v.x, v.y);
    return {v.x / n, v.y / n};
}
struct ArcConstruction {
    Vec2 center;
    double radius = 0, start = 0, sweep = 0;
    int originalStart = 0; // Endpoint index after canonicalizing to positive sweep.
};
inline std::optional<ArcConstruction> directedArc(Vec2 center, Vec2 first, Vec2 last, bool ccw) {
    double radius = length(center, first);
    double start = std::atan2(first.y - center.y, first.x - center.x);
    double end = std::atan2(last.y - center.y, last.x - center.x);
    double sweep = std::fmod(end - start + arcTau, arcTau);
    if (!ccw) {
        start = end;
        sweep = arcTau - sweep;
    }
    if (!std::isfinite(radius) || radius <= 1e-9 || radius > 1e3 || sweep <= 1e-8 ||
        sweep >= arcTau - 1e-8 || !std::isfinite(center.x) || !std::isfinite(center.y) ||
        std::abs(center.x) > 1e3 || std::abs(center.y) > 1e3)
        return {};
    return ArcConstruction{center, radius, start, sweep, ccw ? 0 : 1};
}
inline std::optional<ArcConstruction> threePointArc(Vec2 first, Vec2 last, Vec2 bend) {
    Vec2 u{last.x - first.x, last.y - first.y}, v{bend.x - first.x, bend.y - first.y};
    double cross = u.x * v.y - u.y * v.x;
    if (length(first, last) <= 1e-9 || length(first, bend) <= 1e-9 || length(last, bend) <= 1e-9 ||
        std::abs(cross) <= 1e-10 * std::hypot(u.x, u.y) * std::hypot(v.x, v.y))
        return {};
    double a = u.x * u.x + u.y * u.y, b = v.x * v.x + v.y * v.y;
    Vec2 center{first.x + (a * v.y - b * u.y) / (2 * cross),
                first.y + (u.x * b - v.x * a) / (2 * cross)};
    return directedArc(center, first, last, cross < 0);
}
inline std::optional<ArcConstruction> tangentArc(Vec2 first, Vec2 direction, Vec2 last) {
    Vec2 chord{last.x - first.x, last.y - first.y}, normal{-direction.y, direction.x};
    double denominator = 2 * (chord.x * normal.x + chord.y * normal.y);
    if (std::abs(denominator) <= 1e-10 * std::hypot(chord.x, chord.y))
        return {};
    double d = (chord.x * chord.x + chord.y * chord.y) / denominator;
    Vec2 center{first.x + normal.x * d, first.y + normal.y * d};
    return directedArc(center, first, last, d > 0);
}
} // namespace cad::sketch
