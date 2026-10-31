#include "Constructions.hpp"
#include "Curves.hpp"
#include "Slots.hpp"
#include <algorithm>
#include <cmath>
#include <tuple>
namespace cad::sketch {
using domain::check;
namespace {
constexpr double pi = 3.14159265358979323846;
Vec2 add(Vec2 a, Vec2 b) {
    return {a.x + b.x, a.y + b.y};
}
Vec2 sub(Vec2 a, Vec2 b) {
    return {a.x - b.x, a.y - b.y};
}
Vec2 mul(Vec2 a, double n) {
    return {a.x * n, a.y * n};
}
double cross(Vec2 a, Vec2 b) {
    return a.x * b.y - a.y * b.x;
}
double dot(Vec2 a, Vec2 b) {
    return a.x * b.x + a.y * b.y;
}
bool centered(QString method) {
    return method == "Center" || method == "3 Point Center";
}
bool aligned(QString method) {
    return method == "Corner" || method == "Center";
}
using Relation = std::tuple<QString, QString, QString>;
std::vector<Relation> shapeRelations(const ConstructionGroup &g) {
    std::vector<Relation> result;
    if (aligned(g.method)) {
        for (int i = 0; i < 4; ++i)
            result.emplace_back(i % 2 ? "Vertical" : "Horizontal", g.edges[i], QString());
    } else {
        result.emplace_back("Parallel", g.edges[0], g.edges[2]);
        result.emplace_back("Parallel", g.edges[1], g.edges[3]);
        if (g.method != "Parallelogram")
            result.emplace_back("Perpendicular", g.edges[0], g.edges[1]);
    }
    if (!g.center.isEmpty())
        result.emplace_back("Midpoint", g.center, g.diagonals[0]);
    return result;
}
const Constraint &constraint(const Sketch &s, QString id) {
    auto i = std::find_if(s.constraints.begin(), s.constraints.end(),
                          [&](auto &c) { return c.id == id; });
    check(i != s.constraints.end(), "ReferenceMissing", "Construction shape relation is missing");
    return *i;
}
QStringList strings(QCborValue v, int count) {
    check(v.isArray() && v.toArray().size() == count, "InvalidInput",
          "Invalid construction role count");
    QStringList out;
    for (auto item : v.toArray()) {
        check(item.isString(), "InvalidInput", "Construction role requires an identity");
        out.append(domain::id(item.toString()));
    }
    return out;
}
bool affected(const Constraint &c, const QSet<QString> &members) {
    return members.contains(c.a) || members.contains(c.b) || members.contains(c.third);
}
QSet<QString> members(const ConstructionGroup &g) {
    auto list = constructionMembers(g);
    return {list.begin(), list.end()};
}
void blocked(const Constraint &c) {
    throw domain::Error("ConstrainedGeometry",
                        "Controlled by " + c.kind +
                            ". Edit or explicitly remove the controlling relation first.");
}
} // namespace
QStringList rectangleMethods() {
    return {"Corner", "Center", "3 Point Corner", "3 Point Center", "Parallelogram"};
}
int rectangleInputCount(QString method) {
    check(rectangleMethods().contains(method), "UnsupportedOperation", "Unknown rectangle method");
    return aligned(method) ? 2 : 3;
}
std::array<Vec2, 4> rectangleVertices(QString method, const std::vector<Vec2> &p) {
    check(int(p.size()) == rectangleInputCount(method), "InvalidInput",
          "Complete the rectangle input sequence");
    Vec2 a = p[0], u, v;
    if (aligned(method)) {
        auto d = sub(p[1], p[0]);
        if (centered(method)) {
            a = sub(p[0], d);
            d = mul(d, 2);
        }
        u = {d.x, 0};
        v = {0, d.y};
    } else if (method == "3 Point Center") {
        auto normal = sub(p[1], p[0]);
        double n = length(normal, {});
        check(n > 1e-9, "InvalidGeometry", "Center and edge midpoint must be distinct");
        auto along = Vec2{-normal.y / n, normal.x / n};
        auto half = mul(along, dot(sub(p[2], p[1]), along));
        a = sub(p[1], half);
        u = mul(half, 2);
        v = mul(normal, -2);
    } else {
        u = sub(p[1], p[0]);
        double n = length(u, {});
        check(n > 1e-9, "InvalidGeometry", "First rectangle edge must have positive length");
        v = sub(p[2], p[1]);
        if (method != "Parallelogram") {
            Vec2 normal{-u.y / n, u.x / n};
            v = mul(normal, dot(v, normal));
        }
    }
    auto w = length(u, {}), h = length(v, {});
    check(w > 1e-9 && h > 1e-9 && std::abs(cross(u, v)) > 1e-8 * w * h, "InvalidGeometry",
          "Rectangle spans must be positive and non-collinear");
    std::array<Vec2, 4> out{a, add(a, u), add(add(a, u), v), add(a, v)};
    for (auto q : out)
        check(std::isfinite(q.x) && std::isfinite(q.y) && std::abs(q.x) <= 1e3 &&
                  std::abs(q.y) <= 1e3,
              "InvalidGeometry", "Rectangle coordinates must be finite and within 1 km");
    return out;
}
QStringList constructionMembers(const ConstructionGroup &g) {
    auto result = g.vertices + g.edges + g.diagonals + g.supports;
    if (!g.center.isEmpty())
        result.append(g.center);
    return result;
}
const ConstructionGroup *constructionOf(const Sketch &s, QString id) {
    for (auto &g : s.groups)
        if (g.id == id || constructionMembers(g).contains(id))
            return &g;
    return nullptr;
}
QCborMap ConstructionGroup::map() const {
    auto result = fields({{"id", id},
                          {"kind", kind},
                          {"method", method},
                          {"vertices", QCborArray::fromStringList(vertices)},
                          {"edges", QCborArray::fromStringList(edges)},
                          {"center", center},
                          {"diagonals", QCborArray::fromStringList(diagonals)},
                          {"relations", QCborArray::fromStringList(relations)}});
    if (kind == "Slot") {
        result.insert(QStringLiteral("supports"), QCborArray::fromStringList(supports));
        result.insert(QStringLiteral("length_mode"), lengthMode);
        result.insert(QStringLiteral("dimensions"), dimensions);
    }
    return result;
}
ConstructionGroup ConstructionGroup::parse(const QCborMap &m) {
    auto kind = wire::text(m, "kind");
    QSet<QString> keys{"id",    "kind",   "method",    "vertices",
                       "edges", "center", "diagonals", "relations"};
    if (kind == "Slot")
        keys.unite({"supports", "length_mode", "dimensions"});
    wire::keys(m, keys);
    check(kind == "Rectangle" || kind == "Slot", "UnsupportedOperation",
          "Unsupported construction group");
    ConstructionGroup g;
    g.kind = kind;
    g.id = domain::id(wire::text(m, "id"));
    g.method = wire::text(m, "method");
    bool slot = kind == "Slot", curved = slot && curvedSlot(g.method);
    if (slot)
        slotInputCount(g.method);
    else
        rectangleInputCount(g.method);
    g.vertices = strings(m.value("vertices"), curved ? 0 : 4);
    g.edges = strings(m.value("edges"), 4);
    check(m.value("center").isString(), "InvalidInput",
          "Explicit construction center role required");
    g.center = m.value("center").toString();
    check((slot ? curved : centered(g.method)) == !g.center.isEmpty(), "InvalidInput",
          "Construction center role disagrees with method");
    if (!g.center.isEmpty())
        domain::id(g.center);
    g.diagonals = strings(m.value("diagonals"), slot ? 1 : centered(g.method) ? 2 : 0);
    g.relations =
        strings(m.value("relations"), slot ? (curved ? 6 : 5) : int(shapeRelations(g).size()));
    if (slot) {
        g.supports = strings(m.value("supports"), 2);
        g.lengthMode = wire::text(m, "length_mode");
        check(m.value("dimensions").isBool(), "InvalidInput", "Slot dimension option required");
        g.dimensions = m.value("dimensions").toBool();
    }
    return g;
}
QString createRectangle(Sketch &s, QString method, const std::vector<Vec2> &inputs,
                        bool construction) {
    auto points = rectangleVertices(method, inputs);
    int count = centered(method) ? 11 : 8, relations = (aligned(method)             ? 4
                                                        : method == "Parallelogram" ? 2
                                                                                    : 3) +
                                                       (centered(method) ? 1 : 0);
    check(s.entities.size() + count <= 256 && s.constraints.size() + relations <= 512 &&
              s.groups.size() < 32,
          "ResourceLimit", "Rectangle would exceed the existing sketch admission limits");
    auto candidate = s;
    ConstructionGroup g;
    g.method = method;
    for (auto p : points)
        g.vertices.append(candidate.point(p, construction));
    for (int i = 0; i < 4; ++i)
        g.edges.append(candidate.line(g.vertices[i], g.vertices[(i + 1) % 4], construction));
    if (centered(method)) {
        g.center = candidate.point(inputs[0], true);
        for (int i = 0; i < 2; ++i)
            g.diagonals.append(candidate.line(g.vertices[i], g.vertices[i + 2], true));
    }
    for (auto &[kind, a, b] : shapeRelations(g)) {
        Constraint c;
        c.kind = kind;
        c.a = a;
        c.b = b;
        g.relations.append(c.id);
        candidate.constraints.push_back(c);
    }
    auto selected = g.edges[0];
    candidate.groups.push_back(g);
    candidate.validate();
    s = std::move(candidate);
    return selected;
}
void validateConstructions(const Sketch &s, QSet<QString> &identities) {
    check(s.groups.size() <= 32, "ResourceLimit", "Too many construction groups");
    QSet<QString> owned, ownedRelations;
    for (auto &g : s.groups) {
        domain::id(g.id);
        check(!identities.contains(g.id), "InvalidInput", "Duplicate construction identity");
        identities.insert(g.id);
        if (g.kind == "Slot") {
            validateSlot(s, g, owned, ownedRelations);
            continue;
        }
        check(g.kind == "Rectangle" && g.supports.empty(), "UnsupportedOperation",
              "Unsupported construction group");
        rectangleInputCount(g.method);
        check(g.vertices.size() == 4 && g.edges.size() == 4 &&
                  centered(g.method) == !g.center.isEmpty() &&
                  g.diagonals.size() == (centered(g.method) ? 2 : 0),
              "InvalidInput", "Invalid rectangle roles");
        auto all = constructionMembers(g);
        for (auto id : all) {
            domain::id(id);
            check(!owned.contains(id), "InvalidInput", "Construction member has multiple owners");
            owned.insert(id);
        }
        for (int i = 0; i < 4; ++i) {
            check(s.entity(g.vertices[i]).kind == "Point", "InvalidInput",
                  "Construction vertex must be a point");
            auto &e = s.entity(g.edges[i]);
            check(e.kind == "Line" && e.a == g.vertices[i] && e.b == g.vertices[(i + 1) % 4],
                  "InvalidInput", "Rectangle edge connectivity is invalid");
        }
        auto a = s.entity(g.vertices[0]).point;
        auto u = sub(s.entity(g.vertices[1]).point, a);
        auto v = sub(s.entity(g.vertices[3]).point, a);
        check(std::abs(cross(u, v)) > 1e-8 * length(u, {}) * length(v, {}), "InvalidGeometry",
              "Construction collapsed to collinear sides");
        if (!g.center.isEmpty()) {
            auto &p = s.entity(g.center);
            check(p.kind == "Point" && p.construction, "InvalidInput",
                  "Rectangle center must be a construction point");
            for (int i = 0; i < 2; ++i) {
                auto &e = s.entity(g.diagonals[i]);
                check(e.kind == "Line" && e.construction && e.a == g.vertices[i] &&
                          e.b == g.vertices[i + 2],
                      "InvalidInput", "Invalid rectangle construction diagonal");
            }
        }
        auto expected = shapeRelations(g);
        check(g.relations.size() == qsizetype(expected.size()), "ReferenceMissing",
              "Rectangle shape relations are incomplete");
        for (int i = 0; i < g.relations.size(); ++i) {
            check(!ownedRelations.contains(g.relations[i]), "InvalidInput",
                  "Construction relation has multiple owners");
            ownedRelations.insert(g.relations[i]);
            auto &c = constraint(s, g.relations[i]);
            auto &[kind, a, b] = expected[i];
            check(c.kind == kind && c.a == a && c.b == b && c.value == 0 && c.second == 0 &&
                      c.expression.isEmpty() && c.third.isEmpty(),
                  "InvalidInput",
                  "Construction shape relation was changed; explicit repair required");
        }
    }
}
RectangleParameters rectangleParameters(const Sketch &s, const ConstructionGroup &g) {
    auto p = s.entity(g.vertices[0]).point, u = sub(s.entity(g.vertices[1]).point, p),
         v = sub(s.entity(g.vertices[3]).point, p);
    return {g.center.isEmpty() ? p : s.entity(g.center).point, length(u, {}), length(v, {}),
            std::atan2(u.y, u.x), std::atan2(cross(u, v), dot(u, v))};
}
void resizeRectangle(Sketch &s, const ConstructionGroup &g, RectangleParameters p) {
    check(std::isfinite(p.width) && std::isfinite(p.side) && p.width > 1e-9 && p.side > 1e-9 &&
              std::isfinite(p.rotation) && std::abs(p.rotation) <= 2 * pi &&
              std::isfinite(p.sideAngle) && std::abs(p.sideAngle) > 1e-8 &&
              std::abs(p.sideAngle) < pi - 1e-8,
          "InvalidGeometry",
          "Positive non-collinear rectangle dimensions and finite angles required");
    if (g.method != "Parallelogram")
        check(std::abs(std::abs(p.sideAngle) - pi / 2) < 1e-8, "InvalidGeometry",
              "Rectangle corners must remain perpendicular");
    if (aligned(g.method))
        check(std::abs(std::sin(p.rotation)) < 1e-8, "InvalidGeometry",
              "This rectangle method remains axis-aligned");
    Vec2 u{p.width * std::cos(p.rotation), p.width * std::sin(p.rotation)},
        v{p.side * std::cos(p.rotation + p.sideAngle), p.side * std::sin(p.rotation + p.sideAngle)};
    auto a = g.center.isEmpty() ? p.anchor : sub(p.anchor, mul(add(u, v), .5));
    std::array<Vec2, 4> points{a, add(a, u), add(add(a, u), v), add(a, v)};
    for (int i = 0; i < 4; ++i)
        s.entity(g.vertices[i]).point = points[i];
    if (!g.center.isEmpty())
        s.entity(g.center).point = p.anchor;
    s.validate();
}
void checkConstructionEdit(const Sketch &before, const Sketch &after, const ConstructionGroup &g) {
    auto ids = members(g);
    for (auto &c : before.constraints)
        if (c.driving && !g.relations.contains(c.id) && affected(c, ids)) {
            if (c.kind == "Length")
                continue;
            if (c.kind == "Fixed" && length(location(before, before.entity(c.a)),
                                            location(after, after.entity(c.a))) < 1e-10)
                continue;
            if (c.kind == "Horizontal" || c.kind == "Vertical") {
                auto &e = after.entity(c.a);
                if (e.kind == "Line") {
                    auto a = after.entity(e.a).point, b = after.entity(e.b).point;
                    if (std::abs(c.kind == "Horizontal" ? a.y - b.y : a.x - b.x) < 1e-10)
                        continue;
                }
            }
            blocked(c);
        }
}
Sketch dragConstruction(const Sketch &s, const ConstructionGroup &g, QString id, Vec2 delta) {
    auto ids = members(g);
    for (auto &c : s.constraints)
        if (c.driving && !g.relations.contains(c.id) && affected(c, ids))
            blocked(c);
    auto result = s;
    auto p = rectangleParameters(s, g);
    int index = int(g.vertices.indexOf(id));
    if (index < 0)
        p.anchor = add(p.anchor, delta);
    else {
        Vec2 u{std::cos(p.rotation), std::sin(p.rotation)},
            v{std::cos(p.rotation + p.sideAngle), std::sin(p.rotation + p.sideAngle)};
        auto corner = add(s.entity(id).point, delta);
        auto d = g.center.isEmpty() ? sub(s.entity(g.vertices[(index + 2) % 4]).point, corner)
                                    : sub(corner, p.anchor);
        double a = cross(d, v) / cross(u, v), b = cross(u, d) / cross(u, v);
        int su = (index == 0 || index == 3) ? 1 : -1, sv = index < 2 ? 1 : -1;
        if (!g.center.isEmpty()) {
            p.width = -2 * a / su;
            p.side = -2 * b / sv;
        } else {
            p.width = a / su;
            p.side = b / sv;
            auto a0 = sub(sub(corner, mul(u, (index == 1 || index == 2) ? p.width : 0)),
                          mul(v, index >= 2 ? p.side : 0));
            p.anchor = a0;
        }
    }
    resizeRectangle(result, g, p);
    return result;
}
Sketch eraseConstruction(const Sketch &s, const ConstructionGroup &g) {
    auto ids = members(g);
    for (auto &c : s.constraints)
        if (!g.relations.contains(c.id) && affected(c, ids))
            blocked(c);
    for (auto &e : s.entities)
        if (!ids.contains(e.id) && (ids.contains(e.a) || ids.contains(e.b)))
            throw domain::Error(
                "ReferenceMissing",
                "Another curve references this construction; repair it before deleting");
    auto result = s;
    std::erase_if(result.entities, [&](auto &e) { return ids.contains(e.id); });
    std::erase_if(result.constraints, [&](auto &c) { return g.relations.contains(c.id); });
    std::erase_if(result.groups, [&](auto &other) { return other.id == g.id; });
    result.validate();
    return result;
}
} // namespace cad::sketch
