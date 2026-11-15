#include "../module4/Fixture.hpp"
#include "persistence/ProjectStore.hpp"
#include "support/check.hpp"
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
using namespace cad;
using sketch::fields;
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    return run([&] {
        QTemporaryDir root(QDir::tempPath() + "/cad-module7-b3-XXXXXX"),
            files(QDir::tempPath() + "/cad-module7-files-XXXXXX");
        for (auto *r : {&root, &files}) {
            CHECK(r->isValid());
            QFile marker(r->filePath(".cad-test-root"));
            CHECK(marker.open(QIODevice::WriteOnly));
            marker.write("disposable\n");
        }
        QFile fixture(QString(CAD_SOURCE_ROOT) + "/tests/fixtures/B3.json");
        CHECK(fixture.open(QIODevice::ReadOnly));
        auto bytes = fixture.readAll();
        auto recipe = QJsonDocument::fromJson(bytes).object()["recipe"].toObject();
        storage::ProjectStore store(root.path());
        auto project = newId();
        QString doc;
        store.create(project);
        QElapsedTimer clock;
        clock.start();
        auto command = [&](QString kind, QCborMap p = QCborMap{}) {
            wire::Envelope e;
            e.command = kind;
            e.context.project = store.project();
            e.context.document = doc;
            e.revision = store.revision();
            e.payload = p;
            return e;
        };
        auto create = [&](QString type, QString name) {
            doc = newId();
            store.execute(command("CreateDocument", fields({{"id", doc},
                                                            {"type", type},
                                                            {"name", name},
                                                            {"units", "mm"},
                                                            {"properties_entity", newId()}})));
            return doc;
        };
        QStringList definitions;
        for (auto value : recipe["definitions"].toArray()) {
            definitions << create("Part", "B1 definition " + QString::number(definitions.size()));
            fixture4::B1 b1;
            // Keep B1's cut, four holes and final fillet; vary only the frozen recipe width.
            b1.outer.entities.clear();
            b1.outer.constraints.clear();
            b1.outer.rectangle({0, 0}, {value.toObject()["width_mm"].toDouble() * .001, .05});
            store.execute(command("SetSketches", fields({{"sketches", b1.sketches()}})));
            store.execute(command("SetFeatures", fields({{"features", b1.features()}})));
            auto edge = fixture4::filletEdge(store.evaluatePart(doc).topology());
            CHECK(!edge.isEmpty());
            auto definitions = features::parse(b1.features());
            definitions.push_back(b1.fillet(edge));
            store.execute(
                command("SetFeatures", fields({{"features", features::encode(definitions)}})));
            CHECK(!store.evaluatePart(doc).stale);
        }
        auto assemblyId = create("Assembly", "B3 hundred occurrences");
        assembly::Model model;
        QMap<QString, QString> ids;
        for (auto value : recipe["occurrences"].toArray()) {
            auto r = value.toObject();
            assembly::Occurrence o;
            o.definition = definitions[r["definition"].toInt()];
            o.name = r["id"].toString();
            o.grounded = o.name == recipe["grounded"].toString();
            auto t = r["translation_mm"].toArray();
            for (int k = 0; k < 3; ++k)
                o.pose.translation[k] = t[k].toDouble() * .001;
            ids[o.name] = o.id;
            model.occurrences.push_back(o);
        }
        for (auto value : recipe["mates"].toArray()) {
            auto r = value.toObject();
            assembly::Mate m;
            m.kind = "Distance";
            m.first.occurrence = ids[r["from"].toString()];
            m.second.occurrence = ids[r["to"].toString()];
            m.value = r["distance_mm"].toDouble() * .001;
            model.mates.push_back(m);
        }
        CHECK(model.occurrences.size() == 100 && model.mates.size() == 99 &&
              definitions.size() == 10);
        store.execute(command("SetAssembly", fields({{"assembly", model.map()}})));
        model = store.assemblyModel(doc);
        auto reply = store.assemblyResult(doc);
        auto solution = reply.value("assembly_result").toMap();
        CHECK(solution.value("valid").toBool());
        CHECK(solution.value("freedom").toInteger() == 495);
        auto mesh = unpack(reply.value("display_manifest").toMap(),
                           reply.value("_display_bytes").toByteArray());
        CHECK(mesh->instances.size() == 100);
        CHECK(reply.value("mesh_definitions").toInteger() == 10);
        CHECK(reply.value("bom").toArray().size() == 10);
        for (auto item : reply.value("bom").toArray())
            CHECK(item.toMap().value("quantity").toInteger() == 10);
        qint64 visible = 0;
        for (auto &i : mesh->instances)
            visible += i.triangleCount;
        CHECK(visible <= 1000000);
        for (int i = 10; i < 100; ++i)
            CHECK(mesh->instances[i].firstTriangle == mesh->instances[i % 10].firstTriangle);
        auto unloaded = model;
        for (auto &o : unloaded.occurrences)
            o.unloaded = true;
        unloaded.mates.clear();
        store.execute(command("SetAssembly", fields({{"assembly", unloaded.map()}})));
        auto coarse = store.assemblyResult(doc);
        CHECK(coarse.value("exact_definitions").toInteger() == 0);
        CHECK(coarse.value("display_manifest").toMap().value("instances").toArray().size() == 100);
        store.execute(command("Undo"));
        CHECK(store.assemblyModel(doc).map() == model.map());
        store.trimResults();
        CHECK(store.assemblyResult(doc).value("mesh_definitions").toInteger() == 10);
        auto saved = files.filePath("B3.cadproj");
        store.save(command("SaveProject"), saved);
        auto revision = store.revision();
        store.close();
        store.recover(project);
        CHECK(store.revision() == revision);
        CHECK(store.assemblyModel(doc).map() == model.map());
        store.close();
        store.open(saved, "Saved");
        CHECK(store.assemblyModel(assemblyId).map() == model.map());
        CHECK(store.assemblyResult(assemblyId)
                  .value("display_manifest")
                  .toMap()
                  .value("instances")
                  .toArray()
                  .size() == 100);
        if (argc > 1) {
            QFile output(argv[1]);
            CHECK(output.open(QIODevice::WriteOnly));
            output.write(
                QJsonDocument(
                    QJsonObject{
                        {"status", "Pass"},
                        {"fixture_sha256",
                         QString::fromLatin1(
                             QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex())},
                        {"occurrences", 100},
                        {"definitions", 10},
                        {"mates", 99},
                        {"remaining_freedom", 495},
                        {"unique_display_triangles", mesh->indices.size() / 3},
                        {"visible_triangles", visible},
                        {"seconds", clock.elapsed() / 1000.},
                        {"scope", "Frozen B3 correctness, shared geometry, unload coarse cache and "
                                  "recovery; not full physical memory/FPS/platform acceptance"}})
                    .toJson());
        }
        std::cout << "B3: 100 occurrences, 10 B1 exact definitions, 99 mates, 495 free dimensions, "
                     "shared display, coarse unload, undo, save and recovery passed\n";
    });
}
