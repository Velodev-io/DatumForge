#include "geometry/occt/Part.hpp"
#include "support/check.hpp"
#include <QCoreApplication>
#include <numbers>
using namespace cad;
using sketch::fields;
features::Feature extrude(QString sketch, QString operation = "NewBody", QString target = {},
                          double depth = .01, QString extent = "Forward") {
    features::Feature f;
    f.name = "Extrusion";
    f.kind = "Extrude";
    f.parameters = fields({{"sketch", sketch},
                           {"operation", operation},
                           {"target", target},
                           {"depth", depth},
                           {"extent", extent}});
    return f;
}
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    return run([] {
        sketch::Sketch rectangle;
        rectangle.rectangle({0, 0}, {.08, .05});
        auto sketches = sketch::encode({rectangle});
        auto base = extrude(rectangle.id);
        auto a = features::encode({base});
        auto part = kernel::Part::build(sketches, a);
        near(part.measure(1).value("volume_m3").toDouble(), .08 * .05 * .01, 1e-12);
        CHECK(part.bodies.size() == 1);
        CHECK(!part.history.empty());
        for (auto extent : {"Reverse", "Symmetric"}) {
            auto f = base;
            f.parameters.insert(QStringLiteral("extent"), extent);
            auto r = kernel::Part::build(sketches, features::encode({f})).measure(1);
            near(r.value("minimum_m").toArray()[2].toDouble(),
                 QString(extent) == "Reverse" ? -.01 : -.005, 1e-10);
        }
        auto invalid = base;
        invalid.parameters.insert(QStringLiteral("depth"), -1);
        rejects([&] { kernel::Part::build(sketches, features::encode({invalid})); });
        sketch::Sketch open;
        open.polyline({{0, 0}, {.01, 0}, {.01, .01}}, false);
        rejects([&] {
            kernel::Part::build(sketch::encode({open}), features::encode({extrude(open.id)}));
        });
        sketch::Sketch bow;
        bow.polyline({{0, 0}, {.01, .01}, {.01, 0}, {0, .01}}, true);
        rejects([&] {
            kernel::Part::build(sketch::encode({bow}), features::encode({extrude(bow.id)}));
        });
        sketch::Sketch circle;
        circle.circle(circle.point({0, 0}), .005);
        auto cyl =
            kernel::Part::build(sketch::encode({circle}), features::encode({extrude(circle.id)}));
        near(cyl.measure(1).value("volume_m3").toDouble(), std::numbers::pi * .005 * .005 * .01,
             1e-12);
        sketch::Sketch annulus;
        annulus.circle(annulus.point({0, 0}), .01);
        annulus.circle(annulus.point({0, 0}), .005);
        auto ring =
            kernel::Part::build(sketch::encode({annulus}), features::encode({extrude(annulus.id)}));
        near(ring.measure(1).value("volume_m3").toDouble(),
             std::numbers::pi * (.01 * .01 - .005 * .005) * .01, 1e-12);
        sketch::Sketch revolved;
        revolved.rectangle({.01, 0}, {.02, .01});
        features::Feature rev;
        rev.name = "Revolve";
        rev.kind = "Revolve";
        rev.parameters = fields({{"sketch", revolved.id},
                                 {"operation", "NewBody"},
                                 {"target", ""},
                                 {"angle", 2 * std::numbers::pi},
                                 {"direction", "Forward"},
                                 {"axis_origin", QCborArray{0., 0., 0.}},
                                 {"axis_direction", QCborArray{0., 1., 0.}}});
        auto turned = kernel::Part::build(sketch::encode({revolved}), features::encode({rev}));
        near(turned.measure(1).value("volume_m3").toDouble(),
             std::numbers::pi * (.02 * .02 - .01 * .01) * .01, 1e-12);
        rev.parameters.insert(QStringLiteral("angle"), std::numbers::pi);
        rev.parameters.insert(QStringLiteral("direction"), "Reverse");
        near(kernel::Part::build(sketch::encode({revolved}), features::encode({rev}))
                 .measure(1)
                 .value("volume_m3")
                 .toDouble(),
             std::numbers::pi * (.02 * .02 - .01 * .01) * .005, 1e-12);
        rev.parameters.insert(QStringLiteral("axis_direction"), QCborArray{0., 0., 1.});
        rejects([&] { kernel::Part::build(sketch::encode({revolved}), features::encode({rev})); });
        sketch::Sketch cut;
        cut.rectangle({.02, .02}, {.04, .04});
        sketches = sketch::encode({rectangle, cut});
        auto cutFeature = extrude(cut.id, "Cut", base.id);
        a = features::encode({base, cutFeature});
        part = kernel::Part::build(sketches, a);
        near(part.measure(2).value("volume_m3").toDouble(), .000036, 1e-12);
        auto before = part.measure(2);
        auto bad = cutFeature;
        bad.parameters.insert(QStringLiteral("depth"), INFINITY);
        rejects([&] { kernel::Part::build(sketches, features::encode({base, bad})); });
        CHECK(part.measure(2) == before);
        // B1: one cylindrical tool patterned in X, then pattern both holes in Y.
        features::Feature hole;
        hole.kind = "Hole";
        hole.parameters = fields({{"target", base.id},
                                  {"origin", QCborArray{.01, .01, .01}},
                                  {"direction", QCborArray{0., 0., -1.}},
                                  {"diameter", .004},
                                  {"depth", .01},
                                  {"through", true}});
        features::Feature x;
        x.kind = "LinearPattern";
        x.parameters = fields({{"source", hole.id},
                               {"target", base.id},
                               {"operation", "Cut"},
                               {"direction", QCborArray{1., 0., 0.}},
                               {"spacing", .05},
                               {"count", 2}});
        features::Feature y = x;
        y.id = newId();
        y.parameters.insert(QStringLiteral("source"), hole.id);
        y.parameters.insert(QStringLiteral("direction"), QCborArray{0., 1., 0.});
        y.parameters.insert(QStringLiteral("spacing"), .03);
        features::Feature xy = y;
        xy.id = newId();
        xy.parameters.insert(QStringLiteral("source"), x.id);
        std::vector<features::Feature> history{base, cutFeature, hole, x, y, xy};
        part = kernel::Part::build(sketches, features::encode(history));
        near(part.measure(3).value("volume_m3").toDouble(), (36000 - 160 * std::numbers::pi) * 1e-9,
             1e-12);
        QString edge;
        for (auto t : part.topology()) {
            auto m = t.toMap();
            auto c = m.value("center_si").toArray();
            if (m.value("token").toString().startsWith("edge/") &&
                std::abs(c[0].toDouble()) < 1e-10 && std::abs(c[1].toDouble()) < 1e-10 &&
                std::abs(c[2].toDouble() - .005) < 1e-10)
                edge = m.value("token").toString();
        }
        CHECK(!edge.isEmpty());
        features::Feature fillet;
        fillet.kind = "Fillet";
        fillet.parameters = fields(
            {{"target", base.id}, {"source", xy.id}, {"edges", QCborArray{edge}}, {"size", .002}});
        history.push_back(fillet);
        part = kernel::Part::build(sketches, features::encode(history));
        near(part.measure(4).value("volume_m3").toDouble(), 35488.76110196153e-9, 1e-12);
        auto exact = part.measure(4);
        auto coarse = part.mesh(1), fine = part.mesh(.01);
        CHECK(fine.vertices.size() >= coarse.vertices.size());
        CHECK(part.measure(4) == exact);
        near(part.measure(4, {}, {}, 7800).value("mass_kg").toDouble(), 35488.76110196153e-9 * 7800,
             1e-10);
        auto fail = history;
        fail.back().parameters.insert(QStringLiteral("size"), 1.);
        rejects([&] { kernel::Part::build(sketches, features::encode(fail)); });
        // Chamfer and new-body mirror/pattern have exact Boolean/solid semantics.
        auto plain = kernel::Part::build(sketches, features::encode({base}));
        auto e = plain.topology()[0].toMap().value("token").toString();
        for (auto t : plain.topology())
            if (t.toMap().value("token").toString().startsWith("edge/")) {
                e = t.toMap().value("token").toString();
                break;
            }
        auto chamfer = fillet;
        chamfer.id = newId();
        chamfer.kind = "Chamfer";
        chamfer.parameters.insert(QStringLiteral("source"), base.id);
        chamfer.parameters.insert(QStringLiteral("edges"), QCborArray{e});
        chamfer.parameters.insert(QStringLiteral("size"), .001);
        CHECK(kernel::Part::build(sketches, features::encode({base, chamfer}))
                  .measure(1)
                  .value("volume_m3")
                  .toDouble() < .00004);
        features::Feature mirror;
        mirror.kind = "Mirror";
        mirror.parameters = fields({{"source", base.id},
                                    {"target", ""},
                                    {"operation", "NewBody"},
                                    {"origin", QCborArray{.1, 0., 0.}},
                                    {"normal", QCborArray{1., 0., 0.}}});
        auto mirrored = kernel::Part::build(sketches, features::encode({base, mirror}));
        CHECK(mirrored.bodies.size() == 2);
        near(mirrored.measure(1).value("volume_m3").toDouble(), .00008, 1e-12);
        // Transformed new bodies retain selectable exact edges without flattening body identity.
        auto mirrorEdges = mirrored.topology(mirror.id);
        QString mirrorEdge;
        for (auto value : mirrorEdges) {
            auto m = value.toMap();
            if (m.value("token").toString().startsWith("edge/")) {
                mirrorEdge = m.value("token").toString();
                break;
            }
        }
        CHECK(!mirrorEdge.isEmpty());
        auto mirrorFillet = fillet;
        mirrorFillet.id = newId();
        mirrorFillet.parameters.insert(QStringLiteral("target"), mirror.id);
        mirrorFillet.parameters.insert(QStringLiteral("source"), mirror.id);
        mirrorFillet.parameters.insert(QStringLiteral("edges"), QCborArray{mirrorEdge});
        mirrorFillet.parameters.insert(QStringLiteral("size"), .0001);
        auto dressedMirror =
            kernel::Part::build(sketches, features::encode({base, mirror, mirrorFillet}));
        CHECK(dressedMirror.bodies.size() == 2);
        features::Feature circular;
        circular.kind = "CircularPattern";
        circular.parameters = fields({{"source", base.id},
                                      {"target", ""},
                                      {"operation", "NewBody"},
                                      {"axis_origin", QCborArray{0., 0., 0.}},
                                      {"axis_direction", QCborArray{0., 0., 1.}},
                                      {"count", 4},
                                      {"angle", 2 * std::numbers::pi},
                                      {"distribution", "Full"}});
        auto patterned = kernel::Part::build(sketches, features::encode({base, circular}));
        CHECK(patterned.bodies.size() == 4);
        near(patterned.measure(1).value("volume_m3").toDouble(), .00016, 1e-12);

        // Exact edge, angle, radius, area and bounds queries have independent values.
        QString horizontal, vertical, circularEdge;
        for (auto value : plain.topology()) {
            auto m = value.toMap();
            auto c = m.value("center_si").toArray();
            auto k = m.value("token").toString();
            if (!k.startsWith("edge/"))
                continue;
            if (std::abs(c[0].toDouble() - .04) < 1e-10 && std::abs(c[1].toDouble()) < 1e-10 &&
                std::abs(c[2].toDouble()) < 1e-10)
                horizontal = k;
            if (std::abs(c[0].toDouble()) < 1e-10 && std::abs(c[1].toDouble() - .025) < 1e-10 &&
                std::abs(c[2].toDouble()) < 1e-10)
                vertical = k;
        }
        CHECK(!horizontal.isEmpty() && !vertical.isEmpty());
        auto measured = plain.measure(7, base.id, horizontal, 0, vertical);
        near(measured.value("length_m").toDouble(), .08, 1e-12);
        near(measured.value("angle_rad").toDouble(), std::numbers::pi / 2, 1e-12);
        near(plain.measure(7).value("area_m2").toDouble(), 2 * (.08 * .05 + .08 * .01 + .05 * .01),
             1e-12);
        for (auto value : cyl.topology()) {
            auto k = value.toMap().value("token").toString();
            if (!k.startsWith("edge/"))
                continue;
            auto r = cyl.measure(3, {}, k);
            if (r.contains(QStringLiteral("radius_m"))) {
                near(r.value("radius_m").toDouble(), .005, 1e-12);
                circularEdge = k;
            }
        }
        CHECK(!circularEdge.isEmpty());
        auto noDensity = plain.measure(7);
        CHECK(!noDensity.contains(QStringLiteral("mass_kg")));
        rejects([&] { plain.measure(7, {}, {}, -1); });
        auto blind = hole;
        blind.id = newId();
        blind.parameters.insert(QStringLiteral("through"), false);
        blind.parameters.insert(QStringLiteral("depth"), .005);
        near(kernel::Part::build(sketches, features::encode({base, blind}))
                 .measure(1)
                 .value("volume_m3")
                 .toDouble(),
             .00004 - std::numbers::pi * .002 * .002 * .005, 1e-12);
        auto missed = hole;
        missed.parameters.insert(QStringLiteral("origin"), QCborArray{1., 1., .01});
        rejects([&] { kernel::Part::build(sketches, features::encode({base, missed})); });
        sketch::Sketch upper;
        upper.frame.origin = {0, 0, .01};
        upper.rectangle({0, 0}, {.08, .05});
        auto add = extrude(upper.id, "Add", base.id);
        near(kernel::Part::build(sketch::encode({rectangle, upper}), features::encode({base, add}))
                 .measure(1)
                 .value("volume_m3")
                 .toDouble(),
             .00008, 1e-12);
        upper.frame.origin = {0, 0, 1};
        rejects([&] {
            kernel::Part::build(sketch::encode({rectangle, upper}), features::encode({base, add}));
        });
        // Revolve Add/Cut around Y, independent from the extrusion direction adapter.
        sketch::Sketch disk;
        disk.frame.u = {1, 0, 0};
        disk.frame.v = {0, 0, 1};
        disk.circle(disk.point({0, 0}), .01);
        auto cylinder = extrude(disk.id, "NewBody", {}, .01, "Reverse");
        auto addRev = rev;
        addRev.id = newId();
        addRev.parameters = fields({{"sketch", revolved.id},
                                    {"operation", "Add"},
                                    {"target", cylinder.id},
                                    {"angle", 2 * std::numbers::pi},
                                    {"direction", "Forward"},
                                    {"axis_origin", QCborArray{0., 0., 0.}},
                                    {"axis_direction", QCborArray{0., 1., 0.}}});
        near(kernel::Part::build(sketch::encode({disk, revolved}),
                                 features::encode({cylinder, addRev}))
                 .measure(1)
                 .value("volume_m3")
                 .toDouble(),
             std::numbers::pi * .02 * .02 * .01, 1e-12);
        for (auto &e : disk.entities)
            if (e.kind == "Circle")
                e.radius = .02;
        addRev.parameters.insert(QStringLiteral("operation"), "Cut");
        near(kernel::Part::build(sketch::encode({disk, revolved}),
                                 features::encode({cylinder, addRev}))
                 .measure(1)
                 .value("volume_m3")
                 .toDouble(),
             std::numbers::pi * .01 * .01 * .01, 1e-12);
        sketch::Sketch semi;
        auto center = semi.point({0, 0});
        semi.arc(center, .01, 0, std::numbers::pi);
        auto left = semi.point({-.01, 0}), right = semi.point({.01, 0});
        semi.line(left, right);
        near(kernel::Part::build(sketch::encode({semi}), features::encode({extrude(semi.id)}))
                 .measure(1)
                 .value("volume_m3")
                 .toDouble(),
             std::numbers::pi * .01 * .01 * .005, 1e-12);
        sketch::Sketch overlap;
        overlap.rectangle({0, 0}, {.02, .02});
        overlap.rectangle({.01, .01}, {.03, .03});
        rejects([&] {
            kernel::Part::build(sketch::encode({overlap}), features::encode({extrude(overlap.id)}));
        });
        auto badPattern = circular;
        badPattern.parameters.insert(QStringLiteral("distribution"), "Spacing");
        badPattern.parameters.insert(QStringLiteral("angle"), std::numbers::pi);
        rejects([&] { kernel::Part::build(sketches, features::encode({base, badPattern})); });
        auto badMirror = mirror;
        badMirror.parameters.insert(QStringLiteral("normal"), QCborArray{0., 0., 0.});
        rejects([&] { kernel::Part::build(sketches, features::encode({base, badMirror})); });
        auto other = extrude(cut.id);
        auto withOther = kernel::Part::build(sketches, features::encode({base, other, circular}));
        QSet<QString> originalCopies, shiftedCopies;
        for (auto &b : patterned.bodies)
            if (b.id != base.id)
                originalCopies.insert(b.id);
        for (auto &b : withOther.bodies)
            if (b.id != base.id && b.id != other.id)
                shiftedCopies.insert(b.id);
        CHECK(originalCopies ==
              shiftedCopies); // IDs are feature-local, independent of unrelated body count.
        for (double scale : {.001, 1., 1000.}) {
            auto s = rectangle;
            for (auto &p : s.entities) {
                p.point.x *= scale;
                p.point.y *= scale;
            }
            auto f = extrude(s.id, "NewBody", {}, .01 * scale);
            near(kernel::Part::build(sketch::encode({s}), features::encode({f}))
                     .measure(1)
                     .value("volume_m3")
                     .toDouble(),
                 .00004 * scale * scale * scale,
                 std::max(1e-21, .00004 * scale * scale * scale * 1e-6));
        }
    });
}
