#include "ModelingFixture.hpp"
#include "features/ModelingRecords.hpp"
#include "geometry/occt/ModelingFeatures.hpp"
#include "persistence/ProjectStore.hpp"
#include <BRepAdaptor_Surface.hxx>
#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
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

double volume(const TopoDS_Shape &s) {
    GProp_GProps g;
    BRepGProp::VolumeProperties(s, g);
    return g.Mass() * 1e-9;
}

domain::GeometryReference findPlanarFace(const TopoDS_Shape &shape, QString object, gp_Dir normal,
                                         double zTarget, double tol = 1e-3) {
    domain::GeometryReference r = support_fixture::ref(object, "Face");
    for (TopExp_Explorer ex(shape, TopAbs_FACE); ex.More(); ex.Next()) {
        BRepAdaptor_Surface surf(TopoDS::Face(ex.Current()));
        if (surf.GetType() == GeomAbs_Plane) {
            auto dir = surf.Plane().Axis().Direction();
            if (dir.IsParallel(normal, 0.1)) {
                double dist = surf.Plane().Distance(gp_Pnt(0, 0, zTarget * 1000.0)) / 1000.0;
                if (dist <= tol) {
                    r.topology = geometry::supportToken(ex.Current());
                    return r;
                }
            }
        }
    }
    throw std::runtime_error("Planar face not found");
}

} // namespace

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    return run([] {
        QTemporaryDir root(QDir::tempPath() + "/cad-module8-cm8-store-XXXXXX"),
            files(QDir::tempPath() + "/cad-module8-cm8-saved-XXXXXX");
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
                                                        {"name", "CM8 Store Test"},
                                                        {"units", "mm"}})));

        // 1. Base extrude block: 100 x 100 x 50 mm
        sketch::Sketch s;
        s.rectangle({0, 0}, {0.100, 0.100});

        // Loft section 1 at z = 0, R = 20 mm
        sketch::Sketch loft1;
        loft1.name = "LoftSection1";
        loft1.frame.origin = {0.200, 0, 0};
        auto c1 = loft1.point({0, 0});
        loft1.circle(c1, 0.020);

        // Loft section 2 at z = 100 mm, R = 10 mm
        sketch::Sketch loft2;
        loft2.name = "LoftSection2";
        loft2.frame.origin = {0.200, 0, 0.100};
        auto c2 = loft2.point({0, 0});
        loft2.circle(c2, 0.010);

        // Sweep path along Z from (0.400, 0, 0) to (0.400, 0, 0.100)
        sketch::Sketch sweepPath;
        sweepPath.name = "SweepPath";
        sweepPath.frame.origin = {0.400, 0, 0};
        sweepPath.frame.u = {0, 0, 1};
        sweepPath.frame.v = {1, 0, 0};
        auto sp1 = sweepPath.point({0, 0});
        auto sp2 = sweepPath.point({0.100, 0});
        auto spLine = sweepPath.line(sp1, sp2);

        auto d = base({s, loft1, loft2, sweepPath});
        d.document = document;

        auto baseExt = extrude(s.id);
        baseExt.insert(QStringLiteral("direction1"), blind(0.050));
        auto baseId = baseExt.value("id").toString();

        auto evalBase = geometry::evaluateModeling(d, QCborArray{baseExt});
        auto topFaceRef =
            findPlanarFace(evalBase.part.bodies[0].shape, baseId, gp_Dir(0, 0, 1), 0.050);

        // CM8 Shell intent (t = 5 mm, inward)
        auto shellIntent = fields({{"version", 1},
                                   {"id", newId()},
                                   {"kind", "Shell"},
                                   {"target", body(baseId)},
                                   {"thickness", 0.005},
                                   {"inward", true},
                                   {"remove_faces", QCborArray{topFaceRef.map()}},
                                   {"overrides", QCborArray{}}});

        // CM8 Sweep intent (circular pipe D = 20 mm, thin t = 2 mm)
        auto sweepIntent = fields(
            {{"version", 1},
             {"id", newId()},
             {"kind", "Sweep"},
             {"operation", "NewBody"},
             {"target", QCborValue(nullptr)},
             {"profile", fields({{"type", "Circular"}, {"diameter", 0.020}})},
             {"path",
              fields({{"reference", support_fixture::ref(sweepPath.id, "Curve", spLine).map()},
                      {"reverse", false}})},
             {"guides", QCborArray{}},
             {"corner_transition", "Round"},
             {"start_tangency", "None"},
             {"end_tangency", "None"},
             {"thin", fields({{"mode", "OneDirection"}, {"thickness", 0.002}, {"reverse", false}})},
             {"preview_sections", 0}});

        // CM8 Loft intent (ordered solid loft between loft1 and loft2)
        auto loftIntent = fields(
            {{"version", 1},
             {"id", newId()},
             {"kind", "Loft"},
             {"output", "Solid"},
             {"operation", "NewBody"},
             {"target", QCborValue(nullptr)},
             {"sections",
              QCborArray{
                  fields({{"sketch", loft1.id}, {"mode", "AllClosed"}, {"regions", QCborArray{}}}),
                  fields(
                      {{"sketch", loft2.id}, {"mode", "AllClosed"}, {"regions", QCborArray{}}})}},
             {"centerline", QCborValue(nullptr)},
             {"guides", QCborArray{}},
             {"connectors", QCborArray{}},
             {"start_condition", "None"},
             {"end_condition", "None"},
             {"thin", fields({{"mode", "Solid"}})}});

        // 1. Commit CM8 features to store
        auto commit = command(
            "SetModelSupports",
            fields({{"model", d.map()},
                    {"intents", QCborArray{baseExt, shellIntent, sweepIntent, loftIntent}}}));
        store.execute(commit);
        auto revision = store.revision();
        CHECK(revision > 0);

        auto state = store.featureState(document);
        auto intents = state.value("modeling").toMap().value("intents").toArray();
        CHECK(intents.size() == 4);

        auto evaluatedPart = store.evaluatePart(document);
        CHECK(evaluatedPart.bodies.size() == 3); // 1 shelled body + 1 sweep body + 1 loft body

        // 2. Save project to file and verify manifest requirement 13 (cm8Manifest)
        auto savedPath = files.filePath("cm8-complete.cadproj");
        store.save(command("SaveProject"), savedPath);

        // Verify saved database metadata
        {
            storage::Database savedDb(savedPath);
            auto savedManifest = savedDb.scalar("SELECT manifest FROM project_meta").toString();
            CHECK(savedManifest.contains(";cm8-sweep-loft-shell=1"));
        }

        // 3. Reopen saved project in fresh store and verify exact parity
        store.close();
        store.open(savedPath, "Saved");

        auto reloadedPart = store.evaluatePart(document);
        CHECK(reloadedPart.bodies.size() == 3);

        for (int b = 0; b < 3; ++b) {
            double origVol = volume(evaluatedPart.bodies[b].shape);
            double reloadVol = volume(reloadedPart.bodies[b].shape);
            near(origVol, reloadVol, 1e-9);
        }

        // 4. Test Undo / Redo
        auto revBeforeUndo = store.revision();
        store.execute(command("Undo"));
        CHECK(store.revision() > revBeforeUndo);
        // Prior state had 0 modeling features
        CHECK(store.featureState(document)
                  .value("modeling")
                  .toMap()
                  .value("intents")
                  .toArray()
                  .isEmpty());

        auto revBeforeRedo = store.revision();
        store.execute(command("Redo"));
        CHECK(store.revision() > revBeforeRedo);
        CHECK(store.featureState(document)
                  .value("modeling")
                  .toMap()
                  .value("intents")
                  .toArray()
                  .size() == 4);
        CHECK(store.evaluatePart(document).bodies.size() == 3);

        puts("PASS: cm8 store");
    });
}
