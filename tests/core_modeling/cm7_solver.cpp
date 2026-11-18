#include "model/Supports.hpp"
#include "sketch/SpatialSolver.hpp"
#include "support/check.hpp"
#include <QCoreApplication>
#include <cmath>
#include <numbers>

using namespace cad;

namespace {

domain::GeometryReference planeRef(QString owner) {
    domain::GeometryReference r;
    r.object = owner;
    r.role = QStringLiteral("Plane");
    return r;
}

void testDegreesOfFreedomAndRank() {
    sketch::SpatialSketch s;
    auto p1 = s.point({0.0, 0.0, 0.0});
    auto p2 = s.point({0.1, 0.0, 0.0});
    auto p3 = s.point({0.1, 0.1, 0.0});

    // 3 free points: 9 variables, rank 0, freedom 9
    auto res0 = sketch::solveSpatial(s);
    CHECK(res0.valid);
    CHECK(res0.rank == 0);
    CHECK(res0.freedom == 9);

    // Fix p1: 3 constraints -> rank 3, freedom 6
    sketch::SpatialConstraint cFix;
    cFix.kind = QStringLiteral("Fixed");
    cFix.a = p1;
    s.constraints.push_back(cFix);

    auto res1 = sketch::solveSpatial(s);
    CHECK(res1.valid);
    CHECK(res1.rank == 3);
    CHECK(res1.freedom == 6);

    // Distance p1-p2: 1 constraint -> rank 4, freedom 5
    sketch::SpatialConstraint d12;
    d12.kind = QStringLiteral("Distance");
    d12.a = p1;
    d12.b = p2;
    d12.value = 0.100;
    s.constraints.push_back(d12);

    auto res2 = sketch::solveSpatial(s);
    CHECK(res2.valid);
    CHECK(res2.rank == 4);
    CHECK(res2.freedom == 5);

    // Distance p2-p3: 1 constraint -> rank 5, freedom 4
    sketch::SpatialConstraint d23;
    d23.kind = QStringLiteral("Distance");
    d23.a = p2;
    d23.b = p3;
    d23.value = 0.100;
    s.constraints.push_back(d23);

    auto res3 = sketch::solveSpatial(s);
    CHECK(res3.valid);
    CHECK(res3.rank == 5);
    CHECK(res3.freedom == 4);

    // Distance p3-p1: 1 constraint -> rank 6, freedom 3 (rigid body rotation in 3D)
    sketch::SpatialConstraint d31;
    d31.kind = QStringLiteral("Distance");
    d31.a = p3;
    d31.b = p1;
    d31.value = std::sqrt(0.02);
    s.constraints.push_back(d31);

    auto res4 = sketch::solveSpatial(s);
    CHECK(res4.valid);
    CHECK(res4.rank == 6);
    CHECK(res4.freedom == 3);
}

void testParallelYZRotatedPlane() {
    // CM-038: ParallelYZ: line's local X component is zero; rotate the selected plane and re-solve
    sketch::SpatialSketch s;
    auto p1 = s.point({0.0, 0.0, 0.0});
    auto p2 = s.point({0.03, 0.04, 0.05});
    auto line = s.line(p1, p2);

    auto planeId = newId();
    sketch::SpatialConstraint c;
    c.kind = QStringLiteral("ParallelYZ");
    c.a = line;
    c.plane = planeRef(planeId);
    s.constraints.push_back(c);

    // Fixed p1
    sketch::SpatialConstraint fix1;
    fix1.kind = QStringLiteral("Fixed");
    fix1.a = p1;
    s.constraints.push_back(fix1);

    // Case 1: Standard XY plane (local u = (1, 0, 0), v = (0, 1, 0))
    // ParallelYZ forces local X component to be zero -> (p2 - p1) . (1, 0, 0) == 0 -> x2 == 0
    sketch::Frame frame1{{0, 0, 0}, {1, 0, 0}, {0, 1, 0}};
    auto res1 = sketch::solveSpatial(s, nullptr, [&](const auto &) { return frame1; });
    CHECK(res1.valid);
    auto p2Solved1 = res1.sketch.entity(p2).point;
    CHECK(std::abs(p2Solved1[0]) < 1e-6);

    // Case 2: Rotate the plane by 45 degrees around Z axis
    // local u = (cos 45, sin 45, 0) = (1/sqrt(2), 1/sqrt(2), 0)
    // local v = (-sin 45, cos 45, 0) = (-1/sqrt(2), 1/sqrt(2), 0)
    double c45 = std::cos(std::numbers::pi / 4.0);
    double s45 = std::sin(std::numbers::pi / 4.0);
    sketch::Frame frameRotated{{0, 0, 0}, {c45, s45, 0}, {-s45, c45, 0}};

    auto res2 = sketch::solveSpatial(s, nullptr, [&](const auto &) { return frameRotated; });
    CHECK(res2.valid);
    auto p2Solved2 = res2.sketch.entity(p2).point;
    double localX = p2Solved2[0] * c45 + p2Solved2[1] * s45;
    CHECK(std::abs(localX) < 1e-6);
}

void testParallelZXNonWorldDatum() {
    // CM-039: ParallelZX: line's local Y component is zero; test a non-world datum
    sketch::SpatialSketch s;
    auto p1 = s.point({0.01, 0.02, 0.03});
    auto p2 = s.point({0.04, 0.05, 0.06});
    auto line = s.line(p1, p2);

    auto planeId = newId();
    sketch::SpatialConstraint c;
    c.kind = QStringLiteral("ParallelZX");
    c.a = line;
    c.plane = planeRef(planeId);
    s.constraints.push_back(c);

    sketch::SpatialConstraint fix1;
    fix1.kind = QStringLiteral("Fixed");
    fix1.a = p1;
    s.constraints.push_back(fix1);

    // Non-world datum: tilted origin (0.1, 0.2, 0.3), rotated axes
    sketch::Frame nonWorldFrame{{0.1, 0.2, 0.3}, {0, 0, 1}, {0, 1, 0}}; // local X = Z, local Y = Y
    auto res = sketch::solveSpatial(s, nullptr, [&](const auto &) { return nonWorldFrame; });
    CHECK(res.valid);

    auto p1Solved = res.sketch.entity(p1).point;
    auto p2Solved = res.sketch.entity(p2).point;
    sketch::Vec3 delta{p2Solved[0] - p1Solved[0], p2Solved[1] - p1Solved[1],
                       p2Solved[2] - p1Solved[2]};
    // local Y is (0, 1, 0)
    double localY = delta[1];
    CHECK(std::abs(localY) < 1e-6);
}

void testAlongZConflicting() {
    // CM-040: AlongZ: direction is normal to the selected plane; conflicting direction constraints
    // fail
    sketch::SpatialSketch s;
    auto p1 = s.point({0.0, 0.0, 0.0});
    auto p2 = s.point({0.02, 0.02, 0.05});
    auto line = s.line(p1, p2);

    sketch::SpatialConstraint fix1;
    fix1.kind = QStringLiteral("Fixed");
    fix1.a = p1;
    s.constraints.push_back(fix1);

    auto planeId = newId();
    sketch::SpatialConstraint cAlongZ;
    cAlongZ.kind = QStringLiteral("AlongZ");
    cAlongZ.a = line;
    cAlongZ.plane = planeRef(planeId);
    s.constraints.push_back(cAlongZ);

    sketch::SpatialConstraint dLine;
    dLine.kind = QStringLiteral("Distance");
    dLine.a = p1;
    dLine.b = p2;
    dLine.value = 0.050;
    s.constraints.push_back(dLine);

    sketch::Frame plane{{0, 0, 0}, {1, 0, 0}, {0, 1, 0}}; // normal is (0, 0, 1)
    auto res = sketch::solveSpatial(s, nullptr, [&](const auto &) { return plane; });
    CHECK(res.valid);
    auto p2Sol = res.sketch.entity(p2).point;
    CHECK(std::abs(p2Sol[0]) < 1e-6);
    CHECK(std::abs(p2Sol[1]) < 1e-6);
    CHECK(std::abs(p2Sol[2] - 0.05) < 1e-4);

    // Now introduce a conflicting constraint: AlongX (forces direction along X axis, perpendicular
    // to Z)
    sketch::SpatialConstraint cConflicting;
    cConflicting.kind = QStringLiteral("AlongX");
    cConflicting.a = line;
    s.constraints.push_back(cConflicting);

    auto previousValid = res.sketch;
    auto resConf = sketch::solveSpatial(s, &previousValid, [&](const auto &) { return plane; });
    CHECK(!resConf.valid);
    CHECK(resConf.status == "FailedSolve");
    CHECK(!resConf.conflicting.isEmpty());
    // Preserves previous valid state
    CHECK(resConf.sketch.entity(p2).point[0] == previousValid.entity(p2).point[0]);
    CHECK(resConf.sketch.entity(p2).point[1] == previousValid.entity(p2).point[1]);
    CHECK(resConf.sketch.entity(p2).point[2] == previousValid.entity(p2).point[2]);
}

void testOnPlaneAndMovedDatum() {
    // CM-194: On Plane: signed point-to-plane distance is within tolerance; moved offset datum
    // rebuilds path
    sketch::SpatialSketch s;
    auto p1 = s.point({0.0, 0.0, 0.0});
    auto p2 = s.point({0.05, 0.05, 0.035});

    sketch::SpatialConstraint fix1;
    fix1.kind = QStringLiteral("Fixed");
    fix1.a = p1;
    s.constraints.push_back(fix1);

    auto planeId = newId();
    sketch::SpatialConstraint cOnPlane;
    cOnPlane.kind = QStringLiteral("OnPlane");
    cOnPlane.a = p2;
    cOnPlane.plane = planeRef(planeId);
    s.constraints.push_back(cOnPlane);

    // Initial offset plane at z = 0.040
    sketch::Frame offsetPlane1{{0, 0, 0.040}, {1, 0, 0}, {0, 1, 0}};
    auto res1 = sketch::solveSpatial(s, nullptr, [&](const auto &) { return offsetPlane1; });
    CHECK(res1.valid);
    CHECK(std::abs(res1.sketch.entity(p2).point[2] - 0.040) < 1e-6);

    // Moved offset datum to z = 0.065
    sketch::Frame offsetPlane2{{0, 0, 0.065}, {1, 0, 0}, {0, 1, 0}};
    auto res2 =
        sketch::solveSpatial(res1.sketch, nullptr, [&](const auto &) { return offsetPlane2; });
    CHECK(res2.valid);
    CHECK(std::abs(res2.sketch.entity(p2).point[2] - 0.065) < 1e-6);
}

void testAdmittedEntitiesAndDimensions() {
    // CM-190 & CM-191: All admitted entities and spatial dimensions
    sketch::SpatialSketch s;
    auto p0 = s.point({0.0, 0.0, 0.0});
    auto p1 = s.point({0.0, 0.0, 0.040});
    auto p2 = s.point({0.100, 0.0, 0.040});
    auto p3 = s.point({0.100, 0.085, 0.040});

    auto l01 = s.line(p0, p1);
    auto l12 = s.line(p1, p2);
    auto l23 = s.line(p2, p3);

    // Fixed p0
    sketch::SpatialConstraint cFix;
    cFix.kind = QStringLiteral("Fixed");
    cFix.a = p0;
    s.constraints.push_back(cFix);

    // Perpendicular l01 and l12
    sketch::SpatialConstraint cPerp1;
    cPerp1.kind = QStringLiteral("Perpendicular");
    cPerp1.a = l01;
    cPerp1.b = l12;
    s.constraints.push_back(cPerp1);

    // Perpendicular l12 and l23
    sketch::SpatialConstraint cPerp2;
    cPerp2.kind = QStringLiteral("Perpendicular");
    cPerp2.a = l12;
    cPerp2.b = l23;
    s.constraints.push_back(cPerp2);

    // Dimensions
    sketch::SpatialConstraint d1;
    d1.kind = QStringLiteral("Distance");
    d1.a = p0;
    d1.b = p1;
    d1.value = 0.040;
    s.constraints.push_back(d1);

    sketch::SpatialConstraint d2;
    d2.kind = QStringLiteral("Distance");
    d2.a = p1;
    d2.b = p2;
    d2.value = 0.100;
    s.constraints.push_back(d2);

    sketch::SpatialConstraint d3;
    d3.kind = QStringLiteral("Distance");
    d3.a = p2;
    d3.b = p3;
    d3.value = 0.085;
    s.constraints.push_back(d3);

    auto res = sketch::solveSpatial(s);
    CHECK(res.valid);
    CHECK(res.maximumResidual < 1e-6);

    auto solvedP0 = res.sketch.entity(p0).point;
    auto solvedP1 = res.sketch.entity(p1).point;
    auto solvedP2 = res.sketch.entity(p2).point;
    auto solvedP3 = res.sketch.entity(p3).point;

    near(sketch::norm(
             {solvedP1[0] - solvedP0[0], solvedP1[1] - solvedP0[1], solvedP1[2] - solvedP0[2]}),
         0.040, 1e-6);
    near(sketch::norm(
             {solvedP2[0] - solvedP1[0], solvedP2[1] - solvedP1[1], solvedP2[2] - solvedP1[2]}),
         0.100, 1e-6);
    near(sketch::norm(
             {solvedP3[0] - solvedP2[0], solvedP3[1] - solvedP2[1], solvedP3[2] - solvedP2[2]}),
         0.085, 1e-6);
}

void testRedundantConstraintReporting() {
    sketch::SpatialSketch s;
    auto p1 = s.point({0.0, 0.0, 0.0});
    auto p2 = s.point({0.05, 0.0, 0.0});
    auto l = s.line(p1, p2);

    // Fixed p1
    sketch::SpatialConstraint fix1;
    fix1.kind = QStringLiteral("Fixed");
    fix1.a = p1;
    s.constraints.push_back(fix1);

    // Distance 0.05
    sketch::SpatialConstraint d1;
    d1.kind = QStringLiteral("Distance");
    d1.a = p1;
    d1.b = p2;
    d1.value = 0.05;
    s.constraints.push_back(d1);

    // AlongX constraint forces delta Y = 0 and delta Z = 0
    sketch::SpatialConstraint cAlongX;
    cAlongX.kind = QStringLiteral("AlongX");
    cAlongX.a = l;
    s.constraints.push_back(cAlongX);

    // ParallelZX with standard XY plane forces delta Y = 0 (redundant with AlongX!)
    auto plId = newId();
    sketch::SpatialConstraint cRedundant;
    cRedundant.kind = QStringLiteral("ParallelZX");
    cRedundant.a = l;
    cRedundant.plane = planeRef(plId);
    s.constraints.push_back(cRedundant);

    auto res = sketch::solveSpatial(
        s, nullptr, [&](const auto &) { return sketch::Frame{{0, 0, 0}, {1, 0, 0}, {0, 1, 0}}; });
    CHECK(res.valid);
    CHECK(res.redundant.contains(cRedundant.id) || res.redundant.contains(cAlongX.id));
}

void testChairFramePathSolverGeometry() {
    // T8 Tutorial 4 chair frame 3D path: exact coordinates and relations
    sketch::SpatialSketch chair;
    auto p0 = chair.point({0.0, 0.0, 0.0});
    auto p1 = chair.point({0.0, 0.0, 0.040});
    auto p2 = chair.point({0.100, 0.0, 0.040});
    auto p3 = chair.point({0.100, 0.085, 0.040});
    auto p4 = chair.point({0.015, 0.085, 0.040});
    auto p5 = chair.point({0.0, 0.170, 0.040});

    auto l01 = chair.line(p0, p1);
    auto l12 = chair.line(p1, p2);
    auto l23 = chair.line(p2, p3);
    auto l34 = chair.line(p3, p4);
    auto l45 = chair.line(p4, p5);

    // Fix base p0
    sketch::SpatialConstraint fix0;
    fix0.kind = QStringLiteral("Fixed");
    fix0.a = p0;
    chair.constraints.push_back(fix0);

    auto pl1Id = newId();
    sketch::Frame plane1{{0, 0, 0.040}, {1, 0, 0}, {0, 1, 0}};

    // l01 Along Z
    sketch::SpatialConstraint cAlongZ;
    cAlongZ.kind = QStringLiteral("AlongZ");
    cAlongZ.a = l01;
    cAlongZ.plane = planeRef(pl1Id);
    chair.constraints.push_back(cAlongZ);

    // p5 OnPlane with Plane1
    sketch::SpatialConstraint cOnPlane;
    cOnPlane.kind = QStringLiteral("OnPlane");
    cOnPlane.a = p5;
    cOnPlane.plane = planeRef(pl1Id);
    chair.constraints.push_back(cOnPlane);

    // Dimensions
    sketch::SpatialConstraint d01;
    d01.kind = QStringLiteral("Distance");
    d01.a = p0;
    d01.b = p1;
    d01.value = 0.040;
    chair.constraints.push_back(d01);

    sketch::SpatialConstraint d12;
    d12.kind = QStringLiteral("Distance");
    d12.a = p1;
    d12.b = p2;
    d12.value = 0.100;
    chair.constraints.push_back(d12);

    sketch::SpatialConstraint d23;
    d23.kind = QStringLiteral("Distance");
    d23.a = p2;
    d23.b = p3;
    d23.value = 0.085;
    chair.constraints.push_back(d23);

    auto res = sketch::solveSpatial(chair, nullptr, [&](const auto &) { return plane1; });
    CHECK(res.valid);
    CHECK(res.maximumResidual < 1e-6);

    auto solvedP1 = res.sketch.entity(p1).point;
    auto solvedP5 = res.sketch.entity(p5).point;
    near(solvedP1[2], 0.040, 1e-6);
    near(solvedP5[2], 0.040, 1e-6);
}

} // namespace

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    return run([] {
        testDegreesOfFreedomAndRank();
        testParallelYZRotatedPlane();
        testParallelZXNonWorldDatum();
        testAlongZConflicting();
        testOnPlaneAndMovedDatum();
        testAdmittedEntitiesAndDimensions();
        testRedundantConstraintReporting();
        testChairFramePathSolverGeometry();
    });
}
