#include "ModelingFixture.hpp"
#include "features/ModelingRecords.hpp"
#include "geometry/occt/ModelingFeatures.hpp"
#include "sketch/SpatialSketch.hpp"
#include <BRepAdaptor_Surface.hxx>
#include <BRepBndLib.hxx>
#include <BRepGProp.hxx>
#include <Bnd_Box.hxx>
#include <GProp_GProps.hxx>
#include <QCoreApplication>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <cmath>
#include <numbers>

using namespace cad;
using namespace cad::geometry;
using namespace modeling_fixture;
using sketch::fields;

namespace {

double volume(const TopoDS_Shape &s) {
    GProp_GProps g;
    BRepGProp::VolumeProperties(s, g);
    return g.Mass() * 1e-9;
}

double area(const TopoDS_Shape &s) {
    GProp_GProps g;
    BRepGProp::SurfaceProperties(s, g);
    return g.Mass() * 1e-6;
}

std::array<double, 6> bounds(const TopoDS_Shape &s) {
    Bnd_Box b;
    BRepBndLib::AddOptimal(s, b, false, false);
    double xmin, ymin, zmin, xmax, ymax, zmax;
    b.Get(xmin, ymin, zmin, xmax, ymax, zmax);
    return {xmin / 1000.0, ymin / 1000.0, zmin / 1000.0,
            xmax / 1000.0, ymax / 1000.0, zmax / 1000.0};
}

domain::GeometryReference findPlanarFace(const TopoDS_Shape &shape, QString object, gp_Dir normal,
                                         double zTarget, double tol = 1e-3) {
    domain::GeometryReference r = support_fixture::ref(object, "Face");
    for (TopExp_Explorer ex(shape, TopAbs_FACE); ex.More(); ex.Next()) {
        BRepAdaptor_Surface surf(TopoDS::Face(ex.Current()));
        if (surf.GetType() == GeomAbs_Plane) {
            auto dir = surf.Plane().Axis().Direction();
            if (dir.IsParallel(normal, 0.1)) {
                double dist = surf.Plane().Distance(gp_Pnt(0, 0, zTarget * 1000.0)) / 1000.0;
                if (dist <= tol) {
                    r.topology = supportToken(ex.Current());
                    return r;
                }
            }
        }
    }
    throw std::runtime_error("Face at requested plane not found");
}

QCborMap makeExtrude(QString sketch, double depth = 0.05) {
    auto f = modeling_fixture::extrude(sketch, "NewBody");
    f.insert(QStringLiteral("direction1"), blind(depth));
    return f;
}

void testShellFeatures() {
    // Base block: 100 mm x 100 mm x 50 mm
    sketch::Sketch s;
    s.rectangle({0, 0}, {0.100, 0.100});
    auto d = base({s});
    auto baseExt = makeExtrude(s.id, 0.050);
    auto baseId = baseExt.value("id").toString();

    auto baseRes = evaluateModeling(d, {baseExt});
    CHECK(baseRes.part.bodies.size() == 1);
    double initialVol = volume(baseRes.part.bodies[0].shape);
    near(initialVol, 0.100 * 0.100 * 0.050, 1e-9);

    auto topFaceRef = findPlanarFace(baseRes.part.bodies[0].shape, baseId, gp_Dir(0, 0, 1), 0.050);
    auto botFaceRef = findPlanarFace(baseRes.part.bodies[0].shape, baseId, gp_Dir(0, 0, 1), 0.000);

    // CM-201: Uniform thickness shell with removed top face (t = 5 mm, inward)
    double t = 0.005;
    auto shellOpen = fields({{"version", 1},
                             {"id", newId()},
                             {"kind", "Shell"},
                             {"target", body(baseId)},
                             {"thickness", t},
                             {"inward", true},
                             {"remove_faces", QCborArray{topFaceRef.map()}},
                             {"overrides", QCborArray{}}});

    auto resOpen = evaluateModeling(d, {baseExt, shellOpen});
    CHECK(resOpen.part.bodies.size() == 1);
    double volOpen = volume(resOpen.part.bodies[0].shape);
    // Cavity: (100 - 10) x (100 - 10) x (50 - 5) = 90 x 90 x 45 mm = 364,500 mm^3 = 3.645e-4 m^3
    // Remaining solid: 5e-4 - 3.645e-4 = 1.355e-4 m^3
    double expectedOpenVol = (0.100 * 0.100 * 0.050) - (0.090 * 0.090 * 0.045);
    near(volOpen, expectedOpenVol, 1e-9);

    // CM-203: Inward shell retains external envelope
    auto bInitial = bounds(baseRes.part.bodies[0].shape);
    auto bInward = bounds(resOpen.part.bodies[0].shape);
    for (int i = 0; i < 6; ++i)
        near(bInward[i], bInitial[i], 1e-5);

    // CM-204: Outward shell retains internal cavity and expands envelope
    auto shellOutward = shellOpen;
    shellOutward.insert(QStringLiteral("id"), newId());
    shellOutward.insert(QStringLiteral("inward"), false);
    auto resOutward = evaluateModeling(d, {baseExt, shellOutward});
    CHECK(resOutward.part.bodies.size() == 1);
    auto bOutward = bounds(resOutward.part.bodies[0].shape);
    // Envelope expands by t on X/Y and bottom
    near(bOutward[0], bInitial[0] - t, 1e-5);
    near(bOutward[1], bInitial[1] - t, 1e-5);
    near(bOutward[2], bInitial[2] - t, 1e-5);
    near(bOutward[3], bInitial[3] + t, 1e-5);
    near(bOutward[4], bInitial[4] + t, 1e-5);

    // CM-205: Multi-thickness face overrides (bottom face t_bot = 10 mm)
    double tBot = 0.010;
    auto shellMulti = shellOpen;
    shellMulti.insert(QStringLiteral("id"), newId());
    shellMulti.insert(QStringLiteral("overrides"),
                      QCborArray{fields({{"face", botFaceRef.map()}, {"thickness", tBot}})});

    auto resMulti = evaluateModeling(d, {baseExt, shellMulti});
    CHECK(resMulti.part.bodies.size() == 1);
    double volMulti = volume(resMulti.part.bodies[0].shape);
    // Inner cavity: 90 x 90 x (50 - 10) = 90 x 90 x 40 mm = 324,000 mm^3 = 3.24e-4 m^3
    // Remaining solid: 5e-4 - 3.24e-4 = 1.76e-4 m^3
    double expectedMultiVol = (0.100 * 0.100 * 0.050) - (0.090 * 0.090 * 0.040);
    near(volMulti, expectedMultiVol, 1e-9);

    // CM-202: Closed cavity shell (zero removed faces, t = 5 mm inward)
    auto shellClosed = fields({{"version", 1},
                               {"id", newId()},
                               {"kind", "Shell"},
                               {"target", body(baseId)},
                               {"thickness", t},
                               {"inward", true},
                               {"remove_faces", QCborArray{}},
                               {"overrides", QCborArray{}}});

    auto resClosed = evaluateModeling(d, {baseExt, shellClosed});
    CHECK(resClosed.part.bodies.size() == 1);
    double volClosed = volume(resClosed.part.bodies[0].shape);
    // Inner cavity: (100 - 10) x (100 - 10) x (50 - 10) = 90 x 90 x 40 mm = 324,000 mm^3 = 3.24e-4
    // m^3 Remaining solid: 5e-4 - 3.24e-4 = 1.76e-4 m^3
    double expectedClosedVol = (0.100 * 0.100 * 0.050) - (0.090 * 0.090 * 0.040);
    near(volClosed, expectedClosedVol, 1e-9);

    // Negative test: Excessive thickness failure boundary (t = 60 mm > 100 mm / 2)
    auto shellExcessive = shellOpen;
    shellExcessive.insert(QStringLiteral("id"), newId());
    shellExcessive.insert(QStringLiteral("thickness"), 0.060);
    support_fixture::error("InvalidGeometry",
                           [&] { evaluateModeling(d, {baseExt, shellExcessive}); });

    puts("cm8 shell features verified");
}

void testSweepFeatures() {
    // 1. CM-207: Circular profile straight pipe volume oracle
    // Path: sketch with straight line from (0, 0, 0) to (0, 0, 0.100)
    sketch::Sketch pathSk;
    pathSk.name = "PathSketch";
    pathSk.frame.origin = {0, 0, 0};
    pathSk.frame.u = {0, 0, 1};
    pathSk.frame.v = {1, 0, 0};
    auto p1 = pathSk.point({0, 0});
    auto p2 = pathSk.point({0.100, 0});
    auto pathLine = pathSk.line(p1, p2);

    auto d = base({pathSk});

    // Circular profile D = 20 mm (Ro = 10 mm), thin t = 2 mm (Ri = 8 mm)
    double dia = 0.020;
    double rOut = dia / 2.0;
    double t = 0.002;
    double rIn = rOut - t;
    double length = 0.100;

    auto sweepPipe = fields(
        {{"version", 1},
         {"id", newId()},
         {"kind", "Sweep"},
         {"operation", "NewBody"},
         {"target", QCborValue(nullptr)},
         {"profile", fields({{"type", "Circular"}, {"diameter", dia}})},
         {"path", fields({{"reference", support_fixture::ref(pathSk.id, "Curve", pathLine).map()},
                          {"reverse", false}})},
         {"guides", QCborArray{}},
         {"corner_transition", "Round"},
         {"start_tangency", "None"},
         {"end_tangency", "None"},
         {"thin", fields({{"mode", "OneDirection"}, {"thickness", t}, {"reverse", false}})},
         {"preview_sections", 0}});

    auto resPipe = evaluateModeling(d, {sweepPipe});
    CHECK(resPipe.part.bodies.size() == 1);
    double volPipe = volume(resPipe.part.bodies[0].shape);
    // Oracle: pi * (Ro^2 - Ri^2) * L
    double expectedPipeVol = std::numbers::pi * (rOut * rOut - rIn * rIn) * length;
    near(volPipe, expectedPipeVol, 1e-8);

    // CM-211..214: Thin circular sweep variations
    // Solid circular sweep
    auto sweepSolid = sweepPipe;
    sweepSolid.insert(QStringLiteral("id"), newId());
    sweepSolid.insert(QStringLiteral("thin"), fields({{"mode", "Solid"}}));
    auto resSolid = evaluateModeling(d, {sweepSolid});
    double volSolid = volume(resSolid.part.bodies[0].shape);
    double expectedSolidVol = std::numbers::pi * rOut * rOut * length;
    near(volSolid, expectedSolidVol, 1e-8);

    // Midplane thin: thickness 2 mm => Ro = 11 mm, Ri = 9 mm
    auto sweepMid = sweepPipe;
    sweepMid.insert(QStringLiteral("id"), newId());
    sweepMid.insert(QStringLiteral("thin"), fields({{"mode", "Midplane"}, {"thickness", t}}));
    auto resMid = evaluateModeling(d, {sweepMid});
    double volMid = volume(resMid.part.bodies[0].shape);
    double expectedMidVol =
        std::numbers::pi *
        ((rOut + t / 2.0) * (rOut + t / 2.0) - (rOut - t / 2.0) * (rOut - t / 2.0)) * length;
    near(volMid, expectedMidVol, 1e-8);

    // TwoDirection thin: t1 = 1 mm, t2 = 2 mm => Ri = 9 mm, Ro = 12 mm
    auto sweepTwoDir = sweepPipe;
    sweepTwoDir.insert(QStringLiteral("id"), newId());
    sweepTwoDir.insert(
        QStringLiteral("thin"),
        fields({{"mode", "TwoDirection"}, {"thickness1", 0.001}, {"thickness2", 0.002}}));
    auto resTwoDir = evaluateModeling(d, {sweepTwoDir});
    double volTwoDir = volume(resTwoDir.part.bodies[0].shape);
    double expectedTwoDirVol = std::numbers::pi *
                               ((rOut + 0.002) * (rOut + 0.002) - (rOut - 0.001) * (rOut - 0.001)) *
                               length;
    near(volTwoDir, expectedTwoDirVol, 1e-8);

    // 2. CM-206 & CM-208: Sketch profile along open path
    // Profile: 10 mm x 10 mm square in XY plane
    sketch::Sketch profSk;
    profSk.name = "SquareProfile";
    profSk.rectangle({-0.005, -0.005}, {0.005, 0.005});

    auto d2 = base({profSk, pathSk});
    auto sweepSketch = fields(
        {{"version", 1},
         {"id", newId()},
         {"kind", "Sweep"},
         {"operation", "NewBody"},
         {"target", QCborValue(nullptr)},
         {"profile", fields({{"type", "Sketch"},
                             {"sketch", profSk.id},
                             {"mode", "AllClosed"},
                             {"regions", QCborArray{}}})},
         {"path", fields({{"reference", support_fixture::ref(pathSk.id, "Curve", pathLine).map()},
                          {"reverse", false}})},
         {"guides", QCborArray{}},
         {"corner_transition", "Round"},
         {"start_tangency", "None"},
         {"end_tangency", "None"},
         {"thin", fields({{"mode", "Solid"}})},
         {"preview_sections", 0}});

    auto resSketch = evaluateModeling(d2, {sweepSketch});
    CHECK(resSketch.part.bodies.size() == 1);
    double volSketch = volume(resSketch.part.bodies[0].shape);
    // Oracle: 0.010 * 0.010 * 0.100 = 1e-5 m^3
    near(volSketch, 0.010 * 0.010 * length, 1e-9);

    // 3. CM-209: Closed path sweep (circular path => ring torus)
    sketch::Sketch ringPath;
    ringPath.name = "RingPath";
    auto cCenter = ringPath.point({0, 0});
    auto cPath = ringPath.circle(cCenter, 0.050); // R_major = 50 mm

    auto dRing = base({ringPath});
    auto sweepRing = fields(
        {{"version", 1},
         {"id", newId()},
         {"kind", "Sweep"},
         {"operation", "NewBody"},
         {"target", QCborValue(nullptr)},
         {"profile", fields({{"type", "Circular"}, {"diameter", 0.010}})}, // r_minor = 5 mm
         {"path", fields({{"reference", support_fixture::ref(ringPath.id, "Curve", cPath).map()},
                          {"reverse", false}})},
         {"guides", QCborArray{}},
         {"corner_transition", "Round"},
         {"start_tangency", "None"},
         {"end_tangency", "None"},
         {"thin", fields({{"mode", "Solid"}})},
         {"preview_sections", 0}});

    auto resRing = evaluateModeling(dRing, {sweepRing});
    CHECK(resRing.part.bodies.size() == 1);
    double volRing = volume(resRing.part.bodies[0].shape);
    // Torus volume: 2 * pi^2 * R * r^2 = 2 * pi^2 * 0.050 * 0.005^2
    double expectedRingVol = 2.0 * std::numbers::pi * std::numbers::pi * 0.050 * (0.005 * 0.005);
    near(volRing, expectedRingVol, 1e-8);

    // 4. CM-217: Corner transition (Sharp vs Round on L-shaped path)
    sketch::Sketch lPath;
    lPath.name = "LPath";
    lPath.frame.origin = {0, 0, 0};
    lPath.frame.u = {0, 0, 1};
    lPath.frame.v = {1, 0, 0};
    auto lp1 = lPath.point({0, 0});
    auto lp2 = lPath.point({0.050, 0});
    auto lp3 = lPath.point({0.050, 0.050});
    auto lLine1 = lPath.line(lp1, lp2);
    auto lLine2 = lPath.line(lp2, lp3);

    auto dL = base({lPath});
    auto sweepSharp =
        fields({{"version", 1},
                {"id", newId()},
                {"kind", "Sweep"},
                {"operation", "NewBody"},
                {"target", QCborValue(nullptr)},
                {"profile", fields({{"type", "Circular"}, {"diameter", 0.010}})},
                {"path", fields({{"reference", support_fixture::ref(lPath.id, "Curve").map()},
                                 {"reverse", false}})},
                {"guides", QCborArray{}},
                {"corner_transition", "Sharp"},
                {"start_tangency", "None"},
                {"end_tangency", "None"},
                {"thin", fields({{"mode", "Solid"}})},
                {"preview_sections", 0}});

    auto resSharp = evaluateModeling(dL, {sweepSharp});
    CHECK(resSharp.part.bodies.size() == 1);

    auto sweepRound = sweepSharp;
    sweepRound.insert(QStringLiteral("id"), newId());
    sweepRound.insert(QStringLiteral("corner_transition"), QStringLiteral("Round"));
    auto resRound = evaluateModeling(dL, {sweepRound});
    CHECK(resRound.part.bodies.size() == 1);

    // 5. CM-210: 3D spatial curve path sweep
    sketch::SpatialSketch sp;
    sp.name = "SpatialPath";
    auto sp1 = sp.point({0.0, 0.0, 0.0});
    auto sp2 = sp.point({0.050, 0.050, 0.050});
    auto spLine = sp.line(sp1, sp2);

    model::SupportDefinition dSpatial;
    dSpatial.spatial_sketches = sketch::encodeSpatial({sp});
    dSpatial.history.append(sp.id);

    auto sweep3D =
        fields({{"version", 1},
                {"id", newId()},
                {"kind", "Sweep"},
                {"operation", "NewBody"},
                {"target", QCborValue(nullptr)},
                {"profile", fields({{"type", "Circular"}, {"diameter", 0.010}})},
                {"path", fields({{"reference", support_fixture::ref(sp.id, "Curve", spLine).map()},
                                 {"reverse", false}})},
                {"guides", QCborArray{}},
                {"corner_transition", "Round"},
                {"start_tangency", "None"},
                {"end_tangency", "None"},
                {"thin", fields({{"mode", "Solid"}})},
                {"preview_sections", 0}});

    auto res3D = evaluateModeling(dSpatial, {sweep3D});
    CHECK(res3D.part.bodies.size() == 1);
    CHECK(volume(res3D.part.bodies[0].shape) > 0);

    // 6. CM-215: Guide curve sweep (auxiliary spine)
    sketch::Sketch guideSk;
    guideSk.name = "GuideSketch";
    guideSk.frame.origin = {0.020, 0, 0};
    guideSk.frame.u = {0, 0, 1};
    guideSk.frame.v = {1, 0, 0};
    auto gp1 = guideSk.point({0, 0});
    auto gp2 = guideSk.point({0.100, 0});
    auto gLine = guideSk.line(gp1, gp2);

    auto dGuide = base({pathSk, guideSk});
    auto sweepGuided = sweepPipe;
    sweepGuided.insert(QStringLiteral("id"), newId());
    sweepGuided.insert(QStringLiteral("guides"),
                       QCborArray{support_fixture::ref(guideSk.id, "Curve", gLine).map()});
    auto resGuided = evaluateModeling(dGuide, {sweepGuided});
    CHECK(resGuided.part.bodies.size() == 1);
    CHECK(volume(resGuided.part.bodies[0].shape) > 0);

    puts("cm8 sweep features verified");
}

void testLoftFeatures() {
    // 1. CM-219 & CM-222: Truncated cone solid loft between two circular sections
    // Section 1 at z = 0, R1 = 20 mm
    sketch::Sketch sec1;
    sec1.name = "Section1";
    sec1.frame.origin = {0, 0, 0};
    auto c1 = sec1.point({0, 0});
    sec1.circle(c1, 0.020);

    // Section 2 at z = 100 mm, R2 = 10 mm
    double height = 0.100;
    sketch::Sketch sec2;
    sec2.name = "Section2";
    sec2.frame.origin = {0, 0, height};
    auto c2 = sec2.point({0, 0});
    sec2.circle(c2, 0.010);

    auto d = base({sec1, sec2});

    auto loftSolid = fields(
        {{"version", 1},
         {"id", newId()},
         {"kind", "Loft"},
         {"output", "Solid"},
         {"operation", "NewBody"},
         {"target", QCborValue(nullptr)},
         {"sections",
          QCborArray{
              fields({{"sketch", sec1.id}, {"mode", "AllClosed"}, {"regions", QCborArray{}}}),
              fields({{"sketch", sec2.id}, {"mode", "AllClosed"}, {"regions", QCborArray{}}})}},
         {"centerline", QCborValue(nullptr)},
         {"guides", QCborArray{}},
         {"connectors", QCborArray{}},
         {"start_condition", "None"},
         {"end_condition", "None"},
         {"thin", fields({{"mode", "Solid"}})}});

    auto resLoft = evaluateModeling(d, {loftSolid});
    CHECK(resLoft.part.bodies.size() == 1);
    double volLoft = volume(resLoft.part.bodies[0].shape);
    // Oracle: 1/3 * pi * H * (R1^2 + R1*R2 + R2^2)
    double r1 = 0.020, r2 = 0.010;
    double expectedLoftVol =
        (1.0 / 3.0) * std::numbers::pi * height * (r1 * r1 + r1 * r2 + r2 * r2);
    near(volLoft, expectedLoftVol, 1e-7);

    // 2. Three ordered sections loft: R = 10mm at z=0, R = 15mm at z=50mm, R = 10mm at z=100mm
    sketch::Sketch startSec;
    startSec.name = "SectionStart";
    startSec.frame.origin = {0, 0, 0};
    auto cs = startSec.point({0, 0});
    startSec.circle(cs, 0.010);

    sketch::Sketch midSec;
    midSec.name = "SectionMid";
    midSec.frame.origin = {0, 0, 0.050};
    auto cm = midSec.point({0, 0});
    midSec.circle(cm, 0.015);

    sketch::Sketch endSec;
    endSec.name = "SectionEnd";
    endSec.frame.origin = {0, 0, 0.100};
    auto ce = endSec.point({0, 0});
    endSec.circle(ce, 0.010);

    auto d3Sec = base({startSec, midSec, endSec});
    auto loft3Sec = fields(
        {{"version", 1},
         {"id", newId()},
         {"kind", "Loft"},
         {"output", "Solid"},
         {"operation", "NewBody"},
         {"target", QCborValue(nullptr)},
         {"sections",
          QCborArray{
              fields({{"sketch", startSec.id}, {"mode", "AllClosed"}, {"regions", QCborArray{}}}),
              fields({{"sketch", midSec.id}, {"mode", "AllClosed"}, {"regions", QCborArray{}}}),
              fields({{"sketch", endSec.id}, {"mode", "AllClosed"}, {"regions", QCborArray{}}})}},
         {"centerline", QCborValue(nullptr)},
         {"guides", QCborArray{}},
         {"connectors", QCborArray{}},
         {"start_condition", "None"},
         {"end_condition", "None"},
         {"thin", fields({{"mode", "Solid"}})}});

    auto res3Sec = evaluateModeling(d3Sec, {loft3Sec});
    CHECK(res3Sec.part.bodies.size() == 1);
    CHECK(volume(res3Sec.part.bodies[0].shape) > 0);

    // 3. CM-221: Open section surface loft publishes surface body, refuses volume
    sketch::Sketch openSec1;
    openSec1.name = "OpenSection1";
    openSec1.frame.origin = {0, 0, 0};
    auto op1 = openSec1.point({-0.020, 0});
    auto op2 = openSec1.point({0.020, 0});
    auto openLine1 = openSec1.line(op1, op2);

    sketch::Sketch openSec2;
    openSec2.name = "OpenSection2";
    openSec2.frame.origin = {0, 0, 0.050};
    auto op3 = openSec2.point({-0.030, 0});
    auto op4 = openSec2.point({0.030, 0});
    auto openLine2 = openSec2.line(op3, op4);

    auto dSurface = base({openSec1, openSec2});
    auto openRef1 =
        fields({{"sketch", openSec1.id},
                {"mode", "OpenChains"},
                {"regions",
                 QCborArray{QCborArray{QCborArray{fields(
                     {{"entity", openLine1}, {"from", 0.0}, {"to", 1.0}, {"reverse", false}})}}}}});
    auto openRef2 =
        fields({{"sketch", openSec2.id},
                {"mode", "OpenChains"},
                {"regions",
                 QCborArray{QCborArray{QCborArray{fields(
                     {{"entity", openLine2}, {"from", 0.0}, {"to", 1.0}, {"reverse", false}})}}}}});

    auto loftSurface = fields({{"version", 1},
                               {"id", newId()},
                               {"kind", "Loft"},
                               {"output", "Surface"},
                               {"operation", "NewBody"},
                               {"target", QCborValue(nullptr)},
                               {"sections", QCborArray{openRef1, openRef2}},
                               {"centerline", QCborValue(nullptr)},
                               {"guides", QCborArray{}},
                               {"connectors", QCborArray{}},
                               {"start_condition", "None"},
                               {"end_condition", "None"},
                               {"thin", fields({{"mode", "Solid"}})}});

    auto resSurface = evaluateModeling(dSurface, {loftSurface});
    CHECK(resSurface.part.bodies.size() == 1);
    // Shape is face/shell (non-solid)
    double surfArea = area(resSurface.part.bodies[0].shape);
    CHECK(surfArea > 0);

    // Volume measurement must refuse with UnsupportedOperation (CM-221)
    support_fixture::error("UnsupportedOperation",
                           [&] { resSurface.part.measure(0, resSurface.part.bodies[0].id); });

    // 4. CM-225..227: Connectors and twist synchronization
    auto conn1 = fields({{"id", newId()},
                         {"points", QCborArray{fields({{"section", 0}, {"parameter", 0.0}}),
                                               fields({{"section", 1}, {"parameter", 0.25}})}}});
    auto loftTwist = loftSolid;
    loftTwist.insert(QStringLiteral("id"), newId());
    loftTwist.insert(QStringLiteral("connectors"), QCborArray{conn1});

    auto resTwist = evaluateModeling(d, {loftTwist});
    CHECK(resTwist.part.bodies.size() == 1);
    CHECK(volume(resTwist.part.bodies[0].shape) > 0);

    // 5. CM-224: Centerline guided loft
    sketch::Sketch clSk;
    clSk.name = "LoftCenterline";
    clSk.frame.origin = {0, 0, 0};
    clSk.frame.u = {0, 0, 1};
    clSk.frame.v = {1, 0, 0};
    auto clp1 = clSk.point({0, 0});
    auto clp2 = clSk.point({height, 0});
    auto clLine = clSk.line(clp1, clp2);

    auto dCL = base({sec1, sec2, clSk});
    auto loftCL = loftSolid;
    loftCL.insert(QStringLiteral("id"), newId());
    loftCL.insert(QStringLiteral("centerline"),
                  fields({{"reference", support_fixture::ref(clSk.id, "Curve", clLine).map()},
                          {"reverse", false}}));

    auto resCL = evaluateModeling(dCL, {loftCL});
    CHECK(resCL.part.bodies.size() == 1);
    CHECK(volume(resCL.part.bodies[0].shape) > 0);

    // 6. CM-223: Thin loft modes
    auto loftThin = loftSolid;
    loftThin.insert(QStringLiteral("id"), newId());
    loftThin.insert(QStringLiteral("thin"),
                    fields({{"mode", "OneDirection"}, {"thickness", 0.002}, {"reverse", false}}));

    auto resThin = evaluateModeling(d, {loftThin});
    CHECK(resThin.part.bodies.size() == 1);
    const double thinVolume = volume(resThin.part.bodies[0].shape);
    CHECK(thinVolume > 0 && thinVolume < volLoft);
    const auto invalidThin =
        fields({{"mode", "OneDirection"}, {"thickness", 1.0}, {"reverse", false}});
    loftThin.insert(QStringLiteral("thin"), invalidThin);
    support_fixture::error("InvalidGeometry", [&] { evaluateModeling(d, {loftThin}); });

    auto tangent = loftSolid;
    tangent.insert(QStringLiteral("start_condition"), QStringLiteral("NormalToSection"));
    support_fixture::error("UnsupportedOperation", [&] { evaluateModeling(d, {tangent}); });

    puts("cm8 loft features verified");
}

} // namespace

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    try {
        testShellFeatures();
        testSweepFeatures();
        testLoftFeatures();
        puts("PASS: cm8 features");
        return 0;
    } catch (const std::exception &e) {
        fprintf(stderr, "FAIL: %s\n", e.what());
        return 1;
    }
}
