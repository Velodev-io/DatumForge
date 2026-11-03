#include "features/Model.hpp"
#include "features/ModelingIntent.hpp"
#include "sketch/Sketch.hpp"
#include "support/check.hpp"
#include <QCoreApplication>
#include <limits>
#include <numbers>
using namespace cad;
using sketch::fields;
namespace {
QCborMap profile(QString sketch) {
    return fields({{"sketch", sketch}, {"mode", "AllClosed"}, {"regions", QCborArray{}}});
}
QCborMap blind(double value = .01) {
    return fields({{"mode", "Blind"},
                   {"value", value},
                   {"draft", fields({{"angle", 0.}, {"outward", false}})}});
}
QCborMap extrude(QString sketch) {
    return fields({{"version", 1},
                   {"id", newId()},
                   {"kind", "Extrude"},
                   {"operation", "NewBody"},
                   {"target", QCborValue(nullptr)},
                   {"profile", profile(sketch)},
                   {"start", fields({{"mode", "SketchPlane"}})},
                   {"direction1", blind()},
                   {"direction2", QCborValue(nullptr)},
                   {"reverse", false},
                   {"thin", fields({{"mode", "Solid"}})}});
}
void roundtrip(const QCborMap &m) {
    auto intent = features::ModelingIntent::parse(m);
    CHECK(intent.map() == m);
    CHECK(features::ModelingIntent::parse(QCborValue::fromCbor(QCborValue(m).toCbor()).toMap())
              .map() == m);
}
QCborMap reference(QString owner, QString role) {
    domain::GeometryReference r;
    r.object = owner;
    r.role = role;
    if (role == "Face" || role == "Edge" || role == "Vertex")
        r.topology = role.toLower() + "/" + QString(64, 'a');
    if (role == "Point")
        r.entity = newId();
    return r.map();
}
QCborMap body(QString owner) {
    return fields({{"source", reference(owner, "Object")}, {"body", newId()}});
}
void badField(const QCborMap &m, QString field, QCborValue value) {
    auto bad = m;
    bad.insert(field, value);
    rejects([&] { features::ModelingIntent::parse(bad); });
}
void modeContracts(QString sketch) {
    auto original = extrude(sketch);
    auto source = newId();
    auto target = body(source);
    // Independent depths and drafts retain the course's 10 mm / 15 mm values.
    auto m = original;
    auto first = blind(.01);
    first.insert(QStringLiteral("draft"),
                 fields({{"angle", 35 * std::numbers::pi / 180}, {"outward", true}}));
    m.insert(QStringLiteral("direction1"), first);
    m.insert(QStringLiteral("direction2"), blind(.015));
    roundtrip(m);
    auto parsed = features::ModelingIntent::parse(m).map();
    near(parsed.value("direction1").toMap().value("value").toDouble(), .01, 1e-14);
    near(parsed.value("direction2").toMap().value("value").toDouble(), .015, 1e-14);
    for (auto start :
         {fields({{"mode", "SketchPlane"}}), fields({{"mode", "Offset"}, {"distance", -.005}}),
          fields({{"mode", "Surface"}, {"reference", reference(source, "Face")}}),
          fields({{"mode", "Vertex"}, {"reference", reference(source, "Vertex")}})}) {
        m = original;
        m.insert(QStringLiteral("start"), start);
        roundtrip(m);
    }
    for (QString operation : {"NewBody", "Add", "Cut"}) {
        m = original;
        m.insert(QStringLiteral("operation"), operation);
        if (operation != "NewBody")
            m.insert(QStringLiteral("target"), target);
        roundtrip(m);
        for (auto end :
             {fields({{"mode", "Blind"}, {"value", .01}}),
              fields({{"mode", "Midplane"}, {"value", .03}}),
              fields({{"mode", "UpToVertex"}, {"reference", reference(source, "Vertex")}}),
              fields({{"mode", "UpToSurface"}, {"reference", reference(source, "Face")}}),
              fields({{"mode", "OffsetFromSurface"},
                      {"reference", reference(source, "Face")},
                      {"offset", .002},
                      {"offset_reverse", true}}),
              fields({{"mode", "UpToBody"},
                      {"reference", reference(source, "Object")},
                      {"body", newId()}}),
              fields({{"mode", "UpToNext"}}), fields({{"mode", "ThroughAll"}})}) {
            end.insert(QStringLiteral("draft"), fields({{"angle", 0.}, {"outward", false}}));
            auto candidate = m;
            candidate.insert(QStringLiteral("direction1"), end);
            auto mode = end.value("mode").toString();
            if (operation == "NewBody" && (mode == "UpToNext" || mode == "ThroughAll"))
                rejects([&] { features::ModelingIntent::parse(candidate); });
            else {
                roundtrip(candidate);
                candidate.insert(QStringLiteral("direction2"), blind(.02));
                if (mode == "Midplane")
                    rejects([&] { features::ModelingIntent::parse(candidate); });
                else
                    roundtrip(candidate);
            }
        }
    }
    for (auto thin :
         {fields({{"mode", "Solid"}}),
          fields({{"mode", "OneDirection"}, {"thickness", .003}, {"reverse", true}}),
          fields({{"mode", "Midplane"}, {"thickness", .006}}),
          fields({{"mode", "TwoDirection"}, {"thickness1", .002}, {"thickness2", .004}})}) {
        m = original;
        m.insert(QStringLiteral("thin"), thin);
        roundtrip(m);
        m.insert(QStringLiteral("kind"), "Revolve");
        m.insert(QStringLiteral("axis"), reference(source, "Axis"));
        m.insert(QStringLiteral("direction1"),
                 fields({{"mode", "Blind"}, {"value", 2 * std::numbers::pi}}));
        roundtrip(m);
        m.insert(QStringLiteral("direction2"), fields({{"mode", "Blind"}, {"value", .1}}));
        rejects([&] { features::ModelingIntent::parse(m); });
        m.insert(QStringLiteral("direction1"),
                 fields({{"mode", "Blind"}, {"value", std::numbers::pi}}));
        roundtrip(m);
        m.insert(QStringLiteral("direction1"),
                 fields({{"mode", "Midplane"}, {"value", std::numbers::pi}}));
        rejects([&] { features::ModelingIntent::parse(m); });
        m.insert(QStringLiteral("direction2"), QCborValue(nullptr));
        roundtrip(m);
        badField(m, "start", fields({{"mode", "Offset"}, {"distance", .01}}));
        badField(m, "direction1", blind()); // Revolve must not accept extrude draft fields.
    }
    auto draft = fields({{"version", 1},
                         {"id", newId()},
                         {"kind", "Draft"},
                         {"target", target},
                         {"neutral", reference(source, "Face")},
                         {"pull", reference(newId(), "Axis")},
                         {"faces", QCborArray{reference(source, "Face")}},
                         {"angle", .1},
                         {"outward", true}});
    roundtrip(draft);
    badField(draft, "angle", 0.);
    badField(draft, "angle", std::numbers::pi / 2);
    badField(draft, "faces", QCborArray{});
    badField(draft, "faces", QCborArray{reference(source, "Face"), reference(source, "Face")});
    badField(draft, "faces", QCborArray{reference(source, "Edge")});
    badField(draft, "neutral", reference(source, "Vertex"));
    badField(draft, "pull", reference(source, "Face"));
    badField(draft, "operation", "NewBody");
    // Self and the explicit current document are equivalent reference scopes.
    auto scopedDraft = draft;
    auto scopedFace = reference(source, "Face");
    auto currentDocument = newId();
    scopedFace.insert(QStringLiteral("document"), currentDocument);
    scopedDraft.insert(QStringLiteral("faces"), QCborArray{scopedFace});
    auto scoped = features::ModelingIntent::parse(scopedDraft);
    auto pull = draft.value("pull").toMap().value("object").toString();
    scoped.graph({{source, "Feature", {}, {}}, {pull, "DatumAxis", {}, {}}},
                 {source, pull, scoped.graphNode().id}, currentDocument);
    rejects([&] {
        scoped.graph({{source, "Feature", {}, {}}, {pull, "DatumAxis", {}, {}}},
                     {source, pull, scoped.graphNode().id});
    });
    badField(original, "target", target);
    badField(original, "operation", "UnionMaybe");
    badField(original, "direction2", false);
    badField(original, "reverse", 1);
    badField(original, "version", 2);
    badField(original, "version", 1.);
    badField(original, "display_index", 4);
    badField(original, "thin", fields({{"mode", "Midplane"}, {"thickness", 0.}}));
    for (double value : {0., -1., 1001., std::numeric_limits<double>::infinity(),
                         std::numeric_limits<double>::quiet_NaN()})
        badField(original, "direction1", blind(value));
    for (double value : {-1., std::numbers::pi / 2, std::numeric_limits<double>::quiet_NaN()}) {
        auto end = blind();
        end.insert(QStringLiteral("draft"), fields({{"angle", value}, {"outward", false}}));
        badField(original, "direction1", end);
    }
    auto end = blind();
    end.insert(QStringLiteral("reference"), reference(source, "Face"));
    badField(original, "direction1", end); // Inactive reference must not survive switching modes.
    auto secondReference = fields({{"mode", "UpToSurface"},
                                   {"reference", reference(source, "Face")},
                                   {"draft", fields({{"angle", .2}, {"outward", true}})}});
    auto asymmetric = original;
    asymmetric.insert(QStringLiteral("direction2"), secondReference);
    asymmetric.insert(QStringLiteral("reverse"), true);
    roundtrip(asymmetric);
    auto midpoint = blind(.03);
    midpoint.insert(QStringLiteral("mode"), "Midplane");
    badField(original, "direction2", midpoint);
    auto invalidStart = fields({{"mode", "Surface"}, {"reference", reference(source, "Axis")}});
    badField(original, "start", invalidStart);
    auto pointCurve = reference(source, "Edge");
    pointCurve.insert(QStringLiteral("endpoint"), "Start");
    auto vertexStart = fields({{"mode", "Vertex"}, {"reference", pointCurve}});
    auto vertex = original;
    vertex.insert(QStringLiteral("start"), vertexStart);
    roundtrip(vertex);
    pointCurve.remove(QStringLiteral("endpoint"));
    badField(original, "start", fields({{"mode", "Vertex"}, {"reference", pointCurve}}));
    for (auto mode : {"OneDirection", "Midplane", "TwoDirection"}) {
        QCborMap thickness;
        if (QString(mode) == "TwoDirection")
            thickness = fields({{"mode", mode}, {"thickness1", .001}, {"thickness2", -.001}});
        else {
            thickness = fields({{"mode", mode}, {"thickness", -.001}});
            if (QString(mode) == "OneDirection")
                thickness.insert(QStringLiteral("reverse"), false);
        }
        badField(original, "thin", thickness);
    }
    std::cout << "mode contracts passed\n";
}
QCborMap selected(QString sketch, QString mode, QString curve) {
    auto use = fields({{"entity", curve}, {"from", 0.}, {"to", 1.}, {"reverse", false}});
    return fields(
        {{"sketch", sketch}, {"mode", mode}, {"regions", QCborArray{QCborArray{QCborArray{use}}}}});
}
void profileContracts(QString sketch) {
    auto m = extrude(sketch);
    auto curve = newId();
    m.insert(QStringLiteral("profile"), selected(sketch, "SelectedRegions", curve));
    roundtrip(m);
    auto node = features::ModelingIntent::parse(m).graphNode();
    CHECK(node.references.size() == 2);
    CHECK(node.references[1].role == "Curve" && node.references[1].entity == curve);
    badField(m, "flip_side", false);
    auto p = selected(sketch, "OpenChains", curve);
    m.insert(QStringLiteral("profile"), p);
    rejects([&] { features::ModelingIntent::parse(m); });
    m.insert(QStringLiteral("thin"),
             fields({{"mode", "OneDirection"}, {"thickness", .001}, {"reverse", false}}));
    roundtrip(m);
    badField(m, "flip_side", false);
    m.insert(QStringLiteral("thin"), fields({{"mode", "Solid"}}));
    m.insert(QStringLiteral("operation"), "Cut");
    m.insert(QStringLiteral("target"), body(newId()));
    rejects([&] { features::ModelingIntent::parse(m); });
    for (bool flip : {false, true}) {
        m.insert(QStringLiteral("flip_side"), flip);
        roundtrip(m);
    }
    badField(m, "flip_side", 0);
    auto revolve = m;
    revolve.remove(QStringLiteral("flip_side"));
    revolve.insert(QStringLiteral("kind"), "Revolve");
    revolve.insert(QStringLiteral("axis"), reference(newId(), "Axis"));
    revolve.insert(QStringLiteral("direction1"), fields({{"mode", "Blind"}, {"value", 1.}}));
    rejects([&] { features::ModelingIntent::parse(revolve); });
    revolve.insert(QStringLiteral("thin"), fields({{"mode", "Midplane"}, {"thickness", .002}}));
    roundtrip(revolve);
    auto original = extrude(sketch);
    auto use = fields({{"entity", curve}, {"from", 0.}, {"to", 1.}, {"reverse", false}});
    auto setLoops = [&](QCborArray regions) {
        auto candidate = original;
        auto selection = selected(sketch, "SelectedRegions", curve);
        selection.insert(QStringLiteral("regions"), regions);
        candidate.insert(QStringLiteral("profile"), selection);
        return candidate;
    };
    for (auto regions :
         {QCborArray{}, QCborArray{QCborArray{}}, QCborArray{QCborArray{QCborArray{}}},
          QCborArray{QCborArray{QCborArray{use, use}}}}) {
        auto candidate = setLoops(regions);
        rejects([&] { features::ModelingIntent::parse(candidate); });
    }
    for (auto key : {"entity", "from", "to", "reverse", "extra"}) {
        auto bad = use;
        if (QString(key) == "entity")
            bad.insert(QStringLiteral("entity"), "display/0");
        else if (QString(key) == "reverse")
            bad.insert(QStringLiteral("reverse"), 0);
        else
            bad.insert(QString::fromLatin1(key), 2.);
        auto candidate = setLoops(QCborArray{QCborArray{QCborArray{bad}}});
        rejects([&] { features::ModelingIntent::parse(candidate); });
    }
    QCborArray boundary;
    for (int i = 0; i < 256; ++i) {
        auto next = use;
        next.insert(QStringLiteral("entity"), newId());
        boundary.append(next);
    }
    roundtrip(setLoops(QCborArray{QCborArray{boundary}}));
    boundary.append(use);
    auto tooLarge = setLoops(QCborArray{QCborArray{boundary}});
    rejects([&] { features::ModelingIntent::parse(tooLarge); });
    QCborArray regions;
    for (int i = 0; i < 32; ++i)
        regions.append(QCborArray{QCborArray{use}});
    roundtrip(setLoops(regions));
    regions.append(QCborArray{QCborArray{use}});
    tooLarge = setLoops(regions);
    rejects([&] { features::ModelingIntent::parse(tooLarge); });
    auto oversized = original;
    oversized.insert(QStringLiteral("padding"), QString(131072, 'x'));
    rejects([&] { features::ModelingIntent::parse(oversized); });
    auto intent = features::ModelingIntent::parse(original);
    auto detached = intent.map();
    detached.insert(QStringLiteral("reverse"), true);
    CHECK(!intent.map().value("reverse").toBool());
    std::cout << "profile contracts passed\n";
}
void graphContracts(QString sketch) {
    auto m = extrude(sketch);
    auto source = newId(), datum = newId(), curve = newId();
    auto start = fields({{"mode", "Surface"}, {"reference", reference(datum, "Plane")}});
    m.insert(QStringLiteral("start"), start);
    auto end = blind();
    end.remove(QStringLiteral("value"));
    end.insert(QStringLiteral("mode"), "UpToSurface");
    end.insert(QStringLiteral("reference"), reference(source, "Face"));
    m.insert(QStringLiteral("direction1"), end);
    m.insert(QStringLiteral("profile"), selected(sketch, "SelectedRegions", curve));
    auto intent = features::ModelingIntent::parse(m);
    auto id = intent.graphNode().id;
    std::vector<evaluation::GraphNode> upstream{{sketch, "Sketch", {}, {{curve, "Curve"}}},
                                                {source, "Feature", {}, {}},
                                                {datum, "DatumPlane", {}, {}}};
    QStringList history{sketch, source, datum, id};
    auto graph = intent.graph(upstream, history);
    CHECK(graph.dependencies(id).contains(source));
    CHECK(graph.dependencies(id).contains(datum));
    CHECK(graph.descendants(source) == QStringList({id}));
    CHECK(graph.descendants(sketch) == QStringList({id}));
    rejects([&] { intent.graph(upstream, {}); });
    rejects([&] { intent.graph(upstream, {id, sketch, source, datum}); });
    auto broken = upstream;
    broken[0].members.clear();
    rejects([&] { intent.graph(broken, history); });
    broken = upstream;
    broken[0].members[curve] = "Point";
    rejects([&] { intent.graph(broken, history); });
    broken = upstream;
    broken.erase(broken.begin() + 1);
    rejects([&] { intent.graph(broken, history); });
    domain::GeometryReference back;
    back.object = id;
    broken = upstream;
    broken[1].references = {back};
    rejects([&] { intent.graph(broken, history); });
    // Object references alone cannot distinguish a body stage from a sketch.
    auto all = extrude(sketch);
    all.insert(QStringLiteral("operation"), "Cut");
    all.insert(QStringLiteral("target"), body(source));
    auto bodyIntent = features::ModelingIntent::parse(all);
    rejects([&] {
        bodyIntent.graph({{sketch, "Sketch", {}, {}}, {source, "Sketch", {}, {}}},
                         {sketch, source, bodyIntent.graphNode().id});
    });
    auto allIntent = features::ModelingIntent::parse(extrude(sketch));
    rejects([&] {
        allIntent.graph({{sketch, "DatumPlane", {}, {}}}, {sketch, allIntent.graphNode().id});
    });
    auto external = end.value("reference").toMap();
    auto document = newId();
    external.insert(QStringLiteral("document"), document);
    end.insert(QStringLiteral("reference"), external);
    m.insert(QStringLiteral("direction1"), end);
    auto explicitScope = features::ModelingIntent::parse(m);
    rejects([&] { explicitScope.graph(upstream, history); });
    explicitScope.graph(upstream, history, document);
    std::cout << "graph contracts passed\n";
}
} // namespace
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    return run([] {
        auto sketch = newId();
        auto m = extrude(sketch);
        roundtrip(m);
        auto intent = features::ModelingIntent::parse(m);
        CHECK(intent.graphNode().references.size() == 1);
        CHECK(intent.graphNode().references[0].object == sketch);
        auto graph = intent.graph({{sketch, "Sketch", {}, {}}}, {sketch, m.value("id").toString()});
        CHECK(graph.dependencies(m.value("id").toString()) == QStringList({sketch}));
        rejects([&] { features::Feature::parse(m); });
        modeContracts(sketch);
        profileContracts(sketch);
        graphContracts(sketch);
    });
}
