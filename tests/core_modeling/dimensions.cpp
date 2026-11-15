#include "sketch/Dimensions.hpp"
#include "persistence/ProjectStore.hpp"
#include "sketch/Editing.hpp"
#include "sketch/Solver.hpp"
#include "support/check.hpp"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
using namespace cad;
using namespace cad::sketch;
namespace {
Constraint &add(Sketch &s, QString kind, QString a, QString b = {}, double value = 0) {
    Constraint c;
    c.kind = kind;
    c.a = a;
    c.b = b;
    c.value = value;
    s.constraints.push_back(c);
    return s.constraints.back();
}
Sketch solved(Sketch s, const Sketch *previous = nullptr) {
    auto r = solve(Sketch::parse(s.map()), previous);
    if (!r.valid)
        std::cerr << r.message.toStdString() << '\n';
    CHECK(r.valid);
    CHECK(r.maximumResidual <= 1e-7);
    validateAccepted(encode({r.sketch}));
    return r.sketch;
}
void geometry() {
    // CM-077/078: signed components, point-pair and line, independent SI checks.
    for (double scale : {.001, 1., 1000.})
        for (QString kind : {"HorizontalDistance", "VerticalDistance"})
            for (bool useLine : {false, true})
                for (double sign : {-1., 1.}) {
                    Sketch s;
                    auto p = s.point({.01 * scale, .02 * scale});
                    auto q = s.point({.03 * scale, .04 * scale});
                    auto l = s.line(p, q);
                    auto &fixed = add(s, "Fixed", p);
                    fixed.value = s.entity(p).point.x;
                    fixed.second = s.entity(p).point.y;
                    auto &c = add(s, kind, useLine ? l : p, useLine ? QString() : q);
                    auto id = c.id;
                    c.expression = QString::number(sign * .025 * scale, 'g', 17) + " m";
                    c.placement = Vec2{.017, .032};
                    auto output = solved(s);
                    auto a = output.entity(p).point, b = output.entity(q).point;
                    near(kind == "HorizontalDistance" ? b.x - a.x : b.y - a.y, sign * .025 * scale,
                         1e-9 * scale);
                    CHECK(solve(output).freedom == 1);
                    CHECK(output.constraints.back().id == id);
                    CHECK(output.constraints.back().placement->x == .017);
                    CHECK(output.map().value("version").toInteger() == 6);
                    auto conflict = output;
                    add(conflict, kind, useLine ? l : p, useLine ? QString() : q,
                        sign * .026 * scale);
                    CHECK(!solve(conflict).valid);
                    CHECK(output.map() == Sketch::parse(output.map()).map());
                    auto forged = output.map();
                    forged.insert(QStringLiteral("version"), 5);
                    rejects([&] { Sketch::parse(forged); });
                }
    // CM-076/079/080/081/086: every supported reference is measured, never driving.
    for (QString kind : {"Length", "Distance", "HorizontalDistance", "VerticalDistance", "Angle",
                         "Radius", "Diameter", "Sweep"}) {
        Sketch s;
        auto p = s.point({0, 0}), q = s.point({.03, .04});
        auto line = s.line(p, q), other = s.line(p, s.point({.03, -.01}));
        auto arc = s.arc(p, .005, 0, 1.2);
        auto before = s;
        auto &c = add(s, kind,
                      kind == "Distance"                                          ? p
                      : kind == "Radius" || kind == "Diameter" || kind == "Sweep" ? arc
                                                                                  : line,
                      kind == "Distance" ? q
                      : kind == "Angle"  ? other
                                         : QString(),
                      999);
        c.driving = false;
        double measurement = measureDimension(s, c);
        auto output = solved(s);
        near(output.constraints.back().value, measurement, 1e-12);
        CHECK(solve(before).freedom == solve(output).freedom);
        for (size_t i = 0; i < before.entities.size(); ++i) {
            CHECK(before.entities[i].point.x == output.entities[i].point.x);
            CHECK(before.entities[i].point.y == output.entities[i].point.y);
        }
        auto invalid = s;
        invalid.constraints.back().expression = "1 mm";
        CHECK(!solve(invalid).valid);
        invalid = s;
        invalid.constraints.back().placement = Vec2{NAN, 0};
        rejects([&] { invalid.validate(); });
    }
    Sketch s;
    auto p = s.point({0, 0}), q = s.point({.03, .04}), line = s.line(p, q);
    add(s, "Fixed", p);
    add(s, "Length", line, {}, .05);
    auto &reference = add(s, "HorizontalDistance", p, q);
    reference.driving = false;
    auto out = solved(s);
    near(out.constraints.back().value, .03, 1e-10);
    out.constraints[1].expression = "100 mm";
    auto changed = solved(out);
    near(length(changed.entity(p).point, changed.entity(q).point), .1, 1e-9);
    near(changed.constraints.back().value, changed.entity(q).point.x, 1e-10);
    auto tampered = changed;
    tampered.constraints.back().value += .001;
    rejects([&] { validateAccepted(encode({tampered})); });
    // A readout cannot prevent otherwise free dragging.
    Sketch free;
    auto point = free.point({.01, .02});
    auto circle = free.circle(point, .003);
    add(free, "Radius", circle).driving = false;
    auto dragged = solved(dragEntity(free, circle, {.013, .02}, {.014, .02}));
    near(dragged.constraints[0].value, .004, 1e-10);
    auto wrong = s;
    wrong.constraints[0].driving = false;
    rejects([&] { wrong.validate(); });
    wrong = s;
    wrong.constraints.back().expression = "5 deg";
    wrong.constraints.back().driving = true;
    CHECK(!solve(wrong).valid);
    // Readouts must neither add branch restrictions nor disable existing ones.
    Sketch readout;
    auto origin = readout.point({0, 0}), end = readout.point({.03, .04});
    auto edge = readout.line(origin, end);
    add(readout, "Length", edge).driving = false;
    readout = solved(readout);
    auto reversed = readout;
    reversed.entity(end).point = {-.03, -.04};
    CHECK(solve(reversed, &readout).valid);
    Sketch controlled;
    origin = controlled.point({0, 0});
    end = controlled.point({.03, .04});
    edge = controlled.line(origin, end);
    auto disk = controlled.circle(controlled.point({.1, .1}), .005);
    add(controlled, "Fixed", origin);
    add(controlled, "Length", edge, {}, .05);
    add(controlled, "Radius", disk).driving = false;
    controlled = solved(controlled);
    reversed = controlled;
    reversed.entity(end).point = {-.03, -.04};
    reversed.entity(disk).radius = .007;
    CHECK(!solve(reversed, &controlled).valid);
}
void mark(QString path) {
    QFile marker(path + "/.cad-test-root");
    CHECK(marker.open(QIODevice::WriteOnly));
    marker.write("disposable\n");
}
void persistence() {
    QTemporaryDir root(QDir::tempPath() + "/cad-module3-cm3-dimensions-XXXXXX"),
        files(QDir::tempPath() + "/cad-module3-cm3-files-XXXXXX");
    CHECK(root.isValid() && files.isValid());
    mark(root.path());
    mark(files.path());
    storage::ProjectStore store(root.path());
    auto project = newId(), doc = newId();
    store.create(project);
    auto command = [&](QString kind, QCborMap payload = QCborMap{}) {
        wire::Envelope e;
        e.context.project = project;
        e.context.document = doc;
        e.revision = store.revision();
        e.command = kind;
        e.payload = payload;
        return e;
    };
    store.execute(command("CreateDocument", fields({{"id", doc},
                                                    {"type", "Part"},
                                                    {"name", "CM3 dimensions"},
                                                    {"units", "mm"},
                                                    {"properties_entity", newId()}})));
    Sketch s;
    auto p = s.point({0, 0}), q = s.point({.03, .04});
    s.line(p, q);
    add(s, "Fixed", p);
    store.execute(command("SetSketches", fields({{"sketches", encode({s})}})));
    auto old = files.filePath("old.cadproj");
    store.save(command("SaveProject"), old);
    QFile original(old);
    CHECK(original.open(QIODevice::ReadOnly));
    auto bytes = original.readAll();
    original.close();
    auto &driving = add(s, "HorizontalDistance", p, q);
    driving.expression = "W";
    driving.placement = Vec2{.02, .05};
    s.names.insert("W", "2 * 25.4 mm");
    add(s, "Distance", p, q).driving = false;
    auto edit = command("SetSketches", fields({{"sketches", encode({s})}}));
    store.execute(edit);
    auto accepted = store.sketches(doc);
    auto revision = store.revision();
    store.execute(edit);
    CHECK(store.revision() == revision);
    bool refused = false;
    try {
        store.save(command("SaveProject"), old);
    } catch (const domain::Error &e) {
        refused = e.code == "UpgradeCopyRequired";
    }
    CHECK(refused);
    CHECK(original.open(QIODevice::ReadOnly));
    CHECK(original.readAll() == bytes);
    store.execute(command("Undo"));
    CHECK(store.sketches(doc)[0].toMap().value("version").toInteger() == 1);
    auto undoPath = files.filePath("undo.cadproj");
    store.save(command("SaveProject"), undoPath);
    store.close();
    auto forged = files.filePath("forged.cadproj");
    CHECK(QFile::copy(undoPath, forged));
    {
        storage::Database db(forged);
        auto manifest = db.scalar("SELECT manifest FROM project_meta").toString();
        CHECK(manifest.endsWith(";sketch-dimensions=1"));
        db.run("UPDATE project_meta SET manifest=?",
               {manifest.section(";sketch-dimensions", 0, 0)});
    }
    rejects([&] { store.open(forged, "Saved"); });
    store.open(undoPath, "Saved");
    store.execute(command("Redo"));
    CHECK(store.sketches(doc) == accepted);
    auto invalid = sketch::parse(accepted)[0];
    add(invalid, "HorizontalDistance", p, q, .06);
    auto priorRevision = store.revision();
    rejects(
        [&] { store.execute(command("SetSketches", fields({{"sketches", encode({invalid})}}))); });
    CHECK(store.revision() == priorRevision);
    CHECK(store.sketches(doc) == accepted);
    auto copy = files.filePath("dimensions.cadproj");
    store.save(command("SaveProject"), copy);
    store.close();
    store.open(copy, "Saved");
    CHECK(store.sketches(doc) == accepted);
    store.execute(command("Undo"));
    store.execute(command("Redo"));
    CHECK(store.sketches(doc) == accepted);
}
} // namespace
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    return run([] {
        geometry();
        persistence();
    });
}
