#include "SupportFixture.hpp"
#include "geometry/occt/Supports.hpp"
#include <BRepAdaptor_Surface.hxx>
#include <QCoreApplication>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
using namespace cad;
using namespace support_fixture;
struct Fixture {
    model::SupportDefinition definition;
    sketch::Sketch base, target;
    features::Feature first, second;
    model::Datum plane;
    Fixture() {
        base.rectangle({0, 0}, {.04, .03});
        target.rectangle({.005, .005}, {.015, .015});
        first.parameters = sketch::fields({{"sketch", base.id},
                                           {"operation", "NewBody"},
                                           {"target", ""},
                                           {"depth", .01},
                                           {"extent", "Forward"}});
        second.parameters = first.parameters;
        second.parameters.insert(QStringLiteral("sketch"), target.id);
        second.parameters.insert(QStringLiteral("depth"), .005);
        auto initial = kernel::Part::build(sketch::encode({base}), features::encode({first}));
        auto source = ref(first.id, "Face");
        for (TopExp_Explorer it(initial.stages.value(first.id), TopAbs_FACE); it.More();
             it.Next()) {
            BRepAdaptor_Surface surface(TopoDS::Face(it.Current()));
            if (surface.GetType() == GeomAbs_Plane &&
                std::abs(surface.Plane().Location().Z() - 10) < 1e-8)
                source.topology = geometry::supportToken(it.Current());
        }
        CHECK(!source.topology.isEmpty());
        plane.references = {source};
        plane.parameters = sketch::fields({{"distance", .03}, {"count", 1}});
        model::Attachment attachment;
        attachment.sketch = target.id;
        attachment.reference = ref(plane.id, "Plane");
        definition.sketches = sketch::encode({target, base});
        definition.features = features::encode({first, second});
        definition.datums = {plane};
        definition.attachments = {attachment};
        definition.history = {base.id, first.id, plane.id, target.id, second.id};
    }
};
void mixedHistory() {
    Fixture f;
    auto result = geometry::evaluateSupports(f.definition);
    CHECK(result.sketches[0].id == f.target.id);
    near(result.sketches[0].frame.origin[2], .04);
    near(result.part.measure(0).value("volume_m3").toDouble(), .0000125, 1e-12);
    auto broken = f.definition;
    broken.attachments[0].reference = ref(f.second.id, "Face");
    broken.attachments[0].reference.topology = f.plane.references[0].topology;
    error("DependencyCycle", [&] { geometry::evaluateSupports(broken); });
    broken = f.definition;
    broken.datums[0].references[0].topology = "face/" + QString(64, 'a');
    error("ReferenceMissing", [&] { geometry::evaluateSupports(broken); });
    exchange::Control canceled;
    canceled.canceled = [] { return true; };
    error("Canceled", [&] { geometry::evaluateSupports(f.definition, {}, canceled); });
    auto faceAttached = f.definition;
    faceAttached.datums.clear();
    faceAttached.attachments[0].reference = f.plane.references[0];
    faceAttached.attachments[0].offset = .003;
    faceAttached.attachments[0].angle = .25;
    faceAttached.attachments[0].reversed = true;
    faceAttached.history = {f.base.id, f.first.id, f.target.id, f.second.id};
    auto direct = geometry::evaluateSupports(faceAttached);
    near(direct.sketches[0].frame.origin[2], .013);
    vector(direct.sketches[0].frame.u, {std::cos(.25), std::sin(.25), 0});
    vector(geometry::coordinates(geometry::vector(direct.sketches[0].frame.u)
                                     .Crossed(geometry::vector(direct.sketches[0].frame.v))),
           {0, 0, -1});
}
void convertedFeature() {
    Fixture f;
    f.target.entities.clear();
    f.definition.sketches = sketch::encode({f.target, f.base});
    model::Conversion conversion;
    conversion.sketch = f.target.id;
    conversion.sources = {f.plane.references[0]};
    conversion.scale = .75;
    conversion.angle = .3;
    f.definition.conversions = {conversion};
    f.definition.history = {f.base.id,     f.first.id,  f.plane.id,
                            conversion.id, f.target.id, f.second.id};
    auto result = geometry::evaluateSupports(f.definition);
    CHECK(result.conversions.size() == 1);
    CHECK(!result.sketches[0].entities.empty());
    near(result.part.measure(0).value("volume_m3").toDouble(),
         .000012 + .04 * .03 * .75 * .75 * .005, 1e-12);
    CHECK(result.sketches[0].constraints.size() == 8);
    auto again = geometry::evaluateSupports(f.definition);
    CHECK(again.sketches[0].map() == result.sketches[0].map());
    auto shifted = f.definition;
    shifted.datums[0].parameters.insert(QStringLiteral("distance"), .06);
    auto changed = geometry::evaluateSupports(shifted);
    near(changed.sketches[0].frame.origin[2], .07);
    CHECK(changed.sketches[0].entities[0].id == result.sketches[0].entities[0].id);
    auto broken = f.definition;
    broken.history = {f.base.id, f.first.id, conversion.id, f.plane.id, f.target.id, f.second.id};
    error("DependencyOrder", [&] { geometry::evaluateSupports(broken); });
    bool cancel = false;
    exchange::Control control;
    control.canceled = [&] { return cancel; };
    control.progress = [&](QString, int percent) {
        if (percent >= 50)
            cancel = true;
    };
    error("Canceled", [&] { geometry::evaluateSupports(f.definition, {}, control); });
}
void generatedExternalReference() {
    Fixture f;
    f.target.entities.clear();
    f.definition.sketches = sketch::encode({f.target, f.base});
    model::Conversion first;
    first.sketch = f.target.id;
    first.sources = {f.plane.references[0]};
    first.scale = .75;
    f.definition.conversions = {first};
    f.definition.history = {f.base.id, f.first.id, f.plane.id, first.id, f.target.id, f.second.id};
    auto preview = geometry::evaluateSupports(f.definition);
    QString bottom;
    for (auto &e : preview.sketches[0].entities)
        if (e.kind == "Line") {
            auto a = preview.sketches[0].entity(e.a).point,
                 b = preview.sketches[0].entity(e.b).point;
            if (std::abs(a.y) < 1e-10 && std::abs(b.y) < 1e-10 &&
                std::abs(std::abs(a.x - b.x) - .03) < 1e-10)
                bottom = e.id;
        }
    CHECK(!bottom.isEmpty());
    f.definition.conversions[0].members.insert(bottom, "Curve");
    sketch::Sketch external;
    external.frame.origin[2] = .07;
    f.definition.sketches.append(external.map());
    model::Conversion second;
    second.sketch = external.id;
    second.sources = {ref(f.target.id, "Curve", bottom)};
    f.definition.conversions.push_back(second);
    f.definition.history.append(second.id);
    f.definition.history.append(external.id);
    auto copied = geometry::evaluateSupports(f.definition);
    auto &s = copied.sketches[2];
    CHECK(s.id == external.id);
    near(s.frame.origin[2], .07);
    auto line = s.entities.back();
    near(sketch::length(s.entity(line.a).point, s.entity(line.b).point), .03);
    auto forged = f.definition;
    forged.conversions[0].members.insert(newId(), "Curve");
    error("ReferenceMissing", [&] { geometry::evaluateSupports(forged); });
}
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    return run([] {
        mixedHistory();
        convertedFeature();
        generatedExternalReference();
    });
}
