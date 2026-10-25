#include "../module4/Fixture.hpp"
#include "evaluation/Plan.hpp"
#include "geometry/occt/Part.hpp"
#include "support/check.hpp"
#include <BRepAdaptor_Surface.hxx>
#include <BRepAlgoAPI_Fuse.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <QCoreApplication>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
using namespace cad;
using sketch::fields;
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    return run([] {
        fixture4::B1 b;
        kernel::ResultCache cache;
        auto definitions = b.features();
        auto first = kernel::Part::build(b.sketches(), definitions, &cache);
        auto count = cache.computations;
        auto same = kernel::Part::build(b.sketches(), definitions, &cache);
        CHECK(cache.computations == count);
        near(first.measure(1).value("volume_m3").toDouble(),
             same.measure(1).value("volume_m3").toDouble(), 1e-14);
        auto other = b.base;
        other.id = newId();
        definitions.append(other.map());
        kernel::Part::build(b.sketches(), definitions, &cache);
        auto edited = features::parse(definitions);
        edited[2].parameters.insert(QStringLiteral("diameter"), .005);
        auto start = cache.computations;
        auto rebuilt = kernel::Part::build(b.sketches(), features::encode(edited), &cache);
        CHECK(cache.computations - start == 4);
        CHECK(rebuilt.diagnostics.last().toMap().value("reused").toBool());
        CHECK(first.measure(1) == same.measure(1));
        auto suppressed = evaluation::change(b.sketches(), definitions, "Suppress", b.base.id,
                                             fields({{"suppressed", true}}));
        auto failed = kernel::Part::build(b.sketches(), suppressed, &cache, true);
        CHECK(failed.stale);
        CHECK(failed.bodies.size() == 1);
        CHECK(!failed.errors.empty());
        rejects([&] { failed.measure(1); });
        // Move a selected vertical edge with a parent dimension. Geometry candidates require
        // explicit repair.
        auto old = kernel::Part::build(b.sketches(), features::encode({b.base}));
        QString selected;
        for (auto value : old.topology()) {
            auto m = value.toMap();
            auto c = m.value("center_si").toArray();
            if (m.value("token").toString().startsWith("edge/") &&
                std::abs(c[0].toDouble() - .08) < 1e-10 && std::abs(c[1].toDouble()) < 1e-10 &&
                std::abs(c[2].toDouble() - .005) < 1e-10)
                selected = m.value("token").toString();
        }
        CHECK(!selected.isEmpty());
        auto changed = b;
        for (auto &e : changed.outer.entities)
            if (e.kind == "Point" && std::abs(e.point.x - .08) < 1e-10)
                e.point.x = .09;
        auto moved = kernel::Part::build(changed.sketches(), features::encode({b.base}));
        auto resolution = moved.reference(b.base.id, selected, old);
        CHECK(resolution.value("state").toString() == "Candidate");
        CHECK(resolution.value("candidates").toArray().size() == 1);
        // Exact Boolean split of a top face into two regions, retaining one solid.
        sketch::Sketch groove;
        groove.frame.origin = {0, 0, .005};
        groove.rectangle({.03, -.01}, {.05, .06});
        auto cut = b.cutFeature;
        cut.parameters.insert(QStringLiteral("sketch"), groove.id);
        auto split =
            kernel::Part::build(sketch::encode({b.outer, groove}), features::encode({b.base, cut}));
        QString top;
        for (auto value : old.topology()) {
            auto m = value.toMap();
            auto c = m.value("center_si").toArray();
            if (m.value("token").toString().startsWith("face/") &&
                std::abs(c[2].toDouble() - .01) < 1e-10)
                top = m.value("token").toString();
        }
        QStringList tokens;
        for (auto t : split.topology())
            tokens.append(t.toMap().value("token").toString());
        auto evolution = kernel::evolution(top, split.history, tokens);
        CHECK(evolution.value("state").toString() == "Split");
        CHECK(evolution.value("candidates").toArray().size() == 2);
        // Symmetric same-type equal-length candidates are kept ambiguous, never nearest-index
        // selected.
        auto symmetric = b;
        for (auto &e : symmetric.outer.entities)
            if (e.kind == "Point") {
                if (e.point.x == .08)
                    e.point.x = .005;
                else if (e.point.x == 0)
                    e.point.x = -.005;
            }
        auto sym = kernel::Part::build(symmetric.sketches(), features::encode({b.base}));
        QString corner;
        for (auto t : old.topology()) {
            auto m = t.toMap();
            auto c = m.value("center_si").toArray();
            if (m.value("token").toString().startsWith("edge/") &&
                std::abs(c[0].toDouble()) < 1e-10 && std::abs(c[1].toDouble()) < 1e-10 &&
                std::abs(c[2].toDouble() - .005) < 1e-10)
                corner = m.value("token").toString();
        }
        CHECK(sym.reference(b.base.id, corner, old).value("state").toString() == "Ambiguous");

        sketch::Sketch cornerCut;
        cornerCut.rectangle({-.001, -.001}, {.005, .005});
        auto remove = b.cutFeature;
        remove.parameters.insert(QStringLiteral("sketch"), cornerCut.id);
        auto deleted = kernel::Part::build(sketch::encode({b.outer, cornerCut}),
                                           features::encode({b.base, remove}));
        QStringList survivors;
        for (auto t : deleted.topology())
            survivors.append(t.toMap().value("token").toString());
        CHECK(kernel::evolution(corner, deleted.history, survivors).value("state").toString() ==
              "Deleted");
        auto left = BRepPrimAPI_MakeBox(10, 10, 10).Shape(),
             right = BRepPrimAPI_MakeBox(gp_Pnt(10, 0, 0), 10, 10, 10).Shape();
        BRepAlgoAPI_Fuse fuse(left, right);
        CHECK(fuse.IsDone());
        fuse.SimplifyResult(true, true);
        auto mappings = kernel::trace(fuse, left);
        for (auto row : kernel::trace(fuse, right))
            mappings.append(row);
        QString oldTop;
        for (TopExp_Explorer i(left, TopAbs_FACE); i.More(); i.Next()) {
            auto face = TopoDS::Face(i.Current());
            BRepAdaptor_Surface surface(face);
            if (surface.GetType() == GeomAbs_Plane && surface.Plane().Location().Z() == 10)
                oldTop = kernel::topologyToken(face);
        }
        QStringList merged;
        for (TopExp_Explorer i(fuse.Shape(), TopAbs_FACE); i.More(); i.Next())
            merged.append(kernel::topologyToken(i.Current()));
        CHECK(!oldTop.isEmpty());
        CHECK(kernel::evolution(oldTop, mappings, merged).value("state").toString() == "Merged");
        features::Feature pattern;
        pattern.kind = "LinearPattern";
        pattern.parameters = fields({{"source", b.base.id},
                                     {"target", ""},
                                     {"operation", "NewBody"},
                                     {"direction", QCborArray{1., 0., 0.}},
                                     {"count", 3},
                                     {"spacing", .1}});
        auto drilled = b.hole;
        drilled.parameters.insert(
            QStringLiteral("target"),
            QUuid::createUuidV5(QUuid(pattern.id), QByteArray("1")).toString(QUuid::WithoutBraces));
        drilled.parameters.insert(QStringLiteral("origin"), QCborArray{.21, .01, .01});
        auto full = kernel::Part::build(b.sketches(), features::encode({b.base, pattern, drilled}));
        CHECK(full.bodies.size() == 3);
        pattern.parameters.insert(QStringLiteral("count"), 2);
        auto missing = kernel::Part::build(
            b.sketches(), features::encode({b.base, pattern, drilled}), nullptr, true);
        CHECK(missing.stale);
        CHECK(missing.errors.last().toMap().value("code").toString() == "ReferenceMissing");
        cache.trim();
        CHECK(cache.statistics().value("entries").toInteger() == 0);
        CHECK(cache.statistics().value("estimated_bytes").toInteger() == 0);
    });
}
