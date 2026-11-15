#include "SupportReferences.hpp"
#include <BRepAdaptor_Curve.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepClass_FaceClassifier.hxx>
#include <BRepGProp.hxx>
#include <BRepTools.hxx>
#include <BRep_Tool.hxx>
#include <GProp_GProps.hxx>
#include <GeomAPI_ProjectPointOnSurf.hxx>
#include <GeomAdaptor_Curve.hxx>
#include <Geom_BSplineCurve.hxx>
#include <Geom_BezierCurve.hxx>
#include <TopExp.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopoDS.hxx>
#include <gp_Pnt2d.hxx>
namespace cad::geometry {
namespace {
QCborMap lineage(QString original, const QCborArray &history, const QStringList &current) {
    if (current.contains(original))
        return sketch::fields({{"state", "Direct"}, {"candidates", QCborArray{original}}});
    QMap<QString, QSet<QString>> next, previous;
    QSet<QString> deleted;
    for (auto value : history) {
        auto record = value.toMap();
        auto input = record.value("input").toString();
        if (record.value("deleted").toBool())
            deleted.insert(input);
        for (auto target : record.value("modified").toArray()) {
            next[input].insert(target.toString());
            if (target.toString() != input)
                previous[target.toString()].insert(input);
        }
    }
    QSet<QString> seen, candidates;
    QStringList queue{original};
    bool split = false, merged = false, removed = false;
    while (!queue.empty()) {
        auto token = queue.takeLast();
        if (seen.contains(token))
            continue;
        seen.insert(token);
        domain::check(seen.size() <= 8192, "ResourceLimit",
                      "Topology lineage traversal exceeds admission");
        if (current.contains(token))
            candidates.insert(token);
        split = split || next.value(token).size() > 1;
        merged = merged || previous.value(token).size() > 1;
        removed = removed || deleted.contains(token);
        for (auto target : next.value(token))
            if (target != token)
                queue.append(target);
    }
    auto tokens = candidates.values();
    std::sort(tokens.begin(), tokens.end());
    QString state = split || tokens.size() > 1 ? "Split"
                    : merged                   ? "Merged"
                    : tokens.size() == 1       ? (tokens[0] == original ? "Direct" : "History")
                    : removed                  ? "Deleted"
                                               : "Missing";
    return sketch::fields({{"state", state}, {"candidates", QCborArray::fromStringList(tokens)}});
}
sketch::Frame surfaceFrame(const TopoDS_Face &face, double u, double v) {
    BRepClass_FaceClassifier classifier(face, gp_Pnt2d(u, v), 1e-7);
    domain::check(classifier.State() == TopAbs_IN || classifier.State() == TopAbs_ON,
                  "ReferenceMissing", "Selected surface point is outside the trimmed face");
    BRepAdaptor_Surface surface(face);
    gp_Pnt p;
    gp_Vec du, dv;
    surface.D1(u, v, p, du, dv);
    domain::check(du.Magnitude() > 1e-12 && dv.Magnitude() > 1e-12, "InvalidGeometry",
                  "Surface parameter is singular");
    auto n = du.Normalized().Crossed(dv.Normalized());
    if (face.Orientation() == TopAbs_REVERSED)
        n.Reverse();
    return supportFrame(p, n, du);
}
SupportCurve edgeCurve(const TopoDS_Edge &edge, QString key) {
    double first, last;
    auto c = BRep_Tool::Curve(edge, first, last);
    domain::check(!c.IsNull() && std::isfinite(first) && std::isfinite(last) && last > first,
                  "InvalidGeometry", "A finite exact edge is required");
    if (edge.Orientation() == TopAbs_REVERSED)
        std::swap(first, last);
    return {key, c, first, last, 1, true, {}};
}
} // namespace
QString supportToken(const TopoDS_Shape &s) {
    if (s.ShapeType() != TopAbs_VERTEX)
        return kernel::topologyToken(s);
    auto p = BRep_Tool::Pnt(TopoDS::Vertex(s));
    auto bytes = QString("%1/%2/%3")
                     .arg(p.X(), 0, 'f', 7)
                     .arg(p.Y(), 0, 'f', 7)
                     .arg(p.Z(), 0, 'f', 7)
                     .toUtf8();
    return "vertex/" +
           QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex());
}
gp_Pnt kernelPoint(sketch::Vec3 p) {
    return {p[0] * 1000, p[1] * 1000, p[2] * 1000};
}
sketch::Vec3 siPoint(gp_Pnt p) {
    return {p.X() / 1000, p.Y() / 1000, p.Z() / 1000};
}
gp_Vec vector(sketch::Vec3 v) {
    return {v[0], v[1], v[2]};
}
sketch::Vec3 coordinates(gp_Vec v) {
    return {v.X(), v.Y(), v.Z()};
}
sketch::Frame supportFrame(gp_Pnt p, gp_Vec normal, gp_Vec hint) {
    domain::check(normal.SquareMagnitude() > 1e-20, "InvalidGeometry", "Degenerate support normal");
    normal.Normalize();
    auto u = hint - normal * hint.Dot(normal);
    if (u.SquareMagnitude() <= 1e-20) {
        hint = std::abs(normal.X()) < .8 ? gp_Vec(1, 0, 0) : gp_Vec(0, 1, 0);
        u = hint - normal * hint.Dot(normal);
    }
    u.Normalize();
    sketch::Frame result{siPoint(p), coordinates(u), coordinates(normal.Crossed(u))};
    result.validate();
    return result;
}
void SupportReferences::scope(const domain::GeometryReference &r) const {
    r.validate();
    domain::check(r.document == "Self" || r.document == document, "UnsupportedOperation",
                  "External document geometry closure is not implemented");
}
sketch::Frame SupportReferences::plane(const domain::GeometryReference &r) const {
    scope(r);
    if (r.role == "Face") {
        auto face = TopoDS::Face(topology(r));
        BRepAdaptor_Surface surface(face);
        domain::check(surface.GetType() == GeomAbs_Plane, "UnsupportedOperation",
                      "A planar support face is required");
        auto plane = surface.Plane();
        auto n = gp_Vec(plane.Axis().Direction());
        if (face.Orientation() == TopAbs_REVERSED)
            n.Reverse();
        return supportFrame(plane.Location(), n, gp_Vec(plane.Position().XDirection()));
    }
    domain::check(r.role == "Plane", "UnsupportedOperation", "A plane reference is required");
    if (planes.contains(r.object))
        return planes.value(r.object);
    domain::check(sketches.contains(r.object), "ReferenceMissing", "Source plane is missing");
    return sketches.value(r.object).frame;
}
SupportCurve SupportReferences::curve(const domain::GeometryReference &r) const {
    scope(r);
    domain::check(r.role == "Curve" || r.role == "Edge", "InvalidInput",
                  "Exact curve reference required");
    if (r.role == "Edge")
        return edgeCurve(TopoDS::Edge(topology(r)), r.object + "/" + r.topology);
    if (derived.contains(r.object)) {
        auto values = derived.value(r.object);
        domain::check(values.size() == 1 && !values[0].point, "ReferenceAmbiguous",
                      "Select one derived curve");
        return values[0];
    }
    domain::check(sketches.contains(r.object) && r.role == "Curve", "ReferenceMissing",
                  "Curve source is missing");
    const auto &s = sketches.constFind(r.object).value();
    const auto &e = s.entity(r.entity);
    domain::check(e.kind != "Point", "ReferenceMissing", "Curve member role changed");
    auto value = sketchCurve(s, e);
    return {r.object + "/" + r.entity, value.curve, value.first, value.last,
            value.parameterScale,      false,       {}};
}
double SupportReferences::parameter(const domain::GeometryReference &r) const {
    auto c = curve(r);
    domain::check(r.parameter || !r.endpoint.isEmpty(), "InvalidInput",
                  "Select a curve endpoint or parameter");
    double t = r.endpoint == "Start" ? c.first
               : r.endpoint == "End" ? c.last
               : c.edge              ? c.first + *r.parameter * (c.last - c.first)
                                     : *r.parameter * c.parameterScale;
    domain::check(std::isfinite(t) && t >= std::min(c.first, c.last) - 1e-10 &&
                      t <= std::max(c.first, c.last) + 1e-10,
                  "ReferenceMissing", "Selected point is outside the trimmed curve");
    return t;
}
gp_Pnt SupportReferences::point(const domain::GeometryReference &r) const {
    scope(r);
    if (r.role == "Vertex")
        return BRep_Tool::Pnt(TopoDS::Vertex(topology(r)));
    if (r.role == "Point") {
        domain::check(sketches.contains(r.object), "ReferenceMissing", "Point source is missing");
        const auto &s = sketches.constFind(r.object).value();
        const auto &e = s.entity(r.entity);
        domain::check(e.kind == "Point", "ReferenceMissing", "Source point role changed");
        return kernelPoint(s.frame.world(e.point));
    }
    auto c = curve(r);
    return c.curve->Value(parameter(r));
}
SupportAxis SupportReferences::axis(const domain::GeometryReference &r) const {
    scope(r);
    if (r.role == "Face") {
        BRepAdaptor_Surface s(TopoDS::Face(topology(r)));
        domain::check(s.GetType() == GeomAbs_Cylinder || s.GetType() == GeomAbs_Cone,
                      "UnsupportedOperation", "Cylindrical/conical face required for an axis");
        auto a = s.GetType() == GeomAbs_Cylinder ? s.Cylinder().Axis() : s.Cone().Axis();
        return {siPoint(a.Location()), coordinates(gp_Vec(a.Direction()))};
    }
    if (r.role == "Axis") {
        domain::check(axes.contains(r.object), "ReferenceMissing", "Axis source is missing");
        return axes.value(r.object);
    }
    auto c = curve(r);
    GeomAdaptor_Curve adaptor(c.curve, std::min(c.first, c.last), std::max(c.first, c.last));
    if (adaptor.GetType() == GeomAbs_BSplineCurve || adaptor.GetType() == GeomAbs_BezierCurve) {
        auto start = c.curve->Value(c.first), end = c.curve->Value(c.last);
        auto direction = gp_Vec(start, end);
        domain::check(direction.Magnitude() > 1e-6, "InvalidGeometry",
                      "A nondegenerate straight curve is required");
        direction.Normalize();
        int count = adaptor.GetType() == GeomAbs_BSplineCurve ? adaptor.BSpline()->NbPoles()
                                                              : adaptor.Bezier()->NbPoles();
        domain::check(count <= 64, "ResourceLimit", "Axis curve exceeds bounded control admission");
        for (int i = 1; i <= count; ++i) {
            auto p = adaptor.GetType() == GeomAbs_BSplineCurve ? adaptor.BSpline()->Pole(i)
                                                               : adaptor.Bezier()->Pole(i);
            domain::check(gp_Vec(start, p).Crossed(direction).Magnitude() <= 1e-7,
                          "UnsupportedOperation",
                          "Curved control geometry cannot define a straight axis");
        }
        return {siPoint(start), coordinates(direction)};
    }
    domain::check(adaptor.GetType() == GeomAbs_Line, "UnsupportedOperation",
                  "Straight line/edge required for an axis");
    auto line = adaptor.Line();
    auto direction = gp_Vec(line.Direction());
    if (c.last < c.first)
        direction.Reverse();
    return {siPoint(c.curve->Value(c.first)), coordinates(direction)};
}
QCborMap SupportReferences::inspect(const domain::GeometryReference &r, bool replacements) const {
    scope(r);
    domain::check(r.role == "Face" || r.role == "Edge" || r.role == "Vertex",
                  "UnsupportedOperation",
                  "Exact topology inspection requires a face, edge or vertex operand");
    domain::check(stages.contains(r.object), "ReferenceMissing",
                  "Producing feature stage is unavailable");
    auto kind = r.role == "Face" ? TopAbs_FACE : r.role == "Edge" ? TopAbs_EDGE : TopAbs_VERTEX;
    TopTools_IndexedMapOfShape shapes;
    TopExp::MapShapes(stages.value(r.object), kind, shapes);
    QMap<QString, int> counts;
    for (int i = 1; i <= shapes.Extent(); ++i)
        ++counts[supportToken(shapes(i))];
    domain::check(!replacements || counts.size() <= 256, "ResourceLimit",
                  "Reference inspection exceeds 256 candidates");
    QCborArray sourceHistory;
    for (auto value : history) {
        auto feature = value.toMap().value("feature").toString();
        if (!stageOrder.contains(r.object) ||
            (stageOrder.contains(feature) &&
             stageOrder.value(feature) <= stageOrder.value(r.object)))
            sourceHistory.append(value);
    }
    auto resolved = lineage(r.topology, sourceHistory, counts.keys());
    auto state = resolved.value("state").toString();
    auto mapped = resolved.value("candidates").toArray();
    if ((state == "Direct" || state == "History") && mapped.size() == 1 &&
        counts.value(mapped[0].toString()) != 1)
        resolved.insert(QStringLiteral("state"), "Ambiguous");
    QCborArray candidates;
    QCborArray descriptions;
    for (auto it = counts.cbegin(); it != counts.cend(); ++it)
        if (replacements && it.value() == 1) {
            auto candidate = r;
            candidate.topology = it.key();
            candidates.append(candidate.map());
            for (int i = 1; i <= shapes.Extent(); ++i) {
                auto shape = shapes(i);
                if (supportToken(shape) != it.key())
                    continue;
                gp_Pnt center;
                QString type = r.role;
                if (r.role == "Vertex")
                    center = BRep_Tool::Pnt(TopoDS::Vertex(shape));
                else {
                    GProp_GProps properties;
                    if (r.role == "Face") {
                        BRepGProp::SurfaceProperties(shape, properties);
                        auto surface = BRepAdaptor_Surface(TopoDS::Face(shape)).GetType();
                        type = surface == GeomAbs_Plane      ? "Plane"
                               : surface == GeomAbs_Cylinder ? "Cylinder"
                               : surface == GeomAbs_Cone     ? "Cone"
                                                             : "Surface";
                    } else {
                        BRepGProp::LinearProperties(shape, properties);
                        type = BRepAdaptor_Curve(TopoDS::Edge(shape)).GetType() == GeomAbs_Line
                                   ? "Line"
                                   : "Curve";
                    }
                    center = properties.CentreOfMass();
                }
                descriptions.append(sketch::fields({{"reference", candidate.map()},
                                                    {"center_si", features::array(siPoint(center))},
                                                    {"geometry_type", type}}));
                break;
            }
        }
    resolved.insert(QStringLiteral("reference"), r.map());
    resolved.insert(QStringLiteral("replacements"), candidates);
    resolved.insert(QStringLiteral("descriptions"), descriptions);
    return resolved;
}
TopoDS_Shape SupportReferences::topology(const domain::GeometryReference &r) const {
    auto resolution = inspect(r, false);
    auto state = resolution.value("state").toString();
    domain::check(state != "Split" && state != "Merged" && state != "Ambiguous",
                  "ReferenceAmbiguous",
                  "Topology lineage is not unique; choose a replacement explicitly");
    domain::check(state == "Direct" || state == "History", "ReferenceMissing",
                  "Source topology is missing; choose a replacement explicitly");
    auto token = resolution.value("candidates").toArray()[0].toString();
    TopTools_IndexedMapOfShape all;
    auto kind = r.role == "Face" ? TopAbs_FACE : r.role == "Edge" ? TopAbs_EDGE : TopAbs_VERTEX;
    TopExp::MapShapes(stages.value(r.object), kind, all);
    TopoDS_Shape found;
    for (int i = 1; i <= all.Extent(); ++i)
        if (supportToken(all(i)) == token) {
            domain::check(found.IsNull(), "ReferenceAmbiguous", "Duplicate topology signature");
            found = all(i);
        }
    domain::check(!found.IsNull(), "ReferenceMissing", "Resolved topology is unavailable");
    auto record = sketch::fields({{"reference", r.map()}, {"state", state}, {"resolved", token}});
    if (!evolution.contains(record))
        evolution.append(record);
    return found;
}
sketch::Frame SupportReferences::tangent(const domain::GeometryReference &r, double u,
                                         double v) const {
    scope(r);
    domain::check(r.role == "Face" && std::isfinite(u) && std::isfinite(v) && u >= 0 && u <= 1 &&
                      v >= 0 && v <= 1,
                  "InvalidInput", "Explicit normalized face UV fractions required");
    auto face = TopoDS::Face(topology(r));
    BRepAdaptor_Surface surface(face);
    domain::check(surface.GetType() != GeomAbs_Plane, "UnsupportedOperation",
                  "Use a planar face directly as support");
    double u0, u1, v0, v1;
    BRepTools::UVBounds(face, u0, u1, v0, v1);
    domain::check(std::isfinite(u0) && std::isfinite(u1) && std::isfinite(v0) && std::isfinite(v1),
                  "InvalidGeometry", "Finite trimmed surface required");
    return surfaceFrame(face, u0 + u * (u1 - u0), v0 + v * (v1 - v0));
}
gp_Vec SupportReferences::normalAt(const domain::GeometryReference &r, gp_Pnt p) const {
    if (r.role == "Plane") {
        auto f = plane(r);
        return vector(f.u).Crossed(vector(f.v));
    }
    auto face = TopoDS::Face(topology(r));
    BRepAdaptor_Surface surface(face);
    if (surface.GetType() == GeomAbs_Plane) {
        auto f = plane(r);
        return vector(f.u).Crossed(vector(f.v));
    }
    double u0, u1, v0, v1;
    BRepTools::UVBounds(face, u0, u1, v0, v1);
    GeomAPI_ProjectPointOnSurf project(p, BRep_Tool::Surface(face), u0, u1, v0, v1);
    std::optional<gp_Vec> normal;
    for (int i = 1; i <= project.NbPoints(); ++i) {
        if (project.Point(i).Distance(p) > 1e-7)
            continue;
        double u, v;
        project.Parameters(i, u, v);
        auto f = surfaceFrame(face, u, v);
        auto n = vector(f.u).Crossed(vector(f.v));
        domain::check(!normal || (*normal - n).Magnitude() < 1e-10, "ReferenceAmbiguous",
                      "Point has multiple face normals");
        normal = n;
    }
    domain::check(normal.has_value(), "ReferenceMissing", "Point is not on the selected face");
    return *normal;
}
std::vector<SupportCurve> SupportReferences::curves(const domain::GeometryReference &r) const {
    scope(r);
    if (r.role == "Point" || r.role == "Vertex" || !r.endpoint.isEmpty() || r.parameter) {
        auto key = QString::fromLatin1(
            QCryptographicHash::hash(QCborValue(r.map()).toCbor(QCborValue::SortKeysInMaps),
                                     QCryptographicHash::Sha256)
                .toHex());
        return {{key, {}, 0, 0, 1, false, point(r)}};
    }
    if (r.role != "Face")
        return {curve(r)};
    TopTools_IndexedMapOfShape edges;
    TopExp::MapShapes(topology(r), TopAbs_EDGE, edges);
    domain::check(edges.Extent() > 0 && edges.Extent() <= 128, "ResourceLimit",
                  "Face boundary exceeds conversion admission");
    QMap<QString, SupportCurve> unique;
    for (int i = 1; i <= edges.Extent(); ++i) {
        auto token = supportToken(edges(i));
        domain::check(!unique.contains(token), "ReferenceAmbiguous",
                      "Face boundary has ambiguous edge signatures");
        unique.insert(token, edgeCurve(TopoDS::Edge(edges(i)), r.object + "/" + token));
    }
    std::vector<SupportCurve> result;
    for (auto &c : unique)
        result.push_back(c);
    return result;
}
} // namespace cad::geometry
