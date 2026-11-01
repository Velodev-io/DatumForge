#include "Supports.hpp"
#include "evaluation/Plan.hpp"
#include "sketch/Dependencies.hpp"
#include <numbers>
namespace cad::model {
using domain::check;
using sketch::fields;
namespace {
double number(const QCborMap &m, QString key, double low, double high) {
    auto v = m.value(key);
    check(v.isDouble() || v.isInteger(), "InvalidInput", "Numeric support parameter required");
    auto n = v.toDouble();
    check(std::isfinite(n) && n >= low && n <= high, "InvalidInput",
          "Support parameter outside finite bounds: " + key);
    return n;
}
bool plane(const domain::GeometryReference &r) {
    return r.role == "Plane" || r.role == "Face";
}
bool curve(const domain::GeometryReference &r) {
    return r.role == "Curve" || r.role == "Edge";
}
bool line(const domain::GeometryReference &r) {
    return curve(r) || r.role == "Axis";
}
bool point(const domain::GeometryReference &r) {
    return r.role == "Point" || r.role == "Vertex" ||
           (curve(r) && (!r.endpoint.isEmpty() || r.parameter));
}
} // namespace
void Datum::validate() const {
    domain::id(id);
    check(references.size() <= 3, "ResourceLimit", "At most three datum references");
    for (auto &r : references)
        r.validate();
    check(kind == "DatumPlane" || kind == "DatumAxis", "InvalidInput", "Typed datum kind required");
    auto count = references.size();
    bool valid = false;
    if (mode == "Principal") {
        auto key = kind == "DatumPlane" ? QString("plane") : QString("axis");
        wire::keys(parameters, {key});
        auto choices =
            kind == "DatumPlane" ? QStringList{"XY", "XZ", "YZ"} : QStringList{"X", "Y", "Z"};
        valid = count == 0 && choices.contains(wire::text(parameters, key));
    } else if (kind == "DatumPlane") {
        if (mode == "Offset") {
            wire::keys(parameters, {"distance", "count"});
            auto distance = number(parameters, "distance", -1000, 1000);
            auto n = wire::integer(parameters, "count", 32);
            check(n >= 1 && (n == 1 || std::abs(distance) > 1e-9), "InvalidInput",
                  "Multiple planes require a nonzero spacing");
            valid = count == 1 && plane(references[0]);
        } else if (mode == "Angle") {
            wire::keys(parameters, {"angle"});
            number(parameters, "angle", -2 * std::numbers::pi, 2 * std::numbers::pi);
            valid = count == 2 && plane(references[0]) && line(references[1]);
        } else if (mode == "TangentSurface") {
            wire::keys(parameters, {"u_fraction", "v_fraction"});
            number(parameters, "u_fraction", 0, 1);
            number(parameters, "v_fraction", 0, 1);
            valid = count == 1 && references[0].role == "Face";
        } else if (mode == "Midplane") {
            wire::keys(parameters, {"bisector"});
            check(QStringList{"", "Internal", "External"}.contains(
                      wire::text(parameters, "bisector")),
                  "InvalidInput", "Choose the plane bisector explicitly");
            valid = count == 2 && plane(references[0]) && plane(references[1]);
        } else {
            wire::keys(parameters, {});
            valid =
                (mode == "ParallelPoint" && count == 2 && plane(references[0]) &&
                 point(references[1])) ||
                (mode == "ThreePoints" && count == 3 && point(references[0]) &&
                 point(references[1]) && point(references[2])) ||
                (mode == "LinePoint" && count == 2 && line(references[0]) &&
                 point(references[1])) ||
                (mode == "TwoLines" && count == 2 && line(references[0]) && line(references[1])) ||
                (mode == "NormalCurve" && count == 1 && curve(references[0]) &&
                 point(references[0]));
        }
    } else {
        wire::keys(parameters, {});
        valid =
            (mode == "FaceAxis" && count == 1 && references[0].role == "Face") ||
            (mode == "Line" && count == 1 && line(references[0])) ||
            (mode == "TwoPlanes" && count == 2 && plane(references[0]) && plane(references[1])) ||
            (mode == "TwoPoints" && count == 2 && point(references[0]) && point(references[1])) ||
            (mode == "PointNormal" && count == 2 && point(references[0]) && plane(references[1]));
    }
    check(valid, "InvalidInput", "Datum mode requires matching typed reference operands");
}
QStringList Datum::outputs() const {
    validate();
    QStringList result{id};
    if (kind == "DatumPlane" && mode == "Offset")
        for (qint64 i = 1; i < parameters.value("count").toInteger(); ++i)
            result.append(QUuid::createUuidV5(QUuid(id), "offset/" + QByteArray::number(i))
                              .toString(QUuid::WithoutBraces));
    return result;
}
QCborMap Datum::map() const {
    validate();
    QCborArray refs;
    for (auto &r : references)
        refs.append(r.map());
    return fields({{"version", 1},
                   {"id", id},
                   {"kind", kind},
                   {"mode", mode},
                   {"references", refs},
                   {"parameters", parameters},
                   {"reversed", reversed}});
}
Datum Datum::parse(const QCborMap &m) {
    wire::keys(m, {"version", "id", "kind", "mode", "references", "parameters", "reversed"});
    check(wire::integer(m, "version") == 1, "IncompatibleSchema", "Unsupported datum version");
    check(m.value("references").isArray() && m.value("references").toArray().size() <= 3 &&
              m.value("parameters").isMap() && m.value("reversed").isBool(),
          "InvalidInput", "Typed datum fields required");
    Datum d;
    d.id = wire::text(m, "id");
    d.kind = wire::text(m, "kind");
    d.mode = wire::text(m, "mode");
    for (auto v : m.value("references").toArray()) {
        check(v.isMap(), "InvalidInput", "Typed geometry reference required");
        d.references.push_back(domain::GeometryReference::parse(v.toMap()));
    }
    d.parameters = m.value("parameters").toMap();
    d.reversed = m.value("reversed").toBool();
    d.validate();
    return d;
}
QCborMap Attachment::map() const {
    domain::id(sketch);
    reference.validate();
    check(plane(reference) && std::isfinite(offset) && std::abs(offset) <= 1000 &&
              std::isfinite(angle) && std::abs(angle) <= 2 * std::numbers::pi,
          "InvalidInput", "Attachment requires a plane/face and finite SI offset/angle");
    return fields({{"version", 1},
                   {"sketch", sketch},
                   {"reference", reference.map()},
                   {"offset", offset},
                   {"angle", angle},
                   {"reversed", reversed}});
}
Attachment Attachment::parse(const QCborMap &m) {
    wire::keys(m, {"version", "sketch", "reference", "offset", "angle", "reversed"});
    check(wire::integer(m, "version") == 1, "IncompatibleSchema", "Unsupported attachment version");
    check(m.value("reference").isMap() && m.value("reversed").isBool(), "InvalidInput",
          "Typed attachment fields required");
    Attachment a;
    a.sketch = wire::text(m, "sketch");
    a.reference = domain::GeometryReference::parse(m.value("reference").toMap());
    a.offset = number(m, "offset", -1000, 1000);
    a.angle = number(m, "angle", -2 * std::numbers::pi, 2 * std::numbers::pi);
    a.reversed = m.value("reversed").toBool();
    a.map();
    return a;
}
QCborMap Conversion::map() const {
    domain::id(id);
    domain::id(sketch);
    check(!sources.empty() && sources.size() <= 32, "ResourceLimit",
          "Convert selects 1–32 sources");
    check(std::isfinite(scale) && scale >= 1e-6 && scale <= 1e6 && std::isfinite(angle) &&
              std::abs(angle) <= 2 * std::numbers::pi,
          "InvalidInput", "Finite positive conversion scale and radian angle required");
    for (auto value : {center.x, center.y, translation.x, translation.y})
        check(std::isfinite(value) && std::abs(value) <= 1000, "InvalidInput",
              "Finite SI transform required");
    check(members.size() <= 256, "ResourceLimit",
          "Generated member declaration admission exceeded");
    QCborMap declared;
    for (auto it = members.cbegin(); it != members.cend(); ++it) {
        domain::id(it.key());
        check(it.value() == "Point" || it.value() == "Curve", "InvalidInput",
              "Typed generated member declaration required");
        declared.insert(it.key(), it.value());
    }
    QCborArray refs;
    for (auto &r : sources) {
        r.validate();
        check(curve(r) || r.role == "Point" || r.role == "Vertex" || r.role == "Face",
              "InvalidInput", "Convert requires an exact point/curve/edge/face");
        refs.append(r.map());
    }
    return fields({{"version", 1},
                   {"id", id},
                   {"sketch", sketch},
                   {"sources", refs},
                   {"angle", angle},
                   {"scale", scale},
                   {"center", QCborArray{center.x, center.y}},
                   {"translation", QCborArray{translation.x, translation.y}},
                   {"construction", construction},
                   {"members", declared}});
}
Conversion Conversion::parse(const QCborMap &m) {
    wire::keys(m, {"version", "id", "sketch", "sources", "angle", "scale", "center", "translation",
                   "construction", "members"});
    check(wire::integer(m, "version") == 1, "IncompatibleSchema", "Unsupported conversion version");
    check(m.value("sources").isArray() && m.value("sources").toArray().size() <= 32 &&
              m.value("construction").isBool(),
          "InvalidInput", "Typed conversion fields required");
    Conversion c;
    c.id = wire::text(m, "id");
    c.sketch = wire::text(m, "sketch");
    for (auto value : m.value("sources").toArray()) {
        check(value.isMap(), "InvalidInput", "Typed conversion reference required");
        c.sources.push_back(domain::GeometryReference::parse(value.toMap()));
    }
    c.angle = number(m, "angle", -2 * std::numbers::pi, 2 * std::numbers::pi);
    c.scale = number(m, "scale", 1e-6, 1e6);
    auto point = [&](QString key) {
        auto value = m.value(key);
        check(value.isArray() && value.toArray().size() == 2, "InvalidInput",
              "Two SI transform coordinates required");
        auto a = value.toArray();
        auto p = fields({{"x", a[0]}, {"y", a[1]}});
        return cad::sketch::Vec2{number(p, "x", -1000, 1000), number(p, "y", -1000, 1000)};
    };
    c.center = point("center");
    c.translation = point("translation");
    c.construction = m.value("construction").toBool();
    check(m.value("members").isMap() && m.value("members").toMap().size() <= 256, "ResourceLimit",
          "Bounded generated member declarations required");
    auto declared = m.value("members").toMap();
    for (auto it = declared.cbegin(); it != declared.cend(); ++it) {
        check(it.key().isString() && it.value().isString(), "InvalidInput",
              "Typed generated member declaration required");
        c.members.insert(it.key().toString(), it.value().toString());
    }
    c.map();
    return c;
}
evaluation::Graph SupportDefinition::graph() const {
    check(datums.size() <= 32 && attachments.size() <= 16 && conversions.size() <= 32,
          "ResourceLimit", "Bounded support definition collections required");
    auto parsed = cad::sketch::parse(sketches);
    QMap<QString, QMap<QString, QString>> declared;
    for (auto &c : conversions) {
        c.map();
        for (auto it = c.members.cbegin(); it != c.members.cend(); ++it) {
            check(!declared[c.sketch].contains(it.key()), "InvalidInput",
                  "Repeated generated member declaration");
            declared[c.sketch].insert(it.key(), it.value());
        }
    }
    evaluation::Plan plan(sketches, features, declared);
    auto nodes = cad::sketch::dependencyNodes(parsed);
    QMap<QString, size_t> sketchNodes;
    for (size_t i = 0; i < nodes.size(); ++i) {
        sketchNodes.insert(nodes[i].id, i);
        auto members = declared.value(nodes[i].id);
        for (auto it = members.cbegin(); it != members.cend(); ++it)
            nodes[i].members.insert(it.key(), it.value());
    }
    for (auto &feature : plan.nodes) {
        evaluation::GraphNode node{feature.feature.id, "Feature", {}, {}};
        for (auto id : plan.geometryGraph.dependencies(node.id)) {
            domain::GeometryReference r;
            r.object = id;
            node.references.push_back(r);
        }
        nodes.push_back(node);
    }
    for (auto &d : datums)
        for (auto id : d.outputs())
            nodes.push_back({id, d.kind, d.references, {}});
    QSet<QString> attached;
    for (auto &a : attachments) {
        a.map();
        check(sketchNodes.contains(a.sketch), "ReferenceMissing", "Attachment sketch is missing");
        check(!attached.contains(a.sketch), "InvalidInput",
              "Sketch has multiple support definitions");
        attached.insert(a.sketch);
        nodes[sketchNodes.value(a.sketch)].references.push_back(a.reference);
    }
    for (auto &c : conversions) {
        c.map();
        check(sketchNodes.contains(c.sketch), "ReferenceMissing", "Conversion sketch is missing");
        auto sources = c.sources;
        for (auto &a : attachments)
            if (a.sketch == c.sketch)
                sources.push_back(a.reference);
        nodes.push_back({c.id, "DerivedCurve", sources, {}});
        domain::GeometryReference r;
        r.object = c.id;
        nodes[sketchNodes.value(c.sketch)].references.push_back(r);
    }
    auto graph = evaluation::Graph(nodes, history, document);
    check(history.size() == qsizetype(nodes.size()), "InvalidInput",
          "Complete explicit model history required");
    return graph;
}
QCborMap SupportDefinition::map() const {
    QCborArray ds, as, cs, hs;
    for (auto &d : datums)
        ds.append(d.map());
    for (auto &a : attachments)
        as.append(a.map());
    for (auto &c : conversions)
        cs.append(c.map());
    for (auto id : history)
        hs.append(id);
    auto result = fields({{"version", 1},
                          {"document", document},
                          {"sketches", sketches},
                          {"features", features},
                          {"datums", ds},
                          {"attachments", as},
                          {"conversions", cs},
                          {"history", hs}});
    check(QCborValue(result).toCbor().size() <= qsizetype(limits::control), "ResourceLimit",
          "Support preview exceeds control admission");
    graph();
    return result;
}
SupportDefinition SupportDefinition::parse(const QCborMap &m) {
    check(QCborValue(m).toCbor().size() <= qsizetype(limits::control), "ResourceLimit",
          "Support preview exceeds control admission");
    wire::keys(m, {"version", "document", "sketches", "features", "datums", "attachments",
                   "conversions", "history"});
    check(wire::integer(m, "version") == 1, "IncompatibleSchema",
          "Unsupported support preview version");
    for (auto key : {"sketches", "features", "datums", "attachments", "conversions", "history"})
        check(m.value(QString::fromLatin1(key)).isArray(), "InvalidInput",
              "Typed support collection required");
    check(m.value("datums").toArray().size() <= 32 &&
              m.value("attachments").toArray().size() <= 16 &&
              m.value("conversions").toArray().size() <= 32 &&
              m.value("history").toArray().size() <= 132,
          "ResourceLimit", "Support preview collection admission exceeded");
    SupportDefinition d;
    d.document = wire::text(m, "document");
    d.sketches = m.value("sketches").toArray();
    d.features = m.value("features").toArray();
    auto each = [&](QString key, auto parse) {
        for (auto value : m.value(key).toArray()) {
            check(value.isMap(), "InvalidInput", "Semantic support record required");
            parse(value.toMap());
        }
    };
    each("datums", [&](auto map) { d.datums.push_back(Datum::parse(map)); });
    each("attachments", [&](auto map) { d.attachments.push_back(Attachment::parse(map)); });
    each("conversions", [&](auto map) { d.conversions.push_back(Conversion::parse(map)); });
    for (auto value : m.value("history").toArray()) {
        check(value.isString(), "InvalidInput", "History object identity required");
        d.history.append(domain::id(value.toString()));
    }
    d.graph();
    return d;
}
} // namespace cad::model
