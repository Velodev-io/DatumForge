#include "evaluation/Graph.hpp"
#include "domain/GeometryReference.hpp"
#include "support/check.hpp"
#include <QCoreApplication>
#include <limits>
using namespace cad;
QString identity(int n) {
    return QString("00000000-0000-4000-8000-%1").arg(n, 12, 16, QChar('0'));
}
domain::GeometryReference reference(QString object, QString role = "Object", QString entity = {}) {
    domain::GeometryReference r;
    r.object = object;
    r.role = role;
    r.entity = entity;
    if (role == "Face" || role == "Edge" || role == "Vertex")
        r.topology = role.toLower() + "/" + QString(64, 'a');
    return r;
}
template <class F> void errorCode(QString expected, F &&action) {
    QString code;
    try {
        action();
    } catch (const domain::Error &e) {
        code = e.code;
    }
    if (code != expected)
        throw std::runtime_error("Expected " + expected.toStdString() + ", observed " +
                                 (code.isEmpty() ? "success" : code.toStdString()));
}

void referenceRoundTrip() {
    const QString owner = "00000000-0000-4000-8000-000000000001";
    domain::GeometryReference ref;
    ref.object = owner;
    ref.role = "Edge";
    ref.topology = "edge/" + QString(64, 'a');
    ref.endpoint = "End";
    auto encoded = ref.map();
    CHECK(encoded.value("version").toInteger() == 1);
    CHECK(encoded.value("object").toString() == owner);
    auto decoded = domain::GeometryReference::parse(encoded);
    CHECK(decoded.document == "Self");
    CHECK(decoded.role == "Edge");
    CHECK(decoded.topology == "edge/" + QString(64, 'a'));
    CHECK(decoded.endpoint == "End");
    CHECK(!decoded.parameter);
}

void referenceSelectors() {
    domain::GeometryReference point;
    point.object = "00000000-0000-4000-8000-000000000001";
    point.role = "Point";
    point.entity = "00000000-0000-4000-8000-000000000002";
    auto bad = point;
    bad.parameter = .5;
    rejects([&] { bad.validate(); }); // A point cannot select a curve parameter.
    bad = point;
    bad.topology = "vertex/" + QString(64, 'a');
    rejects([&] { bad.validate(); }); // Entity and topology namespaces cannot mix.
    bad = point;
    bad.role = "Plane";
    rejects([&] { bad.validate(); });
    bad = point;
    bad.document = "display document 0";
    rejects([&] { bad.validate(); });
    bad = point;
    bad.entity.clear();
    rejects([&] { bad.validate(); });
    bad = point;
    bad.role = "display-index";
    rejects([&] { bad.validate(); });

    domain::GeometryReference curve = point;
    curve.role = "Curve";
    curve.parameter = .25;
    CHECK(domain::GeometryReference::parse(curve.map()).parameter == .25);
    bad = curve;
    bad.endpoint = "Start";
    rejects([&] { bad.validate(); });
    bad = curve;
    bad.parameter = std::numeric_limits<double>::quiet_NaN();
    rejects([&] { bad.validate(); });
    bad.parameter = 1000001.;
    rejects([&] { bad.validate(); });
    bad = curve;
    bad.parameter.reset();
    bad.endpoint = "0";
    rejects([&] { bad.validate(); });

    domain::GeometryReference edge;
    edge.object = point.object;
    edge.role = "Edge";
    edge.topology = "face/" + QString(64, 'a');
    rejects([&] { edge.validate(); });
    edge.topology = "edge/0";
    rejects([&] { edge.validate(); });
    auto encoded = point.map();
    encoded.insert(QStringLiteral("version"), 2);
    rejects([&] { domain::GeometryReference::parse(encoded); });
    encoded = point.map();
    encoded.insert(QStringLiteral("display_index"), 0);
    rejects([&] { domain::GeometryReference::parse(encoded); });
    encoded = point.map();
    encoded.remove(QStringLiteral("document"));
    rejects([&] { domain::GeometryReference::parse(encoded); });
}

void mixedGraph() {
    auto f = identity(1), p = identity(2), s = identity(3), a = identity(4), c = identity(5),
         result = identity(6), independent = identity(7), edge = identity(8);
    std::vector<evaluation::GraphNode> nodes{
        {result, "Feature", {reference(c, "Curve")}, {}},
        {s, "Sketch", {reference(p, "Plane")}, {{edge, "Curve"}}},
        {c, "DerivedCurve", {reference(s, "Curve", edge), reference(a, "Axis")}, {}},
        {a, "DatumAxis", {reference(s, "Curve", edge)}, {}},
        {p, "DatumPlane", {reference(f, "Face")}, {}},
        {f, "Feature", {}, {}},
        {independent, "Feature", {}, {}}};
    evaluation::Graph graph(nodes, {f, p, s, a, c, result, independent});
    CHECK(graph.order() == QStringList({f, p, s, a, c, result, independent}));
    CHECK(graph.dependencies(result) == QStringList({c}));
    CHECK(graph.descendants(f) == QStringList({p, s, a, c, result}));
    CHECK(graph.descendants(s) == QStringList({a, c, result}));
    CHECK(graph.descendants(independent).empty());
}

void graphIntegrity() {
    using evaluation::Graph;
    using evaluation::GraphNode;
    auto source = identity(1), target = identity(2), point = identity(3);
    std::vector<GraphNode> valid{{source, "Sketch", {}, {{point, "Point"}}},
                                 {target, "DatumPlane", {reference(source, "Point", point)}, {}}};
    Graph good(valid);
    CHECK(good.dependencies(target) == QStringList({source}));
    auto bad = valid;
    bad.push_back({source, "Feature", {}, {}});
    rejects([&] { Graph g(bad); });
    bad = valid;
    bad[1].references[0].object = identity(99);
    rejects([&] { Graph g(bad); });
    bad = valid;
    bad[1].references[0].entity = identity(99);
    rejects([&] { Graph g(bad); });
    bad = valid;
    bad[1].references[0].role = "Curve";
    rejects([&] { Graph g(bad); });
    bad = valid;
    bad[0].kind = "DatumAxis";
    rejects([&] { Graph g(bad); });
    bad = valid;
    bad[0].kind = "DisplayMesh";
    rejects([&] { Graph g(bad); });
    bad = valid;
    bad[1].references = {reference(source, "Face")};
    rejects([&] { Graph g(bad); });
    bad = valid;
    bad[1].references[0].document = identity(80);
    rejects([&] { Graph g(bad, {}, identity(81)); });
    // Explicit local document and Self must resolve to the same source node.
    Graph local(bad, {}, identity(80));
    CHECK(local.dependencies(target) == QStringList({source}));
    rejects([&] { good.dependencies(identity(99)); });
    rejects([&] { good.descendants(identity(99)); });
}

void graphCycles() {
    using evaluation::Graph;
    using evaluation::GraphNode;
    auto f = identity(1), p = identity(2), s = identity(3);
    std::vector<GraphNode> self{{p, "DatumPlane", {reference(p, "Plane")}, {}}};
    errorCode("DependencyCycle", [&] { Graph g(self); });
    std::vector<GraphNode> mixed{{f, "Feature", {reference(s, "Plane")}, {}},
                                 {p, "DatumPlane", {reference(f, "Face")}, {}},
                                 {s, "Sketch", {reference(p, "Plane")}, {}}};
    errorCode("DependencyCycle", [&] { Graph g(mixed, {f, p, s}); });
    mixed[0].references.clear();
    Graph valid(mixed, {f, p, s});
    CHECK(valid.order() == QStringList({f, p, s}));
}

void graphHistory() {
    using evaluation::Graph;
    using evaluation::GraphNode;
    auto f = identity(1), p = identity(2), s = identity(3), later = identity(4);
    std::vector<GraphNode> nodes{{f, "Feature", {}, {}},
                                 {p, "DatumPlane", {reference(f, "Face")}, {}},
                                 {s, "Sketch", {reference(p, "Plane")}, {}},
                                 {later, "Feature", {reference(s, "Plane")}, {}}};
    errorCode("DependencyOrder", [&] { Graph g(nodes, {p, f, s, later}); });
    // Even when sketch/datum nodes have no history ordinal, a later feature
    // cannot feed an earlier feature through them.
    errorCode("DependencyOrder", [&] { Graph g(nodes, {later, f}); });
    errorCode("InvalidInput", [&] { Graph g(nodes, {f, f}); });
    errorCode("ReferenceMissing", [&] { Graph g(nodes, {identity(99)}); });
    Graph legacy(nodes, {f, later});
    CHECK(legacy.order() == QStringList({f, p, s, later}));
    CHECK(Graph({}).order().empty());
}

void graphBounds() {
    using evaluation::Graph;
    using evaluation::GraphNode;
    std::vector<GraphNode> chain;
    QStringList history;
    for (int i = 1; i <= 132; ++i) {
        GraphNode node{identity(i), i <= 100 ? "Feature" : "DatumPlane", {}, {}};
        if (i > 1)
            node.references.push_back(reference(identity(i - 1)));
        chain.push_back(node);
        history.append(node.id);
    }
    Graph full(chain, history);
    CHECK(full.order() == history);
    CHECK(full.descendants(identity(1)).size() == 131);
    chain.push_back({identity(133), "DatumAxis", {}, {}});
    errorCode("ResourceLimit", [&] { Graph g(chain); });
    chain.resize(101);
    chain.back().kind = "Feature";
    errorCode("ResourceLimit", [&] { Graph g(chain); });
    chain.resize(33);
    for (auto &n : chain)
        n.kind = "Sketch";
    errorCode("ResourceLimit", [&] { Graph g(chain); });

    GraphNode source{identity(1), "Sketch", {}, {}}, target{identity(2), "DatumAxis", {}, {}};
    for (int i = 0; i < 256; ++i)
        source.members.insert(identity(1000 + i), "Point");
    for (int i = 0; i < 512; ++i)
        target.references.push_back(reference(source.id, "Point", identity(1000 + i % 256)));
    Graph bounded({source, target});
    CHECK(bounded.dependencies(target.id) == QStringList({source.id}));
    target.references.push_back(reference(source.id));
    errorCode("ResourceLimit", [&] { Graph g({source, target}); });
    target.references.pop_back();
    source.members.insert(identity(2000), "Point");
    errorCode("ResourceLimit", [&] { Graph g({source, target}); });
}

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    return run([] {
        referenceRoundTrip();
        referenceSelectors();
        mixedGraph();
        graphIntegrity();
        graphCycles();
        graphHistory();
        graphBounds();
    });
}
