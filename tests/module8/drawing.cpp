#include "../module4/Fixture.hpp"
#include "drawing/Geometry.hpp"
#include "drawing/Paths.hpp"
#include "persistence/ProjectStore.hpp"
#include "support/check.hpp"
#include <BRepAdaptor_Curve.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
using namespace cad;
using sketch::fields;
int main(int argc, char **argv) {
    QGuiApplication app(argc, argv);
    return run([&] {
        QTemporaryDir root(QDir::tempPath() + "/cad-module8-store-XXXXXX"),
            files(QDir::tempPath() + "/cad-module8-files-XXXXXX");
        for (auto *r : {&root, &files}) {
            CHECK(r->isValid());
            QFile marker(r->filePath(".cad-test-root"));
            CHECK(marker.open(QIODevice::WriteOnly));
            marker.write("disposable\n");
        }
        storage::ProjectStore store(root.path());
        store.create(newId());
        QString doc;
        auto command = [&](QString c, QCborMap p = QCborMap{}) {
            wire::Envelope e;
            e.command = c;
            e.payload = p;
            e.context.project = store.project();
            e.context.document = doc;
            e.revision = store.revision();
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
        // Real B1 bracket including final fillet.
        auto part = create("Part", "B1 bracket");
        fixture4::B1 b1;
        store.execute(command("SetSketches", fields({{"sketches", b1.sketches()}})));
        store.execute(command("SetFeatures", fields({{"features", b1.features()}})));
        auto defs = features::parse(b1.features());
        defs.push_back(b1.fillet(fixture4::filletEdge(store.evaluatePart(part).topology())));
        store.execute(command("SetFeatures", fields({{"features", features::encode(defs)}})));
        // Frozen B3: ten distinct bracket definitions, 100 occurrences, 99 mates.
        QFile b3(QString(CAD_SOURCE_ROOT) + "/tests/fixtures/B3.json");
        CHECK(b3.open(QIODevice::ReadOnly));
        auto recipe = QJsonDocument::fromJson(b3.readAll()).object()["recipe"].toObject();
        QStringList definitions;
        for (auto v : recipe["definitions"].toArray()) {
            auto id = create("Part", "B3 Part " + QString::number(definitions.size()));
            definitions << id;
            fixture4::B1 b;
            b.outer.entities.clear();
            b.outer.constraints.clear();
            b.outer.rectangle({0, 0}, {v.toObject()["width_mm"].toDouble() * .001, .05});
            store.execute(command("SetSketches", fields({{"sketches", b.sketches()}})));
            store.execute(command("SetFeatures", fields({{"features", b.features()}})));
            auto fs = features::parse(b.features());
            fs.push_back(b.fillet(fixture4::filletEdge(store.evaluatePart(doc).topology())));
            store.execute(command("SetFeatures", fields({{"features", features::encode(fs)}})));
        }
        auto assemblyId = create("Assembly", "B3 assembly");
        assembly::Model assemblyModel;
        QMap<QString, QString> occurrenceIds;
        for (auto v : recipe["occurrences"].toArray()) {
            auto r = v.toObject();
            assembly::Occurrence o;
            o.definition = definitions[r["definition"].toInt()];
            o.name = r["id"].toString();
            o.grounded = o.name == recipe["grounded"].toString();
            auto t = r["translation_mm"].toArray();
            for (int k = 0; k < 3; ++k)
                o.pose.translation[k] = t[k].toDouble() * .001;
            occurrenceIds[o.name] = o.id;
            assemblyModel.occurrences.push_back(o);
        }
        for (auto v : recipe["mates"].toArray()) {
            auto r = v.toObject();
            assembly::Mate m;
            m.kind = "Distance";
            m.first.occurrence = occurrenceIds[r["from"].toString()];
            m.second.occurrence = occurrenceIds[r["to"].toString()];
            m.value = r["distance_mm"].toDouble() * .001;
            assemblyModel.mates.push_back(m);
        }
        CHECK(assemblyModel.occurrences.size() == 100 && assemblyModel.mates.size() == 99);
        store.execute(command("SetAssembly", fields({{"assembly", assemblyModel.map()}})));
        auto drawingId = create("Drawing", "B5 engineering drawing");
        drawing::Model model;
        drawing::Sheet first;
        first.name = "B1 bracket";
        drawing::View front;
        front.source = part;
        front.x = 30;
        front.y = 95;
        first.views.push_back(front);
        drawing::View top;
        top.source = part;
        top.orientation = "Top";
        top.parent = front.id;
        top.x = 30;
        top.y = 50;
        first.views.push_back(top);
        drawing::View iso;
        iso.source = part;
        iso.orientation = "Isometric";
        iso.x = 130;
        iso.y = 80;
        first.views.push_back(iso);
        auto anchors = drawing::anchors(part, store.assemblyLoader());
        drawing::Annotation dim;
        dim.kind = "Linear";
        dim.view = front.id;
        dim.x = 30;
        dim.y = 115;
        for (auto a : anchors) {
            auto m = a.toMap();
            auto body = m.value("body").toString(), token = m.value("token").toString();
            for (auto &b : store.evaluatePart(part).bodies)
                if (b.id == body)
                    for (TopExp_Explorer it(b.shape, TopAbs_EDGE); it.More(); it.Next())
                        if (kernel::topologyToken(it.Current()) == token) {
                            BRepAdaptor_Curve c(TopoDS::Edge(it.Current()));
                            if (c.GetType() == GeomAbs_Line &&
                                std::abs(c.Value(c.FirstParameter())
                                             .Distance(c.Value(c.LastParameter())) -
                                         80) < 1e-6) {
                                dim.body = body;
                                dim.token = token;
                            }
                        }
        }
        CHECK(!dim.token.isEmpty());
        first.annotations.push_back(dim);
        model.sheets.push_back(first);
        drawing::Sheet second;
        second.name = "B3 assembly BOM";
        second.scale = .075;
        second.bomSource = assemblyId;
        front.id = newId();
        front.source = assemblyId;
        front.x = 15;
        front.y = 110;
        front.parent.clear();
        iso.id = newId();
        iso.source = assemblyId;
        iso.x = 30;
        iso.y = 150;
        second.views = {front, iso};
        model.sheets.push_back(second);
        auto set = command("SetDrawing", fields({{"drawing", model.map()}}));
        auto rev = store.revision();
        store.execute(set);
        CHECK(store.revision() == rev + 1);
        store.execute(set);
        CHECK(store.revision() == rev + 1);
        auto result = store.drawingResult(doc);
        CHECK(result.value("current").toBool());
        CHECK(!result.value("raster_fallback").toBool());
        auto sheets = result.value("sheets").toArray();
        CHECK(sheets.size() == 2);
        auto a = sheets[0].toMap().value("annotations").toArray()[0].toMap();
        CHECK(std::abs(a.value("measured_si").toDouble() - .08) < 1e-10);
        auto bom = sheets[1].toMap().value("bom").toArray();
        int quantity = 0;
        for (auto row : bom)
            quantity += int(row.toMap().value("quantity").toInteger());
        CHECK(quantity == 100 && bom.size() == 10);
        auto view = sheets[0].toMap().value("views").toArray()[0].toMap();
        CHECK(!view.value("visible").toArray().empty());
        double paperMax = 0;
        for (auto p : view.value("visible").toArray()) {
            auto points = drawing::points(p);
            for (int i = 1; i < points.size(); ++i) {
                paperMax = qMax(paperMax, std::hypot(points[i - 1].x() - points[i].x(),
                                                     points[i - 1].y() - points[i].y()));
            }
        }
        CHECK(std::abs(paperMax - 40) < 1e-8);
        CHECK(!view.value("hidden_paths").toArray().empty());
        // Actual PDF emitted by production worker service, not a screenshot.
        auto pdf = argc > 1 ? QString::fromLocal8Bit(argv[1]) : files.filePath("B5.pdf");
        store.exportDrawing(command("ExportDrawing", fields({{"overwrite", true}})), pdf);
        auto saved = files.filePath("drawing.cadproj");
        store.save(command("SaveProject"), saved);
        auto copy = files.filePath("portable.cadproj"), copyId = newId();
        store.save(
            command("PortableCopy", fields({{"new_project_id", copyId}, {"policy", "Copied"}})),
            copy, true);
        CHECK(QFile::remove(saved));
        store.close();
        store.open(copy, "Saved");
        doc = drawingId;
        CHECK(store.drawingResult(doc).value("current").toBool());
        store.trimResults();
        CHECK(store.closure().value("complete").toBool());
        auto accepted = store.revision();
        store.close();
        store.recover(copyId);
        CHECK(store.revision() == accepted);
        CHECK(store.drawingModel(doc).sheets[0].annotations[0].id == dim.id);
        // Component source edits update drawing BOM policy, independently of display meshes.
        doc = assemblyId;
        auto suppressed = store.assemblyModel(doc);
        suppressed.occurrences.back().suppressed = true;
        suppressed.mates.back().suppressed = true;
        store.execute(command("SetAssembly", fields({{"assembly", suppressed.map()}})));
        doc = drawingId;
        CHECK(!store.drawingResult(doc).value("current").toBool());
        store.execute(command("UpdateDrawing"));
        int reduced = 0;
        for (auto row :
             store.drawingResult(doc).value("sheets").toArray()[1].toMap().value("bom").toArray())
            reduced += int(row.toMap().value("quantity").toInteger());
        CHECK(reduced == 99);
        doc = assemblyId;
        suppressed.occurrences.back().suppressed = false;
        suppressed.mates.back().suppressed = false;
        store.execute(command("SetAssembly", fields({{"assembly", suppressed.map()}})));
        doc = drawingId;
        store.execute(command("UpdateDrawing"));
        // Source mutation invalidates revisions; export preserves prior PDF on refusal.
        QFile oldPdf(pdf);
        CHECK(oldPdf.open(QIODevice::ReadOnly));
        auto hash = QCryptographicHash::hash(oldPdf.readAll(), QCryptographicHash::Sha256);
        oldPdf.close();
        doc = part;
        store.execute(command("RenameDocument", fields({{"name", "Changed bracket"}})));
        doc = drawingId;
        CHECK(!store.drawingResult(doc).value("current").toBool());
        rejects([&] {
            store.exportDrawing(command("ExportDrawing", fields({{"overwrite", true}})), pdf);
        });
        CHECK(oldPdf.open(QIODevice::ReadOnly));
        CHECK(QCryptographicHash::hash(oldPdf.readAll(), QCryptographicHash::Sha256) == hash);
        oldPdf.close();
        store.execute(command("UpdateDrawing"));
        CHECK(store.drawingResult(doc).value("current").toBool());
        // Export the reopened portable project after source/BOM updates and original removal.
        store.exportDrawing(command("ExportDrawing", fields({{"overwrite", true}})), pdf);
        CHECK(oldPdf.open(QIODevice::ReadOnly));
        hash = QCryptographicHash::hash(oldPdf.readAll(), QCryptographicHash::Sha256);
        oldPdf.close();
        model = store.drawingModel(doc);
        model.sheets[0].annotations[0].token = "edge/deleted";
        store.execute(command("SetDrawing", fields({{"drawing", model.map()}})));
        result = store.drawingResult(doc);
        CHECK(!result.value("current").toBool());
        CHECK(result.value("sheets")
                  .toArray()[0]
                  .toMap()
                  .value("annotations")
                  .toArray()[0]
                  .toMap()
                  .value("status") == "Dangling");
        rejects([&] { store.closure(); });
        store.execute(command("Undo"));
        CHECK(store.drawingResult(doc).value("current").toBool());
        store.execute(command("Redo"));
        CHECK(!store.drawingResult(doc).value("current").toBool());
        store.execute(command("Undo"));
        CHECK(oldPdf.open(QIODevice::ReadOnly));
        CHECK(QCryptographicHash::hash(oldPdf.readAll(), QCryptographicHash::Sha256) == hash);
        auto bad = model.map();
        bad.insert(QStringLiteral("version"), 99);
        rejects([&] { drawing::Model::parse(bad); });
        auto invalid = model;
        invalid.sheets[0].views[1].y = 150;
        rejects([&] { drawing::Model::parse(invalid.map()); });
        result = store.drawingResult(doc);
        if (argc > 2) {
            QFile out(QString::fromLocal8Bit(argv[2]));
            CHECK(out.open(QIODevice::WriteOnly));
            out.write(QJsonDocument(QCborValue(fields({{"sheets", 2},
                                                       {"views", 5},
                                                       {"annotations", 1},
                                                       {"measured_length_m", .08},
                                                       {"paper_length_mm", 40},
                                                       {"bom_rows", 10},
                                                       {"bom_quantity", 100},
                                                       {"drawing", result}}))
                                        .toJsonValue()
                                        .toObject())
                          .toJson());
        }
        std::cout << "B5 exact HLR, 80 mm / 40 mm paper dimension, 100-leaf BOM, vector PDF, "
                     "portable reopen, revisions, dangling/undo/recovery passed\n";
    });
}
