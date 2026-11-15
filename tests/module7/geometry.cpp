#include "assembly/Geometry.hpp"
#include "support/check.hpp"
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <QCoreApplication>
#include <QDir>
#include <gp_Pln.hxx>
using namespace cad;
using namespace cad::assembly;
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    return run([] {
        auto definition = newId(), body = newId();
        Definition d;
        d.kind = "Part";
        d.name = "Analytic box";
        d.revision = 1;
        d.part.bodies.push_back({body, BRepPrimAPI_MakeBox(10, 20, 30).Shape()});
        Loader load = [&](QString id, bool) {
            domain::check(id == definition, "ReferenceMissing", "Unknown definition");
            return d;
        };
        Model model;
        Occurrence a, b;
        a.definition = b.definition = definition;
        a.grounded = true;
        b.pose.translation = {.005, 0, 0};
        model.occurrences = {a, b};
        auto result = interference(model, load);
        CHECK(result.size() == 1);
        CHECK(result[0].toMap().value("status") == "Overlap");
        near(result[0].toMap().value("volume_m3").toDouble(), 3000e-9, 1e-12);
        model.occurrences[1].pose.translation = {.01, 0, 0};
        CHECK(interference(model, load)[0].toMap().value("status") == "Touching");
        model.occurrences[1].pose.translation = {.02, 0, 0};
        CHECK(interference(model, load)[0].toMap().value("status") == "Disjoint");
        model.occurrences[1].pose.rotation = {std::sqrt(.5), 0, 0, std::sqrt(.5)};
        QMap<QString, DisplayMesh> cache;
        auto display = scene(model, load, cache).display;
        auto packed = pack(display);
        auto round = unpack(packed.manifest, packed.bytes);
        CHECK(round->instances.size() == 2);
        CHECK(round->instances[0].triangleCount == 12);
        CHECK(round->vertices.size() == 36);
        near(round->instances[1].map({10, 0, 0}).y(), 10, 1e-5);
        auto bad = packed.manifest;
        auto entries = bad.value("instances").toArray();
        auto item = entries[0].toMap();
        item.insert(QStringLiteral("triangle_count"), 1000000);
        entries[0] = item;
        bad.insert(QStringLiteral("instances"), entries);
        rejects([&] { unpack(bad, packed.bytes); });
        auto refs = references(a.id, definition, load);
        CHECK(refs.size() > 7);
        bool plane = false;
        for (auto v : refs) {
            auto r = Reference::parse(v.toMap().value("reference").toMap());
            if (!r.body.isEmpty() && r.token.startsWith("face/")) {
                CHECK(reference(r, model, load).kind == "Plane");
                plane = true;
                break;
            }
        }
        CHECK(plane);
        auto unknown = model;
        unknown.occurrences[1].definition = newId();
        auto missing = scene(unknown, load, cache);
        CHECK(missing.stale);
        CHECK(missing.states[1].toMap().value("status") == "Unresolved");
        unknown.occurrences[1].suppressed = true;
        CHECK(!scene(unknown, load, cache).stale);
        auto unloaded = model;
        for (auto &o : unloaded.occurrences)
            o.unloaded = true;
        QMap<QString, DisplayMesh> cold;
        CHECK(scene(unloaded, load, cold).display.instances.empty());
        CHECK(cold.empty());
        CHECK(scene(model, load, cold).display.instances.size() == 2);
        CHECK(cold.size() == 1);
        auto solid = d.part.bodies[0].shape;
        d.part.bodies[0].shape =
            BRepBuilderAPI_MakeFace(gp_Pln(gp_Pnt(0, 0, 0), gp_Dir(0, 0, 1)), 0, 10, 0, 20).Shape();
        rejects([&] { interference(model, load); });
        d.part.bodies[0].shape = solid;
        d.stale = true;
        rejects([&] { interference(model, load); });
        d.stale = false;
        d.part.bodies.clear();
        rejects([&] { interference(model, load); });
        std::cout << "Exact overlap 3000 mm3, touching/disjoint/stale/unsupported distinctions; "
                     "shared rigid display ranges, rotated placement, bounded round trip and exact "
                     "planar references passed\n";
    });
}
