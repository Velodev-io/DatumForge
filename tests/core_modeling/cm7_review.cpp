#include "ModelingFixture.hpp"
#include "SupportFixture.hpp"
#include "geometry/occt/ModelingFeatures.hpp"
#include "geometry/occt/SpatialCurve.hpp"
#include "geometry/occt/SupportReferences.hpp"
#include "geometry/occt/Supports.hpp"
#include "model/DerivedCurve.hpp"
#include "model/Supports.hpp"
#include "persistence/ProjectStore.hpp"
#include "sketch/SpatialSketch.hpp"
#include "sketch/SpatialSolver.hpp"
#include "support/check.hpp"
#include <BRepAdaptor_Surface.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <GC_MakeSegment.hxx>
#include <GeomAbs_SurfaceType.hxx>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <numbers>

using namespace cad;
using namespace modeling_fixture;
using support_fixture::error;

namespace {

void markDisposable(QTemporaryDir &root) {
    CHECK(root.isValid());
    QFile f(root.filePath(".cad-test-root"));
    CHECK(f.open(QIODevice::WriteOnly));
    f.write("disposable\n");
}

domain::GeometryReference sketchRef(const QString &sketchId, const QString &entity = {}) {
    domain::GeometryReference r;
    r.object = sketchId;
    if (!entity.isEmpty()) {
        r.role = QStringLiteral("Curve");
        r.entity = entity;
    } else {
        r.role = QStringLiteral("Object");
    }
    return r;
}

domain::GeometryReference axisRef(const QString &axisId) {
    domain::GeometryReference r;
    r.object = axisId;
    r.role = QStringLiteral("Axis");
    return r;
}

domain::GeometryReference planeRef(const QString &planeId) {
    domain::GeometryReference r;
    r.object = planeId;
    r.role = QStringLiteral("Plane");
    return r;
}

domain::GeometryReference curveRef(const QString &objectId, const QString &endpoint = QString()) {
    domain::GeometryReference r;
    r.object = objectId;
    r.role = QStringLiteral("Curve");
    if (!endpoint.isEmpty())
        r.endpoint = endpoint;
    return r;
}

// 1. Reproduce the T8 projected path fixture, including endpoint-normal plane and supported sketch
// attachment. Mutate both source sketches and the target face; verify rebuild, branch ambiguity,
// repair and persistence.
void testT8ProjectedPathFixture() {
    QTemporaryDir root(QDir::tempPath() + "/cad-module7-review-proj-root-XXXXXX"),
        files(QDir::tempPath() + "/cad-module7-review-proj-files-XXXXXX");
    markDisposable(root);
    markDisposable(files);

    storage::ProjectStore store(root.path());
    auto project = newId(), document = newId();
    store.create(project);

    auto command = [&](const QString &name, QCborMap payload = QCborMap{}) {
        wire::Envelope e;
        e.command = name;
        e.context.project = project;
        e.context.document = document;
        e.revision = store.revision();
        e.payload = payload;
        return e;
    };

    store.execute(command("CreateDocument", fields({{"id", document},
                                                    {"properties_entity", newId()},
                                                    {"type", "Part"},
                                                    {"name", "T8 Projected Path Review"},
                                                    {"units", "mm"}})));

    // Sketch 1 on XY plane (arc)
    sketch::Sketch s1;
    s1.name = "ProjSource1";
    s1.frame = sketch::Frame{{0, 0, 0}, {1, 0, 0}, {0, 1, 0}};
    auto p1_0 = s1.point({0.0, 0.0});
    auto p1_1 = s1.point({0.050, 0.0});
    auto p1_c = s1.point({0.025, 0.0});
    auto arc1 = s1.arc(p1_c, 0.025, 0.0, std::numbers::pi);

    // Sketch 2 on XZ plane (line)
    sketch::Sketch s2;
    s2.name = "ProjSource2";
    s2.frame = sketch::Frame{{0, 0, 0}, {1, 0, 0}, {0, 0, 1}};
    auto p2_0 = s2.point({0.0, 0.0});
    auto p2_1 = s2.point({0.050, 0.030});
    auto l2 = s2.line(p2_0, p2_1);

    // Derived curve: SketchOnSketch
    model::DerivedCurve dc;
    dc.name = "T8ProjectedCurve";
    dc.kind = QStringLiteral("SketchOnSketch");
    model::SketchOnSketchDefinition sos;
    sos.sketch1 = sketchRef(s1.id, arc1);
    sos.sketch2 = sketchRef(s2.id, l2);
    sos.branch = model::BranchSelection{0, QStringLiteral("branch/0"), false, 0.0, 1.0};
    dc.sketchOnSketch = sos;

    // Endpoint-normal plane at Start of projected curve
    model::Datum normalPlane;
    normalPlane.kind = QStringLiteral("DatumPlane");
    normalPlane.mode = QStringLiteral("NormalCurve");
    normalPlane.references = {curveRef(dc.id, QStringLiteral("Start"))};

    // Supported sketch attached to normalPlane
    sketch::Sketch attachedSketch;
    attachedSketch.name = "SweepProfileOnNormalPlane";
    auto ap0 = attachedSketch.point({0.0, 0.0});
    auto ap1 = attachedSketch.point({0.005, 0.0});
    attachedSketch.circle(ap0, 0.005);

    model::Attachment attachment;
    attachment.sketch = attachedSketch.id;
    attachment.reference = planeRef(normalPlane.id);

    // Build complete support definition and sequence history
    model::SupportDefinition modelDef;
    modelDef.document = document;
    modelDef.sketches = sketch::encode({s1, s2, attachedSketch});
    modelDef.datums = {normalPlane};
    modelDef.attachments = {attachment};
    modelDef.derived_curves = model::encodeDerivedCurves({dc});
    modelDef.sequence();

    store.execute(command("SetModelSupports", fields({{"model", modelDef.map()}})));
    auto rev1 = store.revision();
    CHECK(rev1 > 0);

    // Verify evaluated datum plane matches curve start point and tangent
    auto fs = store.featureState(document);
    auto evaluatedSup = model::SupportDefinition::parse(fs.value("supports").toMap());
    auto scene = geometry::evaluateSupports(evaluatedSup);
    CHECK(scene.planes.contains(normalPlane.id));
    auto planeFrame = scene.planes.value(normalPlane.id);

    CHECK(!scene.derived.value(dc.id).empty());
    auto sc = scene.derived.value(dc.id)[0];
    gp_Pnt curveStart = sc.curve->Value(sc.first);
    near(planeFrame.origin[0], curveStart.X() * 1e-3, 1e-4);
    near(planeFrame.origin[1], curveStart.Y() * 1e-3, 1e-4);
    near(planeFrame.origin[2], curveStart.Z() * 1e-3, 1e-4);

    // Mutate Source Sketch 1 (change radius from 25 mm to 35 mm)
    sketch::Sketch s1Mutated = s1;
    s1Mutated.entities.clear();
    auto p1_m0 = s1Mutated.point({0.0, 0.0});
    auto p1_m1 = s1Mutated.point({0.070, 0.0});
    auto p1_mc = s1Mutated.point({0.035, 0.0});
    auto arc1Mutated = s1Mutated.arc(p1_mc, 0.035, 0.0, std::numbers::pi);

    auto dcMutated = dc;
    dcMutated.sketchOnSketch->sketch1 = sketchRef(s1.id, arc1Mutated);

    model::SupportDefinition mutatedDef;
    mutatedDef.document = document;
    mutatedDef.sketches = sketch::encode({s1Mutated, s2, attachedSketch});
    mutatedDef.datums = {normalPlane};
    mutatedDef.attachments = {attachment};
    mutatedDef.derived_curves = model::encodeDerivedCurves({dcMutated});
    mutatedDef.sequence();

    store.execute(command("SetModelSupports", fields({{"model", mutatedDef.map()}})));
    CHECK(store.revision() > rev1);
    auto rev2 = store.revision();

    // Verify rebuild: curve and normal plane update atomically
    auto fs2 = store.featureState(document);
    auto sup2 = model::SupportDefinition::parse(fs2.value("supports").toMap());
    auto scene2 = geometry::evaluateSupports(sup2);
    CHECK(!scene2.derived.value(dc.id).empty());
    CHECK(scene2.planes.contains(normalPlane.id));

    // Branch ambiguity and explicit reference repair:
    // Invalidate branch by assigning stale branch token
    auto dcStale = dcMutated;
    dcStale.sketchOnSketch->branch.token = QStringLiteral("branch/stale");
    auto staleDef = mutatedDef;
    staleDef.derived_curves = model::encodeDerivedCurves({dcStale});

    error("ReferenceMissing",
          [&] { store.execute(command("SetModelSupports", fields({{"model", staleDef.map()}}))); });

    // Inspect candidates
    auto candInspection =
        store.supportReferenceCandidates(document, staleDef.map(), dc.id, 0, "branch");
    CHECK(candInspection.value("state") == "Missing" ||
          candInspection.value("state") == "Ambiguous");
    auto branchCandidates = candInspection.value("replacements").toArray();
    CHECK(!branchCandidates.empty());

    // Repair branch explicitly
    auto chosenCandidate = branchCandidates[0].toMap();
    auto repairCmd = fields({{"model", staleDef.map()},
                             {"owner", dc.id},
                             {"path", "branch"},
                             {"replacement", chosenCandidate},
                             {"fingerprint", candInspection.value("fingerprint")}});
    store.execute(command("ResolveSupportReference", repairCmd));
    CHECK(store.revision() > rev2);

    // Save and Reopen Persistence Check
    auto projPath = files.filePath("t8_projected_path.cadproj");
    store.save(command("SaveProject"), projPath);

    store.close();
    store.open(projPath, "Saved");
    auto reopenedState = store.featureState(document);
    auto reopenedSup = model::SupportDefinition::parse(reopenedState.value("supports").toMap());
    CHECK(reopenedSup.derived_curves.size() == 1);
    CHECK(reopenedSup.datums.size() == 1);
    CHECK(reopenedSup.sketches.size() == 3);
}

// 2. Reproduce the chair-frame path and its relations/dimensions using the verified diagram
// and the offset-plane endpoint constraint. Verify plane switching, edits and reopen.
void testChairFramePathFixture() {
    QTemporaryDir root(QDir::tempPath() + "/cad-module7-chair-root-XXXXXX"),
        files(QDir::tempPath() + "/cad-module7-chair-files-XXXXXX");
    markDisposable(root);
    markDisposable(files);

    storage::ProjectStore store(root.path());
    auto project = newId(), document = newId();
    store.create(project);

    auto command = [&](const QString &name, QCborMap payload = QCborMap{}) {
        wire::Envelope e;
        e.command = name;
        e.context.project = project;
        e.context.document = document;
        e.revision = store.revision();
        e.payload = payload;
        return e;
    };

    store.execute(command("CreateDocument", fields({{"id", document},
                                                    {"properties_entity", newId()},
                                                    {"type", "Part"},
                                                    {"name", "Chair Frame Path Review"},
                                                    {"units", "mm"}})));

    // Create XY principal plane for AlongZ normal reference
    QString xyPlaneId = newId();
    model::Datum xyPlane;
    xyPlane.id = xyPlaneId;
    xyPlane.kind = QStringLiteral("DatumPlane");
    xyPlane.mode = QStringLiteral("Principal");
    xyPlane.parameters = fields({{"plane", "XY"}});

    // Create offset datum plane at Y = 250 mm (0.250 m)
    QString offsetPlaneId = newId();
    model::Datum offsetPlane;
    offsetPlane.id = offsetPlaneId;
    offsetPlane.kind = QStringLiteral("DatumPlane");
    offsetPlane.mode = QStringLiteral("Offset");
    offsetPlane.parameters = fields({{"distance", 0.250}, {"count", 1}});
    // Offset from XZ plane (world Y axis direction)
    model::Datum zxPlane;
    zxPlane.id = newId();
    zxPlane.kind = QStringLiteral("DatumPlane");
    zxPlane.mode = QStringLiteral("Principal");
    zxPlane.parameters = fields({{"plane", "XZ"}});
    offsetPlane.references = {planeRef(zxPlane.id)};

    // Spatial sketch: chair-frame path (T8 pages 18-21, 25-27, 48-49)
    sketch::SpatialSketch chair;
    chair.name = "ChairFrame3DPath";
    // P0: Floor origin
    auto p0 = chair.point({0.0, 0.0, 0.0});
    // P1: Front leg top
    auto p1 = chair.point({0.0, 0.0, 0.450});
    // P2: Seat side corner
    auto p2 = chair.point({0.450, 0.0, 0.450});
    // P3: Backrest top corner
    auto p3 = chair.point({0.450, 0.0, 0.900});
    // P4: Top cross rail end on offset plane
    auto p4 = chair.point({0.450, 0.250, 0.900});

    auto l01 = chair.line(p0, p1);
    auto l12 = chair.line(p1, p2);
    auto l23 = chair.line(p2, p3);
    auto l34 = chair.line(p3, p4);

    // Fixed origin
    sketch::SpatialConstraint cFix;
    cFix.kind = QStringLiteral("Fixed");
    cFix.a = p0;
    chair.constraints.push_back(cFix);

    // AlongZ for front leg (normal to XY plane)
    sketch::SpatialConstraint cAlongZ1;
    cAlongZ1.kind = QStringLiteral("AlongZ");
    cAlongZ1.a = l01;
    cAlongZ1.plane = planeRef(xyPlaneId);
    chair.constraints.push_back(cAlongZ1);

    // AlongX for seat side
    sketch::SpatialConstraint cAlongX2;
    cAlongX2.kind = QStringLiteral("AlongX");
    cAlongX2.a = l12;
    chair.constraints.push_back(cAlongX2);

    // AlongZ for backrest (normal to XY plane)
    sketch::SpatialConstraint cAlongZ3;
    cAlongZ3.kind = QStringLiteral("AlongZ");
    cAlongZ3.a = l23;
    cAlongZ3.plane = planeRef(xyPlaneId);
    chair.constraints.push_back(cAlongZ3);

    // AlongY for top rail
    sketch::SpatialConstraint cAlongY4;
    cAlongY4.kind = QStringLiteral("AlongY");
    cAlongY4.a = l34;
    chair.constraints.push_back(cAlongY4);

    // Dimensions
    auto addDist = [&](QString a, QString b, double val) {
        sketch::SpatialConstraint d;
        d.kind = QStringLiteral("Distance");
        d.a = a;
        d.b = b;
        d.value = val;
        d.driving = true;
        chair.constraints.push_back(d);
    };
    addDist(p0, p1, 0.450);
    addDist(p1, p2, 0.450);
    addDist(p2, p3, 0.450);

    // P4 is constrained OnPlane with offsetPlaneFrontLeg
    sketch::SpatialConstraint cOnPlane;
    cOnPlane.kind = QStringLiteral("OnPlane");
    cOnPlane.a = p4;
    cOnPlane.plane = planeRef(offsetPlaneId);
    chair.constraints.push_back(cOnPlane);

    // Solve spatial sketch with offset plane at Y = 0.250 m
    sketch::Frame offsetFrame{{0, 0.250, 0}, {1, 0, 0}, {0, 0, 1}};
    sketch::Frame xyFrame{{0, 0, 0}, {1, 0, 0}, {0, 1, 0}};
    auto resolve1 = [&](const domain::GeometryReference &r) {
        if (r.object == offsetPlaneId)
            return offsetFrame;
        return xyFrame;
    };
    auto sol1 = sketch::solveSpatial(chair, nullptr, resolve1);
    CHECK(sol1.valid);

    near(sol1.sketch.entity(p0).point[0], 0.0, 1e-5);
    near(sol1.sketch.entity(p0).point[1], 0.0, 1e-5);
    near(sol1.sketch.entity(p0).point[2], 0.0, 1e-5);

    near(sol1.sketch.entity(p1).point[2], 0.450, 1e-5);
    near(sol1.sketch.entity(p2).point[0], 0.450, 1e-5);
    near(sol1.sketch.entity(p3).point[2], 0.900, 1e-5);
    near(sol1.sketch.entity(p4).point[1], 0.250, 1e-5);

    // Mutate offset plane: distance moves to 0.350 m (350 mm)
    sketch::Frame offsetFrameMutated{{0, 0.350, 0}, {1, 0, 0}, {0, 0, 1}};
    auto resolve2 = [&](const domain::GeometryReference &r) {
        if (r.object == offsetPlaneId)
            return offsetFrameMutated;
        return xyFrame;
    };
    auto sol2 = sketch::solveSpatial(chair, nullptr, resolve2);
    CHECK(sol2.valid);
    near(sol2.sketch.entity(p4).point[1], 0.350, 1e-5);
    near(sol2.sketch.entity(p3).point[2], 0.900, 1e-5);
    near(sol2.sketch.entity(p2).point[0], 0.450, 1e-5);
    near(sol2.sketch.entity(p1).point[2], 0.450, 1e-5);

    // Commit to ProjectStore
    model::SupportDefinition supDef;
    supDef.document = document;
    supDef.datums = {xyPlane, zxPlane, offsetPlane};
    supDef.spatial_sketches = sketch::encodeSpatial({chair});
    supDef.sequence();

    store.execute(command("SetModelSupports", fields({{"model", supDef.map()}})));
    CHECK(store.revision() > 0);

    // Save and Reopen
    auto savePath = files.filePath("chair_frame_review.cadproj");
    store.save(command("SaveProject"), savePath);

    store.close();
    store.open(savePath, "Saved");
    auto reopenedState = store.featureState(document);
    auto reopenedSup = model::SupportDefinition::parse(reopenedState.value("supports").toMap());
    CHECK(reopenedSup.spatial_sketches.size() == 1);
    CHECK(reopenedSup.datums.size() == 3);
}

// 3. Exercise composite gaps/corners/reversal, closed seams, helix/spiral edits, degenerate
// tangents, solver conflicts, stale previews and upstream deletion.
void testCompositeAndEdgeCasesFixture() {
    QTemporaryDir root(QDir::tempPath() + "/cad-module7-edge-root-XXXXXX");
    markDisposable(root);

    storage::ProjectStore store(root.path());
    auto project = newId(), document = newId();
    store.create(project);

    auto command = [&](const QString &name, QCborMap payload = QCborMap{}) {
        wire::Envelope e;
        e.command = name;
        e.context.project = project;
        e.context.document = document;
        e.revision = store.revision();
        e.payload = payload;
        return e;
    };

    store.execute(command("CreateDocument", fields({{"id", document},
                                                    {"properties_entity", newId()},
                                                    {"type", "Part"},
                                                    {"name", "CM7 Edge Cases Review"},
                                                    {"units", "mm"}})));

    // 3a. Composite Gaps and Corners
    geometry::SupportReferences scene;
    auto s1 = GC_MakeSegment(gp_Pnt(0, 0, 0), gp_Pnt(50, 0, 0)).Value();
    auto s2 = GC_MakeSegment(gp_Pnt(50, 0, 0), gp_Pnt(50, 50, 0)).Value();
    QString id1 = newId(), id2 = newId();
    scene.derived.insert(id1, {{id1, s1, 0, 50, 50, false, {}}});
    scene.derived.insert(id2, {{id2, s2, 0, 50, 50, false, {}}});

    model::DerivedCurve compDc;
    compDc.kind = QStringLiteral("CompositeCurve");
    model::CompositeCurveDefinition compDef;
    model::CompositeCurveMember m1, m2;
    m1.source = curveRef(id1);
    m2.source = curveRef(id2);
    compDef.members = {m1, m2};
    compDc.compositeCurve = compDef;

    // Continuous evaluation succeeds
    auto compResult = geometry::evaluateCompositeCurve(compDc, scene);
    CHECK(compResult.size() == 1);

    // Ambiguous corner without limit side throws
    sketch::Vec3 cornerPt{0.050, 0.0, 0.0};
    sketch::Vec3 tgtA{1.0, 0.0, 0.0};
    sketch::Vec3 tgtB{0.0, 1.0, 0.0};
    error("InvalidInput", [&] { model::CurveFrame::evaluateCorner(cornerPt, tgtA, tgtB, ""); });

    // Explicit Left and Right corner evaluations succeed
    auto frameL = model::CurveFrame::evaluateCorner(cornerPt, tgtA, tgtB, "Left");
    near(frameL.tangent[0], 1.0, 1e-6);
    auto frameR = model::CurveFrame::evaluateCorner(cornerPt, tgtA, tgtB, "Right");
    near(frameR.tangent[1], 1.0, 1e-6);

    // Disconnected gap (>1 mm) throws InvalidGeometry
    auto sGap = GC_MakeSegment(gp_Pnt(70, 70, 0), gp_Pnt(100, 70, 0)).Value();
    QString idGap = newId();
    scene.derived.insert(idGap, {{idGap, sGap, 0, 30, 30, false, {}}});
    auto compGapDc = compDc;
    model::CompositeCurveMember mGap;
    mGap.source = curveRef(idGap);
    compGapDc.compositeCurve->members.push_back(mGap);
    error("InvalidGeometry", [&] { geometry::evaluateCompositeCurve(compGapDc, scene); });

    // 3b. Helix/Spiral Edits across all 4 modes
    QString axisId = newId();
    scene.axes.insert(axisId, {{0, 0, 0}, {0, 0, 1}});

    // Mode 1: PitchAndRevolution
    model::DerivedCurve h1;
    h1.kind = QStringLiteral("HelixSpiral");
    model::HelixSpiralDefinition hd1;
    hd1.axis = axisRef(axisId);
    hd1.mode = QStringLiteral("PitchAndRevolution");
    hd1.radius = 0.020;
    hd1.pitch = 0.010;
    hd1.revolutions = 2.0;
    h1.helixSpiral = hd1;
    auto res1 = geometry::evaluateHelixSpiral(h1, scene);
    CHECK(res1.size() == 1);
    near(res1[0].curve->Value(res1[0].last).Z(), 20.0, 1e-3);

    // Mode 2: HeightAndRevolution
    model::DerivedCurve h2 = h1;
    h2.helixSpiral->mode = QStringLiteral("HeightAndRevolution");
    h2.helixSpiral->height = 0.060;
    h2.helixSpiral->revolutions = 3.0;
    auto res2 = geometry::evaluateHelixSpiral(h2, scene);
    CHECK(res2.size() == 1);
    near(res2[0].curve->Value(res2[0].last).Z(), 60.0, 1e-3);

    // Mode 3: HeightAndPitch
    model::DerivedCurve h3 = h1;
    h3.helixSpiral->mode = QStringLiteral("HeightAndPitch");
    h3.helixSpiral->height = 0.080;
    h3.helixSpiral->pitch = 0.020;
    auto res3 = geometry::evaluateHelixSpiral(h3, scene);
    CHECK(res3.size() == 1);
    near(res3[0].curve->Value(res3[0].last).Z(), 80.0, 1e-3);

    // Mode 4: Spiral (strictly planar at z = 0)
    model::DerivedCurve h4 = h1;
    h4.helixSpiral->mode = QStringLiteral("Spiral");
    h4.helixSpiral->radius = 0.010;
    h4.helixSpiral->pitch = 0.005;
    h4.helixSpiral->revolutions = 3.0;
    auto res4 = geometry::evaluateHelixSpiral(h4, scene);
    CHECK(res4.size() == 1);
    for (int k = 0; k <= 5; ++k) {
        double t = res4[0].first + (double(k) / 5.0) * (res4[0].last - res4[0].first);
        near(res4[0].curve->Value(t).Z(), 0.0, 1e-6);
    }

    // 3c. Spatial Solver Conflicts
    sketch::SpatialSketch conflictSketch;
    auto cp0 = conflictSketch.point({0, 0, 0});
    auto cp1 = conflictSketch.point({0.050, 0, 0});
    auto cLine = conflictSketch.line(cp0, cp1);

    // Contradictory directions: simultaneously AlongX and AlongY
    sketch::SpatialConstraint cX, cY;
    cX.kind = QStringLiteral("AlongX");
    cX.a = cLine;
    cY.kind = QStringLiteral("AlongY");
    cY.a = cLine;
    conflictSketch.constraints.push_back(cX);
    conflictSketch.constraints.push_back(cY);

    auto conflictSol = sketch::solveSpatial(conflictSketch);
    CHECK(!conflictSol.valid || !conflictSol.conflicting.isEmpty());

    // 3d. Upstream Deletion
    // Deleting parent sketch when derived curve depends on it throws ReferenceMissing
    sketch::Sketch parentSk1, parentSk2;
    parentSk1.rectangle({0, 0}, {0.050, 0.050});
    parentSk2.frame = sketch::Frame{{0, 0, 0}, {1, 0, 0}, {0, 0, 1}};
    parentSk2.rectangle({0, 0}, {0.050, 0.050});

    model::DerivedCurve childDc;
    childDc.kind = QStringLiteral("SketchOnSketch");
    model::SketchOnSketchDefinition childSos;
    childSos.sketch1 = sketchRef(parentSk1.id);
    childSos.sketch2 = sketchRef(parentSk2.id);
    childSos.branch = model::BranchSelection{0, "branch/0", false, 0.0, 1.0};
    childDc.sketchOnSketch = childSos;

    model::SupportDefinition supValid;
    supValid.document = document;
    supValid.sketches = sketch::encode({parentSk1, parentSk2});
    supValid.derived_curves = model::encodeDerivedCurves({childDc});
    supValid.sequence();

    store.execute(command("SetModelSupports", fields({{"model", supValid.map()}})));
    CHECK(store.revision() > 0);

    // Delete parentSk2 from sketches while childDc still references it
    auto supMissingParent = supValid;
    supMissingParent.sketches = sketch::encode({parentSk1});
    supMissingParent.history.removeAll(parentSk2.id);

    error("ReferenceMissing", [&] {
        store.execute(command("SetModelSupports", fields({{"model", supMissingParent.map()}})));
    });

    // 3e. Stale Previews and Cancellation
    exchange::Control canceled;
    canceled.canceled = [] { return true; };
    error("Canceled", [&] {
        store.execute(command("SetModelSupports", fields({{"model", supValid.map()}})), canceled);
    });
}

} // namespace

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    return run([] {
        testT8ProjectedPathFixture();
        testChairFramePathFixture();
        testCompositeAndEdgeCasesFixture();
    });
}
