#include "SupportFixture.hpp"
#include "geometry/occt/Datums.hpp"
#include "sketch/Curves.hpp"
#include <BRepAdaptor_Surface.hxx>
#include <BRepBuilderAPI_Copy.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepPrimAPI_MakeCone.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRepPrimAPI_MakeSphere.hxx>
#include <BRep_Builder.hxx>
#include <BRep_Tool.hxx>
#include <QCoreApplication>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Compound.hxx>
#include <numbers>
using namespace cad;
using namespace support_fixture;
void offsetPlanes() {
    geometry::SupportReferences scene;
    sketch::Sketch base;
    scene.sketches.insert(base.id, base);
    model::Datum d;
    d.references = {ref(base.id, "Plane")};
    d.parameters = sketch::fields({{"distance", .055}, {"count", 3}});
    auto output = geometry::datum(d, scene);
    auto ids = d.outputs();
    vector(output.planes.value(ids[0]).origin, {0, 0, .055});
    vector(output.planes.value(ids[1]).origin, {0, 0, .110});
    vector(output.planes.value(ids[2]).origin, {0, 0, .165});
    base.frame.origin[2] = .01;
    scene.sketches[base.id] = base;
    d.reversed = true;
    auto updated = geometry::datum(d, scene);
    vector(updated.planes.value(ids[0]).origin, {0, 0, .065});
    vector(updated.planes.value(ids[0]).v, {0, -1, 0});
    d.parameters.insert(QStringLiteral("distance"), -.055);
    vector(geometry::datum(d, scene).planes.value(ids[0]).origin, {0, 0, -.045});
}
void principalDatums() {
    geometry::SupportReferences scene;
    model::Datum d;
    d.mode = "Principal";
    for (auto name : {"XY", "XZ", "YZ"}) {
        d.parameters = sketch::fields({{"plane", name}});
        auto f = geometry::datum(d, scene).planes.value(d.id);
        vector(f.origin, {0, 0, 0});
        auto n = geometry::coordinates(geometry::vector(f.u).Crossed(geometry::vector(f.v)));
        vector(n, QString(name) == "XY"   ? sketch::Vec3{0, 0, 1}
                  : QString(name) == "XZ" ? sketch::Vec3{0, -1, 0}
                                          : sketch::Vec3{1, 0, 0});
    }
    d.kind = "DatumAxis";
    for (auto name : {"X", "Y", "Z"}) {
        d.parameters = sketch::fields({{"axis", name}});
        auto a = geometry::datum(d, scene).axes.value(d.id);
        vector(a.direction, QString(name) == "X"   ? sketch::Vec3{1, 0, 0}
                            : QString(name) == "Y" ? sketch::Vec3{0, 1, 0}
                                                   : sketch::Vec3{0, 0, 1});
    }
}
void planeMethods() {
    geometry::SupportReferences scene;
    sketch::Sketch base, upper, side;
    auto p0 = base.point({0, 0}), p1 = base.point({.02, 0}), p2 = base.point({0, .03});
    auto xline = base.line(p0, p1);
    auto circle = base.circle(p0, .02);
    upper.frame.origin = {0, 0, .04};
    auto u0 = upper.point({0, 0}), u1 = upper.point({.02, 0});
    auto parallel = upper.line(u0, u1);
    auto cross = upper.line(u0, upper.point({0, .02}));
    side.frame.v = {0, 0, 1};
    scene.sketches = {{base.id, base}, {upper.id, upper}, {side.id, side}};
    auto bp = ref(base.id, "Plane"), up = ref(upper.id, "Plane");
    auto point0 = ref(base.id, "Point", p0), point1 = ref(base.id, "Point", p1),
         point2 = ref(base.id, "Point", p2);
    model::Datum d;
    auto evaluate = [&](QString mode, std::vector<domain::GeometryReference> refs,
                        QCborMap parameters = {}) {
        d.mode = mode;
        d.references = std::move(refs);
        d.parameters = parameters;
        return geometry::datum(d, scene).planes.value(d.id);
    };
    vector(evaluate("ParallelPoint", {bp, ref(upper.id, "Point", u0)}).origin, {0, 0, .04});
    auto angle = evaluate("Angle", {bp, ref(base.id, "Curve", xline)},
                          sketch::fields({{"angle", std::numbers::pi / 6}}));
    vector(geometry::coordinates(geometry::vector(angle.u).Crossed(geometry::vector(angle.v))),
           {0, -.5, std::sqrt(.75)});
    vector(evaluate("ThreePoints", {point0, point1, point2}).u, {1, 0, 0});
    auto lp = evaluate("LinePoint", {ref(base.id, "Curve", xline), ref(upper.id, "Point", u0)});
    vector(lp.v, {0, 0, 1});
    auto two =
        evaluate("TwoLines", {ref(base.id, "Curve", xline), ref(upper.id, "Curve", parallel)});
    vector(two.v, {0, 0, 1});
    error("InvalidGeometry", [&] {
        evaluate("TwoLines", {ref(base.id, "Curve", xline), ref(upper.id, "Curve", cross)});
    });
    error("InvalidGeometry", [&] { evaluate("ThreePoints", {point0, point1, point1}); });
    error("InvalidGeometry",
          [&] { evaluate("LinePoint", {ref(base.id, "Curve", xline), point1}); });
    auto contact = ref(base.id, "Curve", circle);
    contact.parameter = std::numbers::pi / 2;
    auto normal = evaluate("NormalCurve", {contact});
    vector(normal.origin, {0, .02, 0});
    vector(geometry::coordinates(geometry::vector(normal.u).Crossed(geometry::vector(normal.v))),
           {-1, 0, 0});
    vector(evaluate("Midplane", {bp, up}, sketch::fields({{"bisector", ""}})).origin, {0, 0, .02});
    error("ReferenceAmbiguous", [&] {
        evaluate("Midplane", {bp, ref(side.id, "Plane")}, sketch::fields({{"bisector", ""}}));
    });
    auto bisector = evaluate("Midplane", {bp, ref(side.id, "Plane")},
                             sketch::fields({{"bisector", "Internal"}}));
    vector(
        geometry::coordinates(geometry::vector(bisector.u).Crossed(geometry::vector(bisector.v))),
        {0, -std::sqrt(.5), std::sqrt(.5)});
    auto external = evaluate("Midplane", {bp, ref(side.id, "Plane")},
                             sketch::fields({{"bisector", "External"}}));
    vector(
        geometry::coordinates(geometry::vector(external.u).Crossed(geometry::vector(external.v))),
        {0, std::sqrt(.5), std::sqrt(.5)});
    error("ReferenceAmbiguous",
          [&] { evaluate("Midplane", {bp, bp}, sketch::fields({{"bisector", ""}})); });
}
domain::GeometryReference face(geometry::SupportReferences &scene, QString id,
                               const TopoDS_Shape &shape, GeomAbs_SurfaceType type) {
    scene.stages.insert(id, shape);
    for (TopExp_Explorer it(shape, TopAbs_FACE); it.More(); it.Next())
        if (BRepAdaptor_Surface(TopoDS::Face(it.Current())).GetType() == type) {
            auto r = ref(id, "Face");
            r.topology = kernel::topologyToken(it.Current());
            return r;
        }
    throw std::runtime_error("Fixture face missing");
}
void faceAndAxisMethods() {
    geometry::SupportReferences scene;
    auto cylinder =
        face(scene, newId(), BRepPrimAPI_MakeCylinder(10, 40).Shape(), GeomAbs_Cylinder);
    model::Datum d;
    d.mode = "TangentSurface";
    d.references = {cylinder};
    d.parameters = sketch::fields({{"u_fraction", .25}, {"v_fraction", .5}});
    auto tangent = geometry::datum(d, scene).planes.value(d.id);
    vector(tangent.origin, {0, .01, .02});
    vector(geometry::coordinates(geometry::vector(tangent.u).Crossed(geometry::vector(tangent.v))),
           {0, 1, 0});
    auto sphere = face(scene, newId(), BRepPrimAPI_MakeSphere(10).Shape(), GeomAbs_Sphere);
    d.references = {sphere};
    d.parameters.insert(QStringLiteral("v_fraction"), 1.);
    error("InvalidGeometry", [&] { geometry::datum(d, scene); });
    d.kind = "DatumAxis";
    d.mode = "FaceAxis";
    d.parameters = {};
    d.references = {cylinder};
    auto axis = geometry::datum(d, scene).axes.value(d.id);
    vector(axis.direction, {0, 0, 1});
    vector(axis.origin, {0, 0, 0});
    auto cone = face(scene, newId(), BRepPrimAPI_MakeCone(10, 5, 40).Shape(), GeomAbs_Cone);
    d.references = {cone};
    vector(geometry::datum(d, scene).axes.value(d.id).direction, {0, 0, 1});
    sketch::Sketch base, side;
    auto p0 = base.point({0, 0}), p1 = base.point({.02, 0});
    auto line = base.line(p0, p1);
    auto circle = base.circle(p0, .01);
    side.frame.u = {0, 1, 0};
    side.frame.v = {0, 0, 1};
    scene.sketches = {{base.id, base}, {side.id, side}};
    d.mode = "Line";
    d.references = {ref(base.id, "Curve", line)};
    vector(geometry::datum(d, scene).axes.value(d.id).direction, {1, 0, 0});
    d.references = {ref(base.id, "Curve", circle)};
    error("UnsupportedOperation", [&] { geometry::datum(d, scene); });
    auto splineLine = sketch::createSpline(base, {{0, 0}, {.01, 0}, {.02, 0}}, 1);
    scene.sketches[base.id] = base;
    d.references = {ref(base.id, "Curve", splineLine)};
    vector(geometry::datum(d, scene).axes.value(d.id).direction, {1, 0, 0});
    d.mode = "TwoPlanes";
    d.references = {ref(base.id, "Plane"), ref(side.id, "Plane")};
    vector(geometry::datum(d, scene).axes.value(d.id).direction, {0, 1, 0});
    d.references[1] = d.references[0];
    error("ReferenceAmbiguous", [&] { geometry::datum(d, scene); });
    d.mode = "TwoPoints";
    d.references = {ref(base.id, "Point", p0), ref(base.id, "Point", p1)};
    d.reversed = true;
    vector(geometry::datum(d, scene).axes.value(d.id).direction, {-1, 0, 0});
    d.references[1] = d.references[0];
    error("InvalidGeometry", [&] { geometry::datum(d, scene); });
    d.reversed = false;
    d.mode = "PointNormal";
    d.references = {ref(base.id, "Point", p1), ref(side.id, "Plane")};
    axis = geometry::datum(d, scene).axes.value(d.id);
    vector(axis.origin, {.02, 0, 0});
    vector(axis.direction, {1, 0, 0});
    auto missing = cylinder;
    missing.topology = "face/" + QString(64, 'a');
    error("ReferenceMissing", [&] { scene.plane(missing); });
    sketch::Sketch markers;
    markers.frame.origin = {0, .01, .02};
    auto contact = markers.point({0, 0});
    scene.sketches.insert(markers.id, markers);
    d.references = {ref(markers.id, "Point", contact), cylinder};
    axis = geometry::datum(d, scene).axes.value(d.id);
    vector(axis.origin, {0, .01, .02});
    vector(axis.direction, {0, 1, 0});
}
void topologyAndBranches() {
    geometry::SupportReferences scene;
    auto edge = BRepBuilderAPI_MakeEdge(gp_Pnt(0, 0, 0), gp_Pnt(10, 0, 0)).Edge();
    edge.Reverse();
    auto id = newId();
    scene.stages.insert(id, edge);
    auto r = ref(id, "Edge");
    r.topology = geometry::supportToken(edge);
    r.endpoint = "Start";
    vector(geometry::siPoint(scene.point(r)), {.01, 0, 0});
    model::Datum d;
    d.mode = "NormalCurve";
    d.references = {r};
    auto f = geometry::datum(d, scene).planes.value(d.id);
    vector(geometry::coordinates(geometry::vector(f.u).Crossed(geometry::vector(f.v))), {-1, 0, 0});
    r.endpoint.clear();
    r.parameter = .25;
    vector(geometry::siPoint(scene.point(r)), {.0075, 0, 0});
    r.parameter = 1.1;
    error("ReferenceMissing", [&] { scene.point(r); });
    auto vr = ref(id, "Vertex");
    TopExp_Explorer vertices(edge, TopAbs_VERTEX);
    CHECK(vertices.More());
    vr.topology = geometry::supportToken(vertices.Current());
    CHECK(std::abs(scene.point(vr).X()) < 1e-10 || std::abs(scene.point(vr).X() - 10) < 1e-10);
    domain::GeometryReference start, end;
    for (; vertices.More(); vertices.Next()) {
        auto candidate = ref(id, "Vertex");
        candidate.topology = geometry::supportToken(vertices.Current());
        if (std::abs(BRep_Tool::Pnt(TopoDS::Vertex(vertices.Current())).X()) < 1e-10)
            start = candidate;
        else
            end = candidate;
    }
    model::Datum vertexAxis;
    vertexAxis.kind = "DatumAxis";
    vertexAxis.mode = "TwoPoints";
    vertexAxis.references = {start, end};
    vector(geometry::datum(vertexAxis, scene).axes.value(vertexAxis.id).direction, {1, 0, 0});
    auto shape = BRepPrimAPI_MakeCylinder(10, 40).Shape();
    auto fr = face(scene, newId(), shape, GeomAbs_Cylinder);
    auto copied = BRepBuilderAPI_Copy(shape).Shape();
    TopoDS_Compound compound;
    BRep_Builder builder;
    builder.MakeCompound(compound);
    builder.Add(compound, shape);
    builder.Add(compound, copied);
    scene.stages[fr.object] = compound;
    error("ReferenceAmbiguous", [&] { scene.topology(fr); });
}
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    return run([] {
        offsetPlanes();
        planeMethods();
        faceAndAxisMethods();
        topologyAndBranches();
        principalDatums();
    });
}
