#include "persistence/ProjectStore.hpp"
#include "support/check.hpp"
#include <QBuffer>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QThread>
#include <iostream>
using namespace cad;
using namespace cad::storage;
const QString project = "11111111-1111-4111-8111-111111111111",
              document = "22222222-2222-4222-8222-222222222222",
              operation = "44444444-4444-4444-8444-444444444444";
QCborMap fields(std::initializer_list<std::pair<QString, QCborValue>> list) {
    QCborMap out;
    for (auto f : list)
        out.insert(f.first, f.second);
    return out;
}
wire::Envelope request(QString command, qint64 base, QString op = newId(), QCborMap payload = {}) {
    wire::Envelope e;
    e.context.project = project;
    e.context.document = document;
    e.epoch = newId();
    e.command = command;
    e.revision = base;
    e.operation = op;
    e.payload = payload;
    return e;
}
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    try {
        CHECK(argc >= 3);
        auto root = QFileInfo(QString::fromLocal8Bit(argv[1])).canonicalFilePath();
        CHECK(root.startsWith(QFileInfo(QDir::tempPath()).canonicalFilePath() + '/') &&
              QFileInfo(root).fileName().startsWith("cad-module2-") &&
              QFileInfo::exists(root + "/.cad-test-root"));
        auto action = QString::fromLocal8Bit(argv[2]),
             phase = argc > 3 ? QString::fromLocal8Bit(argv[3]) : QString();
        auto work = root + "/cad-module2-recovery", saved = root + "/saved.cadproj";
        StoreOptions options;
        options.boundary = [&](QString at) {
            if (at != phase && "pause_" + at != phase)
                return;
            std::cout << "BOUNDARY " << at.toStdString() << std::endl;
            if (phase.startsWith("pause_"))
                QThread::sleep(30);
            else
                std::_Exit(70);
        };
        if (action == "disk_full") {
            Database read(work + "/workspaces/" +
                              QString::fromUtf8([&] {
                                  QFile f(work + "/index/" + project);
                                  CHECK(f.open(QIODevice::ReadOnly));
                                  return f.readAll();
                              }())
                                  .trimmed() +
                              "/working.sqlite",
                          true);
            options.maximumPages = read.scalar("PRAGMA page_count").toInt() + 4;
        }
        ProjectStore store(work, options);
        if (action == "init") {
            store.create(project);
            QString type = argc > 3 ? QString::fromLocal8Bit(argv[3]) : "Part";
            store.execute(
                request("CreateDocument", 0, newId(),
                        fields({{"id", document},
                                {"type", type},
                                {"name", "Fault fixture"},
                                {"units", "mm"},
                                {"properties_entity", "33333333-3333-4333-8333-333333333333"}})));
            store.execute(
                request("SetParameter", 1, newId(),
                        fields({{"name", "Depth"}, {"dimension", "length"}, {"value_si", .001}})));
            store.save(request("SaveProject", 2), saved);
        } else if (action == "inspect_saved") {
            auto info = store.inspect(saved);
            std::cout << QJsonDocument(QCborValue(info).toJsonValue().toObject())
                             .toJson(QJsonDocument::Compact)
                             .constData()
                      << std::endl;
            return 0;
        } else if (action == "corrupt_future" || action == "corrupt_missing" ||
                   action == "legacy" || action == "corrupt_trigger" || action == "corrupt_delta") {
            Database fixture(saved);
            if (action == "corrupt_future")
                fixture.exec("PRAGMA user_version=999");
            else if (action == "legacy")
                fixture.exec("PRAGMA user_version=0");
            else if (action == "corrupt_trigger")
                fixture.exec("CREATE TRIGGER unsafe AFTER INSERT ON commands BEGIN DELETE FROM "
                             "parameters; END");
            else if (action == "corrupt_delta")
                fixture.run("UPDATE commands SET before_delta=? WHERE category='edit'",
                            {QCborValue(fields({{"type", "execute_something"}})).toCbor()});
            else {
                fixture.exec("PRAGMA foreign_keys=OFF");
                fixture.exec("INSERT INTO entity_objects SELECT "
                             "id,'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa'"
                             " FROM entities LIMIT 1");
            }
            return 0;
        } else if (action == "migrate") {
            ProjectStore::migrationCopy(saved, root + "/migrated.cadproj", options.boundary);
            return 0;
        } else {
            store.recover(project);
            if (action == "edit")
                store.execute(request(
                    "SetParameter", 2, operation,
                    fields({{"name", "Depth"}, {"dimension", "length"}, {"value_si", .002}})));
            else if (action == "save")
                store.save(request("SaveProject", 3, "55555555-5555-4555-8555-555555555555"),
                           saved);
            else if (action == "disk_full") {
                QByteArray bytes(1 << 20, 'B');
                QBuffer input(&bytes);
                input.open(QIODevice::ReadOnly);
                store.attach(
                    request("AttachObject", 2, operation,
                            fields({{"size", bytes.size()},
                                    {"classification", "authoritative"},
                                    {"expected_sha256",
                                     QString::fromLatin1(
                                         QCryptographicHash::hash(bytes, QCryptographicHash::Sha256)
                                             .toHex())}})),
                    input, bytes.size());
            } else if (action == "save_and_lock") {
                store.execute(request(
                    "SetParameter", 2, operation,
                    fields({{"name", "Depth"}, {"dimension", "length"}, {"value_si", .002}})));
                store.save(request("SaveProject", 3), saved);
                std::cout << "LOCKED" << std::endl;
                QThread::sleep(30);
            } else if (action == "lock") {
                std::cout << "LOCKED" << std::endl;
                QThread::sleep(30);
            } else if (action == "open") {
                store.close();
                store.open(saved, "Saved");
            } else if (action == "query") {
                auto report = store.state();
                report.insert(QStringLiteral("operation"), store.outcome(operation));
                report.insert(QStringLiteral("save_operation"),
                              store.outcome("55555555-5555-4555-8555-555555555555"));
                std::cout << QJsonDocument(QCborValue(report).toJsonValue().toObject())
                                 .toJson(QJsonDocument::Compact)
                                 .constData()
                          << std::endl;
                return 0;
            } else if (action == "open_alias") {
                store.close();
                store.open(root + "/alias.cadproj", "Saved");
            } else
                throw domain::Error("InvalidInput", "Unknown test action");
        }
        std::cout << "PASS revision=" << store.revision() << std::endl;
        return 0;
    } catch (const domain::Error &e) {
        std::cerr << e.code.toStdString() << ": " << e.what() << std::endl;
        return 1;
    } catch (const std::exception &e) {
        std::cerr << e.what() << std::endl;
        return 1;
    }
}
