#include "persistence/ProjectStore.hpp"
#include "sketch/ArcGeometry.hpp"
#include "sketch/Constructions.hpp"
#include "sketch/Curves.hpp"
#include "sketch/LineEditing.hpp"
#include "sketch/Solver.hpp"
#include "support/check.hpp"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
using namespace cad;
using sketch::fields;
namespace {
struct Fixture {
    sketch::Sketch s;
    QString line, left, right;
    Fixture(double scale = 1) {
        line = s.line(s.point({0, 0}), s.point({.04 * scale, 0}));
        left =
            s.line(s.point({.01 * scale, -.01 * scale}), s.point({.01 * scale, .01 * scale}), true);
        right =
            s.line(s.point({.03 * scale, -.01 * scale}), s.point({.03 * scale, .01 * scale}), true);
    }
    QCborMap request(QString mode, double x = .02, QCborArray boundaries = {}) const {
        return fields({{"mode", mode},
                       {"source", line},
                       {"boundaries", boundaries},
                       {"pick_x", x},
                       {"pick_y", 0.}});
    }
};
sketch::Sketch candidate(const sketch::Sketch &s, QCborMap request) {
    auto result = sketch::previewLineEdit(s, request);
    auto out = sketch::Sketch::parse(result.value("candidate").toMap());
    CHECK(sketch::solve(out).valid);
    CHECK(out.map().value("version").toInteger() == 5);
    return out;
}
void analytical() {
    for (double scale : {.001, 1., 1000.}) {
        Fixture f(scale);
        auto before = f.s.map();
        for (auto mode : sketch::lineEditModes()) {
            if (mode.contains("extend") || mode.contains("Extend"))
                continue;
            auto out = candidate(f.s, f.request(mode, .02 * scale, {f.left, f.right}));
            auto record = out.edits.last().toMap();
            auto children = record.value("descendants").toArray();
            CHECK(children.size() == (mode == "Trim away outside" ? 1 : 2));
            double total = 0;
            for (auto id : children) {
                auto &e = out.entity(id.toString());
                auto p = out.entity(e.a).point, q = out.entity(e.b).point;
                total += sketch::length(p, q);
                near(p.y, 0, 1e-12);
                near(q.y, 0, 1e-12);
            }
            near(total, .02 * scale, 1e-9 * scale);
            if (children.size() == 2)
                rejects([&] { out.entity(f.line); });
            else
                CHECK(children[0].toString() == f.line);
            CHECK(out.entity(f.left).a == f.s.entity(f.left).a);
            CHECK(out.entity(f.right).b == f.s.entity(f.right).b);
            CHECK(f.s.map() == before);
            auto wrongVersion = out.map();
            wrongVersion.insert(QStringLiteral("version"), 4);
            rejects([&] { sketch::Sketch::parse(wrongVersion); });
        }
        auto end = candidate(f.s, f.request("Trim to closest", .035 * scale));
        near(end.entity(end.entity(f.line).b).point.x, .03 * scale, 1e-10 * scale);
        CHECK(end.entity(f.s.entity(f.line).b).point.x == .04 * scale);
        rejects([&] { candidate(f.s, f.request("Trim to closest", .01 * scale)); });
        rejects([&] { candidate(end, f.request("Dynamic extend", .029 * scale, {f.line})); });
        // Independently rotate/translate every operand: no axis-aligned shortcut.
        auto rotated = f.s;
        double angle = .63, cos = std::cos(angle), sin = std::sin(angle);
        for (auto &e : rotated.entities)
            if (e.kind == "Point") {
                auto p = e.point;
                e.point = {(.007 * scale + p.x * cos - p.y * sin),
                           (-.003 * scale + p.x * sin + p.y * cos)};
            }
        auto req = f.request("Trim away inside", .02 * scale, {f.left, f.right});
        req.insert(QStringLiteral("pick_x"), .007 * scale + .02 * scale * cos);
        req.insert(QStringLiteral("pick_y"), -.003 * scale + .02 * scale * sin);
        auto split = candidate(rotated, req);
        for (auto id : split.edits.last().toMap().value("descendants").toArray()) {
            auto &e = split.entity(id.toString());
            near(sketch::length(split.entity(e.a).point, split.entity(e.b).point), .01 * scale,
                 1e-9 * scale);
        }
    }
}
void extensionAndBoundaries() {
    for (double scale : {.001, 1., 1000.}) {
        Fixture f(scale);
        auto end = candidate(f.s, f.request("Trim to closest", .035 * scale));
        auto far = end.line(end.point({.06 * scale, .01 * scale}),
                            end.point({.06 * scale, .02 * scale}), true);
        auto req = f.request("Extend to apparent intersection", .029 * scale, {far});
        auto out = candidate(end, req);
        near(out.entity(out.entity(f.line).b).point.x, .06 * scale, 1e-10 * scale);
        req.insert(QStringLiteral("mode"), "Dynamic extend");
        rejects([&] { candidate(end, req); });
        auto finite = end.line(end.point({.05 * scale, -.01 * scale}),
                               end.point({.05 * scale, .01 * scale}), true);
        out = candidate(end, f.request("Dynamic extend", .029 * scale, {finite}));
        near(out.entity(out.entity(f.line).b).point.x, .05 * scale, 1e-10 * scale);
        auto negative = end.line(end.point({-.02 * scale, -.01 * scale}),
                                 end.point({-.02 * scale, .01 * scale}), true);
        out = candidate(end, f.request("Dynamic extend", .001 * scale, {negative}));
        near(out.entity(out.entity(f.line).a).point.x, -.02 * scale, 1e-10 * scale);
        auto circle = f.s.circle(f.s.point({.02 * scale, 0}), .005 * scale, true);
        out = candidate(f.s, f.request("Trim to closest", .02 * scale, {circle}));
        auto children = out.edits.last().toMap().value("descendants").toArray();
        near(out.entity(out.entity(children[0].toString()).b).point.x, .015 * scale, 1e-10 * scale);
        near(out.entity(out.entity(children[1].toString()).a).point.x, .025 * scale, 1e-10 * scale);
        auto &arc = f.s.entity(circle);
        arc.kind = "Arc";
        arc.start = 0;
        arc.sweep = 3.141592653589793;
        out = candidate(f.s, f.request("Trim to closest", .02 * scale, {circle}));
        CHECK(out.edits.last().toMap().value("descendants").toArray().size() == 2);
    }
    Fixture f;
    auto overlap = f.s.line(f.s.point({.01, 0}), f.s.point({.03, 0}), true);
    rejects([&] { candidate(f.s, f.request("Trim to closest", .02, {overlap})); });
    auto request = f.request("Trim away inside", .02, {f.left, f.left});
    rejects([&] { candidate(f.s, request); });
    rejects([&] { candidate(f.s, f.request("Dynamic extend", .02)); });
    rejects([&] { candidate(f.s, f.request("Trim away inside", .02, {f.left})); });
    rejects([&] {
        candidate(f.s, f.request("Extend to apparent intersection", .01, {f.left, f.right}));
    });
    auto g = sketch::createRectangle(f.s, "Corner", {{.1, .1}, {.2, .2}});
    (void)g;
    request = f.request("Trim to closest");
    request.insert(QStringLiteral("source"), f.s.groups[0].edges[0]);
    rejects([&] { candidate(f.s, request); });
    auto malformed = f.s;
    malformed.edits = QCborArray{fields({{"id", newId()}})};
    rejects([&] { malformed.validate(); });
}
void constraints() {
    Fixture f;
    sketch::Constraint h;
    h.kind = "Horizontal";
    h.a = f.line;
    sketch::Constraint length;
    length.kind = "Length";
    length.a = f.line;
    length.expression = "Span";
    f.s.names["Span"] = "40 mm";
    f.s.constraints = {h, length};
    auto result = sketch::previewLineEdit(f.s, f.request("Trim to closest", .035));
    auto disclosure = result.value("affected_constraints").toArray();
    CHECK(disclosure.size() == 1 && disclosure[0].toMap().value("id").toString() == length.id);
    CHECK(disclosure[0].toMap().value("expression").toString() == "Span");
    auto out = sketch::Sketch::parse(result.value("candidate").toMap());
    CHECK(out.constraints.size() == 1 && out.constraints[0].id == h.id);
    CHECK(sketch::solve(out).valid);
    result = sketch::previewLineEdit(f.s, f.request("Trim to closest"));
    CHECK(result.value("affected_constraints").toArray().size() == 2);
    // A shared original endpoint stays put; fixed control on it is disclosed.
    auto shared = f.s.entity(f.line).b;
    auto neighbor = f.s.line(shared, f.s.point({.05, .01}));
    sketch::Constraint fixed;
    fixed.kind = "Fixed";
    fixed.a = shared;
    fixed.value = .04;
    f.s.constraints.push_back(fixed);
    result = sketch::previewLineEdit(f.s, f.request("Trim to closest", .035));
    CHECK(result.value("affected_constraints").toArray().size() == 2);
    out = sketch::Sketch::parse(result.value("candidate").toMap());
    CHECK(out.entity(neighbor).a == shared && out.entity(shared).point.x == .04);
    CHECK(out.entity(f.line).b != shared);
    // Cross-sketch Pierce never guesses a split descendant.
    sketch::Sketch target;
    target.frame = {{0, 0, 0}, {1, 0, 0}, {0, 0, 1}};
    sketch::Constraint pierce;
    pierce.kind = "Pierce";
    pierce.a = target.point({.02, 0});
    pierce.reference = sketch::CurveReference{"Self", f.s.id, f.line, "Line", 0, 1, .5};
    target.constraints.push_back(pierce);
    out = candidate(f.s, f.request("Trim to closest"));
    rejects([&] { sketch::evaluate(sketch::encode({out, target}), {}, true); });
    // Admission fails without mutating source.
    Fixture capped;
    for (int i = int(capped.s.entities.size()); i < 256; ++i)
        capped.s.point({.1 + i * .001, .1});
    auto before = capped.s.map();
    rejects([&] { candidate(capped.s, capped.request("Trim to closest")); });
    CHECK(capped.s.map() == before);
    auto history = out.edits[0].toMap();
    Fixture fullHistory;
    for (int i = 0; i < 64; ++i) {
        history.insert(QStringLiteral("id"), newId());
        fullHistory.s.edits.append(history);
    }
    before = fullHistory.s.map();
    rejects([&] { candidate(fullHistory.s, fullHistory.request("Trim to closest")); });
    CHECK(fullHistory.s.map() == before);
}
void mark(QString path) {
    QFile f(path + "/.cad-test-root");
    CHECK(f.open(QIODevice::WriteOnly));
    f.write("disposable\n");
}
QByteArray bytes(QString path) {
    QFile f(path);
    CHECK(f.open(QIODevice::ReadOnly));
    return f.readAll();
}
void persistence() {
    for (auto mode : QStringList{"Trim to closest", "Trim away inside", "Trim away outside"}) {
        QTemporaryDir root(QDir::tempPath() + "/cad-module3-cm2-edit-XXXXXX"),
            files(QDir::tempPath() + "/cad-module3-cm2-edit-files-XXXXXX");
        CHECK(root.isValid() && files.isValid());
        mark(root.path());
        mark(files.path());
        storage::ProjectStore store(root.path());
        auto project = newId(), doc = newId();
        store.create(project);
        auto command = [&](QString name, QCborMap p = QCborMap{}) {
            wire::Envelope e;
            e.context.project = project;
            e.context.document = doc;
            e.command = name;
            e.payload = p;
            e.revision = store.revision();
            return e;
        };
        store.execute(command("CreateDocument", fields({{"id", doc},
                                                        {"type", "Part"},
                                                        {"name", "Line editing"},
                                                        {"units", "mm"},
                                                        {"properties_entity", newId()}})));
        Fixture f;
        auto before = sketch::encode({f.s});
        store.execute(command("SetSketches", fields({{"sketches", before}})));
        auto old = files.filePath("legacy.cadproj");
        store.save(command("SaveProject"), old);
        auto original = bytes(old);
        auto out = candidate(f.s, f.request(mode, .02, {f.left, f.right}));
        auto commit = command("SetSketches", fields({{"sketches", sketch::encode({out})}}));
        store.execute(commit);
        auto accepted = store.sketches(doc);
        auto rev = store.revision();
        store.execute(commit);
        CHECK(store.revision() == rev);
        rejects([&] { store.save(command("SaveProject"), old); });
        CHECK(bytes(old) == original);
        auto saved = files.filePath("edited.cadproj");
        store.save(command("SaveProject"), saved);
        store.execute(command("Undo"));
        CHECK(store.sketches(doc) == before);
        auto redoOnly = files.filePath("redo-only.cadproj");
        store.save(command("SaveProject"), redoOnly);
        {
            storage::Database db(store.workingPath(), true);
            CHECK(db.scalar("SELECT manifest FROM project_meta")
                      .toString()
                      .endsWith(";sketch-edits=1"));
        }
        store.close();
        auto forged = files.filePath("forged.cadproj");
        CHECK(QFile::copy(redoOnly, forged));
        {
            storage::Database db(forged);
            auto m = db.scalar("SELECT manifest FROM project_meta").toString();
            db.run("UPDATE project_meta SET manifest=?", {m.section(";sketch-edits=1", 0, 0)});
        }
        rejects([&] { store.open(forged, "Saved"); });
        store.open(redoOnly, "Saved");
        store.execute(command("Redo"));
        CHECK(store.sketches(doc) == accepted);
        auto copy = files.filePath("copy.cadproj");
        store.save(
            command("PortableCopy", fields({{"new_project_id", newId()}, {"policy", "Copied"}})),
            copy);
        store.close();
        store.open(copy, "Saved");
        CHECK(store.sketches(doc) == accepted);
        auto bad = out.map();
        auto edits = bad.value("edits").toArray();
        edits.append(edits[0]);
        bad.insert(QStringLiteral("edits"), edits);
        rev = store.revision();
        rejects([&] {
            store.execute(command("SetSketches", fields({{"sketches", QCborArray{bad}}})));
        });
        CHECK(store.revision() == rev && store.sketches(doc) == accepted);
    }
}
void upstreamFailure() {
    QTemporaryDir root(QDir::tempPath() + "/cad-module3-cm2-upstream-XXXXXX");
    CHECK(root.isValid());
    mark(root.path());
    storage::ProjectStore store(root.path());
    auto project = newId(), doc = newId();
    store.create(project);
    auto command = [&](QString name, QCborMap p = QCborMap{}) {
        wire::Envelope e;
        e.context.project = project;
        e.context.document = doc;
        e.command = name;
        e.payload = p;
        e.revision = store.revision();
        return e;
    };
    store.execute(command("CreateDocument", fields({{"id", doc},
                                                    {"type", "Part"},
                                                    {"name", "Upstream trim"},
                                                    {"units", "mm"},
                                                    {"properties_entity", newId()}})));
    sketch::Sketch s;
    auto lines = s.rectangle({0, 0}, {.04, .02});
    auto boundary = s.line(s.point({.01, -.01}), s.point({.01, .01}), true);
    store.execute(command("SetSketches", fields({{"sketches", sketch::encode({s})}})));
    QCborArray features{fields({{"version", 1},
                                {"id", newId()},
                                {"name", "Block"},
                                {"kind", "Extrude"},
                                {"parameters", fields({{"sketch", s.id},
                                                       {"operation", "NewBody"},
                                                       {"target", ""},
                                                       {"depth", .01},
                                                       {"extent", "Forward"}})}})};
    store.execute(command("SetFeatures", fields({{"features", features}})));
    auto before = store.sketches(doc);
    auto revision = store.revision();
    auto volume = store.evaluatePart(doc).measure(revision).value("volume_m3").toDouble();
    near(volume, .04 * .02 * .01, 1e-12);
    auto out = candidate(s, fields({{"mode", "Trim to closest"},
                                    {"source", lines[0]},
                                    {"boundaries", QCborArray{boundary}},
                                    {"pick_x", .005},
                                    {"pick_y", 0.}}));
    rejects([&] {
        store.execute(command("SetSketches", fields({{"sketches", sketch::encode({out})}})));
    });
    CHECK(store.revision() == revision && store.sketches(doc) == before);
    near(store.evaluatePart(doc).measure(revision).value("volume_m3").toDouble(), volume, 1e-12);
}
void powerTrimAndCorner() {
    // 1. Power trim on a line between two boundaries
    Fixture f;
    auto ptReq = f.request("Power trim", .02, {f.left, f.right});
    auto ptOut = candidate(f.s, ptReq);
    auto ptRec = ptOut.edits.last().toMap();
    CHECK(ptRec.value("mode").toString() == "Power trim");
    CHECK(ptRec.value("descendants").toArray().size() == 2);

    // 2. Power trim on a circle cut by a line
    sketch::Sketch cs;
    auto circleId = cs.circle(cs.point({0, 0}), .02);
    auto cutLine = cs.line(cs.point({-.03, 0}), cs.point({.03, 0}), true);
    auto cTrimReq = fields({{"mode", "Power trim"},
                            {"source", circleId},
                            {"boundaries", QCborArray{cutLine}},
                            {"pick_x", 0.},
                            {"pick_y", .02}});
    auto cTrimOut = candidate(cs, cTrimReq);
    auto cDesc = cTrimOut.edits.last().toMap().value("descendants").toArray();
    CHECK(cDesc.size() == 1);
    CHECK(cTrimOut.entity(cDesc[0].toString()).kind == "Arc");

    // 2b. Power trim on circle cut by 2 crossing lines: pick quadrant 1 to trim away
    sketch::Sketch cs4;
    auto c4Id = cs4.circle(cs4.point({0, 0}), .02);
    auto cutX = cs4.line(cs4.point({-.03, 0}), cs4.point({.03, 0}), true);
    auto cutY = cs4.line(cs4.point({0, -.03}), cs4.point({0, .03}), true);
    auto c4Req = fields({{"mode", "Power trim"},
                         {"source", c4Id},
                         {"boundaries", QCborArray{cutX, cutY}},
                         {"pick_x", .015},
                         {"pick_y", .015}});
    auto c4Out = candidate(cs4, c4Req);
    auto c4Desc = c4Out.edits.last().toMap().value("descendants").toArray();
    CHECK(c4Desc.size() == 3); // 3 remaining quadrants retained

    // 3. Corner trim on intersecting lines
    sketch::Sketch sCorner;
    auto l1 = sCorner.line(sCorner.point({0, 0}), sCorner.point({.04, 0}));
    auto l2 = sCorner.line(sCorner.point({.02, -.02}), sCorner.point({.02, .04}));
    auto cornerReq = fields({{"mode", "Corner"},
                             {"source", l1},
                             {"boundaries", QCborArray{l2}},
                             {"pick_x", .01},
                             {"pick_y", 0.},
                             {"pick2_x", .02},
                             {"pick2_y", .03}});
    auto cornerOut = candidate(sCorner, cornerReq);
    auto cornerRec = cornerOut.edits.last().toMap();
    CHECK(cornerRec.value("mode").toString() == "Corner");
    CHECK(cornerOut.entity(l1).b == cornerOut.entity(l2).a);
    auto cornerVertex = cornerOut.entity(cornerOut.entity(l1).b).point;
    near(cornerVertex.x, .02, 1e-9);
    near(cornerVertex.y, 0., 1e-9);

    // 3b. Corner trim on Line and Arc
    sketch::Sketch sLineArc;
    auto lineA = sLineArc.line(sLineArc.point({0, 0}), sLineArc.point({.04, 0}));
    auto ptCenter = sLineArc.point({.02, .02});
    auto arcB = sLineArc.arc(ptCenter, .02, 3.141592653589793, 3.141592653589793);
    auto laReq = fields({{"mode", "Corner"},
                         {"source", lineA},
                         {"boundaries", QCborArray{arcB}},
                         {"pick_x", .01},
                         {"pick_y", 0.},
                         {"pick2_x", .03},
                         {"pick2_y", .03}});
    auto laOut = candidate(sLineArc, laReq);
    auto laRec = laOut.edits.last().toMap();
    CHECK(laRec.value("mode").toString() == "Corner");
    CHECK(laRec.value("descendants").toArray().size() == 2);

    // 3c. Corner trim on Arc and Arc
    sketch::Sketch sArcArc;
    auto arc1 = sArcArc.arc(sArcArc.point({0, 0}), .02, 0, 3.141592653589793);
    auto arc2 = sArcArc.arc(sArcArc.point({.03, 0}), .02, 0, 3.141592653589793);
    auto aaReq = fields({{"mode", "Corner"},
                         {"source", arc1},
                         {"boundaries", QCborArray{arc2}},
                         {"pick_x", 0.},
                         {"pick_y", .02},
                         {"pick2_x", .03},
                         {"pick2_y", .02}});
    auto aaOut = candidate(sArcArc, aaReq);
    auto aaRec = aaOut.edits.last().toMap();
    CHECK(aaRec.value("mode").toString() == "Corner");
    CHECK(aaRec.value("descendants").toArray().size() == 2);

    // Corner trim error on parallel lines
    sketch::Sketch sParallel;
    auto p1 = sParallel.line(sParallel.point({0, 0}), sParallel.point({.04, 0}));
    auto p2 = sParallel.line(sParallel.point({0, .01}), sParallel.point({.04, .01}));
    rejects([&] {
        candidate(sParallel, fields({{"mode", "Corner"},
                                     {"source", p1},
                                     {"boundaries", QCborArray{p2}},
                                     {"pick_x", .02},
                                     {"pick_y", 0.}}));
    });
}
void filletsAndChamfers() {
    // 1. Line/line fillet with shared radius
    sketch::Sketch sFillet;
    auto l1 = sFillet.line(sFillet.point({0, 0}), sFillet.point({.04, 0}));
    auto l2 = sFillet.line(sFillet.point({0, 0}), sFillet.point({0, .04}));
    auto fReq = fields({{"mode", "Fillet"},
                        {"source", l1},
                        {"boundaries", QCborArray{l2}},
                        {"radius", .005},
                        {"shared_radius", true}});
    auto fOut = candidate(sFillet, fReq);
    auto fDesc = fOut.edits.last().toMap().value("descendants").toArray();
    CHECK(fDesc.size() == 3);
    auto arcId = fDesc[2].toString();
    auto &arc = fOut.entity(arcId);
    CHECK(arc.kind == "Arc");
    near(arc.radius, .005, 1e-9);

    // 1b. Fillet via corner vertex Point selection
    sketch::Sketch sPointFillet;
    auto cornerPt = sPointFillet.point({0, 0});
    sPointFillet.line(cornerPt, sPointFillet.point({.04, 0}));
    sPointFillet.line(cornerPt, sPointFillet.point({0, .04}));
    auto ptFReq = fields({{"mode", "Fillet"}, {"source", cornerPt}, {"radius", .005}});
    auto ptFOut = candidate(sPointFillet, ptFReq);
    CHECK(ptFOut.edits.last().toMap().value("descendants").toArray().size() == 3);

    // 1c. Line/Arc fillet
    sketch::Sketch sLineArcFillet;
    auto baseLine =
        sLineArcFillet.line(sLineArcFillet.point({0, 0}), sLineArcFillet.point({.04, 0}));
    auto arcCenter = sLineArcFillet.point({0, .02});
    auto baseArc =
        sLineArcFillet.arc(arcCenter, .02, -3.141592653589793 / 2., 3.141592653589793 / 2.);
    auto laFReq = fields({{"mode", "Fillet"},
                          {"source", baseLine},
                          {"boundaries", QCborArray{baseArc}},
                          {"radius", .005}});
    auto laFOut = candidate(sLineArcFillet, laFReq);
    CHECK(laFOut.edits.last().toMap().value("descendants").toArray().size() == 3);

    // 1d. Arc/Arc fillet
    sketch::Sketch sArcArcFillet;
    auto arcA = sArcArcFillet.arc(sArcArcFillet.point({0, .02}), .02, -3.141592653589793 / 2.,
                                  3.141592653589793 / 2.);
    auto arcB = sArcArcFillet.arc(sArcArcFillet.point({.02, 0}), .02, 3.141592653589793,
                                  3.141592653589793 / 2.);
    auto aafReq = fields(
        {{"mode", "Fillet"}, {"source", arcA}, {"boundaries", QCborArray{arcB}}, {"radius", .005}});
    auto aafOut = candidate(sArcArcFillet, aafReq);
    CHECK(aafOut.edits.last().toMap().value("descendants").toArray().size() == 3);

    // 1e. Curved fillet against actual operand geometry (solve offset intersections, verify
    // contact/tangency)
    sketch::Sketch sCurvedRightAngle;
    auto horizLine =
        sCurvedRightAngle.line(sCurvedRightAngle.point({0, 0}), sCurvedRightAngle.point({.04, 0}));
    auto arcCenterPt = sCurvedRightAngle.point({-.02, 0});
    auto rightAngleArc = sCurvedRightAngle.arc(arcCenterPt, .02, 0, 3.141592653589793 / 2.);
    auto cfReq = fields({{"mode", "Fillet"},
                         {"source", horizLine},
                         {"boundaries", QCborArray{rightAngleArc}},
                         {"radius", .005}});
    auto cfOut = candidate(sCurvedRightAngle, cfReq);
    auto cfDesc = cfOut.edits.last().toMap().value("descendants").toArray();
    CHECK(cfDesc.size() == 3);
    auto filletArcId = cfDesc[2].toString();
    auto &fArc = cfOut.entity(filletArcId);
    CHECK(fArc.kind == "Arc");
    near(fArc.radius, .005, 1e-9);
    // Verified center: (sqrt(0.0006) - 0.02, 0.005)
    auto fArcCenter = cfOut.entity(fArc.a).point;
    near(fArcCenter.x, std::sqrt(0.0006) - 0.02, 1e-6);
    near(fArcCenter.y, .005, 1e-6);
    // Verify 0 endpoint gap between trimmed operand arc and fillet arc
    auto &trimmedArc = cfOut.entity(rightAngleArc);
    auto trimmedArcStart =
        sketch::arcPoint(cfOut.entity(trimmedArc.a).point, trimmedArc.radius, trimmedArc.start);
    auto fArcP0 = sketch::arcPoint(fArcCenter, fArc.radius, fArc.start);
    auto fArcP1 = sketch::arcPoint(fArcCenter, fArc.radius, fArc.start + fArc.sweep);
    double arcGap =
        std::min(sketch::length(fArcP0, trimmedArcStart), sketch::length(fArcP1, trimmedArcStart));
    near(arcGap, 0., 1e-6);

    // 1f. Curved fillet with fillet radius exceeding operand radius (r > Ra)
    sketch::Sketch sCurvedLargeFillet;
    auto lgLine = sCurvedLargeFillet.line(sCurvedLargeFillet.point({0, 0}),
                                          sCurvedLargeFillet.point({.04, 0}));
    auto lgArcCenter = sCurvedLargeFillet.point({-.005, 0});
    auto lgArc = sCurvedLargeFillet.arc(lgArcCenter, .005, 0, 3.141592653589793 / 2.);
    auto cfLgReq = fields({{"mode", "Fillet"},
                           {"source", lgLine},
                           {"boundaries", QCborArray{lgArc}},
                           {"radius", .010}});
    auto cfLgOut = candidate(sCurvedLargeFillet, cfLgReq);
    auto cfLgDesc = cfLgOut.edits.last().toMap().value("descendants").toArray();
    CHECK(cfLgDesc.size() == 3);
    auto lgFilletId = cfLgDesc[2].toString();
    auto &fLgArc = cfLgOut.entity(lgFilletId);
    near(fLgArc.radius, .010, 1e-9);
    auto fLgCenter = cfLgOut.entity(fLgArc.a).point;
    near(fLgCenter.x, std::sqrt(0.000125) - 0.005, 1e-6);
    near(fLgCenter.y, .010, 1e-6);

    // 1g. Curved fillet between two arcs
    sketch::Sketch sTwoArcs;
    auto a1Center = sTwoArcs.point({-.02, 0});
    auto arc1 = sTwoArcs.arc(a1Center, .02, 0, 3.141592653589793 / 2.);
    auto a2Center = sTwoArcs.point({0, -.02});
    auto arc2 = sTwoArcs.arc(a2Center, .02, 0, 3.141592653589793 / 2.);
    auto cf2ArcReq = fields(
        {{"mode", "Fillet"}, {"source", arc1}, {"boundaries", QCborArray{arc2}}, {"radius", .005}});
    auto cf2ArcOut = candidate(sTwoArcs, cf2ArcReq);
    CHECK(cf2ArcOut.edits.last().toMap().value("descendants").toArray().size() == 3);

    // 1h. Small-scale Line/Arc fillet on the 1e fixture (0.05 mm = 5e-5 m radius)
    sketch::Sketch sSmallLineArc;
    auto smLine = sSmallLineArc.line(sSmallLineArc.point({0, 0}), sSmallLineArc.point({.04, 0}));
    auto smArcCenter = sSmallLineArc.point({-.02, 0});
    auto smArc = sSmallLineArc.arc(smArcCenter, .02, 0, 3.141592653589793 / 2.);
    auto smFReq = fields({{"mode", "Fillet"},
                          {"source", smLine},
                          {"boundaries", QCborArray{smArc}},
                          {"radius", .00005}});
    auto smFOut = candidate(sSmallLineArc, smFReq);
    auto smDesc = smFOut.edits.last().toMap().value("descendants").toArray();
    CHECK(smDesc.size() == 3);
    auto smFilletId = smDesc[2].toString();
    auto &smFilletArc = smFOut.entity(smFilletId);
    CHECK(smFilletArc.kind == "Arc");
    near(smFilletArc.radius, .00005, 1e-9);
    auto smCenter = smFOut.entity(smFilletArc.a).point;
    near(smCenter.x, std::sqrt(0.000402) - 0.02, 1e-9);
    near(smCenter.y, .00005, 1e-9);

    // 1i. Small-scale Line/Arc fillet on tangent line/arc fixture
    sketch::Sketch sTangLineArc;
    auto tgLine = sTangLineArc.line(sTangLineArc.point({0, 0}), sTangLineArc.point({.04, 0}));
    auto tgArcCenter = sTangLineArc.point({0, .02});
    auto tgArc =
        sTangLineArc.arc(tgArcCenter, .02, -3.141592653589793 / 2., 3.141592653589793 / 2.);
    auto tgFReq = fields({{"mode", "Fillet"},
                          {"source", tgLine},
                          {"boundaries", QCborArray{tgArc}},
                          {"radius", .00005}});
    auto tgFOut = candidate(sTangLineArc, tgFReq);
    auto tgDesc = tgFOut.edits.last().toMap().value("descendants").toArray();
    CHECK(tgDesc.size() == 3);
    auto tgFilletId = tgDesc[2].toString();
    auto &tgFilletArc = tgFOut.entity(tgFilletId);
    CHECK(tgFilletArc.kind == "Arc");
    near(tgFilletArc.radius, .00005, 1e-9);
    auto tgCenter = tgFOut.entity(tgFilletArc.a).point;
    near(tgCenter.x, .002, 1e-6);
    near(tgCenter.y, .00005, 1e-6);

    // 2. Equal-distance chamfer
    sketch::Sketch sChamfer;
    auto cl1 = sChamfer.line(sChamfer.point({0, 0}), sChamfer.point({.04, 0}));
    auto cl2 = sChamfer.line(sChamfer.point({0, 0}), sChamfer.point({0, .04}));
    auto chReq = fields({{"mode", "Chamfer"},
                         {"source", cl1},
                         {"boundaries", QCborArray{cl2}},
                         {"method", "Equal distance"},
                         {"distance", .005}});
    auto chOut = candidate(sChamfer, chReq);
    auto chDesc = chOut.edits.last().toMap().value("descendants").toArray();
    CHECK(chDesc.size() == 3);
    auto chLineId = chDesc[2].toString();
    auto &chLine = chOut.entity(chLineId);
    CHECK(chLine.kind == "Line");
    auto p1 = chOut.entity(chLine.a).point, p2 = chOut.entity(chLine.b).point;
    near(sketch::length(p1, p2), .005 * std::sqrt(2.), 1e-9);

    // 2b. Chamfer via corner vertex Point selection
    sketch::Sketch sPointChamfer;
    auto chCornerPt = sPointChamfer.point({0, 0});
    sPointChamfer.line(chCornerPt, sPointChamfer.point({.04, 0}));
    sPointChamfer.line(chCornerPt, sPointChamfer.point({0, .04}));
    auto ptChReq = fields({{"mode", "Chamfer"},
                           {"source", chCornerPt},
                           {"method", "Equal distance"},
                           {"distance", .005}});
    auto ptChOut = candidate(sPointChamfer, ptChReq);
    CHECK(ptChOut.edits.last().toMap().value("descendants").toArray().size() == 3);

    // 3. Unequal-distance chamfer
    auto unReq = fields({{"mode", "Chamfer"},
                         {"source", cl1},
                         {"boundaries", QCborArray{cl2}},
                         {"method", "Unequal distance"},
                         {"distance1", .003},
                         {"distance2", .004}});
    auto unOut = candidate(sChamfer, unReq);
    auto unLineId = unOut.edits.last().toMap().value("descendants").toArray()[2].toString();
    auto unLine = unOut.entity(unLineId);
    near(sketch::length(unOut.entity(unLine.a).point, unOut.entity(unLine.b).point), .005, 1e-9);

    // 4. Angle-distance chamfer
    auto angReq = fields({{"mode", "Chamfer"},
                          {"source", cl1},
                          {"boundaries", QCborArray{cl2}},
                          {"method", "Angle distance"},
                          {"distance1", .005},
                          {"angle", 3.141592653589793 / 4.}});
    auto angOut = candidate(sChamfer, angReq);
    CHECK(angOut.edits.last().toMap().value("descendants").toArray().size() == 3);

    // 5. Chamfer with pick coordinates
    auto chPickReq = fields({{"mode", "Chamfer"},
                             {"source", cl1},
                             {"boundaries", QCborArray{cl2}},
                             {"method", "Equal distance"},
                             {"distance", .005},
                             {"pick_x", .005},
                             {"pick_y", 0.}});
    auto chPickOut = candidate(sChamfer, chPickReq);
    CHECK(chPickOut.edits.last().toMap().value("descendants").toArray().size() == 3);

    // 6. Refuse identical entities on Fillet and Chamfer
    rejects([&] {
        candidate(sFillet, fields({{"mode", "Fillet"},
                                   {"source", l1},
                                   {"boundaries", QCborArray{l1}},
                                   {"radius", .005}}));
    });
    rejects([&] {
        candidate(sChamfer, fields({{"mode", "Chamfer"},
                                    {"source", cl1},
                                    {"boundaries", QCborArray{cl1}},
                                    {"distance", .005}}));
    });
}
void offsetEntities() {
    // 1. Single entity offset (Line)
    sketch::Sketch sOffset;
    auto line = sOffset.line(sOffset.point({0, 0}), sOffset.point({.04, 0}));
    auto offReq = fields({{"mode", "Offset entities"}, {"source", line}, {"distance", .005}});
    auto offOut = candidate(sOffset, offReq);
    auto offDesc = offOut.edits.last().toMap().value("descendants").toArray();
    CHECK(offDesc.size() == 1);
    auto &oLine = offOut.entity(offDesc[0].toString());
    near(offOut.entity(oLine.a).point.y, .005, 1e-9);
    near(offOut.entity(oLine.b).point.y, .005, 1e-9);

    // 2. Single entity offset (Circle)
    sketch::Sketch sCirc;
    auto circle = sCirc.circle(sCirc.point({.02, .02}), .01);
    auto cOffReq = fields({{"mode", "Offset entities"}, {"source", circle}, {"distance", .005}});
    auto cOffOut = candidate(sCirc, cOffReq);
    auto cOffDesc = cOffOut.edits.last().toMap().value("descendants").toArray();
    CHECK(cOffDesc.size() == 1);
    near(cOffOut.entity(cOffDesc[0].toString()).radius, .015, 1e-9);

    // 3. Bidirectional offset with line caps
    auto bidiReq = fields({{"mode", "Offset entities"},
                           {"source", line},
                           {"distance", .005},
                           {"bidirectional", true},
                           {"cap_ends", "Line"}});
    auto bidiOut = candidate(sOffset, bidiReq);
    auto bidiDesc = bidiOut.edits.last().toMap().value("descendants").toArray();
    CHECK(bidiDesc.size() == 4); // 2 offset lines + 2 cap lines

    // 3b. Bidirectional offset with arc caps
    auto arcCapReq = fields({{"mode", "Offset entities"},
                             {"source", line},
                             {"distance", .005},
                             {"bidirectional", true},
                             {"cap_ends", "Arc"}});
    auto arcCapOut = candidate(sOffset, arcCapReq);
    auto arcCapDesc = arcCapOut.edits.last().toMap().value("descendants").toArray();
    CHECK(arcCapDesc.size() == 4); // 2 offset lines + 2 cap arcs
    int arcCount = 0;
    for (auto d : arcCapDesc) {
        if (arcCapOut.entity(d.toString()).kind == "Arc") {
            arcCount++;
            near(arcCapOut.entity(d.toString()).radius, .005, 1e-9);
        }
    }
    CHECK(arcCount == 2);

    // 3c. Chain offset with corner vertex joining
    sketch::Sketch sChain;
    auto corner = sChain.point({.02, 0});
    auto ch1 = sChain.line(sChain.point({0, 0}), corner);
    auto ch2 = sChain.line(corner, sChain.point({.02, .02}));
    auto chainReq = fields({{"mode", "Offset entities"},
                            {"source", ch1},
                            {"chain", QCborArray{ch1, ch2}},
                            {"distance", .005}});
    auto chainOut = candidate(sChain, chainReq);
    auto chainDesc = chainOut.edits.last().toMap().value("descendants").toArray();
    CHECK(chainDesc.size() == 2);
    // Adjacent offset lines must share the corner point
    auto &off1 = chainOut.entity(chainDesc[0].toString());
    auto &off2 = chainOut.entity(chainDesc[1].toString());
    CHECK(off1.b == off2.a);

    // 3c-2. Corner joining with separate endpoint IDs (reacquiring entity references after
    // candidate.point)
    sketch::Sketch sSeparateEndChain;
    for (int i = 0; i < 16; ++i)
        sSeparateEndChain.point({double(i), double(i)});
    auto p1 = sSeparateEndChain.point({0, 0});
    auto p2 = sSeparateEndChain.point({.02, 0});
    auto p3 = sSeparateEndChain.point({.02, 0}); // Separate endpoint ID with identical coordinate
    auto p4 = sSeparateEndChain.point({.02, .02});
    auto sepLine1 = sSeparateEndChain.line(p1, p2);
    auto sepLine2 = sSeparateEndChain.line(p3, p4);
    auto sepChainReq = fields({{"mode", "Offset entities"},
                               {"source", sepLine1},
                               {"chain", QCborArray{sepLine1, sepLine2}},
                               {"distance", .005}});
    auto sepChainOut = candidate(sSeparateEndChain, sepChainReq);
    auto sepDesc = sepChainOut.edits.last().toMap().value("descendants").toArray();
    CHECK(sepDesc.size() == 2);
    auto &sepOff1 = sepChainOut.entity(sepDesc[0].toString());
    auto &sepOff2 = sepChainOut.entity(sepDesc[1].toString());
    // Offset segments must be connected at the reacquired corner point, not disconnected by 7.07 mm
    CHECK(sepOff1.b == sepOff2.a);
    near(sketch::length(sepChainOut.entity(sepOff1.b).point, sepChainOut.entity(sepOff2.a).point),
         0., 1e-9);
    near(sepChainOut.entity(sepOff1.b).point.x, .015, 1e-9);
    near(sepChainOut.entity(sepOff1.b).point.y, .005, 1e-9);

    // 3d. Automatic chain selection (chain: true)
    sketch::Sketch sAutoChain;
    auto ac1 = sAutoChain.line(sAutoChain.point({0, 0}), sAutoChain.point({.02, 0}));
    auto ac2 = sAutoChain.line(sAutoChain.point({.02, 0}), sAutoChain.point({.02, .02}));
    auto acReq =
        fields({{"mode", "Offset entities"}, {"source", ac1}, {"chain", true}, {"distance", .005}});
    auto acOut = candidate(sAutoChain, acReq);
    CHECK(acOut.edits.last().toMap().value("descendants").toArray().size() == 2);

    // 3e. Closed loop chain offset
    sketch::Sketch sClosed;
    auto cp0 = sClosed.point({0, 0}), cp1 = sClosed.point({.04, 0}),
         cp2 = sClosed.point({.04, .04}), cp3 = sClosed.point({0, .04});
    auto cl0 = sClosed.line(cp0, cp1);
    sClosed.line(cp1, cp2);
    sClosed.line(cp2, cp3);
    sClosed.line(cp3, cp0);
    auto clReq =
        fields({{"mode", "Offset entities"}, {"source", cl0}, {"chain", true}, {"distance", .005}});
    auto clOut = candidate(sClosed, clReq);
    CHECK(clOut.edits.last().toMap().value("descendants").toArray().size() == 4);

    // 4. Base construction option
    auto constReq = fields({{"mode", "Offset entities"},
                            {"source", line},
                            {"distance", .005},
                            {"make_construction", true}});
    auto constOut = candidate(sOffset, constReq);
    CHECK(constOut.entity(line).construction == true);

    // 5. Invalid options rejected
    rejects([&] {
        candidate(sOffset, fields({{"mode", "Offset entities"},
                                   {"source", line},
                                   {"distance", .005},
                                   {"bidirectional", false},
                                   {"cap_ends", "Line"}}));
    });
    rejects([&] {
        candidate(sCirc, fields({{"mode", "Offset entities"},
                                 {"source", circle},
                                 {"distance", .015},
                                 {"reverse", true}}));
    });

    // 6. Refuse fit splines & already-offset geometry
    sketch::Sketch sSpline;
    auto sp0 = sSpline.point({0, 0}), sp1 = sSpline.point({.02, .01}),
         sp2 = sSpline.point({.04, 0});
    sketch::Entity spline;
    spline.id = newId();
    spline.kind = "Spline";
    spline.a = sp0;
    spline.b = sp2;
    spline.poles = {sp0, sp1, sp2};
    spline.weights = {1., 1., 1.};
    spline.knots = {0., 1.};
    spline.multiplicities = {3, 3};
    spline.degree = 2;
    sSpline.entities.push_back(spline);
    rejects([&] {
        candidate(sSpline,
                  fields({{"mode", "Offset entities"}, {"source", spline.id}, {"distance", .005}}));
    });

    auto firstOffset = candidate(sOffset, offReq);
    auto offChildId = firstOffset.edits.last().toMap().value("descendants").toArray()[0].toString();
    rejects([&] {
        candidate(
            firstOffset,
            fields({{"mode", "Offset entities"}, {"source", offChildId}, {"distance", .005}}));
    });
}
void transformsAndPatterns() {
    // 1. Linear pattern 3x2
    sketch::Sketch sPat;
    auto circle = sPat.circle(sPat.point({0, 0}), .002);
    auto lpReq = fields({{"mode", "Linear pattern"},
                         {"source", circle},
                         {"count1", 3},
                         {"spacing1", .01},
                         {"count2", 2},
                         {"spacing2", .01}});
    auto lpOut = candidate(sPat, lpReq);
    auto lpDesc = lpOut.edits.last().toMap().value("descendants").toArray();
    CHECK(lpDesc.size() == 5); // 3*2 - 1 = 5 new instances

    // 2. Circular pattern with equal spacing over half circle (pi)
    sketch::Sketch sCircPat;
    auto seed = sCircPat.circle(sCircPat.point({.02, 0}), .002);
    auto cpReq = fields({{"mode", "Circular pattern"},
                         {"source", seed},
                         {"center_x", 0.},
                         {"center_y", 0.},
                         {"angle", 3.141592653589793},
                         {"count", 3},
                         {"equal_spacing", true}});
    auto cpOut = candidate(sCircPat, cpReq);
    auto cpDesc = cpOut.edits.last().toMap().value("descendants").toArray();
    CHECK(cpDesc.size() == 2);
    // Last instance at angle pi: x = -0.02, y = 0
    auto &lastCircle = cpOut.entity(cpDesc[1].toString());
    near(cpOut.entity(lastCircle.a).point.x, -.02, 1e-9);
    near(cpOut.entity(lastCircle.a).point.y, 0., 1e-9);

    // 3. Mirror & Dynamic mirror (including Arc mirror across vertical line)
    sketch::Sketch sMirror;
    auto ml = sMirror.line(sMirror.point({.02, -.02}), sMirror.point({.02, .04}), true);
    auto target = sMirror.line(sMirror.point({.01, 0}), sMirror.point({.01, .02}));
    auto mReq =
        fields({{"mode", "Mirror"}, {"source", target}, {"mirror_line", ml}, {"copy", true}});
    auto mOut = candidate(sMirror, mReq);
    auto mDesc = mOut.edits.last().toMap().value("descendants").toArray();
    CHECK(mDesc.size() == 1);
    auto &mLine = mOut.entity(mDesc[0].toString());
    near(mOut.entity(mLine.a).point.x, .03, 1e-9);
    near(mOut.entity(mLine.b).point.x, .03, 1e-9);

    auto arcTarget = sMirror.arc(sMirror.point({.01, .01}), .005, 0, 3.141592653589793 / 2.);
    auto mArcReq =
        fields({{"mode", "Mirror"}, {"source", arcTarget}, {"mirror_line", ml}, {"copy", true}});
    auto mArcOut = candidate(sMirror, mArcReq);
    auto mArcDesc = mArcOut.edits.last().toMap().value("descendants").toArray();
    CHECK(mArcDesc.size() == 1);
    auto &refArc = mArcOut.entity(mArcDesc[0].toString());
    near(mArcOut.entity(refArc.a).point.x, .03, 1e-9);
    near(mArcOut.entity(refArc.a).point.y, .01, 1e-9);

    auto dynReq = fields({{"mode", "Dynamic mirror"}, {"source", target}, {"mirror_line", ml}});
    auto dynOut = candidate(sMirror, dynReq);
    bool foundSym = false;
    for (auto &c : dynOut.constraints)
        if (c.kind == "Symmetric")
            foundSym = true;
    CHECK(foundSym);

    // 3b. Dynamic mirror converts non-construction line to centerline
    sketch::Sketch sDynNorm;
    auto normLine = sDynNorm.line(sDynNorm.point({.02, -.02}), sDynNorm.point({.02, .04}), false);
    auto normTgt = sDynNorm.line(sDynNorm.point({.01, 0}), sDynNorm.point({.01, .02}));
    auto normDynReq =
        fields({{"mode", "Dynamic mirror"}, {"source", normTgt}, {"mirror_line", normLine}});
    auto normDynOut = candidate(sDynNorm, normDynReq);
    CHECK(normDynOut.entity(normLine).construction == true);

    // 3c. Dynamic mirror on Circle preserves symmetric relations (center and radius)
    sketch::Sketch sDynCirc;
    auto dml = sDynCirc.line(sDynCirc.point({0, -.05}, true), sDynCirc.point({0, .05}, true), true);
    auto srcCircPt = sDynCirc.point({-.02, .01});
    auto srcCirc = sDynCirc.circle(srcCircPt, .008);
    auto dynCircReq =
        fields({{"mode", "Dynamic mirror"}, {"source", srcCirc}, {"mirror_line", dml}});
    auto dynCircOut = candidate(sDynCirc, dynCircReq);
    auto dynCircDesc = dynCircOut.edits.last().toMap().value("descendants").toArray();
    CHECK(dynCircDesc.size() == 1);
    auto mirCircId = dynCircDesc[0].toString();
    auto &mirCirc = dynCircOut.entity(mirCircId);
    near(dynCircOut.entity(mirCirc.a).point.x, .02, 1e-9);
    near(dynCircOut.entity(mirCirc.a).point.y, .01, 1e-9);
    near(mirCirc.radius, .008, 1e-9);
    bool foundCircSym = false;
    for (auto &c : dynCircOut.constraints) {
        if (c.kind == "Symmetric" && c.a == srcCirc && c.b == mirCircId && c.third == dml)
            foundCircSym = true;
    }
    CHECK(foundCircSym);

    // Dynamic mirror on Circle: later source edit updates its mirror
    dynCircOut.entity(srcCircPt).point = {-.03, .015};
    dynCircOut.entity(srcCirc).radius = .012;
    // Fix mirror line so the centerline is locked during solve
    sketch::Constraint fixML1;
    fixML1.kind = "Fixed";
    fixML1.a = dynCircOut.entity(dml).a;
    fixML1.value = 0.;
    fixML1.second = -.05;
    sketch::Constraint fixML2;
    fixML2.kind = "Fixed";
    fixML2.a = dynCircOut.entity(dml).b;
    fixML2.value = 0.;
    fixML2.second = .05;
    sketch::Constraint fixSrcPt;
    fixSrcPt.kind = "Fixed";
    fixSrcPt.a = srcCircPt;
    fixSrcPt.value = -.03;
    fixSrcPt.second = .015;
    dynCircOut.constraints.push_back(fixML1);
    dynCircOut.constraints.push_back(fixML2);
    dynCircOut.constraints.push_back(fixSrcPt);
    auto solvedCirc = sketch::solve(dynCircOut);
    CHECK(solvedCirc.valid);
    auto &solvedMirCirc = solvedCirc.sketch.entity(mirCircId);
    near(solvedCirc.sketch.entity(solvedMirCirc.a).point.x, .03, 1e-6);
    near(solvedCirc.sketch.entity(solvedMirCirc.a).point.y, .015, 1e-6);
    near(solvedMirCirc.radius, .012, 1e-6);

    // Dynamic mirror rejects copy: false
    rejects([&] {
        candidate(sDynCirc, fields({{"mode", "Dynamic mirror"},
                                    {"source", srcCirc},
                                    {"mirror_line", dml},
                                    {"copy", false}}));
    });

    // 3d. Dynamic mirror on Arc preserves symmetric relations (center, radius, orientation)
    sketch::Sketch sDynArc;
    auto dmlArc = sDynArc.line(sDynArc.point({0, -.05}, true), sDynArc.point({0, .05}, true), true);
    auto srcArcPt = sDynArc.point({-.02, .01});
    auto srcArc = sDynArc.arc(srcArcPt, .01, 0, 3.141592653589793 / 2.);
    auto dynArcReq =
        fields({{"mode", "Dynamic mirror"}, {"source", srcArc}, {"mirror_line", dmlArc}});
    auto dynArcOut = candidate(sDynArc, dynArcReq);
    auto dynArcDesc = dynArcOut.edits.last().toMap().value("descendants").toArray();
    CHECK(dynArcDesc.size() == 1);
    auto mirArcId = dynArcDesc[0].toString();
    auto &mirArc = dynArcOut.entity(mirArcId);
    near(dynArcOut.entity(mirArc.a).point.x, .02, 1e-9);
    near(dynArcOut.entity(mirArc.a).point.y, .01, 1e-9);
    near(mirArc.radius, .01, 1e-9);
    near(mirArc.start, 3.141592653589793 / 2., 1e-9);
    near(mirArc.sweep, 3.141592653589793 / 2., 1e-9);
    bool foundArcSym = false;
    for (auto &c : dynArcOut.constraints) {
        if (c.kind == "Symmetric" && c.a == srcArc && c.b == mirArcId && c.third == dmlArc)
            foundArcSym = true;
    }
    CHECK(foundArcSym);

    // Dynamic mirror rejects unsupported operands (e.g. Spline)
    sketch::Sketch sDynSpline;
    auto spDml =
        sDynSpline.line(sDynSpline.point({0, -.05}, true), sDynSpline.point({0, .05}, true), true);
    auto spId = sketch::createSpline(sDynSpline, {{-.01, 0}, {-.02, .01}, {-.03, 0}, {-.04, .01}},
                                     3, false, false);
    rejects([&] {
        candidate(sDynSpline,
                  fields({{"mode", "Dynamic mirror"}, {"source", spId}, {"mirror_line", spDml}}));
    });

    // 4. Move, Copy, Rotate, Scale
    sketch::Sketch sTrans;
    auto tLine = sTrans.line(sTrans.point({0, 0}), sTrans.point({.02, 0}));
    auto moveOut = candidate(
        sTrans, fields({{"mode", "Move"}, {"source", tLine}, {"delta_x", .01}, {"delta_y", .02}}));
    near(moveOut.entity(moveOut.entity(tLine).a).point.x, .01, 1e-9);
    near(moveOut.entity(moveOut.entity(tLine).a).point.y, .02, 1e-9);

    auto copyOut = candidate(
        sTrans, fields({{"mode", "Copy"}, {"source", tLine}, {"delta_x", .01}, {"delta_y", .02}}));
    CHECK(copyOut.edits.last().toMap().value("descendants").toArray().size() == 1);

    auto rotOut = candidate(sTrans, fields({{"mode", "Rotate"},
                                            {"source", tLine},
                                            {"center_x", 0.},
                                            {"center_y", 0.},
                                            {"angle", 3.141592653589793 / 2.}}));
    near(rotOut.entity(rotOut.entity(tLine).b).point.x, 0., 1e-9);
    near(rotOut.entity(rotOut.entity(tLine).b).point.y, .02, 1e-9);

    auto rotCopyOut = candidate(sTrans, fields({{"mode", "Rotate"},
                                                {"source", tLine},
                                                {"center_x", 0.},
                                                {"center_y", 0.},
                                                {"angle", 3.141592653589793 / 2.},
                                                {"copy", true}}));
    CHECK(rotCopyOut.edits.last().toMap().value("descendants").toArray().size() == 1);

    // Rotate arc crossing 2*pi
    sketch::Sketch sArcRot;
    auto arcRotTarget = sArcRot.arc(sArcRot.point({0, 0}), .01, 3.141592653589793, 1.0);
    auto rotArcReq = fields({{"mode", "Rotate"},
                             {"source", arcRotTarget},
                             {"center_x", 0.},
                             {"center_y", 0.},
                             {"angle", 3.141592653589793 * 1.5}});
    auto rotArcOut = candidate(sArcRot, rotArcReq);
    CHECK(std::abs(rotArcOut.entity(arcRotTarget).start) <= 3.141592653589793);

    // Circular pattern arc crossing 2*pi
    sketch::Sketch sArcPat;
    auto arcPatTarget = sArcPat.arc(sArcPat.point({.02, 0}), .005, 3.0, 1.0);
    auto patArcReq = fields({{"mode", "Circular pattern"},
                             {"source", arcPatTarget},
                             {"center_x", 0.},
                             {"center_y", 0.},
                             {"count", 4},
                             {"angle", 6.2831853071795864769}});
    auto patArcOut = candidate(sArcPat, patArcReq);
    CHECK(patArcOut.edits.last().toMap().value("descendants").toArray().size() == 3);

    auto scaleOut = candidate(sTrans, fields({{"mode", "Scale"},
                                              {"source", tLine},
                                              {"base_x", 0.},
                                              {"base_y", 0.},
                                              {"factor", 2.}}));
    near(scaleOut.entity(scaleOut.entity(tLine).b).point.x, .04, 1e-9);

    auto scaleCopyOut = candidate(sTrans, fields({{"mode", "Scale"},
                                                  {"source", tLine},
                                                  {"base_x", 0.},
                                                  {"base_y", 0.},
                                                  {"factor", 2.},
                                                  {"copy", true}}));
    CHECK(scaleCopyOut.edits.last().toMap().value("descendants").toArray().size() == 1);

    // 4b. Scale conic: 20-by-10 mm ellipse scaled by 2 scales minor radius and major radius
    // proportionally
    sketch::Sketch sScaleEllipse;
    auto ellId = sketch::createEllipse(sScaleEllipse, {0, 0}, {.02, 0}, {0, .01});
    near(sScaleEllipse.entity(ellId).radius, .01, 1e-9); // minor radius = 10 mm
    auto scaleEllReq = fields(
        {{"mode", "Scale"}, {"source", ellId}, {"base_x", 0.}, {"base_y", 0.}, {"factor", 2.}});
    auto scaleEllOut = candidate(sScaleEllipse, scaleEllReq);
    auto &scaledEll = scaleEllOut.entity(ellId);
    near(scaledEll.radius, .02, 1e-9); // minor radius scaled to 20 mm
    auto fCenter = scaleEllOut.entity(scaledEll.a).point;
    auto fFocus = scaleEllOut.entity(scaledEll.b).point;
    double scaledMajor = std::hypot(sketch::length(fFocus, fCenter), scaledEll.radius);
    near(scaledMajor, .04, 1e-9); // major radius scaled from 20 mm to 40 mm

    // 4c. Scale conic with copy: true
    auto scaleEllCopyReq = fields({{"mode", "Scale"},
                                   {"source", ellId},
                                   {"base_x", 0.},
                                   {"base_y", 0.},
                                   {"factor", 2.},
                                   {"copy", true}});
    auto scaleEllCopyOut = candidate(sScaleEllipse, scaleEllCopyReq);
    auto copyEllDesc = scaleEllCopyOut.edits.last().toMap().value("descendants").toArray();
    CHECK(copyEllDesc.size() == 1);
    auto &copiedEll = scaleEllCopyOut.entity(copyEllDesc[0].toString());
    near(copiedEll.radius, .02, 1e-9);
    auto copyCenter = scaleEllCopyOut.entity(copiedEll.a).point;
    auto copyFocus = scaleEllCopyOut.entity(copiedEll.b).point;
    double copyMajor = std::hypot(sketch::length(copyFocus, copyCenter), copiedEll.radius);
    near(copyMajor, .04, 1e-9);

    // 4d. Scale EllipticalArc scales minor and major radii while preserving sweep angle
    sketch::Sketch sScaleEllArc;
    auto ellArcId = sketch::createEllipse(sScaleEllArc, {0, 0}, {.02, 0}, {0, .01}, true, 0,
                                          3.141592653589793 / 2.);
    auto scaleEllArcReq = fields(
        {{"mode", "Scale"}, {"source", ellArcId}, {"base_x", 0.}, {"base_y", 0.}, {"factor", 2.}});
    auto scaleEllArcOut = candidate(sScaleEllArc, scaleEllArcReq);
    auto &scaledEllArc = scaleEllArcOut.entity(ellArcId);
    near(scaledEllArc.radius, .02, 1e-9);
    near(scaledEllArc.start, 0., 1e-9);
    near(scaledEllArc.sweep, 3.141592653589793 / 2., 1e-9);

    // 4e. Scale Parabola scales focus distance and parameter range
    sketch::Sketch sScaleParabola;
    auto parId = sketch::createParabola(sScaleParabola, {.01, 0}, {0, 0}, {0, -.02}, {0, .02});
    auto scaleParReq = fields(
        {{"mode", "Scale"}, {"source", parId}, {"base_x", 0.}, {"base_y", 0.}, {"factor", 2.}});
    auto scaleParOut = candidate(sScaleParabola, scaleParReq);
    auto &scaledPar = scaleParOut.entity(parId);
    auto parVertex = scaleParOut.entity(scaledPar.a).point;
    auto parFocus = scaleParOut.entity(scaledPar.b).point;
    near(sketch::length(parVertex, parFocus), .02, 1e-9);
    near(scaledPar.start, -0.04, 1e-9);
    near(scaledPar.sweep, 0.08, 1e-9);

    // 4f. Copy spline created by createSpline preserves pole identities
    sketch::Sketch sCopySpline;
    auto splineId =
        sketch::createSpline(sCopySpline, {{0, 0}, {.01, .03}, {.03, -.01}, {.05, .01}});
    auto spCopyReq =
        fields({{"mode", "Copy"}, {"source", splineId}, {"delta_x", .01}, {"delta_y", .02}});
    auto spCopyOut = candidate(sCopySpline, spCopyReq);
    auto spDesc = spCopyOut.edits.last().toMap().value("descendants").toArray();
    CHECK(spDesc.size() == 1);
    auto copiedSplineId = spDesc[0].toString();
    auto &copiedSpline = spCopyOut.entity(copiedSplineId);
    CHECK(copiedSpline.kind == "Spline");
    CHECK(copiedSpline.b.isEmpty());
    CHECK(copiedSpline.poles.size() == 4);
    CHECK(copiedSpline.a == copiedSpline.poles[0]);
    near(spCopyOut.entity(copiedSpline.poles[0]).point.x, .01, 1e-9);
    near(spCopyOut.entity(copiedSpline.poles[0]).point.y, .02, 1e-9);
    near(spCopyOut.entity(copiedSpline.poles[1]).point.x, .02, 1e-9);
    near(spCopyOut.entity(copiedSpline.poles[1]).point.y, .05, 1e-9);
    near(spCopyOut.entity(copiedSpline.poles[2]).point.x, .04, 1e-9);
    near(spCopyOut.entity(copiedSpline.poles[2]).point.y, .01, 1e-9);
    near(spCopyOut.entity(copiedSpline.poles[3]).point.x, .06, 1e-9);
    near(spCopyOut.entity(copiedSpline.poles[3]).point.y, .03, 1e-9);

    // 4g. Copy closed spline preserves pole identity (poles.back() == poles.front()) and
    // construction
    sketch::Sketch sCopyClosedSpline;
    auto closedSplineId = sketch::createSpline(
        sCopyClosedSpline, {{0, 0}, {.01, .03}, {.03, -.01}, {.05, .01}}, 3, true);
    auto spClosedCopyReq =
        fields({{"mode", "Copy"}, {"source", closedSplineId}, {"delta_x", .01}, {"delta_y", .02}});
    auto spClosedCopyOut = candidate(sCopyClosedSpline, spClosedCopyReq);
    auto spClosedDesc = spClosedCopyOut.edits.last().toMap().value("descendants").toArray();
    CHECK(spClosedDesc.size() == 1);
    auto &copiedClosed = spClosedCopyOut.entity(spClosedDesc[0].toString());
    CHECK(copiedClosed.kind == "Spline");
    CHECK(copiedClosed.b.isEmpty());
    CHECK(copiedClosed.a == copiedClosed.poles[0]);
    CHECK(copiedClosed.poles.last() == copiedClosed.poles.first());
    CHECK(spClosedCopyOut.entity(copiedClosed.poles[0]).construction);

    // 4h. Move and Copy Point entity
    sketch::Sketch sPointSketch;
    auto ptId = sPointSketch.point({.01, .02});
    auto ptMoveOut =
        candidate(sPointSketch,
                  fields({{"mode", "Move"}, {"source", ptId}, {"delta_x", .03}, {"delta_y", .04}}));
    near(ptMoveOut.entity(ptId).point.x, .04, 1e-9);
    near(ptMoveOut.entity(ptId).point.y, .06, 1e-9);
    auto ptCopyOut =
        candidate(sPointSketch,
                  fields({{"mode", "Copy"}, {"source", ptId}, {"delta_x", .05}, {"delta_y", .06}}));
    auto ptCopyDesc = ptCopyOut.edits.last().toMap().value("descendants").toArray();
    CHECK(ptCopyDesc.size() == 1);
    auto newPtId = ptCopyDesc[0].toString();
    near(ptCopyOut.entity(newPtId).point.x, .06, 1e-9);
    near(ptCopyOut.entity(newPtId).point.y, .08, 1e-9);
}
} // namespace
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    return run([] {
        analytical();
        extensionAndBoundaries();
        constraints();
        persistence();
        upstreamFailure();
        powerTrimAndCorner();
        filletsAndChamfers();
        offsetEntities();
        transformsAndPatterns();
    });
}
