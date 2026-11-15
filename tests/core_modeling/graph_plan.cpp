#include "evaluation/Plan.hpp"
#include "support/check.hpp"
#include <QCoreApplication>
using namespace cad;
template <class F> void errorCode(QString expected, F &&action) {
    QString code;
    try {
        action();
    } catch (const domain::Error &e) {
        code = e.code;
    }
    CHECK(code == expected);
}
struct Inputs {
    sketch::Sketch source, target;
    features::Feature feature;
    QString line;
    Inputs() {
        line = source.line(source.point({.01, -.01}), source.point({.01, .01}));
        target.rectangle({.01, 0}, {.03, .02});
        sketch::Constraint c;
        c.kind = "Pierce";
        c.a = target.entities[0].id;
        c.reference = sketch::CurveReference{"Self", source.id, line, "Line", 0, 1, .5};
        target.constraints.push_back(c);
        feature.parameters = sketch::fields({{"sketch", target.id},
                                             {"operation", "NewBody"},
                                             {"target", ""},
                                             {"depth", .01},
                                             {"extent", "Forward"}});
    }
};
void missingSketchSource() {
    Inputs i;
    errorCode("ReferenceMissing", [&] {
        evaluation::Plan invalid(sketch::encode({i.target}), features::encode({i.feature}));
    });
}
void sourceInvalidation() {
    Inputs i;
    sketch::Sketch unrelated;
    unrelated.rectangle({0, 0}, {.02, .02});
    auto other = i.feature;
    other.id = newId();
    other.parameters.insert(QStringLiteral("sketch"), unrelated.id);
    auto definitions = features::encode({i.feature, other});
    // Dependent-first collection order must stay legal, as in legacy Pierce.
    evaluation::Plan before(sketch::encode({i.target, unrelated, i.source}), definitions);
    CHECK(before.geometryGraph.dependencies(i.target.id) == QStringList({i.source.id}));
    CHECK(before.geometryGraph.descendants(i.source.id).contains(i.feature.id));
    auto sourceEndpoint = i.source.entity(i.line).a;
    i.source.entity(sourceEndpoint).point.x += .005;
    evaluation::Plan after(sketch::encode({i.target, unrelated, i.source}), definitions);
    CHECK(before.nodes[0].fingerprint != after.nodes[0].fingerprint);
    CHECK(before.nodes[1].fingerprint == after.nodes[1].fingerprint);
    CHECK(after.nodes[0].dependencies.empty()); // Existing feature-only output is retained.
    CHECK(after.map().size() == 2);
}
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    return run([] {
        missingSketchSource();
        sourceInvalidation();
    });
}
