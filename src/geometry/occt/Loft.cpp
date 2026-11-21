#include "Loft.hpp"
#include "ModelingProfiles.hpp"
#include "SpatialCurve.hpp"
#include "SupportReferences.hpp"
#include <BRepAdaptor_CompCurve.hxx>
#include <BRepAdaptor_Curve.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepGProp.hxx>
#include <BRepLib.hxx>
#include <BRepOffsetAPI_MakePipeShell.hxx>
#include <BRepOffsetAPI_ThruSections.hxx>
#include <BRepOffset_MakeOffset.hxx>
#include <BRepTools.hxx>
#include <BRep_Tool.hxx>
#include <GProp_GProps.hxx>
#include <Geom_Circle.hxx>
#include <Standard_Failure.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Solid.hxx>
#include <gp_Circ.hxx>

namespace cad::geometry::modeling {
using domain::check;
constexpr double mm = 1000.0;

namespace {

TopoDS_Wire resolvePathWire(const domain::GeometryReference &ref, bool reverse,
                            const Context &context) {
    if (ref.role == "Edge") {
        auto shape = context.references.topology(ref);
        check(!shape.IsNull() && shape.ShapeType() == TopAbs_EDGE, "ReferenceMissing",
              "Selected path edge is missing");
        BRepBuilderAPI_MakeWire wb(TopoDS::Edge(shape));
        check(wb.IsDone(), "InvalidGeometry", "Path edge wire assembly failed");
        auto w = wb.Wire();
        if (reverse)
            w.Reverse();
        return w;
    }
    if (context.references.derived.contains(ref.object)) {
        auto curves = context.references.derived.value(ref.object);
        check(!curves.empty(), "ReferenceMissing", "Derived path curve is empty");
        BRepBuilderAPI_MakeWire wb;
        for (const auto &c : curves) {
            if (c.point)
                continue;
            wb.Add(BRepBuilderAPI_MakeEdge(c.curve, c.first, c.last).Edge());
        }
        check(wb.IsDone(), "InvalidGeometry", "Derived path wire failed");
        auto w = wb.Wire();
        if (reverse)
            w.Reverse();
        return w;
    }
    if (context.references.spatialSketches.contains(ref.object)) {
        const auto &s = context.references.spatialSketches.value(ref.object);
        if (!ref.entity.isEmpty()) {
            const auto &e = s.entity(ref.entity);
            auto c = spatialEntityCurve(s, e);
            BRepBuilderAPI_MakeWire wb(BRepBuilderAPI_MakeEdge(c.curve, c.first, c.last).Edge());
            check(wb.IsDone(), "InvalidGeometry", "Spatial entity wire failed");
            auto w = wb.Wire();
            if (reverse)
                w.Reverse();
            return w;
        }
    }
    if (context.references.sketches.contains(ref.object)) {
        const auto &s = context.references.sketches.value(ref.object);
        if (!ref.entity.isEmpty()) {
            const auto &e = s.entity(ref.entity);
            auto c = sketchCurve(s, e);
            BRepBuilderAPI_MakeWire wb(BRepBuilderAPI_MakeEdge(c.curve, c.first, c.last).Edge());
            check(wb.IsDone(), "InvalidGeometry", "Sketch entity wire failed");
            auto w = wb.Wire();
            if (reverse)
                w.Reverse();
            return w;
        }
        std::vector<TopoDS_Edge> edges;
        for (const auto &e : s.entities) {
            if (e.construction || e.kind == "Point")
                continue;
            auto c = sketchCurve(s, e);
            edges.push_back(BRepBuilderAPI_MakeEdge(c.curve, c.first, c.last).Edge());
        }
        check(!edges.empty(), "ReferenceMissing", "Sketch path has no edges");
        BRepBuilderAPI_MakeWire wb;
        for (const auto &edge : edges)
            wb.Add(edge);
        check(wb.IsDone(), "InvalidGeometry", "Sketch path wire assembly failed");
        auto w = wb.Wire();
        if (reverse)
            w.Reverse();
        return w;
    }
    auto c = context.references.curve(ref);
    BRepBuilderAPI_MakeWire wb(BRepBuilderAPI_MakeEdge(c.curve, c.first, c.last).Edge());
    check(wb.IsDone(), "InvalidGeometry", "Path wire failed");
    auto w = wb.Wire();
    if (reverse)
        w.Reverse();
    return w;
}

TopoDS_Wire orientWireToConnector(const TopoDS_Wire &originalWire, double param) {
    if (std::abs(param) < 1e-6)
        return originalWire;

    // For single periodic edge wires (circle/ellipse), shift start angle by 2*pi*param
    TopTools_IndexedMapOfShape edgeMap;
    TopExp::MapShapes(originalWire, TopAbs_EDGE, edgeMap);
    if (edgeMap.Extent() == 1) {
        auto edge = TopoDS::Edge(edgeMap(1));
        Standard_Real f, l;
        auto curve = BRep_Tool::Curve(edge, f, l);
        if (!curve.IsNull() && curve->IsPeriodic()) {
            double shift = param * (l - f);
            double newF = f + shift;
            double newL = l + shift;
            auto shiftedEdge = BRepBuilderAPI_MakeEdge(curve, newF, newL).Edge();
            return BRepBuilderAPI_MakeWire(shiftedEdge).Wire();
        }
    }
    return originalWire;
}

} // namespace

TopoDS_Shape loft(const TopoDS_Shape & /*target*/, const QCborMap &intent, const Context &context,
                  kernel::Part &part, QString owner) {
    try {
        auto output = intent.value("output").toString();
        auto sectionsArr = intent.value("sections").toArray();
        auto guidesArr = intent.value("guides").toArray();
        auto thinObj = intent.value("thin").toMap();
        auto thinMode = thinObj.value("mode").toString();
        auto connArr = intent.value("connectors").toArray();

        std::vector<TopoDS_Wire> sectionWires;
        for (qsizetype s = 0; s < sectionsArr.size(); ++s) {
            auto secMap = sectionsArr[s].toMap();
            auto sketchId = secMap.value("sketch").toString();
            check(context.references.sketches.contains(sketchId), "ReferenceMissing",
                  "Loft section sketch is missing");
            const auto &sk = context.references.sketches.value(sketchId);

            auto prof = modelingProfile(sk, secMap, sketch::fields({{"mode", "Solid"}}));
            TopoDS_Wire w;
            if (output == "Solid") {
                for (TopExp_Explorer ex(prof.shape, TopAbs_FACE); ex.More(); ex.Next()) {
                    w = BRepTools::OuterWire(TopoDS::Face(ex.Current()));
                    break;
                }
            } else {
                TopTools_IndexedMapOfShape wireMap;
                TopExp::MapShapes(prof.shape, TopAbs_WIRE, wireMap);
                check(wireMap.Extent() >= 1, "InvalidGeometry",
                      "No wire found in surface loft section");
                w = TopoDS::Wire(wireMap(1));
            }
            check(!w.IsNull(), "InvalidGeometry", "Loft section wire is empty");

            // If connector specifies parameter for section s, adjust seam orientation
            if (!connArr.isEmpty()) {
                auto firstConn = connArr[0].toMap();
                auto ptsArr = firstConn.value("points").toArray();
                if (s < ptsArr.size()) {
                    double p = ptsArr[s].toMap().value("parameter").toDouble();
                    w = orientWireToConnector(w, p);
                }
            }
            sectionWires.push_back(w);
        }

        TopoDS_Shape tool;

        if (!intent.value("centerline").isNull()) {
            auto clObj = intent.value("centerline").toMap();
            auto clRef = domain::GeometryReference::parse(clObj.value("reference").toMap());
            bool clRev = clObj.value("reverse").toBool();
            auto clWire = resolvePathWire(clRef, clRev, context);

            BRepOffsetAPI_MakePipeShell pipeLoft(clWire);
            if (!guidesArr.isEmpty()) {
                auto gRef = domain::GeometryReference::parse(guidesArr[0].toMap());
                auto guideWire = resolvePathWire(gRef, false, context);
                pipeLoft.SetMode(guideWire, true, BRepFill_NoContact);
            } else {
                pipeLoft.SetMode(false);
            }

            for (const auto &w : sectionWires)
                pipeLoft.Add(w, false, false);

            pipeLoft.Build();
            check(pipeLoft.IsDone(), "InvalidGeometry", "Centerline guided loft failed");
            if (output == "Solid") {
                check(pipeLoft.MakeSolid(), "InvalidGeometry", "Centerline loft make solid failed");
            }
            tool = pipeLoft.Shape();
        } else {
            bool isSolid = (output == "Solid");
            BRepOffsetAPI_ThruSections thruLoft(isSolid, false, 1e-6);
            for (const auto &w : sectionWires)
                thruLoft.AddWire(w);
            thruLoft.Build();
            check(thruLoft.IsDone(), "InvalidGeometry", "Through-sections loft failed");
            tool = thruLoft.Shape();
        }

        if (output == "Solid") {
            for (TopExp_Explorer solid(tool, TopAbs_SOLID); solid.More(); solid.Next()) {
                auto s = TopoDS::Solid(solid.Current());
                BRepLib::OrientClosedSolid(s);
            }
        }

        check(!tool.IsNull() && BRepCheck_Analyzer(tool, true).IsValid(), "InvalidGeometry",
              "Resulting loft shape is invalid");

        if (output == "Solid" && thinMode != "Solid") {
            double t = 0;
            bool inward = true;
            if (thinMode == "OneDirection") {
                t = thinObj.value("thickness").toDouble() * mm;
                inward = !thinObj.value("reverse").toBool();
            } else if (thinMode == "Midplane") {
                t = thinObj.value("thickness").toDouble() * mm / 2.0;
                inward = true;
            } else if (thinMode == "TwoDirection") {
                t = (thinObj.value("thickness1").toDouble() +
                     thinObj.value("thickness2").toDouble()) *
                    mm / 2.0;
                inward = true;
            }
            BRepOffset_MakeOffset offsetMaker;
            offsetMaker.Initialize(tool, inward ? -t : t, 1e-7, BRepOffset_Skin, false, false,
                                   GeomAbs_Intersection);
            offsetMaker.MakeThickSolid();
            if (offsetMaker.IsDone() && BRepCheck_Analyzer(offsetMaker.Shape(), true).IsValid()) {
                tool = offsetMaker.Shape();
            }
        }

        kernel::validateShape(tool);
        return tool;
    } catch (const Standard_Failure &e) {
        throw domain::Error("InvalidGeometry",
                            QString("Loft exact evaluation failed: ") + e.GetMessageString());
    }
}

} // namespace cad::geometry::modeling
