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
    if (!ref.topology.isEmpty()) {
        for (TopExp_Explorer ex(target, TopAbs_FACE); ex.More(); ex.Next()) {
            if (supportToken(ex.Current()) == ref.topology)
                return TopoDS::Face(ex.Current());
        }
    }
    auto shape = context.references.topology(ref);
    check(!shape.IsNull() && shape.ShapeType() == TopAbs_FACE, "ReferenceMissing",
          "Selected face is missing or invalid");
    return TopoDS::Face(shape);
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
        for (auto it = faces.cbegin(); it != faces.cend(); ++it) {
            if (!it->IsSame(face))
                return TopoDS::Face(*it);
        }
    }
    return face;
}

// E2 experiment ladder: restricted exact full round between parallel planar side faces
std::optional<TopoDS_Shape> fullRoundRestricted(const TopoDS_Shape &target,
                                                const std::vector<TopoDS_Face> &side1,
                                                const std::vector<TopoDS_Face> &center,
                                                const std::vector<TopoDS_Face> &side2,
                                                kernel::Part &part, QString owner) {
    if (side1.size() != 1 || center.size() != 1 || side2.size() != 1)
        return std::nullopt;
    BRepAdaptor_Surface s1(side1[0]), sc(center[0]), s2(side2[0]);
    if (s1.GetType() != GeomAbs_Plane || s2.GetType() != GeomAbs_Plane ||
        sc.GetType() != GeomAbs_Plane)
        return std::nullopt;
    gp_Pln pln1 = s1.Plane(), pln2 = s2.Plane();
    gp_Dir d1 = pln1.Axis().Direction(), d2 = pln2.Axis().Direction();
    if (std::abs(d1.Dot(d2)) < 1.0 - 1e-6)
        return std::nullopt; // side faces must be parallel planes

    double dist = pln1.Distance(pln2.Location());
    if (dist < 1e-6)
        return std::nullopt;
    double radius = dist / 2.0;

    // Find the center face boundary edges sharing side1 and side2
    TopTools_IndexedDataMapOfShapeListOfShape efMap;
    TopExp::MapShapesAndAncestors(target, TopAbs_EDGE, TopAbs_FACE, efMap);
    std::vector<TopoDS_Edge> targetEdges;
    for (TopExp_Explorer ex(center[0], TopAbs_EDGE); ex.More(); ex.Next()) {
        const auto &edge = TopoDS::Edge(ex.Current());
        if (efMap.Contains(edge)) {
            const auto &faces = efMap.FindFromKey(edge);
            for (auto it = faces.cbegin(); it != faces.cend(); ++it) {
                if (it->IsSame(side1[0]) || it->IsSame(side2[0]))
                    targetEdges.push_back(edge);
            }
        }
    }
    if (targetEdges.empty())
        return std::nullopt;

    BRepFilletAPI_MakeFillet fillet(target);
    for (const auto &e : targetEdges)
        fillet.Add(radius, e);
    fillet.Build();
    if (fillet.IsDone() && BRepAlgoAPI_Check(fillet.Shape()).IsValid()) {
        kernel::recordShapeHistory(part, owner, fillet, target);
        return fillet.Shape();
    }
    return std::nullopt;
}
} // namespace

TopoDS_Shape fillet(const TopoDS_Shape &target, const QCborMap &intent, const Context &context,
                    kernel::Part &part, QString owner) {
    auto method = intent.value("method").toString();
    bool propagate = intent.value("propagate_tangent").toBool(true);

    if (method == "Face") {
        // CM-183: OCCT 7.9.2 lacks a general face-face blend API.
        throw domain::Error("UnsupportedOperation",
                            "Face fillet blend is blocked in OCCT 7.9.2 (R1)");
    }
    if (method == "FullRound") {
        std::vector<TopoDS_Face> s1, sc, s2;
        for (auto f : intent.value("side_faces1").toArray())
            s1.push_back(resolveFace(target, domain::GeometryReference::parse(f.toMap()), context));
        for (auto f : intent.value("center_faces").toArray())
            sc.push_back(resolveFace(target, domain::GeometryReference::parse(f.toMap()), context));
        for (auto f : intent.value("side_faces2").toArray())
            s2.push_back(resolveFace(target, domain::GeometryReference::parse(f.toMap()), context));

        auto exact = fullRoundRestricted(target, s1, sc, s2, part, owner);
        if (exact.has_value())
            return exact.value();
        // CM-184: general full round stays honestly Blocked per plan R1
        throw domain::Error(
            "UnsupportedOperation",
            "Full round blend is blocked for non-parallel faces in OCCT 7.9.2 (R1)");
    }

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
        builder.Add(arr, edge);
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
