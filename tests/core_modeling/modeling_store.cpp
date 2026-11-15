#include "ModelingFixture.hpp"
#include "persistence/ProjectStore.hpp"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
using namespace cad;
using namespace modeling_fixture;
using support_fixture::error;
namespace {
QByteArray bytes(QString path) {
    QFile file(path);
    CHECK(file.open(QIODevice::ReadOnly));
    return file.readAll();
}
void mark(QTemporaryDir &root) {
    CHECK(root.isValid());
    QFile f(root.filePath(".cad-test-root"));
    CHECK(f.open(QIODevice::WriteOnly));
    f.write("disposable\n");
}
} // namespace
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    return run([] {
        QTemporaryDir root(QDir::tempPath() + "/cad-module5-cm5-store-XXXXXX"),
            files(QDir::tempPath() + "/cad-module5-cm5-saved-XXXXXX");
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
                                                        {"name", "Durable solids"},
                                                        {"units", "mm"}})));
        auto old = files.filePath("legacy.cadproj");
        store.save(command("SaveProject"), old);
        auto original = bytes(old);
        sketch::Sketch s, limit;
        s.rectangle({0, 0}, {.02, .01});
        limit.frame.origin[2] = .012;
        auto d = base({s, limit});
        d.document = document;
        auto f = extrude(s.id);
        f.insert(QStringLiteral("direction1"),
                 fields({{"mode", "UpToSurface"},
                         {"reference", support_fixture::ref(limit.id, "Plane").map()},
                         {"draft", blind().value("draft")}}));
        auto commit =
            command("SetModelSupports", fields({{"model", d.map()}, {"intents", QCborArray{f}}}));
        store.execute(commit);
        auto revision = store.revision();
        CHECK(store.featureState(document).value("modeling").toMap().value("intents").toArray() ==
              QCborArray{f});
        near(store.previewSupports(document, d.map())
                 .value("measurement")
                 .toMap()
                 .value("volume_m3")
                 .toDouble(),
             .02 * .01 * .012, 1e-12);
        CHECK(store.previewSupports(document, d.map(), {}, QCborArray{})
                  .value("part_empty")
                  .toBool());
        auto provenance =
            store.featureState(document).value("modeling").toMap().value("provenance").toArray();
        CHECK(provenance.size() == 1);
        near(store.evaluatePart(document).measure(0).value("volume_m3").toDouble(),
             .02 * .01 * .012, 1e-12);
        store.execute(commit);
        CHECK(store.revision() == revision);
        store.execute(command("Undo"));
        CHECK(store.evaluatePart(document).bodies.empty());
        store.execute(command("Redo"));
        CHECK(
            store.featureState(document).value("modeling").toMap().value("provenance").toArray() ==
            provenance);
        // An older support editor must preserve intents when updating a support.
        limit.frame.origin[2] = .018;
        d.sketches = sketch::encode({s, limit});
        store.execute(command("SetModelSupports", fields({{"model", d.map()}})));
        near(store.evaluatePart(document).measure(0).value("volume_m3").toDouble(),
             .02 * .01 * .018, 1e-12);
        CHECK(store.featureState(document).value("modeling").toMap().value("intents").toArray() ==
              QCborArray{f});
        revision = store.revision();
        auto accepted = store.featureState(document);
        // Storage boundary: invalid intents type rejected without modifying database state
        auto badIntents =
            command("SetModelSupports", fields({{"model", d.map()}, {"intents", 12345}}));
        error("InvalidInput", [&] { store.execute(badIntents); });
        CHECK(store.revision() == revision && store.featureState(document) == accepted);
        auto bad = d;
        limit.frame.origin[2] = -.01;
        bad.sketches = sketch::encode({s, limit});
        rejects(
            [&] { store.execute(command("SetModelSupports", fields({{"model", bad.map()}}))); });
        CHECK(store.revision() == revision && store.featureState(document) == accepted);
        exchange::Control canceled;
        canceled.canceled = [] { return true; };
        error("Canceled", [&] {
            store.execute(command("SetModelSupports", fields({{"model", d.map()}})), canceled);
        });
        CHECK(store.revision() == revision);
        error("UpgradeCopyRequired", [&] { store.save(command("SaveProject"), old); });
        CHECK(bytes(old) == original);
        auto path = files.filePath("modeling.cadproj");
        store.save(command("SaveProject"), path);
        store.close();
        store.open(path, "Saved");
        CHECK(store.featureState(document) == accepted);
        near(store.evaluatePart(document).measure(0).value("volume_m3").toDouble(),
             .02 * .01 * .018, 1e-12);
        store.close();
        store.recover(project);
        CHECK(store.featureState(document) == accepted);
        auto deletion =
            command("SetModelSupports", fields({{"model", d.map()}, {"intents", QCborArray{}}}));
        store.execute(deletion);
        CHECK(store.evaluatePart(document).bodies.empty());
        store.execute(command("Undo"));
        CHECK(store.featureState(document) == accepted);
        store.close();
        store.open(path, "Saved");
        store.execute(command("Undo"));
        store.execute(command("Undo"));
        CHECK(!store.featureState(document).contains(QStringLiteral("modeling")));
        auto historyPath = files.filePath("history-only.cadproj");
        store.save(command("SaveProject"), historyPath);
        store.close();
        auto forge = [&](QString originalPath, QString name, bool downgrade) {
            auto forged = files.filePath(name);
            CHECK(QFile::copy(originalPath, forged));
            {
                storage::Database db(forged);
                if (downgrade) {
                    auto m = db.scalar("SELECT manifest FROM project_meta").toString();
                    CHECK(m.endsWith(";modeling-intents=1"));
                    m.chop(QString(";modeling-intents=1").size());
                    db.run("UPDATE project_meta SET manifest=?", {m});
                } else {
                    auto records = db.rows("SELECT entity_id,definition FROM features");
                    CHECK(records.size() == 1);
                    auto collection = wire::decode(records[0][1].toByteArray());
                    auto metadata = collection.value("modeling").toMap();
                    auto provenance = metadata.value("provenance").toArray();
                    auto record = provenance[0].toMap();
                    record.insert(QStringLiteral("outputs"), QCborArray{newId()});
                    provenance[0] = record;
                    metadata.insert(QStringLiteral("provenance"), provenance);
                    collection.insert(QStringLiteral("modeling"), metadata);
                    db.run(
                        "UPDATE features SET definition=? WHERE entity_id=?",
                        {QCborValue(collection).toCbor(QCborValue::SortKeysInMaps), records[0][0]});
                }
            }
            auto before = bytes(forged);
            error(downgrade ? "IncompatibleSchema" : "InvalidProject",
                  [&] { store.open(forged, "Saved"); });
            CHECK(bytes(forged) == before);
        };
        forge(path, "provenance-forged.cadproj", false);
        forge(path, "active-downgrade.cadproj", true);
        forge(historyPath, "history-downgrade.cadproj", true);
        CHECK(bytes(old) == original);
    });
}
