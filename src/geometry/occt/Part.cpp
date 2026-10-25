#include "Part.hpp"
#include "SketchCurve.hpp"
#include "evaluation/Plan.hpp"
#include "sketch/Curves.hpp"
#include <BRepAdaptor_Curve.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepAlgoAPI_Check.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepAlgoAPI_Fuse.hxx>
#include <BRepBndLib.hxx>
#include <BRepBuilderAPI_Copy.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepClass_FaceClassifier.hxx>
#include <BRepExtrema_DistShapeShape.hxx>
#include <BRepFilletAPI_MakeChamfer.hxx>
#include <BRepFilletAPI_MakeFillet.hxx>
#include <BRepGProp.hxx>
#include <BRepMesh_IncrementalMesh.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <BRepPrimAPI_MakeRevol.hxx>
#include <BRepTools.hxx>
#include <BRep_Builder.hxx>
#include <BRep_Tool.hxx>
#include <GProp_GProps.hxx>
#include <Geom_Circle.hxx>
#include <QElapsedTimer>
#include <Standard_Failure.hxx>
#include <TColStd_Array1OfInteger.hxx>
#include <TColStd_Array1OfReal.hxx>
#include <TColgp_Array1OfPnt.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopTools_IndexedDataMapOfShapeListOfShape.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopoDS.hxx>
#include <gp_Elips.hxx>
#include <gp_Parab.hxx>
#include <gp_Pln.hxx>
#include <numbers>
namespace cad::kernel {
using domain::check;
using sketch::fields;
namespace {
constexpr double mm = 1000, tol = 1e-7; // millimeters; no automatic tolerance inflation
constexpr double siVolume = 1e-9, siArea = 1e-6;
gp_Pnt point(sketch::Vec3 v) {
    return {v[0] * mm, v[1] * mm, v[2] * mm};
}
gp_Dir direction(sketch::Vec3 v) {
    return {v[0], v[1], v[2]};
}
double volume(const TopoDS_Shape &s) {
    GProp_GProps p;
    BRepGProp::VolumeProperties(s, p);
    return p.Mass();
}
int count(const TopoDS_Shape &s, TopAbs_ShapeEnum kind) {
    TopTools_IndexedMapOfShape m;
    TopExp::MapShapes(s, kind, m);
    return m.Extent();
}
TopoDS_Shape compound(const std::vector<TopoDS_Shape> &shapes) {
    TopoDS_Compound c;
    BRep_Builder b;
    b.MakeCompound(c);
    for (auto &s : shapes)
        b.Add(c, s);
    return c;
}
void valid(const TopoDS_Shape &s, int faceLimit = 20000) {
    check(!s.IsNull() && BRepCheck_Analyzer(s, true).IsValid(), "InvalidGeometry",
          "Kernel produced an invalid shape");
    check(count(s, TopAbs_SOLID) > 0 && count(s, TopAbs_SOLID) <= 10, "ResourceLimit",
          "Result must contain 1–10 closed solids");
    check(count(s, TopAbs_FACE) <= faceLimit && std::isfinite(volume(s)) && volume(s) > 1e-15,
          "InvalidGeometry", "Empty, inverted or oversized solid result");
    for (TopExp_Explorer i(s, TopAbs_SHELL); i.More(); i.Next())
        check(BRep_Tool::IsClosed(i.Current()), "InvalidGeometry", "Solid shell is open");
    for (TopExp_Explorer i(s, TopAbs_VERTEX); i.More(); i.Next())
        check(BRep_Tool::Tolerance(TopoDS::Vertex(i.Current())) <= 1e-4, "InvalidGeometry",
              "Tolerance exceeds declared 0.0001 mm ceiling");
    for (TopExp_Explorer i(s, TopAbs_EDGE); i.More(); i.Next())
        check(BRep_Tool::Tolerance(TopoDS::Edge(i.Current())) <= 1e-4, "InvalidGeometry",
              "Edge tolerance growth exceeds policy");
    for (TopExp_Explorer i(s, TopAbs_FACE); i.More(); i.Next())
        check(BRep_Tool::Tolerance(TopoDS::Face(i.Current())) <= 1e-4, "InvalidGeometry",
              "Face tolerance growth exceeds policy");
}
QString token(const TopoDS_Shape &s) {
    GProp_GProps g;
    QByteArray bytes;
    if (s.ShapeType() == TopAbs_EDGE) {
        auto e = TopoDS::Edge(s);
        BRepAdaptor_Curve c(e);
        BRepGProp::LinearProperties(e, g);
        bytes += QByteArray::number(int(c.GetType()));
        std::vector<QString> samples;
        for (double t : {0., .25, .5, .75, 1.}) {
            auto p = c.Value(c.FirstParameter() + (c.LastParameter() - c.FirstParameter()) * t);
            samples.push_back(QString("%1,%2,%3")
                                  .arg(p.X(), 0, 'f', 7)
                                  .arg(p.Y(), 0, 'f', 7)
                                  .arg(p.Z(), 0, 'f', 7));
        }
        // Orientation-independent signatures; ambiguous signatures are never guessed.
        auto reversed = samples;
        std::reverse(reversed.begin(), reversed.end());
        bytes = QByteArray::number(int(c.GetType())) +
                qMin(QStringList(samples.begin(), samples.end()).join(';').toUtf8(),
                     QStringList(reversed.begin(), reversed.end()).join(';').toUtf8());
    } else {
        BRepGProp::SurfaceProperties(s, g);
        auto p = g.CentreOfMass();
        BRepAdaptor_Surface a(TopoDS::Face(s));
        bytes = QString("%1/%2/%3/%4")
                    .arg(int(a.GetType()))
                    .arg(p.X(), 0, 'f', 7)
                    .arg(p.Y(), 0, 'f', 7)
                    .arg(p.Z(), 0, 'f', 7)
                    .toUtf8();
    }
    bytes += '/' + QByteArray::number(g.Mass(), 'g', 15);
    return (s.ShapeType() == TopAbs_EDGE ? "edge/" : "face/") +
           QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex());
}
QMap<QString, TopoDS_Shape> subshapes(const TopoDS_Shape &s, TopAbs_ShapeEnum kind) {
    QMap<QString, TopoDS_Shape> out;
    QSet<QString> ambiguous;
    TopTools_IndexedMapOfShape m;
    TopExp::MapShapes(s, kind, m);
    for (int i = 1; i <= m.Extent(); ++i) {
        auto k = token(m(i));
        if (out.contains(k))
            ambiguous.insert(k);
        else
            out.insert(k, m(i));
    }
    for (auto k : ambiguous)
        out.remove(k);
    return out;
}
QCborArray tokens(const TopTools_ListOfShape &list) {
    QCborArray a;
    for (auto &s : list)
        if (s.ShapeType() == TopAbs_FACE || s.ShapeType() == TopAbs_EDGE)
            a.append(token(s));
    return a;
}
void recordHistory(Part &part, QString id, BRepBuilderAPI_MakeShape &op,
                   const TopoDS_Shape &input) {
    for (auto kind : {TopAbs_FACE, TopAbs_EDGE}) {
        auto shapes = subshapes(input, kind);
        for (auto it = shapes.cbegin(); it != shapes.cend(); ++it) {
            auto gen = tokens(op.Generated(it.value())), mod = tokens(op.Modified(it.value()));
            bool deleted = op.IsDeleted(it.value());
            if (gen.empty() && !deleted && mod.size() == 1 && mod[0].toString() == it.key())
                continue;
            if (!gen.empty() || !mod.empty() || deleted)
                part.history.append(fields({{"feature", id},
                                            {"input", it.key()},
                                            {"generated", gen},
                                            {"modified", mod},
                                            {"deleted", deleted}}));
        }
    }
    check(QCborValue(part.history).toCbor().size() <= 262144, "ResourceLimit",
          "Topology history projection exceeds 256 KiB");
}
TopoDS_Shape transform(const TopoDS_Shape &s, const gp_Trsf &t) {
    return BRepBuilderAPI_Transform(s, t, true).Shape();
}
TopoDS_Shape profiles(const sketch::Sketch &s) {
    s.validate();
    auto u = direction(s.frame.u), v = direction(s.frame.v);
    auto normal = gp_Dir(gp_Vec(u).Crossed(gp_Vec(v)));
    gp_Pln plane(point(s.frame.origin), normal);
    struct Segment {
        TopoDS_Edge edge;
        gp_Pnt a, b;
        bool used = false;
    };
    std::vector<Segment> segments;
    std::vector<TopoDS_Wire> wires;
    for (auto &e : s.entities) {
        if (e.construction || e.kind == "Point")
            continue;
        auto exact = geometry::sketchCurve(s, e);
        auto edge = BRepBuilderAPI_MakeEdge(exact.curve, exact.first, exact.last).Edge();
        if (e.kind == "Circle" || e.kind == "Ellipse")
            wires.push_back(BRepBuilderAPI_MakeWire(edge).Wire());
        else
            segments.push_back(
                {edge, exact.curve->Value(exact.first), exact.curve->Value(exact.last)});
    }
    for (auto &first : segments) {
        if (first.used)
            continue;
        BRepBuilderAPI_MakeWire w;
        w.Add(first.edge);
        first.used = true;
        auto end = first.b;
        int guard = 0;
        while (end.Distance(first.a) > tol) {
            Segment *next = nullptr;
            bool reverse = false;
            for (auto &e : segments)
                if (!e.used && (e.a.Distance(end) <= tol || e.b.Distance(end) <= tol)) {
                    check(!next, "InvalidGeometry",
                          "Branching profile; choose nonintersecting closed loops");
                    next = &e;
                    reverse = e.b.Distance(end) <= tol;
                }
            check(next && ++guard <= 256, "InvalidGeometry", "Profile is open or disconnected");
            auto edge = next->edge;
            if (reverse)
                edge.Reverse();
            w.Add(edge);
            end = reverse ? next->a : next->b;
            next->used = true;
        }
        check(w.IsDone(), "InvalidGeometry", "Closed wire construction failed");
        wires.push_back(w.Wire());
    }
    check(!wires.empty() && wires.size() <= 16, "InvalidGeometry",
          "Select 1–16 closed nonconstruction profiles");
    std::vector<TopoDS_Face> faces;
    for (auto &w : wires) {
        BRepBuilderAPI_MakeFace f(plane, w, true);
        check(f.IsDone() && BRepCheck_Analyzer(f.Face()).IsValid() &&
                  BRepAlgoAPI_Check(f.Face()).IsValid(),
              "InvalidGeometry", "Profile self-intersects or is degenerate");
        faces.push_back(f.Face());
    }
    for (size_t i = 0; i < wires.size(); ++i)
        for (size_t j = i + 1; j < wires.size(); ++j) {
            BRepExtrema_DistShapeShape distance(wires[i], wires[j]);
            check(distance.IsDone() && distance.Value() > tol, "InvalidGeometry",
                  "Profile boundaries intersect or touch");
        }
    // Classify nesting with a point on each boundary, rejecting touching/intersecting loops.
    std::vector<int> parent(wires.size(), -1), depth(wires.size(), 0);
    for (size_t i = 0; i < wires.size(); ++i) {
        TopExp_Explorer e(wires[i], TopAbs_VERTEX);
        check(e.More(), "InvalidGeometry", "Profile has no vertices");
        auto p = BRep_Tool::Pnt(TopoDS::Vertex(e.Current()));
        for (size_t j = 0; j < wires.size(); ++j)
            if (i != j) {
                BRepClass_FaceClassifier c(faces[j], p, tol);
                check(c.State() != TopAbs_ON, "InvalidGeometry",
                      "Profiles touch; separate their boundaries");
                if (c.State() == TopAbs_IN) {
                    ++depth[i];
                    parent[i] = int(j);
                }
            }
        check(depth[i] <= 1, "UnsupportedOperation",
              "Nested islands deeper than one hole level are unsupported");
    }
    std::vector<TopoDS_Shape> result;
    for (size_t i = 0; i < wires.size(); ++i)
        if (depth[i] == 0) {
            BRepBuilderAPI_MakeFace f(plane, wires[i], true);
            for (size_t j = 0; j < wires.size(); ++j)
                if (parent[j] == int(i)) {
                    auto hole = wires[j];
                    hole.Reverse();
                    f.Add(hole);
                }
            check(f.IsDone() && BRepCheck_Analyzer(f.Face()).IsValid() &&
                      BRepAlgoAPI_Check(f.Face()).IsValid(),
                  "InvalidGeometry", "Invalid or overlapping profile loops");
            result.push_back(f.Face());
        }
    return result.size() == 1 ? result[0] : compound(result);
}
} // namespace
Part Part::build(const QCborArray &sketches, const QCborArray &definitions, ResultCache *cache,
                 bool allowFailure, const SourceResolver &sources) {
    Part out;
    auto all = sketch::parse(sketches);
    evaluation::Plan plan(sketches, definitions);
    QSet<QString> failed;
    QMap<QString, sketch::Sketch> sketchesById;
    for (auto &s : all)
        sketchesById.insert(s.id, s);
    auto body = [&](QString id) -> Body & {
        for (auto &b : out.bodies)
            if (b.id == id)
                return b;
        throw domain::Error("ReferenceMissing", "Target body is missing: " + id);
    };
    try {
        for (auto &node : plan.nodes) {
            auto f = node.feature;
            if (f.suppressed) {
                out.diagnostics.append(fields({{"id", f.id},
                                               {"status", "Suppressed"},
                                               {"valid", true},
                                               {"reused", false},
                                               {"milliseconds", 0.}}));
                continue;
            }
            bool blocked = false;
            for (auto dep : node.dependencies)
                blocked = blocked || failed.contains(dep);
            auto before = out;
            try {
                check(!blocked, "ReferenceMissing", "An upstream result is unavailable");
                Effect cached;
                if (cache && cache->get(node.fingerprint, cached)) {
                    for (auto &replacement : cached.bodies) {
                        auto found = std::find_if(out.bodies.begin(), out.bodies.end(),
                                                  [&](auto &b) { return b.id == replacement.id; });
                        if (found == out.bodies.end())
                            out.bodies.push_back(replacement);
                        else
                            *found = replacement;
                    }
                    if (!cached.stage.IsNull())
                        out.stages[f.id] = cached.stage;
                    if (!cached.tool.IsNull())
                        out.tools[f.id] = cached.tool;
                    for (auto row : cached.history)
                        out.history.append(row);
                    out.diagnostics.append(fields({{"id", f.id},
                                                   {"status", "Current"},
                                                   {"valid", true},
                                                   {"reused", true},
                                                   {"milliseconds", 0.}}));
                    continue;
                }
                if (cache)
                    ++cache->computations;
                QElapsedTimer clock;
                clock.start();
                auto &p = f.parameters;
                TopoDS_Shape tool;
                QString operation = p.value("operation").toString();
                auto target = p.value("target").toString();
                int createdBodies = 0;
                auto publish = [&](const TopoDS_Shape &shape, QString op) {
                    valid(shape, f.kind == "Imported" ? 50000 : 20000);
                    if (op == "NewBody") {
                        for (TopExp_Explorer i(shape, TopAbs_SOLID); i.More(); i.Next()) {
                            auto id = createdBodies == 0
                                          ? f.id
                                          : QUuid::createUuidV5(QUuid(f.id),
                                                                QByteArray::number(createdBodies))
                                                .toString(QUuid::WithoutBraces);
                            ++createdBodies;
                            out.bodies.push_back({id, i.Current()});
                        }
                    } else {
                        auto &b = body(target);
                        double before = volume(b.shape);
                        if (op == "Cut") {
                            BRepAlgoAPI_Cut cut;
                            TopTools_ListOfShape arguments, tools;
                            arguments.Append(b.shape);
                            tools.Append(shape);
                            cut.SetArguments(arguments);
                            cut.SetTools(tools);
                            cut.SetNonDestructive(true);
                            cut.Build();
                            check(cut.IsDone() && !cut.HasErrors(), "InvalidGeometry",
                                  "Cut failed");
                            valid(cut.Shape());
                            check(count(cut.Shape(), TopAbs_SOLID) == 1 &&
                                      volume(cut.Shape()) < before - 1e-9,
                                  "InvalidGeometry",
                                  "Cut misses target, removes whole body or splits it");
                            recordHistory(out, f.id, cut, b.shape);
                            recordHistory(out, f.id, cut, shape);
                            b.shape = cut.Shape();
                        } else {
                            BRepAlgoAPI_Fuse fuse;
                            TopTools_ListOfShape arguments, tools;
                            arguments.Append(b.shape);
                            tools.Append(shape);
                            fuse.SetArguments(arguments);
                            fuse.SetTools(tools);
                            fuse.SetNonDestructive(true);
                            fuse.Build();
                            check(fuse.IsDone() && !fuse.HasErrors(), "InvalidGeometry",
                                  "Add/fuse failed");
                            valid(fuse.Shape());
                            check(count(fuse.Shape(), TopAbs_SOLID) == 1 &&
                                      volume(fuse.Shape()) > before + 1e-9,
                                  "InvalidGeometry",
                                  "Add must join target and contribute positive volume");
                            recordHistory(out, f.id, fuse, b.shape);
                            recordHistory(out, f.id, fuse, shape);
                            b.shape = fuse.Shape();
                        }
                    }
                };
                if (f.kind == "Imported") {
                    check(bool(sources), "ReferenceMissing",
                          "Stored imported source resolver is required");
                    tool = sources(f);
                    publish(tool, "NewBody");
                } else if (f.kind == "Extrude" || f.kind == "Revolve") {
                    auto id = p.value("sketch").toString();
                    check(sketchesById.contains(id), "ReferenceMissing",
                          "Feature profile sketch is missing");
                    auto s = sketchesById.value(id);
                    auto profile = profiles(s);
                    auto normal =
                        gp_Vec(direction(s.frame.u)).Crossed(gp_Vec(direction(s.frame.v)));
                    if (f.kind == "Extrude") {
                        double d = p.value("depth").toDouble() * mm;
                        auto extent = p.value("extent").toString();
                        if (extent == "Reverse")
                            d = -d;
                        if (extent == "Symmetric") {
                            gp_Trsf t;
                            t.SetTranslation(normal * (-d / 2));
                            profile = transform(profile, t);
                        }
                        BRepPrimAPI_MakePrism prism(profile, normal * d);
                        check(prism.IsDone(), "InvalidGeometry", "Extrusion failed");
                        tool = prism.Shape();
                        recordHistory(out, f.id, prism, profile);
                    } else {
                        auto origin = features::vector(p, "axis_origin"),
                             axis = features::vector(p, "axis_direction", true);
                        check(std::abs(normal.Dot(gp_Vec(direction(axis)))) < 1e-10 &&
                                  std::abs(normal.Dot(
                                      gp_Vec(point(s.frame.origin), point(origin)))) <= tol,
                              "InvalidInput", "Revolve axis must lie in the profile plane");
                        double angle = p.value("angle").toDouble() *
                                       (p.value("direction").toString() == "Reverse" ? -1 : 1);
                        BRepPrimAPI_MakeRevol revolve(
                            profile, gp_Ax1(point(origin), direction(axis)), angle, true);
                        check(revolve.IsDone(), "InvalidGeometry",
                              "Revolve failed or crosses its axis");
                        tool = revolve.Shape();
                        recordHistory(out, f.id, revolve, profile);
                        check(BRepAlgoAPI_Check(tool).IsValid(), "InvalidGeometry",
                              "Self-intersecting revolve");
                    }
                    publish(tool, operation);
                } else if (f.kind == "Fillet" || f.kind == "Chamfer") {
                    auto &b = body(target);
                    auto source = p.value("source").toString();
                    bool current =
                        out.stages.contains(source) && out.stages.value(source).IsSame(b.shape);
                    if (out.stages.contains(source))
                        for (TopExp_Explorer i(out.stages.value(source), TopAbs_SOLID); i.More();
                             i.Next())
                            current = current || i.Current().IsSame(b.shape);
                    check(current, "ReferenceMissing",
                          "Edge selection requires the current target result; repair is owned by "
                          "Module 5");
                    auto edges = subshapes(b.shape, TopAbs_EDGE);
                    double size = p.value("size").toDouble() * mm;
                    std::unique_ptr<BRepBuilderAPI_MakeShape> op;
                    if (f.kind == "Fillet") {
                        auto fillet = std::make_unique<BRepFilletAPI_MakeFillet>(b.shape);
                        for (auto e : p.value("edges").toArray()) {
                            check(edges.contains(e.toString()), "ReferenceMissing",
                                  "Selected edge is missing or ambiguous");
                            fillet->Add(size, TopoDS::Edge(edges.value(e.toString())));
                        }
                        fillet->Build();
                        op = std::move(fillet);
                    } else {
                        auto chamfer = std::make_unique<BRepFilletAPI_MakeChamfer>(b.shape);
                        for (auto e : p.value("edges").toArray()) {
                            check(edges.contains(e.toString()), "ReferenceMissing",
                                  "Selected edge is missing or ambiguous");
                            chamfer->Add(size, TopoDS::Edge(edges.value(e.toString())));
                        }
                        chamfer->Build();
                        op = std::move(chamfer);
                    }
                    check(op->IsDone(), "InvalidGeometry",
                          f.kind + " failed; reduce size or choose supported edges");
                    valid(op->Shape());
                    recordHistory(out, f.id, *op, b.shape);
                    b.shape = op->Shape();
                } else if (f.kind == "Hole") {
                    auto &b = body(target);
                    auto origin = point(features::vector(p, "origin"));
                    auto axis = direction(features::vector(p, "direction", true));
                    bool onPlane = false;
                    for (TopExp_Explorer i(b.shape, TopAbs_FACE); i.More(); i.Next()) {
                        BRepAdaptor_Surface surface(TopoDS::Face(i.Current()));
                        if (surface.GetType() != GeomAbs_Plane)
                            continue;
                        auto plane = surface.Plane();
                        BRepClass_FaceClassifier c(TopoDS::Face(i.Current()), origin, tol);
                        if (plane.Distance(origin) <= tol && c.State() == TopAbs_IN &&
                            std::abs(plane.Axis().Direction().Dot(axis)) > 1 - 1e-10)
                            onPlane = true;
                    }
                    check(onPlane, "InvalidInput",
                          "Hole center must be inside a planar target face, normal to its support");
                    double depth = p.value("depth").toDouble() * mm;
                    if (p.value("through").toBool()) {
                        Bnd_Box bounds;
                        BRepBndLib::AddOptimal(b.shape, bounds, false, false);
                        double a, c, d, e, g, h;
                        bounds.Get(a, c, d, e, g, h);
                        depth = 2 * (gp_Pnt(a, c, d).Distance(gp_Pnt(e, g, h)) +
                                     origin.Distance(gp_Pnt(a, c, d)) + 1);
                        origin.Translate(gp_Vec(axis) * (-depth / 2));
                    } else
                        origin.Translate(gp_Vec(axis) * (-tol * 10));
                    tool = BRepPrimAPI_MakeCylinder(
                               gp_Ax2(origin, axis), p.value("diameter").toDouble() * mm / 2,
                               depth + (p.value("through").toBool() ? 0 : tol * 10))
                               .Shape();
                    publish(tool, "Cut");
                } else {
                    auto source = p.value("source").toString();
                    check(out.stages.contains(source), "ReferenceMissing",
                          "Pattern/mirror source must be an earlier feature");
                    auto seed =
                        operation == "Cut" ? out.tools.value(source) : out.stages.value(source);
                    check(!seed.IsNull(), "UnsupportedOperation",
                          "Cut patterns require an earlier volumetric tool feature");
                    std::vector<TopoDS_Shape> copies;
                    int n = f.kind == "Mirror" ? 2 : int(p.value("count").toInteger());
                    for (int i = 1; i < n; ++i) {
                        gp_Trsf t;
                        if (f.kind == "LinearPattern")
                            t.SetTranslation(
                                gp_Vec(direction(features::vector(p, "direction", true))) *
                                (p.value("spacing").toDouble() * mm * i));
                        else if (f.kind == "CircularPattern") {
                            double angle = p.value("distribution").toString() == "Full"
                                               ? 2 * std::numbers::pi / n
                                               : p.value("angle").toDouble();
                            check(angle * i < 2 * std::numbers::pi - 1e-10, "InvalidInput",
                                  "Circular pattern duplicates seed or wraps more than one turn");
                            t.SetRotation(
                                gp_Ax1(point(features::vector(p, "axis_origin")),
                                       direction(features::vector(p, "axis_direction", true))),
                                angle * i);
                        } else
                            t.SetMirror(gp_Ax2(point(features::vector(p, "origin")),
                                               direction(features::vector(p, "normal", true))));
                        BRepBuilderAPI_Transform moved(seed, t, true);
                        auto copy = moved.Shape();
                        recordHistory(out, f.id, moved, seed);
                        copies.push_back(copy);
                        publish(copy, operation);
                    }
                    if (!copies.empty())
                        tool = compound(copies);
                    else {
                        out.stages.insert(f.id, out.stages.value(source));
                        if (out.tools.contains(source))
                            out.tools.insert(f.id, out.tools.value(source));
                    }
                }
                check(out.bodies.size() <= 10, "ResourceLimit", "Part admits at most ten bodies");
                int solids = 0;
                for (auto &b : out.bodies)
                    solids += count(b.shape, TopAbs_SOLID);
                check(solids <= 10, "ResourceLimit", "Part admits at most ten solids");
                if (!tool.IsNull())
                    out.tools.insert(f.id, tool);
                if (operation == "NewBody" && !tool.IsNull())
                    out.stages.insert(f.id, tool);
                else if (!target.isEmpty())
                    out.stages.insert(f.id, body(target).shape);
                Effect effect;
                effect.stage = out.stages.value(f.id);
                effect.tool = out.tools.value(f.id);
                for (auto &b : out.bodies) {
                    auto old = std::find_if(before.bodies.begin(), before.bodies.end(),
                                            [&](auto &v) { return v.id == b.id; });
                    if (old == before.bodies.end() || !old->shape.IsSame(b.shape))
                        effect.bodies.push_back(b);
                }
                for (qsizetype i = before.history.size(); i < out.history.size(); ++i)
                    effect.history.append(out.history[i]);
                if (cache)
                    cache->put(node.fingerprint, effect);
                out.diagnostics.append(fields({{"id", f.id},
                                               {"valid", true},
                                               {"milliseconds", clock.nsecsElapsed() / 1e6},
                                               {"solids", solids},
                                               {"tolerance_mm", 1e-4},
                                               {"reused", false},
                                               {"status", "Current"}}));
            } catch (const Standard_Failure &e) {
                if (!allowFailure)
                    throw;
                out = before;
                out.stale = true;
                failed.insert(f.id);
                out.errors.append(fields({{"owner", f.id},
                                          {"code", "InvalidGeometry"},
                                          {"message", QString::fromUtf8(e.GetMessageString())}}));
            } catch (const domain::Error &e) {
                if (!allowFailure)
                    throw;
                out = before;
                out.stale = true;
                failed.insert(f.id);
                out.errors.append(fields(
                    {{"owner", f.id}, {"code", e.code}, {"message", QString::fromUtf8(e.what())}}));
            }
        }
    } catch (const Standard_Failure &e) {
        throw domain::Error("InvalidGeometry", QString::fromUtf8(e.GetMessageString()));
    }
    return out;
}
QCborArray Part::topology(QString body) const {
    QCborArray a;
    for (auto &b : bodies)
        if (body.isEmpty() || b.id == body) {
            for (auto kind : {TopAbs_EDGE, TopAbs_FACE}) {
                auto m = subshapes(b.shape, kind);
                for (auto it = m.cbegin(); it != m.cend(); ++it) {
                    GProp_GProps g;
                    if (kind == TopAbs_EDGE)
                        BRepGProp::LinearProperties(it.value(), g);
                    else
                        BRepGProp::SurfaceProperties(it.value(), g);
                    auto p = g.CentreOfMass();
                    a.append(fields(
                        {{"body", b.id},
                         {"token", it.key()},
                         {"center_si", QCborArray{p.X() / mm, p.Y() / mm, p.Z() / mm}},
                         {"measure_si", g.Mass() * (kind == TopAbs_EDGE ? 1 / mm : siArea)}}));
                }
            }
        }
    check(QCborValue(a).toCbor().size() <= 262144, "ResourceLimit",
          "Topology projection exceeds 256 KiB");
    return a;
}
QCborMap Part::measure(qint64 revision, QString body, QString selected, double density,
                       QString second) const {
    check(!stale, "StaleResult", "Exact measurement requires a current result");
    check(revision >= 0 && std::isfinite(density) && density >= 0 && density <= 1e6, "InvalidInput",
          "Invalid measurement revision or density (kg/m³)");
    std::vector<TopoDS_Shape> shapes;
    for (auto &b : bodies)
        if (body.isEmpty() || body == b.id)
            shapes.push_back(b.shape);
    check(!shapes.empty(), "ReferenceMissing", "Measurement body is missing");
    auto s = compound(shapes);
    if (!selected.isEmpty()) {
        auto edges = subshapes(s, TopAbs_EDGE), faces = subshapes(s, TopAbs_FACE);
        if (edges.contains(selected)) {
            auto e = TopoDS::Edge(edges.value(selected));
            BRepAdaptor_Curve c(e);
            GProp_GProps g;
            BRepGProp::LinearProperties(e, g);
            auto result =
                fields({{"revision", revision}, {"length_m", g.Mass() / mm}, {"valid", true}});
            if (c.GetType() == GeomAbs_Circle)
                result.insert(QStringLiteral("radius_m"), c.Circle().Radius() / mm);
            if (!second.isEmpty()) {
                check(edges.contains(second), "ReferenceMissing",
                      "Second edge missing or ambiguous");
                BRepAdaptor_Curve other(TopoDS::Edge(edges.value(second)));
                check(c.GetType() == GeomAbs_Line && other.GetType() == GeomAbs_Line,
                      "UnsupportedOperation", "Angle measurement supports two straight edges");
                result.insert(QStringLiteral("angle_rad"),
                              c.Line().Direction().Angle(other.Line().Direction()));
            }
            return result;
        }
        check(faces.contains(selected), "ReferenceMissing",
              "Measurement topology missing or ambiguous");
        s = faces.value(selected);
        GProp_GProps g;
        BRepGProp::SurfaceProperties(s, g);
        auto r = fields({{"revision", revision}, {"area_m2", g.Mass() * siArea}, {"valid", true}});
        BRepAdaptor_Surface surf(TopoDS::Face(s));
        if (surf.GetType() == GeomAbs_Cylinder)
            r.insert(QStringLiteral("radius_m"), surf.Cylinder().Radius() / mm);
        return r;
    }
    Bnd_Box box;
    BRepBndLib::AddOptimal(s, box, false, false);
    double x, y, z, a, b, c;
    box.Get(x, y, z, a, b, c);
    GProp_GProps area;
    BRepGProp::SurfaceProperties(s, area);
    double v = volume(s) * siVolume;
    auto r = fields({{"revision", revision},
                     {"volume_m3", v},
                     {"area_m2", area.Mass() * siArea},
                     {"minimum_m", QCborArray{x / mm, y / mm, z / mm}},
                     {"maximum_m", QCborArray{a / mm, b / mm, c / mm}},
                     {"solids", count(s, TopAbs_SOLID)},
                     {"valid", true}});
    if (density > 0) {
        r.insert(QStringLiteral("density_kg_m3"), density);
        r.insert(QStringLiteral("mass_kg"), v * density);
    }
    return r;
}
DisplayMesh Part::mesh(double deflection) const {
    check(std::isfinite(deflection) && deflection >= .01 && deflection <= 5, "InvalidInput",
          "Display deflection must be .01–5 mm");
    DisplayMesh m;
    Bnd_Box exactBounds;
    for (auto &b : bodies)
        BRepBndLib::AddOptimal(b.shape, exactBounds, false, false);
    bool first = true;
    for (auto &original : bodies) {
        Body b{original.id, BRepBuilderAPI_Copy(original.shape, true, false).Shape()};
        BRepMesh_IncrementalMesh tessellate(b.shape, deflection, false, .35, false);
        check(tessellate.IsDone(), "InvalidGeometry", "Tessellation failed");
        auto edges = subshapes(b.shape, TopAbs_EDGE);
        for (auto it = edges.cbegin(); it != edges.cend(); ++it) {
            BRepAdaptor_Curve c(TopoDS::Edge(it.value()));
            int steps = c.GetType() == GeomAbs_Line ? 1 : 64;
            for (int i = 0; i < steps; ++i) {
                m.edgeIds.append(b.id + "/" + it.key() + "/segment/" + QString::number(i));
                for (int j : {i, i + 1}) {
                    auto p = c.Value(c.FirstParameter() +
                                     (c.LastParameter() - c.FirstParameter()) * j / steps);
                    m.edges.append({float(p.X()), float(p.Y()), float(p.Z())});
                }
            }
        }
        auto faces = subshapes(b.shape, TopAbs_FACE);
        for (TopExp_Explorer i(b.shape, TopAbs_FACE); i.More(); i.Next()) {
            auto face = TopoDS::Face(i.Current());
            auto role = m.faceIds.size();
            auto key = token(face);
            m.faceIds.append(b.id + "/" +
                             (faces.contains(key) ? key : QString("unresolved/%1").arg(role)));
            TopLoc_Location loc;
            auto tri = BRep_Tool::Triangulation(face, loc);
            check(!tri.IsNull(), "InvalidGeometry", "Missing display mesh");
            for (int t = 1; t <= tri->NbTriangles(); ++t) {
                int a, b, c;
                tri->Triangle(t).Get(a, b, c);
                if (face.Orientation() == TopAbs_REVERSED)
                    std::swap(b, c);
                for (int node : {a, b, c}) {
                    auto p = tri->Node(node).Transformed(loc.Transformation());
                    QVector3D v(float(p.X()), float(p.Y()), float(p.Z()));
                    if (first) {
                        m.minimum = m.maximum = v;
                        first = false;
                    } else
                        for (int k = 0; k < 3; ++k) {
                            m.minimum[k] = qMin(m.minimum[k], v[k]);
                            m.maximum[k] = qMax(m.maximum[k], v[k]);
                        }
                    check(m.vertices.size() < 1500000, "ResourceLimit",
                          "Display exceeds 500,000 triangle admission");
                    m.indices.append(quint32(m.vertices.size()));
                    m.vertices.append(v);
                }
                m.triangleRoles.append(quint32(role));
            }
        }
    }
    check(!first, "InvalidGeometry", "No displayable part");
    double x, y, z, a, b, c;
    exactBounds.Get(x, y, z, a, b, c);
    m.minimum = {float(x), float(y), float(z)};
    m.maximum = {float(a), float(b), float(c)};
    m.instances.append(Instance{"part", {}});
    m.validate();
    return m;
}
QCborArray trace(BRepBuilderAPI_MakeShape &operation, const TopoDS_Shape &input) {
    Part result;
    recordHistory(result, QString(), operation, input);
    return result.history;
}
QString topologyToken(const TopoDS_Shape &s) {
    return token(s);
}
QCborMap evolution(QString original, QCborArray history, QStringList current) {
    QSet<QString> mapped;
    bool deleted = false;
    QMap<QString, QSet<QString>> reverse;
    for (auto value : history) {
        auto m = value.toMap();
        auto input = m.value("input").toString();
        for (auto key : {"modified"})
            for (auto t : m.value(QString(key)).toArray())
                if (current.contains(t.toString())) {
                    reverse[t.toString()].insert(input);
                    if (input == original)
                        mapped.insert(t.toString());
                }
        if (input == original)
            deleted = deleted || m.value("deleted").toBool();
    }
    QString state = "Missing";
    if (current.contains(original)) {
        state = "Direct";
        mapped = {original};
    } else if (mapped.size() > 1)
        state = "Split";
    else if (mapped.size() == 1)
        state = reverse.value(*mapped.begin()).size() > 1 ? "Merged" : "History";
    else if (deleted)
        state = "Deleted";
    auto values = mapped.values();
    std::sort(values.begin(), values.end());
    return fields({{"state", state}, {"candidates", QCborArray::fromStringList(values)}});
}
QCborMap Part::reference(QString source, QString original, const Part &former) const {
    check(stages.contains(source), "ReferenceMissing", "Reference source feature is unavailable");
    auto shape = stages.value(source);
    auto kind = original.startsWith("edge/") ? TopAbs_EDGE : TopAbs_FACE;
    auto current = subshapes(shape, kind);
    auto resolved = evolution(original, history, current.keys());
    auto state = resolved.value("state").toString();
    if (state != "Missing")
        return resolved;
    check(former.stages.contains(source), "ReferenceMissing",
          "Former source is unavailable; choose a replacement explicitly");
    auto old = subshapes(former.stages.value(source), kind);
    check(old.contains(original), "ReferenceMissing", "Former role is unavailable");
    auto describe = [&](TopoDS_Shape s, TopoDS_Shape parent) {
        GProp_GProps p;
        int type;
        double radius = 0;
        QStringList adjacency;
        if (kind == TopAbs_EDGE) {
            BRepAdaptor_Curve c(TopoDS::Edge(s));
            type = int(c.GetType());
            if (c.GetType() == GeomAbs_Circle)
                radius = c.Circle().Radius();
            BRepGProp::LinearProperties(s, p);
            TopTools_IndexedDataMapOfShapeListOfShape map;
            TopExp::MapShapesAndAncestors(parent, TopAbs_EDGE, TopAbs_FACE, map);
            if (map.Contains(s))
                for (auto &face : map.FindFromKey(s)) {
                    BRepAdaptor_Surface surf(TopoDS::Face(face));
                    adjacency.append(QString::number(int(surf.GetType())));
                }
            std::sort(adjacency.begin(), adjacency.end());
        } else {
            BRepAdaptor_Surface surf(TopoDS::Face(s));
            type = int(surf.GetType());
            BRepGProp::SurfaceProperties(s, p);
        }
        auto c = p.CentreOfMass();
        return fields({{"type", type},
                       {"measure", p.Mass()},
                       {"radius", radius},
                       {"adjacency", QCborArray::fromStringList(adjacency)},
                       {"center", QCborArray{c.X(), c.Y(), c.Z()}}});
    };
    auto role = describe(old.value(original), former.stages.value(source));
    auto center = role.value("center").toArray();
    QCborArray candidates;
    Bnd_Box box;
    BRepBndLib::AddOptimal(shape, box, false, false);
    double x, y, z, a, b, c;
    box.Get(x, y, z, a, b, c);
    double distanceLimit = std::max(10., gp_Pnt(x, y, z).Distance(gp_Pnt(a, b, c)) * .25);
    for (auto it = current.cbegin(); it != current.cend(); ++it) {
        if (old.contains(it.key()))
            continue;
        auto d = describe(it.value(), shape);
        if (d.value("type") != role.value("type") ||
            d.value("adjacency") != role.value("adjacency"))
            continue;
        if (std::abs(d.value("radius").toDouble() - role.value("radius").toDouble()) > 1e-6)
            continue;
        double measure = role.value("measure").toDouble();
        if (std::abs(d.value("measure").toDouble() - measure) > std::max(1e-6, measure * 1e-6))
            continue;
        auto at = d.value("center").toArray();
        int equal = 0;
        double distance = 0;
        for (int i = 0; i < 3; ++i) {
            double delta = at[i].toDouble() - center[i].toDouble();
            if (std::abs(delta) < 1e-6)
                ++equal;
            distance += delta * delta;
        }
        if (equal < 2 || std::sqrt(distance) > distanceLimit)
            continue;
        candidates.append(it.key());
    }
    resolved.insert(QStringLiteral("state"), candidates.size() > 1    ? "Ambiguous"
                                             : candidates.size() == 1 ? "Candidate"
                                                                      : "Missing");
    resolved.insert(QStringLiteral("candidates"), candidates);
    resolved.insert(QStringLiteral("former_role"), role);
    return resolved;
}
bool ResultCache::get(QByteArray key, Effect &effect) {
    if (!entries.contains(key))
        return false;
    effect = entries.value(key);
    order.removeAll(key);
    order.append(key);
    ++hits;
    return true;
}
void ResultCache::put(QByteArray key, Effect effect) {
    qint64 bytes = QCborValue(effect.history).toCbor().size();
    auto cost = [&](const TopoDS_Shape &s) {
        if (!s.IsNull())
            bytes += count(s, TopAbs_FACE) * 4096LL + count(s, TopAbs_EDGE) * 1024LL +
                     count(s, TopAbs_VERTEX) * 512LL;
    };
    for (auto &b : effect.bodies)
        cost(b.shape);
    cost(effect.stage);
    cost(effect.tool);
    effect.bytes = bytes;
    if (entries.contains(key)) {
        used -= entries.take(key).bytes;
        order.removeAll(key);
    }
    if (bytes > budget)
        return;
    while (!order.empty() && (used + bytes > budget || entries.size() >= 128)) {
        used -= entries.take(order.takeFirst()).bytes;
    }
    entries[key] = effect;
    order.append(key);
    used += bytes;
}
QCborMap ResultCache::statistics() const {
    return fields({{"entries", entries.size()},
                   {"estimated_bytes", used},
                   {"budget_bytes", budget},
                   {"hits", qint64(hits)},
                   {"computations", qint64(computations)}});
}
} // namespace cad::kernel
