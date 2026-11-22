#include "features/Catalogs.hpp"
#include "features/Model.hpp"
#include "features/ModelingIntent.hpp"
#include "sketch/Sketch.hpp"
#include "support/check.hpp"
#include <QCoreApplication>
#include <cmath>
#include <numbers>

using namespace cad;
using namespace cad::features;
using sketch::fields;

namespace {
template <class F> void error(QString expected, F &&action) {
    QString code, message;
    try {
        action();
    } catch (const domain::Error &e) {
        code = e.code;
        message = QString::fromUtf8(e.what());
    } catch (const std::exception &e) {
        code = "InvalidInput";
        message = QString::fromUtf8(e.what());
    }
    if (code != expected)
        throw std::runtime_error("Expected " + expected.toStdString() + ", got " +
                                 code.toStdString() + ": " + message.toStdString());
}

QCborMap faceRef(QString owner, QString topology = {}) {
    domain::GeometryReference r;
    r.object = owner;
    r.role = "Face";
    r.topology = topology.isEmpty() ? "face/" + QString(64, 'a') : topology;
    return r.map();
}

QCborMap planeRef(QString owner) {
    domain::GeometryReference r;
    r.object = owner;
    r.role = "Plane";
    return r.map();
}

// ============================================================================
// 1. Hole Catalog Contracts
// ============================================================================

void testHoleCatalogContracts() {
    // 1.1 ANSI Metric - T7 p. 21 nominal fixture: M8 Socket Button Head Cap Screw
    {
        HoleCatalogQuery q;
        q.version = 1;
        q.dataset = "ansi_metric";
        q.standard = "B18.3.4M";
        q.type = "SocketButtonHeadCapScrew";
        q.size = "M8";
        q.fit = "Normal";

        auto qMap = q.map();
        CHECK(HoleCatalogQuery::parse(qMap).map() == qMap);

        auto resolved = resolveHoleCatalog(q);
        CHECK(resolved.valid);
        CHECK(resolved.family == "Counterbore");
        CHECK(std::abs(resolved.diameter - 0.0090) < 1e-6); // 9 mm drill
        CHECK(resolved.counterbore_diameter.has_value());
        CHECK(std::abs(*resolved.counterbore_diameter - 0.0150) < 1e-6); // 15 mm cbore
        CHECK(resolved.counterbore_depth.has_value());
        CHECK(std::abs(*resolved.counterbore_depth - 0.0088) < 1e-6); // 8.8 mm cbore depth

        auto rMap = resolved.map();
        CHECK(HoleCatalogResolved::parse(rMap).map() == rMap);
    }

    // 1.2 Course Override Fixture: T7 p. 29 M8 Counterbore Override (8.5 / 14 / 4.2 mm)
    {
        HoleCatalogQuery q;
        q.version = 1;
        q.dataset = "ansi_metric";
        q.standard = "B18.3.4M";
        q.type = "SocketButtonHeadCapScrew";
        q.size = "M8";
        q.fit = "Normal";
        q.custom_overrides.insert(QStringLiteral("diameter"), 0.0085);
        q.custom_overrides.insert(QStringLiteral("counterbore_diameter"), 0.0140);
        q.custom_overrides.insert(QStringLiteral("counterbore_depth"), 0.0042);

        auto resolved = resolveHoleCatalog(q);
        CHECK(resolved.valid);
        CHECK(resolved.family == "Counterbore");
        CHECK(std::abs(resolved.diameter - 0.0085) < 1e-6);
        CHECK(std::abs(*resolved.counterbore_diameter - 0.0140) < 1e-6);
        CHECK(std::abs(*resolved.counterbore_depth - 0.0042) < 1e-6);
    }

    // 1.3 ANSI Metric - Countersink & Tapped
    {
        HoleCatalogQuery csink;
        csink.version = 1;
        csink.dataset = "ansi_metric";
        csink.standard = "B18.6.7M";
        csink.type = "CountersunkHeadScrew";
        csink.size = "M8";
        csink.fit = "Normal";
        auto resCsink = resolveHoleCatalog(csink);
        CHECK(resCsink.family == "Countersink");
        CHECK(resCsink.countersink_diameter.has_value());
        CHECK(resCsink.countersink_angle.has_value());

        HoleCatalogQuery tapped;
        tapped.version = 1;
        tapped.dataset = "ansi_metric";
        tapped.standard = "MetricTapped";
        tapped.type = "TappedHole";
        tapped.size = "M8";
        tapped.fit = "Normal";
        auto resTapped = resolveHoleCatalog(tapped);
        CHECK(resTapped.family == "Tapped");
        CHECK(resTapped.tap_drill_diameter.has_value());
        CHECK(std::abs(*resTapped.tap_drill_diameter - 0.0068) < 1e-6); // 6.8 mm
        CHECK(resTapped.pitch.has_value());
        CHECK(std::abs(*resTapped.pitch - 0.00125) < 1e-6); // 1.25 mm
        CHECK(resTapped.cosmetic_thread);
        CHECK(resTapped.thread_designation == "M8x1.25");
    }

    // 1.4 ANSI Inch - Counterbore, Countersink, Simple, Tapped, PipeTap
    {
        constexpr double in2m = 0.0254;
        HoleCatalogQuery qCbore;
        qCbore.version = 1;
        qCbore.dataset = "ansi_inch";
        qCbore.standard = "B18.3";
        qCbore.type = "SocketHeadCapScrew";
        qCbore.size = "1/4-20";
        qCbore.fit = "Normal";
        auto resCbore = resolveHoleCatalog(qCbore);
        CHECK(resCbore.family == "Counterbore");
        CHECK(std::abs(resCbore.diameter - 0.2660 * in2m) < 1e-6);
        CHECK(std::abs(*resCbore.counterbore_diameter - 0.4375 * in2m) < 1e-6);

        HoleCatalogQuery qCsink;
        qCsink.version = 1;
        qCsink.dataset = "ansi_inch";
        qCsink.standard = "B18.6.3";
        qCsink.type = "CountersunkFlatHead";
        qCsink.size = "1/4-20";
        qCsink.fit = "Normal";
        auto resCsink = resolveHoleCatalog(qCsink);
        CHECK(resCsink.family == "Countersink");
        CHECK(std::abs(*resCsink.countersink_angle - 82.0 * std::numbers::pi / 180.0) < 1e-4);

        HoleCatalogQuery qSimple;
        qSimple.version = 1;
        qSimple.dataset = "ansi_inch";
        qSimple.standard = "B18.2.1";
        qSimple.type = "HexBolt";
        qSimple.size = "1/4-20";
        qSimple.fit = "Normal";
        auto resSimple = resolveHoleCatalog(qSimple);
        CHECK(resSimple.family == "Simple");

        HoleCatalogQuery qTapped;
        qTapped.version = 1;
        qTapped.dataset = "ansi_inch";
        qTapped.standard = "InchTapped";
        qTapped.type = "TappedHole";
        qTapped.size = "1/4-20";
        qTapped.fit = "Normal";
        auto resTapped = resolveHoleCatalog(qTapped);
        CHECK(resTapped.family == "Tapped");
        CHECK(resTapped.cosmetic_thread);
        CHECK(resTapped.thread_designation == "1/4-20 UNC");

        HoleCatalogQuery qPipe;
        qPipe.version = 1;
        qPipe.dataset = "ansi_inch";
        qPipe.standard = "ASME_B1.20.1";
        qPipe.type = "PipeTap";
        qPipe.size = "1/4-18 NPT";
        qPipe.fit = "Normal";
        auto resPipe = resolveHoleCatalog(qPipe);
        CHECK(resPipe.family == "PipeTap");
        CHECK(resPipe.thread_designation == "1/4-18 NPT");
    }

    // 1.5 International Families: ISO, DIN, BSI, JIS, GB, GOST
    {
        for (const QString &ds : {"iso", "din", "bsi", "jis", "gb", "gost"}) {
            HoleCatalogQuery q;
            q.version = 1;
            q.dataset = ds;
            q.size = "M8";

            if (ds == "iso") {
                q.standard = "ISO 4762";
                q.type = "SocketHeadCapScrew";
                q.fit = "Medium";
            } else if (ds == "din") {
                q.standard = "DIN 912";
                q.type = "SocketHeadCapScrew";
                q.fit = "Mittel";
            } else if (ds == "bsi") {
                q.standard = "BS 4168";
                q.type = "SocketHeadCapScrew";
                q.fit = "Normal";
            } else if (ds == "jis") {
                q.standard = "JIS B 1176";
                q.type = "SocketHeadCapScrew";
                q.fit = "Class 2";
            } else if (ds == "gb") {
                q.standard = "GB/T 70.1";
                q.type = "SocketHeadCapScrew";
                q.fit = "Medium";
            } else if (ds == "gost") {
                q.standard = "GOST 11738";
                q.type = "SocketHeadCapScrew";
                q.fit = "Row 2";
            }

            auto res = resolveHoleCatalog(q);
            CHECK(res.valid);
            CHECK(res.family == "Counterbore");
            CHECK(std::abs(res.diameter - 0.0090) < 1e-6);
            CHECK(res.counterbore_diameter.has_value());
            CHECK(res.counterbore_depth.has_value());
        }
    }

    // 1.6 Error handling
    {
        HoleCatalogQuery badDs;
        badDs.dataset = "unknown_dataset";
        error("UnsupportedOperation", [&] { resolveHoleCatalog(badDs); });

        HoleCatalogQuery badSize;
        badSize.dataset = "iso";
        badSize.standard = "ISO 4762";
        badSize.type = "SocketHeadCapScrew";
        badSize.size = "M999";
        badSize.fit = "Medium";
        error("UnsupportedOperation", [&] { resolveHoleCatalog(badSize); });

        QCborMap badVersion = badSize.map();
        badVersion.insert(QStringLiteral("version"), 2);
        error("InvalidInput", [&] { HoleCatalogQuery::parse(badVersion); });
    }
}

// ============================================================================
// 2. Materials & Physical Properties Contracts
// ============================================================================

void testMaterialContracts() {
    // 2.1 Standard materials library inspection
    auto materials = standardMaterials();
    CHECK(!materials.empty());
    CHECK(materials.size() >= 15);

    auto steel1020 = findMaterial("steel_1020");
    CHECK(steel1020.has_value());
    CHECK(steel1020->family == "Steel");
    CHECK(steel1020->properties.density_kg_m3 == 7870.0);
    CHECK(steel1020->properties.elastic_modulus_pa.has_value());
    CHECK(*steel1020->properties.elastic_modulus_pa == 205e9);
    CHECK(steel1020->properties.poissons_ratio.has_value());
    CHECK(std::abs(*steel1020->properties.poissons_ratio - 0.29) < 1e-4);

    auto al6061 = findMaterial("al_6061_t6");
    CHECK(al6061.has_value());
    CHECK(al6061->family == "Aluminum");
    CHECK(al6061->properties.density_kg_m3 == 2700.0);

    // 2.2 Material serialization and roundtrip
    auto matMap = steel1020->map();
    auto parsedMat = Material::parse(matMap);
    CHECK(parsedMat.map() == matMap);
    CHECK(parsedMat.id == "steel_1020");
    CHECK(parsedMat.properties.density_kg_m3 == 7870.0);

    // 2.3 Custom material authoring
    Material customMat;
    customMat.version = 1;
    customMat.id = "custom_polymer_x";
    customMat.name = "Custom Polymer X";
    customMat.family = "Plastics";
    customMat.properties.density_kg_m3 = 1350.0;
    customMat.properties.elastic_modulus_pa = 3.2e9;
    customMat.properties.poissons_ratio = 0.38;
    customMat.properties.yield_strength_pa = 80e6;

    auto customMap = customMat.map();
    CHECK(Material::parse(customMap).map() == customMap);

    // 2.4 Error validation
    QCborMap badDensity = customMap;
    QCborMap props = customMap.value("properties").toMap();
    props.insert(QStringLiteral("density_kg_m3"), -5.0);
    badDensity.insert(QStringLiteral("properties"), props);
    error("InvalidInput", [&] { Material::parse(badDensity); });

    QCborMap badPoisson = customMap;
    props = customMap.value("properties").toMap();
    props.insert(QStringLiteral("poissons_ratio"), 0.55); // >= 0.5 is invalid for elastic solids
    badPoisson.insert(QStringLiteral("properties"), props);
    error("InvalidInput", [&] { Material::parse(badPoisson); });

    QCborMap badFamily = customMap;
    badFamily.insert(QStringLiteral("family"), "Unobtainium");
    error("InvalidInput", [&] { Material::parse(badFamily); });
}

// ============================================================================
// 3. Mass Properties Evaluation Contracts
// ============================================================================

void testMassPropertiesContracts() {
    // 3.1 Request parsing and roundtrip
    MassPropertiesRequest req;
    req.revision = 42;
    req.scopeType = "Part";
    req.frame = "Part";
    auto reqMap = req.map();
    CHECK(MassPropertiesRequest::parse(reqMap).map() == reqMap);

    MassPropertiesRequest bodyReq;
    bodyReq.revision = 12;
    bodyReq.scopeType = "Body";
    bodyReq.targetId = newId();
    bodyReq.frame = "CenterOfMass";
    bodyReq.overrideDensity = 2700.0;
    auto bodyMap = bodyReq.map();
    CHECK(MassPropertiesRequest::parse(bodyMap).map() == bodyMap);

    // 3.2 Result parsing, roundtrip, and missing density handling
    MassPropertiesResult res;
    res.revision = 42;
    res.valid = true;
    res.solids_count = 1;
    res.volume_m3 = 0.001; // 1 liter
    res.surface_area_m2 = 0.06;
    res.density_kg_m3 = 7870.0;
    res.density_missing = false;
    res.mass_kg = 7.870;
    res.center_of_mass_m = {0.05, 0.05, 0.05};
    res.inertia_tensor_kg_m2 = {0.0131, 0.0131, 0.0131, 0.0, 0.0, 0.0};
    res.principal_moments_kg_m2 = {0.0131, 0.0131, 0.0131};
    res.principal_axes = {{{1, 0, 0}, {0, 1, 0}, {0, 0, 1}}};

    auto resMap = res.map();
    CHECK(MassPropertiesResult::parse(resMap).map() == resMap);

    // Missing density case: volume/area valid, mass null, density_missing true
    MassPropertiesResult noDensityRes = res;
    noDensityRes.density_kg_m3 = std::nullopt;
    noDensityRes.density_missing = true;
    noDensityRes.mass_kg = std::nullopt;
    auto noDensMap = noDensityRes.map();
    auto parsedNoDens = MassPropertiesResult::parse(noDensMap);
    CHECK(parsedNoDens.density_missing);
    CHECK(!parsedNoDens.mass_kg.has_value());
    CHECK(!parsedNoDens.density_kg_m3.has_value());
    CHECK(parsedNoDens.volume_m3 == 0.001);

    // 3.3 Error validation
    QCborMap badScope = reqMap;
    badScope.insert(QStringLiteral("scope_type"), "InvalidScope");
    error("InvalidInput", [&] { MassPropertiesRequest::parse(badScope); });

    QCborMap missingBodyId = reqMap;
    missingBodyId.insert(QStringLiteral("scope_type"), "Body");
    error("InvalidInput", [&] { MassPropertiesRequest::parse(missingBodyId); });
}

// ============================================================================
// 4. Section Properties Evaluation Contracts
// ============================================================================

void testSectionPropertiesContracts() {
    // 4.1 Sketch target request
    SectionPropertiesRequest skReq;
    skReq.revision = 7;
    skReq.targetType = "Sketch";
    skReq.sketchId = newId();
    skReq.sketchMode = "AllClosed";

    auto skMap = skReq.map();
    CHECK(SectionPropertiesRequest::parse(skMap).map() == skMap);

    // 4.2 Face target request
    SectionPropertiesRequest faceReq;
    faceReq.revision = 8;
    faceReq.targetType = "Face";
    faceReq.faceReference = domain::GeometryReference::parse(faceRef(newId()));

    auto faceMap = faceReq.map();
    CHECK(SectionPropertiesRequest::parse(faceMap).map() == faceMap);

    // 4.3 Section properties result roundtrip
    SectionPropertiesResult res;
    res.revision = 7;
    res.valid = true;
    res.area_m2 = 0.0024; // 60 mm x 40 mm rectangle
    res.perimeter_m = 0.200;
    res.centroid_local_m = {0.030, 0.020};
    res.centroid_part_m = {0.030, 0.020, 0.0};
    // Iuu = b*h^3 / 12 = 0.060 * 0.040^3 / 12 = 3.2e-7 m^4
    // Ivv = h*b^3 / 12 = 0.040 * 0.060^3 / 12 = 7.2e-7 m^4
    // Iuv = 0
    res.second_moments_m4 = {3.2e-7, 7.2e-7, 0.0};
    res.polar_moment_m4 = 3.2e-7 + 7.2e-7; // J = Iuu + Ivv
    res.principal_moments_m4 = {3.2e-7, 7.2e-7};
    res.principal_angle_rad = 0.0;

    auto resMap = res.map();
    CHECK(SectionPropertiesResult::parse(resMap).map() == resMap);

    // 4.4 Error validation
    QCborMap badTarget = skMap;
    badTarget.insert(QStringLiteral("target_type"), "Solid");
    error("InvalidInput", [&] { SectionPropertiesRequest::parse(badTarget); });
}

// ============================================================================
// 5. Appearance Precedence and Overrides Contracts
// ============================================================================

void testAppearancePrecedenceContracts() {
    // 5.1 Appearance serialization and roundtrip
    Appearance app;
    app.color_rgba = {0.9f, 0.1f, 0.2f, 1.0f};
    app.roughness = 0.35f;
    app.metalness = 0.8f;
    app.transparency = 0.0f;

    auto appMap = app.map();
    CHECK(Appearance::parse(appMap).map() == appMap);

    // 5.2 Appearance override record
    AppearanceOverride ovFace;
    ovFace.id = newId();
    ovFace.level = "Face";
    ovFace.faceReference = domain::GeometryReference::parse(faceRef(newId()));
    ovFace.appearance = app;

    auto ovMap = ovFace.map();
    CHECK(AppearanceOverride::parse(ovMap).map() == ovMap);

    // 5.3 Precedence rule verification:
    // Face -> Feature -> Body -> Part -> MaterialDefault
    Appearance faceApp{{1.0f, 0.0f, 0.0f, 1.0f}, 0.1f, 0.0f, 0.0f};
    Appearance featApp{{0.0f, 1.0f, 0.0f, 1.0f}, 0.2f, 0.0f, 0.0f};
    Appearance bodyApp{{0.0f, 0.0f, 1.0f, 1.0f}, 0.3f, 0.0f, 0.0f};
    Appearance partApp{{1.0f, 1.0f, 0.0f, 1.0f}, 0.4f, 0.0f, 0.0f};
    Appearance matDefault{{0.5f, 0.5f, 0.5f, 1.0f}, 0.5f, 0.0f, 0.0f};

    // All active -> Face wins
    CHECK(resolveAppearance(faceApp, featApp, bodyApp, partApp, matDefault) == faceApp);

    // Face removed -> Feature wins
    CHECK(resolveAppearance(std::nullopt, featApp, bodyApp, partApp, matDefault) == featApp);

    // Feature removed -> Body wins
    CHECK(resolveAppearance(std::nullopt, std::nullopt, bodyApp, partApp, matDefault) == bodyApp);

    // Body removed -> Part wins
    CHECK(resolveAppearance(std::nullopt, std::nullopt, std::nullopt, partApp, matDefault) ==
          partApp);

    // Part removed -> Material default wins
    CHECK(resolveAppearance(std::nullopt, std::nullopt, std::nullopt, std::nullopt, matDefault) ==
          matDefault);

    // All empty -> default appearance
    CHECK(resolveAppearance(std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt) ==
          Appearance{});

    // 5.4 Error validation
    QCborMap badRgba = appMap;
    badRgba.insert(QStringLiteral("color_rgba"), QCborArray{1.5, 0.0, 0.0, 1.0}); // > 1.0
    error("InvalidInput", [&] { Appearance::parse(badRgba); });

    QCborMap badLevel = ovMap;
    badLevel.insert(QStringLiteral("level"), "Vertex");
    error("InvalidInput", [&] { AppearanceOverride::parse(badLevel); });
}

// ============================================================================
// 6. Named Section Views Contracts
// ============================================================================

void testSectionViewContracts() {
    auto plane = planeRef(newId());
    SectionView view;
    view.version = 1;
    view.id = newId();
    view.name = "Front Section View";
    view.active = true;
    view.support = domain::GeometryReference::parse(plane);
    view.offset = 0.025; // 25 mm offset
    view.flip = false;
    view.cap_color_rgba = std::array<float, 4>{0.85f, 0.2f, 0.2f, 1.0f};
    view.clip_mode = "ViewOnly";

    auto vMap = view.map();
    CHECK(SectionView::parse(vMap).map() == vMap);

    // Support with planar face
    auto face = faceRef(newId());
    SectionView faceSection = view;
    faceSection.id = newId();
    faceSection.name = "Face Section Cut";
    faceSection.support = domain::GeometryReference::parse(face);
    auto faceMap = faceSection.map();
    CHECK(SectionView::parse(faceMap).map() == faceMap);

    // Error validation
    QCborMap badSupport = vMap;
    domain::GeometryReference edgeRef;
    edgeRef.object = newId();
    edgeRef.role = "Edge";
    edgeRef.topology = "edge/" + QString(64, 'a');
    badSupport.insert(QStringLiteral("support"), edgeRef.map());
    error("InvalidInput", [&] { SectionView::parse(badSupport); });

    QCborMap badClip = vMap;
    badClip.insert(QStringLiteral("clip_mode"), "BRepCut"); // Must be ViewOnly
    error("UnsupportedOperation", [&] { SectionView::parse(badClip); });
}

} // namespace

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    try {
        testHoleCatalogContracts();
        testMaterialContracts();
        testMassPropertiesContracts();
        testSectionPropertiesContracts();
        testAppearancePrecedenceContracts();
        testSectionViewContracts();
        return 0;
    } catch (const std::exception &e) {
        fprintf(stderr, "Contract check failed: %s\n", e.what());
        return 1;
    }
}
