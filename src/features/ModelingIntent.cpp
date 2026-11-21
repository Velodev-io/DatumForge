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
        bool valid =
            category == "Plane"    ? r.role == "Plane" || r.role == "Face"
            : category == "Point"  ? r.role == "Point" || r.role == "Vertex" || (curve && selector)
            : category == "Axis"   ? r.role == "Axis" || (curve && !selector)
            : category == "Body"   ? r.role == "Object"
            : category == "Thread" ? r.role == "Edge" || r.role == "Face" || r.role == "Curve"
            : category == "Direction" ? r.role == "Axis" || r.role == "Edge" || r.role == "Curve" ||
                                            r.role == "Plane" || r.role == "Face"
            : category == "Curve" ? r.role == "Curve" || r.role == "Edge"
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
        QSet<QString> allowed{"sketch", "mode", "regions"};
        if (m.contains(QStringLiteral("type")))
            allowed.insert(QStringLiteral("type"));
        wire::keys(m, allowed);
        if (m.contains(QStringLiteral("type")))
            domain::check(wire::text(m, "type") == "Sketch", "InvalidInput",
                          "Profile type must be Sketch");
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
    } else if (kind == "Hole") {
        wire::keys(m, {"version", "id", "kind", "operation", "target", "family", "placement",
                       "sizing", "termination"});
        domain::check(wire::text(m, "operation") == "Cut", "UnsupportedOperation",
                      "Hole operation must be Cut");
        reader.body(object(m, "target"));
        auto family = wire::text(m, "family");
        domain::check(QStringList{"Simple", "Counterbore", "Countersink", "Drilled", "Tapped",
                                  "PipeTap", "LegacySimple", "LegacySimpleDrilled",
                                  "LegacyCounterboreDrill", "LegacyCountersinkDrill",
                                  "LegacyTapered"}
                          .contains(family),
                      "UnsupportedOperation", "Unsupported hole family: " + family);
        auto placement = object(m, "placement");
        wire::keys(placement, {"face", "sketch", "points"});
        reader.reference(placement, "face", "Face");
        auto sketchId = domain::id(wire::text(placement, "sketch"));
        reader.kind(sketchId, "Sketch");
        domain::GeometryReference skRef;
        skRef.object = sketchId;
        reader.node.references.push_back(skRef);
        domain::check(placement.value("points").isArray(), "InvalidInput",
                      "Hole placement points required");
        auto pts = placement.value("points").toArray();
        domain::check(!pts.empty() && pts.size() <= 64, "ResourceLimit",
                      "Select 1–64 hole placement points");
        QSet<QString> seenPts;
        for (auto ptVal : pts) {
            auto ptId = domain::id(wire::text(sketch::fields({{"p", ptVal}}), "p"));
            domain::check(!seenPts.contains(ptId), "InvalidInput",
                          "Duplicate hole placement point");
            seenPts.insert(ptId);
            domain::GeometryReference ptRef;
            ptRef.object = sketchId;
            ptRef.role = "Point";
            ptRef.entity = ptId;
            reader.node.references.push_back(ptRef);
        }
        auto sizing = object(m, "sizing");
        auto smode = wire::text(sizing, "mode");
        domain::check(smode == "Custom" || smode == "Catalog", "UnsupportedOperation",
                      "Unsupported sizing mode");
        QSet<QString> szKeys{"mode", "diameter", "depth"};
        double dia = number(sizing, "diameter", 1e-8, 1000);
        double depth = number(sizing, "depth", 1e-8, 1000);
        if (family == "Counterbore" || family == "LegacyCounterboreDrill") {
            szKeys.unite({"counterbore_diameter", "counterbore_depth"});
            number(sizing, "counterbore_diameter", dia, 1000);
            number(sizing, "counterbore_depth", 1e-8, depth);
        }
        if (family == "Countersink" || family == "LegacyCountersinkDrill") {
            szKeys.unite({"countersink_diameter", "countersink_angle"});
            number(sizing, "countersink_diameter", dia, 1000);
            number(sizing, "countersink_angle", 1e-4, std::numbers::pi - 1e-4);
        }
        if (family == "Drilled" || family == "LegacySimpleDrilled" ||
            family == "LegacyCounterboreDrill" || family == "LegacyCountersinkDrill") {
            szKeys.insert("drill_angle");
            number(sizing, "drill_angle", 1e-4, std::numbers::pi - 1e-4);
        }
        if (family == "Tapered" || family == "LegacyTapered" || family == "PipeTap") {
            szKeys.insert("taper_angle");
            number(sizing, "taper_angle", 1e-4, std::numbers::pi / 2 - 1e-4);
        }
        if (family == "Tapped" || family == "PipeTap") {
            szKeys.unite({"tap_drill_diameter", "thread"});
            number(sizing, "tap_drill_diameter", 1e-8, dia);
            auto thread = object(sizing, "thread");
            wire::keys(thread, {"cosmetic", "designation", "pitch", "depth"});
            boolean(thread, "cosmetic");
            domain::label(wire::text(thread, "designation"));
            number(thread, "pitch", 1e-8, 1000);
            number(thread, "depth", 1e-8, depth);
        }
        if (smode == "Catalog") {
            szKeys.unite({"dataset", "standard", "type", "size", "fit"});
            domain::label(wire::text(sizing, "dataset"));
            if (sizing.contains(QStringLiteral("version"))) {
                szKeys.insert("version");
                domain::label(wire::text(sizing, "version"));
            }
            wire::text(sizing, "standard");
            wire::text(sizing, "type");
            wire::text(sizing, "size");
            wire::text(sizing, "fit");
        }
        wire::keys(sizing, szKeys);
        auto term = object(m, "termination");
        auto tmode = wire::text(term, "mode");
        auto dref = wire::text(term, "depth_reference");
        domain::check(dref == "Shoulder" || dref == "Tip", "InvalidInput",
                      "Depth reference must be Shoulder or Tip");
        QSet<QString> tmKeys{"mode", "depth_reference"};
        if (tmode == "Blind" || tmode == "ThroughAll" || tmode == "UpToNext") {
        } else if (tmode == "UpToVertex" || tmode == "UpToSurface") {
            tmKeys.insert("reference");
            reader.reference(term, "reference", tmode == "UpToVertex" ? "Point" : "Plane");
        } else if (tmode == "OffsetFromSurface") {
            tmKeys.unite({"reference", "offset", "offset_reverse"});
            reader.reference(term, "reference", "Plane");
            number(term, "offset", 0, 1000);
            boolean(term, "offset_reverse");
        } else
            throw domain::Error("UnsupportedOperation",
                                "Unsupported hole termination mode: " + tmode);
        wire::keys(term, tmKeys);
    } else if (kind == "CosmeticThread") {
        wire::keys(m, {"version", "id", "kind", "target", "reference", "designation", "standard",
                       "type", "size", "thread_type", "major_diameter", "minor_diameter", "pitch",
                       "termination"});
        reader.body(object(m, "target"));
        reader.reference(m, "reference", "Thread");
        domain::label(wire::text(m, "designation"));
        wire::text(m, "standard");
        wire::text(m, "type");
        wire::text(m, "size");
        auto tt = wire::text(m, "thread_type");
        domain::check(tt == "Internal" || tt == "External", "InvalidInput",
                      "Thread type must be Internal or External");
        double major = number(m, "major_diameter", 1e-8, 1000);
        number(m, "minor_diameter", 1e-8, major);
        number(m, "pitch", 1e-8, 1000);
        auto term = object(m, "termination");
        auto tmode = wire::text(term, "mode");
        if (tmode == "Blind") {
            wire::keys(term, {"mode", "depth"});
            number(term, "depth", 1e-8, 1000);
        } else if (tmode == "ThroughAll" || tmode == "UpToNext") {
            wire::keys(term, {"mode"});
        } else
            throw domain::Error("UnsupportedOperation", "Unsupported thread termination: " + tmode);
    } else if (kind == "Rib") {
        wire::keys(m, {"version", "id", "kind", "operation", "target", "profile", "thickness",
                       "side", "direction", "flip_material"});
        domain::check(wire::text(m, "operation") == "Add", "UnsupportedOperation",
                      "Rib operation must be Add");
        reader.body(object(m, "target"));
        reader.profile(object(m, "profile"));
        number(m, "thickness", 1e-8, 1000);
        auto side = wire::text(m, "side");
        domain::check(side == "Both" || side == "First" || side == "Second", "InvalidInput",
                      "Rib side must be Both, First, or Second");
        auto dir = wire::text(m, "direction");
        domain::check(dir == "Parallel" || dir == "Normal", "InvalidInput",
                      "Rib direction must be Parallel or Normal");
        boolean(m, "flip_material");
    } else if (kind == "LinearPattern" || kind == "CircularPattern") {
        bool linear = kind == "LinearPattern";
        auto op = wire::text(m, "operation");
        domain::check(QStringList{"NewBody", "Add", "Cut"}.contains(op), "UnsupportedOperation",
                      "Unsupported pattern Boolean operation");
        if (op == "NewBody")
            domain::check(m.value("target").isNull(), "InvalidInput", "NewBody has no target");
        else
            reader.body(object(m, "target"));
        auto skind = wire::text(m, "seed_kind");
        domain::check(skind == "Feature" || skind == "Face" || skind == "Body", "InvalidInput",
                      "Seed kind must be Feature, Face, or Body");
        if (skind == "Feature") {
            auto seedId = domain::id(wire::text(m, "seed"));
            reader.kind(seedId, "Feature");
            domain::GeometryReference r;
            r.object = seedId;
            reader.node.references.push_back(r);
        } else if (skind == "Body") {
            reader.body(object(m, "seed"));
        } else {
            domain::check(m.value("seed").isArray(), "InvalidInput", "Face seeds required");
            auto faces = m.value("seed").toArray();
            domain::check(!faces.empty() && faces.size() <= 32, "ResourceLimit",
                          "Select 1–32 face seeds");
            for (auto f : faces)
                reader.reference(sketch::fields({{"f", f}}), "f", "Face");
        }
        if (linear) {
            wire::keys(m, {"version", "id", "kind", "operation", "target", "seed_kind", "seed",
                           "direction1", "direction2"});
            auto d1 = object(m, "direction1");
            wire::keys(d1, {"reference", "spacing", "count", "reverse"});
            reader.reference(d1, "reference", "Direction");
            number(d1, "spacing", 1e-8, 1000);
            int count1 = wire::integer(d1, "count");
            domain::check(count1 >= 1 && count1 <= 32, "InvalidInput",
                          "Pattern count must be 1–32");
            boolean(d1, "reverse");
            int total = count1;
            if (!m.value("direction2").isNull()) {
                auto d2 = object(m, "direction2");
                wire::keys(d2, {"reference", "spacing", "count", "reverse"});
                reader.reference(d2, "reference", "Direction");
                number(d2, "spacing", 1e-8, 1000);
                int count2 = wire::integer(d2, "count");
                domain::check(count2 >= 1 && count2 <= 32, "InvalidInput",
                              "Pattern count must be 1–32");
                boolean(d2, "reverse");
                total *= count2;
            }
            domain::check(total <= 1024, "ResourceLimit",
                          "Total pattern instances exceed limit 1024");
        } else {
            wire::keys(m, {"version", "id", "kind", "operation", "target", "seed_kind", "seed",
                           "axis", "count", "equal_spacing", "angle", "reverse"});
            reader.reference(m, "axis", "Axis");
            int count = wire::integer(m, "count");
            domain::check(count >= 1 && count <= 360, "InvalidInput",
                          "Circular pattern count must be 1–360");
            boolean(m, "equal_spacing");
            number(m, "angle", 1e-8, 2 * std::numbers::pi);
            boolean(m, "reverse");
        }
    } else if (kind == "Mirror") {
        wire::keys(m,
                   {"version", "id", "kind", "operation", "target", "seed_kind", "seed", "plane"});
        auto op = wire::text(m, "operation");
        domain::check(QStringList{"NewBody", "Add", "Cut"}.contains(op), "UnsupportedOperation",
                      "Unsupported mirror Boolean operation");
        if (op == "NewBody")
            domain::check(m.value("target").isNull(), "InvalidInput", "NewBody has no target");
        else
            reader.body(object(m, "target"));
        reader.reference(m, "plane", "Plane");
        auto skind = wire::text(m, "seed_kind");
        domain::check(skind == "Feature" || skind == "Face" || skind == "Body", "InvalidInput",
                      "Seed kind must be Feature, Face, or Body");
        if (skind == "Feature") {
            auto seedId = domain::id(wire::text(m, "seed"));
            reader.kind(seedId, "Feature");
            domain::GeometryReference r;
            r.object = seedId;
            reader.node.references.push_back(r);
        } else if (skind == "Body") {
            reader.body(object(m, "seed"));
        } else {
            domain::check(m.value("seed").isArray(), "InvalidInput", "Face seeds required");
            auto faces = m.value("seed").toArray();
            domain::check(!faces.empty() && faces.size() <= 32, "ResourceLimit",
                          "Select 1–32 face seeds");
            for (auto f : faces)
                reader.reference(sketch::fields({{"f", f}}), "f", "Face");
        }
    } else if (kind == "Fillet") {
        auto target = reader.body(object(m, "target"));
        boolean(m, "propagate_tangent");
        auto method = wire::text(m, "method");
        if (method == "Constant") {
            wire::keys(m, {"version", "id", "kind", "target", "method", "propagate_tangent",
                           "radius", "edges"});
            number(m, "radius", 1e-8, 1000);
            domain::check(m.value("edges").isArray(), "InvalidInput", "Fillet edges required");
            auto edges = m.value("edges").toArray();
            domain::check(!edges.empty() && edges.size() <= 32, "ResourceLimit",
                          "Select 1–32 fillet edges");
            for (auto e : edges) {
                auto r = reader.reference(sketch::fields({{"e", e}}), "e", "Edge");
                domain::check(r.object == target.object, "InvalidInput",
                              "Fillet edges must be members of the target stage");
            }
        } else if (method == "MultipleRadius") {
            wire::keys(m,
                       {"version", "id", "kind", "target", "method", "propagate_tangent", "radii"});
            domain::check(m.value("radii").isArray(), "InvalidInput", "Multiple radii required");
            auto radii = m.value("radii").toArray();
            domain::check(!radii.empty() && radii.size() <= 32, "ResourceLimit",
                          "Select 1–32 fillet radii");
            for (auto item : radii) {
                auto map = item.toMap();
                wire::keys(map, {"edge", "radius"});
                auto r = reader.reference(map, "edge", "Edge");
                domain::check(r.object == target.object, "InvalidInput",
                              "Fillet edges must be members of the target stage");
                number(map, "radius", 1e-8, 1000);
            }
        } else if (method == "Variable") {
            wire::keys(m, {"version", "id", "kind", "target", "method", "propagate_tangent", "edge",
                           "control_points", "transition"});
            auto r = reader.reference(m, "edge", "Edge");
            domain::check(r.object == target.object, "InvalidInput",
                          "Variable fillet edge must be a member of the target stage");
            domain::check(m.value("control_points").isArray(), "InvalidInput",
                          "Variable fillet control points required");
            auto pts = m.value("control_points").toArray();
            domain::check(pts.size() >= 2 && pts.size() <= 32, "ResourceLimit",
                          "Select 2–32 variable fillet control points");
            double previousParameter = -1;
            for (auto pt : pts) {
                auto ptMap = pt.toMap();
                wire::keys(ptMap, {"parameter", "radius"});
                auto parameter = number(ptMap, "parameter", 0, 1);
                domain::check(parameter > previousParameter, "InvalidInput",
                              "Radius parameters must strictly increase");
                previousParameter = parameter;
                number(ptMap, "radius", 1e-8, 1000);
            }
            domain::check(pts[0].toMap().value("parameter").toDouble() == 0 &&
                              pts.last().toMap().value("parameter").toDouble() == 1,
                          "InvalidInput", "Radius law must include both edge endpoints");
            auto tr = wire::text(m, "transition");
            domain::check(tr == "Smooth" || tr == "Linear", "InvalidInput",
                          "Transition must be Smooth or Linear");
        } else if (method == "Face") {
            wire::keys(m, {"version", "id", "kind", "target", "method", "propagate_tangent",
                           "faces1", "faces2", "radius"});
            number(m, "radius", 1e-8, 1000);
            for (QString fset : {"faces1", "faces2"}) {
                domain::check(m.value(fset).isArray(), "InvalidInput",
                              "Face set required: " + fset);
                auto farr = m.value(fset).toArray();
                domain::check(!farr.empty() && farr.size() <= 32, "ResourceLimit",
                              "Select 1–32 faces: " + fset);
                for (auto f : farr) {
                    auto r = reader.reference(sketch::fields({{"f", f}}), "f", "Face");
                    domain::check(r.object == target.object, "InvalidInput",
                                  "Face fillet faces must be members of the target stage");
                }
            }
        } else if (method == "FullRound") {
            wire::keys(m, {"version", "id", "kind", "target", "method", "propagate_tangent",
                           "side_faces1", "center_faces", "side_faces2"});
            for (QString fset : {"side_faces1", "center_faces", "side_faces2"}) {
                domain::check(m.value(fset).isArray(), "InvalidInput",
                              "Face set required: " + fset);
                auto farr = m.value(fset).toArray();
                domain::check(!farr.empty() && farr.size() <= 32, "ResourceLimit",
                              "Select 1–32 faces: " + fset);
                for (auto f : farr) {
                    auto r = reader.reference(sketch::fields({{"f", f}}), "f", "Face");
                    domain::check(r.object == target.object, "InvalidInput",
                                  "Full-round faces must be members of the target stage");
                }
            }
        } else
            throw domain::Error("UnsupportedOperation", "Unsupported fillet method: " + method);
    } else if (kind == "Chamfer") {
        auto target = reader.body(object(m, "target"));
        boolean(m, "propagate_tangent");
        domain::check(m.value("edges").isArray(), "InvalidInput", "Chamfer edges required");
        auto edges = m.value("edges").toArray();
        domain::check(!edges.empty() && edges.size() <= 32, "ResourceLimit",
                      "Select 1–32 chamfer edges");
        for (auto e : edges) {
            auto r = reader.reference(sketch::fields({{"e", e}}), "e", "Edge");
            domain::check(r.object == target.object, "InvalidInput",
                          "Chamfer edges must be members of the target stage");
        }
        auto method = wire::text(m, "method");
        if (method == "EqualDistance") {
            wire::keys(m, {"version", "id", "kind", "target", "method", "edges",
                           "propagate_tangent", "distance"});
            number(m, "distance", 1e-8, 1000);
        } else if (method == "DistanceDistance") {
            QSet<QString> keys{
                "version",           "id",        "kind",      "target", "method", "edges",
                "propagate_tangent", "distance1", "distance2", "flip"};
            if (m.contains(QStringLiteral("reference_face")) &&
                !m.value("reference_face").isNull()) {
                keys.insert("reference_face");
                auto r = reader.reference(m, "reference_face", "Face");
                domain::check(r.object == target.object, "InvalidInput",
                              "Reference face must be member of target stage");
            }
            wire::keys(m, keys);
            number(m, "distance1", 1e-8, 1000);
            number(m, "distance2", 1e-8, 1000);
            boolean(m, "flip");
        } else if (method == "AngleDistance") {
            QSet<QString> keys{
                "version",           "id",       "kind",  "target", "method", "edges",
                "propagate_tangent", "distance", "angle", "flip"};
            if (m.contains(QStringLiteral("reference_face")) &&
                !m.value("reference_face").isNull()) {
                keys.insert("reference_face");
                auto r = reader.reference(m, "reference_face", "Face");
                domain::check(r.object == target.object, "InvalidInput",
                              "Reference face must be member of target stage");
            }
            wire::keys(m, keys);
            number(m, "distance", 1e-8, 1000);
            domain::check(number(m, "angle", 1e-8, std::numbers::pi / 2) < std::numbers::pi / 2,
                          "InvalidInput", "Chamfer angle must be below pi/2");
            boolean(m, "flip");
        } else
            throw domain::Error("UnsupportedOperation", "Unsupported chamfer method: " + method);
    } else if (kind == "Shell") {
        wire::keys(m, {"version", "id", "kind", "target", "thickness", "inward", "remove_faces",
                       "overrides"});
        auto target = reader.body(object(m, "target"));
        number(m, "thickness", 1e-8, 1000);
        boolean(m, "inward");
        domain::check(m.value("remove_faces").isArray(), "InvalidInput",
                      "Shell remove_faces array required");
        auto removeFaces = m.value("remove_faces").toArray();
        domain::check(removeFaces.size() <= 64, "ResourceLimit",
                      "At most 64 shell remove faces admitted");
        QSet<QString> seenFaces;
        for (auto faceVal : removeFaces) {
            auto r = reader.reference(sketch::fields({{"f", faceVal}}), "f", "Face");
            domain::check(r.object == target.object, "InvalidInput",
                          "Shell removed faces must belong to the target stage");
            domain::check(!seenFaces.contains(r.topology), "InvalidInput",
                          "Duplicate shell remove face");
            seenFaces.insert(r.topology);
        }
        domain::check(m.value("overrides").isArray(), "InvalidInput",
                      "Shell overrides array required");
        auto overrides = m.value("overrides").toArray();
        domain::check(overrides.size() <= 64, "ResourceLimit",
                      "At most 64 shell thickness overrides admitted");
        QSet<QString> seenOverrides;
        for (auto ovVal : overrides) {
            domain::check(ovVal.isMap(), "InvalidInput", "Shell override map required");
            auto ovMap = ovVal.toMap();
            wire::keys(ovMap, {"face", "thickness"});
            auto r = reader.reference(ovMap, "face", "Face");
            domain::check(r.object == target.object, "InvalidInput",
                          "Shell override face must belong to the target stage");
            domain::check(!seenFaces.contains(r.topology), "InvalidInput",
                          "Face cannot be both removed and overridden in thickness");
            domain::check(!seenOverrides.contains(r.topology), "InvalidInput",
                          "Duplicate shell override face");
            seenOverrides.insert(r.topology);
            number(ovMap, "thickness", 1e-8, 1000);
        }
    } else if (kind == "Sweep") {
        auto operation = wire::text(m, "operation");
        domain::check(QStringList{"NewBody", "Add", "Cut"}.contains(operation),
                      "UnsupportedOperation", "Unsupported Boolean intent");
        if (operation == "NewBody")
            domain::check(m.value("target").isNull(), "InvalidInput", "NewBody has no target");
        else
            reader.body(object(m, "target"));
        auto profileObj = object(m, "profile");
        auto profileType = wire::text(profileObj, "type");
        QString profileMode;
        if (profileType == "Sketch") {
            profileMode = reader.profile(profileObj);
        } else if (profileType == "Circular") {
            wire::keys(profileObj, {"type", "diameter"});
            number(profileObj, "diameter", 1e-8, 1000);
        } else {
            throw domain::Error("InvalidInput", "Unsupported sweep profile type: " + profileType);
        }
        auto pathObj = object(m, "path");
        wire::keys(pathObj, {"reference", "reverse"});
        auto pathRef = reader.reference(pathObj, "reference", "Curve");
        boolean(pathObj, "reverse");
        domain::check(m.value("guides").isArray(), "InvalidInput", "Sweep guides array required");
        auto guidesArr = m.value("guides").toArray();
        domain::check(guidesArr.size() <= 16, "ResourceLimit", "At most 16 sweep guides admitted");
        for (auto gVal : guidesArr) {
            auto gRef = reader.reference(sketch::fields({{"g", gVal}}), "g", "Curve");
            domain::check(gRef.object != pathRef.object || gRef.entity != pathRef.entity ||
                              gRef.topology != pathRef.topology,
                          "InvalidInput", "Guide curve cannot duplicate sweep path");
        }
        auto corner = wire::text(m, "corner_transition");
        domain::check(corner == "Round" || corner == "Sharp", "InvalidInput",
                      "Sweep corner transition must be Round or Sharp");
        auto startTangency = wire::text(m, "start_tangency");
        domain::check(startTangency == "None" || startTangency == "PathTangent", "InvalidInput",
                      "Sweep start tangency must be None or PathTangent");
        auto endTangency = wire::text(m, "end_tangency");
        domain::check(endTangency == "None" || endTangency == "PathTangent", "InvalidInput",
                      "Sweep end tangency must be None or PathTangent");
        int previewSecs = wire::integer(m, "preview_sections");
        domain::check(previewSecs >= 0 && previewSecs <= 32, "InvalidInput",
                      "Sweep preview sections must be 0–32");
        auto thinObj = object(m, "thin");
        reader.thin(thinObj);
        auto thinMode = wire::text(thinObj, "mode");
        QSet<QString> keys{"version",
                           "id",
                           "kind",
                           "operation",
                           "target",
                           "profile",
                           "path",
                           "guides",
                           "corner_transition",
                           "start_tangency",
                           "end_tangency",
                           "thin",
                           "preview_sections"};
        if (profileType == "Sketch" && profileMode == "OpenChains" && thinMode == "Solid") {
            domain::check(operation == "Cut", "UnsupportedOperation",
                          "Open profile sweep requires thickness or a cut");
            keys.insert("flip_side");
            boolean(m, "flip_side");
        }
        wire::keys(m, keys);
    } else if (kind == "Loft") {
        auto output = wire::text(m, "output");
        domain::check(output == "Solid" || output == "Surface", "InvalidInput",
                      "Loft output must be Solid or Surface");
        auto operation = wire::text(m, "operation");
        if (output == "Surface") {
            domain::check(operation == "NewBody", "InvalidInput",
                          "Surface loft requires NewBody operation");
            domain::check(m.value("target").isNull(), "InvalidInput",
                          "Surface loft has no target body");
        } else {
            domain::check(QStringList{"NewBody", "Add", "Cut"}.contains(operation),
                          "UnsupportedOperation", "Unsupported Boolean intent");
            if (operation == "NewBody")
                domain::check(m.value("target").isNull(), "InvalidInput", "NewBody has no target");
            else
                reader.body(object(m, "target"));
        }

        domain::check(m.value("sections").isArray(), "InvalidInput",
                      "Loft sections array required");
        auto sectionsArr = m.value("sections").toArray();
        domain::check(sectionsArr.size() >= 2 && sectionsArr.size() <= 32, "ResourceLimit",
                      "Loft requires 2–32 sections");
        QString firstProfileMode;
        for (qsizetype s = 0; s < sectionsArr.size(); ++s) {
            domain::check(sectionsArr[s].isMap(), "InvalidInput", "Loft section map required");
            auto sMode = reader.profile(sectionsArr[s].toMap());
            if (s == 0)
                firstProfileMode = sMode;
            if (output == "Solid") {
                domain::check(sMode != "OpenChains", "InvalidInput",
                              "Solid loft requires closed sections");
            } else {
                bool open = (sMode == "OpenChains");
                bool firstOpen = (firstProfileMode == "OpenChains");
                domain::check(open == firstOpen, "InvalidInput",
                              "Loft sections must have consistent open/closed topology");
            }
        }

        if (!m.value("centerline").isNull()) {
            auto clObj = object(m, "centerline");
            wire::keys(clObj, {"reference", "reverse"});
            reader.reference(clObj, "reference", "Curve");
            boolean(clObj, "reverse");
        }

        domain::check(m.value("guides").isArray(), "InvalidInput", "Loft guides array required");
        auto guidesArr = m.value("guides").toArray();
        domain::check(guidesArr.size() <= 16, "ResourceLimit", "At most 16 loft guides admitted");
        for (auto gVal : guidesArr) {
            reader.reference(sketch::fields({{"g", gVal}}), "g", "Curve");
        }

        domain::check(m.value("connectors").isArray(), "InvalidInput",
                      "Loft connectors array required");
        auto connArr = m.value("connectors").toArray();
        domain::check(connArr.size() <= 64, "ResourceLimit", "At most 64 loft connectors admitted");
        QSet<QString> seenConnIds;
        std::vector<std::vector<double>> connParams;
        for (auto cVal : connArr) {
            domain::check(cVal.isMap(), "InvalidInput", "Loft connector map required");
            auto cMap = cVal.toMap();
            wire::keys(cMap, {"id", "points"});
            auto cId = domain::id(wire::text(cMap, "id"));
            domain::check(!seenConnIds.contains(cId), "InvalidInput",
                          "Duplicate connector identity");
            seenConnIds.insert(cId);

            domain::check(cMap.value("points").isArray(), "InvalidInput",
                          "Connector points array required");
            auto ptsArr = cMap.value("points").toArray();
            domain::check(ptsArr.size() == sectionsArr.size(), "InvalidInput",
                          "Connector must have a point for every section");
            std::vector<double> params;
            for (qsizetype p = 0; p < ptsArr.size(); ++p) {
                domain::check(ptsArr[p].isMap(), "InvalidInput", "Connector point map required");
                auto ptMap = ptsArr[p].toMap();
                wire::keys(ptMap, {"section", "parameter"});
                domain::check(wire::integer(ptMap, "section") == p, "InvalidInput",
                              "Connector points must correspond to section index in order");
                params.push_back(number(ptMap, "parameter", 0, 1));
            }
            connParams.push_back(std::move(params));
        }
        for (size_t i = 0; i < connParams.size(); ++i) {
            for (size_t j = i + 1; j < connParams.size(); ++j) {
                double diff0 = connParams[i][0] - connParams[j][0];
                if (std::abs(diff0) < 1e-9)
                    throw domain::Error("InvalidInput", "Loft connectors must not coincide");
                for (size_t s = 1; s < static_cast<size_t>(sectionsArr.size()); ++s) {
                    double diffS = connParams[i][s] - connParams[j][s];
                    if (std::abs(diffS) < 1e-9 || (diff0 > 0 && diffS < 0) ||
                        (diff0 < 0 && diffS > 0)) {
                        throw domain::Error("InvalidInput", "Loft connectors must not cross");
                    }
                }
            }
        }

        auto startCond = wire::text(m, "start_condition");
        domain::check(startCond == "None" || startCond == "NormalToSection" ||
                          startCond == "TangentToFace",
                      "InvalidInput", "Unsupported loft start condition");
        auto endCond = wire::text(m, "end_condition");
        domain::check(endCond == "None" || endCond == "NormalToSection" ||
                          endCond == "TangentToFace",
                      "InvalidInput", "Unsupported loft end condition");

        auto thinObj = object(m, "thin");
        reader.thin(thinObj);
        if (output == "Surface") {
            domain::check(wire::text(thinObj, "mode") == "Solid", "InvalidInput",
                          "Surface loft does not support thin wall thickness");
        }

        wire::keys(m, {"version", "id", "kind", "output", "operation", "target", "sections",
                       "centerline", "guides", "connectors", "start_condition", "end_condition",
                       "thin"});
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
