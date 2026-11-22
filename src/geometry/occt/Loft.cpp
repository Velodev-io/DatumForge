#include "Loft.hpp"
#include "ModelingProfiles.hpp"
#include "SpatialCurve.hpp"
#include "SupportReferences.hpp"
#include <BRepAdaptor_CompCurve.hxx>
#include <BRepAdaptor_Curve.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepGProp.hxx>
#include <BRepLib.hxx>
#include <BRepOffsetAPI_MakePipeShell.hxx>
#include <BRepOffsetAPI_ThruSections.hxx>
#include <BRepOffset_MakeOffset.hxx>
#include <BRepTools.hxx>
#include <BRepTools_WireExplorer.hxx>
#include <BRep_Tool.hxx>
#include <GCPnts_AbscissaPoint.hxx>
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

TopoDS_Wire orientWireToConnector(const TopoDS_Wire &wire, double fraction) {
    check(BRep_Tool::IsClosed(wire), "UnsupportedOperation",
          "Loft connectors require closed section wires");
    if (fraction == 0.0 || fraction == 1.0)
        return wire;

    std::vector<TopoDS_Edge> edges;
    std::vector<double> lengths;
    double perimeter = 0.0;
    for (BRepTools_WireExplorer it(wire); it.More(); it.Next()) {
        edges.push_back(it.Current());
        lengths.push_back(GCPnts_AbscissaPoint::Length(BRepAdaptor_Curve(it.Current())));
        perimeter += lengths.back();
    }
    check(perimeter > 1e-7, "InvalidGeometry", "Loft connector section has zero perimeter");
    double distance = fraction * perimeter;
    size_t index = 0;
    while (index + 1 < edges.size() && distance > lengths[index]) {
        distance -= lengths[index++];
    }
    const auto &edge = edges[index];
    const bool reversed = edge.Orientation() == TopAbs_REVERSED;
    double first, last;
    auto curve = BRep_Tool::Curve(edge, first, last);
    check(!curve.IsNull(), "InvalidGeometry", "Loft connector section has no exact curve");
    auto parameter = GCPnts_AbscissaPoint(BRepAdaptor_Curve(edge), reversed ? -distance : distance,
                                          reversed ? last : first);
    check(parameter.IsDone(), "InvalidGeometry", "Loft connector point could not be located");
    const double at = parameter.Parameter();
    BRepBuilderAPI_MakeWire result;
    auto addSegment = [&](double begin, double end) {
        if (std::abs(end - begin) <= 1e-10)
            return;
        auto piece =
            BRepBuilderAPI_MakeEdge(curve, std::min(begin, end), std::max(begin, end)).Edge();
        if (begin > end)
            piece.Reverse();
        result.Add(piece);
    };
    if (edges.size() == 1 && curve->IsPeriodic()) {
        addSegment(at, at + (reversed ? -1 : 1) * (last - first));
    } else {
        addSegment(at, reversed ? first : last);
        for (size_t i = 1; i < edges.size(); ++i)
            result.Add(edges[(index + i) % edges.size()]);
        addSegment(reversed ? last : first, at);
    }
    check(result.IsDone() && BRep_Tool::IsClosed(result.Wire()), "InvalidGeometry",
          "Loft connector failed to close the section wire");
    return result.Wire();
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
        const bool centerline = !intent.value("centerline").isNull();
        check(intent.value("start_condition") == "None" && intent.value("end_condition") == "None",
              "UnsupportedOperation", "Explicit loft end conditions are unavailable");
        check(guidesArr.isEmpty() || (centerline && guidesArr.size() == 1), "UnsupportedOperation",
              "Loft supports one guide in centerline mode");
        check(connArr.isEmpty() || (!centerline && connArr.size() == 1), "UnsupportedOperation",
              "Loft supports one connector without a centerline");

        std::vector<TopoDS_Wire> sectionWires;
        for (qsizetype s = 0; s < sectionsArr.size(); ++s) {
            auto secMap = sectionsArr[s].toMap();
            auto sketchId = secMap.value("sketch").toString();
            check(context.references.sketches.contains(sketchId), "ReferenceMissing",
                  "Loft section sketch is missing");
            const auto &sk = context.references.sketches.value(sketchId);

            auto prof = modelingProfile(sk, secMap, sketch::fields({{"mode", "Solid"}}));
            TopTools_IndexedMapOfShape wires;
            TopExp::MapShapes(prof.shape, TopAbs_WIRE, wires);
            check(wires.Extent() == 1, "UnsupportedOperation",
                  "Loft requires one wire per section. Holes and multiple regions are unavailable");
            auto w = TopoDS::Wire(wires(1));
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

        if (centerline) {
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
            if (!connArr.isEmpty()) {
                const auto edgeCount = [](const TopoDS_Wire &wire) {
                    TopTools_IndexedMapOfShape edges;
                    TopExp::MapShapes(wire, TopAbs_EDGE, edges);
                    return edges.Extent();
                };
                const auto firstCount = edgeCount(sectionWires.front());
                for (const auto &wire : sectionWires)
                    check(edgeCount(wire) == firstCount, "UnsupportedOperation",
                          "Connector sections must have matching edge counts after seam placement");
                thruLoft.CheckCompatibility(false);
            }
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
            double inside = 0.0, outside = 0.0;
            if (thinMode == "OneDirection") {
                const double thickness = thinObj.value("thickness").toDouble() * mm;
                if (thinObj.value("reverse").toBool())
                    outside = thickness;
                else
                    inside = thickness;
            } else if (thinMode == "Midplane") {
                inside = outside = thinObj.value("thickness").toDouble() * mm / 2.0;
            } else {
                inside = thinObj.value("thickness1").toDouble() * mm;
                outside = thinObj.value("thickness2").toDouble() * mm;
            }
            auto offset = [&](double distance) {
                if (distance == 0.0)
                    return tool;
                BRepOffset_MakeOffset maker;
                maker.Initialize(tool, distance, 1e-7, BRepOffset_Skin, false, false,
                                 GeomAbs_Intersection);
                maker.MakeOffsetShape();
                check(maker.IsDone(), "InvalidGeometry", "Thin loft offset failed");
                kernel::validateShape(maker.Shape());
                check(kernel::shapeVolume(maker.Shape()) > 0, "InvalidGeometry",
                      "Thin loft offset did not produce a solid");
                return maker.Shape();
            };
            const auto outer = offset(outside);
            const auto inner = offset(-inside);
            BRepAlgoAPI_Cut wall(outer, inner);
            check(wall.IsDone() && !wall.HasErrors(), "InvalidGeometry",
                  "Thin loft wall subtraction failed");
            tool = wall.Shape();
            kernel::validateShape(tool);
            const double volume = kernel::shapeVolume(tool);
            check(volume > 0 && volume < kernel::shapeVolume(outer) - 1e-9, "InvalidGeometry",
                  "Thin loft did not produce a hollow wall");
        }

        kernel::validateShape(tool);
        return tool;
    } catch (const Standard_Failure &e) {
        throw domain::Error("InvalidGeometry",
                            QString("Loft exact evaluation failed: ") + e.GetMessageString());
    }
}

} // namespace cad::geometry::modeling
