#include "Dimensions.hpp"
#include <cmath>
namespace cad::sketch {
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
    return componentDimension(kind) || associatedDimension(kind) ||
           QStringList{"Length", "Distance", "Angle", "Radius", "Diameter", "SlotLength", "Sweep"}
               .contains(kind);
}
void validateDimension(const Sketch &s, const Constraint &c) {
    domain::check((c.driving && !c.placement) || dimensional(c.kind), "InvalidInput",
                  "Only dimensions carry reference mode or label placement");
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
}
double measureDimension(const Sketch &s, const Constraint &c) {
    validateDimension(s, c);
    domain::check(dimensional(c.kind), "InvalidInput", "Select a dimensional constraint");
    auto &a = s.entity(c.a);
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
    if (c.kind == "Radius" || c.kind == "Diameter")
        return a.radius * (c.kind == "Diameter" ? 2 : 1);
    if (c.kind == "Sweep")
        return a.sweep;
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
    for (auto &c : constraints)
        if (!c.driving || c.placement || componentDimension(c.kind) || associatedDimension(c.kind))
            return true;
    return false;
}
bool Sketch::associatedDimensionSemantics() const {
    for (auto &c : constraints)
        if (associatedDimension(c.kind))
            return true;
    return false;
}
} // namespace cad::sketch
