#include "Model.hpp"
#include <numbers>
namespace cad::assembly {
using domain::check;
using sketch::fields;
QCborMap Pose::map() const {
    return fields(
        {{"translation_si", features::array(translation)},
         {"rotation_wxyz", QCborArray{rotation[0], rotation[1], rotation[2], rotation[3]}}});
}
Pose Pose::parse(QCborMap m) {
    wire::keys(m, {"translation_si", "rotation_wxyz"});
    Pose p;
    p.translation = features::vector(m, "translation_si");
    auto q = m.value("rotation_wxyz").toArray();
    check(q.size() == 4, "InvalidInput", "Four quaternion coordinates required");
    double n = 0;
    for (int k = 0; k < 4; ++k) {
        check(q[k].isDouble() || q[k].isInteger(), "InvalidInput", "Numeric quaternion required");
        p.rotation[k] = q[k].toDouble();
        check(std::isfinite(p.rotation[k]), "InvalidInput", "Finite rotation required");
        n += p.rotation[k] * p.rotation[k];
    }
    check(std::abs(n - 1) < 1e-10, "InvalidInput", "Rotation quaternion must be normalized");
    // Canonical sign removes the double-cover ambiguity in durable definitions.
    for (double v : p.rotation)
        if (std::abs(v) > 1e-15) {
            if (v < 0)
                for (auto &x : p.rotation)
                    x = -x;
            break;
        }
    return p;
}
QCborMap Occurrence::map() const {
    return fields({{"id", id},
                   {"definition", definition},
                   {"name", name},
                   {"source_revision", sourceRevision},
                   {"pose", pose.map()},
                   {"grounded", grounded},
                   {"hidden", hidden},
                   {"suppressed", suppressed},
                   {"unloaded", unloaded}});
}
Occurrence Occurrence::parse(QCborMap m) {
    wire::keys(m, {"id", "definition", "name", "source_revision", "pose", "grounded", "hidden",
                   "suppressed", "unloaded"});
    Occurrence o;
    o.id = domain::id(wire::text(m, "id"));
    o.definition = domain::id(wire::text(m, "definition"));
    o.name = domain::label(wire::text(m, "name"));
    o.sourceRevision = wire::integer(m, "source_revision");
    o.pose = Pose::parse(m.value("pose").toMap());
    for (auto key : {"grounded", "hidden", "suppressed", "unloaded"})
        check(m.value(QString::fromLatin1(key)).isBool(), "InvalidInput",
              "Explicit occurrence state required");
    o.grounded = m.value("grounded").toBool();
    o.hidden = m.value("hidden").toBool();
    o.suppressed = m.value("suppressed").toBool();
    o.unloaded = m.value("unloaded").toBool();
    return o;
}
QCborMap Reference::map() const {
    return fields({{"occurrence", occurrence},
                   {"body", body},
                   {"token", token},
                   {"path", QCborArray::fromStringList(path)}});
}
Reference Reference::parse(QCborMap m) {
    wire::keys(m, {"occurrence", "body", "token", "path"});
    Reference r;
    r.occurrence = domain::id(wire::text(m, "occurrence"));
    r.body = wire::text(m, "body");
    if (!r.body.isEmpty())
        domain::id(r.body);
    r.token = wire::text(m, "token");
    check(QStringList{"Origin", "XY Plane", "XZ Plane", "YZ Plane", "X Axis", "Y Axis", "Z Axis"}
                  .contains(r.token) ||
              QRegularExpression("^(face|edge)/[0-9a-f]{64}$").match(r.token).hasMatch(),
          "InvalidInput", "Choose a datum or current topology reference");
    check(r.body.isEmpty()
              ? QStringList{"Origin", "XY Plane", "XZ Plane", "YZ Plane", "X Axis", "Y Axis",
                            "Z Axis"}
                    .contains(r.token)
              : QRegularExpression("^(face|edge)/[0-9a-f]{64}$").match(r.token).hasMatch(),
          "InvalidInput", "Topology references require a body; datums have no body");
    check(m.value("path").isArray() && m.value("path").toArray().size() <= 8, "ResourceLimit",
          "Occurrence path admission exceeded");
    for (auto id : m.value("path").toArray())
        r.path.append(domain::id(id.toString()));
    return r;
}
QCborMap Mate::map() const {
    return fields({{"id", id},
                   {"name", name},
                   {"kind", kind},
                   {"alignment", alignment},
                   {"first", first.map()},
                   {"second", second.map()},
                   {"value", value},
                   {"suppressed", suppressed}});
}
Mate Mate::parse(QCborMap m) {
    wire::keys(m, {"id", "name", "kind", "alignment", "first", "second", "value", "suppressed"});
    Mate a;
    a.id = domain::id(wire::text(m, "id"));
    a.name = domain::label(wire::text(m, "name"));
    a.kind = wire::text(m, "kind");
    a.alignment = wire::text(m, "alignment");
    check(QStringList{"Coincident", "Concentric", "Parallel", "Perpendicular", "Distance", "Angle"}
              .contains(a.kind),
          "UnsupportedOperation", "Unsupported rigid mate");
    check(a.alignment == "Aligned" || a.alignment == "Opposed", "InvalidInput",
          "Choose mate alignment");
    a.first = Reference::parse(m.value("first").toMap());
    a.second = Reference::parse(m.value("second").toMap());
    check(a.first.occurrence != a.second.occurrence, "InvalidInput",
          "A mate needs distinct root occurrences");
    a.value = features::number(m, "value", -1000, 1000);
    check(a.kind != "Angle" || (a.value >= 0 && a.value <= std::numbers::pi), "InvalidInput",
          "Mate angle must be 0–pi radians");
    check(m.value("suppressed").isBool(), "InvalidInput", "Explicit mate suppression required");
    a.suppressed = m.value("suppressed").toBool();
    return a;
}
QCborMap Model::map() const {
    QCborArray o, m;
    for (auto &v : occurrences)
        o.append(v.map());
    for (auto &v : mates)
        m.append(v.map());
    return fields({{"version", 1}, {"occurrences", o}, {"mates", m}});
}
Model Model::parse(QCborMap m) {
    wire::keys(m, {"version", "occurrences", "mates"});
    check(wire::integer(m, "version") == 1, "IncompatibleSchema", "Unsupported assembly schema");
    check(m.value("occurrences").isArray() && m.value("mates").isArray(), "InvalidInput",
          "Assembly arrays required");
    auto os = m.value("occurrences").toArray(), ms = m.value("mates").toArray();
    check(os.size() <= 100 && ms.size() <= 150 && QCborValue(m).toCbor().size() <= 262144,
          "ResourceLimit", "Assembly admission is 100 root occurrences / 150 mates / 256 KiB");
    Model out;
    QSet<QString> ids;
    for (auto v : os) {
        auto o = Occurrence::parse(v.toMap());
        check(!ids.contains(o.id), "InvalidInput", "Duplicate occurrence identity");
        ids.insert(o.id);
        out.occurrences.push_back(o);
    }
    for (auto v : ms) {
        auto a = Mate::parse(v.toMap());
        check(!ids.contains(a.id), "InvalidInput", "Duplicate mate identity");
        ids.insert(a.id);
        out.mates.push_back(a);
    }
    return out;
}
void validateGraph(const Definitions &definitions) {
    QSet<QString> visiting;
    QMap<QString, int> heights, leaves;
    std::function<int(QString)> visit = [&](QString id) {
        check(!visiting.contains(id), "DependencyCycle", "Occurrence/subassembly cycle");
        if (heights.contains(id))
            return heights[id];
        if (!definitions.contains(id))
            return 0;
        visiting.insert(id);
        int height = 0, count = 0;
        auto d = definitions[id];
        if (d.first == "Part")
            count = 1;
        if (d.first == "Assembly") {
            for (auto &o : d.second.occurrences) {
                height = qMax(height, 1 + visit(o.definition));
                if (!o.suppressed)
                    count += leaves.value(o.definition);
            }
        }
        check(count <= 100, "ResourceLimit",
              "Flattened assembly admission is 100 leaf occurrences");
        leaves[id] = count;
        check(height <= 8, "ResourceLimit", "Subassembly nesting exceeds eight");
        visiting.remove(id);
        heights[id] = height;
        return height;
    };
    for (auto id : definitions.keys())
        visit(id);
}
QCborMap Solution::map() const {
    return fields({{"valid", valid},
                   {"freedom", freedom},
                   {"rank", rank},
                   {"residual", residual},
                   {"diagnostics", diagnostics},
                   {"assembly", model.map()}});
}
} // namespace cad::assembly
