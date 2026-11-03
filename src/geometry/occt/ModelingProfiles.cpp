#include "ModelingProfiles.hpp"
#include "SketchCurve.hpp"
#include "SupportReferences.hpp"
#include <BRepAlgoAPI_Check.hxx>
#include <BRepAlgoAPI_Common.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepAlgoAPI_Fuse.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepExtrema_DistShapeShape.hxx>
#include <BRepGProp.hxx>
#include <BRepOffsetAPI_MakeOffset.hxx>
#include <BRepTools.hxx>
#include <BRepTools_WireExplorer.hxx>
#include <BRepTopAdaptor_FClass2d.hxx>
#include <BRep_Builder.hxx>
#include <BRep_Tool.hxx>
#include <GProp_GProps.hxx>
#include <Standard_Failure.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopoDS.hxx>
namespace cad::geometry {
using domain::check;
namespace {
constexpr double tolerance = 1e-7;
double area(const TopoDS_Shape &shape) {
    GProp_GProps p;
    BRepGProp::SurfaceProperties(shape, p);
    return p.Mass();
}
TopoDS_Face boundedFace(const gp_Pln &plane, TopoDS_Wire wire) {
    BRepBuilderAPI_MakeFace make(plane, wire, true);
    check(make.IsDone(), "InvalidGeometry", "Selected boundary did not form a face");
    if (BRepTopAdaptor_FClass2d(make.Face(), tolerance).PerformInfinitePoint() == TopAbs_IN) {
        wire.Reverse();
        make = BRepBuilderAPI_MakeFace(plane, wire, true);
    }
    check(make.IsDone() && BRepCheck_Analyzer(make.Face()).IsValid(), "InvalidGeometry",
          "Selected boundary has invalid topology");
    check(BRepAlgoAPI_Check(make.Face()).IsValid(), "InvalidGeometry",
          "Selected boundary self-intersects");
    check(area(make.Face()) > 1e-12, "InvalidGeometry", "Selected boundary has no positive area");
    return make.Face();
}
TopoDS_Wire selectedWire(const sketch::Sketch &s, const QCborArray &uses, bool closed) {
    check(!uses.empty() && uses.size() <= 256, "ResourceLimit", "Bounded curve boundary required");
    BRepBuilderAPI_MakeWire make;
    gp_Pnt first, previous;
    bool initial = true;
    for (auto value : uses) {
        auto use = value.toMap();
        auto &entity = s.entity(use.value("entity").toString());
        check(!entity.construction && entity.kind != "Point", "InvalidInput",
              "Profile member must be a nonconstruction curve");
        auto exact = sketchCurve(s, entity);
        double from = exact.first + (exact.last - exact.first) * use.value("from").toDouble();
        double to = exact.first + (exact.last - exact.first) * use.value("to").toDouble();
        auto edge =
            BRepBuilderAPI_MakeEdge(exact.curve, std::min(from, to), std::max(from, to)).Edge();
        if (to < from)
            edge.Reverse();
        auto a = exact.curve->Value(from), b = exact.curve->Value(to);
        if (use.value("reverse").toBool()) {
            edge.Reverse();
            std::swap(a, b);
        }
        if (initial)
            first = a;
        else
            check(previous.Distance(a) <= tolerance, "ReferenceMissing",
                  "Selected curve intervals no longer connect");
        make.Add(edge);
        check(make.IsDone(), "InvalidGeometry", "Selected wire construction failed");
        previous = b;
        initial = false;
    }
    check((previous.Distance(first) <= tolerance) == closed, "InvalidGeometry",
          "Profile closure does not match selected mode");
    return make.Wire();
}
TopoDS_Wire offsetWire(const gp_Pln &plane, const TopoDS_Wire &wire, double left, bool open) {
    if (left == 0)
        return wire;
    // The source wire must also belong to the carrier face: OCCT uses that
    // orientation to distinguish positive/right from negative/left offsets.
    auto carrier = BRepBuilderAPI_MakeFace(plane, wire, false).Face();
    bool clockwise =
        !open && BRepTopAdaptor_FClass2d(carrier, tolerance).PerformInfinitePoint() == TopAbs_IN;
    if (!open)
        carrier = boundedFace(plane, wire);
    BRepOffsetAPI_MakeOffset offset;
    offset.Init(carrier, GeomAbs_Intersection, open);
    offset.SetApprox(false);
    offset.Perform(clockwise ? left : -left);
    check(offset.IsDone(), "InvalidGeometry", "Exact thin offset failed or collapsed");
    TopTools_IndexedMapOfShape wires;
    TopExp::MapShapes(offset.Shape(), TopAbs_WIRE, wires);
    check(wires.Extent() == 1 && BRepCheck_Analyzer(wires(1)).IsValid(), "InvalidGeometry",
          "Thin offset changed branch count");
    return TopoDS::Wire(wires(1));
}
std::pair<gp_Pnt, gp_Pnt> endpoints(const TopoDS_Wire &wire) {
    BRepTools_WireExplorer edge(wire);
    check(edge.More(), "InvalidGeometry", "Empty offset wire");
    auto first = BRep_Tool::Pnt(TopExp::FirstVertex(edge.Current(), true));
    gp_Pnt last;
    for (; edge.More(); edge.Next())
        last = BRep_Tool::Pnt(TopExp::LastVertex(edge.Current(), true));
    return {first, last};
}
TopoDS_Shape thinSection(const ModelingProfile &profile, const std::vector<TopoDS_Wire> &wires,
                         const QCborMap &thin) {
    double left = 0, right = 0;
    auto mode = thin.value("mode").toString();
    if (mode == "OneDirection") {
        if (thin.value("reverse").toBool())
            right = thin.value("thickness").toDouble() * 1000;
        else
            left = thin.value("thickness").toDouble() * 1000;
    } else if (mode == "Midplane")
        left = right = thin.value("thickness").toDouble() * 500;
    else {
        left = thin.value("thickness1").toDouble() * 1000;
        right = thin.value("thickness2").toDouble() * 1000;
    }
    check(!wires.empty() && wires.size() <= 16, "ResourceLimit",
          "Exact thin admission is 1–16 boundaries");
    TopoDS_Shape result;
    for (auto &wire : wires) {
        auto a = offsetWire(profile.plane, wire, left, profile.open),
             b = offsetWire(profile.plane, wire, -right, profile.open);
        TopoDS_Shape band;
        if (profile.open) {
            auto ae = endpoints(a), be = endpoints(b);
            if (ae.first.Distance(be.second) < ae.first.Distance(be.first)) {
                b.Reverse();
                std::swap(be.first, be.second);
            }
            BRepBuilderAPI_MakeWire closed;
            closed.Add(a);
            closed.Add(BRepBuilderAPI_MakeEdge(ae.second, be.second).Edge());
            closed.Add(TopoDS::Wire(b.Reversed()));
            closed.Add(BRepBuilderAPI_MakeEdge(be.first, ae.first).Edge());
            check(closed.IsDone(), "InvalidGeometry", "Thin end closure failed");
            band = boundedFace(profile.plane, closed.Wire());
        } else {
            auto first = boundedFace(profile.plane, a), second = boundedFace(profile.plane, b);
            if (area(first) < area(second))
                std::swap(first, second);
            BRepAlgoAPI_Common overlap(first, second);
            check(overlap.IsDone() && std::abs(area(overlap.Shape()) - area(second)) <= 1e-8,
                  "InvalidGeometry", "Thin boundaries intersect or collapse");
            BRepAlgoAPI_Cut cut(first, second);
            check(cut.IsDone() && !cut.HasErrors() && area(cut.Shape()) > 1e-12, "InvalidGeometry",
                  "Thin wall subtraction failed");
            band = cut.Shape();
        }
        if (result.IsNull())
            result = band;
        else {
            BRepAlgoAPI_Common overlap(result, band);
            check(overlap.IsDone() && area(overlap.Shape()) <= 1e-8, "InvalidGeometry",
                  "Thin wall bands overlap");
            BRepAlgoAPI_Fuse fuse(result, band);
            check(fuse.IsDone() && !fuse.HasErrors(), "InvalidGeometry", "Thin wall union failed");
            result = fuse.Shape();
        }
    }
    return result;
}
} // namespace
ModelingProfile modelingProfile(const sketch::Sketch &s, const QCborMap &selection,
                                const QCborMap &thickness) {
    s.validate();
    check(selection.value("sketch").toString() == s.id, "ReferenceMissing",
          "Profile sketch changed");
    auto normal = vector(s.frame.u).Crossed(vector(s.frame.v));
    ModelingProfile result;
    result.plane = gp_Pln(kernelPoint(s.frame.origin), gp_Dir(normal));
    try {
        if (selection.value("mode").toString() == "AllClosed")
            result.shape = kernel::profileShape(s);
        else {
            result.open = selection.value("mode").toString() == "OpenChains";
            auto regions = selection.value("regions").toArray();
            check(!regions.empty() && regions.size() <= 16, "ResourceLimit",
                  "Exact profile admission is 1–16 regions");
            qsizetype boundaryCount = 0;
            for (auto region : regions)
                boundaryCount += region.toArray().size();
            check(boundaryCount <= 16, "ResourceLimit",
                  "Exact profile admission is at most 16 boundaries including holes");
            TopoDS_Compound compound;
            BRep_Builder builder;
            builder.MakeCompound(compound);
            TopoDS_Shape joined;
            for (auto regionValue : regions) {
                auto region = regionValue.toArray();
                check(!region.empty(), "InvalidInput", "Region boundary required");
                auto outer = selectedWire(s, region[0].toArray(), !result.open);
                if (result.open) {
                    check(region.size() == 1, "InvalidInput", "Open chain has no holes");
                    builder.Add(compound, outer);
                    continue;
                }
                TopoDS_Shape face = boundedFace(result.plane, outer);
                for (qsizetype i = 1; i < region.size(); ++i) {
                    auto hole =
                        boundedFace(result.plane, selectedWire(s, region[i].toArray(), true));
                    for (TopExp_Explorer boundary(face, TopAbs_WIRE); boundary.More();
                         boundary.Next()) {
                        BRepExtrema_DistShapeShape separation(boundary.Current(),
                                                              BRepTools::OuterWire(hole));
                        check(separation.IsDone() && separation.Value() > tolerance,
                              "InvalidGeometry", "Hole boundaries touch or intersect their region");
                    }
                    BRepAlgoAPI_Common overlap(face, hole);
                    check(overlap.IsDone() && std::abs(area(overlap.Shape()) - area(hole)) <= 1e-8,
                          "InvalidGeometry", "Hole is outside its region or overlaps another hole");
                    BRepAlgoAPI_Cut cut(face, hole);
                    check(cut.IsDone() && !cut.HasErrors(), "InvalidGeometry",
                          "Region hole subtraction failed");
                    face = cut.Shape();
                }
                if (joined.IsNull())
                    joined = face;
                else {
                    BRepAlgoAPI_Common overlap(joined, face);
                    check(overlap.IsDone() && area(overlap.Shape()) <= 1e-8, "InvalidGeometry",
                          "Selected regions overlap");
                    BRepAlgoAPI_Fuse fuse(joined, face);
                    check(fuse.IsDone() && !fuse.HasErrors(), "InvalidGeometry",
                          "Selected region union failed");
                    joined = fuse.Shape();
                }
            }
            result.shape = result.open ? TopoDS_Shape(compound) : joined;
        }
        if (thickness.value("mode").toString() != "Solid") {
            std::vector<TopoDS_Wire> wires;
            if (selection.value("mode").toString() == "AllClosed") {
                for (TopExp_Explorer face(result.shape, TopAbs_FACE); face.More(); face.Next())
                    for (TopExp_Explorer wire(face.Current(), TopAbs_WIRE); wire.More();
                         wire.Next())
                        wires.push_back(TopoDS::Wire(wire.Current()));
            } else
                for (auto region : selection.value("regions").toArray())
                    for (auto boundary : region.toArray())
                        wires.push_back(selectedWire(s, boundary.toArray(), !result.open));
            result.shape = thinSection(result, wires, thickness);
            result.open = false;
        }
        check(!result.shape.IsNull() && BRepCheck_Analyzer(result.shape).IsValid() &&
                  BRepAlgoAPI_Check(result.shape).IsValid(),
              "InvalidGeometry", "Invalid or self-intersecting selected profile");
        return result;
    } catch (const Standard_Failure &e) {
        throw domain::Error("InvalidGeometry",
                            QString("Exact profile failed: ") + e.GetMessageString());
    }
}
} // namespace cad::geometry
