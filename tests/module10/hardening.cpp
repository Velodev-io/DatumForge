#include "persistence/ProjectStore.hpp"
#include "support/check.hpp"
#include <QBuffer>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <iostream>
using namespace cad;
using sketch::fields;
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    return run([] {
        QTemporaryDir root(QDir::tempPath() + "/cad-module10-hardening-XXXXXX"),
            files(QDir::tempPath() + "/cad-module10-files-XXXXXX");
        for (auto *r : {&root, &files}) {
            CHECK(r->isValid());
            QFile f(r->filePath(".cad-test-root"));
            CHECK(f.open(QIODevice::WriteOnly));
            f.write("disposable\n");
        }
        QString boundary;
        bool canceled = false;
        storage::StoreOptions options;
        options.historyEntries = 5;
        options.historyBytes = 8192;
        options.boundary = [&](QString stage) {
            if (stage == boundary)
                canceled = true;
        };
        storage::ProjectStore store(root.path(), options);
        store.create(newId());
        auto doc = newId();
        auto command = [&](QString c, QCborMap p = QCborMap{}) {
            wire::Envelope e;
            e.command = c;
            e.payload = p;
            e.revision = store.revision();
            e.context.project = store.project();
            e.context.document = doc;
            return e;
        };
        store.execute(command("CreateDocument", fields({{"id", doc},
                                                        {"type", "Part"},
                                                        {"name", "Hardening"},
                                                        {"units", "mm"},
                                                        {"properties_entity", newId()}})));
        auto parameter = [&](double n) {
            return command("SetParameter",
                           fields({{"name", "Depth"}, {"dimension", "length"}, {"value_si", n}}));
        };
        for (auto stage : {"Before compute", "After compute", "Before durable commit"}) {
            auto e = parameter(.01);
            auto old = store.revision();
            exchange::Control control;
            control.canceled = [&] { return canceled; };
            control.progress = [&](QString at, int) {
                if (at == stage)
                    canceled = true;
            };
            canceled = false;
            try {
                store.execute(e, control);
                CHECK(false);
            } catch (const domain::Error &error) {
                CHECK(error.code == "Canceled");
            }
            CHECK(store.revision() == old);
            CHECK(!store.outcome(e.operation).value("durable").toBool());
            store.execute(e);
            CHECK(store.revision() == old + 1);
            store.execute(e);
            CHECK(store.revision() == old + 1);
        }
        // Cancellation at the durable boundary: before commit aborts, after commit is too late.
        for (auto stage : {"before_compute", "after_compute", "before_commit", "after_commit"}) {
            boundary = stage;
            canceled = false;
            auto e = parameter(.02);
            auto old = store.revision();
            exchange::Control control;
            control.canceled = [&] { return canceled; };
            bool committed = QString(stage) == "after_commit";
            try {
                store.execute(e, control);
                CHECK(committed);
            } catch (const domain::Error &error) {
                CHECK(error.code == "Canceled" && !committed);
            }
            CHECK(store.revision() == old + int(committed));
            CHECK(store.outcome(e.operation).value("durable").toBool() == committed);
            boundary.clear();
            canceled = false;
            store.execute(e);
            CHECK(store.revision() == old + 1);
        }
        // Retained objects: document/history plus explicit job/export roots survive GC.
        QByteArray source(4096, 'S');
        QBuffer input(&source);
        CHECK(input.open(QIODevice::ReadOnly));
        auto hash = QString::fromLatin1(
            QCryptographicHash::hash(source, QCryptographicHash::Sha256).toHex());
        auto attach = command("AttachObject", fields({{"size", source.size()},
                                                      {"classification", "authoritative"},
                                                      {"expected_sha256", hash}}));
        store.attach(attach, input, source.size());
        auto jobRoot = newId(), exportRoot = newId();
        store.retain("job", jobRoot, hash);
        store.retain("export", exportRoot, hash);
        store.execute(command("DetachObject", fields({{"object", hash}})));
        store.collect();
        {
            storage::Database read(store.workingPath(), true);
            CHECK(read.scalar("SELECT count(*) FROM objects WHERE id=?", {hash}).toInt() == 1);
        }
        for (int i = 0; i < 100; ++i) {
            store.execute(parameter(.03 + i * 1e-5));
            store.execute(command("Undo"));
            store.execute(command("Redo"));
        }
        store.collect();
        {
            storage::Database read(store.workingPath(), true);
            CHECK(read.scalar("SELECT count(*) FROM history").toInt() <= 5);
            CHECK(read.scalar("SELECT sum(bytes) FROM history").toLongLong() <= 8192);
            CHECK(read.scalar("SELECT count(*) FROM objects WHERE id=?", {hash}).toInt() == 1);
        }
        store.release("job", jobRoot);
        store.collect();
        {
            storage::Database read(store.workingPath(), true);
            CHECK(read.scalar("SELECT count(*) FROM objects WHERE id=?", {hash}).toInt() == 1);
        }
        store.release("export", exportRoot);
        store.collect();
        {
            storage::Database read(store.workingPath(), true);
            CHECK(read.scalar("SELECT count(*) FROM objects WHERE id=?", {hash}).toInt() == 0);
        }
        auto saved = files.filePath("legacy-v1.cadproj");
        store.save(command("SaveProject"), saved);
        QFile original(saved);
        CHECK(original.open(QIODevice::ReadOnly));
        auto bytes = original.readAll();
        original.close();
        auto upgraded = files.filePath("upgraded.cadproj");
        storage::ProjectStore::migrationCopy(saved, upgraded);
        CHECK(QFileInfo(upgraded).isFile());
        CHECK(original.open(QIODevice::ReadOnly));
        CHECK(original.readAll() == bytes);
        original.close();
        auto failed = files.filePath("failed.cadproj");
        rejects([&] {
            storage::ProjectStore::migrationCopy(saved, failed, [](QString at) {
                if (at == "migration_validated")
                    throw domain::Error("StorageError", "Injected migration failure");
            });
        });
        CHECK(!QFileInfo::exists(failed));
        store.close();
        store.open(upgraded, "Saved");
        CHECK(store.state().value("saved_revision") == store.state().value("revision"));
        // Real truncated SQLite and future authoritative feature encodings remain refused
        // unchanged.
        QFile corrupt(files.filePath("truncated.cadproj"));
        CHECK(corrupt.open(QIODevice::WriteOnly));
        corrupt.write(bytes.left(80));
        corrupt.close();
        rejects([&] { store.inspect(corrupt.fileName()); });
        rejects([&] { storage::ProjectStore unsafe(files.path() + "/release-recovery"); });
        std::cout << "Cancellation/commit cut points, exactly-once retry, bounded 100-cycle "
                     "history, GC job/export roots, validated historical-manifest upgrade and "
                     "migration-failure source preservation passed\n";
    });
}
