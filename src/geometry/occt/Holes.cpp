#include "Holes.hpp"
#include "SupportReferences.hpp"
#include <BRepAdaptor_Curve.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepAlgoAPI_Check.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepClass_FaceClassifier.hxx>
#include <BRepPrimAPI_MakeCone.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRepTools.hxx>
#include <Geom_Circle.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <cmath>
#include <gp_Lin.hxx>
#include <numbers>

namespace cad::geometry::modeling {
using domain::check;
constexpr double mm = 1000.0;
namespace {
TopoDS_Shape singleHole(const TopoDS_Shape &target, const QCborMap &intent, gp_Pnt origin,
                        gp_Vec direction, const Context &context, kernel::Part &part, QString owner,
                        const exchange::Control &control) {
    auto sizing = intent.value("sizing").toMap();
    auto term = intent.value("termination").toMap();
    auto family = intent.value("family").toString();
    auto mode = term.value("mode").toString();
    double radius =
        sizing
            .value(QString(family == "Tapped" || family == "PipeTap" ? "tap_drill_diameter"
                                                                     : "diameter"))
            .toDouble() *
        mm / 2;
    double depth = sizing.value("depth").toDouble() * mm;
    bool drilled = family == "Drilled" || family == "LegacySimpleDrilled" ||
                   family == "LegacyCounterboreDrill" || family == "LegacyCountersinkDrill";
    double tip = drilled ? radius / std::tan(sizing.value("drill_angle").toDouble() / 2) : 0;
    if (drilled && mode == "Blind" && term.value("depth_reference") == "Tip") {
        depth -= tip;
        check(depth > 1e-7, "InvalidInput", "Drill tip depth is shorter than the drill point");
    }
    check(!drilled || mode == "Blind" || mode == "ThroughAll", "UnsupportedOperation",
          "Drill-point holes currently support Blind or Through All termination");
    ModelingProfile profile;
    profile.plane = gp_Pln(origin, gp_Dir(direction));
    Handle(Geom_Circle) circle = new Geom_Circle(gp_Ax2(origin, gp_Dir(direction)), radius);
    profile.shape =
        BRepBuilderAPI_MakeFace(
            profile.plane, BRepBuilderAPI_MakeWire(BRepBuilderAPI_MakeEdge(circle).Edge()).Wire())
            .Face();
    QCborMap end = term;
    end.remove(QStringLiteral("depth_reference"));
    if (mode == "Blind")
        end.insert(QStringLiteral("value"), depth / mm);
    end.insert(QStringLiteral("draft"), sketch::fields({{"angle", 0.0}, {"outward", false}}));
    auto extrusion = sketch::fields({{"id", owner},
                                     {"operation", "Cut"},
                                     {"start", sketch::fields({{"mode", "SketchPlane"}})},
                                     {"reverse", false},
                                     {"direction1", end},
                                     {"direction2", nullptr}});
    auto tool = extrude(profile, extrusion, context, target, part, control);
    auto axis = gp_Ax2(origin, gp_Dir(direction));
    if (drilled && mode == "Blind") {
        auto tipOrigin = origin.Translated(direction * depth);
        tool =
            join(tool,
                 BRepPrimAPI_MakeCone(gp_Ax2(tipOrigin, gp_Dir(direction)), radius, 0, tip).Shape(),
                 part, owner);
    }
    if (family == "Counterbore" || family == "LegacyCounterboreDrill") {
        auto counterbore =
            BRepPrimAPI_MakeCylinder(axis, sizing.value("counterbore_diameter").toDouble() * mm / 2,
                                     sizing.value("counterbore_depth").toDouble() * mm)
                .Shape();
        tool = join(tool, counterbore, part, owner);
    } else if (family == "Countersink" || family == "LegacyCountersinkDrill") {
        double outer = sizing.value("countersink_diameter").toDouble() * mm / 2;
        double height =
            (outer - radius) / std::tan(sizing.value("countersink_angle").toDouble() / 2);
        check(height > 1e-7 && (mode != "Blind" || height < depth), "InvalidInput",
              "Countersink must fit within the hole depth");
        tool = join(tool, BRepPrimAPI_MakeCone(axis, outer, radius, height).Shape(), part, owner);
    } else if (family == "LegacyTapered" || family == "PipeTap") {
        check(mode == "Blind", "UnsupportedOperation",
              "Tapered holes currently require a blind depth");
        double bottom = radius - depth * std::tan(sizing.value("taper_angle").toDouble() / 2);
        check(bottom > 1e-7, "InvalidInput", "Taper closes before the requested depth");
        tool = BRepPrimAPI_MakeCone(axis, radius, bottom, depth).Shape();
    }
    kernel::validateShape(tool);
    return tool;
}
} // namespace
TopoDS_Shape hole(const TopoDS_Shape &target, const QCborMap &intent, const Context &context,
                  kernel::Part &part, QString owner, const exchange::Control &control) {
    auto placement = intent.value("placement").toMap();
    auto ref = domain::GeometryReference::parse(placement.value("face").toMap());
    auto face = TopoDS::Face(context.references.topology(ref));
    BRepAdaptor_Surface surface(face);
    check(surface.GetType() == GeomAbs_Plane, "UnsupportedOperation",
          "Hole placement currently requires a planar face");
    auto plane = surface.Plane();
    gp_Vec direction(plane.Axis().Direction());
    if (face.Orientation() != TopAbs_REVERSED)
        direction.Reverse();
    auto sketchId = placement.value("sketch").toString();
    check(context.references.sketches.contains(sketchId), "ReferenceMissing",
          "Hole placement sketch missing");
    const auto &sketch = context.references.sketches.constFind(sketchId).value();
    TopoDS_Shape tool;
    for (auto point : placement.value("points").toArray()) {
        control.checkpoint("Constructing hole position", 50);
        const auto &entity = sketch.entity(point.toString());
        check(entity.kind == "Point", "InvalidInput", "Hole position must be a sketch point");
        auto origin = kernelPoint(sketch.frame.world(entity.point));
        origin.Translate(-gp_Vec(plane.Axis().Direction()) *
                         gp_Vec(plane.Location(), origin).Dot(gp_Vec(plane.Axis().Direction())));
        check(BRepClass_FaceClassifier(face, origin, 1e-7).State() == TopAbs_IN, "ReferenceMissing",
              "Hole center lies outside the selected face");
        tool =
            join(tool, singleHole(target, intent, origin, direction, context, part, owner, control),
                 part, owner);
    }
    check(!tool.IsNull(), "InvalidInput", "No hole positions specified");
    return tool;
}

QCborMap cosmeticThread(const QCborMap &intent, const Context &context) {
    auto ref = domain::GeometryReference::parse(intent.value("reference").toMap());
    auto shape = context.references.topology(ref);
    TopoDS_Face cylinderFace;
    std::optional<gp_Pnt> entrance;
    if (shape.ShapeType() == TopAbs_FACE) {
        check(BRepAdaptor_Surface(TopoDS::Face(shape)).GetType() == GeomAbs_Cylinder,
              "InvalidInput", "Cosmetic thread requires a cylindrical face");
        cylinderFace = TopoDS::Face(shape);
    } else {
        check(shape.ShapeType() == TopAbs_EDGE, "InvalidInput",
              "Cosmetic thread requires a circular edge or cylindrical face");
        BRepAdaptor_Curve curve(TopoDS::Edge(shape));
        check(curve.GetType() == GeomAbs_Circle, "InvalidInput",
              "Cosmetic thread requires a circular edge");
        auto circle = curve.Circle();
        entrance = circle.Location();
        for (TopExp_Explorer ex(context.references.stages.value(ref.object), TopAbs_FACE);
             ex.More(); ex.Next()) {
            BRepAdaptor_Surface surface(TopoDS::Face(ex.Current()));
            if (surface.GetType() != GeomAbs_Cylinder)
                continue;
            auto cylinder = surface.Cylinder();
            if (gp_Lin(cylinder.Axis()).Distance(*entrance) > 1e-6 ||
                std::abs(cylinder.Radius() - circle.Radius()) > 1e-6)
                continue;
            double u0, u1, v0, v1;
            BRepTools::UVBounds(TopoDS::Face(ex.Current()), u0, u1, v0, v1);
            double station =
                gp_Vec(cylinder.Location(), *entrance).Dot(gp_Vec(cylinder.Axis().Direction()));
            if (std::abs(station - v0) > 1e-6 && std::abs(station - v1) > 1e-6)
                continue;
            check(cylinderFace.IsNull(), "ReferenceAmbiguous",
                  "More than one thread cylinder meets this edge");
            cylinderFace = TopoDS::Face(ex.Current());
        }
    }
    check(!cylinderFace.IsNull(), "ReferenceMissing",
          "Circular edge has no unambiguous cylindrical thread surface");
    BRepAdaptor_Surface surface(cylinderFace);
    auto cylinder = surface.Cylinder();
    double u0, u1, v0, v1;
    BRepTools::UVBounds(cylinderFace, u0, u1, v0, v1);
    gp_Pnt sample;
    gp_Vec du, dv;
    surface.D1((u0 + u1) / 2, (v0 + v1) / 2, sample, du, dv);
    auto normal = du.Crossed(dv);
    if (cylinderFace.Orientation() == TopAbs_REVERSED)
        normal.Reverse();
    auto axis = gp_Vec(cylinder.Axis().Direction());
    auto radial = gp_Vec(cylinder.Location(), sample);
    radial -= axis * radial.Dot(axis);
    bool internal = normal.Dot(radial) < 0;
    check((intent.value("thread_type") == "Internal") == internal, "InvalidInput",
          "Thread type does not match the selected cylindrical surface");
    check(u1 - u0 >= 2 * std::numbers::pi - 1e-6 && v1 > v0, "UnsupportedOperation",
          "Cosmetic thread requires a complete cylindrical surface");
    auto direction = gp_Vec(cylinder.Axis().Direction());
    auto origin = cylinder.Location().Translated(direction * v0);
    if (entrance) {
        origin = *entrance;
        if (std::abs(gp_Vec(cylinder.Location(), origin).Dot(direction) - v1) < 1e-6)
            direction.Reverse();
    }
    auto term = intent.value("termination").toMap();
    double depth = term.value("mode") == "Blind" ? term.value("depth").toDouble() * mm : v1 - v0;
    check(depth <= v1 - v0 + 1e-6, "InvalidInput",
          "Thread depth exceeds the selected cylindrical section");
    auto frame = supportFrame(origin, direction, gp_Vec(cylinder.Position().XDirection()));
    return sketch::fields(
        {{"internal", internal},
         {"id", intent.value("id")},
         {"body", intent.value("target").toMap().value("body")},
         {"designation", intent.value("designation")},
         {"origin", features::array(frame.origin)},
         {"u", features::array(frame.u)},
         {"v", features::array(frame.v)},
         {"direction", features::array({direction.X(), direction.Y(), direction.Z()})},
         {"radius", cylinder.Radius() / mm},
         {"depth", depth / mm},
         {"pitch", intent.value("pitch")}});
}

} // namespace cad::geometry::modeling
