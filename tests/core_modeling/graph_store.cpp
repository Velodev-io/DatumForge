#include "evaluation/Plan.hpp"
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
        QTemporaryDir root(QDir::tempPath() + "/cad-module5-cm4-graph-XXXXXX"),
            files(QDir::tempPath() + "/cad-module5-cm4-files-XXXXXX");
        for (auto *r : {&root, &files}) {
            CHECK(r->isValid());
            QFile marker(r->filePath(".cad-test-root"));
            CHECK(marker.open(QIODevice::WriteOnly));
            marker.write("disposable\n");
        }
        storage::ProjectStore store(root.path());
        auto project = newId(), first = newId(), second = newId();
        store.create(project);
        auto command = [&](QString name, QString doc, QCborMap payload = {}) {
            wire::Envelope e;
            e.context.project = project;
            e.context.document = doc;
            e.command = name;
            e.revision = store.revision();
            e.payload = payload;
            return e;
        };
        for (auto doc : {first, second})
            store.execute(command("CreateDocument", doc,
                                  fields({{"id", doc},
                                          {"properties_entity", newId()},
                                          {"name", "Graph part"},
                                          {"type", "Part"},
                                          {"units", "mm"}})));
        sketch::Sketch source, target;
        auto line = source.line(source.point({.01, -.01}), source.point({.01, .01}));
        target.frame = {{0, 0, 0}, {1, 0, 0}, {0, 0, 1}};
        target.rectangle({.01, 0}, {.03, .02});
        sketch::Constraint c;
        c.kind = "Pierce";
        c.a = target.entities[0].id;
        c.reference = sketch::CurveReference{"Self", source.id, line, "Line", 0, 1, .5};
        target.constraints.push_back(c);
        store.execute(command("SetSketches", first,
                              fields({{"sketches", sketch::encode({target, source})}})));
        features::Feature feature;
        feature.parameters = fields({{"sketch", target.id},
                                     {"operation", "NewBody"},
                                     {"target", ""},
                                     {"depth", .01},
                                     {"extent", "Forward"}});
        auto definitions = features::encode({feature});
        // The consuming Part has no local sketches. Gathering its external
        // profile must retain that profile's own Pierce dependency closure.
        store.execute(command("SetFeatures", second, fields({{"features", definitions}})));
        auto gathered = store.profileSketches(second, definitions);
        CHECK(gathered.size() == 2);
        evaluation::Plan plan(gathered, definitions);
        CHECK(plan.geometryGraph.descendants(source.id).contains(feature.id));
        near(store.evaluatePart(second).measure(store.revision()).value("volume_m3").toDouble(),
             .000004, 1e-12);
        auto revision = store.revision();
        auto broken = sketch::parse(store.sketches(first));
        broken.pop_back();
        rejects([&] {
            store.execute(
                command("SetSketches", first, fields({{"sketches", sketch::encode(broken)}})));
        });
        CHECK(store.revision() == revision);
        store.execute(command("Undo", second));
        CHECK(store.features(second).empty());
        store.execute(command("Redo", second));
        CHECK(store.profileSketches(second, store.features(second)).size() == 2);
        auto path = files.filePath("graph.cadproj");
        store.save(command("SaveProject", second), path);
        store.close();
        store.open(path, "Saved");
        CHECK(store.profileSketches(second, store.features(second)).size() == 2);
        near(store.evaluatePart(second).measure(store.revision()).value("volume_m3").toDouble(),
             .000004, 1e-12);
    });
}
