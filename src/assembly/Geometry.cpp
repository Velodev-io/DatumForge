#include "Geometry.hpp"
#include <BRepAdaptor_Curve.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepAlgoAPI_Common.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepExtrema_DistShapeShape.hxx>
#include <BRepGProp.hxx>
#include <BRep_Builder.hxx>
#include <GProp_GProps.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Compound.hxx>
#include <gp_Quaternion.hxx>
namespace cad::assembly {
using domain::check;
using sketch::fields;
namespace {
Frame datum(QString token) {
    if (token == "Origin")
        return {};
    Frame f;
    f.kind = token.endsWith("Axis") ? "Axis" : "Plane";
    if (token == "X Axis" || token == "YZ Plane")
        f.direction = {1, 0, 0};
    else if (token == "Y Axis" || token == "XZ Plane")
        f.direction = {0, 1, 0};
    return f;
}
Frame exact(const kernel::Part &part, QString body, QString token) {
    check(!part.stale, "StaleResult", "Mate reference requires a current exact definition");
    for (auto &b : part.bodies)
        if (b.id == body)
            for (auto kind : {TopAbs_FACE, TopAbs_EDGE})
                for (TopExp_Explorer it(b.shape, kind); it.More(); it.Next())
                    if (kernel::topologyToken(it.Current()) == token) {
                        Frame f;
                        if (kind == TopAbs_FACE) {
                            BRepAdaptor_Surface s(TopoDS::Face(it.Current()));
                            if (s.GetType() == GeomAbs_Plane) {
                                auto plane = s.Plane();
                                auto p = plane.Location();
                                auto d = plane.Axis().Direction();
                                f = {"Plane",
                                     {p.X() / 1000, p.Y() / 1000, p.Z() / 1000},
                                     {d.X(), d.Y(), d.Z()}};
                                if (it.Current().Orientation() == TopAbs_REVERSED)
                                    for (auto &x : f.direction)
                                        x = -x;
                                return f;
                            }
                            if (s.GetType() == GeomAbs_Cylinder) {
                                auto axis = s.Cylinder().Axis();
                                auto p = axis.Location();
                                auto d = axis.Direction();
                                return {"Axis",
                                        {p.X() / 1000, p.Y() / 1000, p.Z() / 1000},
                                        {d.X(), d.Y(), d.Z()}};
                            }
                        } else {
                            BRepAdaptor_Curve curve(TopoDS::Edge(it.Current()));
                            if (curve.GetType() == GeomAbs_Line) {
                                auto axis = curve.Line();
                                auto p = axis.Location();
                                auto d = axis.Direction();
                                return {"Axis",
                                        {p.X() / 1000, p.Y() / 1000, p.Z() / 1000},
                                        {d.X(), d.Y(), d.Z()}};
                            }
                        }
                        throw domain::Error(
                            "UnsupportedOperation",
                            "Selected topology is not a planar face or straight/cylindrical axis");
                    }
    throw domain::Error("ReferenceMissing",
                        "Mate topology is missing; explicitly select a replacement reference");
}
struct Leaf {
    QString id, definition, name;
    Pose pose;
    bool hidden = false, unloaded = false;
};
std::vector<Leaf> leaves(const Model &model, const Loader &load, QCborArray *states = nullptr) {
    std::vector<Leaf> out;
    QSet<QString> stack;
    std::function<void(const Model &, Pose, QString, bool, bool, int)> walk =
        [&](const Model &m, Pose parent, QString path, bool hidden, bool unloaded, int depth) {
            check(depth <= 8, "ResourceLimit", "Assembly nesting exceeds eight");
            for (auto &o : m.occurrences) {
                auto id = path.isEmpty() ? o.id : path + "/" + o.id;
                QString state = o.suppressed             ? "Suppressed"
                                : o.unloaded || unloaded ? "Unloaded"
                                : o.hidden || hidden     ? "Hidden"
                                                         : "Resolved";
                Definition d;
                try {
                    d = load(o.definition, false);
                    if (d.stale)
                        state = "Stale";
                } catch (const domain::Error &) {
                    state = "Unresolved";
                }
                if (states)
                    states->append(fields({{"id", id},
                                           {"definition", o.definition},
                                           {"name", o.name},
                                           {"status", state},
                                           {"hidden", o.hidden || hidden},
                                           {"suppressed", o.suppressed},
                                           {"unloaded", o.unloaded || unloaded},
                                           {"source_changed", d.revision != o.sourceRevision}}));
                if (o.suppressed || state == "Unresolved")
                    continue;
                auto placement = compose(parent, o.pose);
                if (d.kind == "Assembly") {
                    check(!stack.contains(o.definition), "DependencyCycle", "Assembly cycle");
                    stack.insert(o.definition);
                    walk(d.assembly, placement, id, hidden || o.hidden, unloaded || o.unloaded,
                         depth + 1);
                    stack.remove(o.definition);
                } else if (d.kind == "Part") {
                    check(out.size() < 100, "ResourceLimit",
                          "Flattened assembly admission is 100 leaf occurrences");
                    out.push_back({id, o.definition, o.name, placement, hidden || o.hidden,
                                   unloaded || o.unloaded});
                } else
                    throw domain::Error("UnsupportedOperation",
                                        "Only Part/Assembly definitions can be inserted");
            }
        };
    walk(model, {}, QString(), false, false, 0);
    return out;
}
gp_Trsf transformMM(Pose p) {
    gp_Trsf t;
    t.SetRotation(gp_Quaternion(p.rotation[1], p.rotation[2], p.rotation[3], p.rotation[0]));
    t.SetTranslationPart(
        gp_Vec(p.translation[0] * 1000, p.translation[1] * 1000, p.translation[2] * 1000));
    return t;
}
} // namespace
Frame reference(const Reference &ref, const Model &model, const Loader &load) {
    auto it = std::find_if(model.occurrences.begin(), model.occurrences.end(),
                           [&](auto &o) { return o.id == ref.occurrence; });
    check(it != model.occurrences.end() && !it->suppressed, "ReferenceMissing",
          "Mate occurrence is missing/suppressed");
    check(!it->unloaded, "ReferenceMissing", "Resolve the occurrence before exact mating");
    auto def = it->definition;
    Pose local;
    for (auto child : ref.path) {
        auto d = load(def, false);
        check(d.kind == "Assembly", "ReferenceMissing", "Invalid subassembly path");
        auto found = std::find_if(d.assembly.occurrences.begin(), d.assembly.occurrences.end(),
                                  [&](auto &o) { return o.id == child; });
        check(found != d.assembly.occurrences.end() && !found->suppressed && !found->unloaded,
              "ReferenceMissing", "Subassembly path is missing/suppressed/unloaded");
        local = compose(local, found->pose);
        def = found->definition;
    }
    auto frame =
        ref.body.isEmpty() ? datum(ref.token) : exact(load(def, true).part, ref.body, ref.token);
    frame.origin = transform(local, frame.origin);
    frame.direction = transform(local, frame.direction, true);
    return frame;
}
QCborArray references(QString occurrence, QString definition, const Loader &load) {
    QCborArray out;
    for (auto token :
         {"Origin", "XY Plane", "XZ Plane", "YZ Plane", "X Axis", "Y Axis", "Z Axis"}) {
        Reference r;
        r.occurrence = occurrence;
        r.token = token;
        out.append(fields({{"label", token}, {"reference", r.map()}}));
    }
    auto d = load(definition, true);
    if (d.kind != "Part")
        return out;
    for (auto v : d.part.topology()) {
        auto info = v.toMap();
        Reference r;
        r.occurrence = occurrence;
        r.body = info.value("body").toString();
        r.token = info.value("token").toString();
        try {
            auto frame = exact(d.part, r.body, r.token);
            auto p = frame.origin;
            out.append(fields({{"label", QString("%1 (%2, %3, %4) mm")
                                             .arg(frame.kind)
                                             .arg(p[0] * 1000, 0, 'g', 5)
                                             .arg(p[1] * 1000, 0, 'g', 5)
                                             .arg(p[2] * 1000, 0, 'g', 5)},
                               {"reference", r.map()}}));
        } catch (const domain::Error &) {
        }
    }
    check(out.size() <= 1000, "ResourceLimit", "Mate reference candidate admission exceeded");
    return out;
}
Scene scene(const Model &model, const Loader &load, QMap<QString, DisplayMesh> &cache) {
    Scene out;
    auto all = leaves(model, load, &out.states);
    QMap<QString, QPair<int, int>> triangles, edges;
    QMap<QString, int> quantities;
    for (auto &leaf : all) {
        quantities[leaf.definition]++;
        if (leaf.hidden)
            continue;
        auto d = load(leaf.definition, false);
        auto key = leaf.definition + ":" + QString::number(d.revision);
        if (!cache.contains(key)) {
            if (leaf.unloaded)
                continue;
            auto exactDef = load(leaf.definition, true);
            check(!exactDef.stale, "StaleResult", "Assembly definition is stale");
            cache[key] = exactDef.part.mesh(1.);
        }
        if (!triangles.contains(key)) {
            auto &m = cache[key];
            int firstTriangle = out.display.triangleRoles.size(),
                firstEdge = out.display.edgeIds.size(), vertex = out.display.vertices.size(),
                face = out.display.faceIds.size();
            out.display.vertices += m.vertices;
            out.display.edges += m.edges;
            for (auto index : m.indices)
                out.display.indices.append(index + vertex);
            for (auto role : m.triangleRoles)
                out.display.triangleRoles.append(role + face);
            for (auto id : m.faceIds)
                out.display.faceIds.append(leaf.definition + "/" + id);
            for (auto id : m.edgeIds)
                out.display.edgeIds.append(leaf.definition + "/" + id);
            triangles[key] = {firstTriangle, m.triangleRoles.size()};
            edges[key] = {firstEdge, m.edgeIds.size()};
        }
        auto t = triangles[key], e = edges[key];
        Instance instance;
        instance.id = leaf.id;
        instance.translation = {float(leaf.pose.translation[0] * 1000),
                                float(leaf.pose.translation[1] * 1000),
                                float(leaf.pose.translation[2] * 1000)};
        auto q = leaf.pose.rotation;
        instance.rotation = QQuaternion(float(q[0]), float(q[1]), float(q[2]), float(q[3]));
        instance.firstTriangle = t.first;
        instance.triangleCount = t.second;
        instance.firstEdge = e.first;
        instance.edgeCount = e.second;
        out.display.instances.append(instance);
    }
    for (auto definition : quantities.keys())
        out.bom.append(fields({{"definition", definition},
                               {"quantity", quantities[definition]},
                               {"name", load(definition, false).name}}));
    qint64 used = 0;
    for (auto it = cache.begin(); it != cache.end(); ++it)
        used += it->vertices.size() * 12 + it->indices.size() * 4 + it->edges.size() * 12 +
                it->triangleRoles.size() * 4;
    while ((used > (64LL << 20) || cache.size() > 32) && !cache.empty()) {
        auto m = cache.first();
        used -= m.vertices.size() * 12 + m.indices.size() * 4 + m.edges.size() * 12 +
                m.triangleRoles.size() * 4;
        cache.erase(cache.begin());
    }
    out.meshDefinitions = triangles.size();
    if (!out.display.vertices.empty()) {
        out.display.minimum = out.display.maximum = out.display.vertices.first();
        for (auto list : {out.display.vertices, out.display.edges})
            for (auto p : list)
                for (int k = 0; k < 3; ++k) {
                    out.display.minimum[k] = qMin(out.display.minimum[k], p[k]);
                    out.display.maximum[k] = qMax(out.display.maximum[k], p[k]);
                }
        out.display.validate();
        qint64 visible = 0;
        for (auto &i : out.display.instances)
            visible += i.triangleCount;
        check(visible <= 1000000, "ResourceLimit",
              "Assembly exceeds one million visible triangles");
    }
    for (auto v : out.states)
        out.stale =
            out.stale ||
            (!v.toMap().value("suppressed").toBool() &&
             QStringList{"Unresolved", "Stale"}.contains(v.toMap().value("status").toString()));
    return out;
}
QCborArray interference(const Model &model, const Loader &load) {
    QCborArray out, states;
    auto all = leaves(model, load, &states);
    for (auto v : states)
        check(
            v.toMap().value("suppressed").toBool() ||
                !QStringList{"Stale", "Unresolved"}.contains(v.toMap().value("status").toString()),
            "StaleResult", "Interference requires current resolved subassemblies");
    check(all.size() <= 100, "ResourceLimit", "Interference leaf admission exceeded");
    QMap<QString, TopoDS_Shape> shapes;
    for (auto &leaf : all) {
        check(!leaf.unloaded, "ReferenceMissing", "Resolve components before exact interference");
        auto d = load(leaf.definition, true);
        check(!d.stale, "StaleResult", "Exact interference refuses stale geometry");
        TopoDS_Compound compound;
        BRep_Builder builder;
        builder.MakeCompound(compound);
        for (auto &b : d.part.bodies) {
            check(!b.shape.IsNull() && BRepCheck_Analyzer(b.shape).IsValid() &&
                      TopExp_Explorer(b.shape, TopAbs_SOLID).More(),
                  "UnsupportedOperation", "Interference requires valid solid bodies");
            GProp_GProps properties;
            BRepGProp::VolumeProperties(b.shape, properties);
            check(std::isfinite(properties.Mass()) && properties.Mass() > 0, "UnsupportedOperation",
                  "Interference requires positive solid volume");
            builder.Add(compound, b.shape);
        }
        check(!d.part.bodies.empty(), "UnsupportedOperation", "Component has no solid bodies");
        shapes[leaf.id] = BRepBuilderAPI_Transform(compound, transformMM(leaf.pose), true).Shape();
    }
    for (size_t i = 0; i < all.size(); ++i)
        for (size_t j = i + 1; j < all.size(); ++j) {
            auto a = all[i].id, b = all[j].id;
            QString status;
            double volume = 0;
            try {
                BRepAlgoAPI_Common common(shapes[a], shapes[b]);
                check(common.IsDone() && !common.HasErrors(), "InvalidGeometry",
                      "Intersection evaluation failed");
                GProp_GProps props;
                BRepGProp::VolumeProperties(common.Shape(), props);
                volume = props.Mass() * 1e-9;
                check(std::isfinite(volume) && volume >= 0 &&
                          BRepCheck_Analyzer(common.Shape()).IsValid(),
                      "InvalidGeometry", "Invalid intersection result");
                if (volume > 1e-15)
                    status = "Overlap";
                else {
                    BRepExtrema_DistShapeShape distance(shapes[a], shapes[b]);
                    check(distance.IsDone(), "InvalidGeometry", "Separation query failed");
                    status = distance.Value() <= 1e-6 ? "Touching" : "Disjoint";
                }
            } catch (const Standard_Failure &) {
                status = "Failed";
            } catch (const domain::Error &) {
                status = "Failed";
            }
            out.append(
                fields({{"first", a}, {"second", b}, {"status", status}, {"volume_m3", volume}}));
        }
    check(QCborValue(out).toCbor().size() < 524288, "ResourceLimit",
          "Interference report exceeds control budget");
    return out;
}
} // namespace cad::assembly
