#include "../module4/Fixture.hpp"
#include "Projects.hpp"
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
        QTemporaryDir root(QDir::tempPath() + "/cad-module5-store-XXXXXX"),
            files(QDir::tempPath() + "/cad-module5-files-XXXXXX");
        for (auto *r : {&root, &files}) {
            CHECK(r->isValid());
            QFile f(r->filePath(".cad-test-root"));
            CHECK(f.open(QIODevice::WriteOnly));
            f.write("disposable\n");
        }
        worker::Projects worker(root.path());
        auto project = newId(), doc = newId();
        auto command = [&](QString name, QCborMap payload = QCborMap{}) {
            wire::Envelope e;
            e.command = name;
            e.context.project = project;
            e.context.document = doc;
            e.revision = worker.revision();
            e.payload = payload;
            return e;
        };
        worker.run(command("NewProject", fields({{"id", doc},
                                                 {"properties_entity", newId()},
                                                 {"name", "History part"},
                                                 {"type", "Part"},
                                                 {"units", "mm"},
                                                 {"replace", false}})));
        fixture4::B1 b;
        worker.run(command("SetSketches", fields({{"sketches", b.sketches()}})));
        auto base = kernel::Part::build(b.sketches(), features::encode({b.base}));
        QString topEdge;
        for (auto value : base.topology()) {
            auto m = value.toMap();
            auto c = m.value("center_si").toArray();
            if (m.value("token").toString().startsWith("edge/") &&
                std::abs(c[0].toDouble() - .04) < 1e-10 && std::abs(c[1].toDouble()) < 1e-10 &&
                std::abs(c[2].toDouble() - .01) < 1e-10)
                topEdge = m.value("token").toString();
        }
        CHECK(!topEdge.isEmpty());
        auto fillet = b.fillet(topEdge);
        fillet.parameters.insert(QStringLiteral("source"), b.base.id);
        fillet.parameters.insert(QStringLiteral("size"), .001);
        auto defs = features::encode({b.base, fillet});
        worker.run(command("SetFeatures", fields({{"features", defs}})));
        auto edited = b.base;
        edited.parameters.insert(QStringLiteral("depth"), .012);
        auto bad = features::encode({edited, fillet});
        auto rev = worker.revision();
        auto preview = worker.run(command("PreviewHistory", fields({{"features", bad}})));
        CHECK(preview.value("part_stale").toBool());
        CHECK(worker.revision() == rev);
        auto commit = command("CommitHistory", fields({{"features", bad}}));
        worker.run(commit);
        worker.run(commit);
        CHECK(worker.revision() == rev + 1);
        auto measure = command("MeasurePart", fields({{"body", QString()},
                                                      {"token", QString()},
                                                      {"second_token", QString()},
                                                      {"density_kg_m3", 0.}}));
        rejects([&] { worker.run(measure); });
        auto resolution = worker
                              .run(command("ReferenceCandidates",
                                           fields({{"owner", fillet.id}, {"token", topEdge}})))
                              .value("reference_resolution")
                              .toMap();
        CHECK(resolution.value("state").toString() == "Candidate");
        CHECK(resolution.value("candidates").toArray().size() == 1);
        auto repair =
            command("ResolveReference",
                    fields({{"owner", fillet.id},
                            {"token", topEdge},
                            {"replacement", resolution.value("candidates").toArray()[0]},
                            {"source_fingerprint", resolution.value("source_fingerprint")}}));
        worker.run(repair);
        worker.run(repair);
        auto display = worker.run(command("DisplayPart", fields({{"deflection_mm", .2}})));
        CHECK(!display.value("part_stale").toBool());
        CHECK(display.value("part_measurement").toMap().value("valid").toBool());
        worker.run(command("Undo"));
        CHECK(worker.run(command("DisplayPart", fields({{"deflection_mm", .2}})))
                  .value("part_stale")
                  .toBool());
        worker.run(command("Redo"));
        CHECK(!worker.run(command("DisplayPart", fields({{"deflection_mm", .2}})))
                   .value("part_stale")
                   .toBool());
        auto path = files.filePath("history.cadproj"), token = newId();
        worker.run(
            command("GrantFile", fields({{"token", token}, {"path", path}, {"access", "save"}})));
        worker.run(command("SaveProject", fields({{"capability", token}})));
        worker.run(command("CloseProject", fields({{"choice", "KeepRecoverable"}})));
        token = newId();
        worker.run(
            command("GrantFile", fields({{"token", token}, {"path", path}, {"access", "read"}})));
        worker.run(
            command("OpenProject",
                    fields({{"capability", token}, {"choice", "Saved"}, {"replace", true}})));
        // A second document can consume a stable project-unique sketch identity. Parent edits
        // invalidate it.
        auto other = newId();
        worker.run(command("CreateDocument", fields({{"id", other},
                                                     {"properties_entity", newId()},
                                                     {"name", "Dependent part"},
                                                     {"type", "Part"},
                                                     {"units", "mm"}})));
        doc = other;
        auto shared = b.base;
        shared.id = newId();
        worker.run(command("SetFeatures", fields({{"features", features::encode({shared})}})));
        auto old = worker.run(command("DisplayPart", fields({{"deflection_mm", .2}})))
                       .value("part_measurement")
                       .toMap()
                       .value("volume_m3")
                       .toDouble();
        doc = commit.context.document;
        worker.run(command("SwitchDocument"));
        // Use a valid upstream sketch edit retaining all explicit edge roles after the repair.
        for (auto &e : b.outer.entities)
            if (e.kind == "Point" && std::abs(e.point.y - .05) < 1e-10)
                e.point.y = .06;
        worker.run(command("SetSketches", fields({{"sketches", b.sketches()}})));
        doc = other;
        worker.run(command("SwitchDocument"));
        auto current = worker.run(command("DisplayPart", fields({{"deflection_mm", .2}})))
                           .value("part_measurement")
                           .toMap()
                           .value("volume_m3")
                           .toDouble();
        CHECK(current > old);
        worker.run(command("TrimResults"));
        auto afterTrim = worker.run(command("DisplayPart", fields({{"deflection_mm", .2}})));
        near(afterTrim.value("part_measurement").toMap().value("volume_m3").toDouble(), current,
             1e-12);
    });
}
