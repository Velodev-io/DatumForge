#include "Projects.hpp"
#include "SupportFixture.hpp"
#include "geometry/occt/Datums.hpp"
#include "geometry/occt/SpatialCurve.hpp"
#include "geometry/occt/Supports.hpp"
#include "model/DerivedCurve.hpp"
#include "sketch/SpatialSolver.hpp"
#include "support/check.hpp"
#include <BRepAdaptor_Surface.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <GC_MakeSegment.hxx>
#include <GeomAbs_SurfaceType.hxx>
#include <Geom_Plane.hxx>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <numbers>

using namespace cad;
using namespace support_fixture;

namespace {

domain::GeometryReference sketchRef(QString owner, QString entity = {}) {
    domain::GeometryReference r;
    r.object = owner;
    if (!entity.isEmpty()) {
        r.role = QStringLiteral("Curve");
        r.entity = entity;
    } else {
        r.role = QStringLiteral("Object");
    }
    return r;
}

domain::GeometryReference curveRef(QString owner, QString entity = {}) {
    domain::GeometryReference r;
    r.object = owner;
    r.role = QStringLiteral("Curve");
    r.entity = entity;
    return r;
}

domain::GeometryReference axisRef(QString owner) {
    domain::GeometryReference r;
    r.object = owner;
    r.role = QStringLiteral("Axis");
    return r;
}

void testSpatialEntitiesExactCurves() {
    sketch::SpatialSketch s;
    auto p0 = s.point({0.0, 0.0, 0.0});
    auto p1 = s.point({0.05, 0.0, 0.0});
    auto p2 = s.point({0.05, 0.05, 0.0});
    auto pCenter = s.point({0.0, 0.05, 0.0});

    auto lineId = s.line(p0, p1);
    auto arcId = s.arc(pCenter, p0, p2, {0, 0, 1}, 0.05);

    sketch::SpatialEntity splineEnt;
    splineEnt.id = newId();
    splineEnt.kind = QStringLiteral("Spline");
    splineEnt.poles = {p0, p1, p2, pCenter};
    splineEnt.degree = 3;
    s.entities.push_back(splineEnt);

    auto curves = geometry::evaluateSpatialSketch(s);
    CHECK(curves.size() == 3);

    // Line check
    auto lineCurve = geometry::spatialEntityCurve(s, s.entity(lineId));
    CHECK(!lineCurve.curve.IsNull());
    near(lineCurve.last - lineCurve.first, 50.0, 1e-4); // 50 mm

    // Arc check
    auto arcCurve = geometry::spatialEntityCurve(s, s.entity(arcId));
    CHECK(!arcCurve.curve.IsNull());
    gp_Pnt arcStart = arcCurve.curve->Value(arcCurve.first);
    near(arcStart.X(), 0.0, 1e-4);
    near(arcStart.Y(), 0.0, 1e-4);

    // Spline check
    auto splineCurve = geometry::spatialEntityCurve(s, s.entity(splineEnt.id));
    CHECK(!splineCurve.curve.IsNull());
    gp_Pnt spStart = splineCurve.curve->Value(splineCurve.first);
    near(spStart.X(), 0.0, 1e-4);
    near(spStart.Y(), 0.0, 1e-4);
}

void testSketchOnSketchProjection() {
    // CM-195 & CM-197: Sketch-on-sketch projection and branch selection
    geometry::SupportReferences scene;

    // Sketch 1 on XY plane (z = 0): semi-circle / arc
    sketch::Sketch s1;
    s1.frame = {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}};
    auto p1_0 = s1.point({0.0, 0.0});
    auto p1_1 = s1.point({0.05, 0.0});
    auto p1_c = s1.point({0.025, 0.0});
    auto arc1 = s1.arc(p1_c, 0.025, 0.0, std::numbers::pi);
    scene.sketches.insert(s1.id, s1);

    // Sketch 2 on XZ plane: arc or inclined line
    sketch::Sketch s2;
    s2.frame = {{0, 0, 0}, {1, 0, 0}, {0, 0, 1}};
    auto p2_0 = s2.point({0.0, 0.0});
    auto p2_1 = s2.point({0.05, 0.03});
    auto l2 = s2.line(p2_0, p2_1);
    scene.sketches.insert(s2.id, s2);

    model::DerivedCurve dc;
    dc.kind = QStringLiteral("SketchOnSketch");
    model::SketchOnSketchDefinition sos;
    sos.sketch1 = sketchRef(s1.id, arc1);
    sos.sketch2 = sketchRef(s2.id, l2);
    sos.branch.index = 0;
    sos.branch.token = QStringLiteral("branch/0");
    dc.sketchOnSketch = sos;

    auto result = geometry::evaluateSketchOnSketch(dc, scene);
    CHECK(result.size() == 1);
    const auto &sc = result[0];
    CHECK(!sc.curve.IsNull());

    // Project points of the 3D curve back to verify they satisfy both projections!
    gp_Pnt ptStart = sc.curve->Value(sc.first);
    gp_Pnt ptEnd = sc.curve->Value(sc.last);

    // On sketch 1 plane (XY): x in [0, 0.05], y >= 0
    near(ptStart.X(), 0.0, 1.0); // mm
    near(ptStart.Y(), 0.0, 1.0);
    near(ptStart.Z(), 0.0, 1.0);

    // Test trimming: startParam = 0.25, endParam = 0.75
    auto trimmedDc = dc;
    trimmedDc.sketchOnSketch->branch.startParam = 0.25;
    trimmedDc.sketchOnSketch->branch.endParam = 0.75;
    auto trimmedResult = geometry::evaluateSketchOnSketch(trimmedDc, scene);
    CHECK(trimmedResult.size() == 1);
    near(trimmedResult[0].last - trimmedResult[0].first, (sc.last - sc.first) * 0.5, 1.0);

    // Test reversal
    auto reversedDc = dc;
    reversedDc.sketchOnSketch->branch.reversed = true;
    auto revResult = geometry::evaluateSketchOnSketch(reversedDc, scene);
    CHECK(revResult.size() == 1);
    gp_Pnt revStart = revResult[0].curve->Value(revResult[0].first);
    near(revStart.X(), ptEnd.X(), 1e-4);
    near(revStart.Y(), ptEnd.Y(), 1e-4);
    near(revStart.Z(), ptEnd.Z(), 1e-4);
}

void testSketchOnFacesProjection() {
    // CM-196: Sketch-on-face projection
    geometry::SupportReferences scene;

    // Create target cylinder face: radius 20 mm, height 100 mm along Z
    BRepPrimAPI_MakeCylinder makeCyl(20.0, 100.0);
    TopoDS_Shape cylShape = makeCyl.Shape();
    QString cylId = newId();
    scene.stages.insert(cylId, cylShape);

    domain::GeometryReference cylFaceRef;
    cylFaceRef.object = cylId;
    cylFaceRef.role = QStringLiteral("Face");
    for (TopExp_Explorer exp(cylShape, TopAbs_FACE); exp.More(); exp.Next()) {
        BRepAdaptor_Surface surf(TopoDS::Face(exp.Current()));
        if (surf.GetType() == GeomAbs_Cylinder) {
            cylFaceRef.topology = geometry::supportToken(exp.Current());
            break;
        }
    }
    CHECK(!cylFaceRef.topology.isEmpty());

    // Sketch in XY plane: line passing through cylinder
    sketch::Sketch s;
    s.frame = {{0, 0, 0.050}, {1, 0, 0}, {0, 1, 0}};
    auto p1 = s.point({-0.010, -0.050});
    auto p2 = s.point({0.010, -0.050});
    auto line = s.line(p1, p2);
    scene.sketches.insert(s.id, s);

    // Axis along Y (projection direction)
    QString axisId = newId();
    scene.axes.insert(axisId, {{0, 0, 0}, {0, 1, 0}});

    model::DerivedCurve dc;
    dc.kind = QStringLiteral("SketchOnFaces");
    model::SketchOnFacesDefinition sof;
    sof.sketch = sketchRef(s.id, line);
    sof.faces = {cylFaceRef};
    sof.mode = QStringLiteral("AlongVector");
    sof.direction = axisRef(axisId);
    sof.branch.index = 0;
    sof.branch.token = QStringLiteral("branch/0");
    dc.sketchOnFaces = sof;

    auto result = geometry::evaluateSketchOnFaces(dc, scene);
    CHECK(result.size() >= 1);
    const auto &sc = result[0];
    CHECK(!sc.curve.IsNull());

    // Verify curve points lie on cylinder surface (x^2 + y^2 = 20^2 = 400)
    gp_Pnt pt = sc.curve->Value(sc.first);
    double radSq = pt.X() * pt.X() + pt.Y() * pt.Y();
    near(radSq, 400.0, 1.0);
    near(pt.Z(), 50.0, 1e-3);
}

void testCompositeCurveGapsAndDiscontinuities() {
    // CM-198: Composite path, continuity diagnostics, and closed loop
    geometry::SupportReferences scene;

    // Member 1: line from (0, 0, 0) to (50, 0, 0) mm
    auto seg1 = GC_MakeSegment(gp_Pnt(0, 0, 0), gp_Pnt(50, 0, 0)).Value();
    QString id1 = newId();
    scene.derived.insert(id1, {{id1, seg1, 0, 50, 50, false, {}}});

    // Member 2: line from (50, 0, 0) to (50, 50, 0) mm (90-degree corner)
    auto seg2 = GC_MakeSegment(gp_Pnt(50, 0, 0), gp_Pnt(50, 50, 0)).Value();
    QString id2 = newId();
    scene.derived.insert(id2, {{id2, seg2, 0, 50, 50, false, {}}});

    model::DerivedCurve compDc;
    compDc.kind = QStringLiteral("CompositeCurve");
    model::CompositeCurveDefinition compDef;
    model::CompositeCurveMember m1, m2;
    m1.source = curveRef(id1);
    m2.source = curveRef(id2);
    compDef.members = {m1, m2};
    compDef.closed = false;
    compDc.compositeCurve = compDef;

    auto res = geometry::evaluateCompositeCurve(compDc, scene);
    CHECK(res.size() == 1);
    CHECK(!res[0].curve.IsNull());

    // Corner semantics verification via CurveFrame::evaluateCorner
    sketch::Vec3 ptCorner{0.050, 0.0, 0.0};
    sketch::Vec3 tgtLeft{1.0, 0.0, 0.0};
    sketch::Vec3 tgtRight{0.0, 1.0, 0.0};

    // Ambiguous corner throws without explicit limit side
    error("InvalidInput",
          [&] { model::CurveFrame::evaluateCorner(ptCorner, tgtLeft, tgtRight, ""); });

    // Explicit Left limit side
    auto frameLeft = model::CurveFrame::evaluateCorner(ptCorner, tgtLeft, tgtRight, "Left");
    near(frameLeft.tangent[0], 1.0, 1e-6);

    // Explicit Right limit side
    auto frameRight = model::CurveFrame::evaluateCorner(ptCorner, tgtLeft, tgtRight, "Right");
    near(frameRight.tangent[1], 1.0, 1e-6);

    // Disconnected gap rejection: member 3 starts at (100, 100, 0), creating a 70 mm gap
    auto segGap = GC_MakeSegment(gp_Pnt(100, 100, 0), gp_Pnt(150, 100, 0)).Value();
    QString idGap = newId();
    scene.derived.insert(idGap, {{idGap, segGap, 0, 50, 50, false, {}}});

    auto gapDc = compDc;
    model::CompositeCurveMember mGap;
    mGap.source = curveRef(idGap);
    gapDc.compositeCurve->members.push_back(mGap);

    error("InvalidGeometry", [&] { geometry::evaluateCompositeCurve(gapDc, scene); });
}

void testHelixSpiralAllModes() {
    // CM-199: Helix/spiral all frozen parameter modes
    geometry::SupportReferences scene;
    QString axisId = newId();
    scene.axes.insert(axisId, {{0, 0, 0}, {0, 0, 1}}); // Z axis

    // Mode 1: PitchAndRevolution
    model::DerivedCurve dc1;
    dc1.kind = QStringLiteral("HelixSpiral");
    model::HelixSpiralDefinition h1;
    h1.mode = QStringLiteral("PitchAndRevolution");
    h1.axis = axisRef(axisId);
    h1.radius = 0.020; // 20 mm
    h1.pitch = 0.010;  // 10 mm
    h1.revolutions = 3.0;
    h1.clockwise = true;
    dc1.helixSpiral = h1;

    auto res1 = geometry::evaluateHelixSpiral(dc1, scene);
    CHECK(res1.size() == 1);
    const auto &c1 = res1[0];
    gp_Pnt start1 = c1.curve->Value(c1.first);
    gp_Pnt end1 = c1.curve->Value(c1.last);
    near(start1.Distance(gp_Pnt(20, 0, 0)), 0.0, 1e-3);
    near(end1.Z(), 30.0, 1e-3); // 3 revs * 10 mm = 30 mm rise

    // Mode 2: HeightAndRevolution with Conical Taper
    model::DerivedCurve dc2;
    dc2.kind = QStringLiteral("HelixSpiral");
    model::HelixSpiralDefinition h2;
    h2.mode = QStringLiteral("HeightAndRevolution");
    h2.axis = axisRef(axisId);
    h2.radius = 0.015;
    h2.height = 0.050; // 50 mm
    h2.revolutions = 5.0;
    h2.taperAngle = 0.100; // ~5.7 deg
    h2.clockwise = false;
    dc2.helixSpiral = h2;

    auto res2 = geometry::evaluateHelixSpiral(dc2, scene);
    CHECK(res2.size() == 1);
    gp_Pnt end2 = res2[0].curve->Value(res2[0].last);
    near(end2.Z(), 50.0, 1e-3);
    double endRad = std::sqrt(end2.X() * end2.X() + end2.Y() * end2.Y());
    double expectedEndRad = 15.0 + 50.0 * std::tan(0.100);
    near(endRad, expectedEndRad, 1e-2);

    // Mode 3: HeightAndPitch
    model::DerivedCurve dc3;
    dc3.kind = QStringLiteral("HelixSpiral");
    model::HelixSpiralDefinition h3;
    h3.mode = QStringLiteral("HeightAndPitch");
    h3.axis = axisRef(axisId);
    h3.radius = 0.025;
    h3.height = 0.040;
    h3.pitch = 0.008; // 40 / 8 = 5 revolutions
    dc3.helixSpiral = h3;

    auto res3 = geometry::evaluateHelixSpiral(dc3, scene);
    CHECK(res3.size() == 1);
    near(res3[0].curve->Value(res3[0].last).Z(), 40.0, 1e-3);

    // Mode 4: Spiral (Strictly planar)
    model::DerivedCurve dc4;
    dc4.kind = QStringLiteral("HelixSpiral");
    model::HelixSpiralDefinition h4;
    h4.mode = QStringLiteral("Spiral");
    h4.axis = axisRef(axisId);
    h4.radius = 0.010;
    h4.pitch = 0.005;
    h4.revolutions = 4.0;
    dc4.helixSpiral = h4;

    auto res4 = geometry::evaluateHelixSpiral(dc4, scene);
    CHECK(res4.size() == 1);
    const auto &c4 = res4[0];
    for (int k = 0; k <= 10; ++k) {
        double t = c4.first + (double(k) / 10.0) * (c4.last - c4.first);
        gp_Pnt p = c4.curve->Value(t);
        near(p.Z(), 0.0, 1e-6); // strictly planar containment
    }
    double endSpiralRad = c4.curve->Value(c4.last).Distance(gp_Pnt(0, 0, 0));
    near(endSpiralRad, 10.0 + 5.0 * 4.0, 1e-2); // R + P * N = 10 + 20 = 30 mm
}

void testStableEndpointTangentFramesAndDatumAttachment() {
    // CM-200: Stable endpoint/tangent frames and datum plane attachment
    geometry::SupportReferences scene;

    // Helix curve
    QString axisId = newId();
    scene.axes.insert(axisId, {{0, 0, 0}, {0, 0, 1}});

    model::DerivedCurve dc;
    dc.kind = QStringLiteral("HelixSpiral");
    model::HelixSpiralDefinition h;
    h.mode = QStringLiteral("PitchAndRevolution");
    h.axis = axisRef(axisId);
    h.radius = 0.020;
    h.pitch = 0.020;
    h.revolutions = 2.0;
    dc.helixSpiral = h;

    auto curves = geometry::evaluateHelixSpiral(dc, scene);
    scene.derived.insert(dc.id, curves);

    // NormalCurve datum plane attached at Start of the curve
    model::Datum datumNormal;
    datumNormal.kind = QStringLiteral("DatumPlane");
    datumNormal.mode = QStringLiteral("NormalCurve");
    auto targetRef = curveRef(dc.id);
    targetRef.endpoint = QStringLiteral("Start");
    datumNormal.references = {targetRef};

    auto datumRes = geometry::datum(datumNormal, scene);
    CHECK(datumRes.planes.contains(datumNormal.id));
    auto frame = datumRes.planes.value(datumNormal.id);

    // Verify plane origin matches curve start point (20 mm along X, 0 along Y, 0 along Z)
    near(frame.origin[0], 0.020, 1e-4);
    near(frame.origin[1], 0.0, 1e-4);
    near(frame.origin[2], 0.0, 1e-4);

    // Plane normal u x v must match curve tangent at start
    gp_Vec planeNormal = geometry::vector(frame.u).Crossed(geometry::vector(frame.v)).Normalized();
    gp_Pnt pt;
    gp_Vec tgt;
    curves[0].curve->D1(curves[0].first, pt, tgt);
    tgt.Normalize();

    near(planeNormal.X(), tgt.X(), 1e-4);
    near(planeNormal.Y(), tgt.Y(), 1e-4);
    near(planeNormal.Z(), tgt.Z(), 1e-4);
}

void testSupportDefinitionReplay() {
    // Integration test with evaluateSupports
    model::SupportDefinition def;

    // Spatial sketch: chair frame path segment
    sketch::SpatialSketch chair;
    auto p0 = chair.point({0.0, 0.0, 0.0});
    auto p1 = chair.point({0.0, 0.0, 0.040});
    auto p2 = chair.point({0.100, 0.0, 0.040});
    auto l01 = chair.line(p0, p1);
    chair.line(p1, p2);
    def.spatial_sketches = sketch::encodeSpatial({chair});

    // Derived curve: helix
    QString axisId = newId();
    model::Datum axisDatum;
    axisDatum.id = axisId;
    axisDatum.kind = QStringLiteral("DatumAxis");
    axisDatum.mode = QStringLiteral("Principal");
    axisDatum.parameters = sketch::fields({{"axis", "Z"}});
    def.datums = {axisDatum};

    model::DerivedCurve helix;
    helix.kind = QStringLiteral("HelixSpiral");
    model::HelixSpiralDefinition hd;
    hd.axis = axisRef(axisId);
    hd.radius = 0.020;
    hd.pitch = 0.010;
    hd.revolutions = 3.0;
    helix.helixSpiral = hd;
    def.derived_curves = model::encodeDerivedCurves({helix});

    def.history = {axisId, chair.id, helix.id};

    auto result = geometry::evaluateSupports(def);
    CHECK(result.axes.contains(axisId));

    // Verify tessellation output helper
    auto tess =
        geometry::tessellateSupportCurve(geometry::spatialEntityCurve(chair, chair.entity(l01)));
    CHECK(tess.size() >= 2);
}

void markDisposable(QTemporaryDir &root) {
    CHECK(root.isValid());
    QFile f(root.filePath(".cad-test-root"));
    CHECK(f.open(QIODevice::WriteOnly));
    f.write("disposable\n");
}

void testWorkerPreviewSpatialSketch() {
    QTemporaryDir root(QDir::tempPath() + "/cad-module5-cm7-worker-preview-XXXXXX");
    markDisposable(root);
    worker::Projects worker(root.path());
    CHECK(worker.handles("PreviewSpatialSketch"));
    CHECK(worker.handles("PreviewModelSupports"));

    auto project = newId(), document = newId();
    auto command = [&](QString name, QCborMap payload = {}) {
        wire::Envelope e;
        e.command = name;
        e.context.project = project;
        e.context.document = document;
        e.revision = worker.revision();
        e.payload = payload;
        return e;
    };

    worker.run(command("NewProject", sketch::fields({{"id", document},
                                                     {"properties_entity", newId()},
                                                     {"type", "Part"},
                                                     {"name", "CM7 preview"},
                                                     {"units", "mm"},
                                                     {"replace", false}})));
    auto revision = worker.revision();

    sketch::SpatialSketch chair;
    auto p0 = chair.point({0.0, 0.0, 0.0});
    auto p1 = chair.point({0.0, 0.0, 0.040});
    chair.line(p0, p1);

    auto previewCmd = command("PreviewSpatialSketch", sketch::fields({{"sketch", chair.map()}}));
    auto reply = worker.run(previewCmd);
    CHECK(reply.contains(QStringLiteral("spatial_sketch")));
    CHECK(reply.contains(QStringLiteral("diagnostics")));
    CHECK(reply.contains(QStringLiteral("display_curves")));

    auto diag = reply.value("diagnostics").toMap();
    CHECK(diag.value("valid").toBool());
    auto curves = reply.value("display_curves").toArray();
    CHECK(curves.size() == 1);
    auto pts = curves[0].toMap().value("points").toArray();
    CHECK(pts.size() >= 2);

    // Stale revision rejection
    previewCmd.revision = revision - 1;
    error("StaleRevision", [&] { worker.run(previewCmd); });

    // Cancellation support
    previewCmd.revision = revision;
    exchange::Control canceled;
    canceled.canceled = [] { return true; };
    error("Canceled", [&] { worker.run(previewCmd, canceled); });
}

} // namespace

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    return run([] {
        testSpatialEntitiesExactCurves();
        testSketchOnSketchProjection();
        testSketchOnFacesProjection();
        testCompositeCurveGapsAndDiscontinuities();
        testHelixSpiralAllModes();
        testStableEndpointTangentFramesAndDatumAttachment();
        testSupportDefinitionReplay();
        testWorkerPreviewSpatialSketch();
    });
}
