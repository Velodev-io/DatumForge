#include "DerivedCurve.hpp"
#include <numbers>

namespace cad::model {

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
} // namespace

void BranchSelection::validate() const {
    check(index >= 0 && index <= 1000, "InvalidInput", "Branch index must be non-negative");
    check(!token.isEmpty(), "InvalidInput", "Branch token required");
    check(std::isfinite(startParam) && std::isfinite(endParam), "InvalidInput",
          "Branch parameters must be finite");
    check(startParam >= 0.0 && endParam <= 1.0 && startParam < endParam, "InvalidInput",
          "Branch parameter interval must satisfy 0 <= start < end <= 1");
}

QCborMap BranchSelection::map() const {
    validate();
    QCborMap m;
    m.insert(QStringLiteral("version"), 1);
    m.insert(QStringLiteral("index"), index);
    m.insert(QStringLiteral("token"), token);
    m.insert(QStringLiteral("reversed"), reversed);
    m.insert(QStringLiteral("start_param"), startParam);
    m.insert(QStringLiteral("end_param"), endParam);
    return m;
}

BranchSelection BranchSelection::parse(const QCborMap &m) {
    wire::keys(m, {"version", "index", "token", "reversed", "start_param", "end_param"});
    check(wire::integer(m, "version") == 1, "IncompatibleSchema",
          "Unsupported branch selection schema version");
    BranchSelection b;
    b.index = wire::integer(m, "index", 1000);
    b.token = wire::text(m, "token");
    b.reversed = boolean(m, "reversed");
    b.startParam = number(m, "start_param", 0.0, 1.0);
    b.endParam = number(m, "end_param", 0.0, 1.0);
    b.validate();
    return b;
}

void SketchOnSketchDefinition::validate() const {
    sketch1.validate();
    sketch2.validate();
    check(sketch1.object != sketch2.object, "InvalidInput",
          "Sketch-on-sketch requires two distinct sketches");
    check(sketch1.role == "Object" || sketch1.role == "Curve", "InvalidInput",
          "Sketch 1 reference role must be Object or Curve");
    check(sketch2.role == "Object" || sketch2.role == "Curve", "InvalidInput",
          "Sketch 2 reference role must be Object or Curve");
    branch.validate();
}

QCborMap SketchOnSketchDefinition::map() const {
    validate();
    QCborMap m;
    m.insert(QStringLiteral("version"), 1);
    m.insert(QStringLiteral("sketch1"), sketch1.map());
    m.insert(QStringLiteral("sketch2"), sketch2.map());
    m.insert(QStringLiteral("branch"), branch.map());
    return m;
}

SketchOnSketchDefinition SketchOnSketchDefinition::parse(const QCborMap &m) {
    wire::keys(m, {"version", "sketch1", "sketch2", "branch"});
    check(wire::integer(m, "version") == 1, "IncompatibleSchema",
          "Unsupported sketch-on-sketch schema version");
    SketchOnSketchDefinition d;
    d.sketch1 = domain::GeometryReference::parse(m.value("sketch1").toMap());
    d.sketch2 = domain::GeometryReference::parse(m.value("sketch2").toMap());
    d.branch = BranchSelection::parse(m.value("branch").toMap());
    d.validate();
    return d;
}

void SketchOnFacesDefinition::validate() const {
    sketch.validate();
    check(sketch.role == "Object" || sketch.role == "Curve", "InvalidInput",
          "Source sketch reference role must be Object or Curve");
    check(faces.size() >= 1 && faces.size() <= 32, "ResourceLimit",
          "Sketch-on-faces requires 1 to 32 target faces");
    for (const auto &f : faces) {
        f.validate();
        check(f.role == "Face", "InvalidInput", "Target face reference must have role Face");
    }
    check(mode == "AlongVector" || mode == "NormalToFace", "InvalidInput",
          "Projection mode must be AlongVector or NormalToFace");
    if (mode == "AlongVector") {
        check(direction.has_value(), "InvalidInput",
              "AlongVector projection requires direction reference");
        direction->validate();
        check(direction->role == "Axis" || direction->role == "Edge" || direction->role == "Plane",
              "InvalidInput", "Projection direction must be Axis, Edge, or Plane");
    } else {
        check(!direction.has_value(), "InvalidInput",
              "NormalToFace projection does not accept direction reference");
    }
    branch.validate();
}

QCborMap SketchOnFacesDefinition::map() const {
    validate();
    QCborMap m;
    m.insert(QStringLiteral("version"), 1);
    m.insert(QStringLiteral("sketch"), sketch.map());
    QCborArray faceArr;
    for (const auto &f : faces)
        faceArr.append(f.map());
    m.insert(QStringLiteral("faces"), faceArr);
    m.insert(QStringLiteral("mode"), mode);
    if (direction.has_value())
        m.insert(QStringLiteral("direction"), direction->map());
    m.insert(QStringLiteral("branch"), branch.map());
    return m;
}

SketchOnFacesDefinition SketchOnFacesDefinition::parse(const QCborMap &m) {
    QSet<QString> keys{"version", "sketch", "faces", "mode", "branch"};
    if (m.contains(QStringLiteral("direction")))
        keys.insert(QStringLiteral("direction"));
    wire::keys(m, keys);
    check(wire::integer(m, "version") == 1, "IncompatibleSchema",
          "Unsupported sketch-on-faces schema version");
    SketchOnFacesDefinition d;
    d.sketch = domain::GeometryReference::parse(m.value("sketch").toMap());
    auto fArr = m.value("faces").toArray();
    for (const auto &fVal : fArr)
        d.faces.push_back(domain::GeometryReference::parse(fVal.toMap()));
    d.mode = wire::text(m, "mode");
    if (m.contains(QStringLiteral("direction")))
        d.direction = domain::GeometryReference::parse(m.value("direction").toMap());
    d.branch = BranchSelection::parse(m.value("branch").toMap());
    d.validate();
    return d;
}

void CompositeCurveMember::validate() const {
    source.validate();
    check(source.role == "Curve" || source.role == "Edge", "InvalidInput",
          "Composite curve member must have role Curve or Edge");
}

QCborMap CompositeCurveMember::map() const {
    validate();
    QCborMap m;
    m.insert(QStringLiteral("version"), 1);
    m.insert(QStringLiteral("source"), source.map());
    m.insert(QStringLiteral("reversed"), reversed);
    return m;
}

CompositeCurveMember CompositeCurveMember::parse(const QCborMap &m) {
    wire::keys(m, {"version", "source", "reversed"});
    check(wire::integer(m, "version") == 1, "IncompatibleSchema",
          "Unsupported composite curve member schema version");
    CompositeCurveMember member;
    member.source = domain::GeometryReference::parse(m.value("source").toMap());
    member.reversed = boolean(m, "reversed");
    member.validate();
    return member;
}

void CompositeCurveDefinition::validate() const {
    check(members.size() >= 1 && members.size() <= 32, "ResourceLimit",
          "Composite curve requires 1 to 32 members");
    for (const auto &m : members)
        m.validate();
}

QCborMap CompositeCurveDefinition::map() const {
    validate();
    QCborMap m;
    m.insert(QStringLiteral("version"), 1);
    QCborArray memArr;
    for (const auto &m : members)
        memArr.append(m.map());
    m.insert(QStringLiteral("members"), memArr);
    m.insert(QStringLiteral("closed"), closed);
    return m;
}

CompositeCurveDefinition CompositeCurveDefinition::parse(const QCborMap &m) {
    wire::keys(m, {"version", "members", "closed"});
    check(wire::integer(m, "version") == 1, "IncompatibleSchema",
          "Unsupported composite curve definition schema version");
    CompositeCurveDefinition d;
    auto memArr = m.value("members").toArray();
    for (const auto &mVal : memArr)
        d.members.push_back(CompositeCurveMember::parse(mVal.toMap()));
    d.closed = boolean(m, "closed");
    d.validate();
    return d;
}

void HelixSpiralDefinition::validate() const {
    check(QStringList{"PitchAndRevolution", "HeightAndRevolution", "HeightAndPitch", "Spiral"}
              .contains(mode),
          "InvalidInput", "Invalid helix/spiral mode: " + mode);
    axis.validate();
    check(axis.role == "Axis" || axis.role == "Edge" || axis.role == "Plane", "InvalidInput",
          "Helix/spiral axis must have role Axis, Edge, or Plane");
    if (center.has_value()) {
        center->validate();
        check(center->role == "Point" || center->role == "Vertex", "InvalidInput",
              "Helix/spiral center must have role Point or Vertex");
    }
    check(std::isfinite(radius) && radius > 0 && radius <= 1000, "InvalidInput",
          "Helix/spiral radius must be positive and bounded");
    check(std::isfinite(startAngle) && startAngle >= 0 && startAngle < 2 * std::numbers::pi,
          "InvalidInput", "Helix/spiral start angle must be in [0, 2*pi)");

    if (mode == "PitchAndRevolution") {
        check(std::isfinite(pitch) && pitch > 0 && pitch <= 1000, "InvalidInput",
              "Pitch must be positive and bounded");
        check(std::isfinite(revolutions) && revolutions > 0 && revolutions <= 1000, "InvalidInput",
              "Revolutions must be positive and bounded");
        check(std::isfinite(taperAngle) && taperAngle >= 0 && taperAngle < std::numbers::pi / 2,
              "InvalidInput", "Taper angle must be in [0, pi/2)");
    } else if (mode == "HeightAndRevolution") {
        check(std::isfinite(height) && height > 0 && height <= 1000, "InvalidInput",
              "Height must be positive and bounded");
        check(std::isfinite(revolutions) && revolutions > 0 && revolutions <= 1000, "InvalidInput",
              "Revolutions must be positive and bounded");
        check(std::isfinite(taperAngle) && taperAngle >= 0 && taperAngle < std::numbers::pi / 2,
              "InvalidInput", "Taper angle must be in [0, pi/2)");
    } else if (mode == "HeightAndPitch") {
        check(std::isfinite(height) && height > 0 && height <= 1000, "InvalidInput",
              "Height must be positive and bounded");
        check(std::isfinite(pitch) && pitch > 0 && pitch <= 1000, "InvalidInput",
              "Pitch must be positive and bounded");
        check(std::isfinite(taperAngle) && taperAngle >= 0 && taperAngle < std::numbers::pi / 2,
              "InvalidInput", "Taper angle must be in [0, pi/2)");
    } else if (mode == "Spiral") {
        check(std::isfinite(pitch) && pitch > 0 && pitch <= 1000, "InvalidInput",
              "Pitch must be positive and bounded");
        check(std::isfinite(revolutions) && revolutions > 0 && revolutions <= 1000, "InvalidInput",
              "Revolutions must be positive and bounded");
        check(height == 0, "InvalidInput", "Planar spiral does not support height");
        check(taperAngle == 0, "InvalidInput", "Planar spiral does not support taper angle");
    }
}

QCborMap HelixSpiralDefinition::map() const {
    validate();
    QCborMap m;
    m.insert(QStringLiteral("version"), 1);
    m.insert(QStringLiteral("mode"), mode);
    m.insert(QStringLiteral("axis"), axis.map());
    if (center.has_value())
        m.insert(QStringLiteral("center"), center->map());
    m.insert(QStringLiteral("radius"), radius);
    m.insert(QStringLiteral("start_angle"), startAngle);
    m.insert(QStringLiteral("clockwise"), clockwise);

    if (mode == "PitchAndRevolution") {
        m.insert(QStringLiteral("pitch"), pitch);
        m.insert(QStringLiteral("revolutions"), revolutions);
        m.insert(QStringLiteral("taper_angle"), taperAngle);
    } else if (mode == "HeightAndRevolution") {
        m.insert(QStringLiteral("height"), height);
        m.insert(QStringLiteral("revolutions"), revolutions);
        m.insert(QStringLiteral("taper_angle"), taperAngle);
    } else if (mode == "HeightAndPitch") {
        m.insert(QStringLiteral("height"), height);
        m.insert(QStringLiteral("pitch"), pitch);
        m.insert(QStringLiteral("taper_angle"), taperAngle);
    } else if (mode == "Spiral") {
        m.insert(QStringLiteral("pitch"), pitch);
        m.insert(QStringLiteral("revolutions"), revolutions);
    }
    return m;
}

HelixSpiralDefinition HelixSpiralDefinition::parse(const QCborMap &m) {
    QSet<QString> keys{"version", "mode", "axis", "radius", "start_angle", "clockwise"};
    for (auto k : {"center", "pitch", "height", "revolutions", "taper_angle"})
        if (m.contains(QString::fromLatin1(k)))
            keys.insert(QString::fromLatin1(k));
    wire::keys(m, keys);
    check(wire::integer(m, "version") == 1, "IncompatibleSchema",
          "Unsupported helix/spiral schema version");
    HelixSpiralDefinition d;
    d.mode = wire::text(m, "mode");
    d.axis = domain::GeometryReference::parse(m.value("axis").toMap());
    if (m.contains(QStringLiteral("center")))
        d.center = domain::GeometryReference::parse(m.value("center").toMap());
    d.radius = number(m, "radius", 1e-6, 1000);
    d.startAngle = number(m, "start_angle", 0, 2 * std::numbers::pi);
    d.clockwise = boolean(m, "clockwise");

    if (m.contains(QStringLiteral("pitch")))
        d.pitch = number(m, "pitch", 1e-6, 1000);
    if (m.contains(QStringLiteral("height")))
        d.height = number(m, "height", 1e-6, 1000);
    if (m.contains(QStringLiteral("revolutions")))
        d.revolutions = number(m, "revolutions", 1e-6, 1000);
    if (m.contains(QStringLiteral("taper_angle")))
        d.taperAngle = number(m, "taper_angle", 0, std::numbers::pi / 2);

    d.validate();
    return d;
}

CurveFrame CurveFrame::evaluate(sketch::Vec3 pt, sketch::Vec3 tgt, bool reversed,
                                std::optional<sketch::Vec3> preferredUp) {
    double tNorm = sketch::norm(tgt);
    check(tNorm >= 1e-12, "InvalidInput", "Zero derivative at curve evaluation parameter");

    sketch::Vec3 T = sketch::normalize(tgt);
    if (reversed) {
        T = {-T[0], -T[1], -T[2]};
    }

    sketch::Vec3 up{0, 0, 1};
    if (std::abs(sketch::dot(T, up)) > 0.999) {
        up = {0, 1, 0};
    }

    if (preferredUp.has_value()) {
        sketch::Vec3 pUp = *preferredUp;
        double proj = sketch::dot(T, pUp);
        sketch::Vec3 inPlane{pUp[0] - proj * T[0], pUp[1] - proj * T[1], pUp[2] - proj * T[2]};
        if (sketch::norm(inPlane) > 1e-6) {
            up = sketch::normalize(inPlane);
        }
    }

    double proj = sketch::dot(up, T);
    sketch::Vec3 nVec{up[0] - proj * T[0], up[1] - proj * T[1], up[2] - proj * T[2]};
    sketch::Vec3 N = sketch::normalize(nVec);
    sketch::Vec3 B = sketch::cross(T, N);

    return {pt, T, N, B};
}

void DerivedCurve::validate() const {
    domain::id(id);
    check(version == 1, "IncompatibleSchema", "Unsupported derived curve schema version");
    check(QStringList{"SketchOnSketch", "SketchOnFaces", "CompositeCurve", "HelixSpiral"}.contains(
              kind),
          "InvalidInput", "Invalid derived curve kind: " + kind);

    int count = 0;
    if (kind == "SketchOnSketch") {
        check(sketchOnSketch.has_value(), "InvalidInput", "SketchOnSketch definition missing");
        sketchOnSketch->validate();
        count++;
    } else if (kind == "SketchOnFaces") {
        check(sketchOnFaces.has_value(), "InvalidInput", "SketchOnFaces definition missing");
        sketchOnFaces->validate();
        count++;
    } else if (kind == "CompositeCurve") {
        check(compositeCurve.has_value(), "InvalidInput", "CompositeCurve definition missing");
        compositeCurve->validate();
        count++;
    } else if (kind == "HelixSpiral") {
        check(helixSpiral.has_value(), "InvalidInput", "HelixSpiral definition missing");
        helixSpiral->validate();
        count++;
    }
    check(count == 1, "InvalidInput", "Exactly one derived curve definition variant must be set");
}

QCborMap DerivedCurve::map() const {
    validate();
    QCborMap m;
    m.insert(QStringLiteral("version"), 1);
    m.insert(QStringLiteral("id"), id);
    m.insert(QStringLiteral("name"), name);
    m.insert(QStringLiteral("kind"), kind);
    if (kind == "SketchOnSketch")
        m.insert(QStringLiteral("definition"), sketchOnSketch->map());
    else if (kind == "SketchOnFaces")
        m.insert(QStringLiteral("definition"), sketchOnFaces->map());
    else if (kind == "CompositeCurve")
        m.insert(QStringLiteral("definition"), compositeCurve->map());
    else if (kind == "HelixSpiral")
        m.insert(QStringLiteral("definition"), helixSpiral->map());
    return m;
}

DerivedCurve DerivedCurve::parse(const QCborMap &m) {
    wire::keys(m, {"version", "id", "name", "kind", "definition"});
    check(wire::integer(m, "version") == 1, "IncompatibleSchema",
          "Unsupported derived curve schema version");
    DerivedCurve c;
    c.id = wire::text(m, "id");
    c.name = wire::text(m, "name");
    c.kind = wire::text(m, "kind");
    auto defMap = m.value("definition").toMap();
    if (c.kind == "SketchOnSketch")
        c.sketchOnSketch = SketchOnSketchDefinition::parse(defMap);
    else if (c.kind == "SketchOnFaces")
        c.sketchOnFaces = SketchOnFacesDefinition::parse(defMap);
    else if (c.kind == "CompositeCurve")
        c.compositeCurve = CompositeCurveDefinition::parse(defMap);
    else if (c.kind == "HelixSpiral")
        c.helixSpiral = HelixSpiralDefinition::parse(defMap);
    c.validate();
    return c;
}

std::vector<domain::GeometryReference> DerivedCurve::references() const {
    std::vector<domain::GeometryReference> refs;
    if (kind == "SketchOnSketch" && sketchOnSketch.has_value()) {
        refs.push_back(sketchOnSketch->sketch1);
        refs.push_back(sketchOnSketch->sketch2);
    } else if (kind == "SketchOnFaces" && sketchOnFaces.has_value()) {
        refs.push_back(sketchOnFaces->sketch);
        for (const auto &f : sketchOnFaces->faces)
            refs.push_back(f);
        if (sketchOnFaces->direction.has_value())
            refs.push_back(*sketchOnFaces->direction);
    } else if (kind == "CompositeCurve" && compositeCurve.has_value()) {
        for (const auto &m : compositeCurve->members)
            refs.push_back(m.source);
    } else if (kind == "HelixSpiral" && helixSpiral.has_value()) {
        refs.push_back(helixSpiral->axis);
        if (helixSpiral->center.has_value())
            refs.push_back(*helixSpiral->center);
    }
    return refs;
}

evaluation::GraphNode DerivedCurve::graphNode() const {
    validate();
    evaluation::GraphNode node;
    node.id = id;
    node.kind = QStringLiteral("DerivedCurve");
    node.references = references();
    return node;
}

QCborArray encodeDerivedCurves(const std::vector<DerivedCurve> &curves) {
    QCborArray arr;
    for (const auto &c : curves)
        arr.append(c.map());
    return arr;
}

std::vector<DerivedCurve> parseDerivedCurves(const QCborArray &curves) {
    std::vector<DerivedCurve> result;
    for (const auto &v : curves)
        result.push_back(DerivedCurve::parse(v.toMap()));
    return result;
}

} // namespace cad::model
