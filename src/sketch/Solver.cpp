#include "Solver.hpp"
#include "domain/Expressions.hpp"
#include <Eigen/QR>
#include <GCS.h>
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
    auto reason = constraintReason(s, c.kind, c.a, c.b);
    check(reason.isEmpty(), "UnsupportedOperation", reason);
    if (c.kind == "Length" || c.kind == "Distance" || c.kind == "Radius")
        check(c.value > 1e-9 && c.value <= 1e3, "InvalidGeometry",
              "Dimension must exceed 1 nm and be at most 1 km");
    if (c.kind == "Angle")
        check(std::abs(c.value) <= tau, "InvalidInput", "Angle must be within ±360 degrees");
}
std::vector<double> residuals(const Sketch &s) {
    std::vector<double> out;
    for (auto &c : s.constraints) {
        auto &a = s.entity(c.a);
        auto p = a.kind == "Point" ? a.point : s.entity(a.a).point;
        auto push = [&](double n) { out.push_back(n); };
        if (c.kind == "Fixed") {
            push(p.x - c.value);
            push(p.y - c.second);
        } else if (c.kind == "Coincident") {
            auto q = s.entity(c.b).point;
            push(p.x - q.x);
            push(p.y - q.y);
        } else if (c.kind == "Distance")
            push(length(p, s.entity(c.b).point) - c.value);
        else if (c.kind == "Radius")
            push(a.radius - c.value);
        else if (c.kind == "Equal" && a.kind == "Circle")
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
                push(std::abs((center.x - p.x) * v.y - (center.y - p.y) * v.x) / l - circle.radius);
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
        } else if (e.kind != "Line") {
            out.push_back(&e.radius);
            if (e.kind == "Arc") {
                out.push_back(&e.start);
                out.push_back(&e.sweep);
            }
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
        *v = saved + h;
        auto plus = residuals(s);
        *v = saved - h;
        auto minus = residuals(s);
        *v = saved;
        for (size_t row = 0; row < residual.size(); ++row)
            jac(row, col) = (plus[row] - minus[row]) / (2 * h);
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
                   {"redundant", QCborArray::fromStringList(redundant)}});
}
Result solve(Sketch candidate, const Sketch *previous) {
    Result result;
    result.sketch = candidate;
    try {
        candidate.validate();
        expression::Environment names(candidate.names);
        names.validateAll();
        for (auto &c : candidate.constraints) {
            bool dimensional =
                QStringList{"Length", "Distance", "Angle", "Radius"}.contains(c.kind);
            check(dimensional || c.expression.isEmpty(), "InvalidInput",
                  "Expressions apply only to dimensional constraints");
            if (!c.expression.isEmpty()) {
                auto q = names.evaluate(c.expression);
                check(q.dimension == (c.kind == "Angle" ? expression::angle : expression::length),
                      "UnitMismatch", "Dimension expression has the wrong physical dimension");
                c.value = q.value;
            }
            combinations(candidate, c);
        }
        result.sketch = candidate;
        if (candidate.constraints.empty()) {
            auto count = variables(candidate).size();
            check(count <= 256, "ResourceLimit", "At most 256 numerical sketch variables");
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
            else if (e.kind != "Line")
                scale = std::max(scale, e.radius);
        }
        result.lengthScale = scale;
        for (auto &e : candidate.entities) {
            if (e.kind == "Point") {
                e.point.x = (e.point.x - origin.x) / scale;
                e.point.y = (e.point.y - origin.y) / scale;
            } else if (e.kind != "Line")
                e.radius /= scale;
        }
        for (auto &c : candidate.constraints) {
            if (c.kind == "Fixed") {
                c.value = (c.value - origin.x) / scale;
                c.second = (c.second - origin.y) / scale;
            } else if (c.kind == "Length" || c.kind == "Distance" || c.kind == "Radius")
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
        for (auto &e : candidate.entities)
            if (e.kind == "Point")
                points.insert(e.id, GCS::Point{&e.point.x, &e.point.y});
        for (auto &e : candidate.entities) {
            if (e.kind == "Line") {
                GCS::Line l;
                l.p1 = points[e.a];
                l.p2 = points[e.b];
                lines.insert(e.id, l);
            } else if (e.kind == "Circle" || e.kind == "Arc") {
                GCS::Circle c;
                c.center = points[e.a];
                c.rad = &e.radius;
                circles.insert(e.id, c);
            }
        }
        GCS::System system;
        system.maxIter = 100;
        system.maxIterRedundant = 100;
        system.sketchSizeMultiplier = false;
        system.sketchSizeMultiplierRedundant = false;
        system.convergence = 1e-10;
        int tag = 0;
        for (auto &c : candidate.constraints) {
            ++tag;
            auto a = c.a, b = c.b;
            if (c.kind == "Fixed") {
                system.addConstraintCoordinateX(points[a], constant(c.value), tag);
                system.addConstraintCoordinateY(points[a], constant(c.second), tag);
            } else if (c.kind == "Coincident")
                system.addConstraintP2PCoincident(points[a], points[b], tag);
            else if (c.kind == "Horizontal")
                system.addConstraintHorizontal(lines[a], tag);
            else if (c.kind == "Vertical")
                system.addConstraintVertical(lines[a], tag);
            else if (c.kind == "Parallel")
                system.addConstraintParallel(lines[a], lines[b], tag);
            else if (c.kind == "Perpendicular")
                system.addConstraintPerpendicular(lines[a], lines[b], tag);
            else if (c.kind == "Angle")
                system.addConstraintL2LAngle(lines[a], lines[b], constant(c.value), tag);
            else if (c.kind == "Length")
                system.addConstraintP2PDistance(lines[a].p1, lines[a].p2, constant(c.value), tag);
            else if (c.kind == "Distance")
                system.addConstraintP2PDistance(points[a], points[b], constant(c.value), tag);
            else if (c.kind == "Radius")
                system.addConstraintCircleRadius(circles[a], constant(c.value), tag);
            else if (c.kind == "Tangent")
                system.addConstraintTangent(lines[a], circles[b], tag);
            else if (c.kind == "Equal") {
                if (candidate.entity(a).kind == "Line")
                    system.addConstraintEqualLength(lines[a], lines[b], tag);
                else
                    system.addConstraintEqualRadius(circles[a], circles[b], tag);
            }
        }
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
        check(candidate.constraints.empty() ||
                  system.solve(true, GCS::DogLeg, !redundants.empty()) == GCS::Success,
              "SolveFailure", "PlaneGCS failed to converge");
        if (!candidate.constraints.empty())
            system.applySolution();
        for (auto *v : vars)
            check(std::isfinite(*v), "SolveFailure", "Nonfinite solver output");
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
            } else if (e.kind != "Line")
                e.radius *= scale;
        }
        candidate.constraints = result.sketch.constraints;
        candidate.validate();
        if (previous && previous->constraints.size() == candidate.constraints.size()) {
            bool same = true;
            for (size_t i = 0; i < candidate.constraints.size(); ++i) {
                const auto &a = previous->constraints[i];
                const auto &b = candidate.constraints[i];
                if (a.id != b.id || a.value != b.value || a.second != b.second ||
                    a.expression != b.expression || a.kind != b.kind || a.a != b.a || a.b != b.b)
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
                    if (c.kind == "Tangent") {
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
QCborArray validateAccepted(const QCborArray &input) {
    QCborArray diagnostics;
    for (auto &s : parse(input)) {
        auto r = solve(s);
        check(r.valid, "InvalidProject", "Invalid accepted sketch: " + r.message);
        for (auto &e : s.entities) {
            auto &other = r.sketch.entity(e.id);
            double tolerance = std::max(1e-11, r.lengthScale * 1e-8);
            if (e.kind == "Point")
                check(length(e.point, other.point) <= tolerance, "InvalidProject",
                      "Stored sketch does not satisfy constraints");
            else if (e.kind != "Line")
                check(std::abs(e.radius - other.radius) <= tolerance, "InvalidProject",
                      "Stored radius does not satisfy constraints");
        }
        for (size_t i = 0; i < s.constraints.size(); ++i)
            check(std::abs(s.constraints[i].value - r.sketch.constraints[i].value) <= 1e-12,
                  "InvalidProject", "Stored dimension disagrees with expression");
        diagnostics.append(r.diagnostic());
    }
    return diagnostics;
}
QCborMap evaluate(const QCborArray &input, const QCborArray &previous, bool rejectInvalid) {
    auto all = parse(input), old = parse(previous);
    QCborArray diagnostics;
    for (auto &s : all) {
        auto it = std::find_if(old.begin(), old.end(), [&](auto &v) { return v.id == s.id; });
        auto r = solve(s, it == old.end() ? nullptr : &*it);
        diagnostics.append(r.diagnostic());
        if (rejectInvalid)
            check(r.valid, r.status == "Inconsistent" ? "ConstraintConflict" : "SolveFailure",
                  r.message.isEmpty() ? r.status : r.message);
        if (r.valid)
            s = r.sketch;
    }
    return fields({{"sketches", encode(all)}, {"diagnostics", diagnostics}});
}
} // namespace cad::sketch
