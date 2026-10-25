#include "persistence/ProjectStore.hpp"
#include "sketch/Constructions.hpp"
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
} // namespace
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    return run([] {
        analytical();
        extensionAndBoundaries();
        constraints();
        persistence();
        upstreamFailure();
    });
}
