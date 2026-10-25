#include "Sketch.hpp"
#include "Constructions.hpp"
#include "Curves.hpp"
#include "LineEditing.hpp"
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
void CurveReference::validate() const {
    domain::check(document == "Self", "UnsupportedOperation",
                  "Pierce currently references this document");
    domain::id(sketch);
    domain::id(entity);
    domain::check(QStringList{"Point", "Line", "Circle", "Arc", "Ellipse", "EllipticalArc",
                              "Parabola", "Spline"}
                      .contains(role),
                  "InvalidInput", "Typed curve role required");
    domain::check(std::isfinite(lower) && std::isfinite(upper) && std::isfinite(parameter) &&
                      std::abs(lower) <= 1e6 && std::abs(upper) <= 1e6 && lower < upper &&
                      parameter >= lower && parameter <= upper,
                  "InvalidInput", "Bounded Pierce parameter interval required");
}
QCborMap CurveReference::map() const {
    return fields({{"document", document},
                   {"sketch", sketch},
                   {"entity", entity},
                   {"role", role},
                   {"lower", lower},
                   {"upper", upper},
                   {"parameter", parameter}});
}
CurveReference CurveReference::parse(const QCborMap &m) {
    wire::keys(m, {"document", "sketch", "entity", "role", "lower", "upper", "parameter"});
    CurveReference r{wire::text(m, "document"), wire::text(m, "sketch"), wire::text(m, "entity"),
                     wire::text(m, "role"),     number(m, "lower"),      number(m, "upper"),
                     number(m, "parameter")};
    r.validate();
    return r;
}
QString constraintReason(const Sketch &s, QString kind, QString first, QString second,
                         QString third) {
    if (first.isEmpty())
        return "Select an entity";
    auto a = s.entity(first).kind, b = second.isEmpty() ? QString() : s.entity(second).kind;
    bool ok = false;
    if (kind == "Pierce")
        ok = a == "Point" && b.isEmpty() && third.isEmpty();
    if (kind == "Fixed")
        ok = b.isEmpty();
    if (kind == "Distance")
        ok = a == "Point" && b == "Point" && first != second;
    if (kind == "Merge")
        ok = a == "Point" && b == "Point" && first != second;
    if (kind == "Intersection")
        ok = a == "Point" && b == "Line" && !third.isEmpty() && s.entity(third).kind == "Line" &&
             second != third;
    if (kind == "Symmetric")
        ok = QStringList{"Point", "Line", "Arc", "Ellipse"}.contains(a) && a == b &&
             first != second && !third.isEmpty() && s.entity(third).kind == "Line" &&
             s.entity(third).construction;
    if (kind == "CurveTangent")
        ok = a != "Point" && b != "Point" && !b.isEmpty() && first != second &&
             (advancedCurve(a) || advancedCurve(b));
    if (kind == "Coincident")
        ok = a == "Point" &&
             (QStringList{"Point", "Line", "Circle", "Arc"}.contains(b) || advancedCurve(b)) &&
             first != second;
    if (kind == "Horizontal" || kind == "Vertical")
        ok = (a == "Line" && b.isEmpty()) || (a == "Point" && b == "Point" && first != second);
    if (kind == "Length")
        ok = a == "Line" && b.isEmpty();
    if (kind == "Midpoint")
        ok = a == "Point" && b == "Line";
    if (kind == "Collinear")
        ok = a == "Line" && b == "Line" && first != second;
    if (kind == "Concentric")
        ok = first != second && ((QStringList{"Circle", "Arc"}.contains(a) &&
                                  QStringList{"Circle", "Arc", "Point"}.contains(b)) ||
                                 (a == "Point" && QStringList{"Circle", "Arc"}.contains(b)));
    if (kind == "Coradial")
        ok = QStringList{"Circle", "Arc"}.contains(a) && QStringList{"Circle", "Arc"}.contains(b) &&
             first != second;
    if (kind == "Parallel" || kind == "Perpendicular" || kind == "Angle")
        ok = a == "Line" && b == "Line" && first != second;
    if (kind == "Tangent")
        ok = (a == "Line" && (QStringList{"Circle", "Arc"}.contains(b) || ellipseCurve(b))) ||
             (QStringList{"Circle", "Arc"}.contains(a) &&
              QStringList{"Circle", "Arc"}.contains(b) && first != second);
    if (kind == "ArcTangent" || kind == "ArcNormal" || kind == "ArcCoincident")
        ok = a == "Arc" && first != second &&
             (b == "Line" || b == "Arc" || (kind == "ArcCoincident" && b == "Point"));
    if (kind == "Equal")
        ok = ((a == "Line" && b == "Line") || (QStringList{"Circle", "Arc"}.contains(a) &&
                                               QStringList{"Circle", "Arc"}.contains(b))) &&
             first != second;
    if (kind == "Radius" || kind == "Diameter")
        ok = (a == "Circle" || a == "Arc") && b.isEmpty();
    if (kind == "Sweep")
        ok = a == "Arc" && b.isEmpty();
    if (kind == "SlotLength")
        ok = a == "Line" && b == "Arc";
    if (kind == "SlotGeometry")
        ok = a == "Arc" && b == "Arc";
    static const QMap<QString, QString> requirements{
        {"Pierce", "one point and a typed curve reference in another sketch"},
        {"Fixed", "one geometry entity"},
        {"Merge", "two distinct points"},
        {"Intersection", "a point and two distinct lines"},
        {"Symmetric", "two same-type points/lines/arcs/ellipses and a construction centerline"},
        {"CurveTangent", "two distinct curves including an exact conic or spline"},
        {"Coincident", "a point followed by a point, line, circular or exact conic/spline curve"},
        {"Distance", "two distinct points"},
        {"Horizontal", "one line or two distinct points"},
        {"Vertical", "one line or two distinct points"},
        {"Length", "one line"},
        {"Parallel", "two distinct lines"},
        {"Perpendicular", "two distinct lines"},
        {"Angle", "two distinct lines"},
        {"Tangent", "a line and circular/elliptical curve, or two circular curves"},
        {"Midpoint", "a point followed by a line"},
        {"Collinear", "two distinct lines"},
        {"Concentric", "two circles/arcs"},
        {"Coradial", "two circles/arcs"},
        {"Equal", "two lines or two circles/arcs"},
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
        check(advancedCurve(e.kind) ||
                  QStringList{"Point", "Line", "Circle", "Arc"}.contains(e.kind),
              "UnsupportedOperation", "Unsupported sketch entity");
        if (e.kind == "Point") {
            finite(e.point.x);
            finite(e.point.y);
            continue;
        }
        if (advancedCurve(e.kind)) {
            validateCurve(*this, e);
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
        check((c.kind == "Pierce") == c.reference.has_value(), "InvalidInput",
              "Pierce requires its typed reference; other relations cannot carry one");
        if (c.reference) {
            c.reference->validate();
            check(c.reference->sketch != id, "DependencyCycle",
                  "Pierce source must be another sketch");
        }
        entity(c.a);
        if (!c.b.isEmpty())
            entity(c.b);
        if (!c.third.isEmpty())
            entity(c.third);
        check(std::isfinite(c.value) && std::isfinite(c.second) &&
                  c.expression.toUtf8().size() <= 4096,
              "InvalidInput", "Invalid constraint value/expression");
    }
    validateConstructions(*this, ids);
    validateEditHistory(*this, ids);
}
bool Sketch::extendedRelations() const {
    for (const auto &c : constraints) {
        if (QStringList{"Midpoint", "Collinear", "Concentric", "Coradial"}.contains(c.kind))
            return true;
        if ((c.kind == "Horizontal" || c.kind == "Vertical") && !c.b.isEmpty())
            return true;
        if (c.kind == "Coincident" && !c.b.isEmpty() && entity(c.b).kind != "Point")
            return true;
        if (c.kind == "Equal" && (entity(c.a).kind == "Arc" || entity(c.b).kind == "Arc"))
            return true;
    }
    return false;
}
bool Sketch::primitiveSemantics() const {
    for (auto &c : constraints)
        if ((c.kind == "Coincident" && entity(c.b).kind == "Arc") ||
            (c.kind == "Concentric" &&
             (entity(c.a).kind == "Point" || entity(c.b).kind == "Point")))
            return true;
    for (auto &c : constraints)
        if (!c.third.isEmpty() ||
            QStringList{"Merge", "Intersection", "Symmetric", "CurveTangent", "Pierce"}.contains(
                c.kind) ||
            (c.kind == "Fixed" && entity(c.a).kind != "Point") ||
            (c.kind == "Tangent" && (entity(c.a).kind != "Line" || entity(c.b).kind != "Circle")))
            return true;
    for (auto &e : entities)
        if (advancedCurve(e.kind))
            return true;
    for (auto &g : groups)
        if (g.kind != "Rectangle")
            return true;
    for (auto &c : constraints)
        if (QStringList{"SlotGeometry", "SlotLength", "Sweep", "Diameter"}.contains(c.kind))
            return true;
    return false;
}
QCborMap Sketch::map() const {
    QCborArray es, cs;
    for (auto &e : entities) {
        auto m = fields({{"id", e.id}, {"kind", e.kind}, {"construction", e.construction}});
        if (e.kind == "Point") {
            m.insert(QStringLiteral("x"), e.point.x);
            m.insert(QStringLiteral("y"), e.point.y);
        } else if (e.kind == "Spline") {
            m.insert(QStringLiteral("poles"), QCborArray::fromStringList(e.poles));
            QCborArray weights, knots, mult;
            for (auto v : e.weights)
                weights.append(v);
            for (auto v : e.knots)
                knots.append(v);
            for (auto v : e.multiplicities)
                mult.append(v);
            m.insert(QStringLiteral("weights"), weights);
            m.insert(QStringLiteral("knots"), knots);
            m.insert(QStringLiteral("multiplicities"), mult);
            m.insert(QStringLiteral("degree"), e.degree);
            m.insert(QStringLiteral("start"), e.start);
            m.insert(QStringLiteral("sweep"), e.sweep);
        } else if (advancedCurve(e.kind)) {
            m.insert(QStringLiteral("a"), e.a);
            m.insert(QStringLiteral("b"), e.b);
            if (ellipseCurve(e.kind))
                m.insert(QStringLiteral("minor_radius"), e.radius);
            if (e.kind != "Ellipse") {
                m.insert(QStringLiteral("start"), e.start);
                m.insert(QStringLiteral("sweep"), e.sweep);
            }
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
    for (auto &c : constraints) {
        auto record = fields({{"id", c.id},
                              {"kind", c.kind},
                              {"a", c.a},
                              {"b", c.b},
                              {"value", c.value},
                              {"second", c.second},
                              {"expression", c.expression}});
        if (c.reference)
            record.insert(QStringLiteral("reference"), c.reference->map());
        if (c.kind == "CurveTangent")
            record.insert(QStringLiteral("reversed"), c.reversed);
        if (!c.third.isEmpty())
            record.insert(QStringLiteral("third"), c.third);
        cs.append(record);
    }
    QCborMap ns;
    for (auto i = names.begin(); i != names.end(); ++i)
        ns.insert(i.key(), i.value());
    auto result = fields({{"version", !edits.empty()         ? 5
                                      : primitiveSemantics() ? 4
                                      : !groups.empty()      ? 3
                                      : extendedRelations()  ? 2
                                                             : 1},
                          {"id", id},
                          {"name", name},
                          {"origin", vector(frame.origin)},
                          {"u", vector(frame.u)},
                          {"v", vector(frame.v)},
                          {"entities", es},
                          {"constraints", cs},
                          {"names", ns}});
    if (!groups.empty() || primitiveSemantics() || !edits.empty()) {
        QCborArray gs;
        for (auto &g : groups)
            gs.append(g.map());
        result.insert(QStringLiteral("groups"), gs);
    }
    if (!edits.empty())
        result.insert(QStringLiteral("edits"), edits);
    return result;
}
Sketch Sketch::parse(const QCborMap &m) {
    auto version = wire::integer(m, "version");
    check(version >= 1 && version <= 5, "IncompatibleSchema", "Unsupported sketch version");
    QSet<QString> keys{"version", "id",       "name",        "origin", "u",
                       "v",       "entities", "constraints", "names"};
    if (version >= 3)
        keys.insert("groups");
    if (version >= 5)
        keys.insert("edits");
    wire::keys(m, keys);
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
        } else if (e.kind == "Spline") {
            keys.unite({"poles", "weights", "knots", "multiplicities", "degree", "start", "sweep"});
            auto degree = wire::integer(p, "degree");
            check(degree >= 1 && degree <= 5, "InvalidInput", "Spline degree must be 1-5");
            e.degree = int(degree);
            auto ids = p.value("poles"), weights = p.value("weights"), knots = p.value("knots"),
                 mult = p.value("multiplicities");
            check(ids.isArray() && ids.toArray().size() >= 2 && ids.toArray().size() <= 64 &&
                      weights.isArray() && weights.toArray().size() <= 64 && knots.isArray() &&
                      knots.toArray().size() <= 64 && mult.isArray() && mult.toArray().size() <= 64,
                  "ResourceLimit", "Bounded spline collections required");
            for (auto id : ids.toArray()) {
                check(id.isString(), "InvalidInput", "Spline pole identity required");
                e.poles.append(id.toString());
            }
            for (auto v : weights.toArray()) {
                check(v.isDouble() || v.isInteger(), "InvalidInput",
                      "Numeric spline weight required");
                e.weights.push_back(v.toDouble());
            }
            for (auto v : knots.toArray()) {
                check(v.isDouble() || v.isInteger(), "InvalidInput",
                      "Numeric spline knot required");
                e.knots.push_back(v.toDouble());
            }
            for (auto v : mult.toArray()) {
                check(v.isInteger() && v.toInteger() >= 1 && v.toInteger() <= 6, "InvalidInput",
                      "Integer knot multiplicity required");
                e.multiplicities.push_back(int(v.toInteger()));
            }
            e.a = e.poles[0];
            e.start = number(p, "start");
            e.sweep = number(p, "sweep");
        } else if (advancedCurve(e.kind)) {
            keys.unite({"a", "b"});
            e.a = wire::text(p, "a");
            e.b = wire::text(p, "b");
            if (ellipseCurve(e.kind)) {
                keys.insert("minor_radius");
                e.radius = number(p, "minor_radius");
            }
            if (e.kind != "Ellipse") {
                keys.unite({"start", "sweep"});
                e.start = number(p, "start");
                e.sweep = number(p, "sweep");
            }
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
        QSet<QString> keys{"id", "kind", "a", "b", "value", "second", "expression"};
        if (version >= 4 && p.contains(QStringLiteral("third")))
            keys.insert("third");
        if (version >= 4 && wire::text(p, "kind") == "CurveTangent")
            keys.insert("reversed");
        if (version >= 4 && wire::text(p, "kind") == "Pierce")
            keys.insert("reference");
        wire::keys(p, keys);
        s.constraints.push_back({wire::text(p, "id"), wire::text(p, "kind"), wire::text(p, "a"),
                                 wire::text(p, "b"), wire::text(p, "expression"),
                                 number(p, "value"), number(p, "second")});
        if (s.constraints.back().kind == "Pierce") {
            check(p.value("reference").isMap(), "InvalidInput", "Pierce reference required");
            s.constraints.back().reference = CurveReference::parse(p.value("reference").toMap());
        }
        if (s.constraints.back().kind == "CurveTangent") {
            check(p.value("reversed").isBool(), "InvalidInput",
                  "Curve tangent orientation required");
            s.constraints.back().reversed = p.value("reversed").toBool();
        }
        if (p.contains(QStringLiteral("third"))) {
            s.constraints.back().third = wire::text(p, "third");
            check(!s.constraints.back().third.isEmpty(), "InvalidInput",
                  "Explicit third operand cannot be empty");
        }
    }
    auto ns = m.value("names").toMap();
    for (auto i = ns.begin(); i != ns.end(); ++i) {
        check(i.key().isString() && i.value().isString(), "InvalidInput",
              "Name expressions must be text");
        s.names.insert(i.key().toString(), i.value().toString());
    }
    if (version >= 3) {
        check(m.value("groups").isArray() &&
                  (version >= 4 || !m.value("groups").toArray().empty()) &&
                  m.value("groups").toArray().size() <= 32,
              "InvalidInput", "Version 3 requires bounded construction groups");
        for (auto g : m.value("groups").toArray())
            s.groups.push_back(ConstructionGroup::parse(g.toMap()));
    }
    if (version >= 5) {
        check(m.value("edits").isArray() && !m.value("edits").toArray().empty(), "InvalidInput",
              "Version 5 requires edit provenance");
        s.edits = m.value("edits").toArray();
    }
    s.validate();
    check(version >= 4 || !s.primitiveSemantics(), "IncompatibleSchema",
          "New primitives require sketch version 4");
    check(version >= 2 || !s.extendedRelations(), "IncompatibleSchema",
          "Extended relation operands require sketch version 2");
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
        for (auto &g : s.groups)
            all.append(g.id);
        for (auto e : s.edits)
            all.append(e.toMap().value("id").toString());
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
