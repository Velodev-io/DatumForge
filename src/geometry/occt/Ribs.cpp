#include "Ribs.hpp"
#include <BRepAdaptor_Curve.hxx>
#include <BRepAlgoAPI_Check.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
namespace cad::geometry::modeling {
using domain::check;
TopoDS_Shape rib(const TopoDS_Shape &target, const QCborMap &intent, const Context &context,
                 kernel::Part &part, QString owner, const exchange::Control &control) {
    auto selection = intent.value("profile").toMap();
    auto sketchId = selection.value("sketch").toString();
    check(context.references.sketches.contains(sketchId), "ReferenceMissing",
          "Rib profile sketch missing");
    const auto &sk = context.references.sketches.constFind(sketchId).value();
    auto side = intent.value("side").toString();
    double thickness = intent.value("thickness").toDouble();
    auto profile = modelingProfile(sk, selection, sketch::fields({{"mode", "Solid"}}));
    bool flip = intent.value("flip_material").toBool();
    if (intent.value("direction") == "Parallel") {
        check(profile.open, "UnsupportedOperation",
              "Parallel ribs currently require a single straight open chain");
        TopExp_Explorer ex(profile.shape, TopAbs_EDGE);
        check(ex.More(), "InvalidGeometry", "Rib chain is empty");
        auto edge = TopoDS::Edge(ex.Current());
        ex.Next();
        check(!ex.More(), "UnsupportedOperation",
              "Parallel ribs currently require a single straight open chain");
        BRepAdaptor_Curve curve(edge);
        check(curve.GetType() == GeomAbs_Line, "UnsupportedOperation",
              "Parallel ribs currently require a straight chain");
        auto normal = gp_Vec(profile.plane.Axis().Direction());
        double start = side == "Both" ? -thickness * 500 : side == "Second" ? -thickness * 1000 : 0;
        auto shifted = moved(edge, normal * start);
        BRepPrimAPI_MakePrism sheet(shifted, normal * (thickness * 1000));
        check(sheet.IsDone() && sheet.Shape().ShapeType() == TopAbs_FACE, "InvalidGeometry",
              "Rib section failed");
        kernel::recordShapeHistory(part, owner, sheet, shifted);
        gp_Pnt midpoint;
        gp_Vec tangent;
        curve.D1((curve.FirstParameter() + curve.LastParameter()) / 2, midpoint, tangent);
        if (edge.Orientation() == TopAbs_REVERSED)
            tangent.Reverse();
        auto direction = normal.Crossed(tangent).Normalized();
        if (flip)
            direction.Reverse();
        profile.shape = sheet.Shape();
        profile.plane = gp_Pln(midpoint, gp_Dir(direction));
        profile.open = false;
    } else {
        auto thin = side == "Both"
                        ? sketch::fields({{"mode", "Midplane"}, {"thickness", thickness}})
                        : sketch::fields({{"mode", "OneDirection"},
                                          {"thickness", thickness},
                                          {"reverse", side == "Second"}});
        profile = modelingProfile(sk, selection, thin);
    }
    auto extrusion = sketch::fields(
        {{"id", owner},
         {"operation", "Add"},
         {"start", sketch::fields({{"mode", "SketchPlane"}})},
         {"reverse", intent.value("direction") == "Normal" && flip},
         {"direction1",
          sketch::fields({{"mode", "UpToNext"},
                          {"draft", sketch::fields({{"angle", 0.0}, {"outward", false}})}})},
         {"direction2", nullptr}});
    return extrude(profile, extrusion, context, target, part, control);
}
} // namespace cad::geometry::modeling
