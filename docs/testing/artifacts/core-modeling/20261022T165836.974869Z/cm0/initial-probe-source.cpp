// CM0 probes only. This executable is never linked into the production worker or UI.
#include "support/check.hpp"
#include <BRepAdaptor_Curve.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepBndLib.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepFeat_MakePrism.hxx>
#include <BRepFilletAPI_MakeFillet.hxx>
#include <BRepGProp.hxx>
#include <BRepOffsetAPI_MakePipeShell.hxx>
#include <BRepOffset_MakeOffset.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <Bnd_Box.hxx>
#include <GCS.h>
#include <GProp_GProps.hxx>
#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <Standard_Failure.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <gp_Circ.hxx>
#include <gp_Pln.hxx>
#include <array>
#include <functional>
#include <numbers>

namespace {
double volume(const TopoDS_Shape &shape) {
    CHECK(!shape.IsNull());
    CHECK(BRepCheck_Analyzer(shape).IsValid());
    int solids = 0;
    for (TopExp_Explorer it(shape, TopAbs_SOLID); it.More(); it.Next())
        ++solids;
    CHECK(solids == 1);
    GProp_GProps p;
    BRepGProp::VolumeProperties(shape, p);
    CHECK(p.Mass() > 0);
    return p.Mass();
}
TopoDS_Face planeFace(const TopoDS_Shape &shape, int axis, double coordinate) {
    for (TopExp_Explorer it(shape, TopAbs_FACE); it.More(); it.Next()) {
        BRepAdaptor_Surface s(TopoDS::Face(it.Current()));
        if (s.GetType() == GeomAbs_Plane) {
            auto p = s.Plane();
            if (std::abs(std::abs(p.Axis().Direction().Coord(axis)) - 1) < 1e-8 &&
                std::abs(p.Location().Coord(axis) - coordinate) < 1e-8)
                return TopoDS::Face(it.Current());
        }
    }
    throw std::runtime_error("Required fixture plane missing");
}
QJsonObject fullRound() {
    auto box = BRepPrimAPI_MakeBox(20, 10, 10).Shape();
    BRepFilletAPI_MakeFillet f(box);
    int selected = 0;
    for (TopExp_Explorer it(box, TopAbs_EDGE); it.More(); it.Next()) {
        BRepAdaptor_Curve c(TopoDS::Edge(it.Current()));
        auto a = c.Value(c.FirstParameter()), b = c.Value(c.LastParameter());
        if (std::abs(a.Z() - 10) < 1e-8 && std::abs(b.Z() - 10) < 1e-8 &&
            std::abs(a.X() - b.X()) > 19) {
            // TopExp repeats a shared edge per face; add each underlying edge only once.
            if (f.Contour(TopoDS::Edge(it.Current())) == 0) {
                f.Add(5., TopoDS::Edge(it.Current()));
                ++selected;
            }
        }
    }
    CHECK(selected == 2);
    f.Build();
    CHECK(f.IsDone());
    auto v = volume(f.Shape());
    near(v, 20 * (50 + std::numbers::pi * 25 / 2), 1e-6);
    return {{"volume_mm3", v}, {"limit", "Two-edge box full-round seed only; not general three-face/full-round or face-set blending"}};
}
QJsonObject multiThickness() {
    auto box = BRepPrimAPI_MakeBox(30, 20, 10).Shape();
    auto top = planeFace(box, 3, 10), side = planeFace(box, 1, 0);
    BRepOffset_MakeOffset shell;
    shell.Initialize(box, -1., 1e-7, BRepOffset_Skin, false, false, GeomAbs_Intersection);
    shell.AddFace(top);
    shell.SetOffsetOnFace(side, -2.);
    shell.MakeThickSolid();
    CHECK(shell.IsDone());
    auto v = volume(shell.Shape());
    near(v, 6000 - 27 * 18 * 9, 1e-5);
    return {{"volume_mm3", v}, {"expected_mm3", 1626}, {"limit", "Planar asymmetric walls; curved multi-thickness requires further fixtures"}};
}
QJsonObject terminated() {
    auto box = BRepPrimAPI_MakeBox(20, 20, 2).Shape();
    auto base = planeFace(box, 3, 2);
    QJsonArray results;
    for (double height : {10., 12.}) {
        auto until = BRepBuilderAPI_MakeFace(gp_Pln(gp_Pnt(0, 0, height), gp_Dir(-.2, 0, 1)),
                                           -100, 100, -100, 100).Face();
        BRepFeat_MakePrism prism(box, base, base, gp_Dir(0, 0, 1), 1, true);
        prism.Perform(until);
        CHECK(prism.IsDone());
        auto v = volume(prism.Shape());
        near(v, 400 * (height + 2), 1e-5);
        results.append(v);
    }
    return {{"volumes_mm3", results}, {"limit", "Full rectangular profile to changed inclined plane, not arbitrary curved surfaces or all end conditions"}};
}
TopoDS_Wire circle(double z, double radius) {
    return BRepBuilderAPI_MakeWire(
        BRepBuilderAPI_MakeEdge(gp_Circ(gp_Ax2(gp_Pnt(0, 0, z), gp_Dir(0, 0, 1)), radius)).Edge())
        .Wire();
}
QJsonObject guidedLoft() {
    auto edge = BRepBuilderAPI_MakeEdge(gp_Pnt(0, 0, 0), gp_Pnt(0, 0, 30)).Edge();
    auto path = BRepBuilderAPI_MakeWire(edge).Wire();
    BRepOffsetAPI_MakePipeShell loft(path);
    loft.SetMode(false);
    loft.Add(circle(0, 5), false, false);
    loft.Add(circle(30, 10), false, false);
    loft.Build();
    CHECK(loft.IsDone());
    CHECK(loft.MakeSolid());
    auto v = volume(loft.Shape());
    near(v, std::numbers::pi * 30 / 3 * (25 + 50 + 100), .01);
    return {{"volume_mm3", v}, {"limit", "Linear centerline with two circular sections; curved centerline/guide/connector correspondence remains unproven"}};
}
QJsonObject spline() {
    // A cubic clamped Bezier is a BSpline. All buffers outlive GCS::System.
    std::array<double, 8> values{0, 0, 2, 3, 6, 3, 10, 0};
    std::array<double, 4> weights{1, 1, 1, 1};
    std::array<double, 2> knots{0, 1};
    GCS::BSpline curve;
    curve.degree = 3;
    curve.mult = {4, 4};
    for (int i = 0; i < 4; ++i) {
        curve.poles.emplace_back(&values[i * 2], &values[i * 2 + 1]);
        curve.weights.push_back(&weights[i]);
    }
    for (auto &k : knots)
        curve.knots.push_back(&k);
    curve.setupFlattenedKnots();
    double x = 4, y = 4, t = .5, targetY = 2;
    GCS::Point p{&x, &y};
    GCS::System system;
    system.maxIter = 100;
    system.sketchSizeMultiplier = false;
    GCS::VEC_pD vars{&x, &y, &values[3]};
    system.addConstraintCurveValue(p, curve, &t, 1);
    system.addConstraintCoordinateY(p, &targetY, 2);
    system.declareUnknowns(vars);
    system.initSolution();
    CHECK(system.solve(true, GCS::DogLeg) == GCS::Success);
    system.applySolution();
    auto point = curve.Value(.5, 0);
    near(x, point.x, 1e-7);
    near(y, 2, 1e-7);
    near(point.y, 2, 1e-7);
    // Independent cubic evaluation, not a second call to the same GCS evaluator.
    near(x, .125 * values[0] + .375 * values[2] + .375 * values[4] + .125 * values[6], 1e-7);
    near(y, .125 * values[1] + .375 * values[3] + .375 * values[5] + .125 * values[7], 1e-7);
    return {{"point_x", x}, {"point_y", y}, {"changed_control_y", values[3]}, {"limit", "Clamped cubic point constraint; arbitrary rational/trim/tangent combinations not certified"}};
}
} // namespace
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    QJsonArray results;
    int failures = 0;
    for (const auto &[name, probe] :
         std::vector<std::pair<QString, std::function<QJsonObject()>>>{
             {"full_round_seed", fullRound}, {"multi_thickness", multiThickness},
             {"surface_termination", terminated}, {"guided_loft_seed", guidedLoft},
             {"constrained_spline", spline}}) {
        QJsonObject row{{"probe", name}, {"status", "Pass"}};
        try {
            row.insert("measurements", probe());
        } catch (const Standard_Failure &e) {
            row.insert("status", "Blocked");
            row.insert("reason", e.GetMessageString());
            ++failures;
        } catch (const std::exception &e) {
            row.insert("status", "Blocked");
            row.insert("reason", e.what());
            ++failures;
        }
        results.append(row);
    }
    QJsonObject report{{"scope", "CM0 isolated feasibility, not production acceptance"},
                       {"experiments", results}, {"blocked_count", failures},
                       {"general_face_blend", "Blocked: face-set blend is not established by edge-seeded fillet API/probe"}};
    std::cout << QJsonDocument(report).toJson().constData();
    return failures ? 1 : 0;
}
