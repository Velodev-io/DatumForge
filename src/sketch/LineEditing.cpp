#include "LineEditing.hpp"
#include "Constructions.hpp"
#include "Curves.hpp"
#include <algorithm>
#include <cmath>
namespace cad::sketch {
namespace {
using domain::check;
constexpr double tau = 6.2831853071795864769;
Vec2 sub(Vec2 a, Vec2 b) {
    return {a.x - b.x, a.y - b.y};
}
double dot(Vec2 a, Vec2 b) {
    return a.x * b.x + a.y * b.y;
}
double cross(Vec2 a, Vec2 b) {
    return a.x * b.y - a.y * b.x;
}
double number(const QCborMap &m, QString key) {
    auto v = m.value(key);
    check((v.isDouble() || v.isInteger()) && std::isfinite(v.toDouble()), "InvalidInput",
          "Finite pick coordinate required");
    return v.toDouble();
}
std::vector<double> intersections(const Sketch &s, const Entity &line, const Entity &boundary,
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
    } else {
        check(!apparent && (boundary.kind == "Circle" || boundary.kind == "Arc"),
              "UnsupportedOperation",
              "This edit supports line/circle/arc boundaries; exact conic/spline boundaries remain "
              "open");
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
    }
    return hits;
}
} // namespace
QCborMap previewLineEdit(const Sketch &source, const QCborMap &request) {
    source.validate();
    wire::keys(request, {"mode", "source", "boundaries", "pick_x", "pick_y"});
    auto mode = wire::text(request, "mode"), id = wire::text(request, "source");
    check(lineEditModes().contains(mode), "UnsupportedOperation", "Unsupported line edit mode");
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
        auto roots = intersections(source, e, source.entity(boundary), apparent);
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
    QCborArray disclosure, removed;
    std::erase_if(candidate.constraints, [&](const Constraint &c) {
        bool touches =
            affected.contains(c.a) || affected.contains(c.b) || affected.contains(c.third);
        bool invariant = intervals.size() == 1 && c.a == id && c.b.isEmpty() && c.third.isEmpty() &&
                         (c.kind == "Horizontal" || c.kind == "Vertical");
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
} // namespace cad::sketch
