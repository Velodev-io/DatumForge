#include "LineEditing.hpp"
#include "ArcGeometry.hpp"
#include "Constructions.hpp"
#include "Curves.hpp"
#include "geometry/occt/SketchCurve.hpp"
#include <algorithm>
#include <cmath>

namespace cad::sketch {
namespace {
using domain::check;
constexpr double tau = 6.2831853071795864769;

Vec2 add(Vec2 a, Vec2 b) {
    return {a.x + b.x, a.y + b.y};
}
Vec2 sub(Vec2 a, Vec2 b) {
    return {a.x - b.x, a.y - b.y};
}
Vec2 mul(Vec2 a, double n) {
    return {a.x * n, a.y * n};
}
double dot(Vec2 a, Vec2 b) {
    return a.x * b.x + a.y * b.y;
}
double cross(Vec2 a, Vec2 b) {
    return a.x * b.y - a.y * b.x;
}
Vec2 unit(Vec2 a) {
    double l = std::hypot(a.x, a.y);
    check(l > 1e-14, "InvalidGeometry", "Degenerate vector direction");
    return {a.x / l, a.y / l};
}
Vec2 rotatePoint(Vec2 p, Vec2 center, double angle) {
    double c = std::cos(angle), s = std::sin(angle);
    Vec2 v = sub(p, center);
    return add(center, {v.x * c - v.y * s, v.x * s + v.y * c});
}
Vec2 reflectPoint(Vec2 p, Vec2 linePt, Vec2 lineDir) {
    Vec2 u = unit(lineDir);
    Vec2 v = sub(p, linePt);
    return add(linePt, sub(mul(u, 2.0 * dot(v, u)), v));
}
double number(const QCborMap &m, QString key, double fallback = 0.0, bool required = true) {
    if (!m.contains(key)) {
        check(!required, "InvalidInput", QString("Missing required parameter: %1").arg(key));
        return fallback;
    }
    auto v = m.value(key);
    check((v.isDouble() || v.isInteger()) && std::isfinite(v.toDouble()), "InvalidInput",
          "Finite numeric value required");
    return v.toDouble();
}

std::vector<double> lineIntersections(const Sketch &s, const Entity &line, const Entity &boundary,
                                      bool apparent) {
    auto a = s.entity(line.a).point, d = sub(s.entity(line.b).point, a);
    double size = std::hypot(d.x, d.y), eps = 1e-10 / size;
    std::vector<double> hits;
    if (boundary.kind == "Line") {
        auto p = s.entity(boundary.a).point, q = sub(s.entity(boundary.b).point, p);
        auto delta = sub(p, a);
        double den = cross(d, q);
        if (std::abs(den) <= 1e-12 * size * std::hypot(q.x, q.y)) {
            check(std::abs(cross(delta, d)) > 1e-10 * size, "ReferenceAmbiguous",
                  "Overlapping line supports require explicit repair");
            return hits;
        }
        double t = cross(delta, q) / den, u = cross(delta, d) / den;
        double otherEps = 1e-10 / std::hypot(q.x, q.y);
        if (apparent || (u >= -otherEps && u <= 1 + otherEps))
            hits.push_back(t);
    } else if (boundary.kind == "Circle" || boundary.kind == "Arc") {
        auto center = s.entity(boundary.a).point, delta = sub(a, center);
        double t = -dot(delta, d) / dot(d, d);
        auto nearest = Vec2{a.x + t * d.x, a.y + t * d.y};
        double distance = length(nearest, center);
        if (distance > boundary.radius + 1e-10)
            return hits;
        double offset =
            std::sqrt(std::max(0., (boundary.radius - distance) * (boundary.radius + distance))) /
            size;
        for (double root : {t - offset, t + offset}) {
            auto p = Vec2{a.x + root * d.x, a.y + root * d.y};
            if (boundary.kind == "Arc") {
                double angle = std::fmod(
                    std::atan2(p.y - center.y, p.x - center.x) - boundary.start + 2 * tau, tau);
                if (angle > boundary.sweep + 1e-10 / boundary.radius)
                    continue;
            }
            if (hits.empty() || std::abs(root - hits.back()) > eps)
                hits.push_back(root);
        }
    } else if (advancedCurve(boundary.kind)) {
        for (auto p : geometry::sketchIntersections(s, line, boundary, true))
            hits.push_back(dot(sub(p, a), d) / dot(d, d));
    }
    return hits;
}

std::vector<Vec2> circleCircleIntersections(Vec2 c1, double r1, Vec2 c2, double r2) {
    std::vector<Vec2> hits;
    double d = length(c1, c2);
    if (d > r1 + r2 + 1e-10 || d < std::abs(r1 - r2) - 1e-10 || d < 1e-10)
        return hits;
    double a = (r1 * r1 - r2 * r2 + d * d) / (2.0 * d);
    double h2 = r1 * r1 - a * a;
    double h = std::sqrt(std::max(0.0, h2));
    Vec2 u = mul(sub(c2, c1), 1.0 / d);
    Vec2 p0 = add(c1, mul(u, a));
    Vec2 norm{-u.y, u.x};
    hits.push_back(add(p0, mul(norm, h)));
    if (h > 1e-10)
        hits.push_back(sub(p0, mul(norm, h)));
    return hits;
}

std::vector<Vec2> circleLineIntersections(Vec2 center, double radius, Vec2 a, Vec2 b,
                                          bool segmentOnly = true) {
    std::vector<Vec2> hits;
    Vec2 d = sub(b, a);
    double len = std::hypot(d.x, d.y);
    if (len < 1e-12)
        return hits;
    double t = -dot(sub(a, center), d) / dot(d, d);
    Vec2 nearest = add(a, mul(d, t));
    double dist = length(nearest, center);
    if (dist > radius + 1e-10)
        return hits;
    double offset = std::sqrt(std::max(0.0, (radius - dist) * (radius + dist))) / len;
    for (double root : {t - offset, t + offset}) {
        if (!segmentOnly || (root >= -1e-10 && root <= 1.0 + 1e-10))
            hits.push_back(add(a, mul(d, root)));
    }
    return hits;
}

std::pair<QCborArray, QCborArray> discloseConstraints(Sketch &candidate,
                                                      const QSet<QString> &affected,
                                                      QString invariantLineId = {}) {
    QCborArray disclosure, removed;
    std::erase_if(candidate.constraints, [&](const Constraint &c) {
        bool touches = affected.contains(c.a) || affected.contains(c.b) ||
                       affected.contains(c.third) ||
                       std::any_of(c.members.begin(), c.members.end(),
                                   [&](auto &member) { return affected.contains(member); });
        bool invariant = !invariantLineId.isEmpty() && c.a == invariantLineId && c.b.isEmpty() &&
                         c.third.isEmpty() && (c.kind == "Horizontal" || c.kind == "Vertical");
        if (!touches || invariant)
            return false;
        disclosure.append(fields({{"id", c.id},
                                  {"kind", c.kind},
                                  {"expression", c.expression},
                                  {"value", c.value},
                                  {"a", c.a},
                                  {"b", c.b},
                                  {"third", c.third}}));
        removed.append(c.id);
        return true;
    });
    return {disclosure, removed};
}

QCborMap previewStraightLine(const Sketch &source, const QCborMap &request) {
    wire::keys(request, {"mode", "source", "boundaries", "pick_x", "pick_y"});
    auto mode = wire::text(request, "mode"), id = wire::text(request, "source");
    auto e = source.entity(id);
    check(e.kind == "Line", "UnsupportedOperation", "Select a straight line to trim or extend");
    check(
        !constructionOf(source, id), "UnsupportedOperation",
        "Edit the rectangle/slot construction as a group; trimming its members is not implemented");
    auto a = source.entity(e.a).point, b = source.entity(e.b).point, d = sub(b, a);
    double size = length(a, b), eps = 1e-10 / size;
    Vec2 pick{number(request, "pick_x"), number(request, "pick_y")};
    check(std::abs(pick.x) <= 1e3 && std::abs(pick.y) <= 1e3, "InvalidInput",
          "Pick must be within 1 km");
    double at = dot(sub(pick, a), d) / dot(d, d);
    check(at >= -eps && at <= 1 + eps, "InvalidInput", "Pick a position on the selected line");
    auto boundaries = request.value("boundaries");
    check(boundaries.isArray() && boundaries.toArray().size() <= 256, "InvalidInput",
          "Bounded boundary IDs required");
    QStringList ids;
    for (auto v : boundaries.toArray()) {
        check(v.isString() && v.toString() != id && !ids.contains(v.toString()), "InvalidInput",
              "Distinct boundary IDs required");
        source.entity(v.toString());
        ids.append(v.toString());
    }
    bool inside = mode == "Trim away inside", outside = mode == "Trim away outside";
    bool extend = mode == "Dynamic extend" || mode == "Extend to apparent intersection";
    bool apparent = mode == "Extend to apparent intersection";
    check(!(inside || outside) || ids.size() == 2, "InvalidInput", "Select exactly two boundaries");
    check(!apparent || ids.size() == 1, "InvalidInput",
          "Select one line for its apparent intersection");
    if (ids.empty())
        for (auto &other : source.entities)
            if (other.id != id && other.kind != "Point")
                ids.append(other.id);
    std::vector<double> hits;
    for (auto boundary : ids) {
        auto roots = lineIntersections(source, e, source.entity(boundary), apparent);
        if (inside || outside) {
            std::erase_if(roots, [&](double t) { return t <= eps || t >= 1 - eps; });
            check(roots.size() == 1, "ReferenceAmbiguous",
                  "Each selected boundary must have one interior intersection");
        }
        hits.insert(hits.end(), roots.begin(), roots.end());
    }
    std::sort(hits.begin(), hits.end());
    hits.erase(std::unique(hits.begin(), hits.end(),
                           [&](double x, double y) { return std::abs(x - y) <= eps; }),
               hits.end());
    std::vector<std::pair<double, double>> intervals;
    if (extend) {
        check(std::abs(at - .5) > eps, "ReferenceAmbiguous",
              "Pick the half of the line whose endpoint should extend");
        if (at < .5) {
            auto it = std::find_if(hits.rbegin(), hits.rend(), [&](double t) { return t < -eps; });
            check(it != hits.rend(), "ReferenceMissing",
                  "No intersection beyond the picked start endpoint");
            intervals.push_back({*it, 1});
        } else {
            auto it = std::find_if(hits.begin(), hits.end(), [&](double t) { return t > 1 + eps; });
            check(it != hits.end(), "ReferenceMissing",
                  "No intersection beyond the picked end endpoint");
            intervals.push_back({0, *it});
        }
    } else if (inside || outside) {
        check(hits.size() == 2, "ReferenceAmbiguous",
              "Two distinct interior boundary intersections required");
        if (outside)
            intervals.push_back({hits[0], hits[1]});
        else
            intervals = {{0, hits[0]}, {hits[1], 1}};
    } else {
        std::erase_if(hits, [&](double t) { return t <= eps || t >= 1 - eps; });
        check(!hits.empty(), "ReferenceMissing", "No interior intersection to trim to");
        double low = 0, high = 1;
        for (double t : hits) {
            check(std::abs(t - at) > eps, "ReferenceAmbiguous",
                  "Pick inside an interval, away from its boundary");
            if (t < at)
                low = t;
            else {
                high = t;
                break;
            }
        }
        if (low > eps)
            intervals.push_back({0, low});
        if (high < 1 - eps)
            intervals.push_back({high, 1});
    }
    auto candidate = source;
    QSet<QString> affected{id};
    bool retainsStart = false, retainsEnd = false;
    for (auto [low, high] : intervals) {
        check((high - low) * size > 1e-9, "InvalidGeometry",
              "Trim would create a fragment below 1 nm");
        retainsStart = retainsStart || low == 0;
        retainsEnd = retainsEnd || high == 1;
    }
    if (!retainsStart)
        affected.insert(e.a);
    if (!retainsEnd)
        affected.insert(e.b);
    auto [disclosure, removed] =
        discloseConstraints(candidate, affected, intervals.size() == 1 ? id : QString{});
    std::erase_if(candidate.entities, [&](auto &value) { return value.id == id; });
    QCborArray descendants;
    for (auto [low, high] : intervals) {
        auto point = [&](double t) { return Vec2{a.x + t * d.x, a.y + t * d.y}; };
        auto first = low == 0 ? e.a : candidate.point(point(low), e.construction);
        auto last = high == 1 ? e.b : candidate.point(point(high), e.construction);
        auto child = e;
        child.id = intervals.size() == 1 ? id : newId();
        child.a = first;
        child.b = last;
        candidate.entities.push_back(child);
        descendants.append(child.id);
    }
    candidate.edits.append(fields({{"id", newId()},
                                   {"mode", mode},
                                   {"source", id},
                                   {"descendants", descendants},
                                   {"removed_constraints", removed}}));
    candidate.validate();
    return fields({{"candidate", candidate.map()}, {"affected_constraints", disclosure}});
}

QCborMap previewPowerTrim(const Sketch &source, const QCborMap &request) {
    wire::keys(request, {"mode", "source", "boundaries", "pick_x", "pick_y"});
    auto id = wire::text(request, "source");
    auto e = source.entity(id);
    check(
        !constructionOf(source, id), "UnsupportedOperation",
        "Edit the rectangle/slot construction as a group; trimming its members is not implemented");
    Vec2 pick{number(request, "pick_x"), number(request, "pick_y")};
    check(std::abs(pick.x) <= 1e3 && std::abs(pick.y) <= 1e3, "InvalidInput",
          "Pick must be within 1 km");
    QStringList boundaryIds;
    auto boundaries = request.value("boundaries");
    if (boundaries.isArray() && !boundaries.toArray().empty()) {
        for (auto b : boundaries.toArray()) {
            check(b.isString() && b.toString() != id, "InvalidInput", "Valid boundary ID required");
            source.entity(b.toString());
            boundaryIds.append(b.toString());
        }
    } else {
        for (auto &other : source.entities)
            if (other.id != id && other.kind != "Point")
                boundaryIds.append(other.id);
    }

    if (e.kind == "Line") {
        auto a = source.entity(e.a).point, b = source.entity(e.b).point, d = sub(b, a);
        double size = length(a, b), eps = 1e-10 / size;
        double at = dot(sub(pick, a), d) / dot(d, d);
        check(at >= -eps && at <= 1 + eps, "InvalidInput", "Pick a position on the selected line");
        std::vector<double> hits;
        for (auto bid : boundaryIds) {
            auto roots = lineIntersections(source, e, source.entity(bid), false);
            hits.insert(hits.end(), roots.begin(), roots.end());
        }
        std::sort(hits.begin(), hits.end());
        hits.erase(std::unique(hits.begin(), hits.end(),
                               [&](double x, double y) { return std::abs(x - y) <= eps; }),
                   hits.end());
        std::erase_if(hits, [&](double t) { return t <= eps || t >= 1 - eps; });
        check(!hits.empty(), "ReferenceMissing", "No interior intersection to trim to");
        double low = 0, high = 1;
        for (double t : hits) {
            if (t < at)
                low = t;
            else {
                high = t;
                break;
            }
        }
        std::vector<std::pair<double, double>> intervals;
        if (low > eps)
            intervals.push_back({0, low});
        if (high < 1 - eps)
            intervals.push_back({high, 1});

        auto candidate = source;
        QSet<QString> affected{id};
        if (low <= eps)
            affected.insert(e.a);
        if (high >= 1 - eps)
            affected.insert(e.b);
        auto [disclosure, removed] =
            discloseConstraints(candidate, affected, intervals.size() == 1 ? id : QString{});
        std::erase_if(candidate.entities, [&](auto &value) { return value.id == id; });
        QCborArray descendants;
        for (auto [l, h] : intervals) {
            check((h - l) * size > 1e-9, "InvalidGeometry",
                  "Trim would create a fragment below 1 nm");
            auto first = l == 0 ? e.a : candidate.point(add(a, mul(d, l)), e.construction);
            auto last = h == 1 ? e.b : candidate.point(add(a, mul(d, h)), e.construction);
            auto child = e;
            child.id = intervals.size() == 1 ? id : newId();
            child.a = first;
            child.b = last;
            candidate.entities.push_back(child);
            descendants.append(child.id);
        }
        candidate.edits.append(fields({{"id", newId()},
                                       {"mode", "Power trim"},
                                       {"source", id},
                                       {"descendants", descendants},
                                       {"removed_constraints", removed}}));
        candidate.validate();
        return fields({{"candidate", candidate.map()}, {"affected_constraints", disclosure}});
    } else if (e.kind == "Circle" || e.kind == "Arc") {
        auto center = source.entity(e.a).point;
        double radius = e.radius;
        std::vector<double> hitAngles;
        for (auto bid : boundaryIds) {
            auto bEnt = source.entity(bid);
            check(bEnt.kind != "Point", "InvalidInput", "Curve boundary required");
            for (auto pt : geometry::sketchIntersections(source, e, bEnt))
                hitAngles.push_back(
                    std::fmod(std::atan2(pt.y - center.y, pt.x - center.x) + 2 * tau, tau));
        }
        check(!hitAngles.empty(), "ReferenceMissing", "No intersection on circle/arc to trim");
        std::sort(hitAngles.begin(), hitAngles.end());
        hitAngles.erase(std::unique(hitAngles.begin(), hitAngles.end(),
                                    [](double x, double y) { return std::abs(x - y) <= 1e-8; }),
                        hitAngles.end());

        double pickAngle =
            std::fmod(std::atan2(pick.y - center.y, pick.x - center.x) + 2 * tau, tau);
        auto candidate = source;
        QSet<QString> affected{id};
        auto [disclosure, removed] = discloseConstraints(candidate, affected);
        std::erase_if(candidate.entities, [&](auto &value) { return value.id == id; });
        QCborArray descendants;

        if (e.kind == "Circle") {
            check(hitAngles.size() > 1, "ReferenceAmbiguous",
                  "Circle trim requires two distinct intersections");
            {
                int trimIdx = -1;
                int n = int(hitAngles.size());
                for (int i = 0; i < n; ++i) {
                    double a0 = hitAngles[i];
                    double a1 = (i + 1 < n) ? hitAngles[i + 1] : (hitAngles[0] + tau);
                    double pAng = (pickAngle < a0 && i == n - 1) ? (pickAngle + tau) : pickAngle;
                    if (pAng >= a0 - 1e-8 && pAng <= a1 + 1e-8) {
                        trimIdx = i;
                        break;
                    }
                }
                if (trimIdx < 0)
                    trimIdx = 0;
                if (n == 2) {
                    double a0 = (trimIdx == 0) ? hitAngles[1] : hitAngles[0];
                    double a1 = (trimIdx == 0) ? (hitAngles[0] + tau) : hitAngles[1];
                    double sw = a1 - a0;
                    auto arcId =
                        candidate.arc(e.a, radius, std::remainder(a0, tau), sw, e.construction);
                    descendants.append(arcId);
                } else {
                    for (int i = 0; i < n; ++i) {
                        if (i == trimIdx)
                            continue;
                        double a0 = hitAngles[i];
                        double a1 = (i + 1 < n) ? hitAngles[i + 1] : (hitAngles[0] + tau);
                        double sw = a1 - a0;
                        auto arcId =
                            candidate.arc(e.a, radius, std::remainder(a0, tau), sw, e.construction);
                        descendants.append(arcId);
                    }
                }
            }
        } else {
            std::vector<double> interiorAngles;
            for (double h : hitAngles) {
                double rel = std::fmod(h - e.start + 2 * tau, tau);
                if (rel > 1e-8 && rel < e.sweep - 1e-8)
                    interiorAngles.push_back(e.start + rel);
            }
            check(!interiorAngles.empty(), "ReferenceMissing",
                  "No interior intersection on arc to trim");
            std::sort(interiorAngles.begin(), interiorAngles.end());
            std::vector<double> arcBounds{e.start};
            arcBounds.insert(arcBounds.end(), interiorAngles.begin(), interiorAngles.end());
            arcBounds.push_back(e.start + e.sweep);
            double relPick = std::fmod(pickAngle - e.start + 2 * tau, tau);
            double pAng = e.start + relPick;
            int trimIdx = 0;
            for (size_t i = 0; i + 1 < arcBounds.size(); ++i) {
                if (pAng >= arcBounds[i] - 1e-8 && pAng <= arcBounds[i + 1] + 1e-8) {
                    trimIdx = int(i);
                    break;
                }
            }
            for (size_t i = 0; i + 1 < arcBounds.size(); ++i) {
                if (int(i) == trimIdx)
                    continue;
                double a0 = arcBounds[i], a1 = arcBounds[i + 1];
                double sw = a1 - a0;
                if (sw > 1e-8) {
                    auto arcId =
                        candidate.arc(e.a, radius, std::remainder(a0, tau), sw, e.construction);
                    descendants.append(arcId);
                }
            }
        }
        candidate.edits.append(fields({{"id", newId()},
                                       {"mode", "Power trim"},
                                       {"source", id},
                                       {"descendants", descendants},
                                       {"removed_constraints", removed}}));
        candidate.validate();
        return fields({{"candidate", candidate.map()}, {"affected_constraints", disclosure}});
    }
    check(false, "UnsupportedOperation", "Power trim supports lines, circles, and arcs");
    return {};
}

QCborMap previewCorner(const Sketch &source, const QCborMap &request) {
    QSet<QString> allowed{"mode", "source", "boundaries", "pick_x", "pick_y"};
    if (request.contains(QStringLiteral("pick2_x")))
        allowed.insert("pick2_x");
    if (request.contains(QStringLiteral("pick2_y")))
        allowed.insert("pick2_y");
    wire::keys(request, allowed);
    auto id1 = wire::text(request, "source");
    auto boundaries = request.value("boundaries").toArray();
    check(boundaries.size() == 1, "InvalidInput", "Select exactly one second entity for Corner");
    auto id2 = boundaries[0].toString();
    check(id1 != id2, "InvalidInput", "Corner requires two distinct entities");
    auto e1 = source.entity(id1), e2 = source.entity(id2);
    check((e1.kind == "Line" || e1.kind == "Arc") && (e2.kind == "Line" || e2.kind == "Arc"),
          "UnsupportedOperation", "Corner trim supports lines and arcs");
    check(!constructionOf(source, id1) && !constructionOf(source, id2), "UnsupportedOperation",
          "Construction groups must be edited as a group");

    Vec2 pick1{number(request, "pick_x"), number(request, "pick_y")};
    Vec2 pick2{number(request, "pick2_x", 0.0, false), number(request, "pick2_y", 0.0, false)};
    if (!request.contains(QStringLiteral("pick2_x"))) {
        if (e2.kind == "Line")
            pick2 = add(source.entity(e2.a).point,
                        mul(sub(source.entity(e2.b).point, source.entity(e2.a).point), 0.5));
        else
            pick2 =
                add(source.entity(e2.a).point, {e2.radius * std::cos(e2.start + e2.sweep * 0.5),
                                                e2.radius * std::sin(e2.start + e2.sweep * 0.5)});
    }

    Vec2 cornerPt;
    if (e1.kind == "Line" && e2.kind == "Line") {
        auto a1 = source.entity(e1.a).point, b1 = source.entity(e1.b).point, d1 = sub(b1, a1);
        auto a2 = source.entity(e2.a).point, b2 = source.entity(e2.b).point, d2 = sub(b2, a2);
        double den = cross(d1, d2);
        check(std::abs(den) > 1e-12 * length(a1, b1) * length(a2, b2), "InvalidGeometry",
              "Parallel lines do not form a corner");
        double t1 = cross(sub(a2, a1), d2) / den;
        cornerPt = add(a1, mul(d1, t1));
    } else {
        std::vector<Vec2> hits;
        if (e1.kind == "Line" && e2.kind == "Arc") {
            hits = circleLineIntersections(source.entity(e2.a).point, e2.radius,
                                           source.entity(e1.a).point, source.entity(e1.b).point,
                                           false);
        } else if (e1.kind == "Arc" && e2.kind == "Line") {
            hits = circleLineIntersections(source.entity(e1.a).point, e1.radius,
                                           source.entity(e2.a).point, source.entity(e2.b).point,
                                           false);
        } else if (e1.kind == "Arc" && e2.kind == "Arc") {
            hits = circleCircleIntersections(source.entity(e1.a).point, e1.radius,
                                             source.entity(e2.a).point, e2.radius);
        }
        check(!hits.empty(), "InvalidGeometry", "Curves do not intersect to form a corner");
        cornerPt = hits[0];
        if (hits.size() > 1 && length(hits[1], pick1) < length(hits[0], pick1))
            cornerPt = hits[1];
    }

    auto candidate = source;
    QSet<QString> affected{id1, id2};
    auto sharedPt = candidate.point(cornerPt);

    auto updateOperand = [&](Entity &e, Vec2 pick) {
        if (e.kind == "Line") {
            auto a = source.entity(e.a).point, b = source.entity(e.b).point, d = sub(b, a);
            double at = dot(sub(pick, a), d) / dot(d, d);
            double t = dot(sub(cornerPt, a), d) / dot(d, d);
            if (at < t) {
                affected.insert(e.b);
                e.b = sharedPt;
            } else {
                affected.insert(e.a);
                e.a = sharedPt;
            }
        } else if (e.kind == "Arc") {
            auto C = source.entity(e.a).point;
            double cAng = std::fmod(std::atan2(cornerPt.y - C.y, cornerPt.x - C.x) + 2 * tau, tau);
            double a0 = std::fmod(e.start + 2 * tau, tau);
            double a1 = std::fmod(e.start + e.sweep + 2 * tau, tau);
            Vec2 p0 = add(C, {e.radius * std::cos(a0), e.radius * std::sin(a0)});
            Vec2 p1 = add(C, {e.radius * std::cos(a1), e.radius * std::sin(a1)});
            if (length(p0, pick) < length(p1, pick)) {
                double sw = std::fmod(cAng - a0 + 2 * tau, tau);
                if (sw < 1e-4)
                    sw = tau - 1e-4;
                e.start = std::remainder(a0, tau);
                e.sweep = sw;
            } else {
                double sw = std::fmod(a1 - cAng + 2 * tau, tau);
                if (sw < 1e-4)
                    sw = tau - 1e-4;
                e.start = std::remainder(cAng, tau);
                e.sweep = sw;
            }
        }
    };

    for (auto &e : candidate.entities) {
        if (e.id == id1)
            updateOperand(e, pick1);
        if (e.id == id2)
            updateOperand(e, pick2);
    }
    auto [disclosure, removed] = discloseConstraints(candidate, affected);
    QCborArray descendants{id1, id2};
    candidate.edits.append(fields({{"id", newId()},
                                   {"mode", "Corner"},
                                   {"source", id1},
                                   {"descendants", descendants},
                                   {"removed_constraints", removed}}));
    candidate.validate();
    return fields({{"candidate", candidate.map()}, {"affected_constraints", disclosure}});
}

QCborMap previewFillet(const Sketch &source, const QCborMap &request) {
    QSet<QString> allowed{"mode", "source", "radius"};
    if (request.contains(QStringLiteral("boundaries")))
        allowed.insert("boundaries");
    if (request.contains(QStringLiteral("shared_radius")))
        allowed.insert("shared_radius");
    wire::keys(request, allowed);
    auto id1 = wire::text(request, "source");
    QString id2;
    auto ent1 = source.entity(id1);
    auto candidate = source;
    QString cornerVertex;
    if (ent1.kind == "Point") {
        cornerVertex = id1;
        auto pt = source.entity(id1).point;
        QStringList connected;
        for (auto &e : source.entities) {
            if (e.kind == "Line" && (e.a == id1 || e.b == id1))
                connected.append(e.id);
            else if (e.kind == "Arc" &&
                     (length(arcPoint(source.entity(e.a).point, e.radius, e.start), pt) < 1e-9 ||
                      length(arcPoint(source.entity(e.a).point, e.radius, e.start + e.sweep), pt) <
                          1e-9))
                connected.append(e.id);
            else if (advancedCurve(e.kind) && e.kind != "Ellipse") {
                auto [a, b] = curveRange(e);
                if (length(curveValue(source, e, a), pt) < 1e-9 ||
                    length(curveValue(source, e, b), pt) < 1e-9)
                    connected.append(e.id);
            }
        }
        check(connected.size() == 2, "InvalidInput", "Point must connect exactly two curves");
        id1 = connected[0];
        id2 = connected[1];
    } else {
        auto boundaries = request.value("boundaries").toArray();
        check(boundaries.size() == 1, "InvalidInput", "Select a second boundary entity for Fillet");
        id2 = boundaries[0].toString();
    }
    check(id1 != id2, "InvalidInput", "Fillet requires two distinct entities");
    auto e1 = source.entity(id1), e2 = source.entity(id2);
    check(!constructionOf(source, id1) && !constructionOf(source, id2), "UnsupportedOperation",
          "Construction groups must be edited as a group");

    for (auto e : {e1, e2})
        check(e.kind == "Line" || e.kind == "Arc" || (advancedCurve(e.kind) && e.kind != "Ellipse"),
              "UnsupportedOperation", "Fillet requires open curves");
    double r = number(request, "radius");
    check(r >= 1e-9 && r <= 1e3, "InvalidInput", "Fillet radius must be between 1 nm and 1 km");
    bool shared = request.value("shared_radius").toBool();

    Vec2 V{0, 0};
    if (!cornerVertex.isEmpty()) {
        V = source.entity(cornerVertex).point;
    } else if (e1.kind == "Line" && e2.kind == "Line") {
        auto a1 = source.entity(e1.a).point, b1 = source.entity(e1.b).point, d1 = sub(b1, a1);
        auto a2 = source.entity(e2.a).point, b2 = source.entity(e2.b).point, d2 = sub(b2, a2);
        double den = cross(d1, d2);
        check(std::abs(den) > 1e-12 * length(a1, b1) * length(a2, b2), "InvalidGeometry",
              "Nonparallel lines required for fillet");
        double t1 = cross(sub(a2, a1), d2) / den;
        V = add(a1, mul(d1, t1));
    } else if (e1.kind == "Line" && e2.kind == "Arc") {
        auto hits =
            circleLineIntersections(source.entity(e2.a).point, e2.radius, source.entity(e1.a).point,
                                    source.entity(e1.b).point, false);
        check(!hits.empty(), "InvalidGeometry", "Curves must intersect for fillet");
        Vec2 bestV = hits[0];
        double bestDist = 1e18;
        auto pA1 = source.entity(e1.a).point, pB1 = source.entity(e1.b).point;
        auto Ca = source.entity(e2.a).point;
        Vec2 arcP0 = arcPoint(Ca, e2.radius, e2.start);
        Vec2 arcP1 = arcPoint(Ca, e2.radius, e2.start + e2.sweep);
        for (auto h : hits) {
            double d =
                std::min({length(h, pA1), length(h, pB1), length(h, arcP0), length(h, arcP1)});
            if (d < bestDist) {
                bestDist = d;
                bestV = h;
            }
        }
        V = bestV;
    } else if (e1.kind == "Arc" && e2.kind == "Line") {
        auto hits =
            circleLineIntersections(source.entity(e1.a).point, e1.radius, source.entity(e2.a).point,
                                    source.entity(e2.b).point, false);
        check(!hits.empty(), "InvalidGeometry", "Curves must intersect for fillet");
        Vec2 bestV = hits[0];
        double bestDist = 1e18;
        auto pA2 = source.entity(e2.a).point, pB2 = source.entity(e2.b).point;
        auto Ca = source.entity(e1.a).point;
        Vec2 arcP0 = arcPoint(Ca, e1.radius, e1.start);
        Vec2 arcP1 = arcPoint(Ca, e1.radius, e1.start + e1.sweep);
        for (auto h : hits) {
            double d =
                std::min({length(h, pA2), length(h, pB2), length(h, arcP0), length(h, arcP1)});
            if (d < bestDist) {
                bestDist = d;
                bestV = h;
            }
        }
        V = bestV;
    } else if (e1.kind == "Arc" && e2.kind == "Arc") {
        auto hits = circleCircleIntersections(source.entity(e1.a).point, e1.radius,
                                              source.entity(e2.a).point, e2.radius);
        check(!hits.empty(), "InvalidGeometry", "Curves must intersect for fillet");
        Vec2 bestV = hits[0];
        double bestDist = 1e18;
        auto Ca1 = source.entity(e1.a).point, Ca2 = source.entity(e2.a).point;
        Vec2 a0 = arcPoint(Ca1, e1.radius, e1.start),
             a1 = arcPoint(Ca1, e1.radius, e1.start + e1.sweep);
        Vec2 b0 = arcPoint(Ca2, e2.radius, e2.start),
             b1 = arcPoint(Ca2, e2.radius, e2.start + e2.sweep);
        for (auto h : hits) {
            double d = std::min({length(h, a0), length(h, a1), length(h, b0), length(h, b1)});
            if (d < bestDist) {
                bestDist = d;
                bestV = h;
            }
        }
        V = bestV;
    } else {
        check((e1.kind == "Line" || e1.kind == "Arc" || advancedCurve(e1.kind)) &&
                  (e2.kind == "Line" || e2.kind == "Arc" || advancedCurve(e2.kind)) &&
                  e1.kind != "Ellipse" && e2.kind != "Ellipse",
              "UnsupportedOperation", "Fillet requires two open curves");
        auto [a, b] = curveRange(e1);
        auto [c, d] = curveRange(e2);
        std::vector<Vec2> corners;
        for (double u : {a, b})
            for (double v : {c, d}) {
                auto p = curveValue(source, e1, u);
                if (length(p, curveValue(source, e2, v)) < 1e-9 &&
                    (corners.empty() || length(corners.front(), p) > 1e-9))
                    corners.push_back(p);
            }
        check(corners.size() == 1, "ReferenceAmbiguous", "Select curves with one shared endpoint");
        V = corners.front();
    }

    auto getOutgoing = [&](const Entity &e) -> Vec2 {
        if (e.kind == "Line") {
            auto a = source.entity(e.a).point, b = source.entity(e.b).point;
            return length(a, V) < length(b, V) ? unit(sub(b, a)) : unit(sub(a, b));
        } else if (e.kind == "Arc") {
            auto C = source.entity(e.a).point;
            Vec2 rad = sub(V, C);
            Vec2 tang{-rad.y, rad.x};
            double a0 = e.start;
            Vec2 p0 = add(C, {e.radius * std::cos(a0), e.radius * std::sin(a0)});
            Vec2 p1 = add(C, {e.radius * std::cos(e.start + e.sweep),
                              e.radius * std::sin(e.start + e.sweep)});
            return length(p0, V) <= length(p1, V) ? unit(tang) : unit(mul(tang, -1.0));
        } else if (advancedCurve(e.kind)) {
            auto [u0, u1] = curveRange(e);
            Vec2 p0 = curveValue(source, e, u0);
            Vec2 p1 = curveValue(source, e, u1);
            double u = length(p0, V) <= length(p1, V) ? u0 : u1;
            auto d = curveDerivative(source, e, u);
            return length(p0, V) <= length(p1, V) ? unit(d) : unit(mul(d, -1.0));
        }
        return {1, 0};
    };

    Vec2 u1 = getOutgoing(e1), u2 = getOutgoing(e2);
    double cosA = std::clamp(dot(u1, u2), -0.9999999, 0.9999999);
    double halfAngle = std::acos(cosA) / 2.0;
    double tDist = r / std::tan(halfAngle);
    check(tDist > 1e-9, "InvalidGeometry", "Fillet geometry is degenerate");

    Vec2 bisector = unit(add(u1, u2));
    Vec2 C = add(V, mul(bisector, r / std::sin(halfAngle)));

    bool exactAdvanced = advancedCurve(e1.kind) || advancedCurve(e2.kind);
    double contact1 = 0, contact2 = 0;
    if (exactAdvanced) {
        auto solutions = geometry::sketchFillets(source, e1, e2, r);
        check(!solutions.empty(), "InvalidGeometry", "No finite tangent fillet exists");
        auto best = std::min_element(solutions.begin(), solutions.end(), [&](auto &a, auto &b) {
            return length(a.center, C) < length(b.center, C);
        });
        C = best->center;
        contact1 = best->first;
        contact2 = best->second;
    }
    bool hasArc = !exactAdvanced && (e1.kind == "Arc" || e2.kind == "Arc");
    if (hasArc) {
        auto getOffsetLines = [&](const Entity &lineEnt) -> std::vector<std::pair<Vec2, Vec2>> {
            auto pA = source.entity(lineEnt.a).point;
            auto pB = source.entity(lineEnt.b).point;
            Vec2 dir = unit(sub(pB, pA));
            Vec2 norm{-dir.y, dir.x};
            return {{add(pA, mul(norm, r)), dir}, {add(pA, mul(norm, -r)), dir}};
        };

        auto getOffsetCircles = [&](const Entity &arcEnt) -> std::vector<std::pair<Vec2, double>> {
            auto Ca = source.entity(arcEnt.a).point;
            double Ra = arcEnt.radius;
            std::vector<std::pair<Vec2, double>> res;
            if (Ra + r > 1e-9)
                res.push_back({Ca, Ra + r});
            if (std::abs(Ra - r) > 1e-9)
                res.push_back({Ca, std::abs(Ra - r)});
            return res;
        };

        std::vector<Vec2> hits;
        if (e1.kind == "Line" && e2.kind == "Arc") {
            for (auto [ptOff, dir] : getOffsetLines(e1)) {
                for (auto [Ca, R_off] : getOffsetCircles(e2)) {
                    auto h = circleLineIntersections(Ca, R_off, ptOff, add(ptOff, dir), false);
                    hits.insert(hits.end(), h.begin(), h.end());
                }
            }
        } else if (e1.kind == "Arc" && e2.kind == "Line") {
            for (auto [ptOff, dir] : getOffsetLines(e2)) {
                for (auto [Ca, R_off] : getOffsetCircles(e1)) {
                    auto h = circleLineIntersections(Ca, R_off, ptOff, add(ptOff, dir), false);
                    hits.insert(hits.end(), h.begin(), h.end());
                }
            }
        } else if (e1.kind == "Arc" && e2.kind == "Arc") {
            for (auto [Ca1, R_off1] : getOffsetCircles(e1)) {
                for (auto [Ca2, R_off2] : getOffsetCircles(e2)) {
                    auto h = circleCircleIntersections(Ca1, R_off1, Ca2, R_off2);
                    hits.insert(hits.end(), h.begin(), h.end());
                }
            }
        }

        auto arcContact = [&](const Entity &arcEnt, Vec2 center) -> Vec2 {
            auto Ca = source.entity(arcEnt.a).point;
            Vec2 dir = sub(center, Ca);
            double l = std::hypot(dir.x, dir.y);
            if (l < 1e-12)
                return Ca;
            Vec2 u = {dir.x / l, dir.y / l};
            Vec2 pPlus = add(Ca, mul(u, arcEnt.radius));
            Vec2 pMinus = sub(Ca, mul(u, arcEnt.radius));
            return std::abs(length(center, pPlus) - r) <= std::abs(length(center, pMinus) - r)
                       ? pPlus
                       : pMinus;
        };

        auto lineContact = [&](const Entity &lineEnt, Vec2 center) -> Vec2 {
            auto pA = source.entity(lineEnt.a).point;
            auto pB = source.entity(lineEnt.b).point;
            Vec2 d = unit(sub(pB, pA));
            return add(pA, mul(d, dot(sub(center, pA), d)));
        };

        check(!hits.empty(), "InvalidGeometry", "Curves have no valid fillet center");
        Vec2 bestC = hits[0];
        double bestD = 1e18;
        for (auto h : hits) {
            Vec2 candT1 = (e1.kind == "Line") ? lineContact(e1, h) : arcContact(e1, h);
            Vec2 candT2 = (e2.kind == "Line") ? lineContact(e2, h) : arcContact(e2, h);
            auto inside = [&](const Entity &e, Vec2 point) {
                if (e.kind != "Arc")
                    return true;
                auto center = source.entity(e.a).point;
                double u = std::fmod(
                    std::atan2(point.y - center.y, point.x - center.x) - e.start + 2 * tau, tau);
                return u > 1e-9 / e.radius && u < e.sweep - 1e-9 / e.radius;
            };
            if (!inside(e1, candT1) || !inside(e2, candT2))
                continue;
            if (length(candT1, candT2) < 1e-9)
                continue;
            if (std::abs(length(h, candT1) - r) > 1e-9 || std::abs(length(h, candT2) - r) > 1e-9)
                continue;
            double d = length(h, C);
            if (d < bestD) {
                bestD = d;
                bestC = h;
            }
        }
        check(bestD < 1e17, "InvalidGeometry", "Curves have no valid fillet center");
        C = bestC;
    }

    auto getContactPoint = [&](const Entity &e) -> Vec2 {
        if (e.kind == "Line") {
            auto a = source.entity(e.a).point, b = source.entity(e.b).point;
            Vec2 d = unit(sub(b, a));
            return add(a, mul(d, dot(sub(C, a), d)));
        } else if (e.kind == "Arc") {
            auto Ca = source.entity(e.a).point;
            Vec2 dir = sub(C, Ca);
            double l = std::hypot(dir.x, dir.y);
            if (l < 1e-12)
                return Ca;
            Vec2 u = {dir.x / l, dir.y / l};
            Vec2 pPlus = add(Ca, mul(u, e.radius));
            Vec2 pMinus = sub(Ca, mul(u, e.radius));
            return std::abs(length(C, pPlus) - r) <= std::abs(length(C, pMinus) - r) ? pPlus
                                                                                     : pMinus;
        }
        return V;
    };

    Vec2 T1 = exactAdvanced ? curveValue(source, e1, contact1) : getContactPoint(e1);
    Vec2 T2 = exactAdvanced ? curveValue(source, e2, contact2) : getContactPoint(e2);

    check(std::abs(length(C, T1) - r) < 1e-9, "InvalidGeometry",
          "Fillet does not contact first operand");
    check(std::abs(length(C, T2) - r) < 1e-9, "InvalidGeometry",
          "Fillet does not contact second operand");

    auto checkTangency = [&](const Entity &e, Vec2 T) {
        Vec2 filletRad = unit(sub(T, C));
        Vec2 operandTangent;
        if (e.kind == "Line") {
            operandTangent = unit(sub(source.entity(e.b).point, source.entity(e.a).point));
        } else if (e.kind == "Arc") {
            auto Ca = source.entity(e.a).point;
            Vec2 rad = unit(sub(T, Ca));
            operandTangent = {-rad.y, rad.x};
        } else {
            operandTangent = unit(curveDerivative(source, e, e.id == id1 ? contact1 : contact2));
        }
        check(std::abs(dot(filletRad, operandTangent)) < 1e-3, "InvalidGeometry",
              "Fillet is not tangent to operand");
    };
    checkTangency(e1, T1);
    checkTangency(e2, T2);

    QSet<QString> affected{id1, id2};
    if (!cornerVertex.isEmpty())
        affected.insert(cornerVertex);
    auto ptT1 = candidate.point(T1);
    auto ptT2 = candidate.point(T2);
    auto ptC = candidate.point(C, true);

    auto trimOperand = [&](Entity &e, QString ptId, Vec2 ptPos) {
        if (e.kind == "Line") {
            auto a = source.entity(e.a).point, b = source.entity(e.b).point;
            auto far = length(a, V) < length(b, V) ? b : a;
            check(dot(sub(ptPos, V), sub(far, V)) > 0 && length(ptPos, V) < length(far, V) - 1e-9,
                  "InvalidGeometry", "Fillet consumes the retained line");
            if (length(a, V) < length(b, V)) {
                affected.insert(e.a);
                e.a = ptId;
            } else {
                affected.insert(e.b);
                e.b = ptId;
            }
        } else if (e.kind == "Arc") {
            auto Ca = source.entity(e.a).point;
            double tAng = std::fmod(std::atan2(ptPos.y - Ca.y, ptPos.x - Ca.x) + 2 * tau, tau);
            double parameter = std::fmod(tAng - e.start + 2 * tau, tau);
            check(parameter > 1e-9 / e.radius && parameter < e.sweep - 1e-9 / e.radius,
                  "InvalidGeometry", "Fillet contact must lie inside the finite arc");
            double a0 = std::fmod(e.start + 2 * tau, tau);
            double a1 = std::fmod(e.start + e.sweep + 2 * tau, tau);
            Vec2 p0 = add(Ca, {e.radius * std::cos(a0), e.radius * std::sin(a0)});
            Vec2 p1 = add(Ca, {e.radius * std::cos(a1), e.radius * std::sin(a1)});
            if (length(p0, V) <= length(p1, V)) {
                double sw = std::fmod(a1 - tAng + 2 * tau, tau);
                e.start = std::remainder(tAng, tau);
                e.sweep = sw;
            } else {
                double sw = std::fmod(tAng - a0 + 2 * tau, tau);
                e.start = std::remainder(a0, tau);
                e.sweep = sw;
            }
        } else if (advancedCurve(e.kind)) {
            auto [a, b] = curveRange(e);
            double u = e.id == id1 ? contact1 : contact2;
            if (length(curveValue(source, e, a), V) <= length(curveValue(source, e, b), V))
                e.start = u;
            else
                b = u;
            e.sweep = b - e.start;
        }
    };

    for (auto &e : candidate.entities) {
        if (e.id == id1)
            trimOperand(e, ptT1, T1);
        if (e.id == id2)
            trimOperand(e, ptT2, T2);
    }

    double startAngle = std::atan2(T1.y - C.y, T1.x - C.x);
    double endAngle = std::atan2(T2.y - C.y, T2.x - C.x);
    double sweep = std::fmod(endAngle - startAngle + 2 * tau, tau);
    if (sweep > tau / 2.0) {
        startAngle = endAngle;
        sweep = tau - sweep;
    }
    if (startAngle > tau / 2.0)
        startAngle -= tau;
    else if (startAngle < -tau / 2.0)
        startAngle += tau;
    auto arcId = candidate.arc(ptC, r, startAngle, sweep);
    auto [disclosure, removed] = discloseConstraints(candidate, affected);
    QCborArray descendants{id1, id2, arcId};
    if (shared) {
        for (const auto &other : source.entities) {
            if (other.kind == "Arc" && std::abs(other.radius - r) < 1e-9) {
                Constraint eq;
                eq.kind = "Equal";
                eq.a = arcId;
                eq.b = other.id;
                candidate.constraints.push_back(eq);
                break;
            }
        }
    }
    candidate.edits.append(fields({{"id", newId()},
                                   {"mode", "Fillet"},
                                   {"source", id1},
                                   {"descendants", descendants},
                                   {"removed_constraints", removed}}));
    candidate.validate();
    return fields({{"candidate", candidate.map()}, {"affected_constraints", disclosure}});
}

QCborMap previewChamfer(const Sketch &source, const QCborMap &request) {
    QSet<QString> allowed{"mode", "source"};
    if (request.contains(QStringLiteral("boundaries")))
        allowed.insert("boundaries");
    for (auto k : {"method", "distance", "distance1", "distance2", "angle", "pick_x", "pick_y",
                   "pick1_x", "pick1_y", "pick2_x", "pick2_y"})
        if (request.contains(QLatin1String(k)))
            allowed.insert(QLatin1String(k));
    wire::keys(request, allowed);
    auto id1 = wire::text(request, "source");
    QString id2;
    auto ent1 = source.entity(id1);
    QString cornerVertex;
    if (ent1.kind == "Point") {
        cornerVertex = id1;
        QStringList connected;
        for (auto &e : source.entities)
            if (e.kind == "Line" && (e.a == id1 || e.b == id1))
                connected.append(e.id);
        check(connected.size() == 2, "InvalidInput", "Point must connect exactly two lines");
        id1 = connected[0];
        id2 = connected[1];
    } else {
        auto boundaries = request.value("boundaries").toArray();
        check(boundaries.size() == 1, "InvalidInput",
              "Select a second boundary entity for Chamfer");
        id2 = boundaries[0].toString();
    }
    check(id1 != id2, "InvalidInput", "Chamfer requires two distinct entities");
    auto e1 = source.entity(id1), e2 = source.entity(id2);
    check(e1.kind == "Line" && e2.kind == "Line", "UnsupportedOperation",
          "Chamfer operates on two straight lines");
    check(!constructionOf(source, id1) && !constructionOf(source, id2), "UnsupportedOperation",
          "Construction groups must be edited as a group");

    QString method = request.value("method").toString();
    if (method.isEmpty())
        method = "Equal distance";
    double d1 = 0.0, d2 = 0.0;
    if (method == "Equal distance") {
        d1 = d2 = number(request, "distance");
    } else if (method == "Unequal distance") {
        d1 = number(request, "distance1");
        d2 = number(request, "distance2");
    } else if (method == "Angle distance") {
        d1 = number(request, "distance1");
        double theta = number(request, "angle");
        check(theta > 1e-4 && theta < 3.1415, "InvalidInput", "Chamfer angle out of valid range");
        d2 = d1; // Recomputed below using the actual corner angle.
    } else {
        check(false, "InvalidInput", "Unknown chamfer method");
    }
    check(d1 >= 1e-9 && d2 >= 1e-9, "InvalidInput", "Chamfer distances must be positive");

    auto a1 = source.entity(e1.a).point, b1 = source.entity(e1.b).point, dir1 = sub(b1, a1);
    auto a2 = source.entity(e2.a).point, b2 = source.entity(e2.b).point, dir2 = sub(b2, a2);
    double den = cross(dir1, dir2);
    check(std::abs(den) > 1e-12 * length(a1, b1) * length(a2, b2), "InvalidGeometry",
          "Nonparallel lines required for chamfer");
    double t1 = cross(sub(a2, a1), dir2) / den;
    double t2 = cross(sub(a2, a1), dir1) / den;
    Vec2 V = add(a1, mul(dir1, t1));

    Vec2 u1 = unit(t1 > 0.5 ? sub(a1, V) : sub(b1, V));
    Vec2 u2 = unit(t2 > 0.5 ? sub(a2, V) : sub(b2, V));
    if (method == "Angle distance") {
        double theta = number(request, "angle");
        double alpha = std::acos(std::clamp(dot(u1, u2), -1.0, 1.0));
        check(theta + alpha < tau / 2, "InvalidInput", "Chamfer angle cannot meet the second line");
        d2 = d1 * std::sin(theta) / std::sin(alpha + theta);
    }
    check(d1 < length(t1 > .5 ? a1 : b1, V) - 1e-9 && d2 < length(t2 > .5 ? a2 : b2, V) - 1e-9,
          "InvalidGeometry", "Chamfer consumes a retained line");
    Vec2 P1 = add(V, mul(u1, d1));
    Vec2 P2 = add(V, mul(u2, d2));

    auto candidate = source;
    QSet<QString> affected{id1, id2};
    if (!cornerVertex.isEmpty())
        affected.insert(cornerVertex);
    auto ptP1 = candidate.point(P1);
    auto ptP2 = candidate.point(P2);
    for (auto &e : candidate.entities) {
        if (e.id == id1) {
            if (t1 > 0.5) {
                affected.insert(e.b);
                e.b = ptP1;
            } else {
                affected.insert(e.a);
                e.a = ptP1;
            }
        }
        if (e.id == id2) {
            if (t2 > 0.5) {
                affected.insert(e.b);
                e.b = ptP2;
            } else {
                affected.insert(e.a);
                e.a = ptP2;
            }
        }
    }
    auto chamferLine = candidate.line(ptP1, ptP2);
    auto [disclosure, removed] = discloseConstraints(candidate, affected);
    QCborArray descendants{id1, id2, chamferLine};
    candidate.edits.append(fields({{"id", newId()},
                                   {"mode", "Chamfer"},
                                   {"source", id1},
                                   {"descendants", descendants},
                                   {"removed_constraints", removed}}));
    candidate.validate();
    return fields({{"candidate", candidate.map()}, {"affected_constraints", disclosure}});
}

std::pair<Vec2, Vec2> endpoints(const Sketch &s, const Entity &e) {
    auto [a, b] = curveRange(e);
    return {curveValue(s, e, a), curveValue(s, e, b)};
}

QStringList collectChain(const Sketch &s, const QString &startId) {
    auto &seed = s.entity(startId);
    check(seed.kind == "Line" || seed.kind == "Arc" || seed.kind == "Circle",
          "UnsupportedOperation", "Offset chain requires lines, arcs or a single circle");
    QStringList chain{startId};
    if (seed.kind == "Circle")
        return chain;
    QSet<QString> visited{startId};
    auto [front, back] = endpoints(s, seed);
    for (;;) {
        if (chain.size() > 1 && length(front, back) < 1e-9)
            break;
        QString match;
        Vec2 next;
        bool prepend = false;
        for (auto &e : s.entities) {
            if (visited.contains(e.id) || (e.kind != "Line" && e.kind != "Arc"))
                continue;
            auto [a, b] = endpoints(s, e);
            bool atBack = length(a, back) < 1e-9 || length(b, back) < 1e-9;
            bool atFront = length(a, front) < 1e-9 || length(b, front) < 1e-9;
            if (!atBack && !atFront)
                continue;
            // Grow one end at a time; two candidates at that end are ambiguous.
            if (!match.isEmpty() && prepend != !atBack)
                continue;
            check(match.isEmpty(), "ReferenceAmbiguous", "Offset chain branches at a vertex");
            match = e.id;
            prepend = !atBack;
            auto joint = prepend ? front : back;
            next = length(a, joint) < 1e-9 ? b : a;
        }
        if (match.isEmpty())
            break;
        visited.insert(match);
        if (prepend) {
            chain.prepend(match);
            front = next;
        } else {
            chain.append(match);
            back = next;
        }
    }
    return chain;
}

QCborMap previewOffset(const Sketch &source, const QCborMap &request) {
    QSet<QString> allowed{"mode", "source", "distance"};
    for (auto k : {"chain", "reverse", "bidirectional", "cap_ends", "make_construction"})
        if (request.contains(QLatin1String(k)))
            allowed.insert(QLatin1String(k));
    wire::keys(request, allowed);
    auto id = wire::text(request, "source");
    double distance = number(request, "distance");
    check(distance >= 1e-9 && distance <= 1e3, "InvalidInput", "Offset distance out of range");
    bool bidi = request.value("bidirectional").toBool();
    auto caps = request.value("cap_ends").toString();
    if (caps.isEmpty())
        caps = "None";
    check(caps == "None" || caps == "Line" || caps == "Arc", "InvalidInput", "Unknown cap type");
    check(caps == "None" || bidi, "InvalidInput", "Caps require bidirectional offset");
    QStringList ids{id};
    auto chain = request.value("chain");
    if (chain.isArray()) {
        for (auto v : chain.toArray()) {
            check(v.isString(), "InvalidInput", "Chain entity ID required");
            if (!ids.contains(v.toString()))
                ids.append(v.toString());
        }
    } else if (chain.toBool())
        ids = collectChain(source, id);
    for (auto cid : ids) {
        auto &e = source.entity(cid);
        check(e.kind == "Line" || e.kind == "Arc" || (ids.size() == 1 && e.kind == "Circle"),
              "UnsupportedOperation", "Offset supports line/arc chains or a single circle");
        check(!constructionOf(source, cid), "UnsupportedOperation",
              "Edit construction groups together");
        for (auto v : source.edits) {
            auto edit = v.toMap();
            if (edit.value("mode").toString() == "Offset entities")
                for (auto child : edit.value("descendants").toArray())
                    check(child.toString() != cid, "UnsupportedOperation",
                          "Cannot offset an existing offset");
        }
    }
    std::vector<bool> forward(ids.size(), true);
    if (ids.size() > 1) {
        auto [a, b] = endpoints(source, source.entity(ids[0]));
        auto [c, d] = endpoints(source, source.entity(ids[1]));
        forward[0] = length(b, c) < 1e-9 || length(b, d) < 1e-9;
        Vec2 previous = forward[0] ? b : a;
        for (int i = 1; i < ids.size(); ++i) {
            auto [u, v] = endpoints(source, source.entity(ids[i]));
            check(length(previous, u) < 1e-9 || length(previous, v) < 1e-9, "InvalidInput",
                  "Selected offset chain must be connected and ordered");
            forward[i] = length(previous, u) < 1e-9;
            previous = forward[i] ? v : u;
        }
    }
    auto orientedEnds = [&](const Sketch &s, QString eid, int index) {
        auto pair = endpoints(s, s.entity(eid));
        if (!forward[index])
            std::swap(pair.first, pair.second);
        return pair;
    };
    auto first = orientedEnds(source, ids.front(), 0).first;
    auto last = orientedEnds(source, ids.back(), ids.size() - 1).second;
    bool closed = source.entity(ids.front()).kind == "Circle" || length(first, last) < 1e-9;
    auto candidate = source;
    QCborArray descendants;
    std::vector<QStringList> outputs;
    for (double side :
         bidi ? std::vector<double>{distance, -distance}
              : std::vector<double>{request.value("reverse").toBool() ? -distance : distance}) {
        QStringList out;
        for (int i = 0; i < ids.size(); ++i) {
            auto e = source.entity(ids[i]);
            if (e.kind == "Line") {
                auto [a, b] = endpoints(source, e);
                auto dir = unit(sub(b, a));
                auto delta = mul(Vec2{-dir.y, dir.x}, forward[i] ? side : -side);
                out.append(
                    candidate.line(candidate.point(add(a, delta)), candidate.point(add(b, delta))));
            } else {
                double radius =
                    e.radius + (e.kind == "Circle" ? side : (forward[i] ? -side : side));
                check(radius > 1e-9, "InvalidGeometry", "Offset radius is degenerate");
                out.append(e.kind == "Circle" ? candidate.circle(e.a, radius)
                                              : candidate.arc(e.a, radius, e.start, e.sweep));
            }
        }
        auto setEnd = [&](int i, bool start, Vec2 p, QString point) {
            auto eid = out[i];
            auto e = candidate.entity(eid);
            bool nativeStart = start == forward[i];
            if (e.kind == "Line") {
                if (nativeStart)
                    candidate.entity(eid).a = point;
                else
                    candidate.entity(eid).b = point;
            } else {
                auto center = candidate.entity(e.a).point;
                double angle = std::atan2(p.y - center.y, p.x - center.x);
                auto &arc = candidate.entity(eid);
                if (nativeStart) {
                    arc.sweep = std::fmod(e.start + e.sweep - angle + 2 * tau, tau);
                    arc.start = angle;
                } else
                    arc.sweep = std::fmod(angle - e.start + 2 * tau, tau);
            }
        };
        for (int i = 0; i < (closed && ids.size() > 1 ? ids.size() : ids.size() - 1); ++i) {
            int j = (i + 1) % ids.size();
            auto e = candidate.entity(out[i]), f = candidate.entity(out[j]);
            auto joint = orientedEnds(source, ids[i], i).second;
            std::vector<Vec2> hits;
            if (e.kind == "Line" && f.kind == "Line") {
                auto [a, b] = endpoints(candidate, e);
                auto [c, d] = endpoints(candidate, f);
                auto u = sub(b, a), v = sub(d, c);
                double den = cross(u, v);
                if (std::abs(den) > 1e-12 * length(a, b) * length(c, d))
                    hits.push_back(add(a, mul(u, cross(sub(c, a), v) / den)));
                else if (length(orientedEnds(candidate, out[i], i).second,
                                orientedEnds(candidate, out[j], j).first) < 1e-9)
                    hits.push_back(orientedEnds(candidate, out[i], i).second);
            } else if (e.kind == "Line" || f.kind == "Line") {
                auto line = e.kind == "Line" ? e : f, arc = e.kind == "Arc" ? e : f;
                auto [a, b] = endpoints(candidate, line);
                hits =
                    circleLineIntersections(candidate.entity(arc.a).point, arc.radius, a, b, false);
            } else
                hits = circleCircleIntersections(candidate.entity(e.a).point, e.radius,
                                                 candidate.entity(f.a).point, f.radius);
            check(!hits.empty(), "InvalidGeometry", "Offset curves cannot be joined");
            auto hit = *std::min_element(hits.begin(), hits.end(), [&](auto a, auto b) {
                return length(a, joint) < length(b, joint);
            });
            auto shared = candidate.point(hit);
            setEnd(i, false, hit, shared);
            setEnd(j, true, hit, shared);
        }
        for (int i = 0; i < out.size(); ++i) {
            if (source.entity(ids[i]).kind == "Line") {
                auto [a, b] = endpoints(source, source.entity(ids[i]));
                auto [c, d] = endpoints(candidate, candidate.entity(out[i]));
                check(dot(sub(b, a), sub(d, c)) > 1e-18, "InvalidGeometry",
                      "Offset consumes a line");
            }
            descendants.append(out[i]);
        }
        outputs.push_back(out);
    }
    if (bidi && !closed && caps != "None") {
        for (bool start : {true, false}) {
            int i = start ? 0 : ids.size() - 1;
            auto positive = orientedEnds(candidate, outputs[0][i], i);
            auto negative = orientedEnds(candidate, outputs[1][i], i);
            auto a = start ? positive.first : positive.second,
                 b = start ? negative.first : negative.second;
            if (caps == "Line")
                descendants.append(candidate.line(candidate.point(a), candidate.point(b)));
            else {
                auto &e = source.entity(ids[i]);
                auto [u, v] = curveRange(e);
                auto dir = unit(curveDerivative(source, e, (start == forward[i]) ? u : v));
                dir = mul(dir, (start == forward[i]) ? -1 : 1);
                auto center = mul(add(a, b), .5);
                double angle = std::atan2(a.y - center.y, a.x - center.x);
                double middle = angle + tau / 4;
                if (dot(Vec2{std::cos(middle), std::sin(middle)}, dir) < 0)
                    angle = std::atan2(b.y - center.y, b.x - center.x);
                descendants.append(candidate.arc(candidate.point(center, true), length(a, center),
                                                 angle, tau / 2));
            }
        }
    }
    if (request.value("make_construction").toBool())
        for (auto cid : ids)
            candidate.entity(cid).construction = true;
    candidate.edits.append(fields({{"id", newId()},
                                   {"mode", "Offset entities"},
                                   {"source", id},
                                   {"descendants", descendants},
                                   {"removed_constraints", QCborArray{}}}));
    candidate.validate();
    return fields({{"candidate", candidate.map()}, {"affected_constraints", QCborArray{}}});
}

struct PointCopies {
    Sketch &candidate;
    const Sketch &source;
    QMap<QString, QString> ids;
    QString point(QString id, Vec2 p) {
        if (!ids.contains(id)) {
            check(candidate.entities.size() < 256, "ResourceLimit",
                  "Copied geometry exceeds sketch limit");
            ids[id] = candidate.point(p, source.entity(id).construction);
        }
        return ids[id];
    }
};
QSet<QString> changedGeometry(const Sketch &source, const Sketch &candidate) {
    QSet<QString> affected;
    for (auto &e : source.entities) {
        auto &after = candidate.entity(e.id);
        if ((e.kind == "Point" && length(e.point, after.point) > 1e-14) ||
            e.radius != after.radius || e.start != after.start || e.sweep != after.sweep)
            affected.insert(e.id);
    }
    for (auto &e : source.entities)
        for (auto id : controlPoints(e))
            if (affected.contains(id))
                affected.insert(e.id);
    return affected;
}

QCborMap previewPattern(const Sketch &source, const QCborMap &request) {
    auto mode = wire::text(request, "mode");
    QSet<QString> allowed{"mode"};
    for (auto k : {"source", "entities", "angle1", "spacing1", "count1", "angle2", "spacing2",
                   "count2", "center_x", "center_y", "angle", "count", "equal_spacing", "spacing"})
        if (request.contains(QLatin1String(k)))
            allowed.insert(QLatin1String(k));
    wire::keys(request, allowed);
    QStringList entities;
    if (request.contains(QStringLiteral("entities")) && request.value("entities").isArray()) {
        for (auto v : request.value("entities").toArray())
            entities.append(v.toString());
    } else if (request.contains(QStringLiteral("source"))) {
        entities.append(wire::text(request, "source"));
    }
    check(!entities.empty(), "InvalidInput", "Pattern entities required");
    QSet<QString> selected;
    for (auto id : entities) {
        check(!selected.contains(id), "InvalidInput", "Distinct selected entities required");
        check(!constructionOf(source, id), "UnsupportedOperation",
              "Edit construction groups together");
        source.entity(id);
        selected.insert(id);
    }
    for (auto id : entities) {
        auto &e = source.entity(id);
        check(e.kind == "Line" || e.kind == "Arc" || e.kind == "Circle", "UnsupportedOperation",
              "Patterns support lines, arcs and circles");
    }
    auto countValue = [&](QString key, int fallback, int minimum) {
        double value = number(request, key, fallback, false);
        check(value >= minimum && value <= 64 && value == std::floor(value), "ResourceLimit",
              "Pattern counts must be bounded whole numbers");
        return int(value);
    };
    auto candidate = source;
    QCborArray descendants;

    if (mode == "Linear pattern") {
        int c1 = countValue("count1", 2, 1);
        int c2 = countValue("count2", 1, 1);
        check(c1 * c2 > 1 && c1 * c2 <= 64, "ResourceLimit",
              "Pattern total instances must be between 2 and 64");
        double s1 = number(request, "spacing1", 0.01, false);
        double s2 = number(request, "spacing2", 0.01, false);
        double a1 = number(request, "angle1", 0.0, false);
        double a2 = number(request, "angle2", tau / 4, false);
        Vec2 dir1{std::cos(a1) * s1, std::sin(a1) * s1};
        Vec2 dir2{std::cos(a2) * s2, std::sin(a2) * s2};

        for (int i = 0; i < c1; ++i) {
            for (int j = 0; j < c2; ++j) {
                if (i == 0 && j == 0)
                    continue;
                Vec2 shift = add(mul(dir1, i), mul(dir2, j));
                PointCopies copies{candidate, source, {}};
                for (auto eid : entities) {
                    auto e = source.entity(eid);
                    if (e.kind == "Line") {
                        auto p1 = add(source.entity(e.a).point, shift);
                        auto p2 = add(source.entity(e.b).point, shift);
                        descendants.append(candidate.line(copies.point(e.a, p1),
                                                          copies.point(e.b, p2), e.construction));
                    } else if (e.kind == "Circle") {
                        auto center = add(source.entity(e.a).point, shift);
                        descendants.append(
                            candidate.circle(copies.point(e.a, center), e.radius, e.construction));
                    } else if (e.kind == "Arc") {
                        auto center = add(source.entity(e.a).point, shift);
                        descendants.append(candidate.arc(copies.point(e.a, center), e.radius,
                                                         e.start, e.sweep, e.construction));
                    }
                }
            }
        }
    } else {
        int count = countValue("count", 4, 2);
        check(count <= 64, "ResourceLimit", "Circular pattern count at most 64");
        double sweep = number(request, "angle", tau, false);
        Vec2 center{number(request, "center_x", 0.0, false),
                    number(request, "center_y", 0.0, false)};
        bool equalSpacing = !request.contains(QStringLiteral("equal_spacing")) ||
                            request.value("equal_spacing").toBool();
        double dTheta = 0.0;
        if (equalSpacing) {
            if (std::abs(std::abs(sweep) - tau) < 1e-4)
                dTheta = sweep / count;
            else
                dTheta = sweep / (count - 1);
        } else {
            dTheta = number(request, "spacing", sweep, false);
        }

        for (int k = 1; k < count; ++k) {
            double angle = k * dTheta;
            PointCopies copies{candidate, source, {}};
            for (auto eid : entities) {
                auto e = source.entity(eid);
                if (e.kind == "Line") {
                    auto p1 = rotatePoint(source.entity(e.a).point, center, angle);
                    auto p2 = rotatePoint(source.entity(e.b).point, center, angle);
                    descendants.append(candidate.line(copies.point(e.a, p1), copies.point(e.b, p2),
                                                      e.construction));
                } else if (e.kind == "Circle") {
                    auto cPt = rotatePoint(source.entity(e.a).point, center, angle);
                    descendants.append(
                        candidate.circle(copies.point(e.a, cPt), e.radius, e.construction));
                } else if (e.kind == "Arc") {
                    auto cPt = rotatePoint(source.entity(e.a).point, center, angle);
                    descendants.append(candidate.arc(copies.point(e.a, cPt), e.radius,
                                                     std::remainder(e.start + angle, tau), e.sweep,
                                                     e.construction));
                }
            }
        }
    }

    candidate.edits.append(fields({{"id", newId()},
                                   {"mode", mode},
                                   {"source", entities.front()},
                                   {"descendants", descendants},
                                   {"removed_constraints", QCborArray{}}}));
    candidate.validate();
    return fields({{"candidate", candidate.map()}, {"affected_constraints", QCborArray{}}});
}

QCborMap previewMirror(const Sketch &source, const QCborMap &request) {
    QSet<QString> allowed{"mode", "mirror_line"};
    for (auto k : {"source", "entities", "copy"})
        if (request.contains(QLatin1String(k)))
            allowed.insert(QLatin1String(k));
    wire::keys(request, allowed);
    auto mode = wire::text(request, "mode");
    auto mirrorLineId = wire::text(request, "mirror_line");
    auto ml = source.entity(mirrorLineId);
    check(ml.kind == "Line", "InvalidInput", "Mirror axis must be a straight line");
    auto lp = source.entity(ml.a).point, ld = sub(source.entity(ml.b).point, lp);

    QStringList entities;
    if (request.contains(QStringLiteral("entities")) && request.value("entities").isArray()) {
        for (auto v : request.value("entities").toArray())
            entities.append(v.toString());
    } else if (request.contains(QStringLiteral("source"))) {
        entities.append(wire::text(request, "source"));
    }
    check(!entities.empty(), "InvalidInput", "Mirror entities required");
    QSet<QString> selected;
    for (auto id : entities) {
        check(!selected.contains(id), "InvalidInput", "Distinct selected entities required");
        check(!constructionOf(source, id), "UnsupportedOperation",
              "Edit construction groups together");
        source.entity(id);
        selected.insert(id);
    }
    bool copy = !request.contains(QStringLiteral("copy")) || request.value("copy").toBool();
    check(mode != "Dynamic mirror" || copy, "InvalidInput",
          "Dynamic mirror requires copy mode to maintain symmetric relations");

    auto candidate = source;
    if (mode == "Dynamic mirror" && !ml.construction) {
        candidate.entity(mirrorLineId).construction = true;
    }
    if (!copy) {
        QSet<QString> points;
        QCborArray descendants;
        for (auto id : entities) {
            auto &e = candidate.entity(id);
            check(!constructionOf(source, id), "UnsupportedOperation",
                  "Edit construction groups together");
            for (auto point : controlPoints(e))
                points.insert(point);
            if (e.kind == "Arc")
                e.start = std::remainder(2 * std::atan2(ld.y, ld.x) - e.start - e.sweep, tau);
            check(!advancedCurve(e.kind), "UnsupportedOperation",
                  "Mirror supports points, lines, circles and arcs");
            descendants.append(id);
        }
        for (auto id : points)
            candidate.entity(id).point = reflectPoint(source.entity(id).point, lp, ld);
        auto [disclosure, removed] =
            discloseConstraints(candidate, changedGeometry(source, candidate));
        candidate.edits.append(fields({{"id", newId()},
                                       {"mode", mode},
                                       {"source", entities.front()},
                                       {"descendants", descendants},
                                       {"removed_constraints", removed}}));
        candidate.validate();
        return fields({{"candidate", candidate.map()}, {"affected_constraints", disclosure}});
    }
    PointCopies copies{candidate, source, {}};
    QCborArray descendants;
    for (auto eid : entities) {
        auto e = source.entity(eid);
        check(e.kind == "Line" || e.kind == "Circle" || e.kind == "Arc", "UnsupportedOperation",
              "Mirror supports lines, circles, and arcs");
        if (e.kind == "Line") {
            auto p1 = reflectPoint(source.entity(e.a).point, lp, ld);
            auto p2 = reflectPoint(source.entity(e.b).point, lp, ld);
            auto pt1 = copies.point(e.a, p1), pt2 = copies.point(e.b, p2);
            descendants.append(candidate.line(pt1, pt2, e.construction));
            if (mode == "Dynamic mirror") {
                Constraint sym1, sym2;
                sym1.kind = sym2.kind = "Symmetric";
                sym1.a = e.a;
                sym1.b = pt1;
                sym1.third = mirrorLineId;
                sym2.a = e.b;
                sym2.b = pt2;
                sym2.third = mirrorLineId;
                candidate.constraints.push_back(sym1);
                candidate.constraints.push_back(sym2);
            }
        } else if (e.kind == "Circle") {
            auto cPt = reflectPoint(source.entity(e.a).point, lp, ld);
            auto newPtId = copies.point(e.a, cPt);
            auto newCircleId = candidate.circle(newPtId, e.radius, e.construction);
            descendants.append(newCircleId);
            if (mode == "Dynamic mirror") {
                Constraint sym;
                sym.kind = "Symmetric";
                sym.a = e.id;
                sym.b = newCircleId;
                sym.third = mirrorLineId;
                candidate.constraints.push_back(sym);
            }
        } else if (e.kind == "Arc") {
            auto cPt = reflectPoint(source.entity(e.a).point, lp, ld);
            double theta = std::atan2(ld.y, ld.x);
            double endAngle = e.start + e.sweep;
            double refStart = std::remainder(2.0 * theta - endAngle, tau);
            auto newPtId = copies.point(e.a, cPt);
            auto newArcId = candidate.arc(newPtId, e.radius, refStart, e.sweep, e.construction);
            descendants.append(newArcId);
            if (mode == "Dynamic mirror") {
                Constraint sym;
                sym.kind = "Symmetric";
                sym.a = e.id;
                sym.b = newArcId;
                sym.third = mirrorLineId;
                candidate.constraints.push_back(sym);
            }
        }
    }
    if (!copy) {
        QSet<QString> toRemove(entities.begin(), entities.end());
        std::erase_if(candidate.entities, [&](auto &e) { return toRemove.contains(e.id); });
    }

    candidate.edits.append(fields({{"id", newId()},
                                   {"mode", mode},
                                   {"source", entities.front()},
                                   {"descendants", descendants},
                                   {"removed_constraints", QCborArray{}}}));
    candidate.validate();
    return fields({{"candidate", candidate.map()}, {"affected_constraints", QCborArray{}}});
}

QCborMap previewTransform(const Sketch &source, const QCborMap &request) {
    auto mode = wire::text(request, "mode");
    QSet<QString> allowed{"mode"};
    for (auto k : {"source", "entities", "delta_x", "delta_y", "center_x", "center_y", "angle",
                   "base_x", "base_y", "factor", "copy"})
        if (request.contains(QLatin1String(k)))
            allowed.insert(QLatin1String(k));
    wire::keys(request, allowed);

    QStringList entities;
    if (request.contains(QStringLiteral("entities")) && request.value("entities").isArray()) {
        for (auto v : request.value("entities").toArray())
            entities.append(v.toString());
    } else if (request.contains(QStringLiteral("source"))) {
        entities.append(wire::text(request, "source"));
    }
    check(!entities.empty(), "InvalidInput", "Transform entities required");
    QSet<QString> selected;
    for (auto id : entities) {
        check(!selected.contains(id), "InvalidInput", "Distinct selected entities required");
        check(!constructionOf(source, id), "UnsupportedOperation",
              "Edit construction groups together");
        source.entity(id);
        selected.insert(id);
    }

    auto candidate = source;
    PointCopies copies{candidate, source, {}};
    QCborArray descendants;
    if (mode == "Move" || mode == "Copy") {
        Vec2 shift{number(request, "delta_x"), number(request, "delta_y")};
        if (mode == "Move") {
            QSet<QString> movedPts;
            for (auto eid : entities) {
                auto &e = candidate.entity(eid);
                if (e.kind == "Point") {
                    if (!movedPts.contains(eid)) {
                        e.point = add(e.point, shift);
                        movedPts.insert(eid);
                    }
                    descendants.append(eid);
                    continue;
                }
                for (auto ptId : {e.a, e.b}) {
                    if (!ptId.isEmpty() && !movedPts.contains(ptId)) {
                        candidate.entity(ptId).point = add(candidate.entity(ptId).point, shift);
                        movedPts.insert(ptId);
                    }
                }
                for (auto ptId : e.poles) {
                    if (!ptId.isEmpty() && !movedPts.contains(ptId)) {
                        candidate.entity(ptId).point = add(candidate.entity(ptId).point, shift);
                        movedPts.insert(ptId);
                    }
                }
                descendants.append(eid);
            }
        } else {
            for (auto eid : entities) {
                auto e = source.entity(eid);
                if (e.kind == "Point") {
                    auto p = add(e.point, shift);
                    descendants.append(copies.point(e.id, p));
                } else if (e.kind == "Line") {
                    auto p1 = add(source.entity(e.a).point, shift);
                    auto p2 = add(source.entity(e.b).point, shift);
                    descendants.append(candidate.line(copies.point(e.a, p1), copies.point(e.b, p2),
                                                      e.construction));
                } else if (e.kind == "Circle") {
                    auto c = add(source.entity(e.a).point, shift);
                    descendants.append(
                        candidate.circle(copies.point(e.a, c), e.radius, e.construction));
                } else if (e.kind == "Arc") {
                    auto c = add(source.entity(e.a).point, shift);
                    descendants.append(candidate.arc(copies.point(e.a, c), e.radius, e.start,
                                                     e.sweep, e.construction));
                } else if (advancedCurve(e.kind)) {
                    auto cCopy = e;
                    cCopy.id = newId();
                    if (e.kind == "Spline") {
                        QMap<QString, QString> copiedPoles;
                        QStringList ctrl;
                        for (auto ptId : e.poles) {
                            if (!copiedPoles.contains(ptId)) {
                                auto &srcPt = source.entity(ptId);
                                copiedPoles[ptId] = copies.point(ptId, add(srcPt.point, shift));
                            }
                            ctrl.push_back(copiedPoles[ptId]);
                        }
                        cCopy.poles = ctrl;
                        cCopy.a = ctrl.empty() ? QString{} : ctrl[0];
                        cCopy.b = QString{};
                    } else {
                        cCopy.a = copies.point(e.a, add(source.entity(e.a).point, shift));
                        cCopy.b = copies.point(e.b, add(source.entity(e.b).point, shift));
                    }
                    candidate.entities.push_back(cCopy);
                    descendants.append(cCopy.id);
                }
            }
        }
    } else if (mode == "Rotate") {
        Vec2 center{number(request, "center_x"), number(request, "center_y")};
        double angle = number(request, "angle");
        bool copy = request.value("copy").toBool();
        if (copy) {
            for (auto eid : entities) {
                auto e = source.entity(eid);
                if (e.kind == "Point") {
                    auto p = rotatePoint(e.point, center, angle);
                    descendants.append(copies.point(e.id, p));
                } else if (e.kind == "Line") {
                    auto p1 = rotatePoint(source.entity(e.a).point, center, angle);
                    auto p2 = rotatePoint(source.entity(e.b).point, center, angle);
                    descendants.append(candidate.line(copies.point(e.a, p1), copies.point(e.b, p2),
                                                      e.construction));
                } else if (e.kind == "Circle") {
                    auto cPt = rotatePoint(source.entity(e.a).point, center, angle);
                    descendants.append(
                        candidate.circle(copies.point(e.a, cPt), e.radius, e.construction));
                } else if (e.kind == "Arc") {
                    auto cPt = rotatePoint(source.entity(e.a).point, center, angle);
                    descendants.append(candidate.arc(copies.point(e.a, cPt), e.radius,
                                                     std::remainder(e.start + angle, tau), e.sweep,
                                                     e.construction));
                } else if (advancedCurve(e.kind)) {
                    auto cCopy = e;
                    cCopy.id = newId();
                    if (e.kind == "Spline") {
                        QMap<QString, QString> copiedPoles;
                        QStringList ctrl;
                        for (auto ptId : e.poles) {
                            if (!copiedPoles.contains(ptId)) {
                                auto &srcPt = source.entity(ptId);
                                copiedPoles[ptId] =
                                    copies.point(ptId, rotatePoint(srcPt.point, center, angle));
                            }
                            ctrl.push_back(copiedPoles[ptId]);
                        }
                        cCopy.poles = ctrl;
                        cCopy.a = ctrl.empty() ? QString{} : ctrl[0];
                        cCopy.b = QString{};
                    } else {
                        cCopy.a =
                            copies.point(e.a, rotatePoint(source.entity(e.a).point, center, angle));
                        cCopy.b =
                            copies.point(e.b, rotatePoint(source.entity(e.b).point, center, angle));
                    }
                    candidate.entities.push_back(cCopy);
                    descendants.append(cCopy.id);
                }
            }
        } else {
            QSet<QString> rotatedPts;
            for (auto eid : entities) {
                auto &e = candidate.entity(eid);
                if (e.kind == "Point") {
                    if (!rotatedPts.contains(eid)) {
                        e.point = rotatePoint(e.point, center, angle);
                        rotatedPts.insert(eid);
                    }
                    descendants.append(eid);
                    continue;
                }
                for (auto ptId : {e.a, e.b}) {
                    if (!ptId.isEmpty() && !rotatedPts.contains(ptId)) {
                        candidate.entity(ptId).point =
                            rotatePoint(candidate.entity(ptId).point, center, angle);
                        rotatedPts.insert(ptId);
                    }
                }
                for (auto ptId : e.poles) {
                    if (!ptId.isEmpty() && !rotatedPts.contains(ptId)) {
                        candidate.entity(ptId).point =
                            rotatePoint(candidate.entity(ptId).point, center, angle);
                        rotatedPts.insert(ptId);
                    }
                }
                if (e.kind == "Arc")
                    e.start = std::remainder(e.start + angle, tau);
                descendants.append(eid);
            }
        }
    } else if (mode == "Scale") {
        Vec2 base{number(request, "base_x"), number(request, "base_y")};
        double factor = number(request, "factor");
        check(factor > 1e-6 && std::isfinite(factor), "InvalidInput",
              "Scale factor must be positive");
        bool copy = request.value("copy").toBool();
        if (copy) {
            for (auto eid : entities) {
                auto e = source.entity(eid);
                if (e.kind == "Point") {
                    auto p = add(base, mul(sub(e.point, base), factor));
                    descendants.append(copies.point(e.id, p));
                } else if (e.kind == "Line") {
                    auto p1 = add(base, mul(sub(source.entity(e.a).point, base), factor));
                    auto p2 = add(base, mul(sub(source.entity(e.b).point, base), factor));
                    descendants.append(candidate.line(copies.point(e.a, p1), copies.point(e.b, p2),
                                                      e.construction));
                } else if (e.kind == "Circle") {
                    auto cPt = add(base, mul(sub(source.entity(e.a).point, base), factor));
                    descendants.append(candidate.circle(copies.point(e.a, cPt), e.radius * factor,
                                                        e.construction));
                } else if (e.kind == "Arc") {
                    auto cPt = add(base, mul(sub(source.entity(e.a).point, base), factor));
                    descendants.append(candidate.arc(copies.point(e.a, cPt), e.radius * factor,
                                                     e.start, e.sweep, e.construction));
                } else if (advancedCurve(e.kind)) {
                    auto cCopy = e;
                    cCopy.id = newId();
                    if (e.kind == "Spline") {
                        QMap<QString, QString> copiedPoles;
                        QStringList ctrl;
                        for (auto ptId : e.poles) {
                            if (!copiedPoles.contains(ptId)) {
                                auto &srcPt = source.entity(ptId);
                                copiedPoles[ptId] = copies.point(
                                    ptId, add(base, mul(sub(srcPt.point, base), factor)));
                            }
                            ctrl.push_back(copiedPoles[ptId]);
                        }
                        cCopy.poles = ctrl;
                        cCopy.a = ctrl.empty() ? QString{} : ctrl[0];
                        cCopy.b = QString{};
                    } else {
                        cCopy.a = copies.point(
                            e.a, add(base, mul(sub(source.entity(e.a).point, base), factor)));
                        cCopy.b = copies.point(
                            e.b, add(base, mul(sub(source.entity(e.b).point, base), factor)));
                    }
                    if (ellipseCurve(e.kind))
                        cCopy.radius *= factor;
                    else if (e.kind == "Parabola") {
                        cCopy.start *= factor;
                        cCopy.sweep *= factor;
                    }
                    candidate.entities.push_back(cCopy);
                    descendants.append(cCopy.id);
                }
            }
        } else {
            QSet<QString> scaledPts;
            for (auto eid : entities) {
                auto &e = candidate.entity(eid);
                if (e.kind == "Point") {
                    if (!scaledPts.contains(eid)) {
                        e.point = add(base, mul(sub(e.point, base), factor));
                        scaledPts.insert(eid);
                    }
                    descendants.append(eid);
                    continue;
                }
                for (auto ptId : {e.a, e.b}) {
                    if (!ptId.isEmpty() && !scaledPts.contains(ptId)) {
                        auto p = candidate.entity(ptId).point;
                        candidate.entity(ptId).point = add(base, mul(sub(p, base), factor));
                        scaledPts.insert(ptId);
                    }
                }
                for (auto ptId : e.poles) {
                    if (!ptId.isEmpty() && !scaledPts.contains(ptId)) {
                        auto p = candidate.entity(ptId).point;
                        candidate.entity(ptId).point = add(base, mul(sub(p, base), factor));
                        scaledPts.insert(ptId);
                    }
                }
                if (e.kind == "Circle" || e.kind == "Arc" || ellipseCurve(e.kind))
                    e.radius *= factor;
                else if (e.kind == "Parabola") {
                    e.start *= factor;
                    e.sweep *= factor;
                }
                descendants.append(eid);
            }
        }
    }

    auto affected = changedGeometry(source, candidate);
    auto [disclosure, removed] = discloseConstraints(candidate, affected);
    candidate.edits.append(fields({{"id", newId()},
                                   {"mode", mode},
                                   {"source", entities.front()},
                                   {"descendants", descendants},
                                   {"removed_constraints", removed}}));
    candidate.validate();
    return fields({{"candidate", candidate.map()}, {"affected_constraints", disclosure}});
}

} // namespace

QCborMap previewLineEdit(const Sketch &source, const QCborMap &request) {
    source.validate();
    auto mode = wire::text(request, "mode");
    check(sketchEditModes().contains(mode), "UnsupportedOperation", "Unsupported sketch edit mode");

    if (lineEditModes().contains(mode))
        return previewStraightLine(source, request);
    if (mode == "Power trim")
        return previewPowerTrim(source, request);
    if (mode == "Corner")
        return previewCorner(source, request);
    if (mode == "Fillet")
        return previewFillet(source, request);
    if (mode == "Chamfer")
        return previewChamfer(source, request);
    if (mode == "Offset entities")
        return previewOffset(source, request);
    if (mode == "Linear pattern" || mode == "Circular pattern")
        return previewPattern(source, request);
    if (mode == "Mirror" || mode == "Dynamic mirror")
        return previewMirror(source, request);
    if (mode == "Move" || mode == "Copy" || mode == "Rotate" || mode == "Scale")
        return previewTransform(source, request);

    check(false, "UnsupportedOperation", "Unimplemented edit mode");
    return {};
}

} // namespace cad::sketch
