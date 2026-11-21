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
template <class F> void error(QString expected, F &&action) {
    QString code, message;
    try {
        action();
    } catch (const domain::Error &e) {
        code = e.code;
        message = QString::fromUtf8(e.what());
    } catch (const std::exception &e) {
        code = "InvalidInput";
        message = QString::fromUtf8(e.what());
    }
    if (code != expected)
        throw std::runtime_error("Expected " + expected.toStdString() + ", got " +
                                 code.toStdString() + ": " + message.toStdString());
}

QCborMap reference(QString owner, QString role, QString entity = {}, QString topology = {}) {
    domain::GeometryReference r;
    r.object = owner;
    r.role = role;
    r.entity = entity;
    r.topology = topology.isEmpty() && (role == "Face" || role == "Edge" || role == "Vertex")
                     ? role.toLower() + "/" + QString(64, 'a')
                     : topology;
    return r.map();
}

QCborMap body(QString owner) {
    return fields({{"source", reference(owner, "Object")}, {"body", newId()}});
}

QCborMap sketchProfile(QString sketch, QString mode = "AllClosed") {
    return fields(
        {{"type", "Sketch"}, {"sketch", sketch}, {"mode", mode}, {"regions", QCborArray{}}});
}

QCborMap circularProfile(double diameter = 0.02) {
    return fields({{"type", "Circular"}, {"diameter", diameter}});
}

QCborMap path(QString owner, QString role = "Curve", QString entity = {}, bool reverse = false) {
    return fields({{"reference", reference(owner, role, entity)}, {"reverse", reverse}});
}

void roundtrip(const QCborMap &m) {
    auto intent = features::ModelingIntent::parse(m);
    CHECK(intent.map() == m);
    CHECK(features::ModelingIntent::parse(QCborValue::fromCbor(QCborValue(m).toCbor()).toMap())
              .map() == m);
}

void testShellContracts() {
    auto src = newId();
    auto tgt = body(src);
    auto face1 = reference(src, "Face", {}, "face/" + QString(64, '1'));
    auto face2 = reference(src, "Face", {}, "face/" + QString(64, '2'));
    auto face3 = reference(src, "Face", {}, "face/" + QString(64, '3'));

    // CM-201: Removed faces shell
    QCborMap shellOpen = fields({{"version", 1},
                                 {"id", newId()},
                                 {"kind", "Shell"},
                                 {"target", tgt},
                                 {"thickness", 0.002},
                                 {"inward", true},
                                 {"remove_faces", QCborArray{face1}},
                                 {"overrides", QCborArray{}}});
    roundtrip(shellOpen);

    // CM-202: Closed cavity shell (empty remove_faces)
    QCborMap shellClosed = fields({{"version", 1},
                                   {"id", newId()},
                                   {"kind", "Shell"},
                                   {"target", tgt},
                                   {"thickness", 0.003},
                                   {"inward", true},
                                   {"remove_faces", QCborArray{}},
                                   {"overrides", QCborArray{}}});
    roundtrip(shellClosed);

    // CM-203: Inward shell
    QCborMap shellInward = fields({{"version", 1},
                                   {"id", newId()},
                                   {"kind", "Shell"},
                                   {"target", tgt},
                                   {"thickness", 0.0025},
                                   {"inward", true},
                                   {"remove_faces", QCborArray{face1}},
                                   {"overrides", QCborArray{}}});
    roundtrip(shellInward);

    // CM-204: Outward shell
    QCborMap shellOutward = fields({{"version", 1},
                                    {"id", newId()},
                                    {"kind", "Shell"},
                                    {"target", tgt},
                                    {"thickness", 0.004},
                                    {"inward", false},
                                    {"remove_faces", QCborArray{face1}},
                                    {"overrides", QCborArray{}}});
    roundtrip(shellOutward);

    // CM-205: Multiple thickness overrides
    QCborMap shellMulti =
        fields({{"version", 1},
                {"id", newId()},
                {"kind", "Shell"},
                {"target", tgt},
                {"thickness", 0.002},
                {"inward", true},
                {"remove_faces", QCborArray{face1}},
                {"overrides", QCborArray{fields({{"face", face2}, {"thickness", 0.005}}),
                                         fields({{"face", face3}, {"thickness", 0.008}})}}});
    roundtrip(shellMulti);

    // Negative tests
    // 1. Duplicate removed face
    auto dupRemove = shellOpen;
    dupRemove.insert(QStringLiteral("remove_faces"), QCborArray{face1, face1});
    error("InvalidInput", [&] { features::ModelingIntent::parse(dupRemove); });

    // 2. Duplicate override face
    auto dupOverride = shellMulti;
    dupOverride.insert(QStringLiteral("overrides"),
                       QCborArray{fields({{"face", face2}, {"thickness", 0.005}}),
                                  fields({{"face", face2}, {"thickness", 0.007}})});
    error("InvalidInput", [&] { features::ModelingIntent::parse(dupOverride); });

    // 3. Face both removed and overridden
    auto conflictFace = shellMulti;
    conflictFace.insert(QStringLiteral("overrides"),
                        QCborArray{fields({{"face", face1}, {"thickness", 0.005}})});
    error("InvalidInput", [&] { features::ModelingIntent::parse(conflictFace); });

    // 4. Face belonging to different object
    auto otherSrc = newId();
    auto wrongFace = reference(otherSrc, "Face", {}, "face/" + QString(64, '9'));
    auto wrongStage = shellOpen;
    wrongStage.insert(QStringLiteral("remove_faces"), QCborArray{wrongFace});
    error("InvalidInput", [&] { features::ModelingIntent::parse(wrongStage); });

    // 5. Non-positive thickness
    auto zeroThick = shellOpen;
    zeroThick.insert(QStringLiteral("thickness"), 0.0);
    error("InvalidInput", [&] { features::ModelingIntent::parse(zeroThick); });

    auto negThick = shellOpen;
    negThick.insert(QStringLiteral("thickness"), -0.002);
    error("InvalidInput", [&] { features::ModelingIntent::parse(negThick); });

    // 6. Extraneous keys
    auto extra = shellOpen;
    extra.insert(QStringLiteral("extra_field"), 123);
    error("InvalidInput", [&] { features::ModelingIntent::parse(extra); });
}

void testSweepContracts() {
    auto skProf = newId();
    auto skPath = newId();
    auto srcBody = newId();
    auto tgt = body(srcBody);
    auto guide1 = reference(newId(), "Curve");
    auto guide2 = reference(newId(), "Curve");

    // CM-206: Sketch profile sweep
    QCborMap sweepSketch = fields({{"version", 1},
                                   {"id", newId()},
                                   {"kind", "Sweep"},
                                   {"operation", "NewBody"},
                                   {"target", QCborValue(nullptr)},
                                   {"profile", sketchProfile(skProf)},
                                   {"path", path(skPath)},
                                   {"guides", QCborArray{}},
                                   {"corner_transition", "Round"},
                                   {"start_tangency", "None"},
                                   {"end_tangency", "None"},
                                   {"thin", fields({{"mode", "Solid"}})},
                                   {"preview_sections", 0}});
    roundtrip(sweepSketch);

    // CM-207: Circular profile sweep
    QCborMap sweepCircular = fields({{"version", 1},
                                     {"id", newId()},
                                     {"kind", "Sweep"},
                                     {"operation", "NewBody"},
                                     {"target", QCborValue(nullptr)},
                                     {"profile", circularProfile(0.015)},
                                     {"path", path(skPath)},
                                     {"guides", QCborArray{}},
                                     {"corner_transition", "Round"},
                                     {"start_tangency", "None"},
                                     {"end_tangency", "None"},
                                     {"thin", fields({{"mode", "Solid"}})},
                                     {"preview_sections", 0}});
    roundtrip(sweepCircular);

    // CM-208: Open path sweep
    QCborMap sweepOpenPath = sweepSketch;
    sweepOpenPath.insert(QStringLiteral("path"), path(skPath, "Curve", {}, false));
    roundtrip(sweepOpenPath);

    // CM-209: Closed path sweep
    QCborMap sweepClosedPath = sweepSketch;
    sweepClosedPath.insert(QStringLiteral("path"), path(skPath, "Curve", {}, false));
    roundtrip(sweepClosedPath);

    // CM-210: 3D path sweep (reference to SpatialSketch entity or DerivedCurve)
    auto derivedCurve = newId();
    QCborMap sweep3DPath = sweepSketch;
    sweep3DPath.insert(QStringLiteral("path"), path(derivedCurve, "Curve", {}, false));
    roundtrip(sweep3DPath);

    // CM-211: One-sided thin sweep
    QCborMap sweepThin1 = sweepSketch;
    sweepThin1.insert(QStringLiteral("thin"),
                      fields({{"mode", "OneDirection"}, {"thickness", 0.002}, {"reverse", false}}));
    roundtrip(sweepThin1);

    // CM-212: Midplane thin sweep
    QCborMap sweepThinMid = sweepSketch;
    sweepThinMid.insert(QStringLiteral("thin"),
                        fields({{"mode", "Midplane"}, {"thickness", 0.003}}));
    roundtrip(sweepThinMid);

    // CM-213: Two-sided thin sweep
    QCborMap sweepThin2 = sweepSketch;
    sweepThin2.insert(
        QStringLiteral("thin"),
        fields({{"mode", "TwoDirection"}, {"thickness1", 0.001}, {"thickness2", 0.002}}));
    roundtrip(sweepThin2);

    // CM-214: Reverse thickness sweep
    QCborMap sweepThinRev = sweepSketch;
    sweepThinRev.insert(
        QStringLiteral("thin"),
        fields({{"mode", "OneDirection"}, {"thickness", 0.002}, {"reverse", true}}));
    roundtrip(sweepThinRev);

    // CM-215: Guide curves sweep
    QCborMap sweepGuided = sweepSketch;
    sweepGuided.insert(QStringLiteral("guides"), QCborArray{guide1, guide2});
    roundtrip(sweepGuided);

    // CM-216: Start/end tangency sweep
    QCborMap sweepTangent = sweepSketch;
    sweepTangent.insert(QStringLiteral("start_tangency"), QStringLiteral("PathTangent"));
    sweepTangent.insert(QStringLiteral("end_tangency"), QStringLiteral("PathTangent"));
    roundtrip(sweepTangent);

    // CM-217: Corner transition (Sharp)
    QCborMap sweepSharp = sweepSketch;
    sweepSharp.insert(QStringLiteral("corner_transition"), QStringLiteral("Sharp"));
    roundtrip(sweepSharp);

    // CM-218: Preview sections
    QCborMap sweepPreview = sweepSketch;
    sweepPreview.insert(QStringLiteral("preview_sections"), 5);
    roundtrip(sweepPreview);

    // Negative tests
    // 1. Guide duplicating path
    auto dupGuidePath = sweepSketch;
    dupGuidePath.insert(QStringLiteral("guides"), QCborArray{reference(skPath, "Curve")});
    error("InvalidInput", [&] { features::ModelingIntent::parse(dupGuidePath); });

    // 2. NewBody with target
    auto invalidTarget = sweepSketch;
    invalidTarget.insert(QStringLiteral("target"), tgt);
    error("InvalidInput", [&] { features::ModelingIntent::parse(invalidTarget); });

    // 3. Add without target
    auto addNoTarget = sweepSketch;
    addNoTarget.insert(QStringLiteral("operation"), QStringLiteral("Add"));
    addNoTarget.insert(QStringLiteral("target"), QCborValue(nullptr));
    error("InvalidInput", [&] { features::ModelingIntent::parse(addNoTarget); });

    // 4. Invalid corner transition
    auto badCorner = sweepSketch;
    badCorner.insert(QStringLiteral("corner_transition"), QStringLiteral("Bevel"));
    error("InvalidInput", [&] { features::ModelingIntent::parse(badCorner); });

    // 5. Invalid tangency
    auto badTangency = sweepSketch;
    badTangency.insert(QStringLiteral("start_tangency"), QStringLiteral("Curvature"));
    error("InvalidInput", [&] { features::ModelingIntent::parse(badTangency); });

    // 6. Preview sections out of range
    auto badPreview = sweepSketch;
    badPreview.insert(QStringLiteral("preview_sections"), 50);
    error("InvalidInput", [&] { features::ModelingIntent::parse(badPreview); });
}

void testLoftContracts() {
    auto sk1 = newId();
    auto sk2 = newId();
    auto sk3 = newId();
    auto sec1 = sketchProfile(sk1);
    auto sec2 = sketchProfile(sk2);
    auto sec3 = sketchProfile(sk3);
    auto guide1 = reference(newId(), "Curve");
    auto cl = path(newId());

    auto conn1 = fields({{"id", newId()},
                         {"points", QCborArray{fields({{"section", 0}, {"parameter", 0.0}}),
                                               fields({{"section", 1}, {"parameter", 0.0}})}}});
    auto conn2 = fields({{"id", newId()},
                         {"points", QCborArray{fields({{"section", 0}, {"parameter", 0.5}}),
                                               fields({{"section", 1}, {"parameter", 0.5}})}}});

    // CM-219: Ordered sections solid loft
    QCborMap loftSolid = fields({{"version", 1},
                                 {"id", newId()},
                                 {"kind", "Loft"},
                                 {"output", "Solid"},
                                 {"operation", "NewBody"},
                                 {"target", QCborValue(nullptr)},
                                 {"sections", QCborArray{sec1, sec2, sec3}},
                                 {"centerline", QCborValue(nullptr)},
                                 {"guides", QCborArray{}},
                                 {"connectors", QCborArray{}},
                                 {"start_condition", "None"},
                                 {"end_condition", "None"},
                                 {"thin", fields({{"mode", "Solid"}})}});
    roundtrip(loftSolid);

    // CM-220: Ellipse sections loft
    QCborMap loftEllipse = loftSolid;
    roundtrip(loftEllipse);

    // CM-221: Open sections surface loft
    auto open1 =
        fields({{"sketch", sk1},
                {"mode", "OpenChains"},
                {"regions",
                 QCborArray{QCborArray{QCborArray{fields(
                     {{"entity", newId()}, {"from", 0.0}, {"to", 1.0}, {"reverse", false}})}}}}});
    auto open2 =
        fields({{"sketch", sk2},
                {"mode", "OpenChains"},
                {"regions",
                 QCborArray{QCborArray{QCborArray{fields(
                     {{"entity", newId()}, {"from", 0.0}, {"to", 1.0}, {"reverse", false}})}}}}});
    QCborMap loftSurface = fields({{"version", 1},
                                   {"id", newId()},
                                   {"kind", "Loft"},
                                   {"output", "Surface"},
                                   {"operation", "NewBody"},
                                   {"target", QCborValue(nullptr)},
                                   {"sections", QCborArray{open1, open2}},
                                   {"centerline", QCborValue(nullptr)},
                                   {"guides", QCborArray{}},
                                   {"connectors", QCborArray{}},
                                   {"start_condition", "None"},
                                   {"end_condition", "None"},
                                   {"thin", fields({{"mode", "Solid"}})}});
    roundtrip(loftSurface);

    // CM-222: Closed sections solid loft
    QCborMap loftClosed = loftSolid;
    roundtrip(loftClosed);

    // CM-223: Thin loft modes
    QCborMap loftThin1 = loftSolid;
    loftThin1.insert(QStringLiteral("thin"),
                     fields({{"mode", "OneDirection"}, {"thickness", 0.002}, {"reverse", false}}));
    roundtrip(loftThin1);

    QCborMap loftThinMid = loftSolid;
    loftThinMid.insert(QStringLiteral("thin"),
                       fields({{"mode", "Midplane"}, {"thickness", 0.003}}));
    roundtrip(loftThinMid);

    QCborMap loftThin2 = loftSolid;
    loftThin2.insert(
        QStringLiteral("thin"),
        fields({{"mode", "TwoDirection"}, {"thickness1", 0.001}, {"thickness2", 0.002}}));
    roundtrip(loftThin2);

    // CM-224: Centerline loft
    QCborMap loftCenterline = loftSolid;
    loftCenterline.insert(QStringLiteral("centerline"), cl);
    roundtrip(loftCenterline);

    // CM-225..227: Connectors and synchronization
    QCborMap loftConnectors = fields({{"version", 1},
                                      {"id", newId()},
                                      {"kind", "Loft"},
                                      {"output", "Solid"},
                                      {"operation", "NewBody"},
                                      {"target", QCborValue(nullptr)},
                                      {"sections", QCborArray{sec1, sec2}},
                                      {"centerline", QCborValue(nullptr)},
                                      {"guides", QCborArray{guide1}},
                                      {"connectors", QCborArray{conn1, conn2}},
                                      {"start_condition", "NormalToSection"},
                                      {"end_condition", "NormalToSection"},
                                      {"thin", fields({{"mode", "Solid"}})}});
    roundtrip(loftConnectors);

    // Negative tests
    // 1. Solid loft with open sections
    auto openSolid = loftSolid;
    openSolid.insert(QStringLiteral("sections"), QCborArray{open1, open2});
    error("InvalidInput", [&] { features::ModelingIntent::parse(openSolid); });

    // 2. Surface loft with thin thickness
    auto thinSurface = loftSurface;
    thinSurface.insert(
        QStringLiteral("thin"),
        fields({{"mode", "OneDirection"}, {"thickness", 0.002}, {"reverse", false}}));
    error("InvalidInput", [&] { features::ModelingIntent::parse(thinSurface); });

    // 3. Surface loft with Cut operation
    auto cutSurface = loftSurface;
    cutSurface.insert(QStringLiteral("operation"), QStringLiteral("Cut"));
    error("InvalidInput", [&] { features::ModelingIntent::parse(cutSurface); });

    // 4. Mixed open and closed sections in surface loft
    auto mixedSections = loftSurface;
    mixedSections.insert(QStringLiteral("sections"), QCborArray{open1, sec2});
    error("InvalidInput", [&] { features::ModelingIntent::parse(mixedSections); });

    // 5. Single section loft
    auto singleSection = loftSolid;
    singleSection.insert(QStringLiteral("sections"), QCborArray{sec1});
    error("ResourceLimit", [&] { features::ModelingIntent::parse(singleSection); });

    // 6. Crossing connectors
    auto crossConn1 =
        fields({{"id", newId()},
                {"points", QCborArray{fields({{"section", 0}, {"parameter", 0.2}}),
                                      fields({{"section", 1}, {"parameter", 0.8}})}}});
    auto crossConn2 =
        fields({{"id", newId()},
                {"points", QCborArray{fields({{"section", 0}, {"parameter", 0.6}}),
                                      fields({{"section", 1}, {"parameter", 0.3}})}}});
    auto crossingLoft = loftSolid;
    crossingLoft.insert(QStringLiteral("sections"), QCborArray{sec1, sec2});
    crossingLoft.insert(QStringLiteral("connectors"), QCborArray{crossConn1, crossConn2});
    error("InvalidInput", [&] { features::ModelingIntent::parse(crossingLoft); });

    // 7. Duplicate connector ID
    auto dupConnId = crossConn1;
    auto dupLoft = loftSolid;
    dupLoft.insert(QStringLiteral("sections"), QCborArray{sec1, sec2});
    dupLoft.insert(QStringLiteral("connectors"), QCborArray{dupConnId, dupConnId});
    error("InvalidInput", [&] { features::ModelingIntent::parse(dupLoft); });

    // 8. Connector missing points for all sections
    auto shortConn = fields(
        {{"id", newId()}, {"points", QCborArray{fields({{"section", 0}, {"parameter", 0.2}})}}});
    auto shortLoft = loftSolid;
    shortLoft.insert(QStringLiteral("sections"), QCborArray{sec1, sec2});
    shortLoft.insert(QStringLiteral("connectors"), QCborArray{shortConn});
    error("InvalidInput", [&] { features::ModelingIntent::parse(shortLoft); });
}

void testDependencyGraph() {
    auto baseSk = newId();
    auto sweepSk = newId();
    auto pathSk = newId();
    evaluation::GraphNode nBase{baseSk, "Sketch", {}, {}};
    evaluation::GraphNode nSweep{sweepSk, "Sketch", {}, {}};
    evaluation::GraphNode nPath{pathSk, "Sketch", {}, {}};

    auto sweepIntent = fields({{"version", 1},
                               {"id", newId()},
                               {"kind", "Sweep"},
                               {"operation", "NewBody"},
                               {"target", QCborValue(nullptr)},
                               {"profile", sketchProfile(sweepSk)},
                               {"path", path(pathSk)},
                               {"guides", QCborArray{}},
                               {"corner_transition", "Round"},
                               {"start_tangency", "None"},
                               {"end_tangency", "None"},
                               {"thin", fields({{"mode", "Solid"}})},
                               {"preview_sections", 0}});
    auto parsedSweep = features::ModelingIntent::parse(sweepIntent);

    // Valid dependency order
    QStringList history{baseSk, sweepSk, pathSk, parsedSweep.graphNode().id};
    auto g = parsedSweep.graph({nBase, nSweep, nPath}, history);
    CHECK(g.dependencies(parsedSweep.graphNode().id).contains(sweepSk));
    CHECK(g.dependencies(parsedSweep.graphNode().id).contains(pathSk));

    // History order violation: sweep placed before its path
    QStringList badHistory{baseSk, sweepSk, parsedSweep.graphNode().id, pathSk};
    error("DependencyOrder", [&] { parsedSweep.graph({nBase, nSweep, nPath}, badHistory); });

    // Missing source node
    QStringList incompleteHistory{baseSk, sweepSk, parsedSweep.graphNode().id};
    error("ReferenceMissing", [&] { parsedSweep.graph({nBase, nSweep}, incompleteHistory); });
}
} // namespace

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    try {
        printf("1. Shell\n");
        testShellContracts();
        printf("2. Sweep\n");
        testSweepContracts();
        printf("3. Loft\n");
        testLoftContracts();
        printf("4. Graph\n");
        testDependencyGraph();
        printf("All cm8 contracts passed!\n");
    } catch (const std::exception &e) {
        fprintf(stderr, "Exception: %s\n", e.what());
        return 1;
    }
    return 0;
}
