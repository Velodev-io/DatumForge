#include "ModelingFixture.hpp"
#include "features/ModelingRecords.hpp"
#include "geometry/occt/ModelingFeatures.hpp"
#include <BRepAdaptor_Surface.hxx>
#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <QCoreApplication>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
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

domain::GeometryReference findFace(const TopoDS_Shape &shape, QString object, double zMin = -1e9,
                                   double zMax = 1e9) {
    domain::GeometryReference r = support_fixture::ref(object, "Face");
    for (TopExp_Explorer ex(shape, TopAbs_FACE); ex.More(); ex.Next()) {
        BRepAdaptor_Surface surf(TopoDS::Face(ex.Current()));
        if (surf.GetType() == GeomAbs_Plane) {
            double z = surf.Plane().Location().Z();
            if (z >= zMin && z <= zMax) {
                r.topology = supportToken(ex.Current());
                return r;
            }
        }
    }
    for (TopExp_Explorer ex(shape, TopAbs_FACE); ex.More(); ex.Next()) {
        r.topology = supportToken(ex.Current());
        return r;
    }
    return r;
}

domain::GeometryReference threadFace(const TopoDS_Shape &shape, QString object) {
    auto r = support_fixture::ref(object, "Face");
    for (TopExp_Explorer ex(shape, TopAbs_FACE); ex.More(); ex.Next())
        if (BRepAdaptor_Surface(TopoDS::Face(ex.Current())).GetType() == GeomAbs_Cylinder) {
            r.topology = supportToken(ex.Current());
            return r;
        }
    throw std::runtime_error("Thread cylinder missing");
}

QCborMap blind(double value = 0.02) {
    return fields({{"mode", "Blind"},
                   {"value", value},
                   {"draft", fields({{"angle", 0.0}, {"outward", false}})}});
}

QCborMap makeExtrude(QString sketch, double depth = 0.05) {
    auto f = modeling_fixture::extrude(sketch, "NewBody");
    f.insert(QStringLiteral("direction1"), blind(depth));
    return f;
}

void testTreatments() {
    // Base block: 100 mm x 100 mm x 50 mm
    sketch::Sketch s;
    s.rectangle({0, 0}, {0.100, 0.100});
    auto d = base({s});
    auto baseExt = makeExtrude(s.id, 0.050);
    auto baseId = baseExt.value("id").toString();

    auto res = evaluateModeling(d, {baseExt});
    CHECK(res.part.bodies.size() == 1);
    double initialVol = volume(res.part.bodies[0].shape);
    near(initialVol, 0.100 * 0.100 * 0.050, 1e-9);

    // Pick a top edge (length L = 0.100 m)
    auto topEdge = res.part.bodies[0].shape;
    double edgeLength = 0;
    domain::GeometryReference edgeRef;
    edgeRef.object = baseId;
    edgeRef.role = "Edge";
    for (TopExp_Explorer ex(topEdge, TopAbs_EDGE); ex.More(); ex.Next()) {
        edgeRef.topology = supportToken(ex.Current());
        GProp_GProps length;
        BRepGProp::LinearProperties(ex.Current(), length);
        edgeLength = length.Mass() / 1000;
        break;
    }
    CHECK(!edgeRef.topology.isEmpty());

    // 1. Constant Fillet R = 5 mm (Tutorial 2)
    double rFillet = 0.005;
    auto filletIntent = fields({{"version", 1},
                                {"id", newId()},
                                {"kind", "Fillet"},
                                {"target", body(baseId)},
                                {"method", "Constant"},
                                {"propagate_tangent", true},
                                {"radius", rFillet},
                                {"edges", QCborArray{edgeRef.map()}}});

    auto resFillet = evaluateModeling(d, {baseExt, filletIntent});
    CHECK(resFillet.part.bodies.size() == 1);
    double volFillet = volume(resFillet.part.bodies[0].shape);
    // Analytical oracle for 90 degree convex edge fillet: (1 - pi/4) * r^2 * L
    double expectedLoss = (1.0 - std::numbers::pi / 4.0) * rFillet * rFillet * edgeLength;
    near(initialVol - volFillet, expectedLoss, 1e-10);

    // 2. AngleDistance Chamfer 1.5 mm at 45 deg (Tutorial 3)
    double chamfD = 0.0015;
    double chamfAng = 45.0 * std::numbers::pi / 180.0;
    auto chamfIntent = fields({{"version", 1},
                               {"id", newId()},
                               {"kind", "Chamfer"},
                               {"target", body(baseId)},
                               {"method", "AngleDistance"},
                               {"propagate_tangent", true},
                               {"edges", QCborArray{edgeRef.map()}},
                               {"distance", chamfD},
                               {"angle", chamfAng},
                               {"flip", false}});

    auto resChamf = evaluateModeling(d, {baseExt, chamfIntent});
    CHECK(resChamf.part.bodies.size() == 1);
    double volChamf = volume(resChamf.part.bodies[0].shape);
    // Analytical oracle: d * (d * tan(theta)) / 2 * L
    double expectedChamfLoss = (chamfD * chamfD * std::tan(chamfAng) / 2.0) * edgeLength;
    near(initialVol - volChamf, expectedChamfLoss, 1e-10);

    // 2b. AngleDistance Chamfer with flip: true and 30 deg
    double chamfD30 = 0.002;
    double chamfAng30 = 30.0 * std::numbers::pi / 180.0;
    auto chamfFlipIntent = fields({{"version", 1},
                                   {"id", newId()},
                                   {"kind", "Chamfer"},
                                   {"target", body(baseId)},
                                   {"method", "AngleDistance"},
                                   {"propagate_tangent", true},
                                   {"edges", QCborArray{edgeRef.map()}},
                                   {"distance", chamfD30},
                                   {"angle", chamfAng30},
                                   {"flip", true}});
    auto resChamfFlip = evaluateModeling(d, {baseExt, chamfFlipIntent});
    CHECK(resChamfFlip.part.bodies.size() == 1);
    double volChamfFlip = volume(resChamfFlip.part.bodies[0].shape);
    double expectedChamfFlipLoss = (chamfD30 * chamfD30 * std::tan(chamfAng30) / 2.0) * edgeLength;
    near(initialVol - volChamfFlip, expectedChamfFlipLoss, 1e-10);

    // 3. Equal Chamfer 2 mm
    double eqD = 0.002;
    auto eqChamfIntent = fields({{"version", 1},
                                 {"id", newId()},
                                 {"kind", "Chamfer"},
                                 {"target", body(baseId)},
                                 {"method", "EqualDistance"},
                                 {"propagate_tangent", true},
                                 {"edges", QCborArray{edgeRef.map()}},
                                 {"distance", eqD}});

    auto resEq = evaluateModeling(d, {baseExt, eqChamfIntent});
    double volEq = volume(resEq.part.bodies[0].shape);
    double expectedEqLoss = (eqD * eqD / 2.0) * edgeLength;
    near(initialVol - volEq, expectedEqLoss, 1e-10);

    // 4. Face Fillet Blocked per R1
    auto faceFilletIntent =
        fields({{"version", 1},
                {"id", newId()},
                {"kind", "Fillet"},
                {"target", body(baseId)},
                {"method", "Face"},
                {"propagate_tangent", true},
                {"radius", 0.005},
                {"faces1", QCborArray{findFace(res.part.bodies[0].shape, baseId).map()}},
                {"faces2", QCborArray{findFace(res.part.bodies[0].shape, baseId).map()}}});
    rejects([&] { evaluateModeling(d, {baseExt, faceFilletIntent}); });

    puts("treatment engine passed");
}

void testHoles() {
    // Base block: 100 mm x 100 mm x 50 mm
    sketch::Sketch s;
    s.rectangle({0, 0}, {0.100, 0.100});

    // Placement sketch on top face with placement points
    sketch::Sketch plSk;
    plSk.name = "HolePositions";
    auto pt1 = plSk.point({0.030, 0.030});
    auto pt2 = plSk.point({0.070, 0.070});

    auto d = base({s, plSk});
    auto baseExt = makeExtrude(s.id, 0.050);
    auto baseId = baseExt.value("id").toString();

    auto baseRes = evaluateModeling(d, {baseExt});
    double baseVol = volume(baseRes.part.bodies[0].shape);
    auto topFace = findFace(baseRes.part.bodies[0].shape, baseId, 49.0, 51.0);

    // 1. Simple Hole Through All (Tutorial 1: Ø10 mm)
    double dia = 0.010;
    auto simpleHole = fields(
        {{"version", 1},
         {"id", newId()},
         {"kind", "Hole"},
         {"operation", "Cut"},
         {"target", body(baseId)},
         {"family", "Simple"},
         {"placement",
          fields({{"face", topFace.map()}, {"sketch", plSk.id}, {"points", QCborArray{pt1}}})},
         {"sizing", fields({{"mode", "Custom"}, {"diameter", dia}, {"depth", 0.050}})},
         {"termination", fields({{"mode", "ThroughAll"}, {"depth_reference", "Shoulder"}})}});

    auto h1Res = evaluateModeling(d, {baseExt, simpleHole});
    CHECK(h1Res.part.bodies.size() == 1);
    double h1Vol = volume(h1Res.part.bodies[0].shape);
    // Oracle for cylinder through 50 mm slab: pi * r^2 * h
    double expectedHoleLoss = std::numbers::pi * (dia / 2.0) * (dia / 2.0) * 0.050;
    near(baseVol - h1Vol, expectedHoleLoss, 1e-10);

    // 2. Counterbore Hole (Tutorial 1: 8.5 / 14 / 4.2 mm Through All)
    double cbDia = 0.014, cbDepth = 0.0042, mainDia = 0.0085;
    auto cboreHole = fields(
        {{"version", 1},
         {"id", newId()},
         {"kind", "Hole"},
         {"operation", "Cut"},
         {"target", body(baseId)},
         {"family", "Counterbore"},
         {"placement",
          fields({{"face", topFace.map()}, {"sketch", plSk.id}, {"points", QCborArray{pt1}}})},
         {"sizing", fields({{"mode", "Custom"},
                            {"diameter", mainDia},
                            {"depth", 0.050},
                            {"counterbore_diameter", cbDia},
                            {"counterbore_depth", cbDepth}})},
         {"termination", fields({{"mode", "ThroughAll"}, {"depth_reference", "Shoulder"}})}});

    auto cbRes = evaluateModeling(d, {baseExt, cboreHole});
    double cbVol = volume(cbRes.part.bodies[0].shape);
    double expectedCbLoss =
        std::numbers::pi * (cbDia / 2.0) * (cbDia / 2.0) * cbDepth +
        std::numbers::pi * (mainDia / 2.0) * (mainDia / 2.0) * (0.050 - cbDepth);
    near(baseVol - cbVol, expectedCbLoss, 1e-10);

    // 3. Multiple positions (pt1 and pt2)
    auto multiHole = simpleHole;
    auto plMap = multiHole.value("placement").toMap();
    plMap.insert(QStringLiteral("points"), QCborArray{pt1, pt2});
    multiHole.insert(QStringLiteral("placement"), plMap);

    auto mRes = evaluateModeling(d, {baseExt, multiHole});
    double mVol = volume(mRes.part.bodies[0].shape);
    near(baseVol - mVol, expectedHoleLoss * 2.0, 1e-10);

    // 4. Drilled Hole Blind with 118 deg tip
    double drillDia = 0.010, drillDepth = 0.030, drillAngle = 118.0 * std::numbers::pi / 180.0;
    auto drillHole = fields(
        {{"version", 1},
         {"id", newId()},
         {"kind", "Hole"},
         {"operation", "Cut"},
         {"target", body(baseId)},
         {"family", "Drilled"},
         {"placement",
          fields({{"face", topFace.map()}, {"sketch", plSk.id}, {"points", QCborArray{pt1}}})},
         {"sizing", fields({{"mode", "Custom"},
                            {"diameter", drillDia},
                            {"depth", drillDepth},
                            {"drill_angle", drillAngle}})},
         {"termination", fields({{"mode", "Blind"}, {"depth_reference", "Shoulder"}})}});

    auto drRes = evaluateModeling(d, {baseExt, drillHole});
    double drVol = volume(drRes.part.bodies[0].shape);
    double tipH = (drillDia / 2.0) / std::tan(drillAngle / 2.0);
    double expectedDrillLoss =
        std::numbers::pi * (drillDia / 2.0) * (drillDia / 2.0) * drillDepth +
        (1.0 / 3.0) * std::numbers::pi * (drillDia / 2.0) * (drillDia / 2.0) * tipH;
    near(baseVol - drVol, expectedDrillLoss, 1e-10);

    puts("hole engine passed");
}

void testCosmeticThread() {
    sketch::Sketch s;
    s.rectangle({0, 0}, {0.100, 0.100});

    // Cylindrical cut
    sketch::Sketch plSk;
    auto pt = plSk.point({0.050, 0.050});

    auto d = base({s, plSk});
    auto baseExt = makeExtrude(s.id, 0.050);
    auto baseId = baseExt.value("id").toString();

    auto hole = fields(
        {{"version", 1},
         {"id", newId()},
         {"kind", "Hole"},
         {"operation", "Cut"},
         {"target", body(baseId)},
         {"family", "Simple"},
         {"placement",
          fields({{"face",
                   findFace(evaluateModeling(d, {baseExt}).part.bodies[0].shape, baseId, 49.0, 51.0)
                       .map()},
                  {"sketch", plSk.id},
                  {"points", QCborArray{pt}}})},
         {"sizing", fields({{"mode", "Custom"}, {"diameter", 0.042}, {"depth", 0.050}})},
         {"termination", fields({{"mode", "ThroughAll"}, {"depth_reference", "Shoulder"}})}});
    auto holeId = hole.value("id").toString();

    auto resBefore = evaluateModeling(d, {baseExt, hole});
    double volBefore = volume(resBefore.part.bodies[0].shape);

    // Cosmetic thread on hole inner edge (Tutorial 2: M42x3.0)
    auto threadIntent =
        fields({{"version", 1},
                {"id", newId()},
                {"kind", "CosmeticThread"},
                {"target", body(holeId, baseId)},
                {"reference", threadFace(resBefore.part.bodies[0].shape, holeId).map()},
                {"designation", "M42x3.0"},
                {"standard", "ANSI Metric"},
                {"type", "Machine Thread"},
                {"size", "M42x3.0"},
                {"thread_type", "Internal"},
                {"major_diameter", 0.042},
                {"minor_diameter", 0.03875},
                {"pitch", 0.003},
                {"termination", fields({{"mode", "UpToNext"}})}});

    auto resAfter = evaluateModeling(d, {baseExt, hole, threadIntent});
    double volAfter = volume(resAfter.part.bodies[0].shape);

    // Invariant: Cosmetic thread must NOT alter solid volume or topology!
    near(volBefore, volAfter, 1e-12);
    CHECK(resAfter.part.bodies.size() == 1);
    CHECK(resAfter.part.cosmeticThreads.size() == 1);
    near(resAfter.part.cosmeticThreads[0].toMap().value("depth").toDouble(), .050, 1e-10);
    auto display = resAfter.part.mesh();
    CHECK(std::any_of(display.edgeIds.begin(), display.edgeIds.end(),
                      [](auto id) { return id.startsWith("cosmetic/"); }));

    auto invalid = threadIntent;
    invalid.insert(QStringLiteral("reference"),
                   findFace(resBefore.part.bodies[0].shape, holeId, 49, 51).map());
    support_fixture::error("InvalidInput", [&] { evaluateModeling(d, {baseExt, hole, invalid}); });
    sketch::Sketch round;
    round.circle(round.point({0, 0}), .01);
    auto roundExtrude = makeExtrude(round.id, .05);
    auto roundId = roundExtrude.value("id").toString();
    auto roundModel = base({round});
    auto roundResult = evaluateModeling(roundModel, {roundExtrude});
    auto external = threadIntent;
    external.insert(QStringLiteral("id"), newId());
    external.insert(QStringLiteral("target"), body(roundId));
    external.insert(QStringLiteral("reference"),
                    threadFace(roundResult.part.bodies[0].shape, roundId).map());
    external.insert(QStringLiteral("thread_type"), "External");
    external.insert(QStringLiteral("designation"), "M20x1.5");
    external.insert(QStringLiteral("size"), "M20x1.5");
    external.insert(QStringLiteral("major_diameter"), .020);
    external.insert(QStringLiteral("minor_diameter"), .018);
    external.insert(QStringLiteral("pitch"), .0015);
    auto externalResult = evaluateModeling(roundModel, {roundExtrude, external});
    near(volume(externalResult.part.bodies[0].shape), volume(roundResult.part.bodies[0].shape),
         1e-12);
    auto externalMesh = externalResult.part.mesh();
    CHECK(externalMesh.minimum.x() < -10 && externalMesh.maximum.x() > 10);
    puts("cosmetic thread engine passed");
}

void testRepetition() {
    // Base slab: 200 mm x 200 mm x 20 mm
    sketch::Sketch s;
    auto rectLines = s.rectangle({0, 0}, {0.200, 0.200});

    // Seed hole: Ø10 mm through
    sketch::Sketch plSk;
    auto pt = plSk.point({0.020, 0.020});

    // Midplane at X = 100 mm for mirror
    sketch::Sketch sMid;
    sMid.name = "MidPlane";
    sMid.frame.origin = {0.100, 0, 0};
    sMid.frame.u = {0, 1, 0};
    sMid.frame.v = {0, 0, 1};

    auto d = base({s, plSk, sMid});
    auto baseExt = makeExtrude(s.id, 0.020);
    auto baseId = baseExt.value("id").toString();

    double holeDia = 0.010;
    auto seedHole = fields(
        {{"version", 1},
         {"id", newId()},
         {"kind", "Hole"},
         {"operation", "Cut"},
         {"target", body(baseId)},
         {"family", "Simple"},
         {"placement",
          fields({{"face",
                   findFace(evaluateModeling(d, {baseExt}).part.bodies[0].shape, baseId, 19.0, 21.0)
                       .map()},
                  {"sketch", plSk.id},
                  {"points", QCborArray{pt}}})},
         {"sizing", fields({{"mode", "Custom"}, {"diameter", holeDia}, {"depth", 0.020}})},
         {"termination", fields({{"mode", "ThroughAll"}, {"depth_reference", "Shoulder"}})}});
    auto seedId = seedHole.value("id").toString();

    // 1. Two-Direction Linear Pattern (Tutorial 1: Spacing 43 mm, 5 instances in Dir 1, 2 in Dir 2)
    int n1 = 4, n2 = 2; // total 8 instances including seed (7 patterned)
    double sp1 = 0.043, sp2 = 0.040;
    auto linearPattern =
        fields({{"version", 1},
                {"id", newId()},
                {"kind", "LinearPattern"},
                {"operation", "Cut"},
                {"target", body(seedId, baseId)},
                {"seed_kind", "Feature"},
                {"seed", seedId},
                {"direction1",
                 fields({{"reference", support_fixture::ref(s.id, "Curve", rectLines[0]).map()},
                         {"spacing", sp1},
                         {"count", n1},
                         {"reverse", false}})},
                {"direction2",
                 fields({{"reference", support_fixture::ref(s.id, "Curve", rectLines[1]).map()},
                         {"spacing", sp2},
                         {"count", n2},
                         {"reverse", false}})}});

    auto patRes = evaluateModeling(d, {baseExt, seedHole, linearPattern});
    CHECK(patRes.part.bodies.size() == 1);
    double baseVol = 0.200 * 0.200 * 0.020;
    double singleHoleVol = std::numbers::pi * (holeDia / 2.0) * (holeDia / 2.0) * 0.020;
    double totalPatLoss = baseVol - volume(patRes.part.bodies[0].shape);
    // Oracle: 8 holes * volume per hole
    near(totalPatLoss, (n1 * n2) * singleHoleVol, 1e-10);

    // 2. Mirror of cut hole about midplane (Tutorial 1 / Tutorial 3)
    auto mirrorIntent = fields({{"version", 1},
                                {"id", newId()},
                                {"kind", "Mirror"},
                                {"operation", "Cut"},
                                {"target", body(seedId, baseId)},
                                {"seed_kind", "Feature"},
                                {"seed", seedId},
                                {"plane", support_fixture::ref(sMid.id, "Plane").map()}});

    auto mirRes = evaluateModeling(d, {baseExt, seedHole, mirrorIntent});
    CHECK(mirRes.part.bodies.size() == 1);
    double mirLoss = baseVol - volume(mirRes.part.bodies[0].shape);
    // Oracle: seed + mirror = 2 holes
    near(mirLoss, 2.0 * singleHoleVol, 1e-10);

    puts("repetition engine passed");
}

void testRib() {
    // Base L-bracket: base 100 x 100 x 20, vertical wall 100 x 20 x 80
    sketch::Sketch s;
    s.rectangle({0, 0}, {0.100, 0.100});

    // Wall sketch on base top
    sketch::Sketch sWall;
    sWall.rectangle({0, 0}, {0.100, 0.020});
    sWall.frame.origin = {0, 0, .020};

    // Rib sketch on symmetry plane: line connecting top of wall to front of base
    sketch::Sketch sRib;
    sRib.name = "RibProfile";
    sRib.frame.origin = {0.050, 0, 0};
    sRib.frame.u = {0, 1, 0};
    sRib.frame.v = {0, 0, 1};
    auto p1 = sRib.point({0.020, 0.080});
    auto p2 = sRib.point({0.080, 0.020});
    auto ribLine = sRib.line(p1, p2);

    auto d = base({s, sWall, sRib});
    auto baseExt = makeExtrude(s.id, 0.020);
    auto baseId = baseExt.value("id").toString();

    auto wallExt =
        fields({{"version", 1},
                {"id", newId()},
                {"kind", "Extrude"},
                {"operation", "Add"},
                {"target", body(baseId)},
                {"profile",
                 fields({{"sketch", sWall.id}, {"mode", "AllClosed"}, {"regions", QCborArray{}}})},
                {"start", fields({{"mode", "SketchPlane"}})},
                {"direction1", blind(0.060)},
                {"direction2", QCborValue(nullptr)},
                {"reverse", false},
                {"thin", fields({{"mode", "Solid"}})}});
    auto wallId = wallExt.value("id").toString();

    double tRib = 0.013; // Tutorial 1: 13 mm rib
    auto ribIntent = fields(
        {{"version", 1},
         {"id", newId()},
         {"kind", "Rib"},
         {"operation", "Add"},
         {"target", body(wallId, baseId)},
         {"profile",
          fields(
              {{"sketch", sRib.id},
               {"mode", "OpenChains"},
               {"regions",
                QCborArray{QCborArray{QCborArray{fields(
                    {{"entity", ribLine}, {"from", 0.0}, {"to", 1.0}, {"reverse", false}})}}}}})},
         {"thickness", tRib},
         {"side", "Both"},
         {"direction", "Parallel"},
         {"flip_material", true}});

    auto res = evaluateModeling(d, {baseExt, wallExt, ribIntent});
    CHECK(res.part.bodies.size() == 1);
    double bracketVol = volume(res.part.bodies[0].shape);
    double initialVol = 0.100 * 0.100 * 0.020 + 0.100 * 0.020 * 0.060;
    near(bracketVol - initialVol, .5 * .060 * .060 * tRib, 1e-10);

    puts("rib engine passed");
}
} // namespace

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    try {
        puts("Testing treatments...");
        testTreatments();
        puts("Testing holes...");
        testHoles();
        puts("Testing cosmetic threads...");
        testCosmeticThread();
        puts("Testing repetition...");
        testRepetition();
        puts("Testing ribs...");
        testRib();
        puts("PASS");
        return 0;
    } catch (const std::exception &e) {
        fprintf(stderr, "FATAL EXCEPTION: %s\n", e.what());
        return 1;
    }
}
