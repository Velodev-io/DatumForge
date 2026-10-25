#include "persistence/ProjectStore.hpp"
#include "support/check.hpp"
#include <BRepPrimAPI_MakeBox.hxx>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <numbers>
using namespace cad;
using sketch::fields;
namespace {
QByteArray bytes(QString path) {
    QFile f(path);
    CHECK(f.open(QIODevice::ReadOnly));
    return f.readAll();
}
QString hash(QString path) {
    return QString::fromLatin1(
        QCryptographicHash::hash(bytes(path), QCryptographicHash::Sha256).toHex());
}
} // namespace
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    return run([] {
        QTemporaryDir root(QDir::tempPath() + "/cad-module6-store-XXXXXX"),
            files(QDir::tempPath() + "/cad-module6-files-XXXXXX");
        for (auto *dir : {&root, &files}) {
            CHECK(dir->isValid());
            QFile marker(dir->filePath(".cad-test-root"));
            CHECK(marker.open(QIODevice::WriteOnly));
            marker.write("disposable\n");
        }
        bool failExport = false, failCommit = false, failAfterExport = false;
        storage::StoreOptions options;
        options.boundary = [&](QString stage) {
            if ((failExport && stage == "export_before_publication") ||
                (failCommit && stage == "before_commit") ||
                (failAfterExport && stage == "export_after_publication"))
                throw domain::Error("StorageFull", "injected full volume");
        };
        storage::ProjectStore store(root.path(), options);
        auto project = newId(), doc = newId();
        store.create(project);
        auto command = [&](QString type, QCborMap payload = QCborMap{}) {
            wire::Envelope e;
            e.context.project = store.project();
            e.context.document = doc;
            e.revision = store.revision();
            e.command = type;
            e.payload = payload;
            return e;
        };
        store.execute(command("CreateDocument", fields({{"id", doc},
                                                        {"type", "Part"},
                                                        {"name", "Imported part"},
                                                        {"units", "mm"},
                                                        {"properties_entity", newId()}})));
        auto source = files.filePath("original.step");
        exchange::write(BRepPrimAPI_MakeBox(10, 20, 30).Shape(), source, "STEP", "mm", .2);
        auto sourceHash = hash(source);
        auto request = command("ImportGeometry", fields({{"format", "STEP"},
                                                         {"units", "File"},
                                                         {"healing", "None"},
                                                         {"feature_id", newId()},
                                                         {"name", "Imported body"},
                                                         {"expected_sha256", sourceHash}}));
        auto before = store.revision();
        failCommit = true;
        rejects([&] { store.importGeometry(request, source); });
        failCommit = false;
        CHECK(store.revision() == before && store.features(doc).empty());
        {
            storage::Database db(store.workingPath(), true);
            CHECK(db.scalar("SELECT count(*) FROM objects").toInt() == 0);
        }
        bool canceled = true;
        rejects([&] { store.importGeometry(request, source, {[&] { return canceled; }, {}}); });
        CHECK(store.revision() == before);
        auto changed = request;
        changed.payload.insert(QStringLiteral("expected_sha256"), QString(64, '0'));
        rejects([&] { store.importGeometry(changed, source); });
        CHECK(store.revision() == before);
        store.importGeometry(request, source);
        CHECK(store.revision() == before + 1);
        store.importGeometry(request, source);
        CHECK(store.revision() == before + 1);
        rejects([&] { store.importGeometry(changed, source); });
        auto feature = store.features(doc).first().toMap();
        CHECK(feature.value("kind") == "Imported");
        near(store.evaluatePart(doc).measure(store.revision()).value("volume_m3").toDouble(), 6e-6,
             1e-12);
        CHECK(hash(source) == sourceHash);
        store.execute(command("Undo"));
        CHECK(store.features(doc).empty());
        store.collect();
        store.trimResults();
        store.execute(command("Redo"));
        near(store.evaluatePart(doc).measure(store.revision()).value("volume_m3").toDouble(), 6e-6,
             1e-12);
        // Imported geometry supports subsequent exact features and dependency-aware history.
        features::Feature hole;
        hole.kind = "Hole";
        hole.parameters = fields({{"target", feature.value("id")},
                                  {"origin", QCborArray{.005, .01, .03}},
                                  {"direction", QCborArray{0., 0., -1.}},
                                  {"diameter", .002},
                                  {"depth", .04},
                                  {"through", true}});
        auto definitions = store.features(doc);
        definitions.append(hole.map());
        store.execute(command("SetFeatures", fields({{"features", definitions}})));
        near(store.evaluatePart(doc).measure(1).value("volume_m3").toDouble(),
             (6000 - 30 * std::numbers::pi) * 1e-9, 1e-12);
        store.execute(command("Undo"));
        auto exportRequest = [&](QString kind, QString unit = "mm") {
            return command("ExportGeometry", fields({{"format", kind},
                                                     {"units", unit},
                                                     {"body", ""},
                                                     {"deflection_mm", .2},
                                                     {"overwrite", true}}));
        };
        auto output = files.filePath("output.step");
        {
            QFile f(output);
            CHECK(f.open(QIODevice::WriteOnly));
            f.write("prior valid destination\n");
        }
        auto original = bytes(output);
        failExport = true;
        rejects([&] { store.exportGeometry(exportRequest("STEP"), output); });
        failExport = false;
        CHECK(bytes(output) == original);
        canceled = false;
        rejects([&] {
            store.exportGeometry(exportRequest("STEP"), output,
                                 {[&] { return canceled; },
                                  [&](QString, int p) {
                                      if (p >= 90)
                                          canceled = true;
                                  }});
        });
        CHECK(bytes(output) == original);
        auto stale = exportRequest("STEP");
        stale.revision--;
        rejects([&] { store.exportGeometry(stale, output); });
        CHECK(bytes(output) == original);
        for (auto kind : {QString("STEP"), QString("IGES"), QString("STL")}) {
            auto path = files.filePath("output." + (kind == "STEP"   ? QString("step")
                                                    : kind == "IGES" ? QString("iges")
                                                                     : QString("stl")));
            auto report = store.exportGeometry(exportRequest(kind, "in"), path);
            CHECK(report.value("revision").toInteger() == store.revision());
            CHECK(report.value("sha256").toString() == hash(path));
        }
        auto uncertain = exportRequest("STEP");
        failAfterExport = true;
        bool uncertainReported = false;
        try {
            store.exportGeometry(uncertain, output);
        } catch (const domain::Error &error) {
            uncertainReported = error.code == "PublicationUncertain";
        }
        failAfterExport = false;
        CHECK(uncertainReported);
        CHECK(store.outcome(uncertain.operation).value("outcome") == "Unknown");
        near(exchange::invariants(exchange::read(output, "STEP").shape)
                 .value("volume_mm3")
                 .toDouble(),
             6000, 1e-4);
        auto recorded = exportRequest("STEP");
        auto firstReport = store.exportGeometry(recorded, output);
        auto outputHash = hash(output);
        CHECK(store.exportGeometry(recorded, output) == firstReport && hash(output) == outputHash);
        CHECK(store.outcome(recorded.operation).value("durable").toBool());
        auto reused = recorded;
        reused.payload.insert(QStringLiteral("units"), QString("in"));
        rejects([&] { store.exportGeometry(reused, output); });
        auto saved = files.filePath("original.cadproj");
        store.save(command("SaveProject"), saved);
        auto savedHash = hash(saved);
        auto closure = store.closure();
        CHECK(closure.value("complete").toBool());
        CHECK(closure.value("objects").toArray().size() == 1);
        auto copy = files.filePath("portable.cadproj"), copyProject = newId();
        auto copyRequest = command("PortableCopy",
                                   fields({{"new_project_id", copyProject}, {"policy", "Copied"}}));
        store.save(copyRequest, copy, true);
        CHECK(store.project() == project &&
              store.savedPath() == storage::canonicalDestination(saved));
        CHECK(hash(saved) == savedHash);
        {
            storage::Database db(copy, true);
            CHECK(db.scalar("SELECT id FROM project_meta").toString() == copyProject);
            auto intent = wire::decode(db.scalar("SELECT intent FROM commands WHERE operation_id=?",
                                                 {copyRequest.operation})
                                           .toByteArray());
            CHECK(intent.value("payload")
                      .toMap()
                      .value("copy_manifest")
                      .toMap()
                      .value("source_project")
                      .toString() == project);
            CHECK(db.scalar("PRAGMA integrity_check").toString() == "ok");
        }
        CHECK(QFile::remove(source));
        CHECK(QFile::remove(saved));
        store.close();
        store.open(copy, "Saved");
        store.trimResults();
        store.collect();
        CHECK(store.project() == copyProject && store.features(doc).first().toMap() == feature);
        near(store.evaluatePart(doc).measure(1).value("volume_m3").toDouble(), 6e-6, 1e-12);
        auto corrupt = files.filePath("corrupt.cadproj");
        CHECK(QFile::copy(copy, corrupt));
        {
            storage::Database damaged(corrupt);
            damaged.exec("UPDATE objects SET payload=zeroblob(size)");
        }
        rejects([&] { store.inspect(corrupt); });
        auto missing = files.filePath("missing.cadproj");
        CHECK(QFile::copy(copy, missing));
        {
            storage::Database damaged(missing);
            damaged.exec("PRAGMA foreign_keys=OFF; DELETE FROM object_refs; DELETE FROM objects");
        }
        rejects([&] { store.inspect(missing); });
        CHECK(store.project() == copyProject);
        auto accepted = store.revision();
        store.close();
        store.recover(copyProject);
        CHECK(store.revision() == accepted);
        rejects([&] {
            store.save(command("PortableCopy",
                               fields({{"new_project_id", newId()}, {"policy", "Copied"}})),
                       copy, true);
        });
        CHECK(store.project() == copyProject);
        // Stale history can be saved/ported, but exact export refuses its checkpoint.
        auto broken = store.features(doc);
        hole.parameters.insert(QStringLiteral("diameter"), .2);
        broken.append(hole.map());
        store.execute(command("CommitHistory", fields({{"features", broken}})));
        CHECK(store.evaluatePart(doc).stale);
        auto oldOutput = bytes(output);
        rejects([&] { store.exportGeometry(exportRequest("STEP"), output); });
        CHECK(bytes(output) == oldOutput);
        store.execute(command("Undo"));
        // Module 8 extends closure to empty Drawing documents.
        store.execute(command("CreateDocument", fields({{"id", newId()},
                                                        {"type", "Drawing"},
                                                        {"name", "Future drawing"},
                                                        {"units", "mm"},
                                                        {"properties_entity", newId()}})));
        CHECK(store.closure().value("complete").toBool());
        std::cout
            << "T-11/T-23/T-24/T-38/T-56: atomic import, source retention/undo/redo/GC, hole on "
               "imported body, canceled/full-disk destination preservation, valid-revision export "
               "and portable identity/manifest/source removal/recovery passed\n";
    });
}
