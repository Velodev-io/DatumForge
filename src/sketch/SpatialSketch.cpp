#include "SpatialSketch.hpp"
#include <numbers>

namespace cad::sketch {

using domain::check;

namespace {
double number(const QCborMap &m, QString key, double low, double high) {
    auto v = m.value(key);
    check(v.isDouble() || v.isInteger(), "InvalidInput", "Numeric parameter required for: " + key);
    auto n = v.toDouble();
    check(std::isfinite(n) && n >= low && n <= high, "InvalidInput",
          "Parameter outside finite bounds: " + key);
    return n;
}

bool boolean(const QCborMap &m, const QString &key) {
    auto v = m.value(key);
    check(v.isBool(), "InvalidInput", "Boolean required for: " + key);
    return v.toBool();
}

Vec3 parseVec3(const QCborArray &arr) {
    check(arr.size() == 3, "InvalidInput", "Vec3 array must have length 3");
    Vec3 result;
    for (int i = 0; i < 3; ++i) {
        auto v = arr.at(i);
        check(v.isDouble() || v.isInteger(), "InvalidInput", "Vec3 coordinates must be numbers");
        result[i] = v.toDouble();
        check(std::isfinite(result[i]), "InvalidInput", "Vec3 coordinates must be finite");
    }
    return result;
}

QCborArray encodeVec3(const Vec3 &v) {
    QCborArray arr;
    arr.append(v[0]);
    arr.append(v[1]);
    arr.append(v[2]);
    return arr;
}
} // namespace

void SpatialEntity::validate() const {
    domain::id(id);
    check(QStringList{"Point", "Line", "Arc", "Spline"}.contains(kind), "InvalidInput",
          "Invalid spatial entity kind: " + kind);
    if (kind == "Point") {
        check(std::isfinite(point[0]) && std::isfinite(point[1]) && std::isfinite(point[2]),
              "InvalidInput", "Point coordinates must be finite");
        check(std::abs(point[0]) <= 1000 && std::abs(point[1]) <= 1000 &&
                  std::abs(point[2]) <= 1000,
              "InvalidInput", "Point coordinates out of bounds");
        check(a.isEmpty() && b.isEmpty() && center.isEmpty() && poles.isEmpty(), "InvalidInput",
              "Point entity must not define endpoints or poles");
    } else if (kind == "Line") {
        domain::id(a);
        domain::id(b);
        check(a != b, "InvalidInput", "Line start and end points must differ");
        check(center.isEmpty() && poles.isEmpty(), "InvalidInput",
              "Line entity must not define center or poles");
    } else if (kind == "Arc") {
        domain::id(center);
        domain::id(a);
        domain::id(b);
        check(a != b && a != center && b != center, "InvalidInput",
              "Arc endpoints and center must all differ");
        check(std::isfinite(radius) && radius > 0 && radius <= 1000, "InvalidInput",
              "Arc radius must be positive and bounded");
        check(std::abs(norm(normal) - 1.0) < 1e-4, "InvalidInput",
              "Arc normal must be a unit vector");
        check(poles.isEmpty(), "InvalidInput", "Arc entity must not define poles");
    } else if (kind == "Spline") {
        check(poles.size() >= 2 && poles.size() <= 64, "InvalidInput",
              "Spline requires 2 to 64 control poles");
        for (const auto &p : poles)
            domain::id(p);
        check(degree >= 1 && degree <= 7 && degree < poles.size(), "InvalidInput",
              "Invalid spline degree");
        check(a.isEmpty() && b.isEmpty() && center.isEmpty(), "InvalidInput",
              "Spline entity must not define line/arc endpoints");
        if (!weights.empty()) {
            check(weights.size() == size_t(poles.size()), "InvalidInput",
                  "Spline weights count must match poles count");
            for (double w : weights)
                check(std::isfinite(w) && w > 0, "InvalidInput", "Spline weights must be positive");
        }
        if (!knots.empty()) {
            for (size_t i = 1; i < knots.size(); ++i)
                check(knots[i] >= knots[i - 1], "InvalidInput",
                      "Spline knots must be non-decreasing");
        }
    }
}

QCborMap SpatialEntity::map() const {
    validate();
    QCborMap m;
    m.insert(QStringLiteral("version"), 1);
    m.insert(QStringLiteral("id"), id);
    m.insert(QStringLiteral("kind"), kind);
    m.insert(QStringLiteral("construction"), construction);
    if (kind == "Point") {
        m.insert(QStringLiteral("point"), encodeVec3(point));
    } else if (kind == "Line") {
        m.insert(QStringLiteral("a"), a);
        m.insert(QStringLiteral("b"), b);
    } else if (kind == "Arc") {
        m.insert(QStringLiteral("center"), center);
        m.insert(QStringLiteral("a"), a);
        m.insert(QStringLiteral("b"), b);
        m.insert(QStringLiteral("radius"), radius);
        m.insert(QStringLiteral("normal"), encodeVec3(normal));
    } else if (kind == "Spline") {
        QCborArray pArr;
        for (const auto &p : poles)
            pArr.append(p);
        m.insert(QStringLiteral("poles"), pArr);
        m.insert(QStringLiteral("degree"), degree);
        if (!weights.empty()) {
            QCborArray wArr;
            for (double w : weights)
                wArr.append(w);
            m.insert(QStringLiteral("weights"), wArr);
        }
        if (!knots.empty()) {
            QCborArray kArr;
            for (double k : knots)
                kArr.append(k);
            m.insert(QStringLiteral("knots"), kArr);
        }
    }
    return m;
}

SpatialEntity SpatialEntity::parse(const QCborMap &m) {
    QSet<QString> keys{"version", "id", "kind", "construction"};
    for (auto k :
         {"point", "a", "b", "center", "radius", "normal", "poles", "degree", "weights", "knots"})
        if (m.contains(QString::fromLatin1(k)))
            keys.insert(QString::fromLatin1(k));
    wire::keys(m, keys);
    check(wire::integer(m, "version") == 1, "IncompatibleSchema",
          "Unsupported spatial entity schema version");
    SpatialEntity e;
    e.id = wire::text(m, "id");
    e.kind = wire::text(m, "kind");
    e.construction = boolean(m, "construction");
    if (e.kind == "Point") {
        check(m.contains(QStringLiteral("point")), "InvalidInput",
              "Point entity requires point coordinates");
        e.point = parseVec3(m.value("point").toArray());
    } else if (e.kind == "Line") {
        e.a = wire::text(m, "a");
        e.b = wire::text(m, "b");
    } else if (e.kind == "Arc") {
        e.center = wire::text(m, "center");
        e.a = wire::text(m, "a");
        e.b = wire::text(m, "b");
        e.radius = number(m, "radius", 1e-6, 1000);
        check(m.contains(QStringLiteral("normal")), "InvalidInput", "Arc entity requires normal");
        e.normal = parseVec3(m.value("normal").toArray());
    } else if (e.kind == "Spline") {
        check(m.contains(QStringLiteral("poles")), "InvalidInput", "Spline requires poles");
        auto pArr = m.value("poles").toArray();
        for (const auto &p : pArr)
            e.poles.append(p.toString());
        e.degree = wire::integer(m, "degree", 7);
        if (m.contains(QStringLiteral("weights"))) {
            auto wArr = m.value("weights").toArray();
            for (const auto &w : wArr)
                e.weights.push_back(w.toDouble());
        }
        if (m.contains(QStringLiteral("knots"))) {
            auto kArr = m.value("knots").toArray();
            for (const auto &k : kArr)
                e.knots.push_back(k.toDouble());
        }
    }
    e.validate();
    return e;
}

void SpatialConstraint::validate() const {
    domain::id(id);
    check(QStringList{"Coincident", "ParallelYZ", "ParallelZX", "AlongZ", "AlongX", "AlongY",
                      "OnPlane", "Distance", "Angle", "Perpendicular", "Parallel", "Tangent",
                      "EqualLength", "Fixed"}
              .contains(kind),
          "InvalidInput", "Invalid spatial constraint kind: " + kind);
    domain::id(a);
    if (plane.has_value()) {
        plane->validate();
        check(plane->role == "Plane" || plane->role == "Face", "InvalidInput",
              "Plane constraint reference must have Plane or Face role");
    }
    if (kind == "ParallelYZ" || kind == "ParallelZX" || kind == "AlongZ") {
        check(b.isEmpty(), "InvalidInput",
              "Plane-relative direction constraint operates on a single line");
        check(plane.has_value(), "InvalidInput",
              "Plane-relative direction constraint requires a plane reference");
    } else if (kind == "OnPlane") {
        check(b.isEmpty(), "InvalidInput", "OnPlane constraint operates on a single entity");
        check(plane.has_value(), "InvalidInput", "OnPlane constraint requires a plane reference");
    } else if (kind == "Distance") {
        check(!b.isEmpty() || plane.has_value(), "InvalidInput",
              "Distance requires second entity or plane reference");
        if (!b.isEmpty())
            domain::id(b);
        check(std::isfinite(value) && value > 0 && value <= 1000, "InvalidInput",
              "Distance value must be positive and bounded");
    } else if (kind == "Angle") {
        check(!b.isEmpty() || plane.has_value(), "InvalidInput",
              "Angle requires second entity or plane reference");
        if (!b.isEmpty())
            domain::id(b);
        check(std::isfinite(value) && value > 0 && value < std::numbers::pi, "InvalidInput",
              "Angle value must be in (0, pi)");
    } else if (kind == "Fixed") {
        check(b.isEmpty() && !plane.has_value(), "InvalidInput",
              "Fixed constraint operates on a single point");
    } else if (kind == "AlongX" || kind == "AlongY") {
        check(b.isEmpty(), "InvalidInput", "Along axis constraint operates on a single line");
    } else {
        domain::id(b);
        check(a != b, "InvalidInput", "Constraint entities must differ");
    }
}

QCborMap SpatialConstraint::map() const {
    validate();
    QCborMap m;
    m.insert(QStringLiteral("version"), 1);
    m.insert(QStringLiteral("id"), id);
    m.insert(QStringLiteral("kind"), kind);
    m.insert(QStringLiteral("a"), a);
    if (!b.isEmpty())
        m.insert(QStringLiteral("b"), b);
    if (!third.isEmpty())
        m.insert(QStringLiteral("third"), third);
    if (plane.has_value())
        m.insert(QStringLiteral("plane"), plane->map());
    if (kind == "Distance" || kind == "Angle") {
        m.insert(QStringLiteral("value"), value);
        m.insert(QStringLiteral("driving"), driving);
    }
    if (reversed)
        m.insert(QStringLiteral("reversed"), reversed);
    return m;
}

SpatialConstraint SpatialConstraint::parse(const QCborMap &m) {
    QSet<QString> keys{"version", "id", "kind", "a"};
    for (auto k : {"b", "third", "plane", "value", "driving", "reversed"})
        if (m.contains(QString::fromLatin1(k)))
            keys.insert(QString::fromLatin1(k));
    wire::keys(m, keys);
    check(wire::integer(m, "version") == 1, "IncompatibleSchema",
          "Unsupported spatial constraint schema version");
    SpatialConstraint c;
    c.id = wire::text(m, "id");
    c.kind = wire::text(m, "kind");
    c.a = wire::text(m, "a");
    if (m.contains(QStringLiteral("b")))
        c.b = wire::text(m, "b");
    if (m.contains(QStringLiteral("third")))
        c.third = wire::text(m, "third");
    if (m.contains(QStringLiteral("plane")))
        c.plane = domain::GeometryReference::parse(m.value("plane").toMap());
    if (m.contains(QStringLiteral("value"))) {
        auto val = m.value("value");
        check(val.isDouble() || val.isInteger(), "InvalidInput", "Numeric value required");
        c.value = val.toDouble();
    }
    if (m.contains(QStringLiteral("driving")))
        c.driving = boolean(m, "driving");
    if (m.contains(QStringLiteral("reversed")))
        c.reversed = boolean(m, "reversed");
    c.validate();
    return c;
}

void SpatialSketch::validate() const {
    domain::id(id);
    check(version == 1, "IncompatibleSchema", "Unsupported spatial sketch schema version");
    check(QStringList{"XY", "YZ", "ZX"}.contains(activePlane), "InvalidInput",
          "Active plane must be XY, YZ, or ZX");
    check(entities.size() <= 64 && constraints.size() <= 64, "ResourceLimit",
          "Spatial sketch exceeds admission limits (max 64 entities, max 64 constraints)");

    QMap<QString, QString> entityKinds;
    for (const auto &e : entities) {
        e.validate();
        check(!entityKinds.contains(e.id), "InvalidInput", "Duplicate entity ID: " + e.id);
        entityKinds.insert(e.id, e.kind);
    }

    for (const auto &e : entities) {
        if (e.kind == "Line") {
            check(entityKinds.contains(e.a) && entityKinds.value(e.a) == "Point", "InvalidInput",
                  "Line start point missing or not Point");
            check(entityKinds.contains(e.b) && entityKinds.value(e.b) == "Point", "InvalidInput",
                  "Line end point missing or not Point");
        } else if (e.kind == "Arc") {
            check(entityKinds.contains(e.center) && entityKinds.value(e.center) == "Point",
                  "InvalidInput", "Arc center missing or not Point");
            check(entityKinds.contains(e.a) && entityKinds.value(e.a) == "Point", "InvalidInput",
                  "Arc start point missing or not Point");
            check(entityKinds.contains(e.b) && entityKinds.value(e.b) == "Point", "InvalidInput",
                  "Arc end point missing or not Point");
        } else if (e.kind == "Spline") {
            for (const auto &p : e.poles)
                check(entityKinds.contains(p) && entityKinds.value(p) == "Point", "InvalidInput",
                      "Spline pole missing or not Point");
        }
    }

    QSet<QString> constraintIds;
    for (const auto &c : constraints) {
        c.validate();
        check(!constraintIds.contains(c.id), "InvalidInput", "Duplicate constraint ID: " + c.id);
        constraintIds.insert(c.id);

        check(entityKinds.contains(c.a), "InvalidInput", "Constraint entity a missing: " + c.a);
        if (!c.b.isEmpty())
            check(entityKinds.contains(c.b), "InvalidInput", "Constraint entity b missing: " + c.b);

        if (c.kind == "ParallelYZ" || c.kind == "ParallelZX" || c.kind == "AlongZ" ||
            c.kind == "AlongX" || c.kind == "AlongY") {
            check(entityKinds.value(c.a) == "Line", "InvalidInput",
                  "Plane/axis direction constraint requires Line entity");
        } else if (c.kind == "Fixed") {
            check(entityKinds.value(c.a) == "Point", "InvalidInput",
                  "Fixed constraint requires Point entity");
        }
    }
}

QCborMap SpatialSketch::map() const {
    validate();
    QCborMap m;
    m.insert(QStringLiteral("version"), 1);
    m.insert(QStringLiteral("id"), id);
    m.insert(QStringLiteral("name"), name);
    m.insert(QStringLiteral("active_plane"), activePlane);
    QCborArray entArr;
    for (const auto &e : entities)
        entArr.append(e.map());
    m.insert(QStringLiteral("entities"), entArr);
    QCborArray conArr;
    for (const auto &c : constraints)
        conArr.append(c.map());
    m.insert(QStringLiteral("constraints"), conArr);
    return m;
}

SpatialSketch SpatialSketch::parse(const QCborMap &m) {
    wire::keys(m, {"version", "id", "name", "active_plane", "entities", "constraints"});
    check(wire::integer(m, "version") == 1, "IncompatibleSchema",
          "Unsupported spatial sketch schema version");
    SpatialSketch s;
    s.id = wire::text(m, "id");
    s.name = wire::text(m, "name");
    s.activePlane = wire::text(m, "active_plane");
    auto entArr = m.value("entities").toArray();
    for (const auto &eVal : entArr)
        s.entities.push_back(SpatialEntity::parse(eVal.toMap()));
    auto conArr = m.value("constraints").toArray();
    for (const auto &cVal : conArr)
        s.constraints.push_back(SpatialConstraint::parse(cVal.toMap()));
    s.validate();
    return s;
}

std::vector<domain::GeometryReference> SpatialSketch::references() const {
    std::vector<domain::GeometryReference> refs;
    for (const auto &c : constraints) {
        if (c.plane.has_value())
            refs.push_back(*c.plane);
    }
    return refs;
}

QMap<QString, QString> SpatialSketch::members() const {
    QMap<QString, QString> mems;
    for (const auto &e : entities) {
        if (e.kind == "Point")
            mems.insert(e.id, QStringLiteral("Point"));
        else
            mems.insert(e.id, QStringLiteral("Curve"));
    }
    return mems;
}

evaluation::GraphNode SpatialSketch::graphNode() const {
    validate();
    evaluation::GraphNode node;
    node.id = id;
    node.kind = QStringLiteral("SpatialSketch");
    node.references = references();
    node.members = members();
    return node;
}

SpatialEntity &SpatialSketch::entity(const QString &entityId) {
    for (auto &e : entities)
        if (e.id == entityId)
            return e;
    throw domain::Error("ReferenceMissing", "Spatial entity not found: " + entityId);
}

const SpatialEntity &SpatialSketch::entity(const QString &entityId) const {
    for (const auto &e : entities)
        if (e.id == entityId)
            return e;
    throw domain::Error("ReferenceMissing", "Spatial entity not found: " + entityId);
}

QString SpatialSketch::point(Vec3 p, bool construction) {
    SpatialEntity e;
    e.kind = QStringLiteral("Point");
    e.point = p;
    e.construction = construction;
    entities.push_back(e);
    return e.id;
}

QString SpatialSketch::line(const QString &startPt, const QString &endPt, bool construction) {
    SpatialEntity e;
    e.kind = QStringLiteral("Line");
    e.a = startPt;
    e.b = endPt;
    e.construction = construction;
    entities.push_back(e);
    return e.id;
}

QString SpatialSketch::arc(const QString &centerPt, const QString &startPt, const QString &endPt,
                           const Vec3 &normVec, double r, bool construction) {
    SpatialEntity e;
    e.kind = QStringLiteral("Arc");
    e.center = centerPt;
    e.a = startPt;
    e.b = endPt;
    e.normal = normalize(normVec);
    e.radius = r;
    e.construction = construction;
    entities.push_back(e);
    return e.id;
}

QCborArray encodeSpatial(const std::vector<SpatialSketch> &sketches) {
    QCborArray arr;
    for (const auto &s : sketches)
        arr.append(s.map());
    return arr;
}

std::vector<SpatialSketch> parseSpatial(const QCborArray &sketches) {
    std::vector<SpatialSketch> result;
    for (const auto &v : sketches)
        result.push_back(SpatialSketch::parse(v.toMap()));
    return result;
}

} // namespace cad::sketch
