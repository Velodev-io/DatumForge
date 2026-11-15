#include "persistence/ProjectStore.hpp"
#include "support/check.hpp"
#include <QBuffer>
#include <QCoreApplication>
#include <QFile>
#include <QTemporaryDir>

#include <iostream>
using namespace cad;
using namespace cad::storage;
QCborMap map(std::initializer_list<std::pair<QString, QCborValue>> values) {
    QCborMap m;
    for (const auto &item : values)
        m.insert(item.first, item.second);
    return m;
}
void mark(QString root) {
    QFile f(root + "/.cad-test-root");
    CHECK(f.open(QIODevice::WriteOnly));
    f.write("disposable\n");
}
wire::Envelope command(ProjectStore &store, QString type, QCborMap payload = {}) {
    wire::Envelope e;
    e.epoch = newId();
    e.context.project = store.project();
    e.context.document = store.active().isEmpty() ? newId() : store.active();
    e.command = type;
    e.revision = store.revision();
    e.payload = payload;
    return e;
}
QString addDocument(ProjectStore &store, QString kind) {
    auto id = newId();
    store.execute(command(store, "CreateDocument",
                          map({{"id", id},
                               {"type", kind},
                               {"name", kind + " document"},
                               {"units", "mm"},
                               {"properties_entity", newId()}})));
    return id;
}
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    return run([&] {
        Database bounded(":memory:");
        constexpr auto largeQuery =
            "WITH RECURSIVE n(x) AS (VALUES(1) UNION ALL SELECT x+1 FROM n WHERE x<64) "
            "SELECT zeroblob(1048576) FROM n";
        bool boundedResult = false;
        try {
            bounded.rows(largeQuery);
        } catch (const domain::Error &error) {
            boundedResult = error.code == "ResourceLimit";
        }
        CHECK(boundedResult);
        int streamedRows = 0;
        bounded.forEachRow(largeQuery, [&](const QVariantList &row) {
            CHECK(row[0].toByteArray().size() == 1048576);
            ++streamedRows;
        });
        CHECK(streamedRows == 64);
        QTemporaryDir root(QDir::tempPath() + "/cad-module2-store-XXXXXX");
        CHECK(root.isValid());
        mark(root.path());
        QTemporaryDir files(QDir::tempPath() + "/cad-module2-saved-XXXXXX");
        CHECK(files.isValid());
        mark(files.path());
        bool failPublication = false;
        StoreOptions options;
        options.boundary = [&](QString stage) {
            if (failPublication && stage == "snapshot_validated")
                throw domain::Error("PermissionDenied", "Injected pre-publication failure");
        };
        options.historyEntries = 3;
        ProjectStore store(root.path(), options);
        auto project = newId();
        store.create(project);
        auto part = addDocument(store, "Part"), assembly = addDocument(store, "Assembly"),
             drawing = addDocument(store, "Drawing");
        CHECK(store.revision() == 3);
        CHECK(store.state().value("documents").toArray().size() == 3);
        auto switcher = command(store, "SwitchDocument");
        switcher.context.document = part;
        store.execute(switcher);
        CHECK(store.active() == part);
        auto edit = command(store, "SetParameter",
                            map({{"name", "Depth"}, {"dimension", "length"}, {"value_si", .02}}));
        store.execute(edit);
        CHECK(store.revision() == 4);
        edit.epoch = newId();
        edit.request = newId();
        edit.context.session = newId();
        store.execute(edit);
        CHECK(store.revision() == 4);
        bool rejected = false;
        try {
            auto wrong = edit;
            wrong.payload.insert(QStringLiteral("value_si"), .03);
            store.execute(wrong);
        } catch (const domain::Error &e) {
            rejected = e.code == "InvalidInput";
        }
        CHECK(rejected);
        store.execute(command(store, "Undo"));
        CHECK(store.revision() == 5);
        CHECK(store.state()
                  .value("documents")
                  .toArray()[0]
                  .toMap()
                  .value("parameters")
                  .toArray()
                  .empty());
        store.execute(command(store, "Redo"));
        CHECK(store.revision() == 6);
        auto file = files.path() + "/saved.cadproj";
        auto save = command(store, "SaveProject");
        store.save(save, file);
        CHECK(store.state().value("saved_revision").toInteger() == 6);
        CHECK(!QFileInfo::exists(file + "-wal"));
        failPublication = true;
        for (int attempt = 0; attempt < 40; ++attempt) {
            bool denied = false;
            try {
                store.save(command(store, "SaveProject"), file);
            } catch (const domain::Error &error) {
                denied = error.code == "PermissionDenied";
            }
            CHECK(denied);
            CHECK(store.revision() == 6 && store.state().value("saved_revision").toInteger() == 6);
        }
        failPublication = false;
        store.save(command(store, "SaveProject"), file);

        store.execute(command(store, "RenameDocument", map({{"name", "Edited"}})));
        CHECK(store.revision() == 7);
        auto savedBefore = QFile(file);
        CHECK(savedBefore.open(QIODevice::ReadOnly));
        auto savedHash =
            QCryptographicHash::hash(savedBefore.readAll(), QCryptographicHash::Sha256);
        savedBefore.close();
        store.close();
        CHECK(store.inspect(file).value("recoverable_revision").toInteger() == 7);
        store.recover(project);
        CHECK(store.revision() == 7);
        CHECK(store.state().value("saved_revision").toInteger() == 6);
        CHECK(store.outcome(edit.operation).value("durable").toBool());
        auto copy = files.path() + "/copy.cadproj";
        auto saveAs = command(store, "SaveAs");
        auto copied = newId();
        saveAs.payload.insert(QStringLiteral("new_project_id"), copied);
        store.save(saveAs, copy, true);
        CHECK(store.project() == copied);
        auto copiedRevision = store.revision();
        store.save(saveAs, copy, true);
        CHECK(store.project() == copied && store.revision() == copiedRevision);
        CHECK(store.state().value("documents").toArray()[0].toMap().value("id").toString() == part);
        CHECK(savedBefore.open(QIODevice::ReadOnly));
        CHECK(QCryptographicHash::hash(savedBefore.readAll(), QCryptographicHash::Sha256) ==
              savedHash);
        savedBefore.close();
        ProjectStore original(root.path());
        original.open(file, "Saved");
        CHECK(original.project() == project);
        CHECK(original.revision() == 6);
        bool locked = false;
        try {
            ProjectStore second(root.path());
            second.open(file, "Saved");
        } catch (const domain::Error &e) {
            locked = e.code == "ProjectLocked";
        }
        CHECK(locked);
        QByteArray bytes(4 << 20, 'A');
        QBuffer source(&bytes);
        CHECK(source.open(QIODevice::ReadOnly));
        auto attach =
            command(store, "AttachObject",
                    map({{"size", bytes.size()},
                         {"expected_sha256",
                          QString::fromLatin1(
                              QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex())},
                         {"classification", "authoritative"}}));
        auto initialRevision = store.revision();
        int refusedStreams = 0;
        for (int scenario = 0; scenario < 2; ++scenario) {
            QByteArray tiny("ABCD");
            QBuffer input(&tiny);
            CHECK(input.open(QIODevice::ReadOnly));
            auto requested = scenario == 0 ? qint64(limits::object) + 1 : qint64(5);
            auto invalid = command(store, "AttachObject",
                                   map({{"size", requested},
                                        {"expected_sha256", QString(64, '0')},
                                        {"classification", "authoritative"}}));
            try {
                store.attach(invalid, input, requested);
            } catch (const std::exception &) {
                ++refusedStreams;
            }
            CHECK(store.revision() == initialRevision);
        }
        CHECK(refusedStreams == 2);
        auto blob = store.attach(attach, source, bytes.size());
        auto attachedRevision = store.revision();
        CHECK(store.attach(attach, source, bytes.size()) == blob);
        CHECK(store.revision() == attachedRevision);
        auto job = newId();
        store.retain("job", job, blob);
        store.execute(command(store, "DetachObject", map({{"object", blob}})));
        store.collect();
        {
            Database current(store.workingPath(), true);
            CHECK(current.scalar("SELECT count(*) FROM objects").toInt() == 1);
        }
        store.release("job", job);
        for (int i = 0; i < 5; ++i)
            store.execute(
                command(store, "RenameDocument", map({{"name", QString("edit%1").arg(i)}})));
        store.collect();
        {
            Database current(store.workingPath(), true);
            CHECK(current.scalar("SELECT count(*) FROM objects").toInt() == 0);
            CHECK(current.scalar("SELECT count(*) FROM history").toInt() == 3);
        }
        store.close();
        original.close();
        std::cout
            << "project/document/SI/dedup/undo/save/reopen/SaveAs/locking/stream/retention PASS\n";
    });
}
