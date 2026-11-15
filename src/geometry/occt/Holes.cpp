#include "Holes.hpp"
#include "SupportReferences.hpp"
#include <BRepAdaptor_Surface.hxx>
#include <BRepAlgoAPI_Check.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepAlgoAPI_Fuse.hxx>
#include <BRepBndLib.hxx>
#include <BRepPrimAPI_MakeCone.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <Bnd_Box.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <cmath>
#include <numbers>

namespace cad::geometry::modeling {
using domain::check;
constexpr double mm = 1000.0;

namespace {
gp_Vec faceInwardNormal(const TopoDS_Face &face, gp_Pnt atPoint) {
    BRepAdaptor_Surface surf(face);
    if (surf.GetType() == GeomAbs_Plane) {
        auto pln = surf.Plane();
        gp_Dir n = pln.Axis().Direction();
        if (face.Orientation() == TopAbs_REVERSED)
            n.Reverse();
        // Inward normal into the solid is opposite to outward surface normal
        return -gp_Vec(n);
    }
    return gp_Vec(0, 0, -1);
}

TopoDS_Shape makeSingleHoleTool(const QCborMap &intent, gp_Pnt origin, gp_Vec inwardAxis,
                                double bodyBndSize) {
    auto family = intent.value("family").toString();
    auto sizing = intent.value("sizing").toMap();
    auto term = intent.value("termination").toMap();
    auto tmode = term.value("mode").toString();
    auto dref = term.value("depth_reference").toString();

    double dia = sizing.value("diameter").toDouble() * mm;
    double radius = dia / 2.0;
    double depth = sizing.value("depth").toDouble() * mm;

    gp_Dir dir(inwardAxis);
    gp_Pnt pos = origin;

    // Small clearance step above entrance to ensure clean cut
    double margin = 0.1; // 0.1 mm
    pos.Translate(-gp_Vec(dir) * margin);

    if (tmode == "ThroughAll") {
        depth = bodyBndSize * 2.0 + 100.0;
    }

    if (family == "Simple" || family == "LegacySimple") {
        return BRepPrimAPI_MakeCylinder(gp_Ax2(pos, dir), radius, depth + margin).Shape();
    } else if (family == "Counterbore" || family == "LegacyCounterboreDrill") {
        double cbDia = sizing.value("counterbore_diameter").toDouble() * mm;
        double cbDepth = sizing.value("counterbore_depth").toDouble() * mm;
        double cbRadius = cbDia / 2.0;

        auto tool1 = BRepPrimAPI_MakeCylinder(gp_Ax2(pos, dir), cbRadius, cbDepth + margin).Shape();
        gp_Pnt p2 = pos;
        p2.Translate(gp_Vec(dir) * (cbDepth + margin));
        double mainDepth = std::max(1e-3, depth - cbDepth);
        auto tool2 = BRepPrimAPI_MakeCylinder(gp_Ax2(p2, dir), radius, mainDepth).Shape();

        BRepAlgoAPI_Fuse f(tool1, tool2);
        f.Build();
        return f.Shape();
    } else if (family == "Countersink" || family == "LegacyCountersinkDrill") {
        double csDia = sizing.value("countersink_diameter").toDouble() * mm;
        double csAngle = sizing.value("countersink_angle").toDouble();
        double csRadius = csDia / 2.0;
        double csHeight = (csRadius - radius) / std::tan(csAngle / 2.0);

        auto cone =
            BRepPrimAPI_MakeCone(gp_Ax2(pos, dir), csRadius + margin * std::tan(csAngle / 2.0),
                                 radius, csHeight + margin)
                .Shape();
        gp_Pnt p2 = pos;
        p2.Translate(gp_Vec(dir) * (csHeight + margin));
        double mainDepth = std::max(1e-3, depth - csHeight);
        auto cyl = BRepPrimAPI_MakeCylinder(gp_Ax2(p2, dir), radius, mainDepth).Shape();

        BRepAlgoAPI_Fuse f(cone, cyl);
        f.Build();
        return f.Shape();
    } else if (family == "Drilled" || family == "LegacySimpleDrilled") {
        double drillAngle = sizing.value("drill_angle").toDouble(118.0 * std::numbers::pi / 180.0);
        double tipHeight = radius / std::tan(drillAngle / 2.0);
        double cylDepth = (dref == "Tip") ? std::max(1e-3, depth - tipHeight) : depth;

        auto cyl = BRepPrimAPI_MakeCylinder(gp_Ax2(pos, dir), radius, cylDepth + margin).Shape();
        gp_Pnt pTip = pos;
        pTip.Translate(gp_Vec(dir) * (cylDepth + margin));
        auto cone = BRepPrimAPI_MakeCone(gp_Ax2(pTip, dir), radius, 0.0, tipHeight).Shape();

        BRepAlgoAPI_Fuse f(cyl, cone);
        f.Build();
        return f.Shape();
    } else if (family == "Tapered" || family == "LegacyTapered" || family == "PipeTap") {
        double taperAngle = sizing.value("taper_angle").toDouble(1.7899 * std::numbers::pi / 180.0);
        double rTop = radius + (depth + margin) * std::tan(taperAngle / 2.0);
        return BRepPrimAPI_MakeCone(gp_Ax2(pos, dir), rTop, radius, depth + margin).Shape();
    } else if (family == "Tapped") {
        double tapDia = sizing.value("tap_drill_diameter").toDouble(radius * 2.0 * 0.85 / mm) * mm;
        double tapRadius = tapDia / 2.0;
        return BRepPrimAPI_MakeCylinder(gp_Ax2(pos, dir), tapRadius, depth + margin).Shape();
    }

    return BRepPrimAPI_MakeCylinder(gp_Ax2(pos, dir), radius, depth + margin).Shape();
}
} // namespace

TopoDS_Shape hole(const TopoDS_Shape &target, const QCborMap &intent, const Context &context,
                  kernel::Part &part, QString owner) {
    auto placement = intent.value("placement").toMap();
    auto faceRef = domain::GeometryReference::parse(placement.value("face").toMap());
    TopoDS_Face face;
    if (!faceRef.topology.isEmpty()) {
        for (TopExp_Explorer ex(target, TopAbs_FACE); ex.More(); ex.Next()) {
            if (supportToken(ex.Current()) == faceRef.topology) {
                face = TopoDS::Face(ex.Current());
                break;
            }
        }
    }
    if (face.IsNull()) {
        try {
            face = TopoDS::Face(context.references.topology(faceRef));
        } catch (...) {
            if (context.references.sketches.contains(placement.value("sketch").toString())) {
                const auto &sk =
                    context.references.sketches.value(placement.value("sketch").toString());
                gp_Pnt skOrig = kernelPoint(sk.frame.origin);
                for (TopExp_Explorer ex(target, TopAbs_FACE); ex.More(); ex.Next()) {
                    BRepAdaptor_Surface surf(TopoDS::Face(ex.Current()));
                    if (surf.GetType() == GeomAbs_Plane) {
                        gp_Pln pln = surf.Plane();
                        if (pln.Distance(skOrig) < 1e-4) {
                            face = TopoDS::Face(ex.Current());
                            break;
                        }
                    }
                }
            }
        }
    }
    check(!face.IsNull(), "ReferenceMissing", "Hole placement face missing");

    auto skId = placement.value("sketch").toString();
    check(context.references.sketches.contains(skId), "ReferenceMissing",
          "Hole placement sketch missing");
    const auto &sk = context.references.sketches.value(skId);

    Bnd_Box bnd;
    BRepBndLib::AddOptimal(target, bnd, false, false);
    double xmin, ymin, zmin, xmax, ymax, zmax;
    bnd.Get(xmin, ymin, zmin, xmax, ymax, zmax);
    double bndDiag = gp_Pnt(xmin, ymin, zmin).Distance(gp_Pnt(xmax, ymax, zmax));

    std::vector<TopoDS_Shape> tools;
    for (auto ptVal : placement.value("points").toArray()) {
        auto ptId = ptVal.toString();
        const auto &ptEnt = sk.entity(ptId);
        gp_Pnt p3d = kernelPoint(sk.frame.world(ptEnt.point));
        BRepAdaptor_Surface surf(face);
        if (surf.GetType() == GeomAbs_Plane) {
            auto pln = surf.Plane();
            gp_Dir n = pln.Axis().Direction();
            double signedDist = gp_Vec(pln.Location(), p3d).Dot(gp_Vec(n));
            p3d.Translate(-gp_Vec(n) * signedDist);
        }
        gp_Vec normal = faceInwardNormal(face, p3d);

        tools.push_back(makeSingleHoleTool(intent, p3d, normal, bndDiag));
    }

    check(!tools.empty(), "InvalidInput", "No hole positions specified");

    TopoDS_Shape combinedTool = tools[0];
    for (size_t i = 1; i < tools.size(); ++i) {
        BRepAlgoAPI_Fuse f(combinedTool, tools[i]);
        f.Build();
        combinedTool = f.Shape();
    }

    BRepAlgoAPI_Cut cut(target, combinedTool);
    cut.Build();
    check(cut.IsDone() && BRepAlgoAPI_Check(cut.Shape()).IsValid(), "InvalidGeometry",
          "Hole cut failed; check placement face and boundaries");

    kernel::recordShapeHistory(part, owner, cut, target);
    kernel::recordShapeHistory(part, owner, cut, combinedTool);
    part.tools[owner] = combinedTool;
    return cut.Shape();
}

QCborMap cosmeticThread(const QCborMap &intent, const Context &context) {
    auto edgeRef = domain::GeometryReference::parse(intent.value("reference").toMap());
    auto shape = context.references.topology(edgeRef);
    check(!shape.IsNull(), "ReferenceMissing", "Cosmetic thread target reference missing");

    auto term = intent.value("termination").toMap();
    double depth =
        term.value("mode").toString() == "Blind" ? term.value("depth").toDouble() : 0.025;

    return sketch::fields({{"kind", "CosmeticThreadOverlay"},
                           {"designation", intent.value("designation")},
                           {"major_diameter", intent.value("major_diameter")},
                           {"minor_diameter", intent.value("minor_diameter")},
                           {"pitch", intent.value("pitch")},
                           {"depth", depth}});
}

} // namespace cad::geometry::modeling
