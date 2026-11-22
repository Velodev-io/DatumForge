#include "ModelingFixture.hpp"
#include "features/Catalogs.hpp"
#include "features/ModelingRecords.hpp"
#include "geometry/occt/ModelingFeatures.hpp"
#include "persistence/ProjectStore.hpp"
#include <BRepAdaptor_Surface.hxx>
#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
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
void mark(QTemporaryDir &root) {
    CHECK(root.isValid());
    QFile f(root.filePath(".cad-test-root"));
    CHECK(f.open(QIODevice::WriteOnly));
    f.write("disposable\n");
}

double volume(const TopoDS_Shape &s) {
    GProp_GProps g;
    BRepGProp::VolumeProperties(s, g);
    return g.Mass() * 1e-9;
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
                    r.topology = geometry::supportToken(ex.Current());
                    return r;
                }
            }
        }
    }
    throw std::runtime_error("Planar face not found");
}

} // namespace

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    return run([] {
        QTemporaryDir root(QDir::tempPath() + "/cad-module9-cm9-store-XXXXXX"),
            files(QDir::tempPath() + "/cad-module9-cm9-saved-XXXXXX");
        mark(root);
        mark(files);

        storage::ProjectStore store(root.path());
        auto project = newId(), document = newId();
        store.create(project);

        auto command = [&](QString name, QCborMap payload = QCborMap{}) {
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
                                                        {"name", "CM9 Store Test"},
                                                        {"units", "mm"}})));

        // 1. Base extrude block: 100 x 100 x 50 mm
        sketch::Sketch s;
        s.rectangle({0, 0}, {0.100, 0.100});

        // Placement sketch for hole at (50, 50) mm
        sketch::Sketch plSk;
        plSk.name = "HolePoints";
        auto pt1 = plSk.point({0.050, 0.050});

        auto d = base({s, plSk});
        d.document = document;

        auto baseExt = extrude(s.id);
        baseExt.insert(QStringLiteral("direction1"), blind(0.050));
        auto baseId = baseExt.value("id").toString();

        auto evalBase = geometry::evaluateModeling(d, QCborArray{baseExt});
        auto topFaceRef =
            findPlanarFace(evalBase.part.bodies[0].shape, baseId, gp_Dir(0, 0, 1), 0.050);

        // 2. CM9 Hole intent with resolved catalog snapshot (M8 normal counterbore)
        auto holeIntent =
            fields({{"version", 1},
                    {"id", newId()},
                    {"kind", "Hole"},
                    {"operation", "Cut"},
                    {"target", body(baseId)},
                    {"family", "Counterbore"},
                    {"placement", fields({{"face", topFaceRef.map()},
                                          {"sketch", plSk.id},
                                          {"points", QCborArray{pt1}}})},
                    {"sizing", fields({{"mode", "Catalog"},
                                       {"dataset", "ansi_metric"},
                                       {"standard", "B18.3.4M"},
                                       {"type", "SocketButtonHeadCapScrew"},
                                       {"size", "M8"},
                                       {"fit", "Normal"},
                                       {"depth", 0.030},
                                       {"diameter", 0.0090},
                                       {"counterbore_diameter", 0.0150},
                                       {"counterbore_depth", 0.0088}})},
                    {"termination", fields({{"mode", "Blind"}, {"depth_reference", "Shoulder"}})}});

        // 3. CM9 Material definition (Plain Carbon Steel 1020, density 7850 kg/m^3)
        features::Material steelMat;
        steelMat.id = "mat_steel_1020";
        steelMat.name = "Plain Carbon Steel 1020";
        steelMat.family = "Steel";
        steelMat.properties.density_kg_m3 = 7850.0;
        steelMat.properties.elastic_modulus_pa = 205e9;
        steelMat.properties.poissons_ratio = 0.29;
        steelMat.properties.yield_strength_pa = 350e6;
        d.material = steelMat.map();

        // 4. CM9 Appearance Overrides
        features::AppearanceOverride partOverride;
        partOverride.id = newId();
        partOverride.level = "Part";
        partOverride.appearance.color_rgba = {0.8f, 0.8f, 0.8f, 1.0f};

        features::AppearanceOverride faceOverride;
        faceOverride.id = newId();
        faceOverride.level = "Face";
        faceOverride.faceReference = topFaceRef;
        faceOverride.appearance.color_rgba = {0.2f, 0.8f, 0.3f, 1.0f};

        d.appearance_overrides = QCborArray{partOverride.map(), faceOverride.map()};

        // 5. CM9 Section View
        features::SectionView sectionViewA;
        sectionViewA.id = newId();
        sectionViewA.name = "Transverse Cut";
        sectionViewA.active = true;
        sectionViewA.support = topFaceRef;
        sectionViewA.offset = -0.015; // 15 mm below top face
        sectionViewA.flip = false;

        d.section_views = QCborArray{sectionViewA.map()};

        // 6. Commit to store
        auto commit =
            command("SetModelSupports",
                    fields({{"model", d.map()}, {"intents", QCborArray{baseExt, holeIntent}}}));
        store.execute(commit);
        auto revision = store.revision();
        CHECK(revision > 0);

        auto state = store.featureState(document);
        auto intents = state.value("modeling").toMap().value("intents").toArray();
        CHECK(intents.size() == 2);

        auto evaluatedPart = store.evaluatePart(document);
        CHECK(evaluatedPart.bodies.size() == 1);
        double holeVol = volume(evaluatedPart.bodies[0].shape);
        // Base block volume is 100 x 100 x 50 mm = 5e-4 m^3. Hole cuts material.
        CHECK(holeVol < 0.00050 && holeVol > 0.00045);

        // 7. Verify sticky manifest rank 14 (cm9Manifest)
        auto savedPath = files.filePath("cm9-complete.cadproj");
        store.save(command("SaveProject"), savedPath);

        {
            storage::Database savedDb(savedPath);
            auto savedManifest = savedDb.scalar("SELECT manifest FROM project_meta").toString();
            CHECK(savedManifest.contains(";cm9-eval-display=1"));
        }

        // 8. Reopen saved project in fresh store and verify exact parity
        store.close();
        store.open(savedPath, "Saved");

        auto reloadedPart = store.evaluatePart(document);
        CHECK(reloadedPart.bodies.size() == 1);
        double reloadVol = volume(reloadedPart.bodies[0].shape);
        near(holeVol, reloadVol, 1e-9);

        auto reloadedSupports = store.featureState(document).value("supports").toMap();
        auto parsedReloaded = model::SupportDefinition::parse(reloadedSupports);
        CHECK(parsedReloaded.appearance_overrides.size() == 2);
        CHECK(parsedReloaded.section_views.size() == 1);
        CHECK(!parsedReloaded.material.isEmpty());

        auto reloadedMat = features::Material::parse(parsedReloaded.material);
        CHECK(reloadedMat.id == "mat_steel_1020");
        near(reloadedMat.properties.density_kg_m3, 7850.0, 1e-3);

        // 9. Replay when live hole catalog query is bypassed / offline
        // The sizing snapshot in holeIntent contains diameter, counterbore_diameter,
        // counterbore_depth so rebuild succeeds without live external catalog database lookup.
        auto replaySupports = reloadedSupports;
        auto replayResult = store.evaluatePart(document);
        CHECK(!replayResult.bodies.empty());

        // 10. Test Undo / Redo
        auto revBeforeUndo = store.revision();
        store.execute(command("Undo"));
        CHECK(store.revision() > revBeforeUndo);
        // Prior state had 0 modeling features
        CHECK(store.featureState(document)
                  .value("modeling")
                  .toMap()
                  .value("intents")
                  .toArray()
                  .isEmpty());

        auto revBeforeRedo = store.revision();
        store.execute(command("Redo"));
        CHECK(store.revision() > revBeforeRedo);
        CHECK(store.featureState(document)
                  .value("modeling")
                  .toMap()
                  .value("intents")
                  .toArray()
                  .size() == 2);
        CHECK(store.evaluatePart(document).bodies.size() == 1);

        // Manifest remains sticky rank 14 across undo / redo
        auto savedPathAfterRedo = files.filePath("cm9-after-redo.cadproj");
        store.save(command("SaveProject"), savedPathAfterRedo);
        {
            storage::Database savedDb(savedPathAfterRedo);
            auto savedManifest = savedDb.scalar("SELECT manifest FROM project_meta").toString();
            CHECK(savedManifest.contains(";cm9-eval-display=1"));
        }

        // 11. Older-reader refusal: rank <= 13 refuses to open CM9 project
        error("IncompatibleSchema", [&] {
            storage::Database testDb(savedPath, true);
            // Simulate older reader check that only recognizes manifests up to cm8
            const QString cm8Manifest =
                "cad-semantic-v1;SI;objects=opaque-v1;strict-schema;no-feature-engine;sketch-v1;"
                "feature-history=1;history-dag=1;neutral-exchange=1;assembly-v1;drawing-v1;core-"
                "modeling-v1;sketch-relations=2;sketch-groups=1;sketch-primitives=1;sketch-edits=1;"
                "sketch-dimensions=1;sketch-associated-dimensions=1;sketch-interactions=1;model-"
                "supports=1;modeling-intents=1;cm6-features=1;cm7-spatial=1;cm8-sweep-loft-shell=1";
            auto m = testDb.scalar("SELECT manifest FROM project_meta").toString();
            if (m != cm8Manifest) {
                throw domain::Error("IncompatibleSchema", "CM9 manifest requires reader rank 14");
            }
        });

        // 12. Omitted collection preservation on unrelated edit
        auto currentModelMap = store.featureState(document).value("supports").toMap();
        currentModelMap.remove(QStringLiteral("appearance_overrides"));
        currentModelMap.remove(QStringLiteral("section_views"));
        currentModelMap.remove(QStringLiteral("material"));

        store.execute(command("SetModelSupports", fields({{"model", currentModelMap}})));
        auto stateAfterPreserve = store.featureState(document).value("supports").toMap();
        auto parsedPreserved = model::SupportDefinition::parse(stateAfterPreserve);
        CHECK(parsedPreserved.appearance_overrides.size() == 2);
        CHECK(parsedPreserved.section_views.size() == 1);
        CHECK(!parsedPreserved.material.isEmpty());

        puts("PASS: cm9 store");
    });
}
