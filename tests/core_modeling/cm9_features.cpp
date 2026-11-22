#include "ModelingFixture.hpp"
#include "features/Catalogs.hpp"
#include "geometry/occt/ModelingFeatures.hpp"
#include "geometry/occt/Part.hpp"
#include "render/Viewport.hpp"
#include <BRepAdaptor_Surface.hxx>
#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <QApplication>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <cmath>
#include <numbers>

using namespace cad;
using namespace cad::geometry;
using namespace modeling_fixture;
using sketch::fields;

namespace {

QCborMap makeExtrude(QString sketch, double depth = 0.05) {
    auto f = modeling_fixture::extrude(sketch, "NewBody");
    f.insert(QStringLiteral("direction1"), blind(depth));
    return f;
}

double shapeVol(const TopoDS_Shape &s) {
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

domain::GeometryReference findCylindricalFace(const TopoDS_Shape &shape, QString object) {
    domain::GeometryReference r = support_fixture::ref(object, "Face");
    for (TopExp_Explorer ex(shape, TopAbs_FACE); ex.More(); ex.Next()) {
        BRepAdaptor_Surface surf(TopoDS::Face(ex.Current()));
        if (surf.GetType() == GeomAbs_Cylinder) {
            r.topology = supportToken(ex.Current());
            return r;
        }
    }
    throw std::runtime_error("No cylindrical face found");
}

// ============================================================================
// 1. Hole Catalog Resolution and Overrides (CM6 Hole Adapter Integration)
// ============================================================================

void testHoleCatalogResolutionAndOverrides() {
    sketch::Sketch s;
    s.rectangle({0, 0}, {0.050, 0.050});

    sketch::Sketch plSk;
    plSk.name = "HolePositions";
    auto pt1 = plSk.point({0.025, 0.025});

    auto d = base({s, plSk});
    auto baseExt = makeExtrude(s.id, 0.020);
    auto baseId = baseExt.value("id").toString();

    auto baseRes = evaluateModeling(d, {baseExt});
    CHECK(baseRes.part.bodies.size() == 1);
    double initialVol = shapeVol(baseRes.part.bodies[0].shape);
    near(initialVol, 0.050 * 0.050 * 0.020, 1e-9);

    auto topFace = findFace(baseRes.part.bodies[0].shape, baseId, 19.0, 21.0);

    // 1.1 ANSI Metric Button Head Cap Screw (M8, Normal fit) -> Through All
    {
        auto holeFeature = fields(
            {{"version", 1},
             {"id", newId()},
             {"kind", "Hole"},
             {"operation", "Cut"},
             {"target", body(baseId)},
             {"family", "Simple"},
             {"placement",
              fields({{"face", topFace.map()}, {"sketch", plSk.id}, {"points", QCborArray{pt1}}})},
             {"sizing", fields({{"mode", "Catalog"},
                                {"dataset", "ansi_metric"},
                                {"standard", "B18.3.4M"},
                                {"type", "SocketButtonHeadCapScrew"},
                                {"size", "M8"},
                                {"fit", "Normal"},
                                {"depth", 0.020}})},
             {"termination", fields({{"mode", "ThroughAll"}, {"depth_reference", "Shoulder"}})}});

        auto res = evaluateModeling(d, {baseExt, holeFeature});
        CHECK(res.part.bodies.size() == 1);
        double cutVol = shapeVol(res.part.bodies[0].shape);
        // Resolved diameter for M8 Normal is 9.0 mm (0.009 m)
        double expectedHoleVol = std::numbers::pi * (0.009 / 2.0) * (0.009 / 2.0) * 0.020;
        near(initialVol - cutVol, expectedHoleVol, 1e-9);
    }

    // 1.2 T7 p. 29 Custom Override Fixture (M8 counterbore with 8.5 / 14 / 4.2 mm)
    {
        auto overrideHole = fields(
            {{"version", 1},
             {"id", newId()},
             {"kind", "Hole"},
             {"operation", "Cut"},
             {"target", body(baseId)},
             {"family", "Counterbore"},
             {"placement",
              fields({{"face", topFace.map()}, {"sketch", plSk.id}, {"points", QCborArray{pt1}}})},
             {"sizing", fields({{"mode", "Catalog"},
                                {"dataset", "ansi_metric"},
                                {"standard", "B18.3.1M"},
                                {"type", "SocketHeadCapScrew"},
                                {"size", "M8"},
                                {"fit", "Normal"},
                                {"depth", 0.020},
                                {"custom_overrides", fields({{"diameter", 0.0085},
                                                             {"counterbore_diameter", 0.014},
                                                             {"counterbore_depth", 0.0042}})}})},
             {"termination", fields({{"mode", "ThroughAll"}, {"depth_reference", "Shoulder"}})}});

        auto res = evaluateModeling(d, {baseExt, overrideHole});
        CHECK(res.part.bodies.size() == 1);
        double cutVol = shapeVol(res.part.bodies[0].shape);
        double expectedLoss = std::numbers::pi * (0.014 / 2.0) * (0.014 / 2.0) * 0.0042 +
                              std::numbers::pi * (0.0085 / 2.0) * (0.0085 / 2.0) * (0.020 - 0.0042);
        near(initialVol - cutVol, expectedLoss, 1e-9);
    }

    // 1.3 Unsupported catalog query throws UnsupportedOperation
    {
        auto badHole = fields(
            {{"version", 1},
             {"id", newId()},
             {"kind", "Hole"},
             {"operation", "Cut"},
             {"target", body(baseId)},
             {"family", "Simple"},
             {"placement",
              fields({{"face", topFace.map()}, {"sketch", plSk.id}, {"points", QCborArray{pt1}}})},
             {"sizing", fields({{"mode", "Catalog"},
                                {"dataset", "ansi_metric"},
                                {"standard", "B18.3.4M"},
                                {"type", "SocketButtonHeadCapScrew"},
                                {"size", "M999"},
                                {"fit", "Normal"},
                                {"depth", 0.020}})},
             {"termination", fields({{"mode", "ThroughAll"}, {"depth_reference", "Shoulder"}})}});

        support_fixture::error("UnsupportedOperation",
                               [&]() { evaluateModeling(d, {baseExt, badHole}); });
    }
}

// ============================================================================
// 2. Exact Mass Properties Evaluation
// ============================================================================

void testMassPropertiesEvaluation() {
    // Cuboid: 100 x 50 x 20 mm (0.1 x 0.05 x 0.02 m)
    double a = 0.100, b = 0.050, c = 0.020;
    sketch::Sketch s;
    s.rectangle({0, 0}, {a, b});

    auto d = base({s});
    auto boxExt = makeExtrude(s.id, c);

    auto res = evaluateModeling(d, {boxExt});
    const auto &part = res.part;

    // 2.1 CenterOfMass Frame evaluation with Density
    double density = 7850.0;                             // kg/m^3
    double expectedVol = a * b * c;                      // 0.0001 m^3
    double expectedArea = 2.0 * (a * b + b * c + c * a); // 0.016 m^2
    double expectedMass = expectedVol * density;         // 0.785 kg
    double expectedIxx_com = (1.0 / 12.0) * expectedMass * (b * b + c * c);
    double expectedIyy_com = (1.0 / 12.0) * expectedMass * (a * a + c * c);
    double expectedIzz_com = (1.0 / 12.0) * expectedMass * (a * a + b * b);

    features::MassPropertiesRequest reqCom;
    reqCom.revision = 1;
    reqCom.scopeType = "Part";
    reqCom.frame = "CenterOfMass";
    reqCom.overrideDensity = density;

    auto evalCom = part.evaluateMassProperties(reqCom);
    CHECK(evalCom.valid);
    CHECK(evalCom.solids_count == 1);
    CHECK(!evalCom.density_missing);
    CHECK(evalCom.mass_kg.has_value());
    near(evalCom.volume_m3, expectedVol, 1e-9);
    near(evalCom.surface_area_m2, expectedArea, 1e-8);
    near(*evalCom.mass_kg, expectedMass, 1e-8);

    near(evalCom.center_of_mass_m[0], a / 2.0, 1e-7);
    near(evalCom.center_of_mass_m[1], b / 2.0, 1e-7);
    near(evalCom.center_of_mass_m[2], c / 2.0, 1e-7);

    near(evalCom.inertia_tensor_kg_m2[0], expectedIxx_com, 1e-9);
    near(evalCom.inertia_tensor_kg_m2[1], expectedIyy_com, 1e-9);
    near(evalCom.inertia_tensor_kg_m2[2], expectedIzz_com, 1e-9);
    near(evalCom.inertia_tensor_kg_m2[3], 0.0, 1e-9);
    near(evalCom.inertia_tensor_kg_m2[4], 0.0, 1e-9);
    near(evalCom.inertia_tensor_kg_m2[5], 0.0, 1e-9);

    // Principal moments are sorted: Ixx_com < Iyy_com < Izz_com
    near(evalCom.principal_moments_kg_m2[0], expectedIxx_com, 1e-9);
    near(evalCom.principal_moments_kg_m2[1], expectedIyy_com, 1e-9);
    near(evalCom.principal_moments_kg_m2[2], expectedIzz_com, 1e-9);

    // Principal axes form an orthonormal basis with det = +1
    const auto &ax = evalCom.principal_axes;
    for (int i = 0; i < 3; ++i) {
        double lenSq = ax[i][0] * ax[i][0] + ax[i][1] * ax[i][1] + ax[i][2] * ax[i][2];
        near(lenSq, 1.0, 1e-6);
    }
    double dot01 = ax[0][0] * ax[1][0] + ax[0][1] * ax[1][1] + ax[0][2] * ax[1][2];
    double dot02 = ax[0][0] * ax[2][0] + ax[0][1] * ax[2][1] + ax[0][2] * ax[2][2];
    double dot12 = ax[1][0] * ax[2][0] + ax[1][1] * ax[2][1] + ax[1][2] * ax[2][2];
    near(dot01, 0.0, 1e-6);
    near(dot02, 0.0, 1e-6);
    near(dot12, 0.0, 1e-6);

    // 2.2 Part Frame evaluation (Parallel Axis Theorem)
    features::MassPropertiesRequest reqPart;
    reqPart.revision = 1;
    reqPart.scopeType = "Part";
    reqPart.frame = "Part";
    reqPart.overrideDensity = density;

    auto evalPart = part.evaluateMassProperties(reqPart);
    CHECK(evalPart.valid);
    double xc = a / 2.0, yc = b / 2.0, zc = c / 2.0;
    double expectedIxx_part = expectedIxx_com + expectedMass * (yc * yc + zc * zc);
    double expectedIyy_part = expectedIyy_com + expectedMass * (xc * xc + zc * zc);
    double expectedIzz_part = expectedIzz_com + expectedMass * (xc * xc + yc * yc);
    near(evalPart.inertia_tensor_kg_m2[0], expectedIxx_part, 1e-9);
    near(evalPart.inertia_tensor_kg_m2[1], expectedIyy_part, 1e-9);
    near(evalPart.inertia_tensor_kg_m2[2], expectedIzz_part, 1e-9);

    // 2.3 Missing density handling
    features::MassPropertiesRequest reqNoDensity;
    reqNoDensity.revision = 1;
    reqNoDensity.scopeType = "Part";
    reqNoDensity.frame = "CenterOfMass";
    reqNoDensity.overrideDensity = std::nullopt;

    auto evalNoDensity = part.evaluateMassProperties(reqNoDensity);
    CHECK(evalNoDensity.valid);
    CHECK(evalNoDensity.density_missing);
    CHECK(!evalNoDensity.mass_kg.has_value());
    CHECK(!evalNoDensity.density_kg_m3.has_value());
    near(evalNoDensity.volume_m3, expectedVol, 1e-9);
    near(evalNoDensity.surface_area_m2, expectedArea, 1e-8);
    near(evalNoDensity.center_of_mass_m[0], a / 2.0, 1e-7);

    // 2.4 Repeated eigenvalues / Isotropic symmetry (Cube: a = b = c)
    {
        double sCube = 0.050;
        sketch::Sketch skCube;
        skCube.rectangle({0, 0}, {sCube, sCube});

        auto dCube = base({skCube});
        auto cubeExt = makeExtrude(skCube.id, sCube);
        auto cubeRes = evaluateModeling(dCube, {cubeExt});

        features::MassPropertiesRequest cubeReq;
        cubeReq.revision = 1;
        cubeReq.scopeType = "Part";
        cubeReq.frame = "CenterOfMass";
        cubeReq.overrideDensity = density;

        auto cubeEval = cubeRes.part.evaluateMassProperties(cubeReq);
        CHECK(cubeEval.valid);
        double mCube = sCube * sCube * sCube * density;
        double expectedI = (1.0 / 6.0) * mCube * sCube * sCube;
        near(cubeEval.principal_moments_kg_m2[0], expectedI, 1e-9);
        near(cubeEval.principal_moments_kg_m2[1], expectedI, 1e-9);
        near(cubeEval.principal_moments_kg_m2[2], expectedI, 1e-9);
        for (int i = 0; i < 3; ++i) {
            double lenSq = cubeEval.principal_axes[i][0] * cubeEval.principal_axes[i][0] +
                           cubeEval.principal_axes[i][1] * cubeEval.principal_axes[i][1] +
                           cubeEval.principal_axes[i][2] * cubeEval.principal_axes[i][2];
            near(lenSq, 1.0, 1e-6);
        }
    }
}

// ============================================================================
// 3. Exact Planar Section Properties Evaluation
// ============================================================================

void testSectionPropertiesEvaluation() {
    // Rectangular profile: 100 x 50 mm (0.1 x 0.05 m)
    double w = 0.100, h = 0.050;
    sketch::Sketch skRect;
    skRect.rectangle({0, 0}, {w, h});

    auto d = base({skRect});
    auto rectExt = makeExtrude(skRect.id, 0.020);
    auto rectId = rectExt.value("id").toString();

    auto res = evaluateModeling(d, {rectExt});
    const auto &part = res.part;

    QMap<QString, sketch::Sketch> sketchMap;
    sketchMap[skRect.id] = skRect;

    // 3.1 Sketch profile evaluation
    features::SectionPropertiesRequest skReq;
    skReq.revision = 1;
    skReq.targetType = "Sketch";
    skReq.sketchId = skRect.id;
    skReq.sketchMode = "AllClosed";

    auto skEval = part.evaluateSectionProperties(skReq, sketchMap);
    CHECK(skEval.valid);
    double expectedArea = w * h;          // 0.005 m^2
    double expectedPerim = 2.0 * (w + h); // 0.3 m
    near(skEval.area_m2, expectedArea, 1e-8);
    near(skEval.perimeter_m, expectedPerim, 1e-6);
    near(skEval.centroid_local_m[0], w / 2.0, 1e-6);
    near(skEval.centroid_local_m[1], h / 2.0, 1e-6);

    double expectedIuu = (1.0 / 12.0) * w * h * h * h; // (1/12) * 0.1 * 0.05^3
    double expectedIvv = (1.0 / 12.0) * h * w * w * w; // (1/12) * 0.05 * 0.1^3
    near(skEval.second_moments_m4[0], expectedIuu, 1e-11);
    near(skEval.second_moments_m4[1], expectedIvv, 1e-11);
    near(skEval.second_moments_m4[2], 0.0, 1e-11);
    near(skEval.polar_moment_m4, expectedIuu + expectedIvv, 1e-11);
    near(skEval.principal_moments_m4[0], expectedIuu, 1e-11);
    near(skEval.principal_moments_m4[1], expectedIvv, 1e-11);

    // 3.2 Planar solid face evaluation
    auto topFace = findFace(part.bodies[0].shape, rectId, 19.0, 21.0);
    features::SectionPropertiesRequest faceReq;
    faceReq.revision = 1;
    faceReq.targetType = "Face";
    faceReq.faceReference = topFace;

    auto faceEval = part.evaluateSectionProperties(faceReq, sketchMap);
    CHECK(faceEval.valid);
    near(faceEval.area_m2, expectedArea, 1e-8);
    near(faceEval.perimeter_m, expectedPerim, 1e-6);
    near(faceEval.centroid_part_m[2], 0.020, 1e-6);

    // 3.3 Nested cutout / hole profile evaluation
    // Outer 100x100 mm with concentric 50x50 mm square hole
    sketch::Sketch skHole;
    skHole.rectangle({0, 0}, {0.100, 0.100});
    skHole.rectangle({0.025, 0.025}, {0.075, 0.075});
    sketchMap[skHole.id] = skHole;

    features::SectionPropertiesRequest holeReq;
    holeReq.revision = 1;
    holeReq.targetType = "Sketch";
    holeReq.sketchId = skHole.id;
    holeReq.sketchMode = "AllClosed";

    auto holeEval = part.evaluateSectionProperties(holeReq, sketchMap);
    CHECK(holeEval.valid);
    // Area: 0.1^2 - 0.05^2 = 0.01 - 0.0025 = 0.0075 m^2
    near(holeEval.area_m2, 0.0075, 1e-8);
    // Perimeter: 4*0.1 + 4*0.05 = 0.4 + 0.2 = 0.6 m
    near(holeEval.perimeter_m, 0.600, 1e-6);
    near(holeEval.centroid_local_m[0], 0.050, 1e-6);
    near(holeEval.centroid_local_m[1], 0.050, 1e-6);
    // Centroidal second moment: I_outer - I_inner
    double expectedI_annular = (1.0 / 12.0) * (0.1 * std::pow(0.1, 3) - 0.05 * std::pow(0.05, 3));
    near(holeEval.second_moments_m4[0], expectedI_annular, 1e-11);
    near(holeEval.second_moments_m4[1], expectedI_annular, 1e-11);

    // 3.4 Open curve rejection
    sketch::Sketch openSk;
    auto p1 = openSk.point({0, 0});
    auto p2 = openSk.point({0.100, 0});
    openSk.line(p1, p2);
    sketchMap[openSk.id] = openSk;

    features::SectionPropertiesRequest openReq;
    openReq.revision = 1;
    openReq.targetType = "Sketch";
    openReq.sketchId = openSk.id;

    support_fixture::error("UnsupportedOperation",
                           [&]() { part.evaluateSectionProperties(openReq, sketchMap); });

    // 3.5 Non-planar face rejection
    // Create cylinder to get a cylindrical face
    sketch::Sketch skCircle;
    auto cp = skCircle.point({0.025, 0.025});
    skCircle.circle(cp, 0.020);

    auto dCyl = base({skCircle});
    auto cylExt = makeExtrude(skCircle.id, 0.040);
    auto cylId = cylExt.value("id").toString();

    auto cylRes = evaluateModeling(dCyl, {cylExt});
    auto cylFace = findCylindricalFace(cylRes.part.bodies[0].shape, cylId);

    features::SectionPropertiesRequest nonPlanarReq;
    nonPlanarReq.revision = 1;
    nonPlanarReq.targetType = "Face";
    nonPlanarReq.faceReference = cylFace;

    support_fixture::error("UnsupportedOperation", [&]() {
        cylRes.part.evaluateSectionProperties(nonPlanarReq, sketchMap);
    });
}

// ============================================================================
// 4. Viewport Display, Appearance Overrides and Section View Clipping
// ============================================================================

void testViewportDisplayAppearanceAndClipping() {
    Viewport vp;
    vp.resize(800, 600);

    // 4.1 Display modes
    vp.setMode("Shaded with edges");
    vp.setMode("Hidden lines visible");
    vp.setMode("Hidden edges");
    vp.setMode("Wireframe");

    // 4.2 Shadow toggle
    CHECK(!vp.shadows());
    vp.setShadows(true);
    CHECK(vp.shadows());
    vp.setShadows(false);
    CHECK(!vp.shadows());

    // 4.3 Appearance Override Precedence:
    // Face -> Feature -> Body -> Part -> Base
    QVector4D baseColor(0.3f, 0.64f, 0.83f, 1.0f);
    QVector4D partColor(0.2f, 0.2f, 0.2f, 1.0f);
    QVector4D bodyColor(0.4f, 0.4f, 0.4f, 1.0f);
    QVector4D featColor(0.6f, 0.6f, 0.6f, 1.0f);
    QVector4D faceColor(0.9f, 0.1f, 0.1f, 1.0f);

    QString bodyId = "body-1";
    QString featId = "feat-1";
    QString faceId = bodyId + "/feat-1/face/0123456789abcdef";

    // Level 0: Default base
    near(vp.resolveAppearance(faceId, baseColor).x(), baseColor.x(), 1e-5);

    // Level 1: Part override
    vp.setPartAppearance(partColor);
    near(vp.resolveAppearance(faceId, baseColor).x(), partColor.x(), 1e-5);

    // Level 2: Body override (beats Part)
    vp.setBodyAppearance(bodyId, bodyColor);
    near(vp.resolveAppearance(faceId, baseColor).x(), bodyColor.x(), 1e-5);

    // Level 3: Feature override (beats Body)
    vp.setFeatureAppearance(featId, featColor);
    near(vp.resolveAppearance(faceId, baseColor).x(), featColor.x(), 1e-5);

    // Level 4: Face override (beats Feature)
    vp.setFaceAppearance(faceId, faceColor);
    near(vp.resolveAppearance(faceId, baseColor).x(), faceColor.x(), 1e-5);

    // Clear overrides restores base
    vp.clearAppearanceOverrides();
    near(vp.resolveAppearance(faceId, baseColor).x(), baseColor.x(), 1e-5);

    // 4.4 View-only Clipping and Matching Picking
    auto mesh = std::make_shared<DisplayMesh>();
    // Simple box from (0,0,0) to (50, 50, 50) mm
    // Top face at Z = 50 mm
    mesh->vertices = {
        {0, 0, 50}, {50, 0, 50}, {50, 50, 50}, {0, 50, 50}, // top face (quad as 2 tris)
        {0, 0, 0},  {50, 0, 0},  {50, 50, 0},  {0, 50, 0}   // bottom face
    };
    mesh->indices = {
        0, 1, 2, 0, 2, 3, // top
        4, 6, 5, 4, 7, 6  // bottom
    };
    mesh->triangleRoles = {0, 0, 1, 1};
    mesh->faceIds = {"body-1/face/top", "body-1/face/bottom"};
    mesh->instances = {Instance{"inst-1", {0, 0, 0}, {}}};
    mesh->minimum = {0, 0, 0};
    mesh->maximum = {50, 50, 50};

    vp.setMesh(mesh);
    vp.setView("Front"); // Looking along Z at the front/XY face
    auto pickPos = vp.project({25, 25, 50});

    // Without section clipping, center click picks front face
    auto unclippedHit = vp.pick(pickPos);
    CHECK(!unclippedHit.empty());
    CHECK(unclippedHit.kind == "Face");
    CHECK(unclippedHit.entity == "body-1/face/top");

    // Activate Section Clipping:
    // Cutting plane at Z = 25 mm with normal (0, 0, -1)
    // Retains Z <= 25 mm (bottom), clips away Z > 25 mm (top face clipped!)
    vp.setSectionClipping(true, {25, 25, 25}, {0, 0, -1}, 0.0, false);
    CHECK(vp.sectionClippingActive());
    near(vp.sectionClippingOrigin().z(), 25.0f, 1e-4);

    // Picking at center now encounters clipped top face -> discarded, hits bottom face or passes
    // through
    auto clippedHit = vp.pick(pickPos);
    if (!clippedHit.empty()) {
        // Must NOT hit the clipped top face!
        CHECK(clippedHit.entity != "body-1/face/top");
    }

    // Deactivate section clipping -> top face is pickable again
    vp.setSectionClipping(false);
    CHECK(!vp.sectionClippingActive());
    auto restoredHit = vp.pick(pickPos);
    CHECK(!restoredHit.empty());
    CHECK(restoredHit.entity == "body-1/face/top");
}

} // namespace

int main(int argc, char **argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);

    try {
        testHoleCatalogResolutionAndOverrides();
        testMassPropertiesEvaluation();
        testSectionPropertiesEvaluation();
        testViewportDisplayAppearanceAndClipping();
    } catch (const std::exception &e) {
        fprintf(stderr, "FATAL: Unhandled exception in cm9_features: %s\n", e.what());
        return 1;
    }

    printf("cm9_features: All tests PASSED.\n");
    return 0;
}
