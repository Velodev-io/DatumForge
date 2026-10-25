#include "Slots.hpp"
#include <algorithm>
namespace cad::sketch {
using domain::check;
namespace {
Vec2 add(Vec2 a, Vec2 b) {
    return {a.x + b.x, a.y + b.y};
}
Vec2 sub(Vec2 a, Vec2 b) {
    return {a.x - b.x, a.y - b.y};
}
Vec2 mul(Vec2 a, double d) {
    return {a.x * d, a.y * d};
}
double angle(Vec2 a, Vec2 b) {
    return std::atan2(b.y - a.y, b.x - a.x);
}
double wrap(double a) {
    return std::remainder(a, arcTau);
}
struct Shape {
    std::vector<Vec2> points;
    std::vector<ArcConstruction> arcs;
};
Shape shape(QString method, SlotParameters p) {
    check(slotMethods().contains(method), "UnsupportedOperation", "Unknown slot method");
    check(std::isfinite(p.width) && p.width > 2e-9 && p.width <= 1e3 && std::isfinite(p.rotation) &&
              std::abs(p.rotation) <= arcTau,
          "InvalidGeometry", "Slot width must exceed 2 nm and its angle must be finite");
    double r = p.width / 2;
    Shape out;
    if (!curvedSlot(method)) {
        check(std::isfinite(p.length) && p.length > 1e-9 && p.length <= 1e3, "InvalidGeometry",
              "Slot center distance must exceed 1 nm");
        Vec2 d{std::cos(p.rotation), std::sin(p.rotation)}, n{-d.y, d.x};
        auto a = method == "Centerpoint Straight" ? sub(p.anchor, mul(d, p.length / 2)) : p.anchor;
        auto b = add(a, mul(d, p.length));
        out.points = {
            a, b, add(a, mul(n, r)), add(b, mul(n, r)), sub(b, mul(n, r)), sub(a, mul(n, r))};
        out.arcs = {{a, r, wrap(p.rotation + arcPi / 2), arcPi},
                    {b, r, wrap(p.rotation - arcPi / 2), arcPi}};
    } else {
        check(std::isfinite(p.radius) && p.radius > r + 1e-9 && p.radius <= 1e3 &&
                  std::isfinite(p.sweep) && p.sweep > 1e-8 && p.sweep < arcTau - 1e-8,
              "InvalidGeometry", "Arc slot requires positive inner radius and a nonfull sweep");
        // For a reflex arc, reject overlapping end disks as well as the inner cusp.
        check(p.sweep <= arcPi || 2 * p.radius * std::sin((arcTau - p.sweep) / 2) > p.width + 1e-9,
              "InvalidGeometry", "Arc slot end caps would overlap");
        auto a = arcPoint(p.anchor, p.radius, p.rotation),
             b = arcPoint(p.anchor, p.radius, p.rotation + p.sweep);
        out.points = {a, b, p.anchor};
        out.arcs = {{p.anchor, p.radius + r, p.rotation, p.sweep},
                    {p.anchor, p.radius - r, p.rotation, p.sweep},
                    {a, r, wrap(p.rotation + arcPi), arcPi},
                    {b, r, wrap(p.rotation + p.sweep), arcPi},
                    {p.anchor, p.radius, p.rotation, p.sweep}};
    }
    for (auto q : out.points)
        check(std::isfinite(q.x) && std::isfinite(q.y) && std::abs(q.x) <= 1e3 &&
                  std::abs(q.y) <= 1e3,
              "InvalidGeometry", "Slot coordinates must be finite and within 1 km");
    return out;
}
Constraint relation(QString kind, QString a, QString b = {}, double value = 0, double second = 0) {
    Constraint c;
    c.kind = kind;
    c.a = a;
    c.b = b;
    c.value = value;
    c.second = second;
    return c;
}
std::vector<Constraint> shapeRelations(const ConstructionGroup &g) {
    auto &e = g.edges;
    if (!curvedSlot(g.method))
        return {relation("ArcTangent", e[2], e[0], 0, 0), relation("ArcTangent", e[2], e[1], 1, 1),
                relation("ArcTangent", e[3], e[1], 0, 0), relation("ArcTangent", e[3], e[0], 1, 1),
                relation("Equal", e[2], e[3])};
    return {relation("ArcTangent", e[3], e[0], 0, 1),
            relation("ArcTangent", e[3], e[1], 1, 1),
            relation("ArcTangent", e[2], e[1], 0, 0),
            relation("ArcTangent", e[2], e[0], 1, 0),
            relation("Equal", e[2], e[3]),
            relation("SlotGeometry", g.diagonals[0], e[2])};
}
} // namespace
QStringList slotMethods() {
    return {"Straight", "Centerpoint Straight", "3 Point Arc", "Centerpoint Arc"};
}
bool curvedSlot(QString method) {
    return method == "3 Point Arc" || method == "Centerpoint Arc";
}
int slotInputCount(QString method) {
    check(slotMethods().contains(method), "UnsupportedOperation", "Unknown slot method");
    return curvedSlot(method) ? 4 : 3;
}
SlotParameters slotFromInput(QString method, const std::vector<Vec2> &p) {
    check(int(p.size()) == slotInputCount(method), "InvalidInput",
          "Complete the slot input sequence");
    SlotParameters out;
    if (!curvedSlot(method)) {
        out.anchor = p[0];
        out.length = length(p[0], p[1]) * (method == "Centerpoint Straight" ? 2 : 1);
        out.rotation = angle(p[0], p[1]);
        auto d = sub(p[1], p[0]);
        check(length(p[0], p[1]) > 1e-9, "InvalidGeometry", "Distinct slot centers required");
        auto q = sub(p[2], p[0]);
        out.width = 2 * std::abs(d.x * q.y - d.y * q.x) / length(p[0], p[1]);
    } else {
        auto a = method == "3 Point Arc" ? threePointArc(p[0], p[1], p[2])
                                         : directedArc(p[0], p[1], p[2], true);
        check(a.has_value(), "InvalidGeometry", "Arc slot centerline is degenerate");
        out.anchor = a->center;
        out.radius = a->radius;
        out.rotation = a->start;
        out.sweep = a->sweep;
        out.width = 2 * std::abs(length(out.anchor, p[3]) - out.radius);
    }
    shape(method, out);
    return out;
}
SlotParameters slotParameters(const Sketch &s, const ConstructionGroup &g) {
    check(g.kind == "Slot", "InvalidInput", "Slot group required");
    SlotParameters out;
    if (curvedSlot(g.method)) {
        auto &path = s.entity(g.diagonals[0]);
        out.anchor = s.entity(g.center).point;
        out.radius = path.radius;
        out.rotation = path.start;
        out.sweep = path.sweep;
        out.width = 2 * s.entity(g.edges[2]).radius;
    } else {
        auto a = s.entity(g.supports[0]).point, b = s.entity(g.supports[1]).point;
        out.anchor = g.method == "Centerpoint Straight" ? mul(add(a, b), .5) : a;
        out.length = length(a, b);
        out.rotation = angle(a, b);
        out.width = 2 * s.entity(g.edges[2]).radius;
    }
    return out;
}
QString createSlot(Sketch &s, QString method, const std::vector<Vec2> &input, QString mode,
                   bool dimensions, bool construction) {
    check(mode == "Center to Center" || mode == "Overall Length", "InvalidInput",
          "Unknown slot length convention");
    auto p = slotFromInput(method, input);
    auto geometry = shape(method, p);
    bool curved = curvedSlot(method);
    check(s.entities.size() + (curved ? 8 : 11) <= 256 &&
              s.constraints.size() + (curved ? 6 : 5) + (dimensions ? (curved ? 3 : 2) : 0) <=
                  512 &&
              s.groups.size() < 32,
          "ResourceLimit", "Slot would exceed existing sketch admission limits");
    auto candidate = s;
    ConstructionGroup g;
    g.kind = "Slot";
    g.method = method;
    g.lengthMode = mode;
    g.dimensions = dimensions;
    g.supports = {candidate.point(geometry.points[0], true),
                  candidate.point(geometry.points[1], true)};
    if (curved) {
        g.center = candidate.point(geometry.points[2], true);
        for (int i = 0; i < 5; ++i) {
            auto &a = geometry.arcs[i];
            auto center = i < 2 || i == 4 ? g.center : g.supports[i - 2];
            auto id = candidate.arc(center, a.radius, a.start, a.sweep, i == 4 || construction);
            if (i == 4)
                g.diagonals.append(id);
            else
                g.edges.append(id);
        }
    } else {
        for (int i = 2; i < 6; ++i)
            g.vertices.append(candidate.point(geometry.points[i], construction));
        g.edges = {candidate.line(g.vertices[0], g.vertices[1], construction),
                   candidate.line(g.vertices[2], g.vertices[3], construction)};
        for (int i = 0; i < 2; ++i) {
            auto &a = geometry.arcs[i];
            g.edges.append(candidate.arc(g.supports[i], a.radius, a.start, a.sweep, construction));
        }
        g.diagonals.append(candidate.line(g.supports[0], g.supports[1], true));
    }
    for (auto c : shapeRelations(g)) {
        g.relations.append(c.id);
        candidate.constraints.push_back(c);
    }
    candidate.groups.push_back(g);
    if (dimensions)
        slotDimensions(candidate, g);
    candidate.validate();
    s = std::move(candidate);
    return g.edges[0];
}
void resizeSlot(Sketch &s, const ConstructionGroup &g, SlotParameters p) {
    auto geometry = shape(g.method, p);
    bool curved = curvedSlot(g.method);
    auto candidate = s;
    for (int i = 0; i < 2; ++i)
        candidate.entity(g.supports[i]).point = geometry.points[i];
    if (curved)
        candidate.entity(g.center).point = p.anchor;
    else
        for (int i = 0; i < 4; ++i)
            candidate.entity(g.vertices[i]).point = geometry.points[i + 2];
    for (int i = 0; i < int(geometry.arcs.size()); ++i) {
        auto id = curved ? (i == 4 ? g.diagonals[0] : g.edges[i]) : g.edges[i + 2];
        auto &e = candidate.entity(id);
        auto &a = geometry.arcs[i];
        e.radius = a.radius;
        e.start = a.start;
        e.sweep = a.sweep;
    }
    candidate.validate();
    s = std::move(candidate);
}
void slotDimensions(Sketch &s, const ConstructionGroup &g) {
    auto p = slotParameters(s, g);
    bool curved = curvedSlot(g.method);
    std::vector<Constraint> values{relation("Diameter", g.edges[2], {}, p.width)};
    if (curved) {
        values.push_back(relation("Radius", g.diagonals[0], {}, p.radius));
        values.push_back(relation("Sweep", g.diagonals[0], {}, p.sweep));
    } else
        values.push_back(
            g.lengthMode == "Overall Length"
                ? relation("SlotLength", g.diagonals[0], g.edges[2], p.length + p.width)
                : relation("Length", g.diagonals[0], {}, p.length));
    for (auto v : values) {
        auto i = std::find_if(s.constraints.begin(), s.constraints.end(), [&](auto &c) {
            return c.kind == v.kind && c.a == v.a && c.b == v.b;
        });
        if (i == s.constraints.end())
            s.constraints.push_back(v);
        else {
            i->value = v.value;
            i->expression.clear();
        }
    }
}
Sketch dragSlot(const Sketch &s, const ConstructionGroup &g, Vec2 delta) {
    auto ids = constructionMembers(g);
    for (auto &c : s.constraints)
        if (!g.relations.contains(c.id) &&
            (ids.contains(c.a) || ids.contains(c.b) || ids.contains(c.third)))
            throw domain::Error("ConstrainedGeometry",
                                "Slot is dimensioned; edit its parameters in Properties");
    auto out = s;
    auto p = slotParameters(s, g);
    p.anchor = add(p.anchor, delta);
    resizeSlot(out, g, p);
    return out;
}
void validateSlot(const Sketch &s, const ConstructionGroup &g, QSet<QString> &owned,
                  QSet<QString> &ownedRelations) {
    slotInputCount(g.method);
    bool curved = curvedSlot(g.method);
    check((g.lengthMode == "Center to Center" || g.lengthMode == "Overall Length") &&
              g.edges.size() == 4 && g.supports.size() == 2 && g.diagonals.size() == 1 &&
              g.vertices.size() == (curved ? 0 : 4) && g.center.isEmpty() != curved,
          "InvalidInput", "Invalid slot roles or length convention");
    for (auto id : constructionMembers(g)) {
        domain::id(id);
        check(!owned.contains(id), "InvalidInput", "Construction member has multiple owners");
        owned.insert(id);
        s.entity(id);
    }
    for (auto id : g.supports)
        check(s.entity(id).kind == "Point" && s.entity(id).construction, "InvalidInput",
              "Slot centers must be construction points");
    if (curved) {
        check(s.entity(g.center).kind == "Point" && s.entity(g.center).construction, "InvalidInput",
              "Slot path center must be a construction point");
        for (int i = 0; i < 4; ++i)
            check(s.entity(g.edges[i]).kind == "Arc" &&
                      s.entity(g.edges[i]).a == (i < 2 ? g.center : g.supports[i - 2]),
                  "InvalidInput", "Invalid curved slot boundary");
        check(s.entity(g.diagonals[0]).kind == "Arc" && s.entity(g.diagonals[0]).a == g.center,
              "InvalidInput", "Invalid slot centerline");
    } else {
        for (auto id : g.vertices)
            check(s.entity(id).kind == "Point", "InvalidInput", "Slot vertex requires a point");
        for (int i = 0; i < 2; ++i) {
            auto &e = s.entity(g.edges[i]);
            check(e.kind == "Line" && e.a == g.vertices[2 * i] && e.b == g.vertices[2 * i + 1],
                  "InvalidInput", "Invalid straight slot boundary");
            auto &a = s.entity(g.edges[i + 2]);
            check(a.kind == "Arc" && a.a == g.supports[i], "InvalidInput", "Invalid slot cap");
        }
        auto &line = s.entity(g.diagonals[0]);
        check(line.kind == "Line" && line.a == g.supports[0] && line.b == g.supports[1],
              "InvalidInput", "Invalid slot centerline");
    }
    check(s.entity(g.diagonals[0]).construction, "InvalidInput",
          "Slot centerline must be construction geometry");
    auto expected = shapeRelations(g);
    check(g.relations.size() == qsizetype(expected.size()), "ReferenceMissing",
          "Slot shape relations are incomplete");
    for (int i = 0; i < g.relations.size(); ++i) {
        check(!ownedRelations.contains(g.relations[i]), "InvalidInput",
              "Construction relation has multiple owners");
        ownedRelations.insert(g.relations[i]);
        auto it = std::find_if(s.constraints.begin(), s.constraints.end(),
                               [&](auto &c) { return c.id == g.relations[i]; });
        check(it != s.constraints.end(), "ReferenceMissing", "Slot relation is missing");
        auto &a = *it;
        auto &b = expected[i];
        check(a.kind == b.kind && a.a == b.a && a.b == b.b && a.value == b.value &&
                  a.second == b.second && a.expression.isEmpty() && a.third.isEmpty(),
              "InvalidInput", "Slot shape relation changed; explicit repair required");
    }
    shape(g.method, slotParameters(s, g));
}
} // namespace cad::sketch
