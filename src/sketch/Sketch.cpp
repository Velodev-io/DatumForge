#include "Sketch.hpp"
#include <algorithm>
#include <cmath>
namespace cad::sketch {
using domain::check;
namespace {
constexpr double tau = 6.2831853071795864769;
double dot(Vec3 a, Vec3 b) {
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}
void finite(double n) {
    check(std::isfinite(n) && std::abs(n) <= 1e3, "InvalidGeometry",
          "Sketch coordinates must be finite and within 1 km");
}
QCborArray vector(Vec3 v) {
    return {v[0], v[1], v[2]};
}
Vec3 vector(QCborValue value) {
    check(value.isArray() && value.toArray().size() == 3, "InvalidInput",
          "Three coordinates required");
    Vec3 v;
    auto a = value.toArray();
    for (int i = 0; i < 3; ++i) {
        check(a[i].isDouble() || a[i].isInteger(), "InvalidInput", "Numeric coordinate required");
        v[i] = a[i].toDouble();
        finite(v[i]);
    }
    return v;
}
double number(const QCborMap &m, QString key) {
    auto v = m.value(key);
    check(v.isDouble() || v.isInteger(), "InvalidInput", "Numeric value required");
    double n = v.toDouble();
    check(std::isfinite(n), "InvalidInput", "Finite value required");
    return n;
}
} // namespace
QString constraintReason(const Sketch &s, QString kind, QString first, QString second) {
    if (first.isEmpty())
        return "Select an entity";
    auto a = s.entity(first).kind, b = second.isEmpty() ? QString() : s.entity(second).kind;
    bool ok = false;
    if (kind == "Fixed")
        ok = a == "Point" && b.isEmpty();
    if (kind == "Coincident" || kind == "Distance")
        ok = a == "Point" && b == "Point" && first != second;
    if (kind == "Horizontal" || kind == "Vertical" || kind == "Length")
        ok = a == "Line" && b.isEmpty();
    if (kind == "Parallel" || kind == "Perpendicular" || kind == "Angle")
        ok = a == "Line" && b == "Line" && first != second;
    if (kind == "Tangent")
        ok = a == "Line" && b == "Circle";
    if (kind == "Equal")
        ok = ((a == "Line" && b == "Line") || (a == "Circle" && b == "Circle")) && first != second;
    if (kind == "Radius")
        ok = (a == "Circle" || a == "Arc") && b.isEmpty();
    static const QMap<QString, QString> requirements{{"Fixed", "one point"},
                                                     {"Coincident", "two distinct points"},
                                                     {"Distance", "two distinct points"},
                                                     {"Horizontal", "one line"},
                                                     {"Vertical", "one line"},
                                                     {"Length", "one line"},
                                                     {"Parallel", "two distinct lines"},
                                                     {"Perpendicular", "two distinct lines"},
                                                     {"Angle", "two distinct lines"},
                                                     {"Tangent", "a line followed by a circle"},
                                                     {"Equal", "two lines or two circles"},
                                                     {"Radius", "one circle or arc"}};
    return ok ? QString()
              : kind + " requires " + requirements.value(kind, "a supported entity combination");
}
QCborMap fields(std::initializer_list<std::pair<QString, QCborValue>> list) {
    QCborMap m;
    for (auto f : list)
        m.insert(f.first, f.second);
    return m;
}
double length(Vec2 a, Vec2 b) {
    return std::hypot(a.x - b.x, a.y - b.y);
}
void Frame::validate() const {
    for (auto a : {origin, u, v})
        for (double n : a)
            finite(n);
    check(std::abs(dot(u, u) - 1) < 1e-10 && std::abs(dot(v, v) - 1) < 1e-10 &&
              std::abs(dot(u, v)) < 1e-10,
          "InvalidInput", "Sketch plane axes must be orthonormal");
}
Vec3 Frame::world(Vec2 p) const {
    validate();
    Vec3 out = origin;
    for (int i = 0; i < 3; ++i)
        out[i] += u[i] * p.x + v[i] * p.y;
    return out;
}
Vec2 Frame::local(Vec3 p) const {
    validate();
    for (int i = 0; i < 3; ++i)
        p[i] -= origin[i];
    auto out = Vec2{dot(p, u), dot(p, v)};
    Vec3 normal{u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0]};
    check(std::abs(dot(p, normal)) <= 1e-10, "InvalidInput", "Point does not lie on support plane");
    return out;
}
Entity &Sketch::entity(QString id) {
    for (auto &e : entities)
        if (e.id == id)
            return e;
    throw domain::Error("ReferenceMissing", "Sketch entity is missing");
}
const Entity &Sketch::entity(QString id) const {
    for (auto &e : entities)
        if (e.id == id)
            return e;
    throw domain::Error("ReferenceMissing", "Sketch entity is missing");
}
QString Sketch::point(Vec2 p, bool c) {
    Entity e;
    e.kind = "Point";
    e.point = p;
    e.construction = c;
    entities.push_back(e);
    return e.id;
}
QString Sketch::line(QString a, QString b, bool c) {
    Entity e;
    e.kind = "Line";
    e.a = a;
    e.b = b;
    e.construction = c;
    entities.push_back(e);
    return e.id;
}
QString Sketch::circle(QString a, double r, bool c) {
    Entity e;
    e.kind = "Circle";
    e.a = a;
    e.radius = r;
    e.construction = c;
    entities.push_back(e);
    return e.id;
}
QString Sketch::arc(QString a, double r, double start, double sweep, bool c) {
    auto id = circle(a, r, c);
    auto &e = entity(id);
    e.kind = "Arc";
    e.start = start;
    e.sweep = sweep;
    return id;
}
QStringList Sketch::polyline(const std::vector<Vec2> &p, bool closed, bool c) {
    check(p.size() >= 2 && p.size() <= 128, "ResourceLimit", "Polyline requires 2–128 vertices");
    QStringList ids, lines;
    for (auto a : p)
        ids.append(point(a, c));
    for (int i = 1; i < ids.size(); ++i)
        lines.append(line(ids[i - 1], ids[i], c));
    if (closed)
        lines.append(line(ids.last(), ids.first(), c));
    return lines;
}
QStringList Sketch::rectangle(Vec2 a, Vec2 b, bool c) {
    return polyline({a, {b.x, a.y}, b, {a.x, b.y}}, true, c);
}
Vec2 Sketch::snap(Vec2 p, double tolerance, double grid, QString *id) const {
    check(std::isfinite(tolerance) && tolerance >= 0 && std::isfinite(grid) && grid >= 0,
          "InvalidInput", "Invalid snapping tolerance/grid");
    if (id)
        id->clear();
    double best = tolerance;
    const Entity *chosen = nullptr;
    for (auto &e : entities)
        if (e.kind == "Point") {
            double d = length(p, e.point);
            if (d <= best && (!chosen || d < best)) {
                best = d;
                chosen = &e;
            }
        }
    if (chosen) {
        if (id)
            *id = chosen->id;
        return chosen->point;
    }
    if (grid > 0)
        return {std::round(p.x / grid) * grid, std::round(p.y / grid) * grid};
    return p;
}
void Sketch::validate() const {
    domain::id(id);
    domain::label(name);
    frame.validate();
    check(entities.size() <= 256 && constraints.size() <= 512 && names.size() <= 64,
          "ResourceLimit", "Sketch entity/constraint/name limit exceeded");
    QSet<QString> ids{id};
    for (auto &e : entities) {
        domain::id(e.id);
        check(!ids.contains(e.id), "InvalidInput", "Duplicate sketch identity");
        ids.insert(e.id);
    }
    for (auto &e : entities) {
        check(QStringList{"Point", "Line", "Circle", "Arc"}.contains(e.kind),
              "UnsupportedOperation", "Unsupported sketch entity");
        if (e.kind == "Point") {
            finite(e.point.x);
            finite(e.point.y);
            continue;
        }
        auto &a = entity(e.a);
        check(a.kind == "Point", "InvalidInput", "Point reference required");
        if (e.kind == "Line") {
            auto &b = entity(e.b);
            check(b.kind == "Point" && length(a.point, b.point) > 1e-9, "InvalidGeometry",
                  "Line is degenerate (minimum 1 nm)");
        } else {
            finite(e.radius);
            check(e.radius > 1e-9, "InvalidGeometry", "Radius must exceed 1 nm");
        }
        if (e.kind == "Arc")
            check(std::isfinite(e.start) && std::abs(e.start) <= tau && std::isfinite(e.sweep) &&
                      e.sweep > 1e-8 && e.sweep < tau - 1e-8,
                  "InvalidGeometry", "Arc requires a positive sweep below 360 degrees");
    }
    for (auto &c : constraints) {
        domain::id(c.id);
        check(!ids.contains(c.id), "InvalidInput", "Duplicate constraint identity");
        ids.insert(c.id);
        entity(c.a);
        if (!c.b.isEmpty())
            entity(c.b);
        check(std::isfinite(c.value) && std::isfinite(c.second) &&
                  c.expression.toUtf8().size() <= 4096,
              "InvalidInput", "Invalid constraint value/expression");
    }
}
QCborMap Sketch::map() const {
    QCborArray es, cs;
    for (auto &e : entities) {
        auto m = fields({{"id", e.id}, {"kind", e.kind}, {"construction", e.construction}});
        if (e.kind == "Point") {
            m.insert(QStringLiteral("x"), e.point.x);
            m.insert(QStringLiteral("y"), e.point.y);
        } else {
            m.insert(QStringLiteral("a"), e.a);
            if (e.kind == "Line")
                m.insert(QStringLiteral("b"), e.b);
            else {
                m.insert(QStringLiteral("radius"), e.radius);
                if (e.kind == "Arc") {
                    m.insert(QStringLiteral("start"), e.start);
                    m.insert(QStringLiteral("sweep"), e.sweep);
                }
            }
        }
        es.append(m);
    }
    for (auto &c : constraints)
        cs.append(fields({{"id", c.id},
                          {"kind", c.kind},
                          {"a", c.a},
                          {"b", c.b},
                          {"value", c.value},
                          {"second", c.second},
                          {"expression", c.expression}}));
    QCborMap ns;
    for (auto i = names.begin(); i != names.end(); ++i)
        ns.insert(i.key(), i.value());
    return fields({{"version", 1},
                   {"id", id},
                   {"name", name},
                   {"origin", vector(frame.origin)},
                   {"u", vector(frame.u)},
                   {"v", vector(frame.v)},
                   {"entities", es},
                   {"constraints", cs},
                   {"names", ns}});
}
Sketch Sketch::parse(const QCborMap &m) {
    wire::keys(m,
               {"version", "id", "name", "origin", "u", "v", "entities", "constraints", "names"});
    check(wire::integer(m, "version") == 1, "IncompatibleSchema", "Unsupported sketch version");
    Sketch s;
    s.id = wire::text(m, "id");
    s.name = wire::text(m, "name");
    s.frame = {vector(m.value("origin")), vector(m.value("u")), vector(m.value("v"))};
    check(m.value("entities").isArray() && m.value("constraints").isArray() &&
              m.value("names").isMap(),
          "InvalidInput", "Invalid sketch collections");
    check(m.value("entities").toArray().size() <= 256 &&
              m.value("constraints").toArray().size() <= 512 &&
              m.value("names").toMap().size() <= 64,
          "ResourceLimit", "Sketch collection limit exceeded");
    for (auto item : m.value("entities").toArray()) {
        auto p = item.toMap();
        Entity e;
        e.id = wire::text(p, "id");
        e.kind = wire::text(p, "kind");
        check(p.value("construction").isBool(), "InvalidInput", "Construction flag required");
        e.construction = p.value("construction").toBool();
        QSet<QString> keys{"id", "kind", "construction"};
        if (e.kind == "Point") {
            keys.unite({"x", "y"});
            e.point = {number(p, "x"), number(p, "y")};
        } else {
            keys.insert("a");
            e.a = wire::text(p, "a");
            if (e.kind == "Line") {
                keys.insert("b");
                e.b = wire::text(p, "b");
            } else {
                keys.insert("radius");
                e.radius = number(p, "radius");
                if (e.kind == "Arc") {
                    keys.unite({"start", "sweep"});
                    e.start = number(p, "start");
                    e.sweep = number(p, "sweep");
                }
            }
        }
        wire::keys(p, keys);
        s.entities.push_back(e);
    }
    for (auto item : m.value("constraints").toArray()) {
        auto p = item.toMap();
        wire::keys(p, {"id", "kind", "a", "b", "value", "second", "expression"});
        s.constraints.push_back({wire::text(p, "id"), wire::text(p, "kind"), wire::text(p, "a"),
                                 wire::text(p, "b"), wire::text(p, "expression"),
                                 number(p, "value"), number(p, "second")});
    }
    auto ns = m.value("names").toMap();
    for (auto i = ns.begin(); i != ns.end(); ++i) {
        check(i.key().isString() && i.value().isString(), "InvalidInput",
              "Name expressions must be text");
        s.names.insert(i.key().toString(), i.value().toString());
    }
    s.validate();
    return s;
}
QCborArray encode(const std::vector<Sketch> &sketches) {
    QCborArray a;
    for (auto &s : sketches)
        a.append(s.map());
    return a;
}
std::vector<Sketch> parse(const QCborArray &a) {
    check(a.size() <= 16, "ResourceLimit",
          "At most 16 sketches per document in this bounded adapter");
    std::vector<Sketch> out;
    QSet<QString> ids;
    for (auto v : a) {
        auto s = Sketch::parse(v.toMap());
        QStringList all{s.id};
        for (auto &e : s.entities)
            all.append(e.id);
        for (auto &c : s.constraints)
            all.append(c.id);
        for (auto id : all) {
            check(!ids.contains(id), "InvalidInput", "Duplicate sketch identity across document");
            ids.insert(id);
        }
        out.push_back(s);
    }
    check(QCborValue(a).toCbor().size() <= 262144, "ResourceLimit",
          "Document sketch payload exceeds 256 KiB");
    return out;
}
} // namespace cad::sketch
