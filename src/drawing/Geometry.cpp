#include "Geometry.hpp"
#include "Paths.hpp"
#include <BRepAdaptor_Curve.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <BRepGProp.hxx>
#include <BRepLib.hxx>
#include <BRep_Builder.hxx>
#include <GCPnts_QuasiUniformDeflection.hxx>
#include <GProp_GProps.hxx>
#include <HLRAlgo_Projector.hxx>
#include <HLRBRep_Algo.hxx>
#include <HLRBRep_HLRToShape.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Compound.hxx>
#include <gp_Quaternion.hxx>
#include <numbers>
namespace cad::drawing {
using domain::check;
using sketch::fields;
namespace {
gp_Ax2 axes(QString kind) {
    if (kind == "Front")
        return gp_Ax2(gp_Pnt(), gp_Dir(0, 0, 1), gp_Dir(1, 0, 0));
    if (kind == "Top")
        return gp_Ax2(gp_Pnt(), gp_Dir(0, -1, 0), gp_Dir(1, 0, 0));
    if (kind == "Right")
        return gp_Ax2(gp_Pnt(), gp_Dir(1, 0, 0), gp_Dir(0, 0, -1));
    return gp_Ax2(gp_Pnt(), gp_Dir(1, -1, 1), gp_Dir(1, 1, 0));
}
TopoDS_Edge edge(const kernel::Part &p, QString body, QString token) {
    TopoDS_Edge found;
    int count = 0;
    for (auto &b : p.bodies)
        if (b.id == body)
            for (TopExp_Explorer it(b.shape, TopAbs_EDGE); it.More(); it.Next())
                if (kernel::topologyToken(it.Current()) == token) {
                    if (found.IsNull() || !found.IsSame(it.Current()))
                        ++count;
                    found = TopoDS::Edge(it.Current());
                }
    check(count == 1, "ReferenceMissing",
          "Exact edge is missing or ambiguous; select an explicit replacement");
    return found;
}
QCborMap annotation(const Annotation &a, const Sheet &s, const View *v, const Source *src) {
    auto out = a.map();
    out.insert(QStringLiteral("status"), "Current");
    out.insert(QStringLiteral("label"), a.text);
    if (a.kind == "Text")
        return out;
    try {
        check(v && src && !src->part.bodies.empty(), "UnsupportedOperation",
              "Dimensions currently require a Part view");
        auto e = edge(src->part, a.body, a.token);
        BRepAdaptor_Curve c(e);
        double value = 0;
        gp_Pnt point = c.Value(c.FirstParameter());
        if (a.kind == "Linear") {
            check(c.GetType() == GeomAbs_Line, "UnsupportedOperation",
                  "Linear dimensions require a straight edge");
            GProp_GProps prop;
            BRepGProp::LinearProperties(e, prop);
            value = prop.Mass() / 1000;
        } else if (a.kind == "Angular") {
            BRepAdaptor_Curve second(edge(src->part, a.body, a.secondToken));
            check(c.GetType() == GeomAbs_Line && second.GetType() == GeomAbs_Line,
                  "UnsupportedOperation", "Angular dimensions require two straight edges");
            value = c.Line().Direction().Angle(second.Line().Direction());
        } else {
            check(c.GetType() == GeomAbs_Circle, "UnsupportedOperation",
                  "Radius/diameter/center require a circular edge");
            value = c.Circle().Radius() / 1000 * (a.kind == "Diameter" ? 2 : 1);
            point = c.Circle().Location();
        }
        HLRAlgo_Projector projector(axes(v->orientation));
        Standard_Real x, y, z;
        projector.Project(point, x, y, z);
        out.insert(QStringLiteral("anchor_mm"), QCborArray{v->x + x * s.scale, v->y - y * s.scale});
        if (a.kind == "Linear") {
            QCborArray ends;
            for (auto t : {c.FirstParameter(), c.LastParameter()}) {
                Standard_Real px, py, pz;
                projector.Project(c.Value(t), px, py, pz);
                ends.append(QCborArray{v->x + px * s.scale, v->y - py * s.scale});
            }
            out.insert(QStringLiteral("dimension_points_mm"), ends);
        }
        out.insert(QStringLiteral("measured_si"), value);
        auto label = a.kind == "Angular"
                         ? QString::number(value * 180 / std::numbers::pi, 'f', 2) + " deg"
                         : QString::number(value / domain::toSI(1, "length", s.units), 'f', 3) +
                               " " + s.units;
        if (a.kind == "Radius")
            label = "R " + label;
        if (a.kind == "Diameter")
            label = "Dia " + label;
        if (a.kind == "Center")
            label = "+";
        if (!a.overrideText.isEmpty())
            label = a.overrideText + " [override; measured " + label + "]";
        out.insert(QStringLiteral("label"), label);
    } catch (const domain::Error &e) {
        out.insert(QStringLiteral("status"), "Dangling");
        out.insert(QStringLiteral("label"), "DANGLING: " + a.kind);
        out.insert(QStringLiteral("reason"), QString::fromUtf8(e.what()));
    }
    return out;
}
QCborArray paths(TopoDS_Shape shape, const Sheet &s, const View &v, int &total) {
    QCborArray out;
    if (shape.IsNull())
        return out;
    BRepLib::BuildCurves3d(shape);
    for (TopExp_Explorer it(shape, TopAbs_EDGE); it.More(); it.Next()) {
        BRepAdaptor_Curve curve(TopoDS::Edge(it.Current()));
        GCPnts_QuasiUniformDeflection sample(curve, .01 / s.scale);
        check(sample.IsDone(), "InvalidGeometry", "Projection curve sampling failed");
        QByteArray points;
        for (int i = 1; i <= sample.NbPoints(); ++i) {
            check(++total <= 60000, "ResourceLimit", "Drawing vector point admission exceeded");
            auto p = sample.Value(i);
            double x = v.x + p.X() * s.scale, y = v.y - p.Y() * s.scale;
            check(x >= 6 && x <= s.width - 6 && y >= 6 && y <= s.height - 18, "InvalidInput",
                  "View exceeds printable sheet bounds; adjust scale or placement");
            appendPoint(points, x, y);
        }
        if (points.size() > 8)
            out.append(points);
    }
    return out;
}
} // namespace
Source source(QString document, const assembly::Loader &load) {
    Source out;
    TopoDS_Compound compound;
    BRep_Builder builder;
    builder.MakeCompound(compound);
    QMap<QString, int> quantities;
    QSet<QString> stack;
    int leaves = 0;
    std::function<void(QString, assembly::Pose, bool, int)> walk =
        [&](QString id, assembly::Pose pose, bool hidden, int depth) {
            check(depth <= 8 && !stack.contains(id), "DependencyCycle",
                  "Drawing source cycle/depth exceeded");
            auto d = load(id, true);
            check(!d.stale && d.resolved, "StaleResult",
                  "Drawing requires current resolved exact sources");
            out.revision = qMax(out.revision, d.revision);
            if (d.kind == "Part") {
                check(++leaves <= 100 && !d.part.bodies.empty(), "ResourceLimit",
                      "Drawing requires 1–100 exact leaf parts");
                quantities[id]++;
                if (id == document)
                    out.part = d.part;
                gp_Trsf t;
                t.SetRotation(gp_Quaternion(pose.rotation[1], pose.rotation[2], pose.rotation[3],
                                            pose.rotation[0]));
                t.SetTranslationPart(gp_Vec(pose.translation[0] * 1000, pose.translation[1] * 1000,
                                            pose.translation[2] * 1000));
                if (!hidden)
                    for (auto &b : d.part.bodies)
                        builder.Add(compound, BRepBuilderAPI_Transform(b.shape, t, true).Shape());
            } else {
                check(d.kind == "Assembly", "UnsupportedOperation",
                      "Drawing views require Part/Assembly sources");
                stack.insert(id);
                for (auto &o : d.assembly.occurrences)
                    if (!o.suppressed)
                        walk(o.definition, assembly::compose(pose, o.pose), hidden || o.hidden,
                             depth + 1);
                stack.remove(id);
            }
        };
    walk(document, {}, false, 0);
    out.shape = compound;
    for (auto id : quantities.keys())
        out.bom.append(fields(
            {{"definition", id}, {"name", load(id, false).name}, {"quantity", quantities[id]}}));
    return out;
}
QCborArray anchors(QString document, const assembly::Loader &load) {
    auto src = source(document, load);
    check(!src.part.bodies.empty(), "UnsupportedOperation",
          "Dimension anchors currently require a Part view");
    QCborArray out;
    QSet<QString> seen;
    for (auto &b : src.part.bodies)
        for (TopExp_Explorer it(b.shape, TopAbs_EDGE); it.More(); it.Next()) {
            auto token = kernel::topologyToken(it.Current());
            auto key = b.id + token;
            if (seen.contains(key))
                continue;
            seen.insert(key);
            BRepAdaptor_Curve c(TopoDS::Edge(it.Current()));
            if (c.GetType() != GeomAbs_Line && c.GetType() != GeomAbs_Circle)
                continue;
            out.append(fields({{"body", b.id},
                               {"token", token},
                               {"kind", c.GetType() == GeomAbs_Line ? "Line" : "Circle"}}));
        }
    check(out.size() <= 1000, "ResourceLimit", "Drawing anchor admission exceeded");
    return out;
}
QCborMap render(const Model &model, const assembly::Loader &load) {
    QCborArray sheets;
    QMap<QString, Source> sources;
    bool current = true;
    int total = 0;
    for (auto &s : model.sheets) {
        auto sheet = s.map();
        QCborArray views, annotations;
        QMap<QString, View> byId;
        for (auto &v : s.views) {
            auto view = v.map();
            byId[v.id] = v;
            try {
                if (!sources.contains(v.source))
                    sources[v.source] = source(v.source, load);
                auto &src = sources[v.source];
                bool stale = v.sourceRevision != src.revision;
                current &= !stale;
                Handle(HLRBRep_Algo) algorithm = new HLRBRep_Algo;
                algorithm->Add(src.shape);
                algorithm->Projector(HLRAlgo_Projector(axes(v.orientation)));
                algorithm->Update();
                algorithm->Hide();
                HLRBRep_HLRToShape result(algorithm);
                QCborArray visible = paths(result.VCompound(), s, v, total);
                for (auto p : paths(result.OutLineVCompound(), s, v, total))
                    visible.append(p);
                view.insert(QStringLiteral("visible"), visible);
                QCborArray hidden;
                if (v.hidden) {
                    hidden = paths(result.HCompound(), s, v, total);
                    for (auto p : paths(result.OutLineHCompound(), s, v, total))
                        hidden.append(p);
                }
                view.insert(QStringLiteral("hidden_paths"), hidden);
                view.insert(QStringLiteral("actual_revision"), src.revision);
                view.insert(QStringLiteral("status"), stale ? "Stale" : "Current");
            } catch (const domain::Error &e) {
                current = false;
                view.insert(QStringLiteral("status"), "Unresolved");
                view.insert(QStringLiteral("reason"), QString::fromUtf8(e.what()));
            } catch (const Standard_Failure &) {
                current = false;
                view.insert(QStringLiteral("status"), "Failed");
                view.insert(QStringLiteral("reason"), "Exact hidden-line projection failed");
            }
            views.append(view);
        }
        for (auto &a : s.annotations) {
            auto it = byId.constFind(a.view);
            const View *v = it == byId.cend() ? nullptr : &it.value();
            auto src = v && sources.contains(v->source) ? &sources[v->source] : nullptr;
            auto row = annotation(a, s, v, src);
            current &= row.value("status") == "Current";
            annotations.append(row);
        }
        sheet.insert(QStringLiteral("views"), views);
        sheet.insert(QStringLiteral("annotations"), annotations);
        if (!s.bomSource.isEmpty()) {
            try {
                auto d = load(s.bomSource, false);
                check(d.kind == "Assembly", "UnsupportedOperation", "BOM requires an Assembly");
                auto bom = source(s.bomSource, load);
                current &= s.bomRevision == bom.revision;
                sheet.insert(QStringLiteral("bom_status"),
                             s.bomRevision == bom.revision ? "Current" : "Stale");
                sheet.insert(QStringLiteral("bom"), bom.bom);
            } catch (const domain::Error &e) {
                current = false;
                sheet.insert(QStringLiteral("bom_error"), QString::fromUtf8(e.what()));
            }
        }
        sheets.append(sheet);
    }
    auto result = fields({{"sheets", sheets},
                          {"current", current},
                          {"raster_fallback", false},
                          {"vector_chord_error_mm", .01},
                          {"bom_policy", "Flattened leaf Part quantities; suppressed excluded, "
                                         "hidden/unloaded included; stable document identifiers"}});
    check(QCborValue(result).toCbor().size() < int(limits::control) - 8192, "ResourceLimit",
          "Drawing display exceeds control admission");
    return result;
}
} // namespace cad::drawing
