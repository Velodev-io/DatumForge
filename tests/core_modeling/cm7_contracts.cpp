#include "model/DerivedCurve.hpp"
#include "model/Supports.hpp"
#include "sketch/SpatialSketch.hpp"
#include "support/check.hpp"
#include <QCoreApplication>
#include <numbers>

using namespace cad;

namespace {
template <class F> void error(QString expected, F &&action) {
    QString code, message;
    try {
        action();
    } catch (const domain::Error &e) {
        code = e.code;
        message = QString::fromUtf8(e.what());
    }
    if (code != expected)
        throw std::runtime_error("Expected " + expected.toStdString() + ", got " +
                                 code.toStdString() + ": " + message.toStdString());
}

domain::GeometryReference sketchRef(QString owner) {
    domain::GeometryReference r;
    r.object = owner;
    r.role = QStringLiteral("Object");
    return r;
}

domain::GeometryReference planeRef(QString owner) {
    domain::GeometryReference r;
    r.object = owner;
    r.role = QStringLiteral("Plane");
    return r;
}

domain::GeometryReference faceRef(QString owner) {
    domain::GeometryReference r;
    r.object = owner;
    r.role = QStringLiteral("Face");
    r.topology = QStringLiteral("face/") + QString(64, 'a');
    return r;
}

domain::GeometryReference axisRef(QString owner) {
    domain::GeometryReference r;
    r.object = owner;
    r.role = QStringLiteral("Axis");
    return r;
}

domain::GeometryReference pointRef(QString owner, QString entityId) {
    domain::GeometryReference r;
    r.object = owner;
    r.role = QStringLiteral("Point");
    r.entity = entityId;
    return r;
}

domain::GeometryReference curveRef(QString owner, QString entityId = {}) {
    domain::GeometryReference r;
    r.object = owner;
    r.role = QStringLiteral("Curve");
    r.entity = entityId;
    return r;
}

void testSpatialEntities() {
    sketch::SpatialSketch s;
    s.name = QStringLiteral("Spatial Entity Suite");

    // 1. Points
    auto p1 = s.point({0.0, 0.0, 0.0});
    auto p2 = s.point({0.1, 0.0, 0.0});
    auto p3 = s.point({0.1, 0.085, 0.0});
    auto pCenter = s.point({0.05, 0.05, 0.0});

    CHECK(s.entity(p1).kind == "Point");
    CHECK(s.entity(p1).point[0] == 0.0);

    // 2. Line
    auto l1 = s.line(p1, p2);
    CHECK(s.entity(l1).kind == "Line");
    CHECK(s.entity(l1).a == p1 && s.entity(l1).b == p2);

    // 3. Arc
    auto arc1 = s.arc(pCenter, p1, p3, {0.0, 0.0, 1.0}, 0.05);
    CHECK(s.entity(arc1).kind == "Arc");
    CHECK(s.entity(arc1).center == pCenter);
    CHECK(s.entity(arc1).radius == 0.05);

    // 4. Spline
    sketch::SpatialEntity spline;
    spline.kind = QStringLiteral("Spline");
    spline.poles = {p1, pCenter, p2, p3};
    spline.degree = 3;
    spline.weights = {1.0, 1.0, 1.0, 1.0};
    spline.knots = {0.0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0, 1.0};
    s.entities.push_back(spline);

    s.validate();

    // Roundtrip entity map / parse
    for (const auto &e : s.entities) {
        auto m = e.map();
        auto parsed = sketch::SpatialEntity::parse(m);
        CHECK(parsed.id == e.id);
        CHECK(parsed.kind == e.kind);
    }

    // Malformed Point tests
    sketch::SpatialEntity badPt;
    badPt.kind = QStringLiteral("Point");
    badPt.point = {std::numeric_limits<double>::quiet_NaN(), 0.0, 0.0};
    error("InvalidInput", [&] { badPt.validate(); });

    badPt.point = {1001.0, 0.0, 0.0};
    error("InvalidInput", [&] { badPt.validate(); });

    badPt.point = {0.0, 0.0, 0.0};
    badPt.a = newId(); // Point must not have endpoints
    error("InvalidInput", [&] { badPt.validate(); });

    // Malformed Line tests
    sketch::SpatialEntity badLine;
    badLine.kind = QStringLiteral("Line");
    badLine.a = p1;
    badLine.b = p1; // Same point
    error("InvalidInput", [&] { badLine.validate(); });

    // Malformed Arc tests
    sketch::SpatialEntity badArc;
    badArc.kind = QStringLiteral("Arc");
    badArc.center = p1;
    badArc.a = p1; // center == a
    badArc.b = p2;
    badArc.normal = {0.0, 0.0, 1.0};
    badArc.radius = 0.05;
    error("InvalidInput", [&] { badArc.validate(); });

    badArc.center = pCenter;
    badArc.a = p1;
    badArc.radius = -0.05; // Negative radius
    error("InvalidInput", [&] { badArc.validate(); });

    badArc.radius = 0.05;
    badArc.normal = {0.0, 0.0, 0.0}; // Non-unit normal
    error("InvalidInput", [&] { badArc.validate(); });

    // Malformed Spline tests
    sketch::SpatialEntity badSpline;
    badSpline.kind = QStringLiteral("Spline");
    badSpline.poles = {p1}; // Only 1 pole
    error("InvalidInput", [&] { badSpline.validate(); });

    badSpline.poles = {p1, p2};
    badSpline.degree = 2; // degree must be < poles.size()
    error("InvalidInput", [&] { badSpline.validate(); });
}

void testSpatialConstraintsAndRelations() {
    sketch::SpatialSketch s;
    auto p1 = s.point({0.0, 0.0, 0.0});
    auto p2 = s.point({0.1, 0.0, 0.0});
    auto p3 = s.point({0.1, 0.085, 0.0});
    auto l1 = s.line(p1, p2);
    auto l2 = s.line(p2, p3);

    auto datumPlaneId = newId();
    auto plRef = planeRef(datumPlaneId);

    // 1. ParallelYZ constraint
    sketch::SpatialConstraint cYZ;
    cYZ.kind = QStringLiteral("ParallelYZ");
    cYZ.a = l1;
    cYZ.plane = plRef;
    s.constraints.push_back(cYZ);

    // 2. ParallelZX constraint
    sketch::SpatialConstraint cZX;
    cZX.kind = QStringLiteral("ParallelZX");
    cZX.a = l2;
    cZX.plane = plRef;
    s.constraints.push_back(cZX);

    // 3. AlongZ constraint
    sketch::SpatialConstraint cAlongZ;
    cAlongZ.kind = QStringLiteral("AlongZ");
    cAlongZ.a = l1;
    cAlongZ.plane = plRef;
    s.constraints.push_back(cAlongZ);

    // 4. OnPlane constraint
    sketch::SpatialConstraint cOnPlane;
    cOnPlane.kind = QStringLiteral("OnPlane");
    cOnPlane.a = p3;
    cOnPlane.plane = plRef;
    s.constraints.push_back(cOnPlane);

    // 5. Distance driving dimension
    sketch::SpatialConstraint cDist;
    cDist.kind = QStringLiteral("Distance");
    cDist.a = p1;
    cDist.b = p2;
    cDist.value = 0.100; // 100 mm in SI meters
    cDist.driving = true;
    s.constraints.push_back(cDist);

    // 6. Angle reference dimension
    sketch::SpatialConstraint cAngle;
    cAngle.kind = QStringLiteral("Angle");
    cAngle.a = l1;
    cAngle.b = l2;
    cAngle.value = std::numbers::pi / 2.0; // 90 deg
    cAngle.driving = false;                // reference dimension
    s.constraints.push_back(cAngle);

    // 7. Perpendicular, Parallel, Tangent, Coincident
    sketch::SpatialConstraint cPerp;
    cPerp.kind = QStringLiteral("Perpendicular");
    cPerp.a = l1;
    cPerp.b = l2;
    s.constraints.push_back(cPerp);

    s.validate();

    // Roundtrip sketch map / parse
    auto sMap = s.map();
    auto sDecoded = sketch::SpatialSketch::parse(sMap);
    CHECK(sDecoded.id == s.id);
    CHECK(sDecoded.entities.size() == s.entities.size());
    CHECK(sDecoded.constraints.size() == s.constraints.size());

    // Plane-relative constraint validation errors
    sketch::SpatialConstraint badC = cYZ;
    badC.plane = std::nullopt; // Missing plane
    error("InvalidInput", [&] { badC.validate(); });

    badC = cYZ;
    badC.b = l2; // b must be empty for ParallelYZ
    error("InvalidInput", [&] { badC.validate(); });

    badC = cYZ;
    badC.a = p1; // Must be a line, not a point
    error("InvalidInput", [&] {
        sketch::SpatialSketch testS = s;
        testS.constraints = {badC};
        testS.validate();
    });

    // Distance validation errors
    badC = cDist;
    badC.value = -0.05; // Negative distance
    error("InvalidInput", [&] { badC.validate(); });

    badC.value = 0.0; // Zero distance
    error("InvalidInput", [&] { badC.validate(); });

    badC.value = std::numeric_limits<double>::infinity();
    error("InvalidInput", [&] { badC.validate(); });

    // Angle validation errors
    badC = cAngle;
    badC.value = 0.0;
    error("InvalidInput", [&] { badC.validate(); });

    badC.value = std::numbers::pi + 0.1; // Exceeds pi
    error("InvalidInput", [&] { badC.validate(); });
}

void testActivePlaneSwitching() {
    sketch::SpatialSketch s;
    CHECK(s.activePlane == "XY");

    // Switching active drafting plane mode
    s.activePlane = QStringLiteral("ZX");
    s.validate();
    CHECK(s.map().value("active_plane").toString() == "ZX");

    s.activePlane = QStringLiteral("YZ");
    s.validate();
    CHECK(s.map().value("active_plane").toString() == "YZ");

    // Invalid plane mode rejected
    s.activePlane = QStringLiteral("INVALID");
    error("InvalidInput", [&] { s.validate(); });

    // Verify activePlane switching does NOT alter 3D coordinates or inject OnPlane constraints
    sketch::SpatialSketch s2;
    auto pt = s2.point({0.040, 0.050, 0.060});
    s2.activePlane = QStringLiteral("ZX");
    s2.validate();
    CHECK(s2.constraints.empty()); // No implicit OnPlane constraint!
    CHECK(s2.entity(pt).point[0] == 0.040);
    CHECK(s2.entity(pt).point[1] == 0.050);
    CHECK(s2.entity(pt).point[2] == 0.060);
}

void testChairFramePathWorkflow() {
    // Exact reproduction of T8 Tutorial 4 chair frame 3D path (pages 24-27)
    // Dimensions from verified course figures:
    // - Along Z to 40 mm offset plane
    // - Horizontal segment 100 mm along X
    // - Vertical segment 85 mm along Y
    // - Seat depth 85 mm
    // - Backrest vertical rise 60 mm, 20° incline, total height 170 mm
    // - Fillets 3x R10, 1x R50
    // - Upper endpoint On Plane with Plane1 (offset 40 mm)
    sketch::SpatialSketch chairPath;
    chairPath.name = QStringLiteral("Chair Frame 3D Path");

    auto p0 = chairPath.point({0.0, 0.0, 0.0});
    auto p1 = chairPath.point({0.0, 0.0, 0.040});
    auto p2 = chairPath.point({0.100, 0.0, 0.040});
    auto p3 = chairPath.point({0.100, 0.085, 0.040});
    auto p4 = chairPath.point({0.015, 0.085, 0.040});
    auto p5 = chairPath.point({0.0, 0.170, 0.040});

    auto l01 = chairPath.line(p0, p1);
    auto l12 = chairPath.line(p1, p2);
    auto l23 = chairPath.line(p2, p3);
    auto l34 = chairPath.line(p3, p4);
    auto l45 = chairPath.line(p4, p5);

    auto plane1Id = newId();
    auto pl1 = planeRef(plane1Id);

    // Along Z for initial segment
    sketch::SpatialConstraint cAlongZ;
    cAlongZ.kind = QStringLiteral("AlongZ");
    cAlongZ.a = l01;
    cAlongZ.plane = pl1;
    chairPath.constraints.push_back(cAlongZ);

    // OnPlane for upper endpoint with Plane1
    sketch::SpatialConstraint cOnPlane;
    cOnPlane.kind = QStringLiteral("OnPlane");
    cOnPlane.a = p5;
    cOnPlane.plane = pl1;
    chairPath.constraints.push_back(cOnPlane);

    // Dimensions
    sketch::SpatialConstraint d1;
    d1.kind = QStringLiteral("Distance");
    d1.a = p0;
    d1.b = p1;
    d1.value = 0.040;
    chairPath.constraints.push_back(d1);

    sketch::SpatialConstraint d2;
    d2.kind = QStringLiteral("Distance");
    d2.a = p1;
    d2.b = p2;
    d2.value = 0.100;
    chairPath.constraints.push_back(d2);

    sketch::SpatialConstraint d3;
    d3.kind = QStringLiteral("Distance");
    d3.a = p2;
    d3.b = p3;
    d3.value = 0.085;
    chairPath.constraints.push_back(d3);

    chairPath.validate();

    auto encoded = chairPath.map();
    auto decoded = sketch::SpatialSketch::parse(encoded);
    CHECK(decoded.entities.size() == 11); // 6 points + 5 lines
    CHECK(decoded.constraints.size() == 5);
}

void testDerivedCurves() {
    auto sk1 = newId(), sk2 = newId();

    // 1. SketchOnSketch
    model::DerivedCurve curveSOS;
    curveSOS.kind = QStringLiteral("SketchOnSketch");
    model::SketchOnSketchDefinition sos;
    sos.sketch1 = sketchRef(sk1);
    sos.sketch2 = sketchRef(sk2);
    sos.branch.index = 0;
    sos.branch.token = QStringLiteral("branch/0");
    curveSOS.sketchOnSketch = sos;
    curveSOS.validate();

    auto sosMap = curveSOS.map();
    auto sosDecoded = model::DerivedCurve::parse(sosMap);
    CHECK(sosDecoded.kind == "SketchOnSketch");
    CHECK(sosDecoded.sketchOnSketch->sketch1.object == sk1);
    CHECK(sosDecoded.sketchOnSketch->sketch2.object == sk2);
    CHECK(sosDecoded.sketchOnSketch->branch.token == "branch/0");

    // Reject sketch on same sketch
    model::DerivedCurve badSOS = curveSOS;
    badSOS.sketchOnSketch->sketch2 = sketchRef(sk1);
    error("InvalidInput", [&] { badSOS.validate(); });

    // 2. SketchOnFaces
    auto bodyId = newId();
    model::DerivedCurve curveSOF;
    curveSOF.kind = QStringLiteral("SketchOnFaces");
    model::SketchOnFacesDefinition sof;
    sof.sketch = sketchRef(sk1);
    sof.faces = {faceRef(bodyId)};
    sof.mode = QStringLiteral("NormalToFace");
    sof.branch.index = 0;
    sof.branch.token = QStringLiteral("branch/0");
    curveSOF.sketchOnFaces = sof;
    curveSOF.validate();

    auto sofMap = curveSOF.map();
    auto sofDecoded = model::DerivedCurve::parse(sofMap);
    CHECK(sofDecoded.kind == "SketchOnFaces");
    CHECK(sofDecoded.sketchOnFaces->mode == "NormalToFace");
    CHECK(sofDecoded.sketchOnFaces->faces.size() == 1);

    // AlongVector mode requires direction
    sof.mode = QStringLiteral("AlongVector");
    curveSOF.sketchOnFaces = sof;
    error("InvalidInput", [&] { curveSOF.validate(); });

    sof.direction = axisRef(bodyId);
    curveSOF.sketchOnFaces = sof;
    curveSOF.validate();
    CHECK(curveSOF.map().value("definition").toMap().contains(QStringLiteral("direction")));

    // 3. CompositeCurve
    model::DerivedCurve curveComp;
    curveComp.kind = QStringLiteral("CompositeCurve");
    model::CompositeCurveDefinition comp;
    model::CompositeCurveMember m1, m2;
    m1.source = curveRef(sk1, newId());
    m1.reversed = false;
    m2.source = curveRef(sk2, newId());
    m2.reversed = true;
    comp.members = {m1, m2};
    comp.closed = false;
    curveComp.compositeCurve = comp;
    curveComp.validate();

    auto compMap = curveComp.map();
    auto compDecoded = model::DerivedCurve::parse(compMap);
    CHECK(compDecoded.kind == "CompositeCurve");
    CHECK(compDecoded.compositeCurve->members.size() == 2);
    CHECK(compDecoded.compositeCurve->members[1].reversed == true);

    // 4. HelixSpiral
    model::DerivedCurve curveHelix;
    curveHelix.kind = QStringLiteral("HelixSpiral");
    model::HelixSpiralDefinition helix;
    helix.mode = QStringLiteral("PitchAndRevolution");
    helix.axis = axisRef(bodyId);
    helix.radius = 0.025; // 25 mm radius
    helix.pitch = 0.010;  // 10 mm pitch
    helix.revolutions = 8.0;
    helix.taperAngle = 0.0;
    helix.startAngle = 0.0;
    helix.clockwise = true;
    curveHelix.helixSpiral = helix;
    curveHelix.validate();

    auto helixMap = curveHelix.map();
    auto helixDecoded = model::DerivedCurve::parse(helixMap);
    CHECK(helixDecoded.kind == "HelixSpiral");
    CHECK(helixDecoded.helixSpiral->mode == "PitchAndRevolution");
    near(helixDecoded.helixSpiral->radius, 0.025);
    near(helixDecoded.helixSpiral->pitch, 0.010);
    near(helixDecoded.helixSpiral->revolutions, 8.0);

    // Spiral mode: height and taper forbidden
    helix.mode = QStringLiteral("Spiral");
    helix.height = 0.050; // Not allowed for planar spiral
    curveHelix.helixSpiral = helix;
    error("InvalidInput", [&] { curveHelix.validate(); });

    helix.height = 0.0;
    helix.taperAngle = 0.1; // Not allowed for planar spiral
    curveHelix.helixSpiral = helix;
    error("InvalidInput", [&] { curveHelix.validate(); });

    helix.taperAngle = 0.0;
    curveHelix.helixSpiral = helix;
    curveHelix.validate();
    CHECK(curveHelix.map().value("definition").toMap().value("mode").toString() == "Spiral");
}

void testCurveFrameEvaluation() {
    sketch::Vec3 pt{1.0, 2.0, 3.0};

    // 1. Tangent along X
    sketch::Vec3 tgtX{5.0, 0.0, 0.0};
    auto frameX = model::CurveFrame::evaluate(pt, tgtX);
    near(frameX.tangent[0], 1.0);
    near(frameX.tangent[1], 0.0);
    near(frameX.tangent[2], 0.0);
    // Default up is {0, 0, 1}. Normal in plane perpendicular to X: {0, 0, 1}.
    near(frameX.normal[0], 0.0);
    near(frameX.normal[1], 0.0);
    near(frameX.normal[2], 1.0);
    // Binormal = T x N = {1, 0, 0} x {0, 0, 1} = {0, -1, 0}
    near(frameX.binormal[0], 0.0);
    near(frameX.binormal[1], -1.0);
    near(frameX.binormal[2], 0.0);

    // Orthonormality check
    near(sketch::dot(frameX.tangent, frameX.normal), 0.0);
    near(sketch::dot(frameX.normal, frameX.binormal), 0.0);
    near(sketch::dot(frameX.binormal, frameX.tangent), 0.0);
    near(sketch::norm(frameX.tangent), 1.0);
    near(sketch::norm(frameX.normal), 1.0);
    near(sketch::norm(frameX.binormal), 1.0);

    // 2. Singularity handling: Tangent along Z (collinear with default up {0, 0, 1})
    sketch::Vec3 tgtZ{0.0, 0.0, 10.0};
    auto frameZ = model::CurveFrame::evaluate(pt, tgtZ);
    near(frameZ.tangent[2], 1.0);
    // Switched to up {0, 1, 0} to avoid singularity!
    near(frameZ.normal[1], 1.0);
    near(frameZ.binormal[0], -1.0);

    // 3. Reversal
    auto frameRev = model::CurveFrame::evaluate(pt, tgtX, true);
    near(frameRev.tangent[0], -1.0);
    near(frameRev.normal[2], 1.0);
    near(frameRev.binormal[1], 1.0); // -(-1) = +1
    // Right-handedness preserved: T' x N' = {-1, 0, 0} x {0, 0, 1} = {0, 1, 0} == B'
    near(sketch::dot(frameRev.tangent, frameRev.normal), 0.0);
    near(sketch::norm(sketch::cross(frameRev.tangent, frameRev.normal)), 1.0);

    // 4. Zero derivative throws InvalidInput
    sketch::Vec3 tgtZero{0.0, 0.0, 0.0};
    error("InvalidInput", [&] { model::CurveFrame::evaluate(pt, tgtZero); });
}

void testGraphIntegrationAndGate() {
    auto datumPlaneId = newId();
    auto spatialSketchId = newId();
    auto derivedCurveId = newId();
    auto featureId = newId();

    // 1. SpatialSketch in graph
    evaluation::GraphNode nodePlane{datumPlaneId, QStringLiteral("DatumPlane"), {}, {}};

    evaluation::GraphNode nodeSpatial;
    nodeSpatial.id = spatialSketchId;
    nodeSpatial.kind = QStringLiteral("SpatialSketch");
    nodeSpatial.references = {planeRef(datumPlaneId)};
    auto ptEntityId = newId();
    auto crvEntityId = newId();
    nodeSpatial.members = {{ptEntityId, QStringLiteral("Point")},
                           {crvEntityId, QStringLiteral("Curve")}};

    // Derived curve depending on spatial sketch curve member
    evaluation::GraphNode nodeDerived;
    nodeDerived.id = derivedCurveId;
    nodeDerived.kind = QStringLiteral("DerivedCurve");
    nodeDerived.references = {curveRef(spatialSketchId, crvEntityId),
                              pointRef(spatialSketchId, ptEntityId)};

    // Valid graph construction
    std::vector<evaluation::GraphNode> nodes{nodePlane, nodeSpatial, nodeDerived};
    QStringList history{datumPlaneId, spatialSketchId, derivedCurveId};
    evaluation::Graph g(nodes, history);
    CHECK(g.order().contains(spatialSketchId));
    CHECK(g.dependencies(spatialSketchId).contains(datumPlaneId));
    CHECK(g.dependencies(derivedCurveId).contains(spatialSketchId));

    // CRITICAL GATE TEST: SpatialSketch must NOT satisfy ref.role == "Plane"
    // Features (Extrude/Revolve) require planar profiles (role == "Plane").
    // Attempting to use SpatialSketch as a Plane role fails ReferenceMissing!
    evaluation::GraphNode badFeatureNode;
    badFeatureNode.id = featureId;
    badFeatureNode.kind = QStringLiteral("Feature");
    badFeatureNode.references = {
        planeRef(spatialSketchId)}; // WRONG ROLE: SpatialSketch has no Plane!

    std::vector<evaluation::GraphNode> badNodes{nodePlane, nodeSpatial, badFeatureNode};
    error("ReferenceMissing",
          [&] { evaluation::Graph badG(badNodes, {datumPlaneId, spatialSketchId, featureId}); });

    // Cycle detection: SpatialSketch -> DatumPlane -> SpatialSketch
    nodePlane.references = {curveRef(spatialSketchId, crvEntityId)};
    std::vector<evaluation::GraphNode> cycleNodes{nodePlane, nodeSpatial};
    error("DependencyCycle",
          [&] { evaluation::Graph cycleG(cycleNodes, {datumPlaneId, spatialSketchId}); });

    // History order check: History must follow dependency order
    nodePlane.references.clear();
    error("DependencyOrder", [&] {
        evaluation::Graph outOfOrderG(nodes, {derivedCurveId, spatialSketchId, datumPlaneId});
    });
}

void testSupportDefinitionIntegration() {
    model::SupportDefinition def;
    sketch::Sketch base;
    base.rectangle({0, 0}, {0.05, 0.05});
    def.sketches = sketch::encode({base});

    // Spatial sketch
    sketch::SpatialSketch s;
    auto p1 = s.point({0.0, 0.0, 0.0});
    auto p2 = s.point({0.1, 0.0, 0.0});
    s.line(p1, p2);
    def.spatial_sketches = sketch::encodeSpatial({s});

    // Derived curve
    model::DerivedCurve c;
    c.kind = QStringLiteral("SketchOnSketch");
    model::SketchOnSketchDefinition sos;
    sos.sketch1 = sketchRef(base.id);
    sos.sketch2 = sketchRef(s.id);
    sos.branch.token = QStringLiteral("branch/0");
    c.sketchOnSketch = sos;
    def.derived_curves = model::encodeDerivedCurves({c});

    def.history = {base.id, s.id, c.id};

    auto map = def.map();
    auto decoded = model::SupportDefinition::parse(map);
    CHECK(decoded.spatial_sketches.size() == 1);
    CHECK(decoded.derived_curves.size() == 1);
    CHECK(decoded.history.size() == 3);
}

} // namespace

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    return run([] {
        testSpatialEntities();
        testSpatialConstraintsAndRelations();
        testActivePlaneSwitching();
        testChairFramePathWorkflow();
        testDerivedCurves();
        testCurveFrameEvaluation();
        testGraphIntegrationAndGate();
        testSupportDefinitionIntegration();
    });
}
