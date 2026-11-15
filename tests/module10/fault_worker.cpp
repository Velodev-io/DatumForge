#include "persistence/ProjectStore.hpp"
#include "support/check.hpp"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QThread>
#include <iostream>
using namespace cad;
using sketch::fields;
const QString project = "11111111-1111-4111-8111-111111111111",
              part = "22222222-2222-4222-8222-222222222222",
              asmId = "33333333-3333-4333-8333-333333333333",
              drawingId = "44444444-4444-4444-8444-444444444444",
              op = "55555555-5555-4555-8555-555555555555",
              saveOp = "66666666-6666-4666-8666-666666666666";
int main(int argc, char **argv) {
    QGuiApplication app(argc, argv);
    try {
        CHECK(argc >= 3);
        QString root = QFileInfo(QString::fromLocal8Bit(argv[1])).canonicalFilePath(),
                action = QString::fromLocal8Bit(argv[2]),
                phase = argc > 3 ? QString::fromLocal8Bit(argv[3]) : QString();
        CHECK(root.startsWith(QFileInfo(QDir::tempPath()).canonicalFilePath() + "/") &&
              QFileInfo(root).fileName().startsWith("cad-module10-") &&
              QFileInfo::exists(root + "/.cad-test-root"));
        storage::StoreOptions opts;
        opts.boundary = [&](QString at) {
            if (at != phase && "pause_" + at != phase)
                return;
            std::cout << "BOUNDARY " << at.toStdString() << std::endl;
            if (phase.startsWith("pause_"))
                QThread::sleep(30);
            else
                std::_Exit(70);
        };
        storage::ProjectStore store(root + "/cad-module10-recovery", opts);
        QString doc = part, kind;
        auto command = [&](QString name, QCborMap payload = QCborMap{},
                           QString operation = newId()) {
            wire::Envelope e;
            e.command = name;
            e.payload = payload;
            e.operation = operation;
            e.revision = store.revision();
            e.context.project = project;
            e.context.document = doc;
            return e;
        };
        auto create = [&](QString id, QString type) {
            doc = id;
            store.execute(command("CreateDocument", fields({{"id", id},
                                                            {"type", type},
                                                            {"name", type + " fault fixture"},
                                                            {"units", "mm"},
                                                            {"properties_entity", newId()}})));
        };
        auto saved = root + "/saved.cadproj";
        if (action == "init") {
            kind = phase;
            store.create(project);
            create(part, "Part");
            sketch::Sketch s;
            s.rectangle({0, 0}, {.02, .03});
            store.execute(command("SetSketches", fields({{"sketches", sketch::encode({s})}})));
            features::Feature f;
            f.kind = "Extrude";
            f.parameters = fields({{"sketch", s.id},
                                   {"operation", "NewBody"},
                                   {"target", ""},
                                   {"depth", .01},
                                   {"extent", "Forward"}});
            store.execute(command("SetFeatures", fields({{"features", features::encode({f})}})));
            if (kind != "Part") {
                create(asmId, "Assembly");
                assembly::Model a;
                assembly::Occurrence o;
                o.definition = part;
                o.grounded = true;
                a.occurrences.push_back(o);
                store.execute(command("SetAssembly", fields({{"assembly", a.map()}})));
            }
            if (kind == "Drawing") {
                create(drawingId, "Drawing");
                drawing::Model m;
                drawing::Sheet s;
                drawing::View v;
                v.source = asmId;
                v.x = 30;
                v.y = 80;
                s.views.push_back(v);
                s.bomSource = asmId;
                m.sheets.push_back(s);
                store.execute(command("SetDrawing", fields({{"drawing", m.map()}})));
            }
            if (kind == "Drawing")
                store.exportDrawing(command("ExportDrawing", fields({{"overwrite", true}})),
                                    root + "/saved.pdf");
            store.save(command("SaveProject"), saved);
            QFile meta(root + "/meta.json");
            CHECK(meta.open(QIODevice::WriteOnly));
            meta.write(
                QJsonDocument(
                    QJsonObject{{"kind", kind}, {"document", doc}, {"base", store.revision()}})
                    .toJson());
            return 0;
        }
        QFile meta(root + "/meta.json");
        CHECK(meta.open(QIODevice::ReadOnly));
        auto m = QJsonDocument::fromJson(meta.readAll()).object();
        kind = m["kind"].toString();
        doc = m["document"].toString();
        auto base = m["base"].toInteger();
        if (action == "inspect") {
            auto info = store.inspect(saved);
            std::cout << QJsonDocument(QCborValue(info).toJsonValue().toObject())
                             .toJson(QJsonDocument::Compact)
                             .constData()
                      << std::endl;
            return 0;
        }
        if (action == "migrate") {
            storage::ProjectStore::migrationCopy(saved, root + "/migrated.cadproj", opts.boundary);
            return 0;
        }
        if (action == "open") {
            store.open(saved, "Saved");
            return 0;
        }
        store.recover(project);
        if (action == "edit") {
            wire::Envelope e;
            if (kind == "Part") {
                auto f = features::parse(store.features(doc));
                f[0].parameters.insert(QStringLiteral("depth"), .011);
                e = command("SetFeatures", fields({{"features", features::encode(f)}}), op);
            } else if (kind == "Assembly") {
                auto a = store.assemblyModel(doc);
                a.occurrences[0].pose.translation[0] = .01;
                e = command("SetAssembly", fields({{"assembly", a.map()}}), op);
            } else {
                auto d = store.drawingModel(doc);
                d.sheets[0].scale = .4;
                e = command("SetDrawing", fields({{"drawing", d.map()}}), op);
            }
            e.revision = base;
            store.execute(e);
        } else if (action == "pdf")
            store.exportDrawing(command("ExportDrawing", fields({{"overwrite", true}})),
                                root + "/saved.pdf");
        else if (action == "save")
            store.save(command("SaveProject", {}, saveOp), saved);
        else if (action == "query") {
            auto state = store.state();
            state.insert(QStringLiteral("operation"), store.outcome(op));
            state.insert(QStringLiteral("save_operation"), store.outcome(saveOp));
            state.insert(QStringLiteral("volume_m3"),
                         store.evaluatePart(part).measure(store.revision()).value("volume_m3"));
            if (kind == "Assembly")
                state.insert(QStringLiteral("translation_m"),
                             store.assemblyModel(doc).occurrences[0].pose.translation[0]);
            if (kind == "Drawing") {
                state.insert(QStringLiteral("scale"), store.drawingModel(doc).sheets[0].scale);
                state.insert(QStringLiteral("drawing_current"),
                             store.drawingResult(doc).value("current"));
            }
            storage::Database read(store.workingPath(), true);
            state.insert(
                QStringLiteral("operation_count"),
                read.scalar("SELECT count(*) FROM commands WHERE operation_id=?", {op}).toInt());
            std::cout << QJsonDocument(QCborValue(state).toJsonValue().toObject())
                             .toJson(QJsonDocument::Compact)
                             .constData()
                      << std::endl;
        } else
            throw domain::Error("InvalidInput", "Unknown fault action");
        return 0;
    } catch (const domain::Error &e) {
        std::cerr << e.code.toStdString() << ": " << e.what() << std::endl;
        return 1;
    } catch (const std::exception &e) {
        std::cerr << e.what() << std::endl;
        return 1;
    }
}
