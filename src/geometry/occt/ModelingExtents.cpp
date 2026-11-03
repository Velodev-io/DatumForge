#include "ModelingInternal.hpp"
#include <BRepAdaptor_Curve.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepAlgoAPI_Common.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepAlgoAPI_Fuse.hxx>
#include <BRepBndLib.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <BRepClass3d_SolidClassifier.hxx>
#include <BRepClass_FaceClassifier.hxx>
#include <BRepGProp.hxx>
#include <BRepOffsetAPI_DraftAngle.hxx>
#include <BRepOffsetAPI_MakeOffsetShape.hxx>
#include <BRepPrimAPI_MakeHalfSpace.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <BRepTools.hxx>
#include <BRepTools_WireExplorer.hxx>
#include <BRep_Builder.hxx>
#include <BRep_Tool.hxx>
#include <Bnd_Box.hxx>
#include <GProp_GProps.hxx>
#include <IntCurvesFace_ShapeIntersector.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Face.hxx>
#include <gp_Lin.hxx>
#include <limits>
namespace cad::geometry::modeling {
using domain::check;
TopoDS_Shape moved(const TopoDS_Shape &shape, gp_Vec shift) {
    gp_Trsf transform;
    transform.SetTranslation(shift);
    return BRepBuilderAPI_Transform(shape, transform, true).Shape();
}
TopoDS_Shape join(const TopoDS_Shape &a, const TopoDS_Shape &b, kernel::Part &part, QString id) {
    if (a.IsNull())
        return b;
    if (b.IsNull())
        return a;
    BRepAlgoAPI_Fuse fuse;
    TopTools_ListOfShape arguments, tools;
    arguments.Append(a);
    tools.Append(b);
    fuse.SetArguments(arguments);
    fuse.SetTools(tools);
    fuse.SetNonDestructive(true);
    fuse.Build();
    check(fuse.IsDone() && !fuse.HasErrors(), "InvalidGeometry", "Exact tool union failed");
    kernel::recordShapeHistory(part, id, fuse, a);
    kernel::recordShapeHistory(part, id, fuse, b);
    return fuse.Shape();
}
TopoDS_Shape draft(const TopoDS_Shape &shape, const std::vector<TopoDS_Face> &faces,
                   gp_Dir direction, gp_Pln neutral, double angle, bool outward, kernel::Part &part,
                   QString id) {
    if (angle == 0)
        return shape;
    check(!faces.empty(), "InvalidGeometry", "No supported side faces to draft");
    BRepOffsetAPI_DraftAngle make(shape);
    for (auto &face : faces) {
        auto type = BRepAdaptor_Surface(face).GetType();
        check(type == GeomAbs_Plane || type == GeomAbs_Cylinder || type == GeomAbs_Cone,
              "UnsupportedOperation", "Draft requires analytic plane/cylinder/cone faces");
        make.Add(face, direction, outward ? -angle : angle, neutral);
        check(make.AddDone(), "InvalidGeometry", "Draft face cannot retain its topology");
    }
    make.Build();
    check(make.IsDone(), "InvalidGeometry", "Exact draft construction failed");
    kernel::validateShape(make.Shape());
    kernel::recordShapeHistory(part, id, make, shape);
    return make.Shape();
}
namespace {
double bound(const ModelingProfile &p, gp_Pnt origin) {
    Bnd_Box box;
    BRepBndLib::AddOptimal(p.shape, box, false, false);
    double x0, y0, z0, x1, y1, z1;
    box.Get(x0, y0, z0, x1, y1, z1);
    return 4 * (origin.Distance(gp_Pnt(x0, y0, z0)) + origin.Distance(gp_Pnt(x1, y1, z1)) + 1);
}
TopoDS_Face planeFace(const ModelingProfile &p, const gp_Pln &plane) {
    double size = bound(p, plane.Location());
    return BRepBuilderAPI_MakeFace(plane, -size, size, -size, size).Face();
}
TopoDS_Face limiter(const ModelingProfile &p, const domain::GeometryReference &ref,
                    const Context &context) {
    if (ref.role == "Face")
        return TopoDS::Face(context.references.topology(ref));
    auto f = context.references.plane(ref);
    return planeFace(p, gp_Pln(kernelPoint(f.origin), gp_Dir(vector(f.u).Crossed(vector(f.v)))));
}
std::vector<TopoDS_Face> sideFaces(const TopoDS_Shape &shape, gp_Dir direction) {
    std::vector<TopoDS_Face> result;
    for (TopExp_Explorer it(shape, TopAbs_FACE); it.More(); it.Next()) {
        auto face = TopoDS::Face(it.Current());
        BRepAdaptor_Surface surface(face);
        if (surface.GetType() == GeomAbs_Plane &&
            std::abs(surface.Plane().Axis().Direction().Dot(direction)) > 1 - 1e-10)
            continue;
        result.push_back(face);
    }
    return result;
}
gp_Pnt interior(const TopoDS_Face &face) {
    GProp_GProps properties;
    BRepGProp::SurfaceProperties(face, properties);
    auto p = properties.CentreOfMass();
    if (BRepClass_FaceClassifier(face, p, 1e-7).State() == TopAbs_IN)
        return p;
    double u0, u1, v0, v1;
    BRepTools::UVBounds(face, u0, u1, v0, v1);
    BRepAdaptor_Surface surface(face);
    for (int u = 1; u < 32; ++u)
        for (int v = 1; v < 32; ++v) {
            p = surface.Value(u0 + (u1 - u0) * u / 32., v0 + (v1 - v0) * v / 32.);
            if (BRepClass_FaceClassifier(face, p, 1e-7).State() == TopAbs_IN)
                return p;
        }
    throw domain::Error("InvalidGeometry",
                        "No unambiguous interior seed within profile query admission");
}
std::pair<double, double> range(const TopoDS_Shape &shape, gp_Pnt origin, gp_Vec direction) {
    Bnd_Box box;
    BRepBndLib::AddOptimal(shape, box, false, false);
    check(!box.IsVoid() && !box.IsOpen(), "InvalidGeometry", "Finite construction bounds required");
    double x0, y0, z0, x1, y1, z1;
    box.Get(x0, y0, z0, x1, y1, z1);
    double low = std::numeric_limits<double>::infinity(), high = -low;
    for (double x : {x0, x1})
        for (double y : {y0, y1})
            for (double z : {z0, z1}) {
                double station = gp_Vec(origin, gp_Pnt(x, y, z)).Dot(direction);
                low = std::min(low, station);
                high = std::max(high, station);
            }
    return {low, high};
}
double intersection(const TopoDS_Shape &face, gp_Pnt seed, gp_Vec direction, double low,
                    double high, bool first = false) {
    IntCurvesFace_ShapeIntersector query;
    query.Load(face, 1e-7);
    query.Perform(gp_Lin(seed, gp_Dir(direction)), low, high);
    check(query.IsDone(), "InvalidGeometry", "Limiting support intersection failed");
    std::vector<double> values;
    for (int i = 1; i <= query.NbPnt(); ++i)
        values.push_back(query.WParameter(i));
    std::sort(values.begin(), values.end());
    values.erase(std::unique(values.begin(), values.end(),
                             [](double a, double b) { return std::abs(a - b) <= 1e-7; }),
                 values.end());
    check(!values.empty(), "ReferenceMissing",
          "Limiting support does not cover the profile in the requested direction");
    check(first || values.size() == 1, "ReferenceAmbiguous",
          "Limiting surface has multiple admissible branches");
    return values[0];
}
double surfaceArea(const TopoDS_Shape &shape) {
    GProp_GProps p;
    BRepGProp::SurfaceProperties(shape, p);
    return p.Mass();
}
TopoDS_Shape common(const TopoDS_Shape &a, const TopoDS_Shape &b, kernel::Part &part, QString id) {
    BRepAlgoAPI_Common op;
    TopTools_ListOfShape arguments, tools;
    arguments.Append(a);
    tools.Append(b);
    op.SetArguments(arguments);
    op.SetTools(tools);
    op.SetNonDestructive(true);
    op.Build();
    check(op.IsDone() && !op.HasErrors(), "InvalidGeometry",
          "Exact limiting-surface intersection failed");
    kernel::recordShapeHistory(part, id, op, a);
    kernel::recordShapeHistory(part, id, op, b);
    return op.Shape();
}
void completeCaps(const TopoDS_Shape &result, const TopoDS_Shape &envelope,
                  const TopoDS_Face &profile, const TopoDS_Face &from, const TopoDS_Shape &until,
                  gp_Vec direction, double low, double high, kernel::Part &part, QString id) {
    TopoDS_Compound boundary;
    BRep_Builder builder;
    builder.MakeCompound(boundary);
    for (TopExp_Explorer face(until, TopAbs_FACE); face.More(); face.Next())
        builder.Add(boundary, face.Current());
    auto start = common(result, from, part, id), end = common(result, boundary, part, id);
    check(surfaceArea(start) > 1e-10 && surfaceArea(end) > 1e-10, "ReferenceMissing",
          "Both exact support caps are required");
    // All published boundary area must come from the finite selected supports
    // or the original prism walls, including trimmed-face domain coverage.
    double accounted = surfaceArea(start) + surfaceArea(end);
    for (auto &face : sideFaces(envelope, gp_Dir(direction)))
        accounted += surfaceArea(common(result, face, part, id));
    double total = surfaceArea(result);
    check(std::abs(accounted - total) <= std::max(1e-7, total * 1e-10), "ReferenceMissing",
          "Selected support does not cover the complete result cap");
    for (auto cap : {start, end}) {
        // Sweep the *actual cap* back across the sketch plane. Intersecting
        // that exact swept volume with the profile checks the whole footprint,
        // so a support crossing the start cannot silently delete profile area.
        BRepPrimAPI_MakePrism sweep(moved(cap, direction * (-high)), direction * (high - low));
        check(sweep.IsDone(), "InvalidGeometry", "Support footprint construction failed");
        double expected = surfaceArea(profile),
               covered = surfaceArea(common(profile, sweep.Shape(), part, id));
        check(std::abs(covered - expected) <= std::max(1e-7, expected * 1e-10), "ReferenceMissing",
              "Support fails to cover the entire profile footprint");
    }
}
TopoDS_Shape trimmedPrism(const TopoDS_Face &profile, const TopoDS_Face &from,
                          const TopoDS_Face &until, gp_Vec direction, kernel::Part &part,
                          QString id) {
    auto seed = interior(profile);
    auto a = range(from, seed, direction), b = range(until, seed, direction);
    double low = std::min({0., a.first, b.first}), high = std::max({0., a.second, b.second});
    double margin = high - low + 1;
    low -= margin;
    high += margin;
    double start = intersection(from, seed, direction, low, high);
    double end = intersection(until, seed, direction, start + 1e-7, high);
    check(end - start > 1e-7, "InvalidGeometry", "Extrusion has a zero or reversed extent");
    auto base = moved(profile, direction * low);
    BRepPrimAPI_MakePrism prism(base, direction * (high - low));
    check(prism.IsDone(), "InvalidGeometry", "Construction envelope extrusion failed");
    kernel::recordShapeHistory(part, id, prism, base);
    double epsilon = std::min(.01, (end - start) / 4);
    auto after =
        BRepPrimAPI_MakeHalfSpace(from, seed.Translated(direction * (start + epsilon))).Solid();
    auto before =
        BRepPrimAPI_MakeHalfSpace(until, seed.Translated(direction * (end - epsilon))).Solid();
    auto result = common(common(prism.Shape(), after, part, id), before, part, id);
    kernel::validateShape(result);
    completeCaps(result, prism.Shape(), profile, from, until, direction, low, high, part, id);
    // Workspace end caps must disappear: no finite envelope depth can become a
    // hidden substitute for the selected geometric termination.
    auto lowFace =
        BRepBuilderAPI_MakeFace(gp_Pln(seed.Translated(direction * low), gp_Dir(direction)),
                                -margin * 4, margin * 4, -margin * 4, margin * 4)
            .Face();
    auto highFace = TopoDS::Face(moved(lowFace, direction * (high - low)));
    check(surfaceArea(common(result, lowFace, part, id)) <= 1e-10 &&
              surfaceArea(common(result, highFace, part, id)) <= 1e-10,
          "ReferenceMissing", "Unbounded or incomplete limiting support");
    return result;
}
TopoDS_Shape trimmedToBody(const TopoDS_Face &profile, const TopoDS_Face &from,
                           const TopoDS_Shape &body, gp_Vec direction, kernel::Part &part,
                           QString id) {
    auto seed = interior(profile);
    auto a = range(from, seed, direction), b = range(body, seed, direction);
    double low = std::min({0., a.first, b.first}), high = std::max({0., a.second, b.second});
    double margin = high - low + 1;
    low -= margin;
    high += margin;
    double start = intersection(from, seed, direction, low, high);
    double end = intersection(body, seed, direction, start + 1e-7, high, true);
    auto probe = seed.Translated(direction * (start + std::min(.01, (end - start) / 4)));
    BRepPrimAPI_MakePrism prism(moved(profile, direction * low), direction * (high - low));
    check(prism.IsDone(), "InvalidGeometry", "Body termination envelope failed");
    kernel::recordShapeHistory(part, id, prism, profile);
    auto after = BRepPrimAPI_MakeHalfSpace(from, probe).Solid();
    auto tool = common(prism.Shape(), after, part, id);
    auto state = BRepClass3d_SolidClassifier(body, probe, 1e-7).State();
    check(state == TopAbs_IN || state == TopAbs_OUT, "ReferenceAmbiguous",
          "Extrusion starts on an ambiguous body boundary");
    TopoDS_Shape cells;
    if (state == TopAbs_IN)
        cells = common(tool, body, part, id);
    else {
        BRepAlgoAPI_Cut cut;
        TopTools_ListOfShape arguments, tools;
        arguments.Append(tool);
        tools.Append(body);
        cut.SetArguments(arguments);
        cut.SetTools(tools);
        cut.SetNonDestructive(true);
        cut.Build();
        check(cut.IsDone() && !cut.HasErrors(), "InvalidGeometry", "Body termination split failed");
        kernel::recordShapeHistory(part, id, cut, tool);
        kernel::recordShapeHistory(part, id, cut, body);
        cells = cut.Shape();
    }
    TopoDS_Shape result;
    for (TopExp_Explorer solid(cells, TopAbs_SOLID); solid.More(); solid.Next())
        if (BRepClass3d_SolidClassifier(solid.Current(), probe, 1e-7).State() == TopAbs_IN) {
            check(result.IsNull(), "ReferenceAmbiguous",
                  "Multiple start-adjacent body termination cells");
            result = solid.Current();
        }
    check(!result.IsNull(), "ReferenceMissing", "No start-adjacent body termination cell");
    kernel::validateShape(result);
    completeCaps(result, prism.Shape(), profile, from, body, direction, low, high, part, id);
    check(surfaceArea(common(result, from, part, id)) > 1e-10 &&
              surfaceArea(common(result, moved(profile, direction * high), part, id)) <= 1e-10,
          "ReferenceMissing", "Body does not terminate the complete profile");
    return result;
}
TopoDS_Shape draftedLimit(TopoDS_Shape shape, const TopoDS_Face &from, const TopoDS_Shape &until,
                          gp_Vec direction, double angle, bool outward, kernel::Part &part,
                          QString id) {
    if (angle == 0)
        return shape;
    BRepAdaptor_Surface startSurface(from);
    check(startSurface.GetType() == GeomAbs_Plane, "UnsupportedOperation",
          "Draft neutral support must be planar");
    TopoDS_Compound boundary;
    BRep_Builder builder;
    builder.MakeCompound(boundary);
    for (TopExp_Explorer it(until, TopAbs_FACE); it.More(); it.Next())
        builder.Add(boundary, it.Current());
    std::vector<TopoDS_Face> sides;
    for (TopExp_Explorer it(shape, TopAbs_FACE); it.More(); it.Next())
        if (surfaceArea(common(it.Current(), from, part, id)) <= 1e-10 &&
            surfaceArea(common(it.Current(), boundary, part, id)) <= 1e-10)
            sides.push_back(TopoDS::Face(it.Current()));
    shape = draft(shape, sides, gp_Dir(direction), startSurface.Plane(), angle, outward, part, id);
    // Draft may enlarge a cap beyond a selected face's finite domain. Every
    // resulting cap face that overlaps a support must still fit wholly on it.
    for (auto support : {TopoDS_Shape(from), TopoDS_Shape(boundary)}) {
        double capArea = 0;
        for (TopExp_Explorer it(shape, TopAbs_FACE); it.More(); it.Next()) {
            double covered = surfaceArea(common(it.Current(), support, part, id));
            if (covered > 1e-10) {
                check(std::abs(covered - surfaceArea(it.Current())) <=
                          std::max(1e-7, covered * 1e-10),
                      "ReferenceMissing", "Draft extends beyond the selected support domain");
                capArea += covered;
            }
        }
        check(capArea > 1e-10, "ReferenceMissing", "Draft lost its selected support cap");
    }
    return shape;
}
TopoDS_Shape openCut(const ModelingProfile &profile, const QCborMap &m, const Context &context,
                     const TopoDS_Shape &target, kernel::Part &part,
                     const exchange::Control &control) {
    check(!target.IsNull() && m.value("operation").toString() == "Cut", "InvalidInput",
          "Open profile requires a cut target");
    for (auto key : {"direction1", "direction2"})
        if (m.value(QString::fromLatin1(key)).isMap())
            check(m.value(QString::fromLatin1(key))
                          .toMap()
                          .value("draft")
                          .toMap()
                          .value("angle")
                          .toDouble() == 0,
                  "UnsupportedOperation",
                  "Open-profile cut draft requires a supported closed wall section");
    auto id = m.value("id").toString();
    auto origin = profile.plane.Location();
    auto normal = gp_Vec(profile.plane.Axis().Direction());
    auto u = gp_Vec(profile.plane.Position().XDirection()),
         v = gp_Vec(profile.plane.Position().YDirection());
    auto ur = range(target, origin, u), vr = range(target, origin, v),
         nr = range(target, origin, normal);
    double margin = (ur.second - ur.first) + (vr.second - vr.first) + (nr.second - nr.first) + 1;
    double low = nr.first - margin, high = nr.second + margin;
    TopoDS_Shape selected;
    int count = 0;
    for (TopExp_Explorer it(profile.shape, TopAbs_WIRE); it.More(); it.Next()) {
        check(++count <= 16, "ResourceLimit", "Open cut boundary admission exceeded");
        auto wire = TopoDS::Wire(it.Current());
        BRepTools_WireExplorer edges(wire);
        check(edges.More(), "InvalidGeometry", "Empty cut chain");
        auto first = edges.Current();
        auto start = BRep_Tool::Pnt(TopExp::FirstVertex(first, true));
        gp_Pnt end;
        for (; edges.More(); edges.Next())
            end = BRep_Tool::Pnt(TopExp::LastVertex(edges.Current(), true));
        auto outside = [&](gp_Pnt p) {
            auto delta = gp_Vec(origin, p);
            double x = delta.Dot(u), y = delta.Dot(v);
            return x <= ur.first + 1e-7 || x >= ur.second - 1e-7 || y <= vr.first + 1e-7 ||
                   y >= vr.second - 1e-7;
        };
        check(outside(start) && outside(end), "ReferenceMissing",
              "Open cut endpoints must reach outside the target projection");
        BRepAdaptor_Curve curve(first);
        gp_Pnt point;
        gp_Vec tangent;
        curve.D1((curve.FirstParameter() + curve.LastParameter()) / 2, point, tangent);
        if (first.Orientation() == TopAbs_REVERSED)
            tangent.Reverse();
        auto left = normal.Crossed(tangent).Normalized();
        if (m.value("flip_side").toBool())
            left.Reverse();
        auto probe = point.Translated(left * .01 + normal * ((low + high) / 2));
        BRepPrimAPI_MakePrism curtain(moved(wire, normal * low), normal * (high - low));
        check(curtain.IsDone(), "InvalidGeometry", "Exact cut curtain failed");
        TopoDS_Shape half;
        if (curtain.Shape().ShapeType() == TopAbs_SHELL)
            half = BRepPrimAPI_MakeHalfSpace(TopoDS::Shell(curtain.Shape()), probe).Solid();
        else if (curtain.Shape().ShapeType() == TopAbs_FACE)
            half = BRepPrimAPI_MakeHalfSpace(TopoDS::Face(curtain.Shape()), probe).Solid();
        else
            throw domain::Error("InvalidGeometry",
                                "Cut chain did not generate one connected curtain");
        selected = join(selected, common(target, half, part, id), part, id);
    }
    check(count > 0, "InvalidGeometry", "No open cut chain");
    ModelingProfile envelope;
    envelope.plane = profile.plane;
    envelope.shape = BRepBuilderAPI_MakeFace(profile.plane, ur.first - margin, ur.second + margin,
                                             vr.first - margin, vr.second + margin)
                         .Face();
    auto extent = extrude(envelope, m, context, target, part, control);
    return common(selected, extent, part, id);
}
} // namespace
TopoDS_Shape extrude(const ModelingProfile &profile, const QCborMap &m, const Context &context,
                     const TopoDS_Shape &target, kernel::Part &part,
                     const exchange::Control &control) {
    if (profile.open)
        return openCut(profile, m, context, target, part, control);
    auto id = m.value("id").toString();
    auto start = m.value("start").toMap();
    auto startMode = start.value("mode").toString();
    auto origin = profile.plane.Location();
    gp_Vec normal(profile.plane.Axis().Direction());
    if (startMode == "Offset")
        origin.Translate(normal * (start.value("distance").toDouble() * 1000));
    else if (startMode == "Vertex") {
        auto point = context.references.point(
            domain::GeometryReference::parse(start.value("reference").toMap()));
        origin.Translate(normal * gp_Vec(origin, point).Dot(normal));
    }
    auto startFace =
        startMode == "Surface"
            ? limiter(profile, domain::GeometryReference::parse(start.value("reference").toMap()),
                      context)
            : planeFace(profile, gp_Pln(origin, gp_Dir(normal)));
    auto base = moved(profile.shape, gp_Vec(profile.plane.Location(), origin));
    if (m.value("reverse").toBool())
        normal.Reverse();
    auto buildDirection = [&](QCborMap end, gp_Vec direction) {
        control.checkpoint("Constructing exact extrusion", 50);
        auto mode = end.value("mode").toString();
        auto drafting = end.value("draft").toMap();
        double angle = drafting.value("angle").toDouble();
        double depth = end.value("value").toDouble() * 1000;
        if (mode == "ThroughAll") {
            check(!target.IsNull(), "ReferenceMissing", "ThroughAll requires a current target");
            auto stop = range(target, origin, direction),
                 begin = range(startFace, origin, direction);
            depth = stop.second - begin.first + std::max(1., stop.second - stop.first);
            check(depth > 1e-7, "InvalidGeometry", "ThroughAll has no forward target extent");
            mode = "Blind";
        }
        if ((mode == "Blind" || mode == "Midplane") && startMode != "Surface") {
            if (mode == "Midplane" && angle != 0) {
                TopoDS_Shape halves;
                for (double sign : {1., -1.}) {
                    BRepPrimAPI_MakePrism prism(base, direction * (sign * depth / 2));
                    check(prism.IsDone(), "InvalidGeometry", "Midplane half extrusion failed");
                    kernel::recordShapeHistory(part, id, prism, base);
                    auto half = prism.Shape();
                    half = draft(half, sideFaces(half, gp_Dir(direction)), gp_Dir(direction * sign),
                                 gp_Pln(origin, gp_Dir(direction)), angle,
                                 drafting.value("outward").toBool(), part, id);
                    halves = join(halves, half, part, id);
                }
                return halves;
            }
            auto face = mode == "Midplane" ? moved(base, direction * (-depth / 2)) : base;
            BRepPrimAPI_MakePrism prism(face, direction * depth);
            check(prism.IsDone(), "InvalidGeometry", "Exact extrusion failed");
            kernel::recordShapeHistory(part, id, prism, face);
            auto tool = prism.Shape();
            return draft(tool, sideFaces(tool, gp_Dir(direction)), gp_Dir(direction),
                         gp_Pln(origin, gp_Dir(direction)), angle,
                         drafting.value("outward").toBool(), part, id);
        }
        if (mode == "UpToNext" || mode == "UpToBody") {
            auto until = mode == "UpToNext"
                             ? target
                             : body(context, sketch::fields({{"source", end.value("reference")},
                                                             {"body", end.value("body")}}))
                                   .shape;
            check(!until.IsNull(), "ReferenceMissing", "Body termination source is missing");
            TopoDS_Shape combined;
            for (TopExp_Explorer it(base, TopAbs_FACE); it.More(); it.Next())
                combined = join(combined,
                                trimmedToBody(TopoDS::Face(it.Current()), startFace, until,
                                              direction, part, id),
                                part, id);
            return draftedLimit(combined, startFace, until, direction, angle,
                                drafting.value("outward").toBool(), part, id);
        }
        TopoDS_Face from = startFace, until;
        if (mode == "Blind" || mode == "Midplane") {
            if (mode == "Midplane")
                from = TopoDS::Face(moved(startFace, direction * (-depth / 2)));
            until = TopoDS::Face(
                moved(startFace, direction * (mode == "Midplane" ? depth / 2 : depth)));
        } else if (mode == "UpToVertex") {
            auto p = context.references.point(
                domain::GeometryReference::parse(end.value("reference").toMap()));
            until = planeFace(profile, gp_Pln(p, gp_Dir(direction)));
        } else if (mode == "UpToSurface" || mode == "OffsetFromSurface") {
            until = limiter(
                profile, domain::GeometryReference::parse(end.value("reference").toMap()), context);
            if (mode == "OffsetFromSurface" && end.value("offset").toDouble() > 0) {
                BRepOffsetAPI_MakeOffsetShape offset;
                offset.PerformBySimple(until, end.value("offset").toDouble() * 1000 *
                                                  (end.value("offset_reverse").toBool() ? -1 : 1));
                check(offset.IsDone(), "InvalidGeometry", "Exact limiting surface offset failed");
                TopExp_Explorer face(offset.Shape(), TopAbs_FACE);
                check(face.More(), "InvalidGeometry", "No offset limiting face");
                until = TopoDS::Face(face.Current());
            }
        } else
            throw domain::Error("UnsupportedOperation",
                                "Exact body termination adapter is not available");
        TopoDS_Shape combined;
        for (TopExp_Explorer it(base, TopAbs_FACE); it.More(); it.Next()) {
            auto face = TopoDS::Face(it.Current());
            combined =
                join(combined, trimmedPrism(face, from, until, direction, part, id), part, id);
        }
        return draftedLimit(combined, from, until, direction, angle,
                            drafting.value("outward").toBool(), part, id);
    };
    auto result = buildDirection(m.value("direction1").toMap(), normal);
    if (!m.value("direction2").isNull())
        result = join(result, buildDirection(m.value("direction2").toMap(), -normal), part, id);
    return result;
}
} // namespace cad::geometry::modeling
