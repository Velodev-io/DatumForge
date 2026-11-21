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
        QTemporaryDir root(QDir::tempPath() + "/cad-module8-cm8-repair-XXXXXX");
        mark(root);

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
                                                        {"name", "CM8 Repair Test"},
                                                        {"units", "mm"}})));

        // Base block: 100 x 100 x 50 mm
        sketch::Sketch s;
        s.rectangle({0, 0}, {0.100, 0.100});

        // Path sketches
        sketch::Sketch pathSk1;
        pathSk1.name = "Path1";
        pathSk1.frame.origin = {0, 0, 0};
        pathSk1.frame.u = {0, 0, 1};
        pathSk1.frame.v = {1, 0, 0};
        auto p1 = pathSk1.point({0, 0});
        auto p2 = pathSk1.point({0.050, 0});
        auto line1 = pathSk1.line(p1, p2);

        sketch::Sketch pathSk2;
        pathSk2.name = "Path2";
        pathSk2.frame.origin = {0, 0, 0};
        pathSk2.frame.u = {0, 0, 1};
        pathSk2.frame.v = {1, 0, 0};
        auto p3 = pathSk2.point({0, 0});
        auto p4 = pathSk2.point({0.080, 0});
        auto line2 = pathSk2.line(p3, p4);

        auto d = base({s, pathSk1, pathSk2});
        d.document = document;

        auto baseExt = extrude(s.id);
        baseExt.insert(QStringLiteral("direction1"), blind(0.050));
        auto baseId = baseExt.value("id").toString();

        auto evalBase = geometry::evaluateModeling(d, QCborArray{baseExt});
        auto topFaceRef =
            findPlanarFace(evalBase.part.bodies[0].shape, baseId, gp_Dir(0, 0, 1), 0.050);
        auto botFaceRef =
            findPlanarFace(evalBase.part.bodies[0].shape, baseId, gp_Dir(0, 0, 1), 0.000);

        // 1. Shell with removed top face and override on bottom face
        auto shellIntent =
            fields({{"version", 1},
                    {"id", newId()},
                    {"kind", "Shell"},
                    {"target", body(baseId)},
                    {"thickness", 0.005},
                    {"inward", true},
                    {"remove_faces", QCborArray{topFaceRef.map()}},
                    {"overrides",
                     QCborArray{fields({{"face", botFaceRef.map()}, {"thickness", 0.010}})}}});
        auto shellId = shellIntent.value("id").toString();

        // 2. Sweep along pathSk1
        auto sweepIntent = fields(
            {{"version", 1},
             {"id", newId()},
             {"kind", "Sweep"},
             {"operation", "NewBody"},
             {"target", QCborValue(nullptr)},
             {"profile", fields({{"type", "Circular"}, {"diameter", 0.010}})},
             {"path", fields({{"reference", support_fixture::ref(pathSk1.id, "Curve", line1).map()},
                              {"reverse", false}})},
             {"guides", QCborArray{}},
             {"corner_transition", "Round"},
             {"start_tangency", "None"},
             {"end_tangency", "None"},
             {"thin", fields({{"mode", "Solid"}})},
             {"preview_sections", 0}});
        auto sweepId = sweepIntent.value("id").toString();

        store.execute(
            command("SetModelSupports",
                    fields({{"model", d.map()},
                            {"intents", QCborArray{baseExt, shellIntent, sweepIntent}}})));
        CHECK(store.evaluatePart(document).bodies.size() == 2);

        // 3. Test replaceIntentReference for Sweep path
        auto currentIntents =
            store.featureState(document).value("modeling").toMap().value("intents").toArray();
        auto newPathRef = support_fixture::ref(pathSk2.id, "Curve", line2);
        auto repairedIntents =
            features::replaceIntentReference(currentIntents, sweepId, "path/reference", newPathRef);

        auto sweepParsed = repairedIntents[2].toMap();
        auto repairedPathRef = sweepParsed.value("path").toMap().value("reference").toMap();
        CHECK(repairedPathRef.value("object") == pathSk2.id);
        CHECK(repairedPathRef.value("entity") == line2);

        // Commit repaired sweep to store
        store.execute(command("SetModelSupports",
                              fields({{"model", d.map()}, {"intents", repairedIntents}})));
        CHECK(store.evaluatePart(document).bodies.size() == 2);

        // 4. Test replaceIntentReference for Shell remove_faces/0 and overrides/0/face
        auto sideFaceRef =
            findPlanarFace(evalBase.part.bodies[0].shape, baseId, gp_Dir(0, 1, 0), 0.100);
        auto currentIntents2 =
            store.featureState(document).value("modeling").toMap().value("intents").toArray();

        auto repairedShellIntents = features::replaceIntentReference(currentIntents2, shellId,
                                                                     "remove_faces/0", sideFaceRef);
        auto shellParsed = repairedShellIntents[1].toMap();
        CHECK(shellParsed.value("remove_faces").toArray()[0].toMap().value("topology") ==
              sideFaceRef.topology);

        auto repairedOverrideIntents = features::replaceIntentReference(
            repairedShellIntents, shellId, "overrides/0/face", topFaceRef);
        auto shellOverrideParsed = repairedOverrideIntents[1].toMap();
        CHECK(shellOverrideParsed.value("overrides")
                  .toArray()[0]
                  .toMap()
                  .value("face")
                  .toMap()
                  .value("topology") == topFaceRef.topology);

        // Verify query intentReference
        auto queriedRef = features::intentReference(shellOverrideParsed, "overrides/0/face");
        CHECK(queriedRef.topology == topFaceRef.topology);

        auto queriedPath = features::intentReference(repairedIntents[2].toMap(), "path/reference");
        CHECK(queriedPath.object == pathSk2.id);

        // 5. Test replaceIntentReference for higher array indices (> 31, up to 63)
        QCborArray multiRemoveFaces, multiOverrides;
        for (int i = 0; i < 40; ++i) {
            auto fakeFace = support_fixture::ref(baseId, "Face");
            fakeFace.topology = QString("face/%1").arg(i, 64, 16, QChar('0'));
            multiRemoveFaces.append(fakeFace.map());
            auto fakeOvFace = support_fixture::ref(baseId, "Face");
            fakeOvFace.topology = QString("face/%1").arg(100 + i, 64, 16, QChar('0'));
            multiOverrides.append(fields({{"face", fakeOvFace.map()}, {"thickness", 0.005}}));
        }
        auto multiShell = fields({{"version", 1},
                                  {"id", newId()},
                                  {"kind", "Shell"},
                                  {"target", body(baseId)},
                                  {"thickness", 0.005},
                                  {"inward", true},
                                  {"remove_faces", multiRemoveFaces},
                                  {"overrides", multiOverrides}});
        auto multiShellId = multiShell.value("id").toString();
        QCborArray multiIntents{baseExt, multiShell};

        auto rep35 = features::replaceIntentReference(multiIntents, multiShellId, "remove_faces/35",
                                                      sideFaceRef);
        auto rep35Shell = rep35[1].toMap();
        CHECK(rep35Shell.value("remove_faces").toArray()[35].toMap().value("topology") ==
              sideFaceRef.topology);

        auto rep35Ov =
            features::replaceIntentReference(rep35, multiShellId, "overrides/35/face", botFaceRef);
        auto rep35OvShell = rep35Ov[1].toMap();
        CHECK(rep35OvShell.value("overrides")
                  .toArray()[35]
                  .toMap()
                  .value("face")
                  .toMap()
                  .value("topology") == botFaceRef.topology);
        auto queried35 = features::intentReference(rep35OvShell, "overrides/35/face");
        CHECK(queried35.topology == botFaceRef.topology);

        puts("PASS: cm8 repair");
    });
}
