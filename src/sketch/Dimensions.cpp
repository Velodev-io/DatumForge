#include "Dimensions.hpp"
#include "Curves.hpp"
#include <cmath>
#include <functional>
namespace cad::sketch {
bool angularDimension(QString kind) {
    return kind == "Angle" || kind == "Sweep" || kind == "StartAngle";
}
bool componentDimension(QString kind) {
    return horizontalDimension(kind) || kind == "VerticalDistance" || kind == "VerticalOrdinate";
}
bool horizontalDimension(QString kind) {
    return kind == "HorizontalDistance" || kind == "HorizontalOrdinate";
}
bool associatedDimension(QString kind) {
    return QStringList{"CenterlineDiameter", "ArcLength", "HorizontalOrdinate", "VerticalOrdinate"}
        .contains(kind);
}
double axisDistance(const Sketch &s, const Constraint &c, bool end) {
    auto &profile = s.entity(c.a), &axis = s.entity(c.b);
    auto p = profile.kind == "Point" ? profile.point : s.entity(end ? profile.b : profile.a).point;
    auto a = s.entity(axis.a).point, b = s.entity(axis.b).point;
    return ((b.x - a.x) * (p.y - a.y) - (b.y - a.y) * (p.x - a.x)) / length(a, b);
}
bool dimensional(QString kind) {
    return kind == "TrimStart" || kind == "TrimSpan" || kind == "MinorRadius" ||
           kind == "StartAngle" || kind == "PathLength" || componentDimension(kind) ||
           associatedDimension(kind) ||
           QStringList{"Length", "Distance", "Angle", "Radius", "Diameter", "SlotLength", "Sweep"}
               .contains(kind);
}
Vec2 pathEndpoint(const Sketch &s, const Constraint &c, int member, bool end) {
    auto &edge = s.entity(c.members[member]);
    auto range = curveRange(edge);
    return curveValue(s, edge, end != c.directions[member] ? range.second : range.first);
}
double curveLength(const Sketch &s, const Entity &e) {
    if (e.kind == "Line")
        return length(s.entity(e.a).point, s.entity(e.b).point);
    if (e.kind == "Arc" || e.kind == "Circle")
        return e.radius * (e.kind == "Circle" ? 6.2831853071795864769 : e.sweep);
    domain::check(advancedCurve(e.kind), "UnsupportedOperation",
                  "Parametric curve length required");
    // Adaptive Gauss integration of exact parametric speed, split at spline
    // knots. This never integrates the renderer's sampled display polyline.
    constexpr double nodes[]{.1834346424956498, .5255324099163290, .7966664774136267,
                             .9602898564975363};
    constexpr double weights[]{.3626837833783620, .3137066458778873, .2223810344533745,
                               .1012285362903763};
    int evaluations = 0;
    auto gauss = [&](double first, double last) {
        double middle = (first + last) / 2, half = (last - first) / 2, total = 0;
        for (int i = 0; i < 4; ++i)
            for (double sign : {-1., 1.}) {
                domain::check(++evaluations <= 8192, "ResourceLimit",
                              "Path integration exceeded its bounded work budget");
                auto derivative = curveDerivative(s, e, middle + sign * half * nodes[i]);
                auto speed = std::hypot(derivative.x, derivative.y);
                domain::check(std::isfinite(speed), "InvalidGeometry", "Nonfinite path derivative");
                total += weights[i] * speed;
            }
        return half * total;
    };
    std::function<double(double, double, double, double, int)> integrate;
    integrate = [&](double first, double last, double whole, double tolerance, int depth) {
        double middle = (first + last) / 2;
        auto left = gauss(first, middle), right = gauss(middle, last);
        if (std::abs(left + right - whole) <= tolerance)
            return left + right;
        domain::check(depth < 16, "SolveFailure",
                      "Parametric path-length integration did not converge");
        return integrate(first, middle, left, tolerance / 2, depth + 1) +
               integrate(middle, last, right, tolerance / 2, depth + 1);
    };
    auto range = curveRange(e);
    std::vector<double> boundaries{range.first};
    if (e.kind == "Spline")
        for (double knot : e.knots)
            if (knot > range.first && knot < range.second)
                boundaries.push_back(knot);
    boundaries.push_back(range.second);
    double total = 0;
    for (size_t i = 1; i < boundaries.size(); ++i) {
        auto initial = gauss(boundaries[i - 1], boundaries[i]);
        total += integrate(boundaries[i - 1], boundaries[i], initial,
                           1e-12 * std::max(1e-6, std::abs(initial)), 0);
    }
    return total;
}
void validateDimension(const Sketch &s, const Constraint &c, bool connectivity) {
    if (c.kind == "PathLength") {
        domain::check(!c.members.empty() && c.members.size() <= 64 && c.members[0] == c.a &&
                          c.b.isEmpty() && c.members.size() == qsizetype(c.directions.size()),
                      "InvalidInput",
                      "An ordered path of at most 64 members and explicit directions is required");
        QSet<QString> seen;
        std::optional<Vec2> previous;
        std::vector<std::array<Vec2, 2>> endpoints;
        for (int i = 0; i < c.members.size(); ++i) {
            auto &edge = s.entity(c.members[i]);
            domain::check(edge.kind != "Point" && !seen.contains(edge.id), "InvalidInput",
                          "Distinct supported path curves required");
            seen.insert(edge.id);
            auto start = pathEndpoint(s, c, i, false);
            auto end = pathEndpoint(s, c, i, true);
            endpoints.push_back({start, end});
            domain::check(
                !connectivity || !previous ||
                    length(*previous, start) <= std::max(1e-11, 2e-7 * length(start, end)),
                "ReferenceMissing", "Path members must be connected in their stored order");
            previous = end;
        }
        if (connectivity)
            for (size_t i = 0; i < endpoints.size(); ++i)
                for (auto endpoint : endpoints[i]) {
                    int degree = 0;
                    for (size_t j = 0; j < endpoints.size(); ++j)
                        if (i != j)
                            for (auto other : endpoints[j])
                                degree += length(endpoint, other) <= 1e-9;
                    domain::check(degree <= 1, "ReferenceAmbiguous",
                                  "A path cannot branch or revisit an intermediate junction");
                }
        if (connectivity && c.closed)
            domain::check(length(pathEndpoint(s, c, 0, false), *previous) <= 1e-9,
                          "ReferenceMissing",
                          "Closed path must retain its final endpoint association");
    } else
        domain::check(c.members.empty() && c.directions.empty() && !c.closed, "InvalidInput",
                      "Only path dimensions carry path membership");
    domain::check((c.driving && !c.placement && !c.increment && !c.drawing) || dimensional(c.kind),
                  "InvalidInput", "Only dimensions carry reference mode or label placement");
    if (!dimensional(c.kind))
        return;
    auto reason = constraintReason(s, c.kind, c.a, c.b, c.third);
    domain::check(reason.isEmpty(), "UnsupportedOperation", reason);
    domain::check(c.third.isEmpty(), "InvalidInput", "Dimension has no third operand");
    domain::check(c.driving || c.expression.isEmpty(), "InvalidInput",
                  "Reference dimensions measure geometry and cannot carry driving expressions");
    if (c.placement)
        domain::check(std::isfinite(c.placement->x) && std::isfinite(c.placement->y) &&
                          std::abs(c.placement->x) <= 1e3 && std::abs(c.placement->y) <= 1e3,
                      "InvalidInput", "Finite dimension placement within 1 km required");
    if (c.increment)
        domain::check(std::isfinite(*c.increment) && *c.increment > 0 && *c.increment <= 1e3,
                      "InvalidInput",
                      "Finite positive dimension increment at most 1000 SI units required");
}
double measureDimension(const Sketch &s, const Constraint &c) {
    validateDimension(s, c, false);
    domain::check(dimensional(c.kind), "InvalidInput", "Select a dimensional constraint");
    auto &a = s.entity(c.a);
    if (c.kind == "PathLength") {
        double total = 0;
        for (auto &id : c.members) {
            auto &edge = s.entity(id);
            total += curveLength(s, edge);
        }
        return total;
    }
    if (c.kind == "CenterlineDiameter") {
        double distance = axisDistance(s, c);
        if (a.kind == "Line" && !c.driving) {
            auto &axis = s.entity(c.b);
            double span = std::max(length(s.entity(a.a).point, s.entity(a.b).point),
                                   length(s.entity(axis.a).point, s.entity(axis.b).point));
            domain::check(std::abs(distance - axisDistance(s, c, true)) <=
                              std::max(1e-11, 2e-7 * span),
                          "ReferenceAmbiguous",
                          "A line diameter requires a profile parallel to its centerline");
        }
        return 2 * std::abs(distance);
    }
    if (c.kind == "ArcLength")
        return a.radius * a.sweep;
    if (c.kind == "Radius" || c.kind == "MinorRadius" || c.kind == "Diameter")
        return a.radius * (c.kind == "Diameter" ? 2 : 1);
    if (c.kind == "Sweep" || c.kind == "TrimSpan")
        return a.sweep;
    if (c.kind == "StartAngle" || c.kind == "TrimStart")
        return a.start;
    if (c.kind == "Angle") {
        auto &b = s.entity(c.b);
        auto p = s.entity(a.a).point, q = s.entity(a.b).point, r = s.entity(b.a).point,
             t = s.entity(b.b).point;
        return std::remainder(std::atan2(t.y - r.y, t.x - r.x) - std::atan2(q.y - p.y, q.x - p.x),
                              6.2831853071795864769);
    }
    auto p = a.kind == "Point" ? a.point : s.entity(a.a).point;
    auto q = a.kind == "Point" ? s.entity(c.b).point : s.entity(a.b).point;
    if (horizontalDimension(c.kind))
        return q.x - p.x;
    if (c.kind == "VerticalDistance" || c.kind == "VerticalOrdinate")
        return q.y - p.y;
    double value = length(p, q);
    if (c.kind == "SlotLength")
        value += 2 * s.entity(c.b).radius;
    return value;
}
bool Sketch::dimensionSemantics() const {
    if (interactionSemantics())
        return true;
    for (auto &c : constraints)
        if (!c.driving || c.placement || c.increment || c.drawing || componentDimension(c.kind) ||
            associatedDimension(c.kind))
            return true;
    return false;
}
bool Sketch::associatedDimensionSemantics() const {
    for (auto &c : constraints)
        if (associatedDimension(c.kind))
            return true;
    return false;
}
bool Sketch::interactionSemantics() const {
    return std::any_of(constraints.begin(), constraints.end(), [this](auto &c) {
        return c.kind == "TrimStart" || c.kind == "TrimSpan" || c.kind == "MinorRadius" ||
               c.kind == "StartAngle" || c.kind == "PathLength" || c.increment.has_value() ||
               c.drawing || (c.kind == "Sweep" && entity(c.a).kind == "EllipticalArc") ||
               (c.reversed && (c.kind == "Length" || c.kind == "Distance"));
    });
}
} // namespace cad::sketch
