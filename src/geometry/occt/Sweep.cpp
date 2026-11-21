#include "Sweep.hpp"
#include "ModelingProfiles.hpp"
#include "SpatialCurve.hpp"
#include "SupportReferences.hpp"
#include <BRepAdaptor_CompCurve.hxx>
#include <BRepAdaptor_Curve.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepAlgoAPI_Fuse.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepOffsetAPI_MakePipeShell.hxx>
#include <BRepTools.hxx>
#include <Standard_Failure.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <gp_Ax2.hxx>
#include <gp_Circ.hxx>
#include <gp_Dir.hxx>
#include <gp_Pln.hxx>
#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>

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

TopoDS_Shape sweepSingleWire(const TopoDS_Wire &pathWire, const TopoDS_Wire &profileWire,
                             const QCborArray &guidesArr, const QCborMap &intent,
                             const Context &context) {
    BRepOffsetAPI_MakePipeShell pipe(pathWire);

    auto corner = intent.value("corner_transition").toString();
    if (corner == "Sharp")
        pipe.SetTransitionMode(BRepBuilderAPI_RightCorner);
    else
        pipe.SetTransitionMode(BRepBuilderAPI_RoundCorner);

    if (!guidesArr.isEmpty()) {
        auto gRef = domain::GeometryReference::parse(guidesArr[0].toMap());
        auto guideWire = resolvePathWire(gRef, false, context);
        pipe.SetMode(guideWire, true, BRepFill_NoContact);
    } else {
        pipe.SetMode(false);
    }

    pipe.Add(profileWire, false, false);
    pipe.Build();
    check(pipe.IsDone(), "InvalidGeometry", "Pipe shell sweep failed");
    check(pipe.MakeSolid(), "InvalidGeometry", "Sweep make solid failed");
    auto shape = pipe.Shape();
    check(!shape.IsNull() && BRepCheck_Analyzer(shape, true).IsValid(), "InvalidGeometry",
          "Sweep result shape is invalid");
    return shape;
}

} // namespace

TopoDS_Shape sweep(const TopoDS_Shape & /*target*/, const QCborMap &intent, const Context &context,
                   kernel::Part &part, QString owner) {
    try {
        auto pathObj = intent.value("path").toMap();
        auto pathRef = domain::GeometryReference::parse(pathObj.value("reference").toMap());
        bool reversePath = pathObj.value("reverse").toBool();
        auto pathWire = resolvePathWire(pathRef, reversePath, context);

        auto guidesArr = intent.value("guides").toArray();
        auto profileObj = intent.value("profile").toMap();
        auto profileType = profileObj.value("type").toString();
        auto thinObj = intent.value("thin").toMap();
        auto thinMode = thinObj.value("mode").toString();

        TopoDS_Shape tool;

        if (profileType == "Circular") {
            double diameter = profileObj.value("diameter").toDouble() * mm;
            double radius = diameter / 2.0;

            BRepAdaptor_CompCurve comp(pathWire);
            gp_Pnt p0;
            gp_Vec t0;
            comp.D1(comp.FirstParameter(), p0, t0);
            if (t0.Magnitude() < 1e-9)
                t0 = gp_Vec(0, 0, 1);
            else
                t0.Normalize();

            gp_Ax2 axes(p0, gp_Dir(t0));

            if (thinMode == "Solid") {
                gp_Circ c(axes, radius);
                auto wire = BRepBuilderAPI_MakeWire(BRepBuilderAPI_MakeEdge(c).Edge()).Wire();
                tool = sweepSingleWire(pathWire, wire, guidesArr, intent, context);
            } else {
                double rOut = radius, rIn = radius;
                if (thinMode == "OneDirection") {
                    double t = thinObj.value("thickness").toDouble() * mm;
                    if (thinObj.value("reverse").toBool()) {
                        rIn = radius;
                        rOut = radius + t;
                    } else {
                        rOut = radius;
                        rIn = radius - t;
                    }
                } else if (thinMode == "Midplane") {
                    double t = thinObj.value("thickness").toDouble() * mm;
                    rOut = radius + t / 2.0;
                    rIn = radius - t / 2.0;
                } else if (thinMode == "TwoDirection") {
                    double t1 = thinObj.value("thickness1").toDouble() * mm;
                    double t2 = thinObj.value("thickness2").toDouble() * mm;
                    rIn = radius - t1;
                    rOut = radius + t2;
                }
                check(rIn > 1e-6 && rOut > rIn + 1e-6, "InvalidGeometry",
                      "Thin circular sweep radius collapsed");

                gp_Circ cOut(axes, rOut), cIn(axes, rIn);
                auto wOut = BRepBuilderAPI_MakeWire(BRepBuilderAPI_MakeEdge(cOut).Edge()).Wire();
                auto wIn = BRepBuilderAPI_MakeWire(BRepBuilderAPI_MakeEdge(cIn).Edge()).Wire();

                auto solidOut = sweepSingleWire(pathWire, wOut, guidesArr, intent, context);
                auto solidIn = sweepSingleWire(pathWire, wIn, guidesArr, intent, context);

                BRepAlgoAPI_Cut cut(solidOut, solidIn);
                cut.Build();
                check(cut.IsDone() && !cut.HasErrors(), "InvalidGeometry",
                      "Thin sweep subtraction failed");
                tool = cut.Shape();
            }
        } else {
            // Profile is Sketch
            auto sketchId = profileObj.value("sketch").toString();
            check(context.references.sketches.contains(sketchId), "ReferenceMissing",
                  "Sweep profile sketch is missing");
            const auto &sk = context.references.sketches.value(sketchId);

            auto prof = modelingProfile(sk, profileObj, thinObj);

            // If prof.shape contains faces, sweep outer and cut inner holes
            std::vector<TopoDS_Face> faces;
            for (TopExp_Explorer ex(prof.shape, TopAbs_FACE); ex.More(); ex.Next())
                faces.push_back(TopoDS::Face(ex.Current()));

            if (!faces.empty()) {
                TopoDS_Shape combined;
                for (const auto &face : faces) {
                    auto outerWire = BRepTools::OuterWire(face);
                    auto faceSolid =
                        sweepSingleWire(pathWire, outerWire, guidesArr, intent, context);

                    // Cut holes if present
                    for (TopExp_Explorer wex(face, TopAbs_WIRE); wex.More(); wex.Next()) {
                        auto w = TopoDS::Wire(wex.Current());
                        if (w.IsSame(outerWire))
                            continue;
                        auto holeSolid = sweepSingleWire(pathWire, w, guidesArr, intent, context);
                        BRepAlgoAPI_Cut cut(faceSolid, holeSolid);
                        cut.Build();
                        check(cut.IsDone() && !cut.HasErrors(), "InvalidGeometry",
                              "Hole subtraction in sweep failed");
                        faceSolid = cut.Shape();
                    }
                    if (combined.IsNull())
                        combined = faceSolid;
                    else {
                        BRepAlgoAPI_Fuse fuse(combined, faceSolid);
                        fuse.Build();
                        check(fuse.IsDone() && !fuse.HasErrors(), "InvalidGeometry",
                              "Multi-face sweep union failed");
                        combined = fuse.Shape();
                    }
                }
                tool = combined;
            } else {
                // Open profile wire
                TopTools_IndexedMapOfShape wireMap;
                TopExp::MapShapes(prof.shape, TopAbs_WIRE, wireMap);
                check(wireMap.Extent() >= 1, "InvalidGeometry", "No wires in sweep profile");
                tool =
                    sweepSingleWire(pathWire, TopoDS::Wire(wireMap(1)), guidesArr, intent, context);
            }
        }

        kernel::validateShape(tool);
        return tool;
    } catch (const Standard_Failure &e) {
        throw domain::Error("InvalidGeometry",
                            QString("Sweep exact evaluation failed: ") + e.GetMessageString());
    }
}

} // namespace cad::geometry::modeling
