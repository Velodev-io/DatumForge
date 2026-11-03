#include "ModelingIntent.hpp"
#include "Model.hpp"
#include <numbers>
#include <set>
#include <tuple>
namespace cad::features {
namespace {
QCborMap object(const QCborMap &m, QString key) {
    domain::check(m.value(key).isMap(), "InvalidInput", "Map required: " + key);
    return m.value(key).toMap();
}
void boolean(const QCborMap &m, QString key) {
    domain::check(m.value(key).isBool(), "InvalidInput", "Boolean required: " + key);
}
struct Reader {
    evaluation::GraphNode node;
    QMap<QString, QString> sourceKinds;
    void kind(QString id, QString expected) {
        domain::check(!sourceKinds.contains(id) || sourceKinds.value(id) == expected,
                      "InvalidInput", "Conflicting modeling source kinds");
        sourceKinds[id] = expected;
    }
    domain::GeometryReference reference(const QCborMap &m, QString key, QString category) {
        auto r = domain::GeometryReference::parse(object(m, key));
        bool curve = r.role == "Curve" || r.role == "Edge";
        bool selector = !r.endpoint.isEmpty() || r.parameter.has_value();
        bool valid = category == "Plane" ? r.role == "Plane" || r.role == "Face"
                     : category == "Point"
                         ? r.role == "Point" || r.role == "Vertex" || (curve && selector)
                     : category == "Axis" ? r.role == "Axis" || (curve && !selector)
                     : category == "Body" ? r.role == "Object"
                                          : r.role == category;
        domain::check(valid, "InvalidInput", "Wrong modeling reference role: " + key);
        node.references.push_back(r);
        if (category == "Body" || r.role == "Face" || r.role == "Edge" || r.role == "Vertex")
            kind(r.object, "Feature");
        return r;
    }
    domain::GeometryReference body(const QCborMap &m) {
        wire::keys(m, {"source", "body"});
        domain::id(wire::text(m, "body"));
        return reference(m, "source", "Body");
    }
    QString profile(const QCborMap &m) {
        wire::keys(m, {"sketch", "mode", "regions"});
        domain::GeometryReference r;
        r.object = domain::id(wire::text(m, "sketch"));
        kind(r.object, "Sketch");
        node.references.push_back(r);
        auto mode = wire::text(m, "mode");
        domain::check(m.value("regions").isArray(), "InvalidInput", "Profile regions required");
        auto regions = m.value("regions").toArray();
        if (mode == "AllClosed") {
            domain::check(regions.empty(), "InvalidInput", "AllClosed has no selected regions");
            return mode;
        }
        domain::check(mode == "SelectedRegions" || mode == "OpenChains", "UnsupportedOperation",
                      "Unsupported region selection");
        domain::check(!regions.empty() && regions.size() <= 32, "ResourceLimit",
                      "Select 1–32 profile regions");
        qsizetype boundaryCount = 0, uses = 0;
        for (auto regionValue : regions) {
            domain::check(regionValue.isArray(), "InvalidInput", "Region boundaries required");
            auto region = regionValue.toArray();
            boundaryCount += region.size();
            domain::check(!region.empty() && boundaryCount <= 256 &&
                              (mode != "OpenChains" || region.size() == 1),
                          "InvalidInput", "Invalid or excessive region boundaries");
            for (auto boundaryValue : region) {
                domain::check(boundaryValue.isArray(), "InvalidInput", "Curve boundary required");
                auto boundary = boundaryValue.toArray();
                uses += boundary.size();
                domain::check(!boundary.empty() && uses <= 256, "ResourceLimit",
                              "Select 1–256 curve uses");
                std::set<std::tuple<QString, double, double>> seen;
                for (auto curveValue : boundary) {
                    domain::check(curveValue.isMap(), "InvalidInput",
                                  "Directed curve use required");
                    auto curve = curveValue.toMap();
                    wire::keys(curve, {"entity", "from", "to", "reverse"});
                    auto entity = domain::id(wire::text(curve, "entity"));
                    double from = number(curve, "from", 0, 1), to = number(curve, "to", 0, 1);
                    domain::check(from < to && seen.emplace(entity, from, to).second,
                                  "InvalidInput", "Empty or repeated curve interval");
                    boolean(curve, "reverse");
                    auto member = r;
                    member.role = "Curve";
                    member.entity = entity;
                    node.references.push_back(member);
                }
            }
        }
        return mode;
    }
    void start(const QCborMap &m, bool revolve) {
        auto mode = wire::text(m, "mode");
        QSet<QString> keys{"mode"};
        if (mode == "SketchPlane") {
        } else if (mode == "Offset" && !revolve) {
            keys.insert("distance");
            number(m, "distance", -1000, 1000);
        } else if ((mode == "Surface" || mode == "Vertex") && !revolve) {
            keys.insert("reference");
            reference(m, "reference", mode == "Surface" ? "Plane" : "Point");
        } else
            throw domain::Error("UnsupportedOperation", "Unsupported feature start");
        wire::keys(m, keys);
    }
    double end(const QCborMap &m, bool revolve, bool second, bool target) {
        auto mode = wire::text(m, "mode");
        QSet<QString> keys{"mode"};
        double value = 0;
        if (mode == "Blind" || (mode == "Midplane" && !second)) {
            keys.insert("value");
            value = number(m, "value", 1e-8, revolve ? 2 * std::numbers::pi : 1000);
        } else if (!revolve && (mode == "UpToVertex" || mode == "UpToSurface" ||
                                mode == "OffsetFromSurface" || mode == "UpToBody")) {
            keys.insert("reference");
            reference(m, "reference",
                      mode == "UpToVertex" ? "Point"
                      : mode == "UpToBody" ? "Body"
                                           : "Plane");
            if (mode == "UpToBody") {
                keys.insert("body");
                domain::id(wire::text(m, "body"));
            }
            if (mode == "OffsetFromSurface") {
                keys.unite({"offset", "offset_reverse"});
                number(m, "offset", 0, 1000);
                boolean(m, "offset_reverse");
            }
        } else
            domain::check(!revolve && target && (mode == "UpToNext" || mode == "ThroughAll"),
                          "UnsupportedOperation", "Unsupported feature end or missing target");
        if (!revolve) {
            keys.insert("draft");
            auto draft = object(m, "draft");
            wire::keys(draft, {"angle", "outward"});
            domain::check(number(draft, "angle", 0, std::numbers::pi / 2) < std::numbers::pi / 2,
                          "InvalidInput", "Draft angle must be below pi/2");
            boolean(draft, "outward");
        }
        wire::keys(m, keys);
        return value;
    }
    void thin(const QCborMap &m) {
        auto mode = wire::text(m, "mode");
        QSet<QString> keys{"mode"};
        if (mode == "OneDirection" || mode == "Midplane") {
            keys.insert("thickness");
            number(m, "thickness", 1e-8, 1000);
            if (mode == "OneDirection") {
                keys.insert("reverse");
                boolean(m, "reverse");
            }
        } else if (mode == "TwoDirection") {
            keys.unite({"thickness1", "thickness2"});
            number(m, "thickness1", 1e-8, 1000);
            number(m, "thickness2", 1e-8, 1000);
        } else
            domain::check(mode == "Solid", "UnsupportedOperation", "Unsupported thickness mode");
        wire::keys(m, keys);
    }
};
} // namespace
ModelingIntent ModelingIntent::parse(const QCborMap &m) {
    domain::check(QCborValue(m).toCbor().size() <= 131072, "ResourceLimit",
                  "Modeling intent exceeds 128 KiB");
    domain::check(wire::integer(m, "version") == 1, "IncompatibleSchema",
                  "Unsupported modeling intent version");
    ModelingIntent out;
    Reader reader;
    reader.node.id = domain::id(wire::text(m, "id"));
    reader.node.kind = "Feature";
    auto kind = wire::text(m, "kind");
    if (kind == "Draft") {
        wire::keys(
            m, {"version", "id", "kind", "target", "neutral", "pull", "faces", "angle", "outward"});
        auto target = reader.body(object(m, "target"));
        reader.reference(m, "neutral", "Plane");
        reader.reference(m, "pull", "Axis");
        domain::check(m.value("faces").isArray(), "InvalidInput", "Draft faces required");
        auto faces = m.value("faces").toArray();
        domain::check(!faces.empty() && faces.size() <= 32, "ResourceLimit",
                      "Select 1–32 draft faces");
        QSet<QString> tokens;
        for (auto face : faces) {
            auto r = reader.reference(sketch::fields({{"face", face}}), "face", "Face");
            // Graph validation resolves Self/explicit-current-document aliases.
            domain::check(r.object == target.object && !tokens.contains(r.topology), "InvalidInput",
                          "Draft faces must be unique members of the target stage");
            tokens.insert(r.topology);
        }
        domain::check(number(m, "angle", 1e-8, std::numbers::pi / 2) < std::numbers::pi / 2,
                      "InvalidInput", "Draft angle must be below pi/2");
        boolean(m, "outward");
    } else {
        domain::check(kind == "Extrude" || kind == "Revolve", "UnsupportedOperation",
                      "Unsupported modeling intent kind");
        bool revolve = kind == "Revolve";
        QSet<QString> keys{"version", "id",         "kind",       "operation", "target", "profile",
                           "start",   "direction1", "direction2", "reverse",   "thin"};
        if (revolve) {
            keys.insert("axis");
            reader.reference(m, "axis", "Axis");
        }
        auto operation = wire::text(m, "operation");
        domain::check(QStringList{"NewBody", "Add", "Cut"}.contains(operation),
                      "UnsupportedOperation", "Unsupported Boolean intent");
        if (operation == "NewBody")
            domain::check(m.value("target").isNull(), "InvalidInput", "NewBody has no target");
        else
            reader.body(object(m, "target"));
        auto profileMode = reader.profile(object(m, "profile"));
        reader.start(object(m, "start"), revolve);
        auto first = object(m, "direction1");
        double angle = reader.end(first, revolve, false, operation != "NewBody");
        if (!m.value("direction2").isNull()) {
            domain::check(wire::text(first, "mode") != "Midplane", "InvalidInput",
                          "Midplane already defines both directions");
            angle += reader.end(object(m, "direction2"), revolve, true, operation != "NewBody");
        }
        domain::check(!revolve || angle <= 2 * std::numbers::pi, "InvalidInput",
                      "Revolve directions exceed one turn");
        boolean(m, "reverse");
        auto thin = object(m, "thin");
        reader.thin(thin);
        if (profileMode == "OpenChains" && wire::text(thin, "mode") == "Solid") {
            domain::check(!revolve && operation == "Cut", "UnsupportedOperation",
                          "Open profiles require thickness or an extrude cut");
            keys.insert("flip_side");
            boolean(m, "flip_side");
        }
        wire::keys(m, keys);
    }
    domain::check(reader.node.references.size() <= 512, "ResourceLimit",
                  "Too many modeling references");
    out.node = std::move(reader.node);
    out.sourceKinds = std::move(reader.sourceKinds);
    out.definition = m;
    return out;
}
evaluation::Graph ModelingIntent::graph(const std::vector<evaluation::GraphNode> &upstream,
                                        QStringList history, QString document) const {
    domain::check(upstream.size() < 132 && history.size() <= 132, "ResourceLimit",
                  "Modeling graph exceeds existing metadata admission");
    domain::check(static_cast<size_t>(history.size()) == upstream.size() + 1, "DependencyOrder",
                  "Complete modeling history required");
    auto nodes = upstream;
    nodes.push_back(node);
    evaluation::Graph result(nodes, std::move(history), std::move(document));
    for (auto it = sourceKinds.cbegin(); it != sourceKinds.cend(); ++it) {
        auto source = std::find_if(nodes.begin(), nodes.end(),
                                   [&](const auto &candidate) { return candidate.id == it.key(); });
        domain::check(source != nodes.end() && source->kind == it.value(), "ReferenceMissing",
                      "Modeling reference source has the wrong kind");
    }
    return result;
}
} // namespace cad::features
