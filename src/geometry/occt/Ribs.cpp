#include "Ribs.hpp"
#include "SupportReferences.hpp"
#include <BRepAdaptor_Surface.hxx>
#include <BRepAlgoAPI_Check.hxx>
#include <BRepAlgoAPI_Fuse.hxx>
#include <BRepBndLib.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <BRep_Tool.hxx>
#include <Bnd_Box.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>

namespace cad::geometry::modeling {
using domain::check;
constexpr double mm = 1000.0;

TopoDS_Shape rib(const TopoDS_Shape &target, const QCborMap &intent, const Context &context,
                 kernel::Part &part, QString owner) {
    auto selection = intent.value("profile").toMap();
    auto sketchId = selection.value("sketch").toString();
    check(context.references.sketches.contains(sketchId), "ReferenceMissing",
          "Rib profile sketch missing");
    const auto &sk = context.references.sketches.value(sketchId);

    auto profile = modelingProfile(sk, selection, sketch::fields({{"mode", "Solid"}}));

    double thickness = intent.value("thickness").toDouble() * mm;
    auto side = intent.value("side").toString();
    auto dir = intent.value("direction").toString();
    bool flip = intent.value("flip_material").toBool();

    gp_Pln pln = profile.plane;
    gp_Dir normal = pln.Axis().Direction();

    double tStart = 0, tEnd = thickness;
    if (side == "Both") {
        tStart = -thickness / 2.0;
        tEnd = thickness / 2.0;
    } else if (side == "Second") {
        tStart = -thickness;
        tEnd = 0.0;
    }

    Bnd_Box bnd;
    BRepBndLib::AddOptimal(target, bnd, false, false);
    double xmin, ymin, zmin, xmax, ymax, zmax;
    bnd.Get(xmin, ymin, zmin, xmax, ymax, zmax);
    double diag = gp_Pnt(xmin, ymin, zmin).Distance(gp_Pnt(xmax, ymax, zmax));

    TopoDS_Shape ribSolid;
    if (!profile.open && profile.shape.ShapeType() == TopAbs_FACE) {
        gp_Vec shift = gp_Vec(normal) * tStart;
        gp_Trsf t;
        t.SetTranslation(shift);
        BRepBuilderAPI_Transform moved(profile.shape, t, true);
        ribSolid = BRepPrimAPI_MakePrism(moved.Shape(), gp_Vec(normal) * (tEnd - tStart)).Shape();
    } else {
        gp_Vec extDir;
        if (dir == "Parallel") {
            TopExp_Explorer edgeEx(profile.shape, TopAbs_EDGE);
            check(edgeEx.More(), "InvalidGeometry", "Rib profile wire has no edges");
            auto e = TopoDS::Edge(edgeEx.Current());
            Standard_Real f, l;
            auto c = BRep_Tool::Curve(e, f, l);
            gp_Pnt pMid;
            gp_Vec vTang;
            c->D1((f + l) / 2.0, pMid, vTang);
            extDir = gp_Vec(normal).Crossed(vTang).Normalized();
            if (flip)
                extDir.Reverse();
        } else {
            extDir = gp_Vec(normal);
            if (flip)
                extDir.Reverse();
        }

        auto sheet = BRepPrimAPI_MakePrism(profile.shape, extDir * (diag * 1.5)).Shape();
        gp_Vec tShift = gp_Vec(normal) * tStart;
        gp_Trsf t;
        t.SetTranslation(tShift);
        BRepBuilderAPI_Transform moved(sheet, t, true);
        ribSolid = BRepPrimAPI_MakePrism(moved.Shape(), gp_Vec(normal) * (tEnd - tStart)).Shape();
    }

    BRepAlgoAPI_Fuse fuse(target, ribSolid);
    fuse.Build();
    check(fuse.IsDone() && BRepAlgoAPI_Check(fuse.Shape()).IsValid(), "InvalidGeometry",
          "Rib failed; profile must meet target body");

    kernel::recordShapeHistory(part, owner, fuse, target);
    kernel::recordShapeHistory(part, owner, fuse, ribSolid);
    part.tools[owner] = ribSolid;
    return fuse.Shape();
}

} // namespace cad::geometry::modeling
