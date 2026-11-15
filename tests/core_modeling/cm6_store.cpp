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
#include <numbers>

using namespace cad;
using namespace modeling_fixture;
using support_fixture::error;

namespace {
void mark(QTemporaryDir &root) {
    CHECK(root.isValid());
    QFile f(root.filePath(".cad-test-root"));
    CHECK(f.open(QIODevice::WriteOnly));
    f.write("disposable\n");
}

domain::GeometryReference findFace(const TopoDS_Shape &shape, QString object, double zMin = -1e9,
                                   double zMax = 1e9) {
    domain::GeometryReference r = support_fixture::ref(object, "Face");
    for (TopExp_Explorer ex(shape, TopAbs_FACE); ex.More(); ex.Next()) {
        BRepAdaptor_Surface surf(TopoDS::Face(ex.Current()));
        if (surf.GetType() == GeomAbs_Plane) {
            double z = surf.Plane().Location().Z();
            if (z >= zMin && z <= zMax) {
                r.topology = geometry::supportToken(ex.Current());
                return r;
            }
        }
    }
    for (TopExp_Explorer ex(shape, TopAbs_FACE); ex.More(); ex.Next()) {
        r.topology = geometry::supportToken(ex.Current());
        return r;
    }
    return r;
}

domain::GeometryReference findEdge(const TopoDS_Shape &shape, QString object) {
    domain::GeometryReference r = support_fixture::ref(object, "Edge");
    for (TopExp_Explorer ex(shape, TopAbs_EDGE); ex.More(); ex.Next()) {
        r.topology = geometry::supportToken(ex.Current());
        return r;
    }
    return r;
}
} // namespace

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    return run([] {
        QTemporaryDir root(QDir::tempPath() + "/cad-module6-cm6-store-XXXXXX"),
            files(QDir::tempPath() + "/cad-module6-cm6-saved-XXXXXX");
        mark(root);
        mark(files);

        storage::ProjectStore store(root.path());
        auto project = newId(), document = newId();
        store.create(project);

        auto command = [&](QString name, QCborMap payload = QCborMap{}) {
            wire::Envelope e;
            e.command = name;
            e.context.project = project;
            e.context.document = document;
            e.revision = store.revision();
            e.payload = payload;
            return e;
        };

        store.execute(command("CreateDocument", fields({{"id", document},
                                                        {"properties_entity", newId()},
                                                        {"type", "Part"},
                                                        {"name", "CM6 Store Test"},
                                                        {"units", "mm"}})));

        // Base plate: 100 mm x 100 mm x 20 mm
        sketch::Sketch s;
        auto rectLines = s.rectangle({0, 0}, {0.100, 0.100});

        sketch::Sketch plSk;
        plSk.name = "HolePositions";
        plSk.frame.origin = {0, 0, 0.020};
        auto pt1 = plSk.point({0.025, 0.025});

        auto d = base({s, plSk});
        d.document = document;

        auto baseExt = extrude(s.id);
        baseExt.insert(QStringLiteral("direction1"), blind(0.020));
        auto baseId = baseExt.value("id").toString();

        auto evalBase = geometry::evaluateModeling(d, QCborArray{baseExt});
        auto topFace = findFace(evalBase.part.bodies[0].shape, baseId, 19.0, 21.0);

        // CM6 Hole intent
        auto hole = fields(
            {{"version", 1},
             {"id", newId()},
             {"kind", "Hole"},
             {"operation", "Cut"},
             {"target", body(baseId)},
             {"family", "Simple"},
             {"placement",
              fields({{"face", topFace.map()}, {"sketch", plSk.id}, {"points", QCborArray{pt1}}})},
             {"sizing", fields({{"mode", "Custom"}, {"diameter", 0.010}, {"depth", 0.020}})},
             {"termination", fields({{"mode", "ThroughAll"}, {"depth_reference", "Shoulder"}})}});
        auto holeId = hole.value("id").toString();

        auto evalHole = geometry::evaluateModeling(d, QCborArray{baseExt, hole});
        auto topEdge = findEdge(evalHole.part.bodies[0].shape, holeId);

        // CM6 Fillet intent on top edge
        auto fillet = fields({{"version", 1},
                              {"id", newId()},
                              {"kind", "Fillet"},
                              {"target", body(holeId, baseId)},
                              {"method", "Constant"},
                              {"propagate_tangent", true},
                              {"radius", 0.002},
                              {"edges", QCborArray{topEdge.map()}}});
        auto filletId = fillet.value("id").toString();

        // 1. Commit CM6 features to store
        auto commit =
            command("SetModelSupports",
                    fields({{"model", d.map()}, {"intents", QCborArray{baseExt, hole, fillet}}}));
        store.execute(commit);
        auto revision = store.revision();
        CHECK(revision > 0);

        CHECK(store.featureState(document)
                  .value("modeling")
                  .toMap()
                  .value("intents")
                  .toArray()
                  .size() == 3);
        CHECK(store.evaluatePart(document).bodies.size() == 1);
        double evaluatedVol = store.evaluatePart(document).measure(0).value("volume_m3").toDouble();
        CHECK(evaluatedVol > 0 && evaluatedVol < 0.100 * 0.100 * 0.020);

        // 2. Save project and verify manifest requirement 11
        auto path = files.filePath("cm6-complete.cadproj");
        store.save(command("SaveProject"), path);

        {
            storage::Database db(path);
            auto manifest = db.scalar("SELECT manifest FROM project_meta").toString();
            CHECK(manifest.contains(";cm6-features=1"));
        }

        // 3. Roundtrip reopen and verify exact geometry
        store.close();
        store.open(path, "Saved");
        CHECK(store.evaluatePart(document).bodies.size() == 1);
        near(store.evaluatePart(document).measure(0).value("volume_m3").toDouble(), evaluatedVol,
             1e-12);

        // 4. Recovery check
        store.close();
        store.recover(project);
        near(store.evaluatePart(document).measure(0).value("volume_m3").toDouble(), evaluatedVol,
             1e-12);

        // 5. Schema enforcement test: downgrade manifest to modeling-intents=1 must be rejected
        auto forgedPath = files.filePath("forged-downgrade.cadproj");
        CHECK(QFile::copy(path, forgedPath));
        {
            storage::Database db(forgedPath);
            auto m = db.scalar("SELECT manifest FROM project_meta").toString();
            CHECK(m.contains(";cm6-features=1"));
            m.replace(";cm6-features=1", "");
            db.run("UPDATE project_meta SET manifest=?", {m});
        }
        store.close();
        error("IncompatibleSchema", [&] { store.open(forgedPath, "Saved"); });

        // 6. Test reference candidate repair with ResolveModelingReference
        store.open(path, "Saved");
        auto invalidEdge = topEdge;
        invalidEdge.topology =
            "edge/0000000000000000000000000000000000000000000000000000000000000000";
        auto badFillet = fillet;
        badFillet.insert(QStringLiteral("edges"), QCborArray{invalidEdge.map()});

        QCborArray pendingIntents{baseExt, hole, badFillet};
        error("ReferenceMissing", [&] {
            store.execute(command("SetModelSupports",
                                  fields({{"model", d.map()}, {"intents", pendingIntents}})));
        });

        // Inspect candidates for fillet edge path "edges/0"
        auto inspection = store.modelingReferenceCandidates(document, d.map(), pendingIntents,
                                                            filletId, "edges/0");
        CHECK(inspection.value("state") == "Missing");
        auto candidates = inspection.value("replacements").toArray();
        CHECK(!candidates.empty());

        auto chosen = candidates[0].toMap();
        store.execute(command("ResolveModelingReference",
                              fields({{"model", d.map()},
                                      {"intents", pendingIntents},
                                      {"owner", filletId},
                                      {"path", "edges/0"},
                                      {"replacement", chosen},
                                      {"fingerprint", inspection.value("fingerprint")}})));

        CHECK(store.evaluatePart(document).bodies.size() == 1);
        puts("cm6_store passed");
    });
}
