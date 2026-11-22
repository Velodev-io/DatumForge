#include "ModelingFixture.hpp"
#include "features/Catalogs.hpp"
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
        QTemporaryDir root(QDir::tempPath() + "/cad-module9-cm9-repair-XXXXXX");
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
                                                        {"name", "CM9 Repair Test"},
                                                        {"units", "mm"}})));

        // 1. Base extrude block: 100 x 100 x 50 mm
        sketch::Sketch s;
        s.rectangle({0, 0}, {0.100, 0.100});

        auto d = base({s});
        d.document = document;

        auto baseExt = extrude(s.id);
        baseExt.insert(QStringLiteral("direction1"), blind(0.050));
        auto baseId = baseExt.value("id").toString();

        auto evalBase = geometry::evaluateModeling(d, QCborArray{baseExt});
        auto topFaceRef =
            findPlanarFace(evalBase.part.bodies[0].shape, baseId, gp_Dir(0, 0, 1), 0.050);
        auto botFaceRef =
            findPlanarFace(evalBase.part.bodies[0].shape, baseId, gp_Dir(0, 0, 1), 0.000);
        auto sideFaceRef =
            findPlanarFace(evalBase.part.bodies[0].shape, baseId, gp_Dir(0, 1, 0), 0.100);

        // 2. Add Face AppearanceOverride and SectionView on top face
        features::AppearanceOverride faceOverride;
        faceOverride.id = newId();
        faceOverride.level = "Face";
        faceOverride.faceReference = topFaceRef;
        faceOverride.appearance.color_rgba = {0.2f, 0.8f, 0.4f, 1.0f};

        features::SectionView sectionViewA;
        sectionViewA.id = newId();
        sectionViewA.name = "Top Section";
        sectionViewA.active = true;
        sectionViewA.support = topFaceRef;
        sectionViewA.offset = -0.010;

        d.appearance_overrides = QCborArray{faceOverride.map()};
        d.section_views = QCborArray{sectionViewA.map()};

        store.execute(command("SetModelSupports",
                              fields({{"model", d.map()}, {"intents", QCborArray{baseExt}}})));
        CHECK(store.evaluatePart(document).bodies.size() == 1);

        // 3. Test supportReferenceCandidates inspection for AppearanceOverride
        auto inspOverride =
            store.supportReferenceCandidates(document, d.map(), faceOverride.id, 0, "face");
        CHECK(!inspOverride.value("replacements").toArray().isEmpty());
        CHECK(inspOverride.contains(QStringLiteral("fingerprint")));
        auto fingerprint1 = inspOverride.value("fingerprint").toByteArray();

        // 4. Test supportReferenceCandidates inspection for SectionView
        auto inspSection =
            store.supportReferenceCandidates(document, d.map(), sectionViewA.id, 0, "support");
        CHECK(!inspSection.value("replacements").toArray().isEmpty());
        CHECK(inspSection.contains(QStringLiteral("fingerprint")));

        // 5. Test ResolveSupportReference for AppearanceOverride (repair/retarget to sideFaceRef)
        auto repairOverrideCmd =
            command("ResolveSupportReference", fields({{"model", d.map()},
                                                       {"owner", faceOverride.id},
                                                       {"replacement", sideFaceRef.map()},
                                                       {"fingerprint", fingerprint1}}));
        store.execute(repairOverrideCmd);

        auto stateAfterOverrideRepair = store.featureState(document).value("supports").toMap();
        auto parsedAfterOverrideRepair = model::SupportDefinition::parse(stateAfterOverrideRepair);
        auto repOv = features::AppearanceOverride::parse(
            parsedAfterOverrideRepair.appearance_overrides[0].toMap());
        CHECK(repOv.faceReference.topology == sideFaceRef.topology);

        // 6. Test ResolveSupportReference for SectionView (repair/retarget to botFaceRef)
        auto inspSection2 = store.supportReferenceCandidates(
            document, parsedAfterOverrideRepair.map(), sectionViewA.id, 0, "support");
        auto fingerprint2 = inspSection2.value("fingerprint").toByteArray();

        auto repairSectionCmd =
            command("ResolveSupportReference", fields({{"model", parsedAfterOverrideRepair.map()},
                                                       {"owner", sectionViewA.id},
                                                       {"replacement", botFaceRef.map()},
                                                       {"fingerprint", fingerprint2}}));
        store.execute(repairSectionCmd);

        auto stateAfterSectionRepair = store.featureState(document).value("supports").toMap();
        auto parsedAfterSectionRepair = model::SupportDefinition::parse(stateAfterSectionRepair);
        auto repSv =
            features::SectionView::parse(parsedAfterSectionRepair.section_views[0].toMap());
        CHECK(repSv.support.topology == botFaceRef.topology);

        // 7. Verify broken reference behavior when face reference target is missing
        domain::GeometryReference missingFaceRef = support_fixture::ref(baseId, "Face");
        missingFaceRef.topology = "face/" + QString(64, 'f'); // non-existent face token

        features::AppearanceOverride brokenOverride;
        brokenOverride.id = newId();
        brokenOverride.level = "Face";
        brokenOverride.faceReference = missingFaceRef;

        auto brokenModel = parsedAfterSectionRepair;
        brokenModel.appearance_overrides = QCborArray{brokenOverride.map()};

        error("ReferenceMissing", [&] {
            store.execute(command("SetModelSupports", fields({{"model", brokenModel.map()}})));
        });

        // 8. Broken SectionView reference behavior
        features::SectionView brokenSection;
        brokenSection.id = newId();
        brokenSection.name = "Broken View";
        brokenSection.active = true;
        brokenSection.support = missingFaceRef;

        auto brokenSectionModel = parsedAfterSectionRepair;
        brokenSectionModel.section_views = QCborArray{brokenSection.map()};

        error("ReferenceMissing", [&] {
            store.execute(
                command("SetModelSupports", fields({{"model", brokenSectionModel.map()}})));
        });

        puts("PASS: cm9 repair");
    });
}
