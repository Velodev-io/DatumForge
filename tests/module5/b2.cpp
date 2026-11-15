#include "features/Model.hpp"
#include "persistence/ProjectStore.hpp"
#include "support/check.hpp"
#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <TopExp.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <numbers>
using namespace cad;
using sketch::fields;
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    return run([&] {
        QTemporaryDir root(QDir::tempPath() + "/cad-module5-b2-XXXXXX"),
            files(QDir::tempPath() + "/cad-module5-files-XXXXXX");
        for (auto *r : {&root, &files}) {
            CHECK(r->isValid());
            QFile marker(r->filePath(".cad-test-root"));
            CHECK(marker.open(QIODevice::WriteOnly));
            marker.write("disposable\n");
        }
        QFile fixture(QString(CAD_SOURCE_ROOT) + "/tests/fixtures/B2.json");
        CHECK(fixture.open(QIODevice::ReadOnly));
        auto bytes = fixture.readAll();
        auto recipe = QJsonDocument::fromJson(bytes).object();
        auto commands = recipe["recipe"].toObject()["features"].toArray();
        sketch::Sketch profile;
        profile.rectangle({0, 0}, {.2, .1});
        std::vector<features::Feature> definitions;
        features::Feature base;
        base.kind = "Extrude";
        base.parameters = fields({{"sketch", profile.id},
                                  {"target", ""},
                                  {"operation", "NewBody"},
                                  {"depth", .01},
                                  {"extent", "Forward"}});
        definitions.push_back(base);
        for (int i = 1; i < commands.size(); ++i) {
            auto r = commands[i].toObject();
            auto center = r["center_mm"].toArray();
            features::Feature f;
            f.kind = "Hole";
            f.name = "Hole " + QString::number(i);
            f.parameters = fields({{"target", base.id},
                                   {"origin", QCborArray{center[0].toDouble() * .001,
                                                         center[1].toDouble() * .001, .01}},
                                   {"direction", QCborArray{0., 0., -1.}},
                                   {"diameter", r["radius_mm"].toDouble() * .002},
                                   {"depth", .01},
                                   {"through", true}});
            definitions.push_back(f);
        }
        CHECK(definitions.size() == 100);
        auto sketches = sketch::encode({profile});
        auto encoded = features::encode(definitions);
        kernel::ResultCache cache;
        QElapsedTimer clock;
        clock.start();
        auto result = kernel::Part::build(sketches, encoded, &cache);
        double expected = recipe["expected"].toObject()["volume_mm3"].toDouble() * 1e-9;
        near(result.measure(1).value("volume_m3").toDouble(), expected, expected * 1e-6);
        auto mesh = result.mesh();
        CHECK(mesh.indices.size() / 3 <= 500000);
        TopTools_IndexedMapOfShape faces;
        TopExp::MapShapes(result.bodies[0].shape, TopAbs_FACE, faces);
        CHECK(faces.Extent() <= 20000);
        storage::ProjectStore store(root.path());
        auto project = newId(), doc = newId();
        store.create(project);
        auto command = [&](QString name, QCborMap p = QCborMap{}) {
            wire::Envelope e;
            e.command = name;
            e.context.project = project;
            e.context.document = doc;
            e.revision = store.revision();
            e.payload = p;
            return e;
        };
        store.execute(command("CreateDocument", fields({{"id", doc},
                                                        {"type", "Part"},
                                                        {"name", "Frozen B2"},
                                                        {"units", "mm"},
                                                        {"properties_entity", newId()}})));
        store.execute(command("SetSketches", fields({{"sketches", sketches}})));
        store.execute(command("SetFeatures", fields({{"features", encoded}})));
        double baseline = expected;
        for (int cycle = 0; cycle < 100; ++cycle) {
            auto edit = definitions;
            double diameter = .002 + double(cycle + 1) * 1e-7;
            edit.back().parameters.insert(QStringLiteral("diameter"), diameter);
            store.execute(command("SetFeatures", fields({{"features", features::encode(edit)}})));
            auto measure = store.evaluatePart(doc).measure(store.revision());
            double v = expected + std::numbers::pi * (.001 * .001 - diameter * diameter / 4) * .01;
            near(measure.value("volume_m3").toDouble(), v, v * 1e-6);
            store.execute(command("Undo"));
            near(store.evaluatePart(doc).measure(store.revision()).value("volume_m3").toDouble(),
                 baseline, baseline * 1e-6);
            store.execute(command("Redo"));
            near(store.evaluatePart(doc).measure(store.revision()).value("volume_m3").toDouble(), v,
                 v * 1e-6);
            definitions = edit;
            baseline = v;
            if (cycle % 10 == 0)
                std::cout << "B2 cycle " << cycle << " completed\n" << std::flush;
        }
        auto stats = store.cacheStatistics();
        CHECK(stats.value("estimated_bytes").toInteger() <=
              stats.value("budget_bytes").toInteger());
        CHECK(stats.value("entries").toInteger() <= 128);
        store.trimResults();
        CHECK(store.cacheStatistics().value("entries").toInteger() == 0);
        CHECK(store.cacheStatistics().value("estimated_bytes").toInteger() == 0);
        near(store.evaluatePart(doc).measure(store.revision()).value("volume_m3").toDouble(),
             baseline, baseline * 1e-6);
        auto saved = files.filePath("B2.cadproj");
        store.save(command("SaveProject"), saved);
        auto revision = store.revision();
        store.close();
        store.recover(project);
        CHECK(store.revision() == revision);
        near(store.evaluatePart(doc).measure(revision).value("volume_m3").toDouble(), baseline,
             baseline * 1e-6);
        store.close();
        store.open(saved, "Saved");
        CHECK(store.features(doc).size() == 100);
        near(store.evaluatePart(doc).measure(revision).value("volume_m3").toDouble(), baseline,
             baseline * 1e-6);
        if (argc > 1) {
            QFile out(argv[1]);
            CHECK(out.open(QIODevice::WriteOnly));
            out.write(
                QJsonDocument(
                    QJsonObject{
                        {"status", "Pass"},
                        {"fixture_sha256",
                         QString::fromLatin1(
                             QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex())},
                        {"features", 100},
                        {"solids", 1},
                        {"exact_faces", faces.Extent()},
                        {"visible_triangles", mesh.indices.size() / 3},
                        {"initial_volume_mm3", expected * 1e9},
                        {"cycles", 100},
                        {"elapsed_seconds", clock.elapsed() / 1000.},
                        {"cache_before_trim", QJsonObject::fromVariantMap(stats.toVariantMap())},
                        {"scope", "Correctness and bounded cache accounting; not physical "
                                  "memory/FPS/SRS budget acceptance"}})
                    .toJson());
        }
    });
}
