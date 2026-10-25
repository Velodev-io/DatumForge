#include "evaluation/Plan.hpp"
#include "../module4/Fixture.hpp"
#include "support/check.hpp"
#include <QCoreApplication>
using namespace cad;
using sketch::fields;
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    return run([] {
        fixture4::B1 b;
        auto a = b.features();
        evaluation::Plan first(b.sketches(), a);
        CHECK(first.nodes.size() == 6);
        CHECK(first.descendants[b.base.id].size() == 5);
        auto other = b.base;
        other.id = newId();
        other.name = "Independent";
        a.append(other.map());
        auto changed = features::parse(a);
        changed[2].parameters.insert(QStringLiteral("diameter"), .005);
        auto impact = evaluation::impact(b.sketches(), a, features::encode(changed));
        CHECK(impact.value("affected").toArray().size() == 4);
        CHECK(impact.value("unaffected").toArray().contains(other.id));
        auto renamed =
            evaluation::change(b.sketches(), a, "Rename", b.hole.id, fields({{"name", "Drill"}}));
        CHECK(evaluation::impact(b.sketches(), a, renamed).value("affected").toArray().empty());
        rejects([&] {
            evaluation::change(b.sketches(), a, "Reorder", b.hole.id, fields({{"index", 0}}));
        });
        rejects([&] {
            evaluation::change(b.sketches(), a, "Delete", b.base.id, fields({{"cascade", false}}));
        });
        CHECK(evaluation::change(b.sketches(), a, "Delete", b.base.id, fields({{"cascade", true}}))
                  .size() == 1);
        auto symbol = [](QString id, QString field) { return "F_" + id.remove('-') + "_" + field; };
        auto expr = features::parse(a);
        expr[2].expressions["diameter"] = symbol(b.base.id, "depth") + " / 2";
        evaluation::Plan evaluated(b.sketches(), features::encode(expr));
        near(evaluated.nodes[2].feature.parameters.value("diameter").toDouble(), .005, 1e-12);
        expr[0].expressions["depth"] = symbol(b.hole.id, "diameter");
        rejects([&] { evaluation::Plan invalid(b.sketches(), features::encode(expr)); });
        expr[0].expressions.clear();
        expr[2].expressions["diameter"] = "30 deg";
        rejects([&] { evaluation::Plan invalid(b.sketches(), features::encode(expr)); });
    });
}
