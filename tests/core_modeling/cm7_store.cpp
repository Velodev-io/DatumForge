#include "ModelingFixture.hpp"
#include "geometry/occt/ModelingFeatures.hpp"
#include "geometry/occt/SpatialCurve.hpp"
#include "geometry/occt/SupportReferences.hpp"
#include "model/DerivedCurve.hpp"
#include "persistence/ProjectStore.hpp"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
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

QByteArray bytes(const QString &path) {
    QFile file(path);
    CHECK(file.open(QIODevice::ReadOnly));
    return file.readAll();
}

domain::GeometryReference sketchRef(const QString &sketchId) {
    domain::GeometryReference r;
    r.object = sketchId;
    r.role = QStringLiteral("Object");
    return r;
}

domain::GeometryReference axisRef(const QString &axisId) {
    domain::GeometryReference r;
    r.object = axisId;
    r.role = QStringLiteral("Axis");
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

} // namespace

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    return run([] {
        QTemporaryDir root(QDir::tempPath() + "/cad-module5-cm7-store-XXXXXX"),
            files(QDir::tempPath() + "/cad-module5-cm7-files-XXXXXX");
        mark(root);
        mark(files);

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
                                                        {"name", "CM7 Store Test"},
                                                        {"units", "mm"}})));

        puts("Step 1");
        // 1. Save legacy project before any CM7 spatial data and verify bytes
        auto legacyPath = files.filePath("legacy.cadproj");
        store.save(command("SaveProject"), legacyPath);
        auto originalBytes = bytes(legacyPath);

        puts("Step 2");
        // 2. Build base planar sketch and solid base
        sketch::Sketch baseSketch;
        baseSketch.rectangle({0, 0}, {0.100, 0.100});

        auto baseExt = extrude(baseSketch.id);
        baseExt.insert(QStringLiteral("direction1"), blind(0.020));

        puts("Step 3");
        // 3. Build spatial sketch (3D chair-frame path segment with spatial constraint)
        sketch::SpatialSketch spatial;
        auto p0 = spatial.point({0.0, 0.0, 0.020});
        auto p1 = spatial.point({0.050, 0.0, 0.020});
        auto p2 = spatial.point({0.050, 0.050, 0.050});
        auto l1 = spatial.line(p0, p1);
        auto l2 = spatial.line(p1, p2);
        sketch::SpatialConstraint cAlongX;
        cAlongX.kind = QStringLiteral("AlongX");
        cAlongX.a = l1;
        spatial.constraints.push_back(cAlongX);

        puts("Step 4");
        // 4. Build derived curves:
        // 4a. DatumAxis (Z axis) and HelixSpiral
        QString axisId = newId();
        model::Datum axisDatum;
        axisDatum.id = axisId;
        axisDatum.kind = QStringLiteral("DatumAxis");
        axisDatum.mode = QStringLiteral("Principal");
        axisDatum.parameters = sketch::fields({{"axis", "Z"}});

        model::DerivedCurve helix;
        helix.id = newId();
        helix.name = QStringLiteral("TestHelix");
        helix.kind = QStringLiteral("HelixSpiral");
        model::HelixSpiralDefinition hd;
        hd.axis = axisRef(axisId);
        hd.radius = 0.015;
        hd.pitch = 0.005;
        hd.revolutions = 4.0;
        helix.helixSpiral = hd;

        // 4b. Two planar sketches for SketchOnSketch
        sketch::Sketch skXY;
        skXY.name = "SkXY";
        skXY.frame.origin = {0, 0, 0};
        auto a1 = skXY.point({0.010, 0.010});
        auto a2 = skXY.point({0.080, 0.080});
        skXY.line(a1, a2);

        sketch::Sketch skYZ;
        skYZ.name = "SkYZ";
        skYZ.frame = sketch::Frame{{0, 0, 0}, {0, 1, 0}, {0, 0, 1}};
        auto b1 = skYZ.point({0.010, 0.010});
        auto b2 = skYZ.point({0.080, 0.080});
        skYZ.line(b1, b2);

        model::DerivedCurve sos;
        sos.id = newId();
        sos.name = QStringLiteral("TestSketchOnSketch");
        sos.kind = QStringLiteral("SketchOnSketch");
        model::SketchOnSketchDefinition sosDef;
        sosDef.sketch1 = sketchRef(skXY.id);
        sosDef.sketch2 = sketchRef(skYZ.id);
        sosDef.branch = model::BranchSelection{0, QStringLiteral("branch/0"), false, 0.0, 1.0};
        sos.sketchOnSketch = sosDef;

        // 4c. DatumPlane with mode NormalCurve at Start of sos curve
        model::Datum normalPlane;
        normalPlane.id = newId();
        normalPlane.kind = QStringLiteral("DatumPlane");
        normalPlane.mode = QStringLiteral("NormalCurve");
        normalPlane.references = {curveRef(sos.id, QStringLiteral("Start"))};

        puts("Step 5 - mapping");
        // Construct initial complete SupportDefinition
        model::SupportDefinition modelDef;
        modelDef.document = document;
        modelDef.sketches = sketch::encode({baseSketch, skXY, skYZ});
        modelDef.datums = {axisDatum, normalPlane};
        modelDef.spatial_sketches = sketch::encodeSpatial({spatial});
        modelDef.derived_curves = model::encodeDerivedCurves({helix, sos});
        modelDef.history = {axisId,     baseSketch.id, skXY.id, skYZ.id,
                            spatial.id, helix.id,      sos.id,  normalPlane.id};

        auto mappedDef = modelDef.map();
        puts("Step 5 - committing");
        // 5. Commit CM7 model via SetModelSupports
        auto commit = command("SetModelSupports",
                              fields({{"model", mappedDef}, {"intents", QCborArray{baseExt}}}));
        store.execute(commit);
        puts("Step 5 - committed");
        auto revision = store.revision();
        CHECK(revision > 0);

        // Verify feature state contains spatial_sketches and derived_curves
        auto fs = store.featureState(document);
        auto supMap = fs.value("supports").toMap();
        CHECK(supMap.contains(QStringLiteral("spatial_sketches")));
        CHECK(supMap.contains(QStringLiteral("derived_curves")));
        CHECK(supMap.value("spatial_sketches").toArray().size() == 1);
        CHECK(supMap.value("derived_curves").toArray().size() == 2);

        // Verify exact previewSupports returns evaluated data
        auto preview = store.previewSupports(document, modelDef.map());
        CHECK(preview.value("version").toInteger() == 1);
        CHECK(!preview.value("part_empty").toBool());

        // 6. Deduplication: re-executing identical envelope returns same outcome, revision
        // unchanged
        store.execute(commit);
        CHECK(store.revision() == revision);

        // 7. Verify manifest and requirement rank 12
        auto cm7Path = files.filePath("cm7-complete.cadproj");
        store.save(command("SaveProject"), cm7Path);
        {
            storage::Database db(cm7Path);
            auto manifest = db.scalar("SELECT manifest FROM project_meta").toString();
            CHECK(manifest.contains(";cm7-spatial=1"));
        }

        // 8. Sticky manifest across Undo/Redo
        store.execute(command("Undo"));
        CHECK(store.featureState(document).value("supports").toMap().empty());
        {
            // Manifest remains sticky rank 12 because history retains CM7 semantics
            auto undoPath = files.filePath("undo.cadproj");
            store.save(command("SaveProject"), undoPath);
            storage::Database db(undoPath);
            auto manifest = db.scalar("SELECT manifest FROM project_meta").toString();
            CHECK(manifest.contains(";cm7-spatial=1"));
        }
        store.execute(command("Redo"));
        CHECK(store.featureState(document) == fs);
        CHECK(store.revision() > revision);

        // 9. Unrelated edit preservation (omitted vs deletion invariant)
        // An unrelated edit touching only the base sketch must preserve spatial_sketches and
        // derived_curves
        sketch::Sketch mutatedBase;
        mutatedBase.id = baseSketch.id;
        mutatedBase.name = baseSketch.name;
        mutatedBase.rectangle({0, 0}, {0.120, 0.120});
        auto unrelatedModel = modelDef;
        unrelatedModel.sketches = sketch::encode({mutatedBase, skXY, skYZ});
        auto unrelatedMap = unrelatedModel.map();
        // Simulate older editor omitting spatial_sketches and derived_curves
        unrelatedMap.remove(QStringLiteral("spatial_sketches"));
        unrelatedMap.remove(QStringLiteral("derived_curves"));
        // Remove from history as older editor would
        QCborArray strippedHist;
        for (auto h : unrelatedModel.history) {
            auto id = h;
            if (id != spatial.id && id != helix.id && id != sos.id)
                strippedHist.append(id);
        }
        unrelatedMap.insert(QStringLiteral("history"), strippedHist);

        store.execute(command("SetModelSupports", fields({{"model", unrelatedMap}})));
        CHECK(store.revision() > revision);
        revision = store.revision();

        // Check that spatial_sketches and derived_curves were preserved!
        auto preservedFs = store.featureState(document);
        auto preservedSup = preservedFs.value("supports").toMap();
        CHECK(preservedSup.contains(QStringLiteral("spatial_sketches")));
        CHECK(preservedSup.contains(QStringLiteral("derived_curves")));
        CHECK(preservedSup.value("spatial_sketches").toArray().size() == 1);
        CHECK(preservedSup.value("derived_curves").toArray().size() == 2);

        // 10. Storage boundary: invalid spatial_sketches type rejected without modifying database
        // state
        error("InvalidInput", [&] {
            store.execute(command("SetModelSupports", fields({{"model", modelDef.map()},
                                                              {"spatial_sketches", 12345}})));
        });
        CHECK(store.revision() == revision);

        // 11. Interrupted commit / cancellation: leaves state unchanged
        exchange::Control canceled;
        canceled.canceled = [] { return true; };
        error("Canceled", [&] {
            store.execute(command("SetModelSupports", fields({{"model", modelDef.map()}})),
                          canceled);
        });
        CHECK(store.revision() == revision);

        // 12. Save, Close, Reopen roundtrip verification
        auto finalPath = files.filePath("cm7-saved.cadproj");
        store.save(command("SaveProject"), finalPath);
        auto acceptedState = store.featureState(document);

        store.close();
        store.open(finalPath, "Saved");
        CHECK(store.featureState(document) == acceptedState);
        CHECK(store.evaluatePart(document).bodies.size() == 1);

        // 13. Restart and recovery from index
        store.close();
        store.recover(project);
        CHECK(store.featureState(document) == acceptedState);
        CHECK(store.evaluatePart(document).bodies.size() == 1);

        // 14. Portable copy / upgrade copy
        auto copyPath = files.filePath("cm7-copy.cadproj");
        store.save(
            command("PortableCopy", fields({{"new_project_id", newId()}, {"policy", "Copied"}})),
            copyPath);
        store.close();
        store.open(finalPath, "Saved");
        CHECK(store.featureState(document) == acceptedState);

        // Refuse to overwrite legacy file without Save As
        error("UpgradeCopyRequired", [&] { store.save(command("SaveProject"), legacyPath); });
        CHECK(bytes(legacyPath) == originalBytes);

        // 15. Forged downgrade & schema tamper refusal
        auto forgedPath = files.filePath("forged-downgrade.cadproj");
        CHECK(QFile::copy(finalPath, forgedPath));
        {
            storage::Database db(forgedPath);
            auto m = db.scalar("SELECT manifest FROM project_meta").toString();
            CHECK(m.contains(";cm7-spatial=1"));
            m.replace(";cm7-spatial=1", "");
            db.run("UPDATE project_meta SET manifest=?", {m});
        }
        store.close();
        error("IncompatibleSchema", [&] { store.open(forgedPath, "Saved"); });

        // 16. Upstream mutation, branch ambiguity/missing detection, and explicit reference repair
        store.open(finalPath, "Saved");

        // Mutate skYZ so that branch/0 is lost or changed
        sketch::Sketch mutatedYZ = skYZ;
        mutatedYZ.entities.clear();
        mutatedYZ.constraints.clear();
        auto mb1 = mutatedYZ.point({0.020, 0.020});
        auto mb2 = mutatedYZ.point({0.090, 0.090});
        mutatedYZ.line(mb1, mb2);

        auto mutatedSos = sos;
        mutatedSos.sketchOnSketch->branch.token = QStringLiteral("branch/stale");
        auto mutatedModel = modelDef;
        mutatedModel.sketches = sketch::encode({baseSketch, skXY, mutatedYZ});
        mutatedModel.derived_curves = model::encodeDerivedCurves({helix, mutatedSos});

        // Calling SetModelSupports directly with mutatedModel fails with ReferenceMissing on branch
        error("ReferenceMissing", [&] {
            store.execute(command("SetModelSupports", fields({{"model", mutatedModel.map()}})));
        });

        // Inspect candidate branches using supportReferenceCandidates
        auto inspection =
            store.supportReferenceCandidates(document, mutatedModel.map(), sos.id, 0, "branch");
        CHECK(inspection.value("state") == "Missing" || inspection.value("state") == "Ambiguous");
        auto candidates = inspection.value("replacements").toArray();
        CHECK(!candidates.empty());
        auto chosenBranch = candidates[0].toMap();

        // Test forged replacement and stale fingerprint
        auto forgedBranch = fields({{"model", mutatedModel.map()},
                                    {"owner", sos.id},
                                    {"path", "branch"},
                                    {"replacement", chosenBranch},
                                    {"fingerprint", QByteArray("stale-fingerprint")}});
        error("StaleRevision",
              [&] { store.execute(command("ResolveSupportReference", forgedBranch)); });
        CHECK(store.revision() == revision);

        auto invalidReplacement = forgedBranch;
        invalidReplacement.insert(QStringLiteral("fingerprint"), inspection.value("fingerprint"));
        invalidReplacement.insert(QStringLiteral("replacement"),
                                  fields({{"index", 999}, {"token", "branch/999"}}));
        error("InvalidInput",
              [&] { store.execute(command("ResolveSupportReference", invalidReplacement)); });
        CHECK(store.revision() == revision);

        // Resolve branch reference atomically
        auto repairPayload = fields({{"model", mutatedModel.map()},
                                     {"owner", sos.id},
                                     {"path", "branch"},
                                     {"replacement", chosenBranch},
                                     {"fingerprint", inspection.value("fingerprint")}});
        store.execute(command("ResolveSupportReference", repairPayload));
        CHECK(store.revision() > revision);

        // Verify downstream NormalCurve datum plane rebuilt atomically
        auto repairedSup = store.featureState(document).value("supports").toMap();
        auto repairedParsed = model::SupportDefinition::parse(repairedSup);
        CHECK(repairedParsed.derived_curves.size() == 2);
        auto evalResult = geometry::evaluateSupports(repairedParsed);
        CHECK(evalResult.planes.contains(normalPlane.id));

        // 17. Explicit deletion of derived_curves via explicit empty array []
        auto deletionModel = repairedParsed;
        deletionModel.derived_curves = QCborArray{};
        // NormalPlane depends on sos curve, so deleting derived_curves must fail ReferenceMissing
        error("ReferenceMissing", [&] {
            store.execute(command("SetModelSupports", fields({{"model", deletionModel.map()},
                                                              {"derived_curves", QCborArray{}}})));
        });

        // Remove normalPlane so deletion is valid
        deletionModel.datums = {axisDatum};
        deletionModel.history = {axisId, baseSketch.id, skXY.id, mutatedYZ.id, spatial.id};
        store.execute(command("SetModelSupports", fields({{"model", deletionModel.map()},
                                                          {"derived_curves", QCborArray{}}})));
        auto afterDeletion = store.featureState(document).value("supports").toMap();
        CHECK(afterDeletion.value("derived_curves").toArray().isEmpty());
        // spatial_sketches is preserved!
        CHECK(afterDeletion.value("spatial_sketches").toArray().size() == 1);

        puts("cm7_store passed");
    });
}
