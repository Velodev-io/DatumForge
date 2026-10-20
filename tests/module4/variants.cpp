#include "Fixture.hpp"
#include "Projects.hpp"
#include "support/check.hpp"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <numbers>
using namespace cad;
using sketch::fields;
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    return run([] {
        QTemporaryDir root(QDir::tempPath() + "/cad-module4-variants-XXXXXX"),
            files(QDir::tempPath() + "/cad-module4-files-XXXXXX");
        for (auto *r : {&root, &files}) {
            CHECK(r->isValid());
            QFile f(r->filePath(".cad-test-root"));
            CHECK(f.open(QIODevice::WriteOnly));
            f.write("disposable\n");
        }
        worker::Projects projects(root.path());
        for (auto type : {"Extrude", "Revolve", "Fillet", "Chamfer", "Hole", "LinearPattern",
                          "CircularPattern", "Mirror"}) {
            fixture4::B1 b;
            sketch::Sketch radial;
            radial.rectangle({.01, 0}, {.02, .01});
            auto sketches = b.sketches();
            auto base = b.base;
            std::vector<features::Feature> definitions{base};
            features::Feature f;
            f.name = type;
            f.kind = type;
            if (f.kind == "Extrude") {
                f = b.cutFeature;
            } else if (f.kind == "Revolve") {
                definitions.clear();
                sketches = sketch::encode({radial});
                f.parameters = fields({{"sketch", radial.id},
                                       {"operation", "NewBody"},
                                       {"target", ""},
                                       {"angle", 2 * std::numbers::pi},
                                       {"direction", "Forward"},
                                       {"axis_origin", QCborArray{0., 0., 0.}},
                                       {"axis_direction", QCborArray{0., 1., 0.}}});
            } else if (f.kind == "Hole")
                f = b.hole;
            else if (f.kind == "Fillet" || f.kind == "Chamfer") {
                auto part = kernel::Part::build(sketches, features::encode({base}));
                auto edge = fixture4::filletEdge(part.topology());
                CHECK(!edge.isEmpty());
                f.parameters = fields({{"target", base.id},
                                       {"source", base.id},
                                       {"edges", QCborArray{edge}},
                                       {"size", .001}});
            } else if (f.kind == "LinearPattern")
                f.parameters = fields({{"source", base.id},
                                       {"target", ""},
                                       {"operation", "NewBody"},
                                       {"direction", QCborArray{1., 0., 0.}},
                                       {"count", 2},
                                       {"spacing", .1}});
            else if (f.kind == "CircularPattern")
                f.parameters = fields({{"source", base.id},
                                       {"target", ""},
                                       {"operation", "NewBody"},
                                       {"axis_origin", QCborArray{0., 0., 0.}},
                                       {"axis_direction", QCborArray{0., 0., 1.}},
                                       {"count", 4},
                                       {"angle", 2 * std::numbers::pi},
                                       {"distribution", "Full"}});
            else
                f.parameters = fields({{"source", base.id},
                                       {"target", ""},
                                       {"operation", "NewBody"},
                                       {"origin", QCborArray{.1, 0., 0.}},
                                       {"normal", QCborArray{1., 0., 0.}}});
            definitions.push_back(f);
            auto complete = features::encode(definitions);
            auto project = newId(), doc = newId();
            auto cmd = [&](QString name, QCborMap p) {
                wire::Envelope e;
                e.context.project = project;
                e.context.document = doc;
                e.command = name;
                e.payload = p;
                e.revision = projects.revision();
                return e;
            };
            auto create = cmd("NewProject", fields({{"id", doc},
                                                    {"properties_entity", newId()},
                                                    {"type", "Part"},
                                                    {"name", QString(type)},
                                                    {"units", "in"},
                                                    {"replace", true}}));
            projects.run(create);
            projects.run(cmd("SetSketches", fields({{"sketches", sketches}})));
            auto rev = projects.revision();
            auto preview = projects.run(
                cmd("PreviewFeatures", fields({{"features", complete}, {"deflection_mm", .2}})));
            CHECK(projects.revision() == rev);
            CHECK(preview.value("part_measurement").toMap().value("valid").toBool());
            CHECK(!preview.value("topology_history").toArray().empty());
            // Discarding a preview is absence of a commit, even after a subsequent failure.
            auto bad = complete;
            auto invalid = bad.last().toMap();
            auto p = invalid.value("parameters").toMap();
            p.insert(QStringLiteral("unsupported_variant"), true);
            invalid.insert(QStringLiteral("parameters"), p);
            bad[bad.size() - 1] = invalid;
            rejects([&] {
                projects.run(
                    cmd("PreviewFeatures", fields({{"features", bad}, {"deflection_mm", .2}})));
            });
            CHECK(projects.revision() == rev);
            auto accepted = cmd("SetFeatures", fields({{"features", complete}}));
            projects.run(accepted);
            projects.run(accepted);
            CHECK(projects.revision() == rev + 1);
            auto changed = cmd("SetFeatures", fields({{"features", bad}}));
            rejects([&] { projects.run(changed); });
            CHECK(projects.revision() == rev + 1);
            auto token = newId(), path = files.filePath(QString(type) + ".cadproj");
            projects.run(
                cmd("GrantFile", fields({{"token", token}, {"path", path}, {"access", "save"}})));
            projects.run(cmd("SaveProject", fields({{"capability", token}})));
            auto density = cmd("SetParameter", fields({{"name", "MaterialDensityKgM3"},
                                                       {"dimension", "scalar"},
                                                       {"value_si", -1.}}));
            rejects([&] { projects.run(density); });
            projects.run(cmd("Undo", {}));
            projects.run(cmd("Redo", {}));
            auto displayed = projects.run(cmd("DisplayPart", fields({{"deflection_mm", 1.}})));
            near(displayed.value("part_measurement").toMap().value("volume_m3").toDouble(),
                 preview.value("part_measurement").toMap().value("volume_m3").toDouble(), 1e-12);
            auto stale = cmd("MeasurePart", fields({{"body", QString()},
                                                    {"token", QString()},
                                                    {"second_token", QString()},
                                                    {"density_kg_m3", 7800.}}));
            stale.revision--;
            rejects([&] { projects.run(stale); });
            projects.run(cmd("CloseProject", fields({{"choice", "KeepRecoverable"}})));
            token = newId();
            projects.run(
                cmd("GrantFile", fields({{"token", token}, {"path", path}, {"access", "read"}})));
            auto opened = projects.run(
                cmd("OpenProject",
                    fields({{"capability", token}, {"choice", "Saved"}, {"replace", true}})));
            auto docs = opened.value("project_state").toMap().value("documents").toArray();
            CHECK(docs[0].toMap().value("features").toArray() == complete);
            projects.run(cmd("CloseProject", fields({{"choice", "KeepRecoverable"}})));
            std::cout << "Verified " << type
                      << ": valid/invalid preview, discard, commit/dedup, invalid commit "
                         "preservation, exact mesh invariance, undo/redo, save/reopen\n";
        }
    });
}
