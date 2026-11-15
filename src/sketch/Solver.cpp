#include "Solver.hpp"
#include "ArcGeometry.hpp"
#include "ArcLengthConstraint.hpp"
#include "AxisDistanceConstraint.hpp"
#include "CurveConstraint.hpp"
#include "Curves.hpp"
#include "Dimensions.hpp"
#include "PathLengthConstraint.hpp"
#include "References.hpp"
#include "Slots.hpp"
#include "domain/Expressions.hpp"
#include "geometry/occt/SketchCurve.hpp"
#include <Eigen/QR>
#include <GCS.h>
#include <GeomAPI_ExtremaCurveCurve.hxx>
#include <GeomAPI_ProjectPointOnCurve.hxx>
#include <QElapsedTimer>
#include <Standard_Failure.hxx>
#include <deque>
namespace cad::sketch {
using domain::check;
namespace {
constexpr double tau = 6.2831853071795864769;
Vec2 vector(const Sketch &s, const Entity &e) {
    auto a = s.entity(e.a).point, b = s.entity(e.b).point;
    return {b.x - a.x, b.y - a.y};
}
void combinations(const Sketch &s, const Constraint &c) {
    auto reason = constraintReason(s, c.kind, c.a, c.b, c.third);
    check(reason.isEmpty(), "UnsupportedOperation", reason);
    check(c.third.isEmpty() == !(c.kind == "Intersection" || c.kind == "Symmetric"), "InvalidInput",
          "Relation has the wrong third-operand contract");
    if (!c.driving)
        return;
    if (componentDimension(c.kind))
        check(std::abs(c.value) <= 1e3, "InvalidGeometry",
              "Component distance must be within ±1 km");
    if (c.kind == "Symmetric")
        check(c.value == 0 || c.value == 1, "InvalidInput",
              "Symmetry correspondence must be zero or one");
    if (c.kind == "Tangent" && s.entity(c.a).kind != "Line")
        check(c.value == 0 || c.value == 1, "InvalidInput",
              "Circular tangent branch must be external or internal");
    if (c.kind == "Intersection") {
        auto u = vector(s, s.entity(c.b)), v = vector(s, s.entity(c.third));
        check(std::abs(u.x * v.y - u.y * v.x) > 1e-8 * length(u, {}) * length(v, {}),
              "ReferenceAmbiguous", "Parallel/coincident lines have no unique intersection");
    }
    if (c.kind == "ArcTangent" || c.kind == "ArcNormal" || c.kind == "ArcCoincident")
        check((c.value == 0 || c.value == 1) && (c.second == 0 || c.second == 1) &&
                  (s.entity(c.b).kind != "Point" || c.second == 0),
              "InvalidInput", "Endpoint selectors must be zero or one");
    if (QStringList{"Length", "Distance", "Radius", "MinorRadius", "Diameter", "SlotLength",
                    "CenterlineDiameter", "ArcLength", "PathLength"}
            .contains(c.kind))
        check(c.value > 1e-9 && c.value <= 1e3, "InvalidGeometry",
              "Dimension must exceed 1 nm and be at most 1 km");
    if (c.kind == "SlotGeometry") {
        auto g = constructionOf(s, c.a);
        check(g && g->kind == "Slot" && curvedSlot(g->method) && g->diagonals[0] == c.a &&
                  g->edges[2] == c.b,
              "InvalidInput", "Slot geometry requires its identified centerline and cap");
    }
    if (c.kind == "Sweep")
        check(c.value > 1e-8 && c.value < tau - 1e-8, "InvalidGeometry",
              "Sweep must be between zero and 360 degrees");
    if (c.kind == "Angle" || c.kind == "StartAngle")
        check(std::abs(c.value) <= tau, "InvalidInput", "Angle must be within ±360 degrees");
    if (c.kind == "TrimStart")
        check(std::abs(c.value) <= 1000, "InvalidGeometry", "Trim start must be within ±1 km");
    if (c.kind == "TrimSpan")
        check(c.value > 1e-9 && c.value <= 2000, "InvalidGeometry",
              "Trim span must be positive and within 2 km");
}
std::vector<double> residuals(const Sketch &s) {
    std::vector<double> out;
    for (auto &c : s.constraints) {
        if (!c.driving)
            continue;
        auto &a = s.entity(c.a);
        auto p = a.kind == "Point" ? a.point : s.entity(a.a).point;
        auto push = [&](double n) { out.push_back(n); };
        if (c.kind == "CurveTangent") {
            auto &b = s.entity(c.b);
            auto p = curveValue(s, a, c.value), q = curveValue(s, b, c.second),
                 u = curveDerivative(s, a, c.value), v = curveDerivative(s, b, c.second);
            check(length(u, {}) > 1e-12 && length(v, {}) > 1e-12, "InvalidGeometry",
                  "Tangent contact has a zero derivative");
            push(p.x - q.x);
            push(p.y - q.y);
            push(std::remainder(std::atan2(u.x * v.y - u.y * v.x, u.x * v.x + u.y * v.y) -
                                    (c.reversed ? tau / 2 : 0),
                                tau));
        } else if (c.kind == "SlotGeometry") {
            auto g = constructionOf(s, c.a);
            auto &outer = s.entity(g->edges[0]), &inner = s.entity(g->edges[1]),
                 &cap = s.entity(c.b);
            push(outer.radius - a.radius - cap.radius);
            push(a.radius - inner.radius - cap.radius);
            push(std::remainder(outer.start - a.start, tau));
            push(outer.sweep - a.sweep);
        } else if (c.kind == "SlotLength") {
            push(length(s.entity(a.a).point, s.entity(a.b).point) + 2 * s.entity(c.b).radius -
                 c.value);
        } else if (c.kind == "Diameter")
            push(2 * a.radius - c.value);
        else if (c.kind == "StartAngle" || c.kind == "TrimStart")
            push(a.start - c.value);
        else if (c.kind == "Sweep" || c.kind == "TrimSpan")
            push(a.sweep - c.value);
        else if (c.kind == "ArcTangent" || c.kind == "ArcNormal" || c.kind == "ArcCoincident") {
            auto p = curveEnd(s, a, int(c.value));
            auto &b = s.entity(c.b);
            auto q = b.kind == "Point" ? b.point : curveEnd(s, b, int(c.second));
            push(p.x - q.x);
            push(p.y - q.y);
            if (c.kind != "ArcCoincident") {
                auto u = outgoing(s, a, int(c.value)), v = outgoing(s, b, int(c.second));
                push(c.kind == "ArcTangent" ? u.x * v.y - u.y * v.x : u.x * v.x + u.y * v.y);
            }
        } else if (c.kind == "Symmetric") {
            auto &axis = s.entity(c.third);
            auto origin = s.entity(axis.a).point, v = vector(s, axis);
            double n = length(v, {});
            Vec2 normal{-v.y / n, v.x / n};
            auto pair = [&](Vec2 first, Vec2 second) {
                double distance = (first.x - origin.x) * normal.x + (first.y - origin.y) * normal.y;
                push(second.x - first.x + 2 * distance * normal.x);
                push(second.y - first.y + 2 * distance * normal.y);
            };
            auto &b = s.entity(c.b);
            if (a.kind == "Point")
                pair(a.point, b.point);
            else if (a.kind == "Line") {
                pair(s.entity(a.a).point, s.entity(c.value == 0 ? b.a : b.b).point);
                pair(s.entity(a.b).point, s.entity(c.value == 0 ? b.b : b.a).point);
            } else {
                pair(s.entity(a.a).point, s.entity(b.a).point);
                if (a.kind == "Arc") {
                    pair(curveEnd(s, a, 0), curveEnd(s, b, 1));
                    pair(curveEnd(s, a, 1), curveEnd(s, b, 0));
                } else if (a.kind == "Ellipse") {
                    pair(s.entity(a.b).point, s.entity(b.b).point);
                    push(a.radius - b.radius);
                } else
                    push(a.radius - b.radius);
            }
        } else if (c.kind == "Intersection") {
            auto u = vector(s, s.entity(c.b)), v = vector(s, s.entity(c.third));
            check(std::abs(u.x * v.y - u.y * v.x) > 1e-8 * length(u, {}) * length(v, {}),
                  "ReferenceAmbiguous", "Intersection became ambiguous");
            for (auto id : {c.b, c.third}) {
                auto &line = s.entity(id);
                auto q = s.entity(line.a).point, w = vector(s, line);
                push(((p.x - q.x) * w.y - (p.y - q.y) * w.x) / length(w, {}));
            }
        } else if (c.kind == "Merge") {
            auto q = s.entity(c.b).point;
            push(p.x - q.x);
            push(p.y - q.y);
        } else if (c.kind == "Tangent" && a.kind != "Line") {
            auto &b = s.entity(c.b);
            push(length(p, s.entity(b.a).point) -
                 (c.value == 0 ? a.radius + b.radius : std::abs(a.radius - b.radius)));
        } else if (c.kind == "Fixed" || c.kind == "Pierce") {
            p = location(s, a);
            push(p.x - c.value);
            push(p.y - c.second);
        } else if ((c.kind == "Horizontal" || c.kind == "Vertical") && a.kind == "Point") {
            auto q = s.entity(c.b).point;
            push(c.kind == "Horizontal" ? p.y - q.y : p.x - q.x);
        } else if (c.kind == "Coincident") {
            auto &b = s.entity(c.b);
            if (b.kind == "Point") {
                push(p.x - b.point.x);
                push(p.y - b.point.y);
            } else if (ellipseCurve(b.kind)) {
                auto center = s.entity(b.a).point, focus = s.entity(b.b).point;
                Vec2 other{2 * center.x - focus.x, 2 * center.y - focus.y};
                push(length(p, focus) + length(p, other) -
                     2 * std::hypot(length(center, focus), b.radius));
            } else if (b.kind == "Parabola") {
                auto vertex = s.entity(b.a).point, focus = s.entity(b.b).point;
                double f = length(vertex, focus);
                Vec2 axis{(focus.x - vertex.x) / f, (focus.y - vertex.y) / f},
                    q{p.x - vertex.x, p.y - vertex.y};
                double x = q.x * axis.x + q.y * axis.y, y = -q.x * axis.y + q.y * axis.x;
                push((y * y - 4 * f * x) / (4 * f));
            } else if (b.kind == "Spline") {
                auto q = curveValue(s, b, c.second);
                push(p.x - q.x);
                push(p.y - q.y);
            } else if (b.kind == "Circle" || b.kind == "Arc")
                push(length(p, s.entity(b.a).point) - b.radius);
            else {
                auto q = s.entity(b.a).point, v = vector(s, b);
                push(((p.x - q.x) * v.y - (p.y - q.y) * v.x) / std::hypot(v.x, v.y));
            }
        } else if (c.kind == "Midpoint") {
            auto &b = s.entity(c.b);
            auto q = s.entity(b.a).point, r = s.entity(b.b).point;
            push(p.x - (q.x + r.x) / 2);
            push(p.y - (q.y + r.y) / 2);
        } else if (c.kind == "Concentric" || c.kind == "Coradial") {
            auto &b = s.entity(c.b);
            auto q = b.kind == "Point" ? b.point : s.entity(b.a).point;
            push(p.x - q.x);
            push(p.y - q.y);
            if (c.kind == "Coradial")
                push(a.radius - b.radius);
        } else if (c.kind == "Collinear") {
            auto &b = s.entity(c.b);
            auto v = vector(s, a), w = vector(s, b), q = s.entity(b.a).point;
            double l = std::hypot(v.x, v.y), m = std::hypot(w.x, w.y);
            push((v.x * w.y - v.y * w.x) / (l * m));
            push(((q.x - p.x) * v.y - (q.y - p.y) * v.x) / l);
        } else if (c.kind == "PathLength") {
            push(measureDimension(s, c) - c.value);
            for (int i = 0; i < c.members.size() - (c.closed ? 0 : 1); ++i) {
                auto a = pathEndpoint(s, c, i, true),
                     b = pathEndpoint(s, c, (i + 1) % c.members.size(), false);
                push(a.x - b.x);
                push(a.y - b.y);
            }
        } else if (c.kind == "CenterlineDiameter") {
            double side = c.reversed ? -1 : 1;
            push(2 * side * axisDistance(s, c) - c.value);
            if (a.kind == "Line")
                push(2 * side * axisDistance(s, c, true) - c.value);
        } else if (c.kind == "ArcLength")
            push(a.radius * a.sweep - c.value);
        else if (componentDimension(c.kind))
            push(measureDimension(s, c) - c.value);
        else if (c.kind == "Distance")
            push(length(p, s.entity(c.b).point) - c.value);
        else if (c.kind == "Radius" || c.kind == "MinorRadius")
            push(a.radius - c.value);
        else if (c.kind == "Equal" && (a.kind == "Circle" || a.kind == "Arc"))
            push(a.radius - s.entity(c.b).radius);
        else {
            auto v = vector(s, a);
            double l = std::hypot(v.x, v.y);
            if (c.kind == "Length")
                push(l - c.value);
            else if (c.kind == "Horizontal")
                push(v.y);
            else if (c.kind == "Vertical")
                push(v.x);
            else if (c.kind == "Tangent") {
                auto &circle = s.entity(c.b);
                auto center = s.entity(circle.a).point;
                double support = circle.radius;
                if (ellipseCurve(circle.kind)) {
                    auto focus = s.entity(circle.b).point;
                    double f = length(center, focus);
                    Vec2 axis{(focus.x - center.x) / f, (focus.y - center.y) / f},
                        normal{-v.y / l, v.x / l};
                    double x = normal.x * axis.x + normal.y * axis.y,
                           y = -normal.x * axis.y + normal.y * axis.x;
                    support = std::hypot(std::hypot(f, circle.radius) * x, circle.radius * y);
                }
                push(std::abs((center.x - p.x) * v.y - (center.y - p.y) * v.x) / l - support);
            } else {
                auto &b = s.entity(c.b);
                if (c.kind == "Equal" && a.kind == "Circle")
                    push(a.radius - b.radius);
                else {
                    auto w = vector(s, b);
                    double m = std::hypot(w.x, w.y);
                    if (c.kind == "Equal")
                        push(l - m);
                    else if (c.kind == "Parallel")
                        push((v.x * w.y - v.y * w.x) / (l * m));
                    else if (c.kind == "Perpendicular")
                        push((v.x * w.x + v.y * w.y) / (l * m));
                    else if (c.kind == "Angle")
                        push(std::remainder(std::atan2(w.y, w.x) - std::atan2(v.y, v.x) - c.value,
                                            tau));
                }
            }
        }
    }
    return out;
}
std::vector<double *> variables(Sketch &s) {
    std::vector<double *> out;
    for (auto &e : s.entities) {
        if (e.kind == "Point") {
            out.push_back(&e.point.x);
            out.push_back(&e.point.y);
        } else {
            if (radiusCurve(e.kind))
                out.push_back(&e.radius);
            if (e.kind == "Arc" || e.kind == "EllipticalArc" || e.kind == "Parabola") {
                out.push_back(&e.start);
                out.push_back(&e.sweep);
            }
        }
    }
    for (auto &c : s.constraints) {
        if (c.kind == "Coincident" && s.entity(c.b).kind == "Spline")
            out.push_back(&c.second);
        if (c.kind == "CurveTangent") {
            out.push_back(&c.value);
            out.push_back(&c.second);
        }
    }
    return out;
}
int freedom(Sketch &s) {
    auto vars = variables(s);
    auto residual = residuals(s);
    if (residual.empty())
        return int(vars.size());
    Eigen::MatrixXd jac(residual.size(), vars.size());
    for (size_t col = 0; col < vars.size(); ++col) {
        auto *v = vars[col];
        double saved = *v, h = 1e-6;
        double high = saved + h, low = saved - h;
        for (auto &c : s.constraints)
            if (c.kind == "Coincident" && s.entity(c.b).kind == "Spline" && v == &c.second) {
                auto range = curveRange(s.entity(c.b));
                high = std::min(high, range.second);
                low = std::max(low, range.first);
            }
        for (auto &c : s.constraints)
            if (c.kind == "CurveTangent")
                for (auto pair : {std::pair{&c.value, c.a}, std::pair{&c.second, c.b}})
                    if (v == pair.first) {
                        auto &curve = s.entity(pair.second);
                        if (curve.kind != "Circle" && curve.kind != "Ellipse") {
                            auto range = curveRange(curve);
                            high = std::min(high, range.second);
                            low = std::max(low, range.first);
                        }
                    }
        *v = high;
        auto plus = residuals(s);
        *v = low;
        auto minus = residuals(s);
        *v = saved;
        check(high > low, "SolveFailure", "Curve parameter left its trimmed range");
        for (size_t row = 0; row < residual.size(); ++row)
            jac(row, col) = (plus[row] - minus[row]) / (high - low);
    }
    check(jac.allFinite(), "SolveFailure", "Nonfinite constraint Jacobian");
    Eigen::ColPivHouseholderQR<Eigen::MatrixXd> qr(jac);
    qr.setThreshold(1e-7);
    return int(vars.size()) - int(qr.rank());
}
} // namespace
QCborMap Result::diagnostic() const {
    return fields({{"sketch_id", sketch.id},
                   {"status", status},
                   {"message", message},
                   {"valid", valid},
                   {"freedom", freedom},
                   {"maximum_normalized_residual", maximumResidual},
                   {"length_scale_si", lengthScale},
                   {"conflicting", QCborArray::fromStringList(conflicting)},
                   {"redundant", QCborArray::fromStringList(redundant)},
                   {"reference_candidates", referenceCandidates}});
}
struct DragTargets {
    std::vector<std::pair<QString, Vec2>> points;
    std::vector<std::pair<QString, double>> radii;
    QString circle;
    Vec2 cursor;
};
static void seedTangent(Sketch &s, const Constraint &c) {
    if (c.kind != "CurveTangent" || s.entity(c.a).kind != "Line" || constructionOf(s, c.a))
        return;
    auto &line = s.entity(c.a);
    auto &curve = s.entity(c.b);
    auto controls = controlPoints(curve);
    if (controls.contains(line.a) || controls.contains(line.b))
        return;
    auto contact = curveValue(s, curve, c.second), direction = curveDerivative(s, curve, c.second);
    double speed = std::hypot(direction.x, direction.y);
    double extent = length(s.entity(line.a).point, s.entity(line.b).point);
    double sign = c.reversed ? -1 : 1;
    if (speed > 1e-12) {
        Vec2 first{contact.x - sign * c.value * extent * direction.x / speed,
                   contact.y - sign * c.value * extent * direction.y / speed};
        Vec2 last{contact.x + sign * (1 - c.value) * extent * direction.x / speed,
                  contact.y + sign * (1 - c.value) * extent * direction.y / speed};
        // A local numerical warm start must not choose a distant contact or
        // reverse a curve on the user's behalf.
        if (std::max(length(first, s.entity(line.a).point), length(last, s.entity(line.b).point)) <=
            .01 * extent) {
            s.entity(line.a).point = first;
            s.entity(line.b).point = last;
        }
    }
}
static Result solveLocal(Sketch candidate, const Sketch *previous, bool referencesResolved,
                         const DragTargets *dragTargets = nullptr) {
    Result result;
    result.sketch = candidate;
    try {
        candidate.validate();
        for (auto &c : candidate.constraints)
            check(c.kind != "Pierce" || referencesResolved, "ReferenceMissing",
                  "Pierce requires dependency evaluation with its source sketch");
        expression::Environment names(candidate.names);
        names.validateAll();
        for (auto &c : candidate.constraints) {
            check(dimensional(c.kind) || c.expression.isEmpty(), "InvalidInput",
                  "Expressions apply only to dimensional constraints");
            if (!c.expression.isEmpty()) {
                auto q = names.evaluate(c.expression);
                check(q.dimension ==
                          (angularDimension(c.kind) ? expression::angle : expression::length),
                      "UnitMismatch", "Dimension expression has the wrong physical dimension");
                c.value = q.value;
            }
            combinations(candidate, c);
        }
        for (auto &c : candidate.constraints)
            if (!previous ||
                std::none_of(previous->constraints.begin(), previous->constraints.end(),
                             [&](auto &old) { return old.id == c.id; }))
                seedTangent(candidate, c);
        candidate.validate();
        result.sketch = candidate;
        if (!dragTargets && std::none_of(candidate.constraints.begin(), candidate.constraints.end(),
                                         [](auto &c) { return c.driving; })) {
            auto count = variables(candidate).size();
            check(count <= 256, "ResourceLimit", "At most 256 numerical sketch variables");
            for (auto &c : candidate.constraints)
                c.value = measureDimension(candidate, c);
            result.sketch = candidate;
            result.valid = true;
            result.freedom = int(count);
            result.status = count ? "Underdefined" : "FullyDefined";
            return result;
        }
        // Normalize lengths before handing pointer-owned variables to the solver.
        Vec2 origin;
        for (auto &e : candidate.entities)
            if (e.kind == "Point") {
                origin = e.point;
                break;
            }
        double scale = 1e-6;
        for (auto &e : candidate.entities) {
            if (e.kind == "Point")
                scale = std::max(scale, length(e.point, origin));
            else if (radiusCurve(e.kind))
                scale = std::max(scale, e.radius);
            else if (e.kind == "Parabola")
                scale = std::max({scale, std::abs(e.start), std::abs(e.start + e.sweep)});
        }
        result.lengthScale = scale;
        for (auto &e : candidate.entities) {
            if (e.kind == "Point") {
                e.point.x = (e.point.x - origin.x) / scale;
                e.point.y = (e.point.y - origin.y) / scale;
            } else if (radiusCurve(e.kind))
                e.radius /= scale;
            else if (e.kind == "Parabola") {
                e.start /= scale;
                e.sweep /= scale;
            }
        }
        for (auto &c : candidate.constraints) {
            if (c.kind == "Fixed" || c.kind == "Pierce") {
                c.value = (c.value - origin.x) / scale;
                c.second = (c.second - origin.y) / scale;
            } else if (c.kind == "CurveTangent") {
                if (candidate.entity(c.a).kind == "Parabola")
                    c.value /= scale;
                if (candidate.entity(c.b).kind == "Parabola")
                    c.second /= scale;
            } else if (componentDimension(c.kind) ||
                       QStringList{"TrimStart", "TrimSpan", "Length", "Distance", "Radius",
                                   "MinorRadius", "Diameter", "SlotLength", "CenterlineDiameter",
                                   "ArcLength", "PathLength"}
                           .contains(c.kind))
                c.value /= scale;
        }
        auto vars = variables(candidate);
        check(vars.size() <= 256, "ResourceLimit", "At most 256 numerical sketch variables");
        std::deque<double> constants;
        auto constant = [&](double n) {
            constants.push_back(n);
            return &constants.back();
        };
        QMap<QString, GCS::Point> points;
        QMap<QString, GCS::Line> lines;
        QMap<QString, GCS::Circle> circles;
        QMap<QString, GCS::Arc> arcs;
        QMap<QString, GCS::Ellipse> ellipses;
        QMap<QString, GCS::ArcOfParabola> parabolas;
        QMap<QString, GCS::Parabola> exactParabolas;
        QMap<QString, GCS::BSpline> splines;
        QSet<QString> joinedArcs;
        for (auto &c : candidate.constraints)
            if (c.driving && c.kind == "PathLength")
                for (auto &id : c.members)
                    if (candidate.entity(id).kind == "Arc")
                        joinedArcs.insert(id);
        for (const auto &c : candidate.constraints)
            if (c.kind == "ArcTangent" || c.kind == "ArcNormal" || c.kind == "ArcCoincident") {
                joinedArcs.insert(c.a);
                if (candidate.entity(c.b).kind == "Arc")
                    joinedArcs.insert(c.b);
            }
        for (auto &c : candidate.constraints)
            if (c.kind == "Symmetric" && candidate.entity(c.a).kind == "Arc") {
                joinedArcs.insert(c.a);
                joinedArcs.insert(c.b);
            }
        for (auto &g : candidate.groups)
            if (g.kind == "Slot" && curvedSlot(g.method))
                joinedArcs.insert(g.diagonals[0]);
        std::deque<double> endpointVariables;
        auto endpointVariable = [&](double value) {
            endpointVariables.push_back(value);
            vars.push_back(&endpointVariables.back());
            return &endpointVariables.back();
        };
        for (auto &e : candidate.entities)
            if (e.kind == "Point")
                points.insert(e.id, GCS::Point{&e.point.x, &e.point.y});
        for (auto &e : candidate.entities) {
            if (e.kind == "Line") {
                GCS::Line l;
                l.p1 = points[e.a];
                l.p2 = points[e.b];
                lines.insert(e.id, l);
            } else if (ellipseCurve(e.kind)) {
                GCS::Ellipse curve;
                curve.center = points[e.a];
                curve.focus1 = points[e.b];
                curve.radmin = &e.radius;
                ellipses.insert(e.id, curve);
            } else if (e.kind == "Parabola") {
                GCS::ArcOfParabola curve;
                curve.vertex = points[e.a];
                curve.focus1 = points[e.b];
                parabolas.insert(e.id, curve);
                GCS::Parabola full;
                full.vertex = curve.vertex;
                full.focus1 = curve.focus1;
                exactParabolas.insert(e.id, full);
            } else if (e.kind == "Spline") {
                GCS::BSpline curve;
                curve.degree = e.degree;
                curve.periodic = false;
                for (auto id : e.poles)
                    curve.poles.push_back(points[id]);
                for (auto w : e.weights)
                    curve.weights.push_back(constant(w));
                for (auto k : e.knots)
                    curve.knots.push_back(constant(k));
                curve.mult = e.multiplicities;
                curve.start = points[e.poles[0]];
                curve.end = points[e.poles.last()];
                curve.setupFlattenedKnots();
                splines.insert(e.id, curve);
            } else if (e.kind == "Circle" || e.kind == "Arc") {
                GCS::Circle c;
                c.center = points[e.a];
                c.rad = &e.radius;
                circles.insert(e.id, c);
                if (joinedArcs.contains(e.id)) {
                    auto first = curveEnd(candidate, e, 0), last = curveEnd(candidate, e, 1);
                    GCS::Arc arc;
                    arc.center = c.center;
                    arc.rad = c.rad;
                    arc.start = {endpointVariable(first.x), endpointVariable(first.y)};
                    arc.end = {endpointVariable(last.x), endpointVariable(last.y)};
                    e.sweep += e.start; // GCS uses an absolute end angle during this solve only.
                    arc.startAngle = &e.start;
                    arc.endAngle = &e.sweep;
                    arcs.insert(e.id, arc);
                }
            }
        }
        GCS::System system;
        system.maxIter = 100;
        system.maxIterRedundant = 100;
        system.sketchSizeMultiplier = false;
        system.sketchSizeMultiplierRedundant = false;
        system.convergence = 1e-10;
        if (dragTargets)
            for (auto &[id, target] : dragTargets->points) {
                auto point = points[id];
                // Negative tags are the pinned PlaneGCS auxiliary subsystem:
                // approach the cursor while preserving the driving equations.
                system.addConstraintCoordinateX(point, constant((target.x - origin.x) / scale), -1);
                system.addConstraintCoordinateY(point, constant((target.y - origin.y) / scale), -1);
            }
        if (dragTargets) {
            for (auto &[id, target] : dragTargets->radii)
                system.addConstraintEqual(&candidate.entity(id).radius, constant(target / scale),
                                          -1);
            if (!dragTargets->circle.isEmpty()) {
                GCS::Point cursor{constant((dragTargets->cursor.x - origin.x) / scale),
                                  constant((dragTargets->cursor.y - origin.y) / scale)};
                auto index =
                    system.addConstraintPointOnCircle(cursor, circles[dragTargets->circle], -1);
                system.rescaleConstraint(index, 1000);
            }
        }
        check(vars.size() <= 256, "ResourceLimit",
              "At most 256 numerical variables including arc endpoints");
        for (auto &arc : arcs)
            system.addConstraintArcRules(arc);
        auto endpoint = [&](QString id, int end) -> GCS::Point {
            auto &e = candidate.entity(id);
            if (e.kind == "Point")
                return points[id];
            if (e.kind == "Line")
                return end ? lines[id].p2 : lines[id].p1;
            return end ? arcs[id].end : arcs[id].start;
        };
        auto exactCurve = [&](QString id) -> GCS::Curve & {
            auto kind = candidate.entity(id).kind;
            if (kind == "Line")
                return lines[id];
            if (kind == "Circle" || kind == "Arc")
                return circles[id];
            if (ellipseCurve(kind))
                return ellipses[id];
            if (kind == "Parabola")
                return exactParabolas[id];
            return splines[id];
        };
        int tag = 0;
        for (auto &c : candidate.constraints) {
            ++tag;
            if (!c.driving)
                continue;
            auto a = c.a, b = c.b;
            if (c.kind == "CurveTangent") {
                auto p = curveValue(candidate, candidate.entity(a), c.value),
                     q = curveValue(candidate, candidate.entity(b), c.second);
                GCS::Point contact{endpointVariable((p.x + q.x) / 2),
                                   endpointVariable((p.y + q.y) / 2)};
                for (auto pair : {std::pair{a, &c.value}, std::pair{b, &c.second}}) {
                    auto &entity = candidate.entity(pair.first);
                    auto range = curveRange(entity);
                    if (entity.kind == "Arc" && joinedArcs.contains(pair.first))
                        range = {*arcs[pair.first].startAngle, *arcs[pair.first].endAngle};
                    if (entity.kind == "Ellipse" || entity.kind == "Circle")
                        range = {-tau, 2 * tau};
                    check(*pair.second >= range.first && *pair.second <= range.second,
                          "InvalidInput",
                          "Contact parameter lies outside its selected curve range");
                    for (int axis = 0; axis < 2; ++axis)
                        system.addConstraint(new CurveCoordinate(exactCurve(pair.first), contact,
                                                                 pair.second, axis, range.first,
                                                                 range.second, tag));
                }
                system.addConstraintAngleViaPointAndTwoParams(
                    exactCurve(a), exactCurve(b), contact, &c.value, &c.second,
                    constant(c.reversed ? tau / 2 : 0), tag);
            } else if (c.kind == "SlotGeometry") {
                auto g = constructionOf(candidate, a);
                auto &mid = arcs[a], &outer = arcs[g->edges[0]], &inner = arcs[g->edges[1]];
                system.addConstraintDifference(mid.rad, outer.rad, circles[b].rad, tag);
                system.addConstraintDifference(inner.rad, mid.rad, circles[b].rad, tag);
                system.addConstraintEqual(outer.startAngle, mid.startAngle, tag);
                system.addConstraintEqual(outer.endAngle, mid.endAngle, tag);
            } else if (c.kind == "SlotLength") {
                auto distance =
                    endpointVariable(length(candidate.entity(candidate.entity(a).a).point,
                                            candidate.entity(candidate.entity(a).b).point));
                auto diameter = endpointVariable(2 * candidate.entity(b).radius);
                system.addConstraintP2PDistance(lines[a].p1, lines[a].p2, distance, tag);
                // The pinned Equal reduction aliases two unknowns even for a ratio.
                // Express diameter - radius = radius without that reduction path.
                system.addConstraintDifference(circles[b].rad, diameter, circles[b].rad, tag);
                system.addConstraintDifference(distance, constant(c.value), diameter, tag);
            } else if (c.kind == "PathLength") {
                std::vector<GCS::Line> edges;
                bool curves = false;
                for (auto &id : c.members)
                    if (candidate.entity(id).kind == "Line")
                        edges.push_back(lines[id]);
                    else
                        curves = true;
                auto target = constant(c.value);
                if (!curves)
                    system.addConstraint(new PathLengthConstraint(edges, target, tag));
                else
                    system.addConstraint(
                        new CurvePathConstraint(candidate, c, joinedArcs, target, tag));
                for (int i = 0; i < c.members.size() - (c.closed ? 0 : 1); ++i) {
                    int next = (i + 1) % c.members.size();
                    auto &a = candidate.entity(c.members[i]),
                         &b = candidate.entity(c.members[next]);
                    if (a.id == b.id && (a.kind == "Circle" || a.kind == "Ellipse"))
                        continue;
                    if (a.kind == "Line" && b.kind == "Line" &&
                        (c.directions[i] ? a.a : a.b) == (c.directions[next] ? b.b : b.a))
                        continue;
                    for (int axis = 0; axis < 2; ++axis)
                        system.addConstraint(new CurvePathConstraint(candidate, c, joinedArcs,
                                                                     target, tag, i, axis));
                }
            } else if (c.kind == "CenterlineDiameter") {
                auto target = constant((c.reversed ? -1 : 1) * c.value / 2);
                if (candidate.entity(a).kind == "Point")
                    system.addConstraint(
                        new AxisDistanceConstraint(points[a], lines[b], target, tag));
                else {
                    system.addConstraint(
                        new AxisDistanceConstraint(lines[a].p1, lines[b], target, tag));
                    system.addConstraint(
                        new AxisDistanceConstraint(lines[a].p2, lines[b], target, tag));
                }
            } else if (c.kind == "ArcLength") {
                auto &arc = candidate.entity(a);
                system.addConstraint(new ArcLengthConstraint(
                    &arc.radius, joinedArcs.contains(a) ? &arc.start : constant(0), &arc.sweep,
                    constant(c.value), tag));
            } else if (c.kind == "Diameter")
                system.addConstraintCircleRadius(circles[a], constant(c.value / 2), tag);
            else if (c.kind == "StartAngle" || c.kind == "TrimStart")
                system.addConstraintEqual(&candidate.entity(a).start, constant(c.value), tag);
            else if (c.kind == "TrimSpan")
                system.addConstraintEqual(&candidate.entity(a).sweep, constant(c.value), tag);
            else if (c.kind == "Sweep") {
                if (joinedArcs.contains(a))
                    system.addConstraintDifference(arcs[a].startAngle, arcs[a].endAngle,
                                                   constant(c.value), tag);
                else
                    system.addConstraintEqual(&candidate.entity(a).sweep, constant(c.value), tag);
            } else if (c.kind == "ArcTangent" || c.kind == "ArcNormal" ||
                       c.kind == "ArcCoincident") {
                auto p = endpoint(a, int(c.value)), q = endpoint(b, int(c.second));
                system.addConstraintP2PCoincident(p, q, tag);
                if (c.kind != "ArcCoincident") {
                    auto center = points[candidate.entity(a).a];
                    auto &other = candidate.entity(b);
                    auto u = other.kind == "Line" ? lines[b].p1 : points[other.a];
                    auto v = other.kind == "Line" ? lines[b].p2 : q;
                    double ax = *p.x - *center.x, ay = *p.y - *center.y;
                    double bx = *v.x - *u.x, by = *v.y - *u.y;
                    bool perpendicular = (c.kind == "ArcTangent") == (other.kind == "Line");
                    double target = perpendicular ? ((ax * by - ay * bx >= 0) ? tau / 4 : -tau / 4)
                                                  : ((ax * bx + ay * by >= 0) ? 0 : tau / 2);
                    system.addConstraintL2LAngle(center, p, u, v, constant(target), tag);
                }
            } else if (c.kind == "Symmetric") {
                auto &axis = lines[c.third];
                auto &first = candidate.entity(a), &second = candidate.entity(b);
                auto pair = [&](GCS::Point p, GCS::Point q) {
                    system.addConstraintP2PSymmetric(p, q, axis, tag);
                };
                if (first.kind == "Point")
                    pair(points[a], points[b]);
                else if (first.kind == "Line") {
                    pair(lines[a].p1, c.value == 0 ? lines[b].p1 : lines[b].p2);
                    pair(lines[a].p2, c.value == 0 ? lines[b].p2 : lines[b].p1);
                } else {
                    pair(points[first.a], points[second.a]);
                    if (first.kind == "Arc") {
                        pair(arcs[a].start, arcs[b].end);
                        pair(arcs[a].end, arcs[b].start);
                    } else if (first.kind == "Ellipse") {
                        pair(points[first.b], points[second.b]);
                        system.addConstraintEqual(ellipses[a].radmin, ellipses[b].radmin, tag);
                    } else
                        system.addConstraintEqualRadius(circles[a], circles[b], tag);
                }
            } else if (c.kind == "Intersection") {
                system.addConstraintPointOnLine(points[a], lines[b], tag);
                system.addConstraintPointOnLine(points[a], lines[c.third], tag);
            } else if (c.kind == "Merge")
                system.addConstraintP2PCoincident(points[a], points[b], tag);
            else if (c.kind == "Fixed" || c.kind == "Pierce") {
                auto &entity = candidate.entity(a);
                if (entity.kind == "Line") {
                    GCS::Point target{constant(c.value), constant(c.second)};
                    system.addConstraintP2PSymmetric(lines[a].p1, lines[a].p2, target, tag);
                } else {
                    auto point = points[entity.kind == "Point" ? a : entity.a];
                    system.addConstraintCoordinateX(point, constant(c.value), tag);
                    system.addConstraintCoordinateY(point, constant(c.second), tag);
                }
            } else if (c.kind == "Coincident") {
                auto kind = candidate.entity(b).kind;
                if (kind == "Point")
                    system.addConstraintP2PCoincident(points[a], points[b], tag);
                else if (kind == "Line")
                    system.addConstraintPointOnLine(points[a], lines[b], tag);
                else if (ellipseCurve(kind))
                    system.addConstraintPointOnEllipse(points[a], ellipses[b], tag);
                else if (kind == "Parabola")
                    system.addConstraintPointOnParabolicArc(points[a], parabolas[b], tag);
                else if (kind == "Spline")
                    system.addConstraintPointOnBSpline(points[a], splines[b], &c.second, tag);
                else
                    system.addConstraintPointOnCircle(points[a], circles[b], tag);
            } else if (c.kind == "Horizontal") {
                if (candidate.entity(a).kind == "Point")
                    system.addConstraintHorizontal(points[a], points[b], tag);
                else
                    system.addConstraintHorizontal(lines[a], tag);
            } else if (c.kind == "Vertical") {
                if (candidate.entity(a).kind == "Point")
                    system.addConstraintVertical(points[a], points[b], tag);
                else
                    system.addConstraintVertical(lines[a], tag);
            } else if (c.kind == "Midpoint") {
                system.addConstraintPointOnPerpBisector(points[a], lines[b], tag);
                system.addConstraintPointOnLine(points[a], lines[b], tag);
            } else if (c.kind == "Collinear") {
                system.addConstraintParallel(lines[a], lines[b], tag);
                system.addConstraintPointOnLine(lines[b].p1, lines[a], tag);
            } else if (c.kind == "Concentric" || c.kind == "Coradial") {
                auto center = [&](QString id) {
                    auto &e = candidate.entity(id);
                    return points[e.kind == "Point" ? id : e.a];
                };
                auto firstCenter = center(a), secondCenter = center(b);
                system.addConstraintP2PCoincident(firstCenter, secondCenter, tag);
                if (c.kind == "Coradial")
                    system.addConstraintEqualRadius(circles[a], circles[b], tag);
            } else if (c.kind == "Parallel")
                system.addConstraintParallel(lines[a], lines[b], tag);
            else if (c.kind == "Perpendicular")
                system.addConstraintPerpendicular(lines[a], lines[b], tag);
            else if (c.kind == "Angle")
                system.addConstraintL2LAngle(lines[a], lines[b], constant(c.value), tag);
            else if (componentDimension(c.kind)) {
                auto p = candidate.entity(a).kind == "Line" ? lines[a].p1 : points[a];
                auto q = candidate.entity(a).kind == "Line" ? lines[a].p2 : points[b];
                system.addConstraintDifference(horizontalDimension(c.kind) ? p.x : p.y,
                                               horizontalDimension(c.kind) ? q.x : q.y,
                                               constant(c.value), tag);
            } else if (c.kind == "Length")
                system.addConstraintP2PDistance(lines[a].p1, lines[a].p2, constant(c.value), tag);
            else if (c.kind == "Distance")
                system.addConstraintP2PDistance(points[a], points[b], constant(c.value), tag);
            else if (c.kind == "MinorRadius")
                system.addConstraintEqual(&candidate.entity(a).radius, constant(c.value), tag);
            else if (c.kind == "Radius")
                system.addConstraintCircleRadius(circles[a], constant(c.value), tag);
            else if (c.kind == "Tangent") {
                if (candidate.entity(a).kind != "Line")
                    system.addConstraintTangentCircumf(circles[a].center, circles[b].center,
                                                       circles[a].rad, circles[b].rad, c.value == 1,
                                                       tag);
                else if (ellipseCurve(candidate.entity(b).kind))
                    system.addConstraintTangent(lines[a], ellipses[b], tag);
                else
                    system.addConstraintTangent(lines[a], circles[b], tag);
            } else if (c.kind == "Equal") {
                if (candidate.entity(a).kind == "Line")
                    system.addConstraintEqualLength(lines[a], lines[b], tag);
                else
                    system.addConstraintEqualRadius(circles[a], circles[b], tag);
            }
        }
        check(vars.size() <= 256, "ResourceLimit",
              "At most 256 numerical variables including derived dimension helpers");
        GCS::VEC_pD unknowns(vars.begin(), vars.end());
        system.declareUnknowns(unknowns);
        system.initSolution();
        system.diagnose();
        GCS::VEC_I conflicts, redundants, partial;
        system.getConflicting(conflicts);
        system.getRedundant(redundants);
        system.getPartiallyRedundant(partial);
        redundants.insert(redundants.end(), partial.begin(), partial.end());
        for (int t : conflicts)
            if (t > 0 && t <= tag)
                result.conflicting.append(candidate.constraints[t - 1].id);
        for (int t : redundants)
            if (t > 0 && t <= tag && !result.redundant.contains(candidate.constraints[t - 1].id))
                result.redundant.append(candidate.constraints[t - 1].id);
        if (!conflicts.empty()) {
            result.status = "Inconsistent";
            result.message = "Conflicting constraints: " + result.conflicting.mid(0, 16).join(", ");
            return result;
        }
        check(system.solve(true, GCS::DogLeg, !redundants.empty()) == GCS::Success, "SolveFailure",
              "PlaneGCS failed to converge");
        system.applySolution();
        for (auto *v : vars)
            check(std::isfinite(*v), "SolveFailure", "Nonfinite solver output");
        for (auto &e : candidate.entities)
            if (joinedArcs.contains(e.id)) {
                e.sweep -= e.start;
                e.start = std::remainder(e.start, tau);
            }
        auto residual = residuals(candidate);
        for (double n : residual) {
            check(std::isfinite(n), "SolveFailure", "Nonfinite residual");
            result.maximumResidual = std::max(result.maximumResidual, std::abs(n));
        }
        check(result.maximumResidual <= 1e-7, "SolveFailure",
              "Solved constraint residual exceeds declared tolerance");
        result.freedom = freedom(candidate);
        for (auto &e : candidate.entities) {
            if (e.kind == "Point") {
                e.point.x = e.point.x * scale + origin.x;
                e.point.y = e.point.y * scale + origin.y;
            } else if (radiusCurve(e.kind))
                e.radius *= scale;
            else if (e.kind == "Parabola") {
                e.start *= scale;
                e.sweep *= scale;
            }
        }
        auto resolved = result.sketch.constraints;
        for (size_t i = 0; i < resolved.size(); ++i)
            if (resolved[i].kind == "CurveTangent") {
                auto convert = [&](QString id, double value) {
                    auto &e = candidate.entity(id);
                    if (e.kind == "Parabola")
                        return value * scale;
                    if (e.kind == "Ellipse" || e.kind == "Circle")
                        return std::fmod(value + 2 * tau, tau);
                    return value;
                };
                resolved[i].value = convert(resolved[i].a, candidate.constraints[i].value);
                resolved[i].second = convert(resolved[i].b, candidate.constraints[i].second);
            }
        for (size_t i = 0; i < resolved.size(); ++i)
            if (resolved[i].kind == "Coincident" &&
                candidate.entity(resolved[i].b).kind == "Spline")
                resolved[i].second = candidate.constraints[i].second;
        candidate.constraints = std::move(resolved);
        for (auto &c : candidate.constraints)
            if (!c.driving)
                c.value = measureDimension(candidate, c);
        for (auto &c : candidate.constraints)
            if (c.kind == "Coincident") {
                auto &curve = candidate.entity(c.b);
                auto p = candidate.entity(c.a).point;
                if (curve.kind == "Spline") {
                    auto range = curveRange(curve);
                    check(c.second >= range.first - 1e-10 && c.second <= range.second + 1e-10,
                          "ReferenceAmbiguous", "Spline contact left its selected trimmed range");
                } else if (curve.kind == "Arc" || curve.kind == "EllipticalArc" ||
                           curve.kind == "Parabola") {
                    auto center = candidate.entity(curve.a).point;
                    Vec2 q{p.x - center.x, p.y - center.y};
                    double parameter = 0;
                    if (curve.kind == "Arc")
                        parameter = std::atan2(q.y, q.x);
                    else {
                        auto focus = candidate.entity(curve.b).point;
                        double f = length(center, focus);
                        Vec2 axis{(focus.x - center.x) / f, (focus.y - center.y) / f};
                        double x = q.x * axis.x + q.y * axis.y, y = -q.x * axis.y + q.y * axis.x;
                        parameter =
                            curve.kind == "Parabola"
                                ? y
                                : std::atan2(y / curve.radius, x / std::hypot(f, curve.radius));
                    }
                    double delta = curve.kind == "Parabola"
                                       ? parameter - curve.start
                                       : std::fmod(parameter - curve.start + 2 * tau, tau);
                    if (curve.kind != "Parabola" && std::abs(delta - tau) < 1e-8)
                        delta = 0;
                    double tolerance =
                        curve.kind == "Parabola" ? std::max(1e-11, scale * 1e-8) : 1e-8;
                    check(delta >= -tolerance && delta <= curve.sweep + tolerance,
                          "ReferenceAmbiguous", "Point lies outside the selected curve trim");
                }
            }
        auto checkTrim = [&](const Entity &curve, Vec2 p) {
            if (curve.kind != "Arc" && curve.kind != "EllipticalArc")
                return;
            auto center = candidate.entity(curve.a).point;
            Vec2 q{p.x - center.x, p.y - center.y};
            double parameter = std::atan2(q.y, q.x);
            if (curve.kind == "EllipticalArc") {
                auto focus = candidate.entity(curve.b).point;
                double f = length(center, focus);
                Vec2 axis{(focus.x - center.x) / f, (focus.y - center.y) / f};
                parameter = std::atan2((-q.x * axis.y + q.y * axis.x) / curve.radius,
                                       (q.x * axis.x + q.y * axis.y) / std::hypot(f, curve.radius));
            }
            double delta = std::fmod(parameter - curve.start + 2 * tau, tau);
            if (std::abs(delta - tau) < 1e-8)
                delta = 0;
            check(delta <= curve.sweep + 1e-8, "ReferenceAmbiguous",
                  "Tangent contact lies outside the selected curve trim");
        };
        for (auto &c : candidate.constraints)
            if (c.kind == "Tangent") {
                auto &a = candidate.entity(c.a), &b = candidate.entity(c.b);
                Vec2 contact;
                if (a.kind == "Line") {
                    auto origin = candidate.entity(a.a).point, v = vector(candidate, a),
                         center = candidate.entity(b.a).point;
                    double t = ((center.x - origin.x) * v.x + (center.y - origin.y) * v.y) /
                               (v.x * v.x + v.y * v.y);
                    contact = {origin.x + t * v.x, origin.y + t * v.y};
                    if (ellipseCurve(b.kind)) {
                        auto focus = candidate.entity(b.b).point;
                        double f = length(center, focus), major = std::hypot(f, b.radius),
                               n = length(v, {});
                        Vec2 axis{(focus.x - center.x) / f, (focus.y - center.y) / f},
                            normal{-v.y / n, v.x / n};
                        double x = normal.x * axis.x + normal.y * axis.y,
                               y = -normal.x * axis.y + normal.y * axis.x;
                        double h = std::hypot(major * x, b.radius * y),
                               sign = ((contact.x - center.x) * normal.x +
                                       (contact.y - center.y) * normal.y) >= 0
                                          ? 1
                                          : -1;
                        double px = sign * major * major * x / h,
                               py = sign * b.radius * b.radius * y / h;
                        contact = {center.x + px * axis.x - py * axis.y,
                                   center.y + px * axis.y + py * axis.x};
                    }
                } else {
                    auto p = candidate.entity(a.a).point, q = candidate.entity(b.a).point;
                    double d = length(p, q);
                    check(d > 1e-9, "ReferenceAmbiguous",
                          "Concentric circles have no unique tangent contact");
                    double sign = c.value == 1 && b.radius > a.radius ? -1 : 1;
                    contact = {p.x + sign * a.radius * (q.x - p.x) / d,
                               p.y + sign * a.radius * (q.y - p.y) / d};
                    checkTrim(a, contact);
                }
                checkTrim(b, contact);
            }
        candidate.validate();
        if (previous) {
            auto drivers = [](const Sketch &s) {
                return std::count_if(s.constraints.begin(), s.constraints.end(),
                                     [](auto &c) { return c.driving; });
            };
            bool same = drivers(candidate) > 0 && drivers(candidate) == drivers(*previous);
            for (auto &b : candidate.constraints) {
                if (!b.driving)
                    continue;
                auto i = std::find_if(previous->constraints.begin(), previous->constraints.end(),
                                      [&](auto &a) { return a.driving && a.id == b.id; });
                if (i == previous->constraints.end() || i->value != b.value ||
                    i->second != b.second || i->expression != b.expression || i->kind != b.kind ||
                    i->a != b.a || i->b != b.b || i->third != b.third ||
                    i->reversed != b.reversed || i->members != b.members ||
                    i->directions != b.directions || i->closed != b.closed)
                    same = false;
            }
            if (same)
                for (auto &e : candidate.entities)
                    if (e.kind == "Line") {
                        auto it = std::find_if(
                            previous->entities.begin(), previous->entities.end(),
                            [&](auto &old) { return old.id == e.id && old.kind == "Line"; });
                        if (it != previous->entities.end()) {
                            auto before = vector(*previous, *it), after = vector(candidate, e);
                            check(before.x * after.x + before.y * after.y >= 0, "SolveFailure",
                                  "Drag would flip a line branch; use smaller steps or edit "
                                  "constraints explicitly");
                        }
                    }
            if (same)
                for (auto &c : candidate.constraints)
                    if (c.kind == "Tangent" && candidate.entity(c.a).kind == "Line") {
                        auto side = [&](const Sketch &s) {
                            auto &line = s.entity(c.a);
                            auto v = vector(s, line);
                            auto a = s.entity(line.a).point, p = s.entity(s.entity(c.b).a).point;
                            return v.x * (p.y - a.y) - v.y * (p.x - a.x);
                        };
                        check(side(*previous) * side(candidate) > 0, "SolveFailure",
                              "Drag would switch tangent sides; use smaller steps or edit "
                              "constraints explicitly");
                    }
            if (same)
                for (auto &c : candidate.constraints)
                    if (c.kind == "ArcTangent" || c.kind == "ArcNormal") {
                        auto branch = [&](const Sketch &s) {
                            auto a = outgoing(s, s.entity(c.a), int(c.value)),
                                 b = outgoing(s, s.entity(c.b), int(c.second));
                            return c.kind == "ArcTangent" ? a.x * b.x + a.y * b.y
                                                          : a.x * b.y - a.y * b.x;
                        };
                        check(
                            branch(*previous) * branch(candidate) > 0, "SolveFailure",
                            "Edit would flip the arc join branch; replace the relation explicitly");
                    }
        }
        result.sketch = candidate;
        result.valid = true;
        result.status = !result.redundant.empty() ? "Redundant"
                        : result.freedom == 0     ? "FullyDefined"
                                                  : "Underdefined";
    } catch (const domain::Error &e) {
        result.status = e.code == "UnsupportedOperation" ? "Unsupported" : "FailedSolve";
        result.message = e.code + ": " + e.what();
    }
    return result;
}
Result solve(Sketch candidate, const Sketch *previous) {
    return solveLocal(std::move(candidate), previous, false);
}
QCborMap evaluate(const QCborArray &input, const QCborArray &previous, bool rejectInvalid) {
    auto all = parse(input), old = parse(previous);
    QCborArray diagnostics;
    std::vector<size_t> order;
    try {
        order = dependencyOrder(all);
    } catch (const domain::Error &e) {
        if (rejectInvalid)
            throw;
        for (auto &s : all) {
            Result r;
            r.sketch = s;
            r.message = e.code + ": " + e.what();
            diagnostics.append(r.diagnostic());
        }
        return fields({{"sketches", encode(all)}, {"diagnostics", diagnostics}});
    }
    QSet<QString> failed;
    for (auto index : order) {
        auto &s = all[index];
        auto it = std::find_if(old.begin(), old.end(), [&](auto &v) { return v.id == s.id; });
        Result r;
        r.sketch = s;
        try {
            auto candidate = s;
            for (auto &c : candidate.constraints)
                if (c.reference) {
                    check(!failed.contains(c.reference->sketch), "ReferenceMissing",
                          "Pierce source failed evaluation; repair it first");
                    auto source = std::find_if(all.begin(), all.end(), [&](auto &v) {
                        return v.id == c.reference->sketch;
                    });
                    check(source != all.end(), "ReferenceMissing",
                          "Pierce source sketch is missing");
                    resolvePierce(candidate, c, *source);
                }
            r = solveLocal(std::move(candidate), it == old.end() ? nullptr : &*it, true);
        } catch (const ReferenceFailure &e) {
            r.message = e.code + ": " + e.what();
            r.referenceCandidates = e.candidates;
        } catch (const domain::Error &e) {
            r.message = e.code + ": " + e.what();
        }
        diagnostics.append(r.diagnostic());
        if (rejectInvalid)
            check(r.valid, r.status == "Inconsistent" ? "ConstraintConflict" : "SolveFailure",
                  r.message.isEmpty() ? r.status : r.message);
        if (r.valid)
            s = r.sketch;
        else
            failed.insert(s.id);
    }
    return fields({{"sketches", encode(all)}, {"diagnostics", diagnostics}});
}
static QCborMap infer(const QCborArray &input, const QCborMap &request,
                      const QCborArray &previous) {
    wire::keys(request, {"kind", "sketch", "selection", "tolerance"});
    check(request.value("selection").isArray() &&
              request.value("selection").toArray().size() <= 256,
          "InvalidInput", "Bounded inference selection required");
    auto toleranceValue = request.value("tolerance");
    check(toleranceValue.isDouble() || toleranceValue.isInteger(), "InvalidInput",
          "Numeric inference tolerance required");
    double tolerance = toleranceValue.toDouble();
    check(std::isfinite(tolerance) && tolerance > 0 && tolerance <= .01, "InvalidInput",
          "Inference tolerance must be positive and at most 10 mm");
    auto all = parse(evaluate(input, previous).value("sketches").toArray());
    auto id = domain::id(wire::text(request, "sketch"));
    auto found = std::find_if(all.begin(), all.end(), [&](auto &s) { return s.id == id; });
    check(found != all.end(), "ReferenceMissing", "Inference sketch missing");
    QStringList selected;
    for (auto value : request.value("selection").toArray()) {
        check(value.isString(), "InvalidInput", "Inference entity identity required");
        selected << domain::id(value.toString());
        found->entity(selected.back());
    }
    if (selected.empty())
        for (auto &e : found->entities)
            selected << e.id;
    selected.removeDuplicates();
    std::sort(selected.begin(), selected.end());
    auto candidate = *found;
    int freedomBefore = solveLocal(candidate, nullptr, true).freedom;
    QElapsedTimer budget;
    budget.start();
    auto key = [&](Constraint c) {
        bool symmetric =
            c.kind == "Perpendicular" ||
            ((c.kind == "Horizontal" || c.kind == "Vertical" || c.kind == "Coincident") &&
             !c.b.isEmpty() && found->entity(c.a).kind == "Point" &&
             found->entity(c.b).kind == "Point") ||
            (c.kind == "Tangent" && found->entity(c.a).kind != "Line");
        if (symmetric && c.b < c.a)
            std::swap(c.a, c.b);
        if (c.kind == "Intersection" && c.third < c.b)
            std::swap(c.b, c.third);
        return c.kind + ":" + c.a + ":" + c.b + ":" + c.third;
    };
    QSet<QString> existing;
    for (auto &c : candidate.constraints)
        existing.insert(key(c));
    int attempts = 0;
    auto propose = [&](QString kind, QString a, QString b = {}, QString third = {},
                       double value = 0, double second = 0, bool reversed = false) {
        check(budget.elapsed() <= 5000, "ResourceLimit",
              "Inference work budget exceeded; select fewer entities");
        Constraint relation;
        relation.kind = kind;
        relation.a = a;
        relation.b = b;
        relation.third = third;
        relation.value = value;
        relation.second = second;
        relation.reversed = reversed;
        auto identity = key(relation);
        if (existing.contains(identity) ||
            !constraintReason(candidate, kind, a, b, third).isEmpty())
            return;
        check(++attempts <= 512 && candidate.constraints.size() < 512, "ResourceLimit",
              "Inference proposal budget exceeded; select fewer entities");
        relation.id = QUuid::createUuidV5(QUuid(found->id), ("CM3:relation:" + identity).toUtf8())
                          .toString(QUuid::WithoutBraces);
        auto trial = candidate;
        trial.constraints.push_back(relation);
        auto result = solveLocal(trial, &candidate, true);
        if (!result.valid || result.freedom >= freedomBefore)
            return;
        bool close = true;
        for (auto &e : found->entities) {
            auto &after = result.sketch.entity(e.id);
            if (e.kind == "Point" && length(e.point, after.point) > tolerance)
                close = false;
            if (radiusCurve(e.kind) && std::abs(e.radius - after.radius) > tolerance)
                close = false;
        }
        if (!close)
            return;
        candidate = result.sketch;
        freedomBefore = result.freedom;
        existing.insert(identity);
    };
    std::vector<const Entity *> lines;
    for (auto &e : found->entities)
        if (e.kind == "Line")
            lines.push_back(&e);
    int contactQueries = 0;
    auto contactBudget = [&] {
        check(++contactQueries <= 512 && budget.elapsed() <= 5000, "ResourceLimit",
              "Exact inference contact budget exceeded; select fewer entities");
    };
    auto closest = [&](const Entity &curve, Vec2 point) -> std::optional<double> {
        contactBudget();
        try {
            auto exact = geometry::sketchCurve(*found, curve);
            auto world = found->frame.world(point);
            GeomAPI_ProjectPointOnCurve projection(
                gp_Pnt(world[0] * 1000, world[1] * 1000, world[2] * 1000), exact.curve, exact.first,
                exact.last);
            if (projection.NbPoints() == 0 || projection.LowerDistance() > tolerance * 1000)
                return {};
            check(projection.NbPoints() <= 128, "ResourceLimit",
                  "Inference contact projection is too complex");
            double parameter = projection.LowerDistanceParameter();
            for (int i = 1; i <= projection.NbPoints(); ++i)
                if (std::abs(projection.Distance(i) - projection.LowerDistance()) <= 1e-7 &&
                    std::abs(projection.Parameter(i) - parameter) >
                        1e-6 * (exact.last - exact.first)) {
                    bool seam = curve.kind == "Ellipse" &&
                                std::abs(std::abs(projection.Parameter(i) - parameter) -
                                         (exact.last - exact.first)) <= 1e-8;
                    if (!seam)
                        return {};
                }
            return parameter / exact.parameterScale;
        } catch (const Standard_Failure &) {
            throw domain::Error("SolveFailure", "Exact inference contact projection failed");
        }
    };
    auto tangent = [&](const Entity &line, const Entity &circle) {
        auto a = found->entity(line.a).point, b = found->entity(line.b).point,
             center = found->entity(circle.a).point;
        Vec2 v{b.x - a.x, b.y - a.y};
        double norm = length(a, b);
        double t = ((center.x - a.x) * v.x + (center.y - a.y) * v.y) / (norm * norm);
        Vec2 foot{a.x + t * v.x, a.y + t * v.y};
        if (std::abs(length(center, foot) - circle.radius) > tolerance)
            return;
        if (circle.kind == "Arc") {
            double delta = std::fmod(
                std::atan2(foot.y - center.y, foot.x - center.x) - circle.start + 2 * tau, tau);
            if (delta > circle.sweep + 1e-8)
                return;
        }
        propose("Tangent", line.id, circle.id);
    };
    auto curveTangent = [&](const Entity &line, const Entity &curve) {
        auto a = found->entity(line.a).point, b = found->entity(line.b).point;
        Vec2 v{b.x - a.x, b.y - a.y};
        double size = length(a, b);
        struct Contact {
            double line, curve, score;
        };
        std::vector<Contact> contacts;
        for (double fraction : {0., .5, 1.}) {
            Vec2 query{a.x + fraction * v.x, a.y + fraction * v.y};
            auto parameter = closest(curve, query);
            if (!parameter)
                continue;
            auto point = curveValue(*found, curve, *parameter),
                 direction = curveDerivative(*found, curve, *parameter);
            double speed = std::hypot(direction.x, direction.y);
            double angle = speed < 1e-12
                               ? 1
                               : std::abs(v.x * direction.y - v.y * direction.x) / (size * speed);
            double angularTolerance = std::min(.05, tolerance / std::max(size, 2 * tolerance));
            if (speed < 1e-12 || angle > angularTolerance)
                continue;
            double along = ((point.x - a.x) * v.x + (point.y - a.y) * v.y) / (size * size);
            double distance = length(point, {a.x + along * v.x, a.y + along * v.y});
            if (along < -tolerance / size || along > 1 + tolerance / size || distance > tolerance)
                continue;
            auto range = curveRange(curve);
            if (std::none_of(contacts.begin(), contacts.end(), [&](auto old) {
                    return std::abs(old.curve - *parameter) < 1e-6 * (range.second - range.first);
                }))
                contacts.push_back({std::clamp(along, 1e-6, 1 - 1e-6), *parameter,
                                    distance / tolerance + angle / angularTolerance});
        }
        // Multiple contacts require explicit branch selection, not an arbitrary
        // automatic tangent choice.
        if (contacts.empty())
            return;
        std::sort(contacts.begin(), contacts.end(),
                  [](auto a, auto b) { return a.score < b.score; });
        if (contacts.size() > 1 && contacts[1].score - contacts[0].score <= 1e-4)
            return;
        auto direction = curveDerivative(*found, curve, contacts[0].curve);
        propose("CurveTangent", line.id, curve.id, {}, contacts[0].line, contacts[0].curve,
                v.x * direction.x + v.y * direction.y < 0);
    };
    auto curvePair = [&](const Entity &left, const Entity &right) {
        if (left.id == right.id)
            return;
        auto *a = &left, *b = &right;
        if (b->id < a->id)
            std::swap(a, b);
        contactBudget();
        try {
            auto first = geometry::sketchCurve(*found, *a),
                 second = geometry::sketchCurve(*found, *b);
            GeomAPI_ExtremaCurveCurve extrema(first.curve, second.curve, first.first, first.last,
                                              second.first, second.last);
            if (extrema.NbExtrema() == 0 || extrema.IsParallel())
                return;
            check(extrema.NbExtrema() <= 128, "ResourceLimit", "Too many competing curve contacts");
            struct Contact {
                double u, v, distance;
            };
            std::vector<Contact> contacts;
            for (int i = 1; i <= extrema.NbExtrema(); ++i) {
                double u, v;
                extrema.Parameters(i, u, v);
                contacts.push_back(
                    {u / first.parameterScale, v / second.parameterScale, extrema.Distance(i)});
            }
            std::sort(contacts.begin(), contacts.end(),
                      [](auto &x, auto &y) { return x.distance < y.distance; });
            auto best = contacts.front();
            if (best.distance > tolerance * 1000)
                return;
            auto equivalent = [&](const Entity &e, double x, double y) {
                double difference = std::abs(x - y);
                return difference <= 1e-8 || ((e.kind == "Circle" || e.kind == "Ellipse") &&
                                              std::abs(difference - tau) <= 1e-8);
            };
            for (size_t i = 1; i < contacts.size(); ++i)
                if (std::abs(contacts[i].distance - best.distance) <= 1e-7 &&
                    (!equivalent(*a, contacts[i].u, best.u) ||
                     !equivalent(*b, contacts[i].v, best.v)))
                    return;
            auto u = curveDerivative(*found, *a, best.u), v = curveDerivative(*found, *b, best.v);
            double product = length(u, {}) * length(v, {});
            if (product <= 1e-20)
                return;
            double angularTolerance =
                std::min(.05, tolerance / std::max(1e-6, std::min(length(u, {}), length(v, {}))));
            if (std::abs(u.x * v.y - u.y * v.x) / product > angularTolerance)
                return;
            propose("CurveTangent", a->id, b->id, {}, best.u, best.v, u.x * v.x + u.y * v.y < 0);
        } catch (const Standard_Failure &) {
            throw domain::Error("SolveFailure", "Exact curve-to-curve inference failed");
        }
    };
    for (auto &entity : selected) {
        check(budget.elapsed() <= 5000, "ResourceLimit",
              "Inference work budget exceeded; select fewer entities");
        auto &e = found->entity(entity);
        if (e.kind == "Point") {
            for (size_t i = 0; i < lines.size(); ++i)
                for (size_t j = i + 1; j < lines.size(); ++j) {
                    auto a = found->entity(lines[i]->a).point, b = found->entity(lines[i]->b).point,
                         c = found->entity(lines[j]->a).point, d = found->entity(lines[j]->b).point;
                    Vec2 v{b.x - a.x, b.y - a.y}, w{d.x - c.x, d.y - c.y};
                    double det = v.x * w.y - v.y * w.x;
                    if (std::abs(det) < 1e-10 * length(a, b) * length(c, d))
                        continue;
                    double t = ((c.x - a.x) * w.y - (c.y - a.y) * w.x) / det;
                    if (length(e.point, {a.x + t * v.x, a.y + t * v.y}) <= tolerance)
                        propose("Intersection", e.id, lines[i]->id, lines[j]->id);
                }
            for (auto *line : lines) {
                auto a = found->entity(line->a).point, b = found->entity(line->b).point;
                if (length(e.point, {(a.x + b.x) / 2, (a.y + b.y) / 2}) <= tolerance)
                    propose("Midpoint", e.id, line->id);
            }
            for (auto &other : found->entities) {
                if (other.id == e.id)
                    continue;
                if (other.kind == "Point") {
                    if (length(e.point, other.point) <= tolerance)
                        propose("Coincident", e.id, other.id);
                    else if (std::abs(e.point.y - other.point.y) <= tolerance)
                        propose("Horizontal", e.id, other.id);
                    else if (std::abs(e.point.x - other.point.x) <= tolerance)
                        propose("Vertical", e.id, other.id);
                } else if (other.kind == "Line") {
                    auto a = found->entity(other.a).point, b = found->entity(other.b).point;
                    double size = length(a, b),
                           t = ((e.point.x - a.x) * (b.x - a.x) + (e.point.y - a.y) * (b.y - a.y)) /
                               (size * size);
                    if (t >= 0 && t <= 1 &&
                        length(e.point, {a.x + t * (b.x - a.x), a.y + t * (b.y - a.y)}) <=
                            tolerance)
                        propose("Coincident", e.id, other.id);
                } else if (advancedCurve(other.kind)) {
                    if (auto parameter = closest(other, e.point))
                        propose("Coincident", e.id, other.id, {}, 0,
                                other.kind == "Spline" ? *parameter : 0);
                } else if (other.kind == "Circle" || other.kind == "Arc") {
                    auto center = found->entity(other.a).point;
                    double delta =
                        std::fmod(std::atan2(e.point.y - center.y, e.point.x - center.x) -
                                      other.start + 2 * tau,
                                  tau);
                    if (std::abs(length(e.point, center) - other.radius) <= tolerance &&
                        (other.kind == "Circle" || delta <= other.sweep + 1e-8))
                        propose("Coincident", e.id, other.id);
                }
            }
        } else if (e.kind == "Line") {
            auto a = found->entity(e.a).point, b = found->entity(e.b).point;
            if (std::abs(b.y - a.y) <= tolerance && std::abs(b.x - a.x) > 2 * tolerance)
                propose("Horizontal", e.id);
            if (std::abs(b.x - a.x) <= tolerance && std::abs(b.y - a.y) > 2 * tolerance)
                propose("Vertical", e.id);
            for (auto &other : found->entities) {
                if (other.id == e.id)
                    continue;
                if (other.kind == "Line") {
                    auto c = found->entity(other.a).point, d = found->entity(other.b).point;
                    double size = length(a, b), span = length(c, d);
                    double cosine =
                        std::abs((b.x - a.x) * (d.x - c.x) + (b.y - a.y) * (d.y - c.y)) /
                        (size * span);
                    if (cosine <=
                        std::min(.05, tolerance / std::max(2 * tolerance, std::min(size, span))))
                        propose("Perpendicular", e.id, other.id);
                } else if (other.kind == "Circle" || other.kind == "Arc")
                    tangent(e, other);
                else if (advancedCurve(other.kind))
                    curveTangent(e, other);
            }
        } else if (advancedCurve(e.kind)) {
            for (auto *line : lines)
                curveTangent(*line, e);
            for (auto &other : found->entities)
                if (other.kind != "Point" && other.kind != "Line")
                    curvePair(e, other);
        } else if (e.kind == "Circle" || e.kind == "Arc") {
            for (auto &other : found->entities) {
                if (other.id == e.id)
                    continue;
                if (other.kind == "Line")
                    tangent(other, e);
                else if (advancedCurve(other.kind))
                    curvePair(e, other);
                else if (other.kind == "Circle" || other.kind == "Arc") {
                    double distance =
                        length(found->entity(e.a).point, found->entity(other.a).point);
                    if (distance <= 1e-9)
                        continue;
                    if (std::abs(distance - e.radius - other.radius) <= tolerance)
                        propose("Tangent", e.id, other.id);
                    else if (std::abs(distance - std::abs(e.radius - other.radius)) <= tolerance)
                        propose("Tangent", e.id, other.id, {}, 1);
                }
            }
        }
    }
    *found = candidate;
    return evaluate(encode(all), previous, false);
}
static QCborMap fullyDefine(const QCborArray &input, const QCborMap &request,
                            const QCborArray &previous) {
    wire::keys(request, {"kind", "sketch", "scope", "selection"});
    auto scope = wire::text(request, "scope");
    check(scope == "All" || scope == "Selected", "UnsupportedOperation",
          "Unknown Fully Define scope");
    check(request.value("selection").isArray() &&
              request.value("selection").toArray().size() <= 256,
          "InvalidInput", "Bounded Fully Define selection required");
    auto prepared = evaluate(input, previous);
    auto all = parse(prepared.value("sketches").toArray());
    auto id = domain::id(wire::text(request, "sketch"));
    auto found = std::find_if(all.begin(), all.end(), [&](auto &s) { return s.id == id; });
    check(found != all.end(), "ReferenceMissing", "Fully Define sketch missing");
    auto original = *found;
    QSet<QString> selected;
    for (auto value : request.value("selection").toArray()) {
        check(value.isString(), "InvalidInput", "Fully Define entity identity required");
        auto entity = domain::id(value.toString());
        original.entity(entity);
        selected.insert(entity);
    }
    check(scope == "All" || !selected.empty(), "InvalidInput", "Select geometry to fully define");
    if (scope == "All")
        for (auto &e : original.entities)
            selected.insert(e.id);
    QSet<QString> selectedPoints;
    for (auto &entity : selected)
        for (auto &point : controlPoints(original.entity(entity)))
            selectedPoints.insert(point);
    auto base = solveLocal(original, nullptr, true);
    if (base.freedom == 0)
        return prepared;
    auto candidate = original;
    auto identity = [&](QString key) {
        return QUuid::createUuidV5(QUuid(id), ("CM3:define:" + key).toUtf8())
            .toString(QUuid::WithoutBraces);
    };
    auto anchor = identity("origin");
    auto origin = std::find_if(candidate.entities.begin(), candidate.entities.end(),
                               [&](auto &e) { return e.id == anchor; });
    if (origin == candidate.entities.end()) {
        candidate.point({0, 0}, true);
        candidate.entities.back().id = anchor;
        Constraint fixed;
        fixed.id = identity("anchor");
        fixed.kind = "Fixed";
        fixed.a = anchor;
        candidate.constraints.push_back(fixed);
    } else
        check(origin->kind == "Point" && origin->construction &&
                  length(origin->point, {0, 0}) <= 1e-12,
              "ReferenceAmbiguous", "Fully Define origin identity is occupied");
    auto anchored = solveLocal(candidate, &original, true);
    check(anchored.valid, "ResourceLimit",
          "Fully Define requires capacity for a construction origin");
    int freedom = anchored.freedom;
    candidate = anchored.sketch;
    QElapsedTimer budget;
    budget.start();
    int attempts = 0;
    auto propose = [&](Constraint c) {
        if (freedom == 0)
            return;
        check(budget.elapsed() <= 5000 && ++attempts <= 512 && candidate.constraints.size() < 512,
              "ResourceLimit", "Fully Define work budget exceeded; select fewer entities");
        c.id = identity(c.kind + ":" + c.a + ":" + c.b);
        for (auto &existing : candidate.constraints)
            if (existing.id == c.id)
                return;
        c.value = measureDimension(candidate, c);
        c.expression =
            QString::number(c.value, 'g', 17) + (angularDimension(c.kind) ? " rad" : " m");
        auto trial = candidate;
        trial.constraints.push_back(c);
        auto result = solveLocal(trial, &candidate, true);
        if (!result.valid || result.freedom >= freedom)
            return;
        for (auto &e : original.entities) {
            auto &after = result.sketch.entity(e.id);
            if (e.kind == "Point" && length(e.point, after.point) > 1e-9)
                return;
            if (radiusCurve(e.kind) && std::abs(e.radius - after.radius) > 1e-9)
                return;
            if ((e.kind == "Arc" || e.kind == "EllipticalArc" || e.kind == "Parabola") &&
                (std::abs(e.start - after.start) > 1e-9 || std::abs(e.sweep - after.sweep) > 1e-9))
                return;
        }
        candidate = result.sketch;
        freedom = result.freedom;
    };
    QStringList points;
    for (auto &e : original.entities)
        if (e.kind == "Point" && selectedPoints.contains(e.id))
            points << e.id;
    std::sort(points.begin(), points.end());
    for (auto &point : points)
        for (QString kind : {"HorizontalDistance", "VerticalDistance"}) {
            Constraint c;
            c.kind = kind;
            c.a = anchor;
            c.b = point;
            propose(c);
        }
    QStringList curves(selected.begin(), selected.end());
    std::sort(curves.begin(), curves.end());
    for (auto &curve : curves)
        if (original.entity(curve).kind == "Circle" || original.entity(curve).kind == "Arc") {
            Constraint c;
            c.kind = "Radius";
            c.a = curve;
            propose(c);
            if (original.entity(curve).kind == "Arc")
                for (QString kind : {"StartAngle", "Sweep"}) {
                    c.kind = kind;
                    propose(c);
                }
        } else if (ellipseCurve(original.entity(curve).kind)) {
            Constraint c;
            c.kind = "MinorRadius";
            c.a = curve;
            propose(c);
            if (original.entity(curve).kind == "EllipticalArc")
                for (QString kind : {"StartAngle", "Sweep"}) {
                    c.kind = kind;
                    propose(c);
                }
        } else if (original.entity(curve).kind == "Parabola") {
            Constraint c;
            c.a = curve;
            for (QString kind : {"TrimStart", "TrimSpan"}) {
                c.kind = kind;
                propose(c);
            }
        }
    if (freedom == anchored.freedom)
        return prepared;
    *found = candidate;
    return evaluate(encode(all), previous, false);
}
QCborMap interact(const QCborArray &candidates, const QCborMap &request,
                  const QCborArray &previous) {
    if (wire::text(request, "kind") == "Infer")
        return infer(candidates, request, previous);
    if (wire::text(request, "kind") == "FullyDefine")
        return fullyDefine(candidates, request, previous);
    wire::keys(request, {"kind", "sketch", "entity", "from", "to"});
    check(wire::text(request, "kind") == "Drag", "UnsupportedOperation",
          "Unknown sketch interaction");
    auto point = [&](QString key) {
        auto value = request.value(key);
        check(value.isArray() && value.toArray().size() == 2, "InvalidInput",
              "Two drag coordinates required");
        auto pair = value.toArray();
        for (auto coordinate : pair)
            check((coordinate.isDouble() || coordinate.isInteger()) &&
                      std::isfinite(coordinate.toDouble()) &&
                      std::abs(coordinate.toDouble()) <= 1e3,
                  "InvalidInput", "Finite drag coordinates within 1 km required");
        return Vec2{pair[0].toDouble(), pair[1].toDouble()};
    };
    auto from = point("from"), to = point("to");
    auto all = parse(evaluate(candidates, previous).value("sketches").toArray());
    auto id = domain::id(wire::text(request, "sketch"));
    auto selected = std::find_if(all.begin(), all.end(), [&](auto &s) { return s.id == id; });
    check(selected != all.end(), "ReferenceMissing", "Drag sketch missing");
    auto &entity = selected->entity(domain::id(wire::text(request, "entity")));
    DragTargets targets;
    Vec2 delta{to.x - from.x, to.y - from.y};
    auto move = [&](QString point) {
        auto p = selected->entity(point).point;
        targets.points.push_back({point, {p.x + delta.x, p.y + delta.y}});
    };
    if (entity.kind == "Point")
        move(entity.id);
    else if (auto group = constructionOf(*selected, entity.id)) {
        auto members = constructionMembers(*group);
        for (auto &id : members) {
            auto &member = selected->entity(id);
            if (member.kind == "Point")
                move(id);
            else if (radiusCurve(member.kind))
                targets.radii.push_back({id, member.radius});
        }
    } else if (entity.kind == "Circle" || entity.kind == "Arc") {
        auto center = selected->entity(entity.a).point;
        auto distance = length(center, from);
        check(distance > 1e-9, "ReferenceAmbiguous", "Choose the circle or arc perimeter");
        targets.points.push_back({entity.a, center});
        targets.circle = entity.id;
        targets.cursor = {center.x + entity.radius * (from.x - center.x) / distance + delta.x,
                          center.y + entity.radius * (from.y - center.y) / distance + delta.y};
    } else {
        for (auto &point : controlPoints(entity))
            move(point);
        if (radiusCurve(entity.kind))
            targets.radii.push_back({entity.id, entity.radius});
    }
    auto before = *selected;
    auto result = solveLocal(before, &before, true, &targets);
    if (!result.valid)
        return fields(
            {{"sketches", encode(all)}, {"diagnostics", QCborArray{result.diagnostic()}}});
    *selected = result.sketch;
    return evaluate(encode(all), previous, false);
}
QCborArray validateAccepted(const QCborArray &input) {
    auto result = evaluate(input);
    auto stored = parse(input), solved = parse(result.value("sketches").toArray());
    for (size_t i = 0; i < stored.size(); ++i) {
        auto &s = stored[i], &other = solved[i];
        double scale = 1e-6;
        Vec2 origin;
        for (auto &e : s.entities)
            if (e.kind == "Point") {
                origin = e.point;
                break;
            }
        for (auto &e : s.entities) {
            if (e.kind == "Point")
                scale = std::max(scale, length(e.point, origin));
            else if (radiusCurve(e.kind))
                scale = std::max(scale, e.radius);
        }
        double tolerance = std::max(1e-11, scale * 1e-8);
        for (auto &e : s.entities) {
            auto &q = other.entity(e.id);
            if (e.kind == "Point")
                check(length(e.point, q.point) <= tolerance, "InvalidProject",
                      "Stored sketch does not satisfy constraints or references");
            if (radiusCurve(e.kind))
                check(std::abs(e.radius - q.radius) <= tolerance, "InvalidProject",
                      "Stored radius does not satisfy constraints");
            if (e.kind == "Arc" || e.kind == "EllipticalArc" || e.kind == "Parabola") {
                double angularTolerance = e.kind == "Parabola" ? tolerance : 1e-8;
                check(std::abs(e.start - q.start) <= angularTolerance &&
                          std::abs(e.sweep - q.sweep) <= angularTolerance,
                      "InvalidProject", "Stored curve trim does not satisfy constraints");
            }
        }
        for (size_t j = 0; j < s.constraints.size(); ++j) {
            auto &a = s.constraints[j], &b = other.constraints[j];
            check(std::abs(a.value - b.value) <= 1e-10 && std::abs(a.second - b.second) <= 1e-10,
                  "InvalidProject",
                  "Stored dimension/contact/reference target differs from regeneration");
            if (a.reference)
                check(std::abs(a.reference->parameter - b.reference->parameter) <= 1e-10,
                      "InvalidProject", "Stored Pierce parameter differs from exact regeneration");
        }
    }
    return result.value("diagnostics").toArray();
}
} // namespace cad::sketch
