#include "ModelingFixture.hpp"
#include "geometry/occt/ModelingFeatures.hpp"
#include "persistence/ProjectStore.hpp"
#include <BRepAdaptor_Surface.hxx>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
using namespace cad;
using namespace modeling_fixture;
using support_fixture::error;
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    return run([] {
        QTemporaryDir root(QDir::tempPath() + "/cad-module5-cm5-repair-XXXXXX");
        CHECK(root.isValid());
        QFile marker(root.filePath(".cad-test-root"));
        CHECK(marker.open(QIODevice::WriteOnly));
        marker.write("disposable\n");
        marker.close();
        storage::ProjectStore store(root.path());
        auto project = newId(), document = newId();
        store.create(project);
        auto command = [&](QString name, QCborMap payload = QCborMap{}) {
            wire::Envelope e;
            e.command = name;
            e.payload = payload;
            e.context.project = project;
            e.context.document = document;
            e.revision = store.revision();
            return e;
        };
        store.execute(command("CreateDocument", fields({{"id", document},
                                                        {"properties_entity", newId()},
                                                        {"type", "Part"},
                                                        {"name", "Repair"},
                                                        {"units", "mm"}})));
        sketch::Sketch s, p;
        s.rectangle({0, 0}, {.04, .03});
        p.rectangle({.005, .005}, {.015, .015});
        p.frame.origin[2] = -.005;
        auto d = base({s, p});
        d.document = document;
        auto first = extrude(s.id);
        auto owner = first.value("id").toString();
        auto initial = geometry::evaluateModeling(d, QCborArray{first});
        auto top = support_fixture::ref(owner, "Face");
        for (TopExp_Explorer f(initial.part.stages.value(owner), TopAbs_FACE); f.More(); f.Next()) {
            BRepAdaptor_Surface surface(TopoDS::Face(f.Current()));
            if (surface.GetType() == GeomAbs_Plane &&
                std::abs(surface.Plane().Location().Z() - 10) < 1e-8)
                top.topology = geometry::supportToken(f.Current());
        }
        CHECK(!top.topology.isEmpty());
        auto second = extrude(p.id);
        second.insert(QStringLiteral("direction1"), fields({{"mode", "UpToSurface"},
                                                            {"reference", top.map()},
                                                            {"draft", blind().value("draft")}}));
        store.execute(
            command("SetModelSupports",
                    fields({{"model", d.map()}, {"intents", QCborArray{first, second}}})));
        auto accepted = store.featureState(document);
        auto revision = store.revision();
        first.insert(QStringLiteral("direction1"), blind(.02));
        QCborArray pending{first, second};
        error("ReferenceMissing", [&] {
            store.execute(
                command("SetModelSupports", fields({{"model", d.map()}, {"intents", pending}})));
        });
        CHECK(store.featureState(document) == accepted && store.revision() == revision);
        auto inspection = store.modelingReferenceCandidates(
            document, d.map(), pending, second.value("id").toString(), "direction1/reference");
        CHECK(inspection.value("state") == "Missing");
        QCborMap chosen;
        for (auto value : inspection.value("descriptions").toArray()) {
            auto m = value.toMap();
            if (m.value("geometry_type") == "Plane" &&
                std::abs(m.value("center_si").toArray()[2].toDouble() - .02) < 1e-9)
                chosen = m.value("reference").toMap();
        }
        CHECK(!chosen.empty());
        auto payload = fields({{"model", d.map()},
                               {"intents", pending},
                               {"owner", second.value("id")},
                               {"path", "direction1/reference"},
                               {"replacement", chosen},
                               {"fingerprint", inspection.value("fingerprint")}});
        auto stale = payload;
        stale.insert(QStringLiteral("fingerprint"), QByteArray("stale"));
        error("StaleRevision", [&] { store.execute(command("ResolveModelingReference", stale)); });
        auto forged = payload;
        forged.insert(QStringLiteral("replacement"), top.map());
        error("InvalidInput", [&] { store.execute(command("ResolveModelingReference", forged)); });
        CHECK(store.revision() == revision);
        auto repair = command("ResolveModelingReference", payload);
        store.execute(repair);
        store.execute(repair);
        CHECK(store.revision() == revision + 1);
        near(store.evaluatePart(document).measure(0).value("volume_m3").toDouble(),
             .04 * .03 * .02 + .01 * .01 * .025, 1e-12);
        auto repaired = store.featureState(document);
        store.execute(command("Undo"));
        CHECK(store.featureState(document) == accepted);
        store.execute(command("Redo"));
        CHECK(store.featureState(document) == repaired);
        sketch::Sketch slit;
        slit.rectangle({.019, -.001}, {.021, .031});
        slit.frame.origin[2] = .005;
        auto splitModel = base({s, p, slit});
        splitModel.document = document;
        auto seed = extrude(s.id);
        seed.insert(QStringLiteral("id"), owner);
        auto cut = extrude(slit.id, "Cut", body(owner));
        cut.insert(QStringLiteral("direction1"), blind(.005));
        auto splitRef = top;
        splitRef.object = cut.value("id").toString();
        auto dependent = second;
        dependent.insert(QStringLiteral("direction1"), fields({{"mode", "UpToSurface"},
                                                               {"reference", splitRef.map()},
                                                               {"draft", blind().value("draft")}}));
        QCborArray split{seed, cut, dependent};
        revision = store.revision();
        error("ReferenceAmbiguous", [&] {
            store.execute(command("SetModelSupports",
                                  fields({{"model", splitModel.map()}, {"intents", split}})));
        });
        CHECK(store.revision() == revision && store.featureState(document) == repaired);
        auto ambiguous = store.modelingReferenceCandidates(document, splitModel.map(), split,
                                                           dependent.value("id").toString(),
                                                           "direction1/reference");
        CHECK(ambiguous.value("state") == "Split");
        CHECK(ambiguous.value("candidates").toArray().size() == 2);
        chosen = {};
        for (auto value : ambiguous.value("descriptions").toArray()) {
            auto m = value.toMap();
            auto c = m.value("center_si").toArray();
            if (m.value("geometry_type") == "Plane" && std::abs(c[2].toDouble() - .01) < 1e-9 &&
                c[0].toDouble() < .019)
                chosen = m.value("reference").toMap();
        }
        CHECK(!chosen.empty());
        store.execute(command("ResolveModelingReference",
                              fields({{"model", splitModel.map()},
                                      {"intents", split},
                                      {"owner", dependent.value("id")},
                                      {"path", "direction1/reference"},
                                      {"replacement", chosen},
                                      {"fingerprint", ambiguous.value("fingerprint")}})));
        auto final = store.featureState(document);
        CHECK(!final.value("support_evolution").toArray().empty());
        CHECK(!final.value("modeling")
                   .toMap()
                   .value("provenance")
                   .toArray()[1]
                   .toMap()
                   .value("history")
                   .toArray()
                   .empty());
        QTemporaryDir files(QDir::tempPath() + "/cad-module5-cm5-repair-saved-XXXXXX");
        CHECK(files.isValid());
        QFile mark(files.filePath(".cad-test-root"));
        CHECK(mark.open(QIODevice::WriteOnly));
        mark.write("disposable\n");
        mark.close();
        auto saved = files.filePath("repaired.cadproj");
        store.save(command("SaveProject"), saved);
        store.close();
        store.open(saved, "Saved");
        CHECK(store.featureState(document) == final);
        store.execute(command("Undo"));
        CHECK(store.featureState(document) == repaired);
        store.execute(command("Redo"));
        CHECK(store.featureState(document) == final);
    });
}
