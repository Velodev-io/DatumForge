#include "Treatments.hpp"
#include "SupportReferences.hpp"
#include <BRepAdaptor_Surface.hxx>
#include <BRepAlgoAPI_Check.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepAlgoAPI_Fuse.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepFilletAPI_MakeChamfer.hxx>
#include <BRepFilletAPI_MakeFillet.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <TColgp_Array1OfPnt2d.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <gp_Pln.hxx>

namespace cad::geometry::modeling {
using domain::check;
constexpr double mm = 1000.0;

namespace {
TopoDS_Edge resolveEdge(const TopoDS_Shape &target, const domain::GeometryReference &ref,
                        const Context &context) {
    if (!ref.topology.isEmpty()) {
        for (TopExp_Explorer ex(target, TopAbs_EDGE); ex.More(); ex.Next()) {
            if (supportToken(ex.Current()) == ref.topology)
                return TopoDS::Edge(ex.Current());
        }
    }
    auto shape = context.references.topology(ref);
    check(!shape.IsNull() && shape.ShapeType() == TopAbs_EDGE, "ReferenceMissing",
          "Selected edge is missing or invalid");
    return TopoDS::Edge(shape);
}

TopoDS_Face resolveFace(const TopoDS_Shape &target, const domain::GeometryReference &ref,
                        const Context &context) {
    auto shape = context.references.topology(ref);
    check(!shape.IsNull() && shape.ShapeType() == TopAbs_FACE, "ReferenceMissing",
          "Selected face is missing");
    for (TopExp_Explorer face(target, TopAbs_FACE); face.More(); face.Next())
        if (face.Current().IsSame(shape) || supportToken(face.Current()) == ref.topology)
            return TopoDS::Face(face.Current());
    throw domain::Error("ReferenceMissing", "Reference face is not in the target body");
}

TopoDS_Face faceForEdge(const TopoDS_Shape &target, const TopoDS_Edge &edge) {
    TopTools_IndexedDataMapOfShapeListOfShape edgeFaceMap;
    TopExp::MapShapesAndAncestors(target, TopAbs_EDGE, TopAbs_FACE, edgeFaceMap);
    if (edgeFaceMap.Contains(edge)) {
        const auto &faces = edgeFaceMap.FindFromKey(edge);
        if (!faces.IsEmpty())
            return TopoDS::Face(faces.First());
    }
    throw domain::Error("ReferenceMissing", "Reference face for edge not found");
}

TopoDS_Face otherFaceForEdge(const TopoDS_Shape &target, const TopoDS_Edge &edge,
                             const TopoDS_Face &face) {
    TopTools_IndexedDataMapOfShapeListOfShape edgeFaceMap;
    TopExp::MapShapesAndAncestors(target, TopAbs_EDGE, TopAbs_FACE, edgeFaceMap);
    if (edgeFaceMap.Contains(edge)) {
        const auto &faces = edgeFaceMap.FindFromKey(edge);
        bool adjacent = false;
        for (auto it = faces.cbegin(); it != faces.cend(); ++it)
            adjacent = adjacent || it->IsSame(face);
        check(adjacent, "InvalidInput", "Chamfer reference face does not adjoin the edge");
        for (auto it = faces.cbegin(); it != faces.cend(); ++it)
            if (!it->IsSame(face))
                return TopoDS::Face(*it);
    }
    throw domain::Error("InvalidInput", "Chamfer reference face does not adjoin the edge");
}

} // namespace

TopoDS_Shape fillet(const TopoDS_Shape &target, const QCborMap &intent, const Context &context,
                    kernel::Part &part, QString owner) {
    auto method = intent.value("method").toString();
    check(intent.value("propagate_tangent").toBool(), "UnsupportedOperation",
          "Disabling tangent propagation is not supported by this OCCT adapter");

    if (method == "Face") {
        // CM-183: OCCT 7.9.2 lacks a general face-face blend API.
        throw domain::Error("UnsupportedOperation",
                            "Face fillet blend is blocked in OCCT 7.9.2 (R1)");
    }
    if (method == "FullRound")
        throw domain::Error("UnsupportedOperation",
                            "Full round requires a verified face-set blend adapter (R1)");

    BRepFilletAPI_MakeFillet builder(target);

    if (method == "Constant") {
        double r = intent.value("radius").toDouble() * mm;
        for (auto val : intent.value("edges").toArray()) {
            auto edge = resolveEdge(target, domain::GeometryReference::parse(val.toMap()), context);
            builder.Add(r, edge);
        }
    } else if (method == "MultipleRadius") {
        for (auto val : intent.value("radii").toArray()) {
            auto m = val.toMap();
            double r = m.value("radius").toDouble() * mm;
            auto edge = resolveEdge(
                target, domain::GeometryReference::parse(m.value("edge").toMap()), context);
            builder.Add(r, edge);
        }
    } else if (method == "Variable") {
        auto edge = resolveEdge(
            target, domain::GeometryReference::parse(intent.value("edge").toMap()), context);
        auto pts = intent.value("control_points").toArray();
        TColgp_Array1OfPnt2d arr(1, int(pts.size()));
        for (int i = 0; i < pts.size(); ++i) {
            auto pt = pts[i].toMap();
            arr.SetValue(i + 1, gp_Pnt2d(pt.value("parameter").toDouble(),
                                         pt.value("radius").toDouble() * mm));
        }
        if (intent.value("transition") == "Linear") {
            check(pts.size() == 2, "UnsupportedOperation",
                  "Linear radius transition currently requires two endpoints");
            builder.Add(arr.Value(1).Y(), arr.Value(2).Y(), edge);
        } else {
            builder.Add(arr, edge);
        }
    } else {
        throw domain::Error("UnsupportedOperation", "Unsupported fillet method: " + method);
    }

    builder.Build();
    check(builder.IsDone() && BRepAlgoAPI_Check(builder.Shape()).IsValid(), "InvalidGeometry",
          "Fillet failed; reduce radius or choose supported edges");
    kernel::recordShapeHistory(part, owner, builder, target);
    return builder.Shape();
}

TopoDS_Shape chamfer(const TopoDS_Shape &target, const QCborMap &intent, const Context &context,
                     kernel::Part &part, QString owner) {
    auto method = intent.value("method").toString();
    check(intent.value("propagate_tangent").toBool(), "UnsupportedOperation",
          "Disabling tangent propagation is not supported by this OCCT adapter");
    BRepFilletAPI_MakeChamfer builder(target);

    for (auto val : intent.value("edges").toArray()) {
        auto edge = resolveEdge(target, domain::GeometryReference::parse(val.toMap()), context);
        if (method == "EqualDistance") {
            double d = intent.value("distance").toDouble() * mm;
            builder.Add(d, edge);
        } else if (method == "DistanceDistance") {
            double d1 = intent.value("distance1").toDouble() * mm;
            double d2 = intent.value("distance2").toDouble() * mm;
            bool flip = intent.value("flip").toBool();
            TopoDS_Face face;
            if (intent.contains(QStringLiteral("reference_face")) &&
                !intent.value("reference_face").isNull()) {
                face = resolveFace(
                    target,
                    domain::GeometryReference::parse(intent.value("reference_face").toMap()),
                    context);
            } else {
                face = faceForEdge(target, edge);
            }
            otherFaceForEdge(target, edge, face);
            builder.Add(flip ? d2 : d1, flip ? d1 : d2, edge, face);
        } else if (method == "AngleDistance") {
            double d = intent.value("distance").toDouble() * mm;
            double angle = intent.value("angle").toDouble();
            bool flip = intent.value("flip").toBool();
            TopoDS_Face face;
            if (intent.contains(QStringLiteral("reference_face")) &&
                !intent.value("reference_face").isNull()) {
                face = resolveFace(
                    target,
                    domain::GeometryReference::parse(intent.value("reference_face").toMap()),
                    context);
            } else {
                face = faceForEdge(target, edge);
            }
            otherFaceForEdge(target, edge, face);
            if (flip) {
                auto other = otherFaceForEdge(target, edge, face);
                builder.AddDA(d, angle, edge, other);
            } else {
                builder.AddDA(d, angle, edge, face);
            }
        } else {
            throw domain::Error("UnsupportedOperation", "Unsupported chamfer method: " + method);
        }
    }

    builder.Build();
    check(builder.IsDone() && BRepAlgoAPI_Check(builder.Shape()).IsValid(), "InvalidGeometry",
          "Chamfer failed; check dimensions and edges");
    kernel::recordShapeHistory(part, owner, builder, target);
    return builder.Shape();
}

} // namespace cad::geometry::modeling
