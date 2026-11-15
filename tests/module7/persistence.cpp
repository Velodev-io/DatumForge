#include "../module4/Fixture.hpp"
#include "persistence/ProjectStore.hpp"
#include "support/check.hpp"
#include <BRepPrimAPI_MakeBox.hxx>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <numbers>
using namespace cad;
using sketch::fields;
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    return run([] {
        QTemporaryDir root(QDir::tempPath() + "/cad-module7-store-XXXXXX"),
            files(QDir::tempPath() + "/cad-module7-files-XXXXXX");
        for (auto *r : {&root, &files}) {
            CHECK(r->isValid());
            QFile marker(r->filePath(".cad-test-root"));
            CHECK(marker.open(QIODevice::WriteOnly));
            marker.write("disposable\n");
        }
        storage::ProjectStore store(root.path());
        auto project = newId(), part = newId(), assemblyId = newId(), subId = newId();
        store.create(project);
        QString doc = part;
        auto command = [&](QString kind, QCborMap p = QCborMap{}) {
            wire::Envelope e;
            e.context.project = store.project();
            e.context.document = doc;
            e.command = kind;
            e.payload = p;
            e.revision = store.revision();
            return e;
        };
        auto create = [&](QString id, QString type, QString name) {
            store.execute(command("CreateDocument", fields({{"id", id},
                                                            {"type", type},
                                                            {"name", name},
                                                            {"units", "mm"},
                                                            {"properties_entity", newId()}})));
        };
        create(part, "Part", "Shared exact part");
        fixture4::B1 fixture;
        store.execute(command("SetSketches", fields({{"sketches", fixture.sketches()}})));
        store.execute(command("SetFeatures", fields({{"features", fixture.features()}})));
        create(assemblyId, "Assembly", "Parent assembly");
        doc = assemblyId;
        assembly::Model model;
        assembly::Occurrence a, b;
        a.definition = b.definition = part;
        a.grounded = true;
        a.name = "Grounded";
        b.name = "Movable";
        b.pose.translation = {.1, 0, 0};
        model.occurrences = {a, b};
        auto set = command("SetAssembly", fields({{"assembly", model.map()}}));
        auto old = store.revision();
        store.execute(set);
        CHECK(store.revision() == old + 1);
        store.execute(set);
        CHECK(store.revision() == old + 1);
        auto reply = store.assemblyResult(doc);
        CHECK(reply.value("mesh_definitions").toInteger() == 1);
        CHECK(reply.value("exact_definitions").toInteger() == 1);
        auto mesh = unpack(reply.value("display_manifest").toMap(),
                           reply.value("_display_bytes").toByteArray());
        CHECK(mesh->instances.size() == 2);
        CHECK(mesh->instances[0].firstTriangle == mesh->instances[1].firstTriangle);
        CHECK(mesh->instances[0].id != mesh->instances[1].id);
        auto candidates = store.assemblyReferences(doc, a.id).value("candidates").toArray();
        CHECK(candidates.size() > 7);
        assembly::Mate distance;
        distance.kind = "Distance";
        distance.first.occurrence = a.id;
        distance.second.occurrence = b.id;
        distance.value = .1;
        model = store.assemblyModel(doc);
        model.mates = {distance};
        store.execute(command("SetAssembly", fields({{"assembly", model.map()}})));
        CHECK(store.assemblyResult(doc)
                  .value("assembly_result")
                  .toMap()
                  .value("freedom")
                  .toInteger() == 5);
        auto conflicting = model;
        auto extra = distance;
        extra.id = newId();
        extra.value = .2;
        conflicting.mates.push_back(extra);
        old = store.revision();
        rejects([&] {
            store.execute(command("SetAssembly", fields({{"assembly", conflicting.map()}})));
        });
        CHECK(store.revision() == old);
        auto preview = store.assemblyResult(doc, conflicting.map());
        CHECK(!preview.value("assembly_result").toMap().value("valid").toBool());
        CHECK(store.revision() == old);
        model = store.assemblyModel(doc);
        model.mates.clear();
        model.occurrences[1].unloaded = true;
        store.execute(command("SetAssembly", fields({{"assembly", model.map()}})));
        reply = store.assemblyResult(doc);
        CHECK(reply.value("exact_definitions").toInteger() == 0);
        CHECK(reply.value("display_manifest").toMap().value("instances").toArray().size() == 2);
        CHECK(reply.value("occurrence_states").toArray()[1].toMap().value("status") == "Unloaded");
        rejects([&] { store.assemblyInterference(doc); });
        model.occurrences[1].unloaded = false;
        model.occurrences[1].hidden = true;
        store.execute(command("SetAssembly", fields({{"assembly", model.map()}})));
        reply = store.assemblyResult(doc);
        CHECK(reply.value("display_manifest").toMap().value("instances").toArray().size() == 1);
        CHECK(reply.value("bom").toArray()[0].toMap().value("quantity").toInteger() == 2);
        model.occurrences[1].hidden = false;
        model.occurrences[1].suppressed = true;
        store.execute(command("SetAssembly", fields({{"assembly", model.map()}})));
        CHECK(store.assemblyResult(doc)
                  .value("bom")
                  .toArray()[0]
                  .toMap()
                  .value("quantity")
                  .toInteger() == 1);
        store.execute(command("Undo"));
        store.execute(command("Redo"));
        CHECK(store.assemblyModel(doc).occurrences[1].suppressed);
        // Nested definitions share source parts; cycles are rejected even through suppressed nodes.
        create(subId, "Assembly", "Subassembly");
        doc = subId;
        assembly::Model sub;
        assembly::Occurrence child;
        child.definition = part;
        child.grounded = true;
        sub.occurrences.push_back(child);
        store.execute(command("SetAssembly", fields({{"assembly", sub.map()}})));
        doc = assemblyId;
        model = store.assemblyModel(doc);
        model.occurrences[1].suppressed = false;
        model.occurrences[1].definition = subId;
        store.execute(command("SetAssembly", fields({{"assembly", model.map()}})));
        CHECK(store.assemblyResult(doc)
                  .value("display_manifest")
                  .toMap()
                  .value("instances")
                  .toArray()
                  .size() == 2);
        doc = subId;
        sub.occurrences[0].definition = assemblyId;
        rejects([&] { store.execute(command("SetAssembly", fields({{"assembly", sub.map()}}))); });
        doc = assemblyId;
        auto saved = files.filePath("assembly.cadproj");
        store.save(command("SaveProject"), saved);
        auto copy = files.filePath("portable.cadproj"), copyId = newId();
        store.save(
            command("PortableCopy", fields({{"new_project_id", copyId}, {"policy", "Copied"}})),
            copy, true);
        CHECK(store.project() == project);
        CHECK(QFile::remove(saved));
        store.close();
        store.open(copy, "Saved");
        CHECK(store.project() == copyId);
        store.trimResults();
        reply = store.assemblyResult(doc);
        CHECK(reply.value("display_manifest").toMap().value("instances").toArray().size() == 2);
        auto accepted = store.revision();
        store.close();
        store.recover(copyId);
        CHECK(store.revision() == accepted);
        CHECK(store.assemblyModel(doc).occurrences[0].id == model.occurrences[0].id);
        CHECK(store.assemblyModel(doc).occurrences[1].definition == subId);
        // A real top face moves after an upstream depth edit. Never guess its replacement.
        model = store.assemblyModel(doc);
        model.occurrences[1].definition = part;
        auto top = [&](QString occurrence) {
            auto refs = store.assemblyReferences(doc, a.id).value("candidates").toArray();
            for (auto value : refs) {
                auto r = assembly::Reference::parse(value.toMap().value("reference").toMap());
                if (r.body.isEmpty())
                    continue;
                auto frame = assembly::reference(r, model, store.assemblyLoader());
                if (frame.kind == "Plane" && std::abs(frame.direction[2]) > .9 &&
                    frame.origin[2] > .009) {
                    r.occurrence = occurrence;
                    return r;
                }
            }
            throw std::runtime_error("Expected B1 top face");
        };
        assembly::Mate exactMate;
        exactMate.kind = "Distance";
        exactMate.value = .005;
        exactMate.first = top(a.id);
        exactMate.second = top(b.id);
        model.mates = {exactMate};
        store.execute(command("SetAssembly", fields({{"assembly", model.map()}})));
        CHECK(store.assemblyResult(doc).value("assembly_result").toMap().value("valid").toBool());
        doc = part;
        auto changedFeatures = features::parse(fixture.features());
        changedFeatures[0].parameters.insert(QStringLiteral("depth"), .012);
        changedFeatures[2].parameters.insert(QStringLiteral("origin"), QCborArray{.01, .01, .012});
        store.execute(
            command("SetFeatures", fields({{"features", features::encode(changedFeatures)}})));
        doc = assemblyId;
        reply = store.assemblyResult(doc);
        CHECK(!reply.value("assembly_result").toMap().value("valid").toBool());
        CHECK(reply.value("part_stale").toBool());
        rejects([&] { store.assemblyInterference(doc); });
        model = store.assemblyModel(doc);
        model.mates[0].first = top(a.id);
        model.mates[0].second = top(b.id);
        store.execute(command("SetAssembly", fields({{"assembly", model.map()}})));
        CHECK(store.assemblyResult(doc).value("assembly_result").toMap().value("valid").toBool());
        auto broken = model;
        broken.occurrences[1].definition = newId();
        CHECK(!store.assemblyResult(doc, broken.map())
                   .value("assembly_result")
                   .toMap()
                   .value("valid")
                   .toBool());
        auto replacement = newId();
        create(replacement, "Part", "Replacement definition");
        doc = replacement;
        fixture4::B1 other;
        store.execute(command("SetSketches", fields({{"sketches", other.sketches()}})));
        store.execute(command("SetFeatures", fields({{"features", other.features()}})));
        doc = assemblyId;
        model = store.assemblyModel(doc);
        model.occurrences[1].definition = replacement;
        CHECK(!store.assemblyResult(doc, model.map())
                   .value("assembly_result")
                   .toMap()
                   .value("valid")
                   .toBool());
        // An explicit new datum binding is a user repair; occurrence identity stays stable.
        model.mates[0].first.body.clear();
        model.mates[0].first.token = "XY Plane";
        model.mates[0].second.body.clear();
        model.mates[0].second.token = "XY Plane";
        store.execute(command("SetAssembly", fields({{"assembly", model.map()}})));
        CHECK(store.assemblyModel(doc).occurrences[1].id == b.id);
        store.execute(command("Undo"));
        CHECK(store.assemblyModel(doc).occurrences[1].definition == part);
        std::cout << "Assembly durability: shared instances, exact refs, mate conflicts/preview, "
                     "states/cache/unload, nested cycle refusal, BOM source quantities, portable "
                     "copy without originals and restart recovery passed\n";
    });
}
