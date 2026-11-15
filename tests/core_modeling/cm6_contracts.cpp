#include "features/Model.hpp"
#include "features/ModelingIntent.hpp"
#include "features/ModelingRecords.hpp"
#include "sketch/Sketch.hpp"
#include "support/check.hpp"
#include <QCoreApplication>
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
    }
    if (code != expected)
        throw std::runtime_error("Expected " + expected.toStdString() + ", got " +
                                 code.toStdString() + ": " + message.toStdString());
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

void roundtrip(const QCborMap &m) {
    auto intent = features::ModelingIntent::parse(m);
    CHECK(intent.map() == m);
    CHECK(features::ModelingIntent::parse(QCborValue::fromCbor(QCborValue(m).toCbor()).toMap())
              .map() == m);
}

void holeContracts() {
    auto src = newId(), sk = newId(), pt1 = newId(), pt2 = newId();
    auto tgt = body(src);

    // Simple hole through all
    QCborMap simpleHole = fields(
        {{"version", 1},
         {"id", newId()},
         {"kind", "Hole"},
         {"operation", "Cut"},
         {"target", tgt},
         {"family", "Simple"},
         {"placement", fields({{"face", reference(src, "Face")},
                               {"sketch", sk},
                               {"points", QCborArray{pt1, pt2}}})},
         {"sizing", fields({{"mode", "Custom"}, {"diameter", 0.010}, {"depth", 0.025}})},
         {"termination", fields({{"mode", "ThroughAll"}, {"depth_reference", "Shoulder"}})}});
    roundtrip(simpleHole);

    // Counterbore hole matching Tutorial 1 dimensions (8.5 / 14 / 4.2 mm)
    QCborMap cboreHole = fields(
        {{"version", 1},
         {"id", newId()},
         {"kind", "Hole"},
         {"operation", "Cut"},
         {"target", tgt},
         {"family", "Counterbore"},
         {"placement",
          fields({{"face", reference(src, "Face")}, {"sketch", sk}, {"points", QCborArray{pt1}}})},
         {"sizing", fields({{"mode", "Catalog"},
                            {"diameter", 0.0085},
                            {"depth", 0.030},
                            {"counterbore_diameter", 0.014},
                            {"counterbore_depth", 0.0042},
                            {"dataset", "ansi_metric"},
                            {"standard", "B18.3.4M"},
                            {"type", "SocketButtonHeadCapScrew"},
                            {"size", "M8"},
                            {"fit", "Normal"}})},
         {"termination", fields({{"mode", "ThroughAll"}, {"depth_reference", "Shoulder"}})}});
    roundtrip(cboreHole);

    // Countersink hole
    QCborMap csinkHole = fields(
        {{"version", 1},
         {"id", newId()},
         {"kind", "Hole"},
         {"operation", "Cut"},
         {"target", tgt},
         {"family", "Countersink"},
         {"placement",
          fields({{"face", reference(src, "Face")}, {"sketch", sk}, {"points", QCborArray{pt1}}})},
         {"sizing", fields({{"mode", "Custom"},
                            {"diameter", 0.006},
                            {"depth", 0.020},
                            {"countersink_diameter", 0.012},
                            {"countersink_angle", 90.0 * std::numbers::pi / 180.0}})},
         {"termination", fields({{"mode", "Blind"}, {"depth_reference", "Shoulder"}})}});
    roundtrip(csinkHole);

    // Drilled hole with 118 degree tip
    QCborMap drillHole = fields(
        {{"version", 1},
         {"id", newId()},
         {"kind", "Hole"},
         {"operation", "Cut"},
         {"target", tgt},
         {"family", "Drilled"},
         {"placement",
          fields({{"face", reference(src, "Face")}, {"sketch", sk}, {"points", QCborArray{pt1}}})},
         {"sizing", fields({{"mode", "Custom"},
                            {"diameter", 0.008},
                            {"depth", 0.020},
                            {"drill_angle", 118.0 * std::numbers::pi / 180.0}})},
         {"termination", fields({{"mode", "Blind"}, {"depth_reference", "Tip"}})}});
    roundtrip(drillHole);

    // Tapped hole with thread metadata
    QCborMap tapHole = fields(
        {{"version", 1},
         {"id", newId()},
         {"kind", "Hole"},
         {"operation", "Cut"},
         {"target", tgt},
         {"family", "Tapped"},
         {"placement",
          fields({{"face", reference(src, "Face")}, {"sketch", sk}, {"points", QCborArray{pt1}}})},
         {"sizing", fields({{"mode", "Custom"},
                            {"diameter", 0.010},
                            {"depth", 0.025},
                            {"tap_drill_diameter", 0.0085},
                            {"thread", fields({{"cosmetic", true},
                                               {"designation", "M10x1.5"},
                                               {"pitch", 0.0015},
                                               {"depth", 0.020}})}})},
         {"termination", fields({{"mode", "Blind"}, {"depth_reference", "Shoulder"}})}});
    roundtrip(tapHole);

    // OffsetFromSurface termination
    QCborMap offsetHole = simpleHole;
    offsetHole.insert(QStringLiteral("termination"), fields({{"mode", "OffsetFromSurface"},
                                                             {"depth_reference", "Shoulder"},
                                                             {"reference", reference(src, "Face")},
                                                             {"offset", 0.005},
                                                             {"offset_reverse", false}}));
    roundtrip(offsetHole);

    // Rejections
    // Invalid operation
    auto badOp = simpleHole;
    badOp.insert(QStringLiteral("operation"), "Add");
    error("UnsupportedOperation", [&] { features::ModelingIntent::parse(badOp); });

    // Counterbore diameter smaller than hole diameter
    auto badCbore = cboreHole;
    auto cbSz = badCbore.value("sizing").toMap();
    cbSz.insert(QStringLiteral("counterbore_diameter"), 0.005);
    badCbore.insert(QStringLiteral("sizing"), cbSz);
    error("InvalidInput", [&] { features::ModelingIntent::parse(badCbore); });

    // Excessive placement points (>64)
    auto badPts = simpleHole;
    QCborArray excessive;
    for (int i = 0; i < 65; ++i)
        excessive.append(newId());
    auto plMap = badPts.value("placement").toMap();
    plMap.insert(QStringLiteral("points"), excessive);
    badPts.insert(QStringLiteral("placement"), plMap);
    error("ResourceLimit", [&] { features::ModelingIntent::parse(badPts); });

    // Duplicate placement point
    auto dupPt = simpleHole;
    auto plDup = dupPt.value("placement").toMap();
    plDup.insert(QStringLiteral("points"), QCborArray{pt1, pt1});
    dupPt.insert(QStringLiteral("placement"), plDup);
    error("InvalidInput", [&] { features::ModelingIntent::parse(dupPt); });
}

void cosmeticThreadContracts() {
    auto src = newId();
    auto tgt = body(src);

    QCborMap thread = fields({{"version", 1},
                              {"id", newId()},
                              {"kind", "CosmeticThread"},
                              {"target", tgt},
                              {"reference", reference(src, "Edge")},
                              {"designation", "M42x3.0"},
                              {"standard", "ANSI Metric"},
                              {"type", "Machine Thread"},
                              {"size", "M42x3.0"},
                              {"thread_type", "Internal"},
                              {"major_diameter", 0.042},
                              {"minor_diameter", 0.03875},
                              {"pitch", 0.003},
                              {"termination", fields({{"mode", "UpToNext"}})}});
    roundtrip(thread);

    // Blind depth
    auto blindThread = thread;
    blindThread.insert(QStringLiteral("termination"),
                       fields({{"mode", "Blind"}, {"depth", 0.020}}));
    roundtrip(blindThread);

    // Rejections: minor > major
    auto badDiam = thread;
    badDiam.insert(QStringLiteral("minor_diameter"), 0.050);
    error("InvalidInput", [&] { features::ModelingIntent::parse(badDiam); });

    // Invalid thread type
    auto badType = thread;
    badType.insert(QStringLiteral("thread_type"), "Unknown");
    error("InvalidInput", [&] { features::ModelingIntent::parse(badType); });
}

void ribContracts() {
    auto src = newId(), sk = newId();
    auto tgt = body(src);

    QCborMap rib = fields(
        {{"version", 1},
         {"id", newId()},
         {"kind", "Rib"},
         {"operation", "Add"},
         {"target", tgt},
         {"profile",
          fields(
              {{"sketch", sk},
               {"mode", "SelectedRegions"},
               {"regions",
                QCborArray{QCborArray{QCborArray{fields(
                    {{"entity", newId()}, {"from", 0.0}, {"to", 1.0}, {"reverse", false}})}}}}})},
         {"thickness", 0.013},
         {"side", "Both"},
         {"direction", "Parallel"},
         {"flip_material", false}});
    roundtrip(rib);

    // Rejections: side toggle
    for (QString side : {"First", "Second", "Both"}) {
        auto r = rib;
        r.insert(QStringLiteral("side"), side);
        roundtrip(r);
    }
    for (QString dir : {"Parallel", "Normal"}) {
        auto r = rib;
        r.insert(QStringLiteral("direction"), dir);
        roundtrip(r);
    }

    // Invalid operation (Cut not supported for rib)
    auto badRib = rib;
    badRib.insert(QStringLiteral("operation"), "Cut");
    error("UnsupportedOperation", [&] { features::ModelingIntent::parse(badRib); });

    // Negative thickness
    auto negThick = rib;
    negThick.insert(QStringLiteral("thickness"), -0.005);
    error("InvalidInput", [&] { features::ModelingIntent::parse(negThick); });
}

void repetitionContracts() {
    auto src = newId(), feat = newId();
    auto tgt = body(src);

    // Linear pattern 2D (5 instances, spacing 43 mm)
    QCborMap linearPattern = fields({{"version", 1},
                                     {"id", newId()},
                                     {"kind", "LinearPattern"},
                                     {"operation", "Cut"},
                                     {"target", tgt},
                                     {"seed_kind", "Feature"},
                                     {"seed", feat},
                                     {"direction1", fields({{"reference", reference(src, "Edge")},
                                                            {"spacing", 0.043},
                                                            {"count", 5},
                                                            {"reverse", false}})},
                                     {"direction2", fields({{"reference", reference(src, "Edge")},
                                                            {"spacing", 0.030},
                                                            {"count", 2},
                                                            {"reverse", true}})}});
    roundtrip(linearPattern);

    // Circular pattern
    QCborMap circularPattern = fields({{"version", 1},
                                       {"id", newId()},
                                       {"kind", "CircularPattern"},
                                       {"operation", "Add"},
                                       {"target", tgt},
                                       {"seed_kind", "Feature"},
                                       {"seed", feat},
                                       {"axis", reference(src, "Axis")},
                                       {"count", 6},
                                       {"equal_spacing", true},
                                       {"angle", 2 * std::numbers::pi},
                                       {"reverse", false}});
    roundtrip(circularPattern);

    // Mirror
    QCborMap mirror = fields({{"version", 1},
                              {"id", newId()},
                              {"kind", "Mirror"},
                              {"operation", "Cut"},
                              {"target", tgt},
                              {"seed_kind", "Feature"},
                              {"seed", feat},
                              {"plane", reference(src, "Plane")}});
    roundtrip(mirror);

    // Body seed mirror (NewBody)
    QCborMap bodyMirror = fields({{"version", 1},
                                  {"id", newId()},
                                  {"kind", "Mirror"},
                                  {"operation", "NewBody"},
                                  {"target", QCborValue(nullptr)},
                                  {"seed_kind", "Body"},
                                  {"seed", tgt},
                                  {"plane", reference(src, "Plane")}});
    roundtrip(bodyMirror);

    // Face seed mirror
    QCborMap faceMirror = mirror;
    faceMirror.insert(QStringLiteral("seed_kind"), "Face");
    faceMirror.insert(QStringLiteral("seed"), QCborArray{reference(src, "Face")});
    roundtrip(faceMirror);

    // Rejection: Total instances exceed 1024 (e.g. 33 x 33 = 1089)
    auto excessiveLinear = linearPattern;
    auto d1 = excessiveLinear.value("direction1").toMap();
    d1.insert(QStringLiteral("count"), 32);
    excessiveLinear.insert(QStringLiteral("direction1"), d1);
    auto d2 = excessiveLinear.value("direction2").toMap();
    d2.insert(QStringLiteral("count"), 33); // exceeds direction cap 32
    excessiveLinear.insert(QStringLiteral("direction2"), d2);
    error("InvalidInput", [&] { features::ModelingIntent::parse(excessiveLinear); });
}

void treatmentContracts() {
    auto src = newId();
    auto tgt = body(src);

    // Constant fillet
    QCborMap constFillet = fields({{"version", 1},
                                   {"id", newId()},
                                   {"kind", "Fillet"},
                                   {"target", tgt},
                                   {"method", "Constant"},
                                   {"propagate_tangent", true},
                                   {"radius", 0.005},
                                   {"edges", QCborArray{reference(src, "Edge")}}});
    roundtrip(constFillet);

    // Multiple radius fillet
    QCborMap multiFillet = fields(
        {{"version", 1},
         {"id", newId()},
         {"kind", "Fillet"},
         {"target", tgt},
         {"method", "MultipleRadius"},
         {"propagate_tangent", true},
         {"radii", QCborArray{fields({{"edge", reference(src, "Edge")}, {"radius", 0.003}}),
                              fields({{"edge", reference(src, "Edge")}, {"radius", 0.006}})}}});
    roundtrip(multiFillet);

    // Variable radius fillet
    QCborMap varFillet =
        fields({{"version", 1},
                {"id", newId()},
                {"kind", "Fillet"},
                {"target", tgt},
                {"method", "Variable"},
                {"propagate_tangent", true},
                {"edge", reference(src, "Edge")},
                {"control_points", QCborArray{fields({{"parameter", 0.0}, {"radius", 0.002}}),
                                              fields({{"parameter", 1.0}, {"radius", 0.008}})}},
                {"transition", "Smooth"}});
    roundtrip(varFillet);

    // Equal distance chamfer
    QCborMap eqChamfer = fields({{"version", 1},
                                 {"id", newId()},
                                 {"kind", "Chamfer"},
                                 {"target", tgt},
                                 {"method", "EqualDistance"},
                                 {"propagate_tangent", true},
                                 {"edges", QCborArray{reference(src, "Edge")}},
                                 {"distance", 0.0015}});
    roundtrip(eqChamfer);

    // Distance-distance chamfer
    QCborMap ddChamfer = fields({{"version", 1},
                                 {"id", newId()},
                                 {"kind", "Chamfer"},
                                 {"target", tgt},
                                 {"method", "DistanceDistance"},
                                 {"propagate_tangent", true},
                                 {"edges", QCborArray{reference(src, "Edge")}},
                                 {"reference_face", reference(src, "Face")},
                                 {"distance1", 0.002},
                                 {"distance2", 0.004},
                                 {"flip", false}});
    roundtrip(ddChamfer);

    // Angle-distance chamfer (Tutorial 3: 1.5 mm at 45 deg)
    QCborMap adChamfer = fields({{"version", 1},
                                 {"id", newId()},
                                 {"kind", "Chamfer"},
                                 {"target", tgt},
                                 {"method", "AngleDistance"},
                                 {"propagate_tangent", true},
                                 {"edges", QCborArray{reference(src, "Edge")}},
                                 {"reference_face", reference(src, "Face")},
                                 {"distance", 0.0015},
                                 {"angle", 45.0 * std::numbers::pi / 180.0},
                                 {"flip", false}});
    roundtrip(adChamfer);

    // Rejection: excessive edges (>32)
    auto excessiveEdges = constFillet;
    QCborArray edgeArr;
    for (int i = 0; i < 33; ++i)
        edgeArr.append(reference(src, "Edge"));
    excessiveEdges.insert(QStringLiteral("edges"), edgeArr);
    error("ResourceLimit", [&] { features::ModelingIntent::parse(excessiveEdges); });

    // Angle >= pi/2
    auto badAng = adChamfer;
    badAng.insert(QStringLiteral("angle"), std::numbers::pi / 2);
    error("InvalidInput", [&] { features::ModelingIntent::parse(badAng); });
}

void graphAndRepairContracts() {
    auto src = newId(), sk = newId(), pt = newId();
    auto tgt = body(src);

    QCborMap hole = fields(
        {{"version", 1},
         {"id", newId()},
         {"kind", "Hole"},
         {"operation", "Cut"},
         {"target", tgt},
         {"family", "Simple"},
         {"placement",
          fields({{"face", reference(src, "Face")}, {"sketch", sk}, {"points", QCborArray{pt}}})},
         {"sizing", fields({{"mode", "Custom"}, {"diameter", 0.010}, {"depth", 0.020}})},
         {"termination", fields({{"mode", "ThroughAll"}, {"depth_reference", "Shoulder"}})}});

    auto intent = features::ModelingIntent::parse(hole);
    evaluation::GraphNode srcNode{src, "Feature", {}, {}};
    evaluation::GraphNode skNode{sk, "Sketch", {}, {{pt, "Point"}}};
    auto graph = intent.graph({srcNode, skNode}, {src, sk, hole.value("id").toString()});
    CHECK(graph.order().size() == 3);

    // Reference replacement test
    auto originalFace = features::intentReference(hole, "placement/face");
    CHECK(originalFace.role == "Face");

    domain::GeometryReference newFace = originalFace;
    newFace.topology = "face/" + QString(64, 'b');
    auto replaced = features::replaceIntentReference(QCborArray{hole}, hole.value("id").toString(),
                                                     "placement/face", newFace);
    auto replacedFace = features::intentReference(replaced[0].toMap(), "placement/face");
    CHECK(replacedFace.topology == newFace.topology);
}
} // namespace

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    holeContracts();
    cosmeticThreadContracts();
    ribContracts();
    repetitionContracts();
    treatmentContracts();
    graphAndRepairContracts();
    puts("PASS");
    return 0;
}
