#include "geometry/occt/Part.hpp"
#include "persistence/ProjectStore.hpp"
#include "sketch/Dimensions.hpp"
#include "sketch/Solver.hpp"
#include "support/check.hpp"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <numbers>
using namespace cad;
using namespace cad::sketch;
namespace {
Constraint &add(Sketch &s, QString kind, QString a, QString b = {}, double value = 0) {
    Constraint c;
    c.kind = kind;
    c.a = a;
    c.b = b;
    c.value = value;
    if (kind == "Fixed") {
        c.value = s.entity(a).point.x;
        c.second = s.entity(a).point.y;
    }
    s.constraints.push_back(c);
    return s.constraints.back();
}
Sketch exact(Sketch s, const Sketch *previous = nullptr) {
    auto r = solve(Sketch::parse(s.map()), previous);
    if (!r.valid)
        std::cerr << r.message.toStdString() << '\n';
    CHECK(r.valid);
    CHECK(r.maximumResidual < 1e-7);
    validateAccepted(encode({r.sketch}));
    return r.sketch;
}
void corpus() {
    {
        Sketch ambiguous;
        auto axis = ambiguous.line(ambiguous.point({0, 0}), ambiguous.point({0, .1}), true);
        auto profile = ambiguous.line(ambiguous.point({.03, 0}), ambiguous.point({.04, .04}));
        add(ambiguous, "CenterlineDiameter", profile, axis).driving = false;
        auto result = solve(ambiguous);
        CHECK(!result.valid);
        CHECK(result.message.contains("ReferenceAmbiguous"));
        ambiguous.constraints.back().b = newId();
        rejects([&] { ambiguous.validate(); });
    }
    // Free-axis derivatives must work too; fixed-axis fixtures alone would hide them.
    {
        Sketch s;
        auto a = s.point({0, 0}), b = s.point({0, .1});
        auto axis = s.line(a, b, true), point = s.point({.03, .04});
        add(s, "Fixed", point);
        add(s, "Length", axis, {}, .1);
        add(s, "CenterlineDiameter", point, axis, .1).reversed = true;
        auto out = exact(s);
        near(2 * std::abs(axisDistance(out, out.constraints.back())), .1, 1e-9);
        CHECK(solve(out).freedom == 2);
        auto self = s;
        self.constraints.back().a = a;
        CHECK(!solve(self).valid);
    }
    for (double scale : {.001, 1., 1000.})
        for (double rotation : {0., .7, std::numbers::pi / 2})
            for (double side : {-1., 1.})
                for (bool line : {false, true}) {
                    Sketch s;
                    auto at = [&](double x, double y) {
                        return Vec2{scale * (.01 + x * std::cos(rotation) - y * std::sin(rotation)),
                                    scale *
                                        (.02 + x * std::sin(rotation) + y * std::cos(rotation))};
                    };
                    auto a = s.point(at(0, 0)), b = s.point(at(0, .1));
                    auto axis = s.line(a, b, true);
                    add(s, "Fixed", a);
                    add(s, "Fixed", b);
                    auto p = s.point(at(side * .03, .01));
                    auto q = s.point(at(side * .03, .05));
                    auto profile = line ? s.line(p, q) : p;
                    auto &c = add(s, "CenterlineDiameter", profile, axis, .1 * scale);
                    c.reversed = axisDistance(s, c) < 0;
                    auto id = c.id;
                    auto out = exact(s);
                    auto u = out.entity(a).point, v = out.entity(b).point, w = out.entity(p).point;
                    double area = (v.x - u.x) * (w.y - u.y) - (v.y - u.y) * (w.x - u.x);
                    near(2 * std::abs(area) / length(u, v), .1 * scale, 1e-8 * scale);
                    CHECK(out.constraints.back().id == id);
                    CHECK(out.constraints.back().reversed == c.reversed);
                    CHECK(out.map().value("version").toInteger() == 7);
                    if (line)
                        near(axisDistance(out, c, true), axisDistance(out, c), 1e-9 * scale);
                    CHECK(solve(out).freedom ==
                          (line ? 2 : 3)); // unused q stays free for point mode
                    auto conflict = out;
                    auto &extra = add(conflict, "CenterlineDiameter", profile, axis, .12 * scale);
                    extra.reversed = c.reversed;
                    CHECK(!solve(conflict).valid);
                    auto old = out.map();
                    old.insert(QStringLiteral("version"), 6);
                    rejects([&] { Sketch::parse(old); });
                    auto missing = out;
                    missing.entity(axis).construction = false;
                    rejects([&] { missing.validate(); });
                    auto reference = out;
                    reference.constraints.back().driving = false;
                    reference.constraints.back().value = 999;
                    auto ref = exact(reference);
                    near(ref.constraints.back().value, .1 * scale, 1e-8 * scale);
                    CHECK(solve(ref).freedom == solve(out).freedom + (line ? 2 : 1));
                    auto shifted = out;
                    shifted.constraints[0].value += .2 * scale;
                    shifted.constraints[1].value += .2 * scale;
                    auto following = exact(shifted, &out);
                    near(2 * (c.reversed ? -1 : 1) * axisDistance(following, c), .1 * scale,
                         1e-8 * scale);
                }
    for (QString kind : {"HorizontalOrdinate", "VerticalOrdinate"}) {
        Sketch s;
        auto origin = s.point({.01, .02}), target = s.point({.03, .05});
        add(s, "Fixed", origin);
        add(s, kind, origin, target, -.04);
        auto out = exact(s);
        near(horizontalDimension(kind) ? out.entity(target).point.x - .01
                                       : out.entity(target).point.y - .02,
             -.04, 1e-10);
        out.constraints[0].value += .015;
        out.constraints[0].second += .025;
        auto moved = exact(out, &out);
        near(measureDimension(moved, moved.constraints.back()), -.04, 1e-10);
        CHECK(moved.constraints.back().a == origin);
    }
    for (double scale : {.001, 1., 1000.})
        for (bool joined : {false, true}) {
            Sketch s;
            auto center = s.point({0, 0});
            auto arc = s.arc(center, .01 * scale, .2, 1.2);
            add(s, "Fixed", center);
            add(s, "Radius", arc, {}, .01 * scale);
            if (joined) {
                auto p = s.point({std::cos(.2) * .01 * scale, std::sin(.2) * .01 * scale});
                add(s, "ArcCoincident", arc, p, 0);
            }
            add(s, "ArcLength", arc, {}, .02 * scale);
            auto out = exact(s);
            near(out.entity(arc).sweep, 2, 1e-8);
            near(out.entity(arc).radius * out.entity(arc).sweep, .02 * scale, 1e-10 * scale);
            // A radius edit changes sweep to preserve real arc length.
            out.constraints[1].value = .02 * scale;
            auto changed = exact(out, &out);
            near(changed.entity(arc).sweep, 1, 1e-8);
            auto bad = changed;
            bad.constraints.back().value = -.01;
            CHECK(!solve(bad).valid);
            bad = changed;
            bad.constraints.back().expression = "5 deg";
            CHECK(!solve(bad).valid);
            auto ref = changed;
            ref.constraints.back().driving = false;
            ref.entity(arc).sweep = .7;
            auto readout = exact(ref);
            near(readout.constraints.back().value, .014 * scale, 1e-9 * scale);
        }
}
Sketch profile() {
    Sketch s;
    auto edges = s.rectangle({.01, 0}, {.03, .135});
    auto a = s.point({0, -.01}, true), b = s.point({0, .15}, true);
    auto axis = s.line(a, b, true);
    add(s, "Fixed", a);
    add(s, "Fixed", b);
    for (auto edge : edges)
        add(s, edge == edges[0] || edge == edges[2] ? "Horizontal" : "Vertical", edge);
    auto &inner = add(s, "CenterlineDiameter", edges[3], axis, .02);
    inner.reversed = true;
    auto &outer = add(s, "CenterlineDiameter", edges[1], axis, .1);
    outer.reversed = true;
    add(s, "Length", edges[1], {}, .135);
    return exact(s);
}
void revolve() {
    auto s = profile();
    features::Feature feature;
    feature.kind = "Revolve";
    feature.parameters = fields({{"sketch", s.id},
                                 {"operation", "NewBody"},
                                 {"target", ""},
                                 {"angle", 2 * std::numbers::pi},
                                 {"direction", "Forward"},
                                 {"axis_origin", QCborArray{0., 0., 0.}},
                                 {"axis_direction", QCborArray{0., 1., 0.}}});
    auto volume = kernel::Part::build(encode({s}), features::encode({feature}))
                      .measure(1)
                      .value("volume_m3")
                      .toDouble();
    near(volume, std::numbers::pi * (.05 * .05 - .01 * .01) * .135, 1e-11);
    for (auto &c : s.constraints)
        if (c.kind == "CenterlineDiameter" && c.value == .1)
            c.expression = "135 mm";
    s = exact(s);
    volume = kernel::Part::build(encode({s}), features::encode({feature}))
                 .measure(2)
                 .value("volume_m3")
                 .toDouble();
    near(volume, std::numbers::pi * (.0675 * .0675 - .01 * .01) * .135, 1e-11);
}
void mark(QString path) {
    QFile f(path + "/.cad-test-root");
    CHECK(f.open(QIODevice::WriteOnly));
    f.write("disposable\n");
}
void persistence() {
    QTemporaryDir root(QDir::tempPath() + "/cad-module3-cm3-associated-XXXXXX"),
        files(QDir::tempPath() + "/cad-module3-cm3-associated-files-XXXXXX");
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
                                                    {"name", "Associated dimensions"},
                                                    {"units", "mm"},
                                                    {"properties_entity", newId()}})));
    Sketch old;
    auto a = old.point({0, 0}), b = old.point({.03, .04});
    old.line(a, b);
    add(old, "HorizontalDistance", a, b, .03);
    store.execute(command("SetSketches", fields({{"sketches", encode({old})}})));
    auto oldPath = files.filePath("schema6.cadproj");
    store.save(command("SaveProject"), oldPath);
    QFile original(oldPath);
    CHECK(original.open(QIODevice::ReadOnly));
    auto bytes = original.readAll();
    original.close();
    auto s = profile();
    auto arc = s.arc(s.point({.2, 0}), .01, 0, 1.2);
    add(s, "ArcLength", arc, {}, .02).expression = "20 mm";
    auto origin = s.point({.2, .1}), point = s.point({.23, .14});
    add(s, "HorizontalOrdinate", origin, point, .03);
    add(s, "VerticalOrdinate", origin, point, .04);
    auto edit = command("SetSketches", fields({{"sketches", encode({s})}}));
    store.execute(edit);
    auto accepted = store.sketches(doc);
    auto revision = store.revision();
    store.execute(edit);
    CHECK(store.revision() == revision);
    bool refused = false;
    try {
        store.save(command("SaveProject"), oldPath);
    } catch (const domain::Error &e) {
        refused = e.code == "UpgradeCopyRequired";
    }
    CHECK(refused);
    CHECK(original.open(QIODevice::ReadOnly));
    CHECK(original.readAll() == bytes);
    store.execute(command("Undo"));
    CHECK(store.sketches(doc)[0].toMap().value("version").toInteger() == 6);
    auto path = files.filePath("undo.cadproj");
    store.save(command("SaveProject"), path);
    store.close();
    auto forged = files.filePath("forged.cadproj");
    CHECK(QFile::copy(path, forged));
    {
        storage::Database db(forged);
        auto manifest = db.scalar("SELECT manifest FROM project_meta").toString();
        CHECK(manifest.endsWith(";sketch-associated-dimensions=1"));
        db.run("UPDATE project_meta SET manifest=?",
               {manifest.section(";sketch-associated-dimensions", 0, 0)});
    }
    rejects([&] { store.open(forged, "Saved"); });
    store.open(path, "Saved");
    store.execute(command("Redo"));
    CHECK(store.sketches(doc) == accepted);
    auto invalid = sketch::parse(accepted)[0];
    for (auto &c : invalid.constraints)
        if (c.kind == "CenterlineDiameter")
            c.value = -1;
    revision = store.revision();
    rejects(
        [&] { store.execute(command("SetSketches", fields({{"sketches", encode({invalid})}}))); });
    CHECK(store.revision() == revision);
    CHECK(store.sketches(doc) == accepted);
    auto saved = files.filePath("associated.cadproj");
    store.save(command("SaveProject"), saved);
    store.close();
    store.open(saved, "Saved");
    CHECK(store.sketches(doc) == accepted);
    store.execute(command("Undo"));
    store.execute(command("Redo"));
    CHECK(store.sketches(doc) == accepted);
    auto portable = files.filePath("portable.cadproj");
    store.save(command("PortableCopy", fields({{"new_project_id", newId()}, {"policy", "Copied"}})),
               portable, true);
    store.close();
    store.open(portable, "Saved");
    CHECK(store.sketches(doc) == accepted);
}
} // namespace
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    return run([] {
        corpus();
        revolve();
        persistence();
    });
}
