#include "persistence/ProjectStore.hpp"
#include "sketch/Solver.hpp"
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
        QTemporaryDir temp(QDir::tempPath() + "/cad-module3-storage-XXXXXX");
        CHECK(temp.isValid());
        QFile marker(temp.filePath(".cad-test-root"));
        CHECK(marker.open(QIODevice::WriteOnly));
        marker.write("disposable\n");
        marker.close();
        QTemporaryDir files(QDir::tempPath() + "/cad-module3-files-XXXXXX");
        CHECK(files.isValid());
        QFile fileMarker(files.filePath(".cad-test-root"));
        CHECK(fileMarker.open(QIODevice::WriteOnly));
        fileMarker.write("disposable\n");
        fileMarker.close();
        storage::ProjectStore store(temp.path());
        auto project = newId(), doc = newId(), owner = newId();
        store.create(project);
        auto command = [&](QString cmd, QCborMap p) {
            wire::Envelope e;
            e.context.project = project;
            e.context.document = doc;
            e.command = cmd;
            e.payload = p;
            e.revision = store.revision();
            return e;
        };
        store.execute(command("CreateDocument", fields({{"id", doc},
                                                        {"type", "Part"},
                                                        {"name", "Sketch part"},
                                                        {"units", "mm"},
                                                        {"properties_entity", owner}})));
        sketch::Sketch first, second;
        first.rectangle({0, 0}, {.08, .05});
        second.rectangle({.02, .02}, {.04, .04});
        auto a = sketch::encode({first, second});
        auto edit = command("SetSketches", fields({{"sketches", a}}));
        store.execute(edit);
        CHECK(store.revision() == 2);
        CHECK(store.sketches(doc) == a);
        store.execute(edit);
        CHECK(store.revision() == 2);
        auto invalid = first;
        invalid.entities[0].point.x = INFINITY;
        rejects([&] {
            store.execute(
                command("SetSketches", fields({{"sketches", sketch::encode({invalid})}})));
        });
        CHECK(store.revision() == 2);
        CHECK(store.sketches(doc) == a);
        store.execute(command("Undo", {}));
        CHECK(store.sketches(doc).empty());
        store.execute(command("Undo", {}));
        CHECK(store.state().value("documents").toArray().empty());
        store.execute(command("Redo", {}));
        CHECK(store.state().value("documents").toArray().size() == 1);
        store.execute(command("Redo", {}));
        CHECK(store.sketches(doc) == a);
        auto path = files.filePath("part.cadproj");
        auto save = command("SaveProject", {});
        store.save(save, path);
        store.close();
        store.open(path, "Saved");
        CHECK(store.sketches(doc) == a);
        CHECK(store.state().value("documents").toArray()[0].toMap().value("sketches").toArray() ==
              a);
        auto constrained = first;
        auto lines = QStringList{};
        for (auto &e : constrained.entities)
            if (e.kind == "Line")
                lines.append(e.id);
        auto add = [&](QString kind, QString a, double val = 0) {
            sketch::Constraint c;
            c.kind = kind;
            c.a = a;
            c.value = val;
            constrained.constraints.push_back(c);
        };
        add("Fixed", constrained.entity(lines[0]).a);
        for (int i = 0; i < 4; ++i)
            add(i % 2 ? "Vertical" : "Horizontal", lines[i]);
        add("Length", lines[0], .08);
        add("Length", lines[1], .05);
        constrained.names.insert("Width", "80 mm");
        constrained.constraints[5].expression = "Width";
        store.execute(
            command("SetSketches", fields({{"sketches", sketch::encode({constrained, second})}})));
        auto dimensioned = store.sketches(doc);
        auto revised = sketch::parse(dimensioned);
        revised[0].names["Width"] = "90 mm";
        store.execute(command("SetSketches", fields({{"sketches", sketch::encode(revised)}})));
        auto changed = store.sketches(doc);
        auto parsed = sketch::parse(changed);
        near(parsed[0].entities[2].point.x, .09, 1e-10);
        near(parsed[0].constraints[5].value, .09, 1e-10);
        store.execute(command("Undo", {}));
        CHECK(store.sketches(doc) == dimensioned);
        store.execute(command("Redo", {}));
        CHECK(store.sketches(doc) == changed);
        auto oldRevision = store.revision();
        auto cycle = parsed;
        cycle[0].names["Width"] = "Width + 1 mm";
        rejects([&] {
            store.execute(command("SetSketches", fields({{"sketches", sketch::encode(cycle)}})));
        });
        CHECK(store.revision() == oldRevision);
        CHECK(store.sketches(doc) == changed);
        auto wrong = parsed;
        wrong[0].constraints[5].expression = "10 deg";
        rejects([&] {
            store.execute(command("SetSketches", fields({{"sketches", sketch::encode(wrong)}})));
        });
        CHECK(store.revision() == oldRevision);
        CHECK(store.sketches(doc) == changed);
        parsed[0].constraints.pop_back();
        store.execute(command("SetSketches", fields({{"sketches", sketch::encode(parsed)}})));
        auto deleted = store.sketches(doc);
        CHECK(store.state()
                  .value("documents")
                  .toArray()[0]
                  .toMap()
                  .value("sketch_diagnostics")
                  .toArray()[0]
                  .toMap()
                  .value("freedom")
                  .toInteger() == 1);
        store.execute(command("Undo", {}));
        CHECK(store.sketches(doc) == changed);
        store.execute(command("Redo", {}));
        CHECK(store.sketches(doc) == deleted);
        store.save(command("SaveProject", {}), path);
        store.close();
        store.open(path, "Saved");
        CHECK(store.sketches(doc) == deleted);
        auto accepted = sketch::parse(deleted);
        accepted[0].entities[2].point.x += .01;
        rejects([&] { sketch::validateAccepted(sketch::encode(accepted)); });
        std::cout << "3.3b: named dimension edits, real rebuild, constraint deletion, atomic "
                     "invalid-unit/cycle rejection, semantic undo/redo/save/reopen\n";
        std::cout << "3.1b: two B1 sketch transactions, dedup, failed edit preservation, "
                     "undo/redo, actual SQLite save/reopen IDs\n";
    });
}
