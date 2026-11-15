#include "SupportFixture.hpp"
#include "geometry/occt/SupportReferences.hpp"
#include <BRepAdaptor_Surface.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <QCoreApplication>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
using namespace cad;
using namespace support_fixture;
using sketch::fields;
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    return run([] {
        auto box = BRepPrimAPI_MakeBox(40, 30, 10).Shape();
        auto source = ref(newId(), "Face");
        for (TopExp_Explorer it(box, TopAbs_FACE); it.More(); it.Next()) {
            BRepAdaptor_Surface surface(TopoDS::Face(it.Current()));
            if (surface.GetType() == GeomAbs_Plane &&
                std::abs(surface.Plane().Location().Z() - 10) < 1e-8)
                source.topology = geometry::supportToken(it.Current());
        }
        CHECK(!source.topology.isEmpty());
        auto hole = BRepPrimAPI_MakeBox(gp_Pnt(5, 5, -1), 5, 5, 12).Shape();
        BRepAlgoAPI_Cut cut(box, hole);
        CHECK(cut.IsDone());
        geometry::SupportReferences scene;
        scene.stages.insert(source.object, cut.Shape());
        scene.history = kernel::trace(cut, box);
        CHECK(scene.inspect(source).value("state") == "History");
        auto resolved = scene.topology(source);
        CHECK(geometry::supportToken(resolved) != source.topology);
        near(scene.plane(source).origin[2], .01);
        CHECK(scene.evolution.size() == 1);
        auto successor = geometry::supportToken(resolved);
        // A two-operation one-to-one lineage must resolve transitively.
        auto middle = "face/" + QString(64, 'b');
        scene.history =
            QCborArray{fields({{"input", source.topology}, {"modified", QCborArray{middle}}}),
                       fields({{"input", middle}, {"modified", QCborArray{successor}}})};
        CHECK(scene.inspect(source).value("state") == "History");
        CHECK(scene.topology(source).IsSame(resolved));
        // A merge is ambiguous even with just one surviving candidate.
        scene.history.append(
            fields({{"input", "face/" + QString(64, 'c')}, {"modified", QCborArray{successor}}}));
        error("ReferenceAmbiguous", [&] { scene.topology(source); });
        auto slit = BRepPrimAPI_MakeBox(gp_Pnt(19, -1, -1), 2, 32, 12).Shape();
        BRepAlgoAPI_Cut split(box, slit);
        CHECK(split.IsDone());
        scene.stages[source.object] = split.Shape();
        scene.history = kernel::trace(split, box);
        CHECK(scene.inspect(source).value("state") == "Split");
        error("ReferenceAmbiguous", [&] { scene.topology(source); });
        auto missing = source;
        missing.topology = "face/" + QString(64, 'd');
        error("ReferenceMissing", [&] { scene.topology(missing); });
        // A reference to an earlier producing stage is not split just because
        // a later operation split that face in the final body.
        scene.stages[source.object] = box;
        CHECK(scene.inspect(source).value("state") == "Direct");
        near(scene.plane(source).origin[2], .01);
        // Only the producing feature's prefix can evolve an older token.
        // A subsequent split must not turn that prefix's unique history into
        // an ambiguity, even if the original token itself is no longer present.
        auto later = newId();
        scene.stageOrder.insert(source.object, 0);
        scene.stageOrder.insert(later, 1);
        scene.stages[source.object] = cut.Shape();
        scene.history = {};
        for (auto value : kernel::trace(cut, box)) {
            auto record = value.toMap();
            record.insert(QStringLiteral("feature"), source.object);
            scene.history.append(record);
        }
        scene.history.append(fields(
            {{"feature", later},
             {"input", successor},
             {"modified", QCborArray{"face/" + QString(64, 'e'), "face/" + QString(64, 'f')}}}));
        CHECK(scene.inspect(source).value("state") == "History");
        near(scene.plane(source).origin[2], .01);
    });
}
