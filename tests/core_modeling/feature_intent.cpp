#include "features/Model.hpp"
#include "features/ModelingIntent.hpp"
#include "sketch/Sketch.hpp"
#include "support/check.hpp"
#include <QCoreApplication>
#include <limits>
#include <numbers>
#include <utility>
#include <vector>
using namespace cad;
using sketch::fields;
namespace {
template <class F> void error(QString expected, F &&action) {
    QString code, message;
    try {
        action();
    } catch (const domain::Error &e) {
        code = e.code;
        message = QString::fromUtf8(e.what());
    }
    if (code != expected)
        throw std::runtime_error("Expected " + expected.toStdString() + ", got " +
                                 code.toStdString() + ": " + message.toStdString());
}
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
    auto back = domain::GeometryReference::parse(reference(id, "Face"));
    broken = upstream;
    broken[1].references = {back};
    error("DependencyCycle", [&] { intent.graph(broken, history); });
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
void ambiguousInteractions(QString sketch) {
    auto original = extrude(sketch);
    auto source = newId();
    auto target = body(source);

    // 1. Midplane depth, thickness, and revolve angle are totals
    auto midExtrude = original;
    auto midEnd = blind(.03);
    midEnd.insert(QStringLiteral("mode"), "Midplane");
    midExtrude.insert(QStringLiteral("direction1"), midEnd);
    roundtrip(midExtrude);
    auto parsedMid = features::ModelingIntent::parse(midExtrude).map();
    CHECK(parsedMid.value("direction1").toMap().value("mode").toString() == "Midplane");
    near(parsedMid.value("direction1").toMap().value("value").toDouble(), .03, 1e-14);
    CHECK(parsedMid.value("direction2").isNull());

    auto midThin = original;
    midThin.insert(QStringLiteral("thin"), fields({{"mode", "Midplane"}, {"thickness", .006}}));
    roundtrip(midThin);
    auto parsedThin = features::ModelingIntent::parse(midThin).map();
    CHECK(parsedThin.value("thin").toMap().value("mode").toString() == "Midplane");
    near(parsedThin.value("thin").toMap().value("thickness").toDouble(), .006, 1e-14);

    auto midRevolve = original;
    midRevolve.insert(QStringLiteral("kind"), "Revolve");
    midRevolve.insert(QStringLiteral("axis"), reference(source, "Axis"));
    midRevolve.insert(QStringLiteral("direction1"),
                      fields({{"mode", "Midplane"}, {"value", std::numbers::pi}}));
    roundtrip(midRevolve);
    auto parsedRev = features::ModelingIntent::parse(midRevolve).map();
    CHECK(parsedRev.value("direction1").toMap().value("mode").toString() == "Midplane");
    near(parsedRev.value("direction1").toMap().value("value").toDouble(), std::numbers::pi, 1e-14);
    CHECK(parsedRev.value("direction2").isNull());

    // 2. Direction 2 is inactive for Midplane (extrude and revolve)
    auto invalidDir2 = midExtrude;
    invalidDir2.insert(QStringLiteral("direction2"), blind(.01));
    rejects([&] { features::ModelingIntent::parse(invalidDir2); });
    invalidDir2.insert(QStringLiteral("direction2"), fields({}));
    rejects([&] { features::ModelingIntent::parse(invalidDir2); });

    auto invalidRevDir2 = midRevolve;
    invalidRevDir2.insert(QStringLiteral("direction2"), fields({{"mode", "Blind"}, {"value", .2}}));
    rejects([&] { features::ModelingIntent::parse(invalidRevDir2); });

    auto midInDir2 = original;
    midInDir2.insert(QStringLiteral("direction2"), midEnd);
    rejects([&] { features::ModelingIntent::parse(midInDir2); });

    // 3. Direction drafts are independent
    auto draftFixture = original;
    auto dir1 = blind(.010);
    dir1.insert(QStringLiteral("draft"),
                fields({{"angle", 35. * std::numbers::pi / 180.}, {"outward", true}}));
    auto dir2 = blind(.015);
    dir2.insert(QStringLiteral("draft"), fields({{"angle", 0.}, {"outward", false}}));
    draftFixture.insert(QStringLiteral("direction1"), dir1);
    draftFixture.insert(QStringLiteral("direction2"), dir2);
    roundtrip(draftFixture);
    auto parsedDraft = features::ModelingIntent::parse(draftFixture).map();
    auto pDir1Draft = parsedDraft.value("direction1").toMap().value("draft").toMap();
    auto pDir2Draft = parsedDraft.value("direction2").toMap().value("draft").toMap();
    near(pDir1Draft.value("angle").toDouble(), 35. * std::numbers::pi / 180., 1e-14);
    CHECK(pDir1Draft.value("outward").toBool() == true);
    near(pDir2Draft.value("angle").toDouble(), 0., 1e-14);
    CHECK(pDir2Draft.value("outward").toBool() == false);

    dir1.insert(QStringLiteral("draft"), fields({{"angle", 0.}, {"outward", false}}));
    dir2.insert(QStringLiteral("draft"),
                fields({{"angle", 20. * std::numbers::pi / 180.}, {"outward", true}}));
    draftFixture.insert(QStringLiteral("direction1"), dir1);
    draftFixture.insert(QStringLiteral("direction2"), dir2);
    roundtrip(draftFixture);
    parsedDraft = features::ModelingIntent::parse(draftFixture).map();
    pDir1Draft = parsedDraft.value("direction1").toMap().value("draft").toMap();
    pDir2Draft = parsedDraft.value("direction2").toMap().value("draft").toMap();
    near(pDir1Draft.value("angle").toDouble(), 0., 1e-14);
    CHECK(pDir1Draft.value("outward").toBool() == false);
    near(pDir2Draft.value("angle").toDouble(), 20. * std::numbers::pi / 180., 1e-14);
    CHECK(pDir2Draft.value("outward").toBool() == true);

    dir1.insert(QStringLiteral("draft"),
                fields({{"angle", 10. * std::numbers::pi / 180.}, {"outward", false}}));
    dir2.insert(QStringLiteral("draft"),
                fields({{"angle", 25. * std::numbers::pi / 180.}, {"outward", true}}));
    draftFixture.insert(QStringLiteral("direction1"), dir1);
    draftFixture.insert(QStringLiteral("direction2"), dir2);
    roundtrip(draftFixture);

    auto revDraft = original;
    revDraft.insert(QStringLiteral("kind"), "Revolve");
    revDraft.insert(QStringLiteral("axis"), reference(source, "Axis"));
    revDraft.insert(QStringLiteral("direction1"), fields({{"mode", "Blind"}, {"value", 1.}}));
    revDraft.insert(QStringLiteral("direction2"),
                    fields({{"mode", "Blind"},
                            {"value", 1.},
                            {"draft", fields({{"angle", .1}, {"outward", false}})}}));
    rejects([&] { features::ModelingIntent::parse(revDraft); });

    // 4. Offset side is independent of reversal
    for (bool rev : {false, true}) {
        for (bool offRev : {false, true}) {
            auto offsetCombo = original;
            offsetCombo.insert(QStringLiteral("operation"), "Add");
            offsetCombo.insert(QStringLiteral("target"), target);
            offsetCombo.insert(QStringLiteral("reverse"), rev);
            auto offEnd = fields({{"mode", "OffsetFromSurface"},
                                  {"reference", reference(source, "Face")},
                                  {"offset", .005},
                                  {"offset_reverse", offRev},
                                  {"draft", fields({{"angle", 0.}, {"outward", false}})}});
            offsetCombo.insert(QStringLiteral("direction1"), offEnd);
            roundtrip(offsetCombo);
            auto parsedCombo = features::ModelingIntent::parse(offsetCombo).map();
            CHECK(parsedCombo.value("reverse").toBool() == rev);
            auto parsedEnd = parsedCombo.value("direction1").toMap();
            CHECK(parsedEnd.value("offset_reverse").toBool() == offRev);
            near(parsedEnd.value("offset").toDouble(), .005, 1e-14);
        }
    }

    for (bool rev : {false, true}) {
        for (double dist : {-.005, .005}) {
            auto startOffset = original;
            startOffset.insert(QStringLiteral("reverse"), rev);
            startOffset.insert(QStringLiteral("start"),
                               fields({{"mode", "Offset"}, {"distance", dist}}));
            roundtrip(startOffset);
            auto parsedStart = features::ModelingIntent::parse(startOffset).map();
            CHECK(parsedStart.value("reverse").toBool() == rev);
            near(parsedStart.value("start").toMap().value("distance").toDouble(), dist, 1e-14);
        }
    }

    // 5. Revolve angles use radians and total at most one turn (2*pi)
    auto rev = original;
    rev.insert(QStringLiteral("kind"), "Revolve");
    rev.insert(QStringLiteral("axis"), reference(source, "Axis"));
    rev.insert(QStringLiteral("direction1"),
               fields({{"mode", "Blind"}, {"value", 2 * std::numbers::pi}}));
    roundtrip(rev);
    rev.insert(QStringLiteral("direction1"),
               fields({{"mode", "Blind"}, {"value", 2 * std::numbers::pi + 1e-6}}));
    rejects([&] { features::ModelingIntent::parse(rev); });
    rev.insert(QStringLiteral("direction1"),
               fields({{"mode", "Midplane"}, {"value", 2 * std::numbers::pi}}));
    roundtrip(rev);
    rev.insert(QStringLiteral("direction1"),
               fields({{"mode", "Midplane"}, {"value", 2 * std::numbers::pi + 1e-4}}));
    rejects([&] { features::ModelingIntent::parse(rev); });
    rev.insert(QStringLiteral("direction1"),
               fields({{"mode", "Blind"}, {"value", 1.2 * std::numbers::pi}}));
    rev.insert(QStringLiteral("direction2"),
               fields({{"mode", "Blind"}, {"value", .8 * std::numbers::pi}}));
    roundtrip(rev);
    rev.insert(QStringLiteral("direction2"),
               fields({{"mode", "Blind"}, {"value", .8001 * std::numbers::pi}}));
    rejects([&] { features::ModelingIntent::parse(rev); });
    rev.insert(QStringLiteral("direction1"),
               fields({{"mode", "Blind"}, {"value", 1.5 * std::numbers::pi}}));
    rev.insert(QStringLiteral("direction2"),
               fields({{"mode", "Blind"}, {"value", .5 * std::numbers::pi}}));
    roundtrip(rev);
    rev.insert(QStringLiteral("direction2"), QCborValue(nullptr));
    for (double badAngle : {0., -0.1, std::numeric_limits<double>::quiet_NaN(),
                            std::numeric_limits<double>::infinity()}) {
        rev.insert(QStringLiteral("direction1"), fields({{"mode", "Blind"}, {"value", badAngle}}));
        rejects([&] { features::ModelingIntent::parse(rev); });
    }

    // 6. Open non-thin cut alone requires flip-side
    auto openCurve = newId();
    auto openProfile = selected(sketch, "OpenChains", openCurve);
    auto selProfile = selected(sketch, "SelectedRegions", openCurve);

    auto openSolidCut = original;
    openSolidCut.insert(QStringLiteral("operation"), "Cut");
    openSolidCut.insert(QStringLiteral("target"), target);
    openSolidCut.insert(QStringLiteral("profile"), openProfile);
    openSolidCut.insert(QStringLiteral("thin"), fields({{"mode", "Solid"}}));
    rejects([&] { features::ModelingIntent::parse(openSolidCut); });
    openSolidCut.insert(QStringLiteral("flip_side"), false);
    roundtrip(openSolidCut);
    openSolidCut.insert(QStringLiteral("flip_side"), true);
    roundtrip(openSolidCut);

    for (QString op : {"NewBody", "Add"}) {
        auto openSolidOther = openSolidCut;
        openSolidOther.insert(QStringLiteral("operation"), op);
        if (op == "NewBody")
            openSolidOther.insert(QStringLiteral("target"), QCborValue(nullptr));
        rejects([&] { features::ModelingIntent::parse(openSolidOther); });
        openSolidOther.remove(QStringLiteral("flip_side"));
        rejects([&] { features::ModelingIntent::parse(openSolidOther); });
    }

    auto openSolidRevCut = openSolidCut;
    openSolidRevCut.insert(QStringLiteral("kind"), "Revolve");
    openSolidRevCut.insert(QStringLiteral("axis"), reference(source, "Axis"));
    openSolidRevCut.insert(QStringLiteral("direction1"),
                           fields({{"mode", "Blind"}, {"value", 1.}}));
    rejects([&] { features::ModelingIntent::parse(openSolidRevCut); });

    for (auto thinMode :
         {fields({{"mode", "OneDirection"}, {"thickness", .002}, {"reverse", false}}),
          fields({{"mode", "Midplane"}, {"thickness", .004}}),
          fields({{"mode", "TwoDirection"}, {"thickness1", .001}, {"thickness2", .003}})}) {
        auto openThinCut = openSolidCut;
        openThinCut.insert(QStringLiteral("thin"), thinMode);
        rejects([&] { features::ModelingIntent::parse(openThinCut); });
        openThinCut.remove(QStringLiteral("flip_side"));
        roundtrip(openThinCut);
    }

    auto closedCut = original;
    closedCut.insert(QStringLiteral("operation"), "Cut");
    closedCut.insert(QStringLiteral("target"), target);
    closedCut.insert(QStringLiteral("flip_side"), false);
    rejects([&] { features::ModelingIntent::parse(closedCut); });

    auto selCut = closedCut;
    selCut.insert(QStringLiteral("profile"), selProfile);
    rejects([&] { features::ModelingIntent::parse(selCut); });

    std::cout << "ambiguous interactions passed\n";
}
void failureAndImmutabilityContracts(QString sketch) {
    auto original = extrude(sketch);
    auto source = newId();
    auto target = body(source);

    // 1. Model immutability on parse failure and success
    {
        auto validMap = original;
        auto snapshot = validMap;
        auto intent = features::ModelingIntent::parse(validMap);
        CHECK(validMap == snapshot);

        auto badMap = original;
        badMap.insert(QStringLiteral("unknown_key"), "corrupt");
        auto badSnapshot = badMap;
        rejects([&] { features::ModelingIntent::parse(badMap); });
        CHECK(badMap == badSnapshot);
    }

    // 2. Inactive fields in start, end, thin
    {
        auto bad = original;
        bad.insert(QStringLiteral("start"), fields({{"mode", "SketchPlane"}, {"distance", .01}}));
        rejects([&] { features::ModelingIntent::parse(bad); });
    }
    {
        auto bad = original;
        bad.insert(QStringLiteral("start"),
                   fields({{"mode", "SketchPlane"}, {"reference", reference(source, "Face")}}));
        rejects([&] { features::ModelingIntent::parse(bad); });
    }
    {
        auto bad = original;
        bad.insert(QStringLiteral("direction1"),
                   fields({{"mode", "UpToSurface"},
                           {"reference", reference(source, "Face")},
                           {"offset", .002},
                           {"draft", fields({{"angle", 0.}, {"outward", false}})}}));
        rejects([&] { features::ModelingIntent::parse(bad); });
    }
    {
        auto bad = original;
        bad.insert(QStringLiteral("direction1"),
                   fields({{"mode", "UpToSurface"},
                           {"reference", reference(source, "Face")},
                           {"body", newId()},
                           {"draft", fields({{"angle", 0.}, {"outward", false}})}}));
        rejects([&] { features::ModelingIntent::parse(bad); });
    }
    {
        auto bad = original;
        bad.insert(QStringLiteral("operation"), "Cut");
        bad.insert(QStringLiteral("target"), target);
        bad.insert(QStringLiteral("direction1"),
                   fields({{"mode", "ThroughAll"},
                           {"reference", reference(source, "Face")},
                           {"draft", fields({{"angle", 0.}, {"outward", false}})}}));
        rejects([&] { features::ModelingIntent::parse(bad); });
    }
    {
        auto bad = original;
        bad.insert(QStringLiteral("operation"), "Cut");
        bad.insert(QStringLiteral("target"), target);
        bad.insert(QStringLiteral("direction1"),
                   fields({{"mode", "UpToNext"},
                           {"reference", reference(source, "Face")},
                           {"draft", fields({{"angle", 0.}, {"outward", false}})}}));
        rejects([&] { features::ModelingIntent::parse(bad); });
    }
    {
        auto bad = original;
        bad.insert(QStringLiteral("thin"), fields({{"mode", "Solid"}, {"thickness", .002}}));
        rejects([&] { features::ModelingIntent::parse(bad); });
    }
    {
        auto bad = original;
        bad.insert(QStringLiteral("thin"),
                   fields({{"mode", "Midplane"}, {"thickness", .002}, {"reverse", true}}));
        rejects([&] { features::ModelingIntent::parse(bad); });
    }
    {
        auto bad = original;
        bad.insert(QStringLiteral("thin"), fields({{"mode", "TwoDirection"},
                                                   {"thickness1", .001},
                                                   {"thickness2", .002},
                                                   {"thickness", .002}}));
        rejects([&] { features::ModelingIntent::parse(bad); });
    }
    {
        auto bad = original;
        bad.insert(QStringLiteral("thin"), fields({{"mode", "TwoDirection"},
                                                   {"thickness1", .001},
                                                   {"thickness2", .002},
                                                   {"reverse", false}}));
        rejects([&] { features::ModelingIntent::parse(bad); });
    }
    {
        auto bad = original;
        bad.insert(QStringLiteral("thin"), fields({{"mode", "OneDirection"},
                                                   {"thickness", .002},
                                                   {"thickness1", .001},
                                                   {"reverse", false}}));
        rejects([&] { features::ModelingIntent::parse(bad); });
    }
    {
        auto bad = original;
        bad.insert(QStringLiteral("axis"), reference(source, "Axis"));
        rejects([&] { features::ModelingIntent::parse(bad); });
    }

    // 3. Unknown keys in nested objects
    {
        auto bad = original;
        bad.insert(QStringLiteral("start"), fields({{"mode", "SketchPlane"}, {"bogus", 123}}));
        rejects([&] { features::ModelingIntent::parse(bad); });
    }
    {
        auto bad = original;
        bad.insert(QStringLiteral("direction1"),
                   fields({{"mode", "Blind"},
                           {"value", .01},
                           {"draft", fields({{"angle", 0.}, {"outward", false}})},
                           {"bogus", "extra"}}));
        rejects([&] { features::ModelingIntent::parse(bad); });
    }
    {
        auto bad = original;
        bad.insert(QStringLiteral("thin"), fields({{"mode", "Solid"}, {"bogus", true}}));
        rejects([&] { features::ModelingIntent::parse(bad); });
    }
    {
        auto bad = original;
        bad.insert(QStringLiteral("profile"), fields({{"sketch", sketch},
                                                      {"mode", "AllClosed"},
                                                      {"regions", QCborArray{}},
                                                      {"bogus", 1}}));
        rejects([&] { features::ModelingIntent::parse(bad); });
    }
    {
        auto bad = original;
        bad.insert(QStringLiteral("operation"), "Add");
        bad.insert(QStringLiteral("target"), fields({{"source", reference(source, "Object")},
                                                     {"body", newId()},
                                                     {"extra_key", "fail"}}));
        rejects([&] { features::ModelingIntent::parse(bad); });
    }
    {
        auto bad = original;
        auto end = blind();
        end.insert(QStringLiteral("draft"),
                   fields({{"angle", 0.}, {"outward", false}, {"extra", 1}}));
        bad.insert(QStringLiteral("direction1"), end);
        rejects([&] { features::ModelingIntent::parse(bad); });
    }

    // 4. Invalid units and ranges
    for (double badDist : {-1001., 1001., std::numeric_limits<double>::quiet_NaN(),
                           std::numeric_limits<double>::infinity()}) {
        auto bad = original;
        bad.insert(QStringLiteral("start"), fields({{"mode", "Offset"}, {"distance", badDist}}));
        rejects([&] { features::ModelingIntent::parse(bad); });
    }
    for (double badOff : {-.001, 1001., std::numeric_limits<double>::quiet_NaN(),
                          std::numeric_limits<double>::infinity()}) {
        auto bad = original;
        bad.insert(QStringLiteral("operation"), "Add");
        bad.insert(QStringLiteral("target"), target);
        bad.insert(QStringLiteral("direction1"),
                   fields({{"mode", "OffsetFromSurface"},
                           {"reference", reference(source, "Face")},
                           {"offset", badOff},
                           {"offset_reverse", false},
                           {"draft", fields({{"angle", 0.}, {"outward", false}})}}));
        rejects([&] { features::ModelingIntent::parse(bad); });
    }
    auto draftBase = fields({{"version", 1},
                             {"id", newId()},
                             {"kind", "Draft"},
                             {"target", target},
                             {"neutral", reference(source, "Face")},
                             {"pull", reference(newId(), "Axis")},
                             {"faces", QCborArray{reference(source, "Face")}},
                             {"angle", .1},
                             {"outward", true}});
    for (double badAngle :
         {0., -0.01, std::numbers::pi / 2, 2.0, std::numeric_limits<double>::quiet_NaN(),
          std::numeric_limits<double>::infinity()}) {
        auto bad = draftBase;
        bad.insert(QStringLiteral("angle"), badAngle);
        rejects([&] { features::ModelingIntent::parse(bad); });
    }
    auto curveId = newId();
    for (auto [from, to] :
         std::vector<std::pair<double, double>>{{0.5, 0.5}, {0.8, 0.2}, {-0.1, 0.5}, {0.2, 1.1}}) {
        auto use = fields({{"entity", curveId}, {"from", from}, {"to", to}, {"reverse", false}});
        auto badProfile = fields({{"sketch", sketch},
                                  {"mode", "SelectedRegions"},
                                  {"regions", QCborArray{QCborArray{QCborArray{use}}}}});
        auto bad = original;
        bad.insert(QStringLiteral("profile"), badProfile);
        rejects([&] { features::ModelingIntent::parse(bad); });
    }
    {
        auto use1 = fields({{"entity", curveId}, {"from", 0.}, {"to", 0.5}, {"reverse", false}});
        auto use2 = fields({{"entity", newId()}, {"from", 0.5}, {"to", 1.}, {"reverse", false}});
        auto multiBoundary =
            fields({{"sketch", sketch},
                    {"mode", "OpenChains"},
                    {"regions", QCborArray{QCborArray{QCborArray{use1}, QCborArray{use2}}}}});
        auto bad = original;
        bad.insert(QStringLiteral("profile"), multiBoundary);
        bad.insert(QStringLiteral("thin"),
                   fields({{"mode", "OneDirection"}, {"thickness", .002}, {"reverse", false}}));
        rejects([&] { features::ModelingIntent::parse(bad); });
    }

    // 5. Wrong reference kinds
    {
        auto bad = original;
        bad.insert(QStringLiteral("operation"), "Add");
        bad.insert(QStringLiteral("target"),
                   fields({{"source", reference(source, "Face")}, {"body", newId()}}));
        rejects([&] { features::ModelingIntent::parse(bad); });
    }
    {
        auto bad = original;
        bad.insert(QStringLiteral("start"),
                   fields({{"mode", "Surface"}, {"reference", reference(source, "Point")}}));
        rejects([&] { features::ModelingIntent::parse(bad); });
    }
    {
        auto bad = original;
        bad.insert(QStringLiteral("start"),
                   fields({{"mode", "Vertex"}, {"reference", reference(source, "Face")}}));
        rejects([&] { features::ModelingIntent::parse(bad); });
    }
    {
        auto bad = original;
        bad.insert(QStringLiteral("direction1"),
                   fields({{"mode", "UpToSurface"},
                           {"reference", reference(source, "Point")},
                           {"draft", fields({{"angle", 0.}, {"outward", false}})}}));
        rejects([&] { features::ModelingIntent::parse(bad); });
    }
    {
        auto bad = original;
        bad.insert(QStringLiteral("direction1"),
                   fields({{"mode", "UpToVertex"},
                           {"reference", reference(source, "Face")},
                           {"draft", fields({{"angle", 0.}, {"outward", false}})}}));
        rejects([&] { features::ModelingIntent::parse(bad); });
    }
    {
        auto bad = original;
        bad.insert(QStringLiteral("direction1"),
                   fields({{"mode", "UpToBody"},
                           {"reference", reference(source, "Face")},
                           {"body", newId()},
                           {"draft", fields({{"angle", 0.}, {"outward", false}})}}));
        rejects([&] { features::ModelingIntent::parse(bad); });
    }
    {
        auto otherSource = newId();
        auto bad = draftBase;
        bad.insert(QStringLiteral("faces"), QCborArray{reference(otherSource, "Face")});
        rejects([&] { features::ModelingIntent::parse(bad); });
    }

    // 6. Stale and missing producing stages fail with DependencyOrder / ReferenceMissing
    {
        auto cutIntent = original;
        cutIntent.insert(QStringLiteral("operation"), "Cut");
        cutIntent.insert(QStringLiteral("target"), target);
        auto parsedCut = features::ModelingIntent::parse(cutIntent);
        auto cutId = parsedCut.graphNode().id;
        std::vector<evaluation::GraphNode> upstream{{sketch, "Sketch", {}, {}},
                                                    {source, "Feature", {}, {}}};

        // Normal dependency order: sketch, source, cutId
        QStringList validHistory{sketch, source, cutId};
        auto g = parsedCut.graph(upstream, validHistory);
        CHECK(g.dependencies(cutId).contains(source));
        CHECK(g.dependencies(cutId).contains(sketch));

        // Stale producing stage: source placed AFTER cutId in history fails DependencyOrder
        QStringList staleHistory{sketch, cutId, source};
        auto upstreamCopy = upstream;
        auto staleCopy = staleHistory;
        error("DependencyOrder", [&] { parsedCut.graph(upstreamCopy, staleCopy); });
        CHECK(upstreamCopy.size() == upstream.size());
        CHECK(staleCopy == staleHistory);
        CHECK(parsedCut.map() == cutIntent);

        // Truncated history (missing elements) fails DependencyOrder
        QStringList truncatedHistory{sketch, cutId};
        auto truncatedCopy = truncatedHistory;
        error("DependencyOrder", [&] { parsedCut.graph(upstreamCopy, truncatedCopy); });
        CHECK(upstreamCopy.size() == upstream.size());
        CHECK(truncatedCopy == truncatedHistory);
        CHECK(parsedCut.map() == cutIntent);

        // History containing an identity not in the graph nodes fails ReferenceMissing
        QStringList missingNodeHistory{sketch, newId(), cutId};
        auto missingCopy = missingNodeHistory;
        error("ReferenceMissing", [&] { parsedCut.graph(upstreamCopy, missingCopy); });
        CHECK(upstreamCopy.size() == upstream.size());
        CHECK(missingCopy == missingNodeHistory);
        CHECK(parsedCut.map() == cutIntent);
    }

    // 7. Graph cycles fail with DependencyCycle without modifying inputs
    {
        auto cutIntent = original;
        cutIntent.insert(QStringLiteral("operation"), "Cut");
        cutIntent.insert(QStringLiteral("target"), target);
        auto parsedCut = features::ModelingIntent::parse(cutIntent);
        auto cutId = parsedCut.graphNode().id;

        // Upstream cycle: datumA references datumB and datumB references datumA
        auto datumA = newId();
        auto datumB = newId();
        auto toB = domain::GeometryReference::parse(reference(datumB, "Plane"));
        auto toA = domain::GeometryReference::parse(reference(datumA, "Plane"));
        std::vector<evaluation::GraphNode> cyclicUpstream{{sketch, "Sketch", {}, {}},
                                                          {datumA, "DatumPlane", {toB}, {}},
                                                          {datumB, "DatumPlane", {toA}, {}},
                                                          {source, "Feature", {}, {}}};
        QStringList cyclicHistory{sketch, datumA, datumB, source, cutId};
        auto cyclicUpCopy = cyclicUpstream;
        auto cyclicHistCopy = cyclicHistory;
        error("DependencyCycle", [&] { parsedCut.graph(cyclicUpCopy, cyclicHistCopy); });
        CHECK(cyclicUpCopy.size() == cyclicUpstream.size());
        CHECK(cyclicHistCopy == cyclicHistory);
        CHECK(parsedCut.map() == cutIntent);

        // Intent-upstream cycle: source references cutId while cutId depends on source
        auto toCut = domain::GeometryReference::parse(reference(cutId, "Face"));
        std::vector<evaluation::GraphNode> backEdgeUpstream{{sketch, "Sketch", {}, {}},
                                                            {source, "Feature", {toCut}, {}}};
        QStringList validHistory{sketch, source, cutId};
        auto backEdgeCopy = backEdgeUpstream;
        auto histCopy = validHistory;
        error("DependencyCycle", [&] { parsedCut.graph(backEdgeCopy, histCopy); });
        CHECK(backEdgeCopy.size() == backEdgeUpstream.size());
        CHECK(histCopy == validHistory);
        CHECK(parsedCut.map() == cutIntent);
    }

    // 8. Cross-document IDs fail without modifying input model
    {
        // Reference targeting foreign document rejected by graph projection
        auto crossDoc = original;
        auto extRef = reference(source, "Object");
        auto foreignDoc = newId();
        extRef.insert(QStringLiteral("document"), foreignDoc);
        crossDoc.insert(QStringLiteral("operation"), "Add");
        crossDoc.insert(QStringLiteral("target"), fields({{"source", extRef}, {"body", newId()}}));
        auto crossSnapshot = crossDoc;
        auto parsedCross = features::ModelingIntent::parse(crossDoc);
        CHECK(crossDoc == crossSnapshot);

        auto currentDoc = newId();
        std::vector<evaluation::GraphNode> upstream{{sketch, "Sketch", {}, {}},
                                                    {source, "Feature", {}, {}}};
        QStringList history{sketch, source, parsedCross.graphNode().id};
        auto upCopy = upstream;
        auto histCopy = history;
        error("UnsupportedOperation", [&] { parsedCross.graph(upCopy, histCopy, currentDoc); });
        CHECK(upCopy.size() == upstream.size());
        CHECK(histCopy == history);
        CHECK(parsedCross.map() == crossDoc);

        // Reference with malformed document ID format rejected at parse time
        auto malformedRef = reference(source, "Object");
        malformedRef.insert(QStringLiteral("document"), "not-a-valid-uuid!");
        auto malformedIntent = original;
        malformedIntent.insert(QStringLiteral("operation"), "Add");
        malformedIntent.insert(QStringLiteral("target"),
                               fields({{"source", malformedRef}, {"body", newId()}}));
        auto malformedSnapshot = malformedIntent;
        error("InvalidInput", [&] { features::ModelingIntent::parse(malformedIntent); });
        CHECK(malformedIntent == malformedSnapshot);
    }

    std::cout << "failure and immutability contracts passed\n";
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
        ambiguousInteractions(sketch);
        failureAndImmutabilityContracts(sketch);
    });
}
