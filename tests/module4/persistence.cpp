#include "Fixture.hpp"
#include "geometry/occt/Part.hpp"
#include "persistence/ProjectStore.hpp"
#include "support/check.hpp"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
using namespace cad;
using sketch::fields;
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    return run([] {
        QTemporaryDir root(QDir::tempPath() + "/cad-module4-storage-XXXXXX"),
            files(QDir::tempPath() + "/cad-module4-files-XXXXXX");
        for (auto *r : {&root, &files}) {
            CHECK(r->isValid());
            QFile mark(r->filePath(".cad-test-root"));
            CHECK(mark.open(QIODevice::WriteOnly));
            mark.write("disposable\n");
        }
        storage::ProjectStore store(root.path());
        auto project = newId(), doc = newId();
        store.create(project);
        auto command = [&](QString cmd, QCborMap payload = QCborMap{}) {
            wire::Envelope e;
            e.context.project = project;
            e.context.document = doc;
            e.command = cmd;
            e.payload = payload;
            e.revision = store.revision();
            return e;
        };
        store.execute(command("CreateDocument", fields({{"id", doc},
                                                        {"type", "Part"},
                                                        {"name", "B1 part"},
                                                        {"units", "mm"},
                                                        {"properties_entity", newId()}})));
        fixture4::B1 b;
        store.execute(command("SetSketches", fields({{"sketches", b.sketches()}})));
        auto definitions = b.features();
        auto part = kernel::Part::build(b.sketches(), definitions);
        auto edge = fixture4::filletEdge(part.topology());
        CHECK(!edge.isEmpty());
        definitions.append(b.fillet(edge).map());
        // Each prefix independently validates, commits, saves and reopens through actual schema
        // validation.
        QCborArray prefix;
        for (auto f : definitions) {
            prefix.append(f);
            auto e = command("SetFeatures", fields({{"features", prefix}}));
            auto before = store.revision();
            store.execute(e);
            CHECK(store.revision() == before + 1);
            store.execute(e);
            CHECK(store.revision() == before + 1);
            auto saved = files.filePath("prefix.cadproj");
            store.save(command("SaveProject"), saved);
            storage::ProjectStore other(root.path());
            auto inspect = other.inspect(saved);
            CHECK(inspect.value("saved_revision").toInteger() == store.revision());
        }
        part = kernel::Part::build(store.sketches(doc), store.features(doc));
        near(part.measure(store.revision()).value("volume_m3").toDouble(), 35488.76110196153e-9,
             1e-12);
        auto old = store.revision();
        auto savedDefs = store.features(doc);
        auto bad = definitions;
        auto f = bad.last().toMap();
        auto p = f.value("parameters").toMap();
        p.insert(QStringLiteral("size"), 2.);
        f.insert(QStringLiteral("parameters"), p);
        bad[bad.size() - 1] = f;
        rejects([&] { store.execute(command("SetFeatures", fields({{"features", bad}}))); });
        CHECK(store.revision() == old);
        CHECK(store.features(doc) == savedDefs);
        auto stale = command("SetFeatures", fields({{"features", definitions}}));
        stale.revision--;
        rejects([&] { store.execute(stale); });
        CHECK(store.revision() == old);
        // Sketch changes replay ordered features atomically; rollback and durable undo retain IDs.
        for (auto &e : b.outer.entities)
            if (e.kind == "Point" && std::abs(e.point.x - .08) < 1e-10)
                e.point.x = .09;
        store.execute(command("SetSketches", fields({{"sketches", b.sketches()}})));
        near(kernel::Part::build(store.sketches(doc), store.features(doc))
                 .measure(1)
                 .value("volume_m3")
                 .toDouble(),
             40488.76110196153e-9, 1e-12);
        store.execute(command("Undo"));
        near(kernel::Part::build(store.sketches(doc), store.features(doc))
                 .measure(1)
                 .value("volume_m3")
                 .toDouble(),
             35488.76110196153e-9, 1e-12);
        store.execute(command("Redo"));
        CHECK(store.features(doc) == savedDefs);
        auto accepted = store.revision();
        store.close();
        store.recover(project);
        CHECK(store.revision() == accepted);
        CHECK(store.features(doc) == savedDefs);
        auto path = files.filePath("final.cadproj");
        store.save(command("SaveProject"), path);
        store.close();
        store.open(path, "Saved");
        CHECK(store.features(doc) == savedDefs);
        CHECK(store.sketches(doc) == b.sketches());
        auto changed = store.sketches(doc);
        changed.removeLast();
        rejects([&] { store.execute(command("SetSketches", fields({{"sketches", changed}}))); });
        CHECK(store.features(doc) == savedDefs);
        // Existing release and unmarked roots remain refused.
        QTemporaryDir unmarked(QDir::tempPath() + "/cad-module4-unmarked-XXXXXX");
        rejects([&] { storage::ProjectStore forbidden(unmarked.path()); });
    });
}
